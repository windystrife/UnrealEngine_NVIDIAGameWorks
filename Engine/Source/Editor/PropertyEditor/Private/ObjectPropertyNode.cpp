// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "ObjectPropertyNode.h"
#include "Misc/ConfigCacheIni.h"
#include "CategoryPropertyNode.h"
#include "ItemPropertyNode.h"
#include "ObjectEditorUtils.h"
#include "EditorCategoryUtils.h"

FObjectPropertyNode::FObjectPropertyNode(void)
	: FComplexPropertyNode()
	, BaseClass(NULL)
{
}

FObjectPropertyNode::~FObjectPropertyNode(void)
{
}

UObject* FObjectPropertyNode::GetUObject(int32 InIndex)
{
	check(Objects.IsValidIndex(InIndex));
	return Objects[InIndex].Get();
}

const UObject* FObjectPropertyNode::GetUObject(int32 InIndex) const
{
	check(Objects.IsValidIndex(InIndex));
	return Objects[InIndex].Get();
}

UPackage* FObjectPropertyNode::GetUPackage(int32 InIndex)
{
	UObject* Obj = GetUObject(InIndex);
	if (Obj)
	{
		TWeakObjectPtr<UPackage>* Package = ObjectToPackageMapping.Find(Obj);
		return Package ? Package->Get() : Obj->GetOutermost();
	}
	return nullptr;
}

const UPackage* FObjectPropertyNode::GetUPackage(int32 InIndex) const
{
	return const_cast<FObjectPropertyNode*>(this)->GetUPackage(InIndex);
}

// Adds a new object to the list.
void FObjectPropertyNode::AddObject( UObject* InObject )
{
	Objects.Add( InObject );
}

// Removes an object from the list.
void FObjectPropertyNode::RemoveObject( UObject* InObject )
{
	const int32 idx = Objects.Find( InObject );

	if( idx != INDEX_NONE )
	{
		Objects.RemoveAt( idx, 1 );
	}
}

// Removes all objects from the list.
void FObjectPropertyNode::RemoveAllObjects()
{
	Objects.Empty();
}

void FObjectPropertyNode::SetObjectPackageOverrides(const TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UPackage>>& InMapping)
{
	ObjectToPackageMapping = InMapping;
}

void FObjectPropertyNode::ClearObjectPackageOverrides()
{
	ObjectToPackageMapping.Empty();
}

// Purges any objects marked pending kill from the object list
void FObjectPropertyNode::PurgeKilledObjects()
{
	// Purge any objects that are marked pending kill from the object list
	for (int32 Index = 0; Index < Objects.Num(); )
	{
		TWeakObjectPtr<UObject> Object = Objects[Index];

		if ( !Object.IsValid() || Object->IsPendingKill() )
		{
			Objects.RemoveAt(Index, 1);
		}
		else
		{
			++Index;
		}
	}
}

// Called when the object list is finalized, Finalize() finishes the property window setup.
void FObjectPropertyNode::Finalize()
{
	// May be NULL...
	UClass* OldBase = GetObjectBaseClass();

	// Find an appropriate base class.
	SetBestBaseClass();
	if (BaseClass.IsValid() && BaseClass->HasAnyClassFlags(CLASS_CollapseCategories) )
	{
		SetNodeFlags(EPropertyNodeFlags::ShowCategories, false );
	}
}

bool FObjectPropertyNode::GetReadAddressUncached(FPropertyNode& InNode,
											   bool InRequiresSingleSelection,
											   FReadAddressListData* OutAddresses,
											   bool bComparePropertyContents,
											   bool bObjectForceCompare,
											   bool bArrayPropertiesCanDifferInSize) const
{
	// Are any objects selected for property editing?
	if( !GetNumObjects())
	{
		return false;
	}

	UProperty* InItemProperty = InNode.GetProperty();
	// Is there a InItemProperty bound to the InItemProperty window?
	if( !InItemProperty )
	{
		return false;
	}

	// Requesting a single selection?
	if( InRequiresSingleSelection && GetNumObjects() > 1)
	{
		// Fail: we're editing properties for multiple objects.
		return false;
	}

	//assume all properties are the same unless proven otherwise
	bool bAllTheSame = true;

	//////////////////////////////////////////

	// If this item is the child of a container, return NULL if there is a different number
	// of items in the container in different objects, when multi-selecting.

	UArrayProperty* ArrayOuter = Cast<UArrayProperty>(InItemProperty->GetOuter());
	USetProperty* SetOuter = Cast<USetProperty>(InItemProperty->GetOuter());
	UMapProperty* MapOuter = Cast<UMapProperty>(InItemProperty->GetOuter());

	if( ArrayOuter || SetOuter || MapOuter)
	{
		FPropertyNode* ParentPropertyNode = InNode.GetParentNode();
		check(ParentPropertyNode);
		const UObject* TempObject = GetUObject(0);
		if( TempObject )
		{
			uint8* BaseAddr = ParentPropertyNode->GetValueBaseAddress( (uint8*)TempObject );
			if( BaseAddr )
			{
				if ( ArrayOuter )
				{
					const int32 Num = FScriptArrayHelper::Num(BaseAddr);
					for (int32 ObjIndex = 1; ObjIndex < GetNumObjects(); ++ObjIndex)
					{
						TempObject = GetUObject(ObjIndex);
						BaseAddr = ParentPropertyNode->GetValueBaseAddress((uint8*)TempObject);

						if (BaseAddr && Num != FScriptArrayHelper::Num(BaseAddr))
						{
							bAllTheSame = false;
						}
					}
				}
				else if ( SetOuter )
				{
					const int32 Num = FScriptSetHelper::Num(BaseAddr);
					for (int32 ObjIndex = 1; ObjIndex < GetNumObjects(); ++ObjIndex)
					{
						TempObject = GetUObject(ObjIndex);
						BaseAddr = ParentPropertyNode->GetValueBaseAddress((uint8*)TempObject);

						if (BaseAddr && Num != FScriptSetHelper::Num(BaseAddr))
						{
							bAllTheSame = false;
						}
					}
				}
				else if ( MapOuter )
				{
					const int32 Num = FScriptMapHelper::Num(BaseAddr);
					for (int32 ObjIndex = 1; ObjIndex < GetNumObjects(); ++ObjIndex)
					{
						TempObject = GetUObject(ObjIndex);
						BaseAddr = ParentPropertyNode->GetValueBaseAddress((uint8*)TempObject);

						if (BaseAddr && Num != FScriptMapHelper::Num(BaseAddr))
						{
							bAllTheSame = false;
						}
					}
				}
			}
		}
	}

	uint8* Base = GetUObject(0) ? InNode.GetValueBaseAddress( (uint8*)(GetUObject(0)) ) : NULL;
	if (Base)
	{
		// If the item is an array or set itself, return NULL if there are a different number of
		// items in the container in different objects, when multi-selecting.

		UArrayProperty* ArrayProp = Cast<UArrayProperty>(InItemProperty);
		USetProperty* SetProp = Cast<USetProperty>(InItemProperty);
		UMapProperty* MapProp = Cast<UMapProperty>(InItemProperty);

		if( ArrayProp || SetProp || MapProp)
		{
			// This flag is an override for array properties which want to display e.g. the "Clear" and "Empty"
			// buttons, even though the array properties may differ in the number of elements.
			if ( !bArrayPropertiesCanDifferInSize )
			{
				const UObject* TempObject = GetUObject(0);

				if (ArrayProp)
				{
					int32 const Num = FScriptArrayHelper::Num(InNode.GetValueBaseAddress((uint8*)TempObject));
					for (int32 ObjIndex = 1; ObjIndex < GetNumObjects(); ObjIndex++)
					{
						TempObject = GetUObject(ObjIndex);
						if (TempObject && Num != FScriptArrayHelper::Num(InNode.GetValueBaseAddress((uint8*)TempObject)))
						{
							bAllTheSame = false;
						}
					}
				}
				else if (SetProp)
				{
					int32 const Num = FScriptSetHelper::Num(InNode.GetValueBaseAddress((uint8*)TempObject));
					for (int32 ObjIndex = 1; ObjIndex < GetNumObjects(); ++ObjIndex)
					{
						TempObject = GetUObject(ObjIndex);
						if (TempObject && Num != FScriptSetHelper::Num(InNode.GetValueBaseAddress((uint8*)TempObject)))
						{
							bAllTheSame = false;
						}
					}
				}
				else if (MapProp)
				{
					int32 const Num = FScriptMapHelper::Num(InNode.GetValueBaseAddress((uint8*)TempObject));
					for (int32 ObjIndex = 1; ObjIndex < GetNumObjects(); ++ObjIndex)
					{
						TempObject = GetUObject(ObjIndex);
						if (TempObject && Num != FScriptMapHelper::Num(InNode.GetValueBaseAddress((uint8*)TempObject)))
						{
							bAllTheSame = false;
						}
					}
				}
			}
		}
		else
		{
			if ( bComparePropertyContents || !Cast<UObjectPropertyBase>(InItemProperty) || bObjectForceCompare )
			{
				// Make sure the value of this InItemProperty is the same in all selected objects.
				for( int32 ObjIndex = 1 ; ObjIndex < GetNumObjects() ; ObjIndex++ )
				{
					const UObject* TempObject = GetUObject(ObjIndex);
					if( !InItemProperty->Identical( Base, InNode.GetValueBaseAddress( (uint8*)TempObject ) ) )
					{
						bAllTheSame = false;
					}
				}
			}
			else
			{
				if ( Cast<UObjectPropertyBase>(InItemProperty) )
				{
					// We don't want to exactly compare component properties.  However, we
					// need to be sure that all references are either valid or invalid.

					// If BaseObj is NON-NULL, all other objects' properties should also be non-NULL.
					// If BaseObj is NULL, all other objects' properties should also be NULL.
					UObject* BaseObj = Cast<UObjectPropertyBase>(InItemProperty)->GetObjectPropertyValue(Base);

					for( int32 ObjIndex = 1 ; ObjIndex < GetNumObjects() ; ObjIndex++ )
					{
						const UObject* TempObject = GetUObject(ObjIndex);
						UObject* CurObj = Cast<UObjectPropertyBase>(InItemProperty)->GetObjectPropertyValue(InNode.GetValueBaseAddress( (uint8*)TempObject ));
						if (   ( !BaseObj && CurObj )			// BaseObj is NULL, but this InItemProperty is non-NULL!
							|| ( BaseObj && !CurObj ) )			// BaseObj is non-NULL, but this InItemProperty is NULL!
						{

							bAllTheSame = false;
						}
					}
				}
			}
		}
	}

	if(OutAddresses != nullptr)
	{
		// Write addresses to the output.
		for(int32 ObjIndex = 0; ObjIndex < GetNumObjects(); ++ObjIndex)
		{
			const UObject* TempObject = GetUObject(ObjIndex);
			if(TempObject)
			{
				OutAddresses->Add(TempObject, InNode.GetValueBaseAddress((uint8*)(TempObject)));
			}
		}
	}

	// Everything checked out and we have usable addresses.
	return bAllTheSame;
}

/**
 * fills in the OutAddresses array with the addresses of all of the available objects.
 * @param InItem		The property to get objects from.
 * @param OutAddresses	Storage array for all of the objects' addresses.
 */
bool FObjectPropertyNode::GetReadAddressUncached( FPropertyNode& InNode, FReadAddressListData& OutAddresses ) const
{
	// Are any objects selected for property editing?
	if( !GetNumObjects())
	{
		return false;
	}

	UProperty* InItemProperty = InNode.GetProperty();
	// Is there a InItemProperty bound to the InItemProperty window?
	if( !InItemProperty )
	{
		return false;
	}


	// Write addresses to the output.
	for ( int32 ObjectIdx = 0 ; ObjectIdx < GetNumObjects() ; ++ObjectIdx )
	{
		const UObject* TempObject = GetUObject(ObjectIdx);

		if( TempObject != NULL )
		{
			OutAddresses.Add( TempObject, InNode.GetValueBaseAddress( (uint8*)(TempObject) ) );
		}
	}

	// Everything checked out and we have usable addresses.
	return true;
}


/**
 * Calculates the memory address for the data associated with this item's property.  This is typically the value of a UProperty or a UObject address.
 *
 * @param	StartAddress	the location to use as the starting point for the calculation; typically the address of the object that contains this property.
 *
 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to an FArray*)
 */
uint8* FObjectPropertyNode::GetValueBaseAddress( uint8* StartAddress )
{
	uint8* Result = StartAddress;

	UClass* ClassObject;
	if ( (ClassObject=Cast<UClass>((UObject*)Result)) != NULL )
	{
		Result = (uint8*)ClassObject->GetDefaultObject();
	}

	return Result;
}


void FObjectPropertyNode::InitBeforeNodeFlags()
{
	StoredProperty = Property;
	Property = NULL;

	Finalize();
}

void FObjectPropertyNode::InitChildNodes()
{
	InternalInitChildNodes();
}

void FObjectPropertyNode::InternalInitChildNodes( FName SinglePropertyName )
{
	HiddenCategories.Empty();
	// Assemble a list of category names by iterating over all fields of BaseClass.

	// build a list of classes that we need to look at
	TSet<UClass*> ClassesToConsider;
	for( int32 i = 0; i < GetNumObjects(); ++i )
	{
		UObject* TempObject = GetUObject( i );
		if( TempObject )
		{
			ClassesToConsider.Add( TempObject->GetClass() );
		}
	}

	const bool bShouldShowHiddenProperties = !!HasNodeFlags(EPropertyNodeFlags::ShouldShowHiddenProperties);
	const bool bShouldShowDisableEditOnInstance = !!HasNodeFlags(EPropertyNodeFlags::ShouldShowDisableEditOnInstance);

	TSet<FName> Categories;
	for( TFieldIterator<UProperty> It(BaseClass.Get()); It; ++It )
	{
		bool bHidden = false;

		FName CategoryName = FObjectEditorUtils::GetCategoryFName(*It);

		for( UClass* Class : ClassesToConsider )
		{
			if( FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, CategoryName.ToString()) )
			{
				HiddenCategories.Add( CategoryName );

				bHidden = true;
				break;
			}
		}

		// 
		static const FName Name_InlineEditConditionToggle("InlineEditConditionToggle");
		if (It->HasMetaData(Name_InlineEditConditionToggle))
		{
			bHidden = true;
		}

		bool bMetaDataAllowVisible = true;
		static const FName Name_bShowOnlyWhenTrue("bShowOnlyWhenTrue");
		const FString& MetaDataVisibilityCheckString = It->GetMetaData(Name_bShowOnlyWhenTrue);
		if (MetaDataVisibilityCheckString.Len())
		{
			//ensure that the metadata visibility string is actually set to true in order to show this property
			GConfig->GetBool(TEXT("UnrealEd.PropertyFilters"), *MetaDataVisibilityCheckString, bMetaDataAllowVisible, GEditorPerProjectIni);
		}

		if (bMetaDataAllowVisible)
		{
			const bool bShowIfNonHiddenEditableProperty = (*It)->HasAnyPropertyFlags(CPF_Edit) && !bHidden;
			const bool bShowIfDisableEditOnInstance = !(*It)->HasAnyPropertyFlags(CPF_DisableEditOnInstance) || bShouldShowDisableEditOnInstance;
			if( bShouldShowHiddenProperties || (bShowIfNonHiddenEditableProperty && bShowIfDisableEditOnInstance) )
			{
				Categories.Add( CategoryName );
			}
		}
	}

	//////////////////////////////////////////
	// Add the category headers and the child items that belong inside of them.

	// Only show category headers if this is the top level object window and the parent window allows headers.
	if( HasNodeFlags(EPropertyNodeFlags::ShowCategories) )
	{
		FString CategoryDelimiterString;
		CategoryDelimiterString.AppendChar( FPropertyNodeConstants::CategoryDelimiterChar );

		TArray< FPropertyNode* > ParentNodesToSort;

		for( const FName& FullCategoryPath : Categories )
		{
			// Figure out the nesting level for this category
			TArray< FString > FullCategoryPathStrings;
			FullCategoryPath.ToString().ParseIntoArray( FullCategoryPathStrings, *CategoryDelimiterString, true );

			TSharedPtr<FPropertyNode> ParentLevelNode = SharedThis(this);
			FString CurCategoryPathString;
			for( int32 PathLevelIndex = 0; PathLevelIndex < FullCategoryPathStrings.Num(); ++PathLevelIndex )
			{
				// Build up the category path name for the current path level index
				if( CurCategoryPathString.Len() != 0 )
				{
					CurCategoryPathString += FPropertyNodeConstants::CategoryDelimiterChar;
				}
				CurCategoryPathString += FullCategoryPathStrings[ PathLevelIndex ];
				const FName CategoryName( *CurCategoryPathString );

				// Check to see if we've already created a category at the specified path level
				bool bFoundMatchingCategory = false;
				{
					for( int32 CurNodeIndex = 0; CurNodeIndex < ParentLevelNode->GetNumChildNodes(); ++CurNodeIndex )
					{
						TSharedPtr<FPropertyNode>& ChildNode = ParentLevelNode->GetChildNode( CurNodeIndex );
						check( ChildNode.IsValid() );

						// Is this a category node?
						FCategoryPropertyNode* ChildCategoryNode = ChildNode->AsCategoryNode();
						if( ChildCategoryNode != NULL )
						{
							// Does the name match?
							if( ChildCategoryNode->GetCategoryName() == CategoryName )
							{
								// Descend by using the child node as the new parent
								bFoundMatchingCategory = true;
								ParentLevelNode = ChildNode;
								break;
							}
						}
					}
				}

				// If we didn't find the category, then we'll need to create it now!
				if( !bFoundMatchingCategory )
				{
					// Create the category node and assign it to its parent node
					TSharedPtr<FCategoryPropertyNode> NewCategoryNode( new FCategoryPropertyNode );
					{
						NewCategoryNode->SetCategoryName( CategoryName );

						FPropertyNodeInitParams InitParams;
						InitParams.ParentNode = ParentLevelNode;
						InitParams.Property = NULL;
						InitParams.ArrayOffset = 0;
						InitParams.ArrayIndex = INDEX_NONE;
						InitParams.bAllowChildren = true;
						InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;
						InitParams.bCreateDisableEditOnInstanceNodes = bShouldShowDisableEditOnInstance;

						NewCategoryNode->InitNode( InitParams );

						// Recursively expand category properties if the category has been flagged for auto-expansion.
						if (BaseClass->IsAutoExpandCategory(*CategoryName.ToString())
							&&	!BaseClass->IsAutoCollapseCategory(*CategoryName.ToString()))
						{
							NewCategoryNode->SetNodeFlags(EPropertyNodeFlags::Expanded, true);
						}

						// Add this node to it's parent.  Note that no sorting happens here, so the parent's
						// list of child nodes will not be in the correct order.  We'll keep track of which
						// nodes we added children to so we can sort them after we're finished adding new nodes.
						ParentLevelNode->AddChildNode(NewCategoryNode);
						ParentNodesToSort.AddUnique( ParentLevelNode.Get() );
					}

					// Descend into the newly created category by using this node as the new parent
					ParentLevelNode = NewCategoryNode;
				}
			}
		}
	}
	else
	{
		// Iterate over all fields, creating items.
		for( TFieldIterator<UProperty> It(BaseClass.Get()); It; ++It )
		{
			static const FName Name_InlineEditConditionToggle("InlineEditConditionToggle");
			const bool bOnlyShowAsInlineEditCondition = (*It)->HasMetaData(Name_InlineEditConditionToggle);
			const bool bShowIfNonHiddenEditableProperty = (*It)->HasAnyPropertyFlags(CPF_Edit) && !FEditorCategoryUtils::IsCategoryHiddenFromClass(BaseClass.Get(), FObjectEditorUtils::GetCategory(*It));
			const bool bShowIfDisableEditOnInstance = !(*It)->HasAnyPropertyFlags(CPF_DisableEditOnInstance) || bShouldShowDisableEditOnInstance;
			if (bShouldShowHiddenProperties || (bShowIfNonHiddenEditableProperty && !bOnlyShowAsInlineEditCondition && bShowIfDisableEditOnInstance))
			{
				UProperty* CurProp = *It;
				if( SinglePropertyName == NAME_None || CurProp->GetFName() == SinglePropertyName )
				{
					TSharedPtr<FItemPropertyNode> NewItemNode( new FItemPropertyNode );

					FPropertyNodeInitParams InitParams;
					InitParams.ParentNode = SharedThis(this);
					InitParams.Property = CurProp;
					InitParams.ArrayOffset =  0;
					InitParams.ArrayIndex = INDEX_NONE;
					InitParams.bAllowChildren = SinglePropertyName == NAME_None;
					InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;
					InitParams.bCreateDisableEditOnInstanceNodes = bShouldShowDisableEditOnInstance;

					NewItemNode->InitNode( InitParams );

					AddChildNode(NewItemNode);

					if( SinglePropertyName != NAME_None )
					{
						// Generate no other children
						break;
					}
				}
			}
		}
	}

}


TSharedPtr<FPropertyNode> FObjectPropertyNode::GenerateSingleChild( FName ChildPropertyName )
{
	bool bDestroySelf = false;
	DestroyTree(bDestroySelf);

	// No category nodes should be created in single property mode
	SetNodeFlags( EPropertyNodeFlags::ShowCategories, false );

	InternalInitChildNodes( ChildPropertyName );

	if( ChildNodes.Num() > 0 )
	{
		// only one node should be been created
		check( ChildNodes.Num() == 1);

		return ChildNodes[0];
	}

	return NULL;
}

/**
 * Appends my path, including an array index (where appropriate)
 */
bool FObjectPropertyNode::GetQualifiedName(FString& PathPlusIndex, bool bWithArrayIndex, const FPropertyNode* StopParent, bool bIgnoreCategories ) const
{
	bool bAddedAnything = false;
	if( ParentNode && ParentNode != StopParent )
	{
		bAddedAnything = ParentNode->GetQualifiedName(PathPlusIndex, bWithArrayIndex, StopParent, bIgnoreCategories);
		if( bAddedAnything )
		{
			PathPlusIndex += TEXT(".");
		}
	}

	bAddedAnything = true;
	PathPlusIndex += TEXT("Object");

	return bAddedAnything;
}

// Looks at the Objects array and returns the best base class.  Called by
// Finalize(); that is, when the list of selected objects is being finalized.
void FObjectPropertyNode::SetBestBaseClass()
{
	BaseClass = NULL;

	for( int32 x = 0 ; x < Objects.Num() ; ++x )
	{
		UObject* Obj = Objects[x].Get();
		
		if( Obj )
		{
			UClass* ObjClass = Cast<UClass>(Obj);
			if (ObjClass == NULL)
			{
				ObjClass = Obj->GetClass();
			}
			check( ObjClass );

			// Initialize with the class of the first object we encounter.
			if( BaseClass == NULL )
			{
				BaseClass = ObjClass;
			}

			// If we've encountered an object that's not a subclass of the current best baseclass,
			// climb up a step in the class hierarchy.
			while( !ObjClass->IsChildOf( BaseClass.Get() ) )
			{
				BaseClass = BaseClass->GetSuperClass();
			}
		}
	}
}
