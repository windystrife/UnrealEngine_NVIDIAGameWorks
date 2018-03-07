// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DiffUtils.h"
#include "UObject/PropertyPortFlags.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"

#include "EditorCategoryUtils.h"
#include "Engine/Blueprint.h"
#include "IAssetTypeActions.h"
#include "ObjectEditorUtils.h"

static const UProperty* Resolve( const UStruct* Class, FName PropertyName )
{
	if(Class == nullptr )
	{
		return nullptr;
	}

	for (TFieldIterator<UProperty> PropertyIt(Class); PropertyIt; ++PropertyIt)
	{
		if( PropertyIt->GetFName() == PropertyName )
		{
			return *PropertyIt;
		}
	}

	return nullptr;
}

static FPropertySoftPathSet GetPropertyNameSet(const UObject* ForObj)
{
	return FPropertySoftPathSet(DiffUtils::GetVisiblePropertiesInOrderDeclared(ForObj));
}

FResolvedProperty FPropertySoftPath::Resolve(const UObject* Object) const
{
	// dig into the object, finding nested objects, etc:
	const void* CurrentBlock = Object;
	const UStruct* NextClass = Object->GetClass();
	const void* NextBlock = CurrentBlock;
	const UProperty* Property = nullptr;

	for( int32 i = 0; i < PropertyChain.Num(); ++i )
	{
		CurrentBlock = NextBlock;
		const UProperty* NextProperty = ::Resolve(NextClass, PropertyChain[i]);
		if( NextProperty )
		{
			Property = NextProperty;
			if (const UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property))
			{
				const UObject* NextObject = ObjectProperty->GetObjectPropertyValue(Property->ContainerPtrToValuePtr<UObject*>(CurrentBlock));
				NextBlock = NextObject;
				NextClass = NextObject ? NextObject->GetClass() : nullptr;
			}
			else if (const UStructProperty* StructProperty = Cast<UStructProperty>(Property))
			{
				NextBlock = StructProperty->ContainerPtrToValuePtr<void>(CurrentBlock);
				NextClass = StructProperty->Struct;
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}

	return FResolvedProperty(CurrentBlock, Property);
}

FPropertyPath FPropertySoftPath::ResolvePath(const UObject* Object) const
{
	auto UpdateContainerAddress = [](const UProperty* Property, const void* Instance, const void*& OutContainerAddress, const UStruct*& OutContainerStruct)
	{
		if( ensure(Instance) )
		{
			if(const UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property))
			{
				const UObject* const* InstanceObject = reinterpret_cast<const UObject* const*>(Instance);
				if( *InstanceObject)
				{
					OutContainerAddress = *InstanceObject;
					OutContainerStruct = (*InstanceObject)->GetClass();
				}
			}
			else if(const UStructProperty* StructProperty = Cast<UStructProperty>(Property))
			{
				OutContainerAddress = Instance;
				OutContainerStruct = StructProperty->Struct;
			}
		}
	};

	auto TryReadIndex = [](const TArray<FName>& LocalPropertyChain, int32& OutIndex) -> int32
	{
		if(OutIndex + 1 < LocalPropertyChain.Num())
		{
			FString AsString = LocalPropertyChain[OutIndex + 1].ToString();
			if(AsString.IsNumeric())
			{
				++OutIndex;
				return FCString::Atoi(*AsString);
			}
		}
		return INDEX_NONE;
	};

	const void* ContainerAddress = Object;
	const UStruct* ContainerStruct = (Object ? Object->GetClass() : nullptr);

	FPropertyPath Ret;
	for( int32 I = 0; I < PropertyChain.Num(); ++I )
	{
		FName PropertyIdentifier = PropertyChain[I];
		const UProperty * ResolvedProperty = ::Resolve(ContainerStruct, PropertyIdentifier);
			
		FPropertyInfo Info(ResolvedProperty, INDEX_NONE);
		Ret.AddProperty(Info);

		int32 PropertyIndex = TryReadIndex(PropertyChain, I);
		
		
		// calculate offset so we can continue resolving object properties/structproperties:
		if( const UArrayProperty* ArrayProperty = Cast<UArrayProperty>(ResolvedProperty) )
		{
			if(PropertyIndex != INDEX_NONE)
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<const void*>( ContainerAddress ));

				UpdateContainerAddress( ArrayProperty->Inner, ArrayHelper.GetRawPtr(PropertyIndex), ContainerAddress, ContainerStruct );

				FPropertyInfo ArrayInfo(ArrayProperty->Inner, PropertyIndex);
				Ret.AddProperty(ArrayInfo);
			}
		}
		else if( const USetProperty* SetProperty = Cast<USetProperty>(ResolvedProperty) )
		{
			if(PropertyIndex != INDEX_NONE)
			{
				FScriptSetHelper SetHelper(SetProperty, SetProperty->ContainerPtrToValuePtr<const void*>( ContainerAddress ));

				// Figure out the real index in this instance of the set (sets have gaps in them):
				int32 RealIndex = -1;
				for( int32 J = 0; PropertyIndex >= 0; ++J)
				{
					++RealIndex;
					if(SetHelper.IsValidIndex(J))
					{
						--PropertyIndex;
					}
				}

				UpdateContainerAddress( SetProperty->ElementProp, SetHelper.GetElementPtr(RealIndex), ContainerAddress, ContainerStruct );

				FPropertyInfo SetInfo(SetProperty->ElementProp, RealIndex);
				Ret.AddProperty(SetInfo);
			}
		}
		else if( const UMapProperty* MapProperty = Cast<UMapProperty>(ResolvedProperty) )
		{
			if(PropertyIndex != INDEX_NONE)
			{
				FScriptMapHelper MapHelper(MapProperty, MapProperty->ContainerPtrToValuePtr<const void*>( ContainerAddress ));
				
				// Figure out the real index in this instance of the map (map have gaps in them):
				int32 RealIndex = -1;
				for( int32 J = 0; PropertyIndex >= 0; ++J)
				{
					++RealIndex;
					if(MapHelper.IsValidIndex(J))
					{
						--PropertyIndex;
					}
				}

				// we have an index, but are we looking into a key or value? Peek ahead to find out:
				if(ensure(I + 1 < PropertyChain.Num()))
				{
					if(PropertyChain[I+1] == MapProperty->KeyProp->GetFName())
					{
						++I;

						UpdateContainerAddress( MapProperty->KeyProp, MapHelper.GetKeyPtr(RealIndex), ContainerAddress, ContainerStruct );

						FPropertyInfo MakKeyInfo(MapProperty->KeyProp, RealIndex);
						Ret.AddProperty(MakKeyInfo);
					}
					else if(ensure( PropertyChain[I+1] == MapProperty->ValueProp->GetFName() ))
					{	
						++I;

						UpdateContainerAddress( MapProperty->ValueProp, MapHelper.GetValuePtr(RealIndex), ContainerAddress, ContainerStruct );
						
						FPropertyInfo MapValueInfo(MapProperty->ValueProp, RealIndex);
						Ret.AddProperty(MapValueInfo);
					}
				}
			}
		}
		else if (const UObjectProperty* ObjectProperty = Cast<UObjectProperty>(ResolvedProperty))
		{
			UpdateContainerAddress( ObjectProperty, ObjectProperty->ContainerPtrToValuePtr<const void*>( ContainerAddress, FMath::Max(PropertyIndex, 0) ), ContainerAddress, ContainerStruct );
			
			// handle static arrays:
			if(PropertyIndex != INDEX_NONE )
			{
				FPropertyInfo ObjectInfo(ResolvedProperty, PropertyIndex);
				Ret.AddProperty(ObjectInfo);
			}
		}
		else if( const UStructProperty* StructProperty = Cast<UStructProperty>(ResolvedProperty) )
		{
			UpdateContainerAddress( StructProperty, StructProperty->ContainerPtrToValuePtr<const void*>( ContainerAddress, FMath::Max(PropertyIndex, 0) ), ContainerAddress, ContainerStruct );
			
			// handle static arrays:
			if(PropertyIndex != INDEX_NONE )
			{
				FPropertyInfo StructInfo(ResolvedProperty, PropertyIndex);
				Ret.AddProperty(StructInfo);
			}
		}
		else
		{
			// handle static arrays:
			if(PropertyIndex != INDEX_NONE )
			{
				FPropertyInfo StaticArrayInfo(ResolvedProperty, PropertyIndex);
				Ret.AddProperty(StaticArrayInfo);
			}
		}
	}
	return Ret;
}

FString FPropertySoftPath::ToDisplayName() const
{
	FString Ret;
	for( FName Property : PropertyChain )
	{
		FString PropertyAsString = Property.ToString();
		if(Ret.IsEmpty())
		{
			Ret.Append(PropertyAsString);
		}
		else if( PropertyAsString.IsNumeric())
		{
			Ret.AppendChar('[');
			Ret.Append(PropertyAsString);
			Ret.AppendChar(']');
		}
		else
		{
			Ret.AppendChar(' ');
			Ret.Append(PropertyAsString);
		}
	}
	return Ret;
}

const UObject* DiffUtils::GetCDO(const UBlueprint* ForBlueprint)
{
	if (!ForBlueprint
		|| !ForBlueprint->GeneratedClass)
	{
		return NULL;
	}

	return ForBlueprint->GeneratedClass->ClassDefaultObject;
}

void DiffUtils::CompareUnrelatedObjects(const UObject* A, const UObject* B, TArray<FSingleObjectDiffEntry>& OutDifferingProperties)
{
	FPropertySoftPathSet PropertiesInA = GetPropertyNameSet(A);
	FPropertySoftPathSet PropertiesInB = GetPropertyNameSet(B);

	// any properties in A that aren't in B are differing:
	auto AddedToA = PropertiesInA.Difference(PropertiesInB).Array();
	for( const auto& Entry : AddedToA )
	{
		OutDifferingProperties.Push(FSingleObjectDiffEntry( Entry, EPropertyDiffType::PropertyAddedToA ));
	}

	// and the converse:
	auto AddedToB = PropertiesInB.Difference(PropertiesInA).Array();
	for (const auto& Entry : AddedToB)
	{
		OutDifferingProperties.Push(FSingleObjectDiffEntry( Entry, EPropertyDiffType::PropertyAddedToB ));
	}

	// for properties in common, dig out the uproperties and determine if they're identical:
	if (A && B)
	{
		FPropertySoftPathSet Common = PropertiesInA.Intersect(PropertiesInB);
		for (const auto& PropertyName : Common)
		{
			FResolvedProperty AProp = PropertyName.Resolve(A);
			FResolvedProperty BProp = PropertyName.Resolve(B);

			check(AProp != FResolvedProperty() && BProp != FResolvedProperty());
			TArray<FPropertySoftPath> DifferingSubProperties;
			if (!DiffUtils::Identical(AProp, BProp, PropertyName, DifferingSubProperties))
			{
				for (int DifferingIndex = 0; DifferingIndex < DifferingSubProperties.Num(); DifferingIndex++)
				{
					OutDifferingProperties.Push(FSingleObjectDiffEntry(DifferingSubProperties[DifferingIndex], EPropertyDiffType::PropertyValueChanged));
				}
			}
		}
	}
}

void DiffUtils::CompareUnrelatedSCS(const UBlueprint* Old, const TArray< FSCSResolvedIdentifier >& OldHierarchy, const UBlueprint* New, const TArray< FSCSResolvedIdentifier >& NewHierarchy, FSCSDiffRoot& OutDifferingEntries )
{
	const auto FindEntry = [](TArray< FSCSResolvedIdentifier > const& InArray, const FSCSIdentifier* Value) -> const FSCSResolvedIdentifier*
	{
		for (const auto& Node : InArray)
		{
			if (Node.Identifier.Name == Value->Name )
			{
				return &Node;
			}
		}
		return nullptr;
	};

	for (const auto& OldNode : OldHierarchy)
	{
		const FSCSResolvedIdentifier* NewEntry = FindEntry(NewHierarchy, &OldNode.Identifier);

		if (NewEntry != nullptr)
		{
			bool bShouldDiffProperties = true;

			// did it change class?
			const bool bObjectTypesDiffer = OldNode.Object != nullptr && NewEntry->Object != nullptr && OldNode.Object->GetClass() != NewEntry->Object->GetClass();
			if (bObjectTypesDiffer)
			{
				FSCSDiffEntry Diff = { OldNode.Identifier, ETreeDiffType::NODE_TYPE_CHANGED, FSingleObjectDiffEntry() };
				OutDifferingEntries.Entries.Push(Diff);

				// Only diff properties if we're still within the same class inheritance hierarchy.
				bShouldDiffProperties = OldNode.Object->GetClass()->IsChildOf(NewEntry->Object->GetClass()) || NewEntry->Object->GetClass()->IsChildOf(OldNode.Object->GetClass());
			}

			// did a property change?
			if(bShouldDiffProperties)
			{
				TArray<FSingleObjectDiffEntry> DifferingProperties;
				DiffUtils::CompareUnrelatedObjects(OldNode.Object, NewEntry->Object, DifferingProperties);
				for (const auto& Property : DifferingProperties)
				{
					// Only include property value change entries if the object types differ.
					if (!bObjectTypesDiffer || Property.DiffType == EPropertyDiffType::PropertyValueChanged)
					{
						FSCSDiffEntry Diff = { OldNode.Identifier, ETreeDiffType::NODE_PROPERTY_CHANGED, Property };
						OutDifferingEntries.Entries.Push(Diff);
					}
				}
			}

			// did it move?
			if( NewEntry->Identifier.TreeLocation != OldNode.Identifier.TreeLocation )
			{
				FSCSDiffEntry Diff = { OldNode.Identifier, ETreeDiffType::NODE_MOVED, FSingleObjectDiffEntry() };
				OutDifferingEntries.Entries.Push(Diff);
			}

			// no change! Do nothing.
		}
		else
		{
			// not found in the new data, must have been deleted:
			FSCSDiffEntry Entry = { OldNode.Identifier, ETreeDiffType::NODE_REMOVED, FSingleObjectDiffEntry() };
			OutDifferingEntries.Entries.Push( Entry );
		}
	}

	for (const auto& NewNode : NewHierarchy)
	{
		const FSCSResolvedIdentifier* OldEntry = FindEntry(OldHierarchy, &NewNode.Identifier);

		if (OldEntry == nullptr)
		{
			FSCSDiffEntry Entry = { NewNode.Identifier, ETreeDiffType::NODE_ADDED, FSingleObjectDiffEntry() };
			OutDifferingEntries.Entries.Push( Entry );
		}
	}
}

static void AdvanceSetIterator( FScriptSetHelper& SetHelper, int32& Index)
{
	while(Index < SetHelper.Num() && !SetHelper.IsValidIndex(Index))
	{
		++Index;
	}
}

static void AdvanceMapIterator( FScriptMapHelper& MapHelper, int32& Index)
{
	while(Index < MapHelper.Num() && !MapHelper.IsValidIndex(Index))
	{
		++Index;
	}
}

static void IdenticalHelper(const UProperty* AProperty, const UProperty* BProperty, const void* AValue, const void* BValue, const FPropertySoftPath& RootPath, TArray<FPropertySoftPath>& DifferingSubProperties, bool bStaticArrayHandled = false)
{
	if(AProperty == nullptr || BProperty == nullptr || AProperty->ArrayDim != BProperty->ArrayDim || AProperty->GetClass() != BProperty->GetClass())
	{
		DifferingSubProperties.Push(RootPath);
		return;
	}

	if(!bStaticArrayHandled && AProperty->ArrayDim != 1)
	{
		// Identical does not handle static array case automatically and we have to do the offset calculation ourself because 
		// our container (e.g. the struct or class or dynamic array) has already done the initial offset calculation:
		for( int32 I = 0; I < AProperty->ArrayDim; ++I )
		{
			int32 Offset = AProperty->ElementSize * I;
			const void* CurAValue = reinterpret_cast<const void*>(reinterpret_cast<const uint8*>(AValue) + Offset);
			const void* CurBValue = reinterpret_cast<const void*>(reinterpret_cast<const uint8*>(BValue) + Offset);

			IdenticalHelper(AProperty, BProperty, CurAValue, CurBValue, FPropertySoftPath(RootPath, I), DifferingSubProperties, true);
		}

		return;
	}
	
	const UStructProperty* APropAsStruct = Cast<UStructProperty>(AProperty);
	const UArrayProperty* APropAsArray = Cast<UArrayProperty>(AProperty);
	const USetProperty* APropAsSet = Cast<USetProperty>(AProperty);
	const UMapProperty* APropAsMap = Cast<UMapProperty>(AProperty);
	const UObjectProperty* APropAsObject = Cast<UObjectProperty>(AProperty);
	if (APropAsStruct != nullptr)
	{
		const UStructProperty* BPropAsStruct = CastChecked<UStructProperty>(BProperty);
		if (APropAsStruct->Struct->StructFlags & STRUCT_IdenticalNative || BPropAsStruct->Struct != APropAsStruct->Struct)
		{
			// If the struct uses CPP identical tests, then we can't dig into it, and we already know it's not identical from the test when we started
			DifferingSubProperties.Push(RootPath);
		}
		else
		{
			for (TFieldIterator<UProperty> PropertyIt(APropAsStruct->Struct); PropertyIt; ++PropertyIt)
			{
				const UProperty* StructProp = *PropertyIt;
				IdenticalHelper(StructProp, StructProp, StructProp->ContainerPtrToValuePtr<void>(AValue, 0), StructProp->ContainerPtrToValuePtr<void>(BValue, 0), FPropertySoftPath(RootPath, StructProp), DifferingSubProperties);
			}
		}
	}
	else if (APropAsArray != nullptr)
	{
		const UArrayProperty* BPropAsArray = CastChecked<UArrayProperty>(BProperty);
		if(BPropAsArray->Inner->GetClass() == APropAsArray->Inner->GetClass())
		{
			FScriptArrayHelper ArrayHelperA(APropAsArray, AValue);
			FScriptArrayHelper ArrayHelperB(BPropAsArray, BValue);
		
			// note any differences in contained types:
			for (int32 ArrayIndex = 0; ArrayIndex < ArrayHelperA.Num() && ArrayIndex < ArrayHelperB.Num(); ArrayIndex++)
			{
				IdenticalHelper(APropAsArray->Inner, APropAsArray->Inner, ArrayHelperA.GetRawPtr(ArrayIndex), ArrayHelperB.GetRawPtr(ArrayIndex), FPropertySoftPath(RootPath, ArrayIndex), DifferingSubProperties);
			}

			// note any size difference:
			if (ArrayHelperA.Num() != ArrayHelperB.Num())
			{
				DifferingSubProperties.Push(RootPath);
			}
		}
		else
		{
			DifferingSubProperties.Push(RootPath);
		}
	}
	else if(APropAsSet != nullptr)
	{
		const USetProperty* BPropAsSet = CastChecked<USetProperty>(BProperty);
		if(BPropAsSet->ElementProp->GetClass() == APropAsSet->ElementProp->GetClass())
		{
			FScriptSetHelper SetHelperA(APropAsSet, AValue);
			FScriptSetHelper SetHelperB(BPropAsSet, BValue);

			if (SetHelperA.Num() != SetHelperB.Num())
			{
				// API not robust enough to indicate changes made to # of set elements, would
				// need to return something more detailed than DifferingSubProperties array:
				DifferingSubProperties.Push(RootPath);
			}

			// note any differences in contained elements:
			const int32 SetSizeA = SetHelperA.Num();
			const int32 SetSizeB = SetHelperB.Num();
			
			int32 SetIndexA = 0;
			int32 SetIndexB = 0;

			AdvanceSetIterator(SetHelperA, SetIndexA);
			AdvanceSetIterator(SetHelperB, SetIndexB);

			for (int32 VirtualIndex = 0; VirtualIndex < SetSizeA && VirtualIndex < SetSizeB; ++VirtualIndex)
			{
				IdenticalHelper(APropAsSet->ElementProp, APropAsSet->ElementProp, SetHelperA.GetElementPtr(SetIndexA), SetHelperB.GetElementPtr(SetIndexB), FPropertySoftPath(RootPath, VirtualIndex), DifferingSubProperties);

				// advance iterators in step:
				AdvanceSetIterator(SetHelperA, SetIndexA);
				AdvanceSetIterator(SetHelperB, SetIndexB);
			}
		}
		else
		{
			DifferingSubProperties.Push(RootPath);
		}
	}
	else if(APropAsMap != nullptr)
	{
		const UMapProperty* BPropAsMap = CastChecked<UMapProperty>(BProperty);
		if(APropAsMap->KeyProp->GetClass() == BPropAsMap->KeyProp->GetClass() && APropAsMap->ValueProp->GetClass() == BPropAsMap->ValueProp->GetClass())
		{
			FScriptMapHelper MapHelperA(APropAsMap, AValue);
			FScriptMapHelper MapHelperB(BPropAsMap, BValue);

			if (MapHelperA.Num() != MapHelperB.Num())
			{
				// API not robust enough to indicate changes made to # of set elements, would
				// need to return something more detailed than DifferingSubProperties array:
				DifferingSubProperties.Push(RootPath);
			}

			int32 MapSizeA = MapHelperA.Num();
			int32 MapSizeB = MapHelperB.Num();
			
			int32 MapIndexA = 0;
			int32 MapIndexB = 0;

			AdvanceMapIterator(MapHelperA, MapIndexA);
			AdvanceMapIterator(MapHelperB, MapIndexB);
			
			for (int32 VirtualIndex = 0; VirtualIndex < MapSizeA && VirtualIndex < MapSizeB; ++VirtualIndex)
			{
				IdenticalHelper(APropAsMap->KeyProp, APropAsMap->KeyProp, MapHelperA.GetKeyPtr(MapIndexA), MapHelperB.GetKeyPtr(MapIndexB), FPropertySoftPath(RootPath, VirtualIndex), DifferingSubProperties);
				IdenticalHelper(APropAsMap->ValueProp, APropAsMap->ValueProp, MapHelperA.GetValuePtr(MapIndexA), MapHelperB.GetValuePtr(MapIndexB), FPropertySoftPath(RootPath, VirtualIndex), DifferingSubProperties);

				AdvanceMapIterator(MapHelperA, MapIndexA);
				AdvanceMapIterator(MapHelperB, MapIndexB);
			}
		}
		else
		{
			DifferingSubProperties.Push(RootPath);
		}
	}
	else if(APropAsObject != nullptr)
	{
		// Past container check, do a normal identical check now before going into components
		if (AProperty->Identical(AValue, BValue, PPF_DeepComparison))
		{
			return;
		}

		// dig into the objects if they are in the same package as our initial object:
		const UObjectProperty* BPropAsObject = CastChecked<UObjectProperty>(BProperty);

		const UObject* A = *((const UObject* const*)AValue);
		const UObject* B = *((const UObject* const*)BValue);

		if(BPropAsObject->HasAnyPropertyFlags(CPF_InstancedReference) && APropAsObject->HasAnyPropertyFlags(CPF_InstancedReference) && A && B && A->GetClass() == B->GetClass())
		{
			// dive into the object to find actual differences:
			const UClass* AClass = A->GetClass(); // BClass and AClass are identical!

			for (TFieldIterator<UProperty> PropertyIt(AClass); PropertyIt; ++PropertyIt)
			{
				const UProperty* ClassProp = *PropertyIt;
				IdenticalHelper(ClassProp, ClassProp, ClassProp->ContainerPtrToValuePtr<void>(A, 0), ClassProp->ContainerPtrToValuePtr<void>(B, 0), FPropertySoftPath(RootPath, ClassProp), DifferingSubProperties);
			}
		}
		else
		{
			DifferingSubProperties.Push(RootPath);
		}
	}
	else
	{
		// Passed all container tests that would check for nested properties being wrong
		if (AProperty->Identical(AValue, BValue, PPF_DeepComparison))
		{
			return;
		}

		DifferingSubProperties.Push(RootPath);
	}
}

bool DiffUtils::Identical(const FResolvedProperty& AProp, const FResolvedProperty& BProp, const FPropertySoftPath& RootPath, TArray<FPropertySoftPath>& DifferingProperties)
{
	const void* AValue = AProp.Property->ContainerPtrToValuePtr<void>(AProp.Object);
	const void* BValue = BProp.Property->ContainerPtrToValuePtr<void>(BProp.Object);

	// We _could_ just ask the property for comparison but that would make the "identical" functions significantly more complex.
	// Instead let's write a new function, specific to DiffUtils, that handles the sub properties
	// NOTE: For Static Arrays, AValue and BValue were, and are, only references to the value at index 0.  So changes to values past index 0 didn't show up before and
	// won't show up now.  Changes to index 0 will show up as a change to the entire array.
	IdenticalHelper(AProp.Property, BProp.Property, AValue, BValue, RootPath, DifferingProperties);
	
	return DifferingProperties.Num() == 0;
}

TArray<FPropertySoftPath> DiffUtils::GetVisiblePropertiesInOrderDeclared(const UObject* ForObj, const TArray<FName>& Scope /*= TArray<FName>()*/)
{
	TArray<FPropertySoftPath> Ret;
	if (ForObj)
	{
		const UClass* Class = ForObj->GetClass();
		TSet<FString> HiddenCategories = FEditorCategoryUtils::GetHiddenCategories(Class);
		for (TFieldIterator<UProperty> PropertyIt(Class); PropertyIt; ++PropertyIt)
		{
			FName CategoryName = FObjectEditorUtils::GetCategoryFName(*PropertyIt);
			if (!HiddenCategories.Contains(CategoryName.ToString()))
			{
				if (PropertyIt->PropertyFlags&CPF_Edit)
				{
					TArray<FName> NewPath(Scope);
					NewPath.Push(PropertyIt->GetFName());
					if (const UObjectProperty* ObjectProperty = Cast<UObjectProperty>(*PropertyIt))
					{
						const UObject* const* BaseObject = reinterpret_cast<const UObject* const*>( ObjectProperty->ContainerPtrToValuePtr<void>(ForObj) );
						if (BaseObject && *BaseObject)
						{
							Ret.Append( GetVisiblePropertiesInOrderDeclared(*BaseObject, NewPath) );
						}
					}
					else
					{
						Ret.Push(NewPath);
					}
				}
			}
		}
	}
	return Ret;
}

TArray<FPropertyPath> DiffUtils::ResolveAll(const UObject* Object, const TArray<FPropertySoftPath>& InSoftProperties)
{
	TArray< FPropertyPath > Ret;
	for (const auto& Path : InSoftProperties)
	{
		Ret.Push(Path.ResolvePath(Object));
	}
	return Ret;
}

TArray<FPropertyPath> DiffUtils::ResolveAll(const UObject* Object, const TArray<FSingleObjectDiffEntry>& InDifferences)
{
	TArray< FPropertyPath > Ret;
	for (const auto& Difference : InDifferences)
	{
		Ret.Push(Difference.Identifier.ResolvePath(Object));
	}
	return Ret;
}

TSharedPtr<FBlueprintDifferenceTreeEntry> FBlueprintDifferenceTreeEntry::NoDifferencesEntry()
{
	// This just generates a widget that tells the user that no differences were detected. Without this
	// the treeview displaying differences is confusing when no differences are present because it is not obvious
	// that the control is a treeview (a treeview with no children looks like a listview).
	const auto GenerateWidget = []() -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.ColorAndOpacity(FLinearColor(.7f, .7f, .7f))
			.TextStyle(FEditorStyle::Get(), TEXT("BlueprintDif.ItalicText"))
			.Text(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "NoDifferencesLabel", "No differences detected..."));
	};

	return TSharedPtr<FBlueprintDifferenceTreeEntry>(new FBlueprintDifferenceTreeEntry(
		FOnDiffEntryFocused()
		, FGenerateDiffEntryWidget::CreateStatic(GenerateWidget)
		, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >()
	) );
}

TSharedPtr<FBlueprintDifferenceTreeEntry> FBlueprintDifferenceTreeEntry::AnimBlueprintEntry()
{
	// For now, a widget and a short message explaining that differences in the AnimGraph are
	// not detected by the diff tool:
	const auto GenerateWidget = []() -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.ColorAndOpacity(FLinearColor(.7f, .7f, .7f))
			.TextStyle(FEditorStyle::Get(), TEXT("BlueprintDif.ItalicText"))
			.Text(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "AnimBlueprintsNotSupported", "Warning: Detecting differences in Animation Blueprint specific data is not yet supported..."));
	};

	TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> > Children;
	Children.Emplace( TSharedPtr<FBlueprintDifferenceTreeEntry>(new FBlueprintDifferenceTreeEntry(
		FOnDiffEntryFocused()
		, FGenerateDiffEntryWidget::CreateStatic(GenerateWidget)
		, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >()
		)) );

	const auto CreateAnimGraphRootEntry = []() -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.ToolTipText(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "AnimGraphTooltip", "Detecting differences in Animation Blueprint specific data is not yet supported"))
			.ColorAndOpacity(DiffViewUtils::LookupColor(true))
			.Text(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "AnimGraphLabel", "Animation Blueprint"));
	};

	return TSharedPtr<FBlueprintDifferenceTreeEntry>(new FBlueprintDifferenceTreeEntry(
		FOnDiffEntryFocused()
		, FGenerateDiffEntryWidget::CreateStatic(CreateAnimGraphRootEntry)
		, Children
		));
}

TSharedPtr<FBlueprintDifferenceTreeEntry> FBlueprintDifferenceTreeEntry::WidgetBlueprintEntry()
{
	// For now, a widget and a short message explaining that differences in the WidgetTree are
	// not detected by the diff tool:
	const auto GenerateWidget = []() -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.ColorAndOpacity(FLinearColor(.7f, .7f, .7f))
			.TextStyle(FEditorStyle::Get(), TEXT("BlueprintDif.ItalicText"))
			.Text(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "WidgetTreeNotSupported", "Warning: Detecting differences in Widget Blueprint specific data is not yet supported..."));
	};

	TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> > Children;
	Children.Emplace(TSharedPtr<FBlueprintDifferenceTreeEntry>(new FBlueprintDifferenceTreeEntry(
		FOnDiffEntryFocused()
		, FGenerateDiffEntryWidget::CreateStatic(GenerateWidget)
		, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >()
		)));

	const auto CreateWidgetTreeRootEntry = []() -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.ToolTipText(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "WidgetTreeTooltip", "Detecting differences in Widget Blueprint specific data is not yet supported"))
			.ColorAndOpacity(DiffViewUtils::LookupColor(true))
			.Text(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "WidgetTreeLabel", "Widget Blueprint"));
	};

	return TSharedPtr<FBlueprintDifferenceTreeEntry>(new FBlueprintDifferenceTreeEntry(
		FOnDiffEntryFocused()
		, FGenerateDiffEntryWidget::CreateStatic(CreateWidgetTreeRootEntry)
		, Children
		));
}

TSharedPtr<FBlueprintDifferenceTreeEntry> FBlueprintDifferenceTreeEntry::CreateDefaultsCategoryEntry(FOnDiffEntryFocused FocusCallback, const TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& Children, bool bHasDifferences)
{
	const auto CreateDefaultsRootEntry = [](FLinearColor Color) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.ToolTipText(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "DefaultsTooltip", "The list of changes made in the Defaults panel"))
			.ColorAndOpacity(Color)
			.Text(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "DefaultsLabel", "Defaults"));
	};

	return TSharedPtr<FBlueprintDifferenceTreeEntry>(new FBlueprintDifferenceTreeEntry(
		FocusCallback
		, FGenerateDiffEntryWidget::CreateStatic(CreateDefaultsRootEntry, DiffViewUtils::LookupColor(bHasDifferences) )
		, Children
	));
}

TSharedPtr<FBlueprintDifferenceTreeEntry> FBlueprintDifferenceTreeEntry::CreateDefaultsCategoryEntryForMerge(FOnDiffEntryFocused FocusCallback, const TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& Children, bool bHasRemoteDifferences, bool bHasLocalDifferences, bool bHasConflicts)
{
	const auto CreateDefaultsRootEntry = [](bool bInHasRemoteDifferences, bool bInHasLocalDifferences, bool bInHasConflicts) -> TSharedRef<SWidget>
	{
		const FLinearColor BaseColor = DiffViewUtils::LookupColor(bInHasRemoteDifferences || bInHasLocalDifferences, bInHasConflicts);
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.ToolTipText(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "DefaultsTooltip", "The list of changes made in the Defaults panel"))
				.ColorAndOpacity(BaseColor)
				.Text(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "DefaultsLabel", "Defaults"))
			]
			+ DiffViewUtils::Box(true, DiffViewUtils::LookupColor(bInHasRemoteDifferences, bInHasConflicts))
			+ DiffViewUtils::Box(true, BaseColor)
			+ DiffViewUtils::Box(true, DiffViewUtils::LookupColor(bInHasLocalDifferences, bInHasConflicts));
	};

	return TSharedPtr<FBlueprintDifferenceTreeEntry>(new FBlueprintDifferenceTreeEntry(
		FocusCallback
		, FGenerateDiffEntryWidget::CreateStatic(CreateDefaultsRootEntry, bHasRemoteDifferences, bHasLocalDifferences, bHasConflicts)
		, Children
		));
}

TSharedPtr<FBlueprintDifferenceTreeEntry> FBlueprintDifferenceTreeEntry::CreateComponentsCategoryEntry(FOnDiffEntryFocused FocusCallback, const TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& Children, bool bHasDifferences)
{
	const auto CreateComponentsRootEntry = [](FLinearColor Color) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.ToolTipText(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "SCSTooltip", "The list of changes made in the Components panel"))
			.ColorAndOpacity(Color)
			.Text(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "SCSLabel", "Components"));
	};

	return TSharedPtr<FBlueprintDifferenceTreeEntry>(new FBlueprintDifferenceTreeEntry(
		FocusCallback
		, FGenerateDiffEntryWidget::CreateStatic(CreateComponentsRootEntry, DiffViewUtils::LookupColor(bHasDifferences))
		, Children
		));
}

TSharedPtr<FBlueprintDifferenceTreeEntry> FBlueprintDifferenceTreeEntry::CreateComponentsCategoryEntryForMerge(FOnDiffEntryFocused FocusCallback, const TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& Children, bool bHasRemoteDifferences, bool bHasLocalDifferences, bool bHasConflicts)
{
	const auto CreateComponentsRootEntry = [](bool bInHasRemoteDifferences, bool bInHasLocalDifferences, bool bInHasConflicts) -> TSharedRef<SWidget>
	{
		const FLinearColor BaseColor = DiffViewUtils::LookupColor(bInHasRemoteDifferences || bInHasLocalDifferences, bInHasConflicts);
		return  SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.ToolTipText(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "SCSTooltip", "The list of changes made in the Components panel"))
				.ColorAndOpacity(BaseColor)
				.Text(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "SCSLabel", "Components"))
			]
			+ DiffViewUtils::Box(true, DiffViewUtils::LookupColor(bInHasRemoteDifferences, bInHasConflicts))
			+ DiffViewUtils::Box(true, BaseColor)
			+ DiffViewUtils::Box(true, DiffViewUtils::LookupColor(bInHasLocalDifferences, bInHasConflicts));
	};

	return TSharedPtr<FBlueprintDifferenceTreeEntry>(new FBlueprintDifferenceTreeEntry(
		FocusCallback
		, FGenerateDiffEntryWidget::CreateStatic(CreateComponentsRootEntry, bHasRemoteDifferences, bHasLocalDifferences, bHasConflicts)
		, Children
	));
}

TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > DiffTreeView::CreateTreeView(TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >* DifferencesList)
{
	const auto RowGenerator = [](TSharedPtr< FBlueprintDifferenceTreeEntry > Entry, const TSharedRef<STableViewBase>& Owner) -> TSharedRef< ITableRow >
	{
		return SNew(STableRow<TSharedPtr<FBlueprintDifferenceTreeEntry> >, Owner)
			[
				Entry->GenerateWidget.Execute()
			];
	};

	const auto ChildrenAccessor = [](TSharedPtr<FBlueprintDifferenceTreeEntry> InTreeItem, TArray< TSharedPtr< FBlueprintDifferenceTreeEntry > >& OutChildren, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >* MasterList)
	{
		OutChildren = InTreeItem->Children;
	};

	const auto Selector = [](TSharedPtr<FBlueprintDifferenceTreeEntry> InTreeItem, ESelectInfo::Type Type)
	{
		if (InTreeItem.IsValid())
		{
			InTreeItem->OnFocus.ExecuteIfBound();
		}
	};

	return SNew(STreeView< TSharedPtr< FBlueprintDifferenceTreeEntry > >)
		.OnGenerateRow(STreeView< TSharedPtr< FBlueprintDifferenceTreeEntry > >::FOnGenerateRow::CreateStatic(RowGenerator))
		.OnGetChildren(STreeView< TSharedPtr< FBlueprintDifferenceTreeEntry > >::FOnGetChildren::CreateStatic(ChildrenAccessor, DifferencesList))
		.OnSelectionChanged(STreeView< TSharedPtr< FBlueprintDifferenceTreeEntry > >::FOnSelectionChanged::CreateStatic(Selector))
		.TreeItemsSource(DifferencesList);
}

int32 DiffTreeView::CurrentDifference(TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > TreeView, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& Differences)
{
	auto SelectedItems = TreeView->GetSelectedItems();
	if (SelectedItems.Num() == 0)
	{
		return INDEX_NONE;
	}

	for (int32 Iter = 0; Iter < SelectedItems.Num(); ++Iter)
	{
		int32 Index = Differences.Find(SelectedItems[Iter]);
		if (Index != INDEX_NONE)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

void DiffTreeView::HighlightNextDifference(TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > TreeView, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& Differences, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& RootDifferences)
{
	int32 CurrentIndex = CurrentDifference(TreeView, Differences);

	auto Next = Differences[CurrentIndex + 1];
	// we have to manually expand our parent:
	for (auto& Test : RootDifferences)
	{
		if (Test->Children.Contains(Next))
		{
			TreeView->SetItemExpansion(Test, true);
			break;
		}
	}

	TreeView->SetSelection(Next);
	TreeView->RequestScrollIntoView(Next);
}

void DiffTreeView::HighlightPrevDifference(TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > TreeView, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& Differences, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& RootDifferences)
{
	int32 CurrentIndex = CurrentDifference(TreeView, Differences);

	auto Prev = Differences[CurrentIndex - 1];
	// we have to manually expand our parent:
	for (auto& Test : RootDifferences)
	{
		if (Test->Children.Contains(Prev))
		{
			TreeView->SetItemExpansion(Test, true);
			break;
		}
	}

	TreeView->SetSelection(Prev);
	TreeView->RequestScrollIntoView(Prev);
}

bool DiffTreeView::HasNextDifference(TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > TreeView, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& Differences)
{
	int32 CurrentIndex = CurrentDifference(TreeView, Differences);
	return Differences.IsValidIndex(CurrentIndex + 1);
}

bool DiffTreeView::HasPrevDifference(TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > TreeView, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& Differences)
{
	int32 CurrentIndex = CurrentDifference(TreeView, Differences);
	return Differences.IsValidIndex(CurrentIndex - 1);
}

FLinearColor DiffViewUtils::LookupColor(bool bDiffers, bool bConflicts)
{
	if( bConflicts )
	{
		return DiffViewUtils::Conflicting();
	}
	else if( bDiffers )
	{
		return DiffViewUtils::Differs();
	}
	else
	{
		return DiffViewUtils::Identical();
	}
}

FLinearColor DiffViewUtils::Differs()
{
	// yellow color
	return FLinearColor(0.85f,0.71f,0.25f);
}

FLinearColor DiffViewUtils::Identical()
{
	return FLinearColor::White;
}

FLinearColor DiffViewUtils::Missing()
{
	// blue color
	return FLinearColor(0.3f,0.3f,1.f);
}

FLinearColor DiffViewUtils::Conflicting()
{
	// red color
	return FLinearColor(1.0f,0.2f,0.3f);
}

FText DiffViewUtils::PropertyDiffMessage(FSingleObjectDiffEntry Difference, FText ObjectName)
{
	FText Message;
	FString PropertyName = Difference.Identifier.ToDisplayName();
	switch (Difference.DiffType)
	{
	case EPropertyDiffType::PropertyAddedToA:
		Message = FText::Format(NSLOCTEXT("DiffViewUtils", "PropertyValueChange_Removed", "{0} removed from {1}"), FText::FromString(PropertyName), ObjectName);
		break;
	case EPropertyDiffType::PropertyAddedToB:
		Message = FText::Format(NSLOCTEXT("DiffViewUtils", "PropertyValueChange_Added", "{0} added to {1}"), FText::FromString(PropertyName), ObjectName);
		break;
	case EPropertyDiffType::PropertyValueChanged:
		Message = FText::Format(NSLOCTEXT("DiffViewUtils", "PropertyValueChange", "{0} changed value in {1}"), FText::FromString(PropertyName), ObjectName);
		break;
	}
	return Message;
}

FText DiffViewUtils::SCSDiffMessage(const FSCSDiffEntry& Difference, FText ObjectName)
{
	const FText NodeName = FText::FromName(Difference.TreeIdentifier.Name);
	FText Text;
	switch (Difference.DiffType)
	{
	case ETreeDiffType::NODE_ADDED:
		Text = FText::Format(NSLOCTEXT("DiffViewUtils", "NodeAdded", "Added Node {0} to {1}"), NodeName, ObjectName);
		break;
	case ETreeDiffType::NODE_REMOVED:
		Text = FText::Format(NSLOCTEXT("DiffViewUtils", "NodeRemoved", "Removed Node {0} from {1}"), NodeName, ObjectName);
		break;
	case ETreeDiffType::NODE_TYPE_CHANGED:
		Text = FText::Format(NSLOCTEXT("DiffViewUtils", "NodeTypeChanged", "Node {0} changed type in {1}"), NodeName, ObjectName);
		break;
	case ETreeDiffType::NODE_PROPERTY_CHANGED:
		Text = FText::Format(NSLOCTEXT("DiffViewUtils", "NodePropertyChanged", "{0} on {1}"), DiffViewUtils::PropertyDiffMessage(Difference.PropertyDiff, NodeName), ObjectName);
		break;
	case ETreeDiffType::NODE_MOVED:
		Text = FText::Format(NSLOCTEXT("DiffViewUtils", "NodeMoved", "Moved Node {0} in {1}"), NodeName, ObjectName);
		break;
	}
	return Text;
}

FText DiffViewUtils::GetPanelLabel(const UBlueprint* Blueprint, const FRevisionInfo& Revision, FText Label )
{
	if( !Revision.Revision.IsEmpty() )
	{
		FText RevisionData;
		
		if(ISourceControlModule::Get().GetProvider().UsesChangelists())
		{
			RevisionData = FText::Format(NSLOCTEXT("DiffViewUtils", "RevisionData", "Revision {0} - CL {1} - {2}")
				, FText::FromString(Revision.Revision)
				, FText::AsNumber(Revision.Changelist, &FNumberFormattingOptions::DefaultNoGrouping())
				, FText::FromString(Revision.Date.ToString(TEXT("%m/%d/%Y"))));
		}
		else
		{
			RevisionData = FText::Format(NSLOCTEXT("DiffViewUtils", "RevisionDataNoChangelist", "Revision {0} - {1}")
				, FText::FromString(Revision.Revision)
				, FText::FromString(Revision.Date.ToString(TEXT("%m/%d/%Y"))));		
		}

		return FText::Format( NSLOCTEXT("DiffViewUtils", "RevisionLabel", "{0}\n{1}\n{2}")
			, Label
			, FText::FromString( Blueprint->GetName() )
			, RevisionData );
	}
	else
	{
		if( Blueprint )
		{
			return FText::Format( NSLOCTEXT("DiffViewUtils", "RevisionLabel", "{0}\n{1}\n{2}")
				, Label
				, FText::FromString( Blueprint->GetName() )
				, NSLOCTEXT("DiffViewUtils", "LocalRevisionLabel", "Local Revision" ));
		}

		return NSLOCTEXT("DiffViewUtils", "NoBlueprint", "None" );
	}
}

SHorizontalBox::FSlot& DiffViewUtils::Box(bool bIsPresent, FLinearColor Color)
{
	return SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.MaxWidth(8.0f)
		[
			SNew(SImage)
			.ColorAndOpacity(Color)
			.Image(bIsPresent ? FEditorStyle::GetBrush("BlueprintDif.HasGraph") : FEditorStyle::GetBrush("BlueprintDif.MissingGraph"))
		];
};

