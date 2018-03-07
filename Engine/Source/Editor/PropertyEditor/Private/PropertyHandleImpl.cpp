// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PropertyHandleImpl.h"
#include "GameFramework/Actor.h"
#include "Editor.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"
#include "ObjectPropertyNode.h"
#include "Misc/MessageDialog.h"
#include "Misc/App.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/TextProperty.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "PropertyEditorHelpers.h"
#include "StructurePropertyNode.h"
#include "ScopedTransaction.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Selection.h"
#include "ItemPropertyNode.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "EnumProperty.h"
#include "IDetailPropertyRow.h"

#define LOCTEXT_NAMESPACE "PropertyHandleImplementation"

FPropertyValueImpl::FPropertyValueImpl( TSharedPtr<FPropertyNode> InPropertyNode, FNotifyHook* InNotifyHook, TSharedPtr<IPropertyUtilities> InPropertyUtilities )
	: PropertyNode( InPropertyNode )
	, PropertyUtilities( InPropertyUtilities )
	, NotifyHook( InNotifyHook )
	, bInteractiveChangeInProgress( false )
	, InvalidOperationError( nullptr )
{

}

FPropertyValueImpl::~FPropertyValueImpl()
{
}

void FPropertyValueImpl::EnumerateObjectsToModify( FPropertyNode* InPropertyNode, const EnumerateObjectsToModifyFuncRef& InObjectsToModifyCallback ) const
{
	// Find the parent object node which contains offset addresses for reading a property value on an object
	FComplexPropertyNode* ComplexNode = InPropertyNode->FindComplexParent();
	if (ComplexNode)
	{
		const bool bIsStruct = FComplexPropertyNode::EPT_StandaloneStructure == ComplexNode->GetPropertyType();
		const int32 NumInstances = ComplexNode->GetInstancesNum();
		for (int32 Index = 0; Index < NumInstances; ++Index)
		{
			UObject*	Object = ComplexNode->GetInstanceAsUObject(Index).Get();
			uint8*		Addr = InPropertyNode->GetValueBaseAddress(ComplexNode->GetMemoryOfInstance(Index));
			if (!InObjectsToModifyCallback(FObjectBaseAddress(Object, Addr, bIsStruct), Index, NumInstances))
			{
				break;
			}
		}
	}
}

void FPropertyValueImpl::GetObjectsToModify( TArray<FObjectBaseAddress>& ObjectsToModify, FPropertyNode* InPropertyNode ) const
{
	EnumerateObjectsToModify(InPropertyNode, [&](const FObjectBaseAddress& ObjectToModify, const int32 ObjectIndex, const int32 NumObjects) -> bool
	{
		if (ObjectIndex == 0)
		{
			ObjectsToModify.Reserve(ObjectsToModify.Num() + NumObjects);
		}
		ObjectsToModify.Add(ObjectToModify);
		return true;
	});
}

FPropertyAccess::Result FPropertyValueImpl::GetPropertyValueString( FString& OutString, FPropertyNode* InPropertyNode, const bool bAllowAlternateDisplayValue, EPropertyPortFlags PortFlags ) const
{
	FPropertyAccess::Result Result = FPropertyAccess::Success;

	uint8* ValueAddress = nullptr;
	FReadAddressList ReadAddresses;
	bool bAllValuesTheSame = InPropertyNode->GetReadAddress( !!InPropertyNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses, false, true );

	if( (ReadAddresses.Num() > 0 && bAllValuesTheSame) || ReadAddresses.Num() == 1 ) 
	{
		ValueAddress = ReadAddresses.GetAddress(0);

		if( ValueAddress != nullptr )
		{
			UProperty* Property = InPropertyNode->GetProperty();

			// Check for bogus data
			if( Property != nullptr && InPropertyNode->GetParentNode() != nullptr )
			{
				Property->ExportText_Direct( OutString, ValueAddress, ValueAddress, nullptr, PortFlags );

				UEnum* Enum = nullptr;
				int64 EnumValue = 0;
				if ( UByteProperty* ByteProperty = Cast<UByteProperty>(Property) )
				{
					if ( ByteProperty->Enum != nullptr )
					{
						Enum = ByteProperty->Enum;
						EnumValue = ByteProperty->GetPropertyValue(ValueAddress);
					}
				}
				else if ( UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property) )
				{
					Enum = EnumProperty->GetEnum();
					EnumValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValueAddress);
				}

				if ( Enum != nullptr )
				{
					if (Enum->IsValidEnumValue(EnumValue))
					{
						// See if we specified an alternate name for this value using metadata
						OutString = Enum->GetDisplayNameTextByValue(EnumValue).ToString();
						if(!bAllowAlternateDisplayValue || OutString.Len() == 0) 
						{
							OutString = Enum->GetNameStringByValue(EnumValue);
						}
					}
					else
					{
						Result = FPropertyAccess::Fail;
					}
				}
			}
			else
			{
				Result = FPropertyAccess::Fail;
			}
		}

	}
	else
	{
		Result = ReadAddresses.Num() > 1 ? FPropertyAccess::MultipleValues : FPropertyAccess::Fail;
	}

	return Result;
}

FPropertyAccess::Result FPropertyValueImpl::GetPropertyValueText( FText& OutText, FPropertyNode* InPropertyNode, const bool bAllowAlternateDisplayValue ) const
{
	FPropertyAccess::Result Result = FPropertyAccess::Success;

	uint8* ValueAddress = nullptr;
	FReadAddressList ReadAddresses;
	bool bAllValuesTheSame = InPropertyNode->GetReadAddress( !!InPropertyNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses, false, true );

	if( ReadAddresses.Num() > 0 && InPropertyNode->GetProperty() != nullptr && (bAllValuesTheSame || ReadAddresses.Num() == 1) ) 
	{
		ValueAddress = ReadAddresses.GetAddress(0);

		if( ValueAddress != nullptr )
		{
			UProperty* Property = InPropertyNode->GetProperty();

			if(Property->IsA(UTextProperty::StaticClass()))
			{
				OutText = Cast<UTextProperty>(Property)->GetPropertyValue(ValueAddress);
			}
			else
			{
				FString ExportedTextString;
				Property->ExportText_Direct(ExportedTextString, ValueAddress, ValueAddress, nullptr, PPF_PropertyWindow );

				UEnum* Enum = nullptr;
				int64 EnumValue = 0;
				if (UByteProperty* ByteProperty = Cast<UByteProperty>(Property))
				{
					Enum = ByteProperty->Enum;
					EnumValue = ByteProperty->GetPropertyValue(ValueAddress);
				}
				else if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
				{
					Enum = EnumProperty->GetEnum();
					EnumValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValueAddress);
				}

				if (Enum)
				{
					if (Enum->IsValidEnumValue(EnumValue))
					{
						// Text form is always display name
						OutText = Enum->GetDisplayNameTextByValue(EnumValue);
					}
					else
					{
						Result = FPropertyAccess::Fail;
					}
				}
				else
				{
					OutText = FText::FromString(ExportedTextString);
				}
			}
		}

	}
	else
	{
		Result = ReadAddresses.Num() > 1 ? FPropertyAccess::MultipleValues : FPropertyAccess::Fail;
	}

	return Result;
}

FPropertyAccess::Result FPropertyValueImpl::GetValueData( void*& OutAddress ) const
{
	FPropertyAccess::Result Res = FPropertyAccess::Fail;
	OutAddress = nullptr;
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if( PropertyNodePin.IsValid() )
	{
		uint8* ValueAddress = nullptr;
		FReadAddressList ReadAddresses;
		bool bAllValuesTheSame = PropertyNodePin->GetReadAddress( !!PropertyNodePin->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses, false, true );

		if( (ReadAddresses.Num() > 0 && bAllValuesTheSame) || ReadAddresses.Num() == 1 ) 
		{
			ValueAddress = ReadAddresses.GetAddress(0);
			const UProperty* Property = PropertyNodePin->GetProperty();
			if (ValueAddress && Property)
			{
				const int32 Index = 0;
				OutAddress = ValueAddress + Index * Property->ElementSize;
				Res = FPropertyAccess::Success;
			}
		}
		else if (ReadAddresses.Num() > 1)
		{
			Res = FPropertyAccess::MultipleValues;
		}
	}

	return Res;
}

FPropertyAccess::Result FPropertyValueImpl::ImportText( const FString& InValue, EPropertyValueSetFlags::Type Flags )
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if( PropertyNodePin.IsValid() && !PropertyNodePin->IsEditConst() )
	{
		return ImportText( InValue, PropertyNodePin.Get(), Flags );
	}

	// The property node is not valid or cant be set.  If not valid it probably means this value was stored somewhere and selection changed causing the node to be destroyed
	return FPropertyAccess::Fail;
}

FString FPropertyValueImpl::GetPropertyValueArray() const
{
	FString String;
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if( PropertyNodePin.IsValid() )
	{
		FReadAddressList ReadAddresses;

		bool bSingleValue = PropertyNodePin->GetReadAddress( !!PropertyNodePin->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses, false );

		if( bSingleValue )
		{
			UProperty* NodeProperty = PropertyNodePin->GetProperty();
			if( NodeProperty != nullptr )
			{
				uint8* Addr = ReadAddresses.GetAddress(0);
				if( Addr )
				{
					if ( NodeProperty->IsA<UArrayProperty>() )
					{
						String = FString::Printf( TEXT("%(%d)"), FScriptArrayHelper::Num(Addr) );
					}
					else if ( Cast<USetProperty>(NodeProperty) != nullptr )	
					{
						String = FString::Printf( TEXT("%(%d)"), FScriptSetHelper::Num(Addr) );
					}
					else if (Cast<UMapProperty>(NodeProperty) != nullptr)
					{
						String = FString::Printf(TEXT("%(%d)"), FScriptMapHelper::Num(Addr));
					}
					else
					{
						String = FString::Printf( TEXT("%[%d]"), NodeProperty->ArrayDim );
					}
				}
			}
		}
		else
		{
			String = NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values").ToString();
		}
	}

	return String;
}

bool FPropertyValueImpl::SendTextToObjectProperty( const FString& Text, EPropertyValueSetFlags::Type Flags )
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if( PropertyNodePin.IsValid() )
	{
		FComplexPropertyNode* ParentNode = PropertyNodePin->FindComplexParent();

		// If more than one object is selected, an empty field indicates their values for this property differ.
		// Don't send it to the objects value in this case (if we did, they would all get set to None which isn't good).
		if ((!ParentNode || ParentNode->GetInstancesNum() > 1) && !Text.Len())
		{
			return false;
		}

		return ImportText( Text, PropertyNodePin.Get(), Flags ) != FPropertyAccess::Fail;

	}

	return false;
}

void FPropertyValueImpl::GenerateArrayIndexMapToObjectNode( TMap<FString,int32>& OutArrayIndexMap, FPropertyNode* PropertyNode )
{
	if( PropertyNode )
	{
		OutArrayIndexMap.Empty();
		for (FPropertyNode* IterationNode = PropertyNode; (IterationNode != nullptr) && (IterationNode->AsObjectNode() == nullptr); IterationNode = IterationNode->GetParentNode())
		{
			UProperty* Property = IterationNode->GetProperty();
			if (Property)
			{
				//since we're starting from the lowest level, we have to take the first entry.  In the case of an array, the entries and the array itself have the same name, except the parent has an array index of -1
				if (!OutArrayIndexMap.Contains(Property->GetName()))
				{
					OutArrayIndexMap.Add(Property->GetName(), IterationNode->GetArrayIndex());
				}
			}
		}
	}
}

FPropertyAccess::Result FPropertyValueImpl::ImportText( const FString& InValue, FPropertyNode* InPropertyNode, EPropertyValueSetFlags::Type Flags )
{
	TArray<FObjectBaseAddress> ObjectsToModify;
	GetObjectsToModify( ObjectsToModify, InPropertyNode );

	TArray<FString> Values;
	for( int32 ObjectIndex = 0 ; ObjectIndex < ObjectsToModify.Num() ; ++ObjectIndex )
	{
		if (ObjectsToModify[ObjectIndex].Object != nullptr || ObjectsToModify[ObjectIndex].bIsStruct)
		{
			Values.Add( InValue );
		}
	}

	FPropertyAccess::Result Result = FPropertyAccess::Fail;
	if( Values.Num() > 0 )
	{
		Result = ImportText( ObjectsToModify, Values, InPropertyNode, Flags );
	}

	return Result;
}

FPropertyAccess::Result FPropertyValueImpl::ImportText( const TArray<FObjectBaseAddress>& InObjects, const TArray<FString>& InValues, FPropertyNode* InPropertyNode, EPropertyValueSetFlags::Type Flags )
{
	check(InPropertyNode);
	UProperty* NodeProperty = InPropertyNode->GetProperty();

	FPropertyAccess::Result Result = FPropertyAccess::Success;

	if( !NodeProperty )
	{
		// The property has been deleted out from under this
		Result = FPropertyAccess::Fail;
	}
	else if( NodeProperty->IsA<UObjectProperty>() || NodeProperty->IsA<UNameProperty>() )
	{
		// certain properties have requirements on the size of string values that can be imported.  Search for strings that are too large.
		for( int32 ValueIndex = 0; ValueIndex < InValues.Num(); ++ValueIndex )
		{
			if( InValues[ValueIndex].Len() > NAME_SIZE )
			{
				Result = FPropertyAccess::Fail;
				break;
			}
		}
	}

	if( Result != FPropertyAccess::Fail )
	{
		UWorld* OldGWorld = nullptr;

		bool bIsGameWorld = false;
		// If the object we are modifying is in the PIE world, than make the PIE world the active
		// GWorld.  Assumes all objects managed by this property window belong to the same world.
		if (UPackage* ObjectPackage = (InObjects[0].Object ? InObjects[0].Object->GetOutermost() : nullptr))
		{
			const bool bIsPIEPackage = ObjectPackage->HasAnyPackageFlags(PKG_PlayInEditor);
			if (GUnrealEd && GUnrealEd->PlayWorld && bIsPIEPackage && !GIsPlayInEditorWorld)
			{
				OldGWorld = SetPlayInEditorWorld(GUnrealEd->PlayWorld);
				bIsGameWorld = true;
			}
		}
		///////////////

		// Send the values and assemble a list of pre/posteditchange values.
		bool bNotifiedPreChange = false;
		UObject *NotifiedObj = nullptr;
		TArray< TMap<FString,int32> > ArrayIndicesPerObject;

		const bool bTransactable = (Flags & EPropertyValueSetFlags::NotTransactable) == 0;
		const bool bFinished = ( Flags & EPropertyValueSetFlags::InteractiveChange) == 0;
		
		// List of top level objects sent to the PropertyChangedEvent
		TArray<const UObject*> TopLevelObjects;
		TopLevelObjects.Reserve(InObjects.Num());

		for ( int32 ObjectIndex = 0 ; ObjectIndex < InObjects.Num() ; ++ObjectIndex )
		{	
			const FObjectBaseAddress& Cur = InObjects[ ObjectIndex ];
			if (Cur.BaseAddress == nullptr)
			{
				//Fully abort this procedure.  The data has changed out from under the object
				Result = FPropertyAccess::Fail;
				break;
			}

			// Cache the value of the property before modifying it.
			FString PreviousValue;
			NodeProperty->ExportText_Direct(PreviousValue, Cur.BaseAddress, Cur.BaseAddress, nullptr, 0 );

			// If this property is the inner-property of a container, cache the current value as well
			FString PreviousContainerValue;
			if (Cur.Object)
			{
				FPropertyNode* ParentNode = InPropertyNode->GetParentNode();
				UProperty* Property = ParentNode ? ParentNode->GetProperty() : nullptr;

				bool bIsInContainer = false;

				if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property))
				{
					bIsInContainer = (ArrayProperty->Inner == NodeProperty);
				}
				else if (USetProperty* SetProp = Cast<USetProperty>(Property))
				{
					// If the element is part of a set, check for duplicate elements
					bIsInContainer = SetProp->ElementProp == NodeProperty;

					if (bIsInContainer)
					{
						FScriptSetHelper SetHelper(SetProp, ParentNode->GetValueBaseAddress((uint8*)Cur.Object));

						if ( SetHelper.HasElement(Cur.BaseAddress, InValues[ObjectIndex]) )
						{
							// Duplicate element in the set
							ShowInvalidOperationError(LOCTEXT("DuplicateSetElement", "Duplicate elements are not allowed in Set properties."));

							return FPropertyAccess::Fail;
						}
					}
				}
				else if (UMapProperty* MapProperty = Cast<UMapProperty>(Property))
				{
					bIsInContainer = MapProperty->KeyProp == NodeProperty;

					if (bIsInContainer)
					{
						FScriptMapHelper MapHelper(MapProperty, ParentNode->GetValueBaseAddress((uint8*)Cur.Object));

						if ( MapHelper.HasKey(Cur.BaseAddress, InValues[ObjectIndex]) )
						{
							// Duplicate key in the map
							ShowInvalidOperationError(LOCTEXT("DuplicateMapKey", "Duplicate keys are not allowed in Map properties."));

							return FPropertyAccess::Fail;
						}
					}
					else
					{
						bIsInContainer = MapProperty->ValueProp == NodeProperty;
					}
				}

				if (bIsInContainer)
				{
					uint8* Addr = ParentNode->GetValueBaseAddress((uint8*)Cur.Object);
					Property->ExportText_Direct(PreviousContainerValue, Addr, Addr, nullptr, 0);
				}
			}

			// Check if we need to call PreEditChange on all objects.
			// Remove quotes from the original value because FName properties  
			// are wrapped in quotes before getting here. This causes the 
			// string comparison to fail even when the name is unchanged. 
			if ( !bNotifiedPreChange && ( FCString::Strcmp(*InValues[ObjectIndex].TrimQuotes(), *PreviousValue) != 0 || ( bFinished && bInteractiveChangeInProgress ) ) )
			{
				bNotifiedPreChange = true;
				NotifiedObj = Cur.Object;

				if (!bInteractiveChangeInProgress)
				{
					// Begin a transaction only if we need to call PreChange
					if (GEditor && bTransactable)
					{
						GEditor->BeginTransaction(TEXT("PropertyEditor"), FText::Format(NSLOCTEXT("PropertyEditor", "EditPropertyTransaction", "Edit {0}"), InPropertyNode->GetDisplayName()), NodeProperty);
					}
				}

				InPropertyNode->NotifyPreChange(NodeProperty, NotifyHook);

				bInteractiveChangeInProgress = (Flags & EPropertyValueSetFlags::InteractiveChange) != 0;
			}

			// Set the new value.
			const TCHAR* NewValue = *InValues[ObjectIndex];
			NodeProperty->ImportText( NewValue, Cur.BaseAddress, 0, Cur.Object );

			if (Cur.Object)
			{
				// Cache the value of the property after having modified it.
				FString ValueAfterImport;
				NodeProperty->ExportText_Direct(ValueAfterImport, Cur.BaseAddress, Cur.BaseAddress, nullptr, 0);

				if ((Cur.Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) ||
					(Cur.Object->HasAnyFlags(RF_DefaultSubObject) && Cur.Object->GetOuter()->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))) &&
					!bIsGameWorld)
				{
					InPropertyNode->PropagatePropertyChange(Cur.Object, NewValue, PreviousContainerValue.IsEmpty() ? PreviousValue : PreviousContainerValue);
				}

				// If the values before and after setting the property differ, mark the object dirty.
				if (FCString::Strcmp(*PreviousValue, *ValueAfterImport) != 0)
				{
					Cur.Object->MarkPackageDirty();

					// For TMap and TSet, we need to rehash it in case a key was modified
					if (NodeProperty->GetOuter()->IsA<UMapProperty>())
					{
						uint8* Addr = InPropertyNode->GetParentNode()->GetValueBaseAddress((uint8*)Cur.Object);
						FScriptMapHelper MapHelper(Cast<UMapProperty>(NodeProperty->GetOuter()), Addr);
						MapHelper.Rehash();
					}
					else if (NodeProperty->GetOuter()->IsA<USetProperty>())
					{
						uint8* Addr = InPropertyNode->GetParentNode()->GetValueBaseAddress((uint8*)Cur.Object);
						FScriptSetHelper SetHelper(Cast<USetProperty>(NodeProperty->GetOuter()), Addr);
						SetHelper.Rehash();
					}
				}

				TopLevelObjects.Add(Cur.Object);
			}

			//add on array index so we can tell which entry just changed
			ArrayIndicesPerObject.Add(TMap<FString,int32>());
			FPropertyValueImpl::GenerateArrayIndexMapToObjectNode( ArrayIndicesPerObject[ObjectIndex], InPropertyNode );
		}

		FPropertyChangedEvent ChangeEvent(NodeProperty, bFinished ? EPropertyChangeType::ValueSet : EPropertyChangeType::Interactive, &TopLevelObjects);
		ChangeEvent.SetArrayIndexPerObject(ArrayIndicesPerObject);

		// If PreEditChange was called, so should PostEditChange.
		if (bNotifiedPreChange)
		{
			// Call PostEditChange on all objects.
			InPropertyNode->NotifyPostChange( ChangeEvent, NotifyHook );

			if (bFinished)
			{
				bInteractiveChangeInProgress = false;

				if (bTransactable)
				{
					// End the transaction if we called PreChange
					GEditor->EndTransaction();
				}
			}
		}

		if (OldGWorld)
		{
			// restore the original (editor) GWorld
			RestoreEditorWorld( OldGWorld );
		}

		if (PropertyUtilities.IsValid() && !bInteractiveChangeInProgress)
		{
			InPropertyNode->FixPropertiesInEvent(ChangeEvent);
			PropertyUtilities.Pin()->NotifyFinishedChangingProperties(ChangeEvent);
		}
	}

	return Result;
}

void FPropertyValueImpl::EnumerateRawData( const IPropertyHandle::EnumerateRawDataFuncRef& InRawDataCallback )
{
	EnumerateObjectsToModify(PropertyNode.Pin().Get(), [&](const FObjectBaseAddress& ObjectToModify, const int32 ObjectIndex, const int32 NumObjects) -> bool
	{
		return InRawDataCallback(ObjectToModify.BaseAddress, ObjectIndex, NumObjects);
	});
}

void FPropertyValueImpl::EnumerateConstRawData( const IPropertyHandle::EnumerateConstRawDataFuncRef& InRawDataCallback ) const
{
	EnumerateObjectsToModify(PropertyNode.Pin().Get(), [&](const FObjectBaseAddress& ObjectToModify, const int32 ObjectIndex, const int32 NumObjects) -> bool
	{
		return InRawDataCallback(ObjectToModify.BaseAddress, ObjectIndex, NumObjects);
	});
}

void FPropertyValueImpl::AccessRawData( TArray<void*>& RawData )
{
	RawData.Empty();
	EnumerateObjectsToModify(PropertyNode.Pin().Get(), [&](const FObjectBaseAddress& ObjectToModify, const int32 ObjectIndex, const int32 NumObjects) -> bool
	{
		if (ObjectIndex == 0)
		{
			RawData.Reserve(NumObjects);
		}
		RawData.Add(ObjectToModify.BaseAddress);
		return true;
	});
}

void FPropertyValueImpl::AccessRawData( TArray<const void*>& RawData ) const
{
	RawData.Empty();
	EnumerateObjectsToModify(PropertyNode.Pin().Get(), [&](const FObjectBaseAddress& ObjectToModify, const int32 ObjectIndex, const int32 NumObjects) -> bool
	{
		if (ObjectIndex == 0)
		{
			RawData.Reserve(NumObjects);
		}
		RawData.Add(ObjectToModify.BaseAddress);
		return true;
	});
}

void FPropertyValueImpl::SetOnPropertyValueChanged( const FSimpleDelegate& InOnPropertyValueChanged )
{
	if( PropertyNode.IsValid() )
	{
		PropertyNode.Pin()->OnPropertyValueChanged().Add( InOnPropertyValueChanged );
	}
}

void FPropertyValueImpl::SetOnChildPropertyValueChanged( const FSimpleDelegate& InOnChildPropertyValueChanged )
{
	if( PropertyNode.IsValid() )
	{
		PropertyNode.Pin()->OnChildPropertyValueChanged().Add( InOnChildPropertyValueChanged );
	}
}

void FPropertyValueImpl::SetOnPropertyValuePreChange(const FSimpleDelegate& InOnPropertyValuePreChange)
{
	if (PropertyNode.IsValid())
	{
		PropertyNode.Pin()->OnPropertyValuePreChange().Add(InOnPropertyValuePreChange);
	}
}

void FPropertyValueImpl::SetOnChildPropertyValuePreChange(const FSimpleDelegate& InOnChildPropertyValuePreChange)
{
	if (PropertyNode.IsValid())
	{
		PropertyNode.Pin()->OnChildPropertyValuePreChange().Add(InOnChildPropertyValuePreChange);
	}
}

void FPropertyValueImpl::SetOnRebuildChildren( const FSimpleDelegate& InOnRebuildChildren )
{
	if( PropertyNode.IsValid() )
	{
		PropertyNode.Pin()->SetOnRebuildChildren( InOnRebuildChildren );
	}
}
/**
 * Gets the max valid index for a array property of an object
 * @param InObjectNode - The parent of the variable being clamped
 * @param InArrayName - The array name we're hoping to clamp to the extents of
 * @return LastValidEntry in the array (if the array is found)
 */
static int32 GetArrayPropertyLastValidIndex( FObjectPropertyNode* InObjectNode, const FString& InArrayName )
{
	int32 ClampMax = MAX_int32;

	check(InObjectNode->GetNumObjects()==1);
	UObject* ParentObject = InObjectNode->GetUObject(0);

	//find the associated property
	UProperty* FoundProperty = nullptr;
	for( TFieldIterator<UProperty> It(ParentObject->GetClass()); It; ++It )
	{
		UProperty* CurProp = *It;
		if (CurProp->GetName()==InArrayName)
		{
			FoundProperty = CurProp;
			break;
		}
	}

	if (FoundProperty && (FoundProperty->ArrayDim == 1))
	{
		UArrayProperty* ArrayProperty = CastChecked<UArrayProperty>( FoundProperty );
		if (ArrayProperty)
		{
			uint8* PropertyAddressBase = ArrayProperty->ContainerPtrToValuePtr<uint8>(ParentObject);
			ClampMax = FScriptArrayHelper::Num(PropertyAddressBase) - 1;
		}
		else
		{
			UE_LOG(LogPropertyNode, Warning, TEXT("The property (%s) passed for array clamping use is not an array.  Clamp will only ensure greater than zero."), *InArrayName);
		}
	}
	else
	{
		UE_LOG(LogPropertyNode, Warning, TEXT("The property (%s) passed for array clamping was not found.  Clamp will only ensure greater than zero."), *InArrayName);
	}

	return ClampMax;
}


FPropertyAccess::Result FPropertyValueImpl::GetValueAsString( FString& OutString, EPropertyPortFlags PortFlags ) const
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();

	FPropertyAccess::Result Res = FPropertyAccess::Success;

	if( PropertyNodePin.IsValid() )
	{
		const bool bAllowAlternateDisplayValue = false;
		Res = GetPropertyValueString( OutString, PropertyNodePin.Get(), bAllowAlternateDisplayValue, PortFlags );
	}
	else
	{
		Res = FPropertyAccess::Fail;
	}

	return Res;
}

FPropertyAccess::Result FPropertyValueImpl::GetValueAsDisplayString( FString& OutString, EPropertyPortFlags PortFlags ) const
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();

	FPropertyAccess::Result Res = FPropertyAccess::Success;

	if( PropertyNodePin.IsValid() )
	{
		const bool bAllowAlternateDisplayValue = true;
		Res = GetPropertyValueString(OutString, PropertyNodePin.Get(), bAllowAlternateDisplayValue, PortFlags);
	}
	else
	{
		Res = FPropertyAccess::Fail;
	}

	return Res;
}

FPropertyAccess::Result FPropertyValueImpl::GetValueAsText( FText& OutText ) const
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();

	FPropertyAccess::Result Res = FPropertyAccess::Success;

	if( PropertyNodePin.IsValid() )
	{
		Res = GetPropertyValueText( OutText, PropertyNodePin.Get(), false/*bAllowAlternateDisplayValue*/ );
	}
	else
	{
		Res = FPropertyAccess::Fail;
	}

	return Res;
}

FPropertyAccess::Result FPropertyValueImpl::GetValueAsDisplayText( FText& OutText ) const
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();

	FPropertyAccess::Result Res = FPropertyAccess::Success;

	if( PropertyNodePin.IsValid() )
	{
		Res = GetPropertyValueText( OutText, PropertyNodePin.Get(), true/*bAllowAlternateDisplayValue*/ );
	}
	else
	{
		Res = FPropertyAccess::Fail;
	}

	return Res;
}

FPropertyAccess::Result FPropertyValueImpl::SetValueAsString( const FString& InValue, EPropertyValueSetFlags::Type Flags )
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;

	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if( PropertyNodePin.IsValid() )
	{
		UProperty* NodeProperty = PropertyNodePin->GetProperty();

		FString Value = InValue;

		// Strip any leading underscores and spaces from names.
		if( NodeProperty && NodeProperty->IsA( UNameProperty::StaticClass() ) )
		{
			while ( true )
			{
				if ( Value.StartsWith( TEXT("_") ) )
				{
					// Strip leading underscores.
					do
					{
						Value = Value.Right( Value.Len()-1 );
					} while ( Value.StartsWith( TEXT("_") ) );
				}
				else if ( Value.StartsWith( TEXT(" ") ) )
				{
					// Strip leading spaces.
					do
					{
						Value = Value.Right( Value.Len()-1 );
					} while ( Value.StartsWith( TEXT(" ") ) );
				}
				else
				{
					// Starting with something valid -- break.
					break;
				}
			}
		}

		// If more than one object is selected, an empty field indicates their values for this property differ.
		// Don't send it to the objects value in this case (if we did, they would all get set to None which isn't good).
		FComplexPropertyNode* ParentNode = PropertyNodePin->FindComplexParent();

		FString PreviousValue;
		GetValueAsString( PreviousValue );

		const bool bDidValueChange = Value.Len() && (FCString::Strcmp(*PreviousValue, *Value) != 0);
		const bool bComingOutOfInteractiveChange = bInteractiveChangeInProgress && ( ( Flags & EPropertyValueSetFlags::InteractiveChange ) != EPropertyValueSetFlags::InteractiveChange );

		if ( ParentNode && ( ParentNode->GetInstancesNum() == 1 || bComingOutOfInteractiveChange || bDidValueChange ) )
		{
			ImportText( Value, PropertyNodePin.Get(), Flags );
		}

		Result = FPropertyAccess::Success;
	}

	return Result;
}

bool FPropertyValueImpl::SetObject( const UObject* NewObject, EPropertyValueSetFlags::Type Flags )
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if( PropertyNodePin.IsValid() )
	{
		FString ObjectPathName = NewObject ? NewObject->GetPathName() : TEXT("None");
		bool bResult = SendTextToObjectProperty( ObjectPathName, Flags );

		// @todo PropertyEditing:  This fails when it should not.  some UProperties remove the object class from the name and some dont. 
		// So comparing the path name to one that adds the object class name will always fail even though the value was set correctly
#if 0
		if( bResult )
		{
			UProperty* NodeProperty = PropertyNodePin->GetProperty();

			const FString CompareName = NewObject ? FString::Printf( TEXT("%s'%s'"), *NewObject->GetClass()->GetName(), *ObjectPathName ) : TEXT("None");

			// Read values back and report any failures.
			TArray<FObjectBaseAddress> ObjectsThatWereModified;
			FObjectPropertyNode* ObjectNode = PropertyNodePin->FindObjectItemParent();
			
			if( ObjectNode )
			{
				bool bAllObjectPropertyValuesMatch = true;
				for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
				{
					TWeakObjectPtr<UObject>	Object = *Itor;
					uint8*		Addr = PropertyNodePin->GetValueBaseAddress( (uint8*) Object.Get() );

					FString PropertyValue;
					NodeProperty->ExportText_Direct(PropertyValue, Addr, Addr, nullptr, 0 );
					if ( PropertyValue != CompareName )
					{
						bAllObjectPropertyValuesMatch = false;
						break;
					}
				}

				// Warn that some object assignments failed.
				if ( !bAllObjectPropertyValuesMatch )
				{
					bResult = false;
				}
			}
			else
			{
				bResult = false;
			}
		}
#endif
		return bResult;
	}

	return false;
}

FPropertyAccess::Result FPropertyValueImpl::OnUseSelected()
{
	FPropertyAccess::Result Res = FPropertyAccess::Fail;
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if( PropertyNodePin.IsValid() )
	{
		UProperty* NodeProperty = PropertyNodePin->GetProperty();

		UObjectPropertyBase* ObjProp = Cast<UObjectPropertyBase>( NodeProperty );
		UInterfaceProperty* IntProp = Cast<UInterfaceProperty>( NodeProperty );
		UClassProperty* ClassProp = Cast<UClassProperty>( NodeProperty );
		USoftClassProperty* SoftClassProperty = Cast<USoftClassProperty>( NodeProperty );
		UClass* const InterfaceThatMustBeImplemented = ObjProp ? ObjProp->GetOwnerProperty()->GetClassMetaData(TEXT("MustImplement")) : nullptr;

		if(ClassProp || SoftClassProperty)
		{
			FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

			const UClass* const SelectedClass = GEditor->GetFirstSelectedClass(ClassProp ? ClassProp->MetaClass : SoftClassProperty->MetaClass);
			if(SelectedClass)
			{
				if (!InterfaceThatMustBeImplemented || SelectedClass->ImplementsInterface(InterfaceThatMustBeImplemented))
				{
					FString const ClassPathName = SelectedClass->GetPathName();

					TArray<FText> RestrictReasons;
					if (PropertyNodePin->IsRestricted(ClassPathName, RestrictReasons))
					{
						check(RestrictReasons.Num() > 0);
						FMessageDialog::Open(EAppMsgType::Ok, RestrictReasons[0]);
					}
					else 
					{
						
						Res = SetValueAsString(ClassPathName, EPropertyValueSetFlags::DefaultFlags);
					}
				}
			}
		}
		else
		{
			FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

			UClass* ObjPropClass = UObject::StaticClass();
			if( ObjProp )
			{
				ObjPropClass = ObjProp->PropertyClass;
			}
			else if( IntProp ) 
			{
				ObjPropClass = IntProp->InterfaceClass;
			}

			bool const bMustBeLevelActor = ObjProp ? ObjProp->GetOwnerProperty()->GetBoolMetaData(TEXT("MustBeLevelActor")) : false;

			// Find best appropriate selected object
			UObject* SelectedObject = nullptr;

			if (bMustBeLevelActor)
			{
				// @todo: ensure at compile time that MustBeLevelActor flag is only set on actor properties

				// looking only for level actors here
				USelection* const SelectedSet = GEditor->GetSelectedActors();
				SelectedObject = SelectedSet->GetTop( ObjPropClass, InterfaceThatMustBeImplemented);
			}
			else
			{
				// normal behavior, where actor classes will look for level actors and 
				USelection* const SelectedSet = GEditor->GetSelectedSet( ObjPropClass );
				SelectedObject = SelectedSet->GetTop( ObjPropClass, InterfaceThatMustBeImplemented );
			}

			if( SelectedObject )
			{
				FString const ObjPathName = SelectedObject->GetPathName();

				TArray<FText> RestrictReasons;
				if (PropertyNodePin->IsRestricted(ObjPathName, RestrictReasons))
				{
					check(RestrictReasons.Num() > 0);
					FMessageDialog::Open(EAppMsgType::Ok, RestrictReasons[0]);
				}
				else if (!SetObject(SelectedObject, EPropertyValueSetFlags::DefaultFlags))
				{
					// Warn that some object assignments failed.
					FMessageDialog::Open( EAppMsgType::Ok, FText::Format(
						NSLOCTEXT("UnrealEd", "ObjectAssignmentsFailed", "Failed to assign {0} to the {1} property, see log for details."),
						FText::FromString(SelectedObject->GetPathName()), PropertyNodePin->GetDisplayName()) );
				}
				else
				{
					Res = FPropertyAccess::Success;
				}
			}
		}
	}

	return Res;
}

bool FPropertyValueImpl::IsPropertyTypeOf( UClass* ClassType ) const
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if( PropertyNodePin.IsValid() )
	{
		if(UProperty* Property = PropertyNodePin->GetProperty())
		{
			return Property->IsA( ClassType );
		}
	}
	return false;
}

template< typename Type>
static Type ClampValueFromMetaData(Type InValue, FPropertyNode& InPropertyNode )
{
	UProperty* Property = InPropertyNode.GetProperty();

	Type RetVal = InValue;
	if( Property )
	{
		//enforce min
		const FString& MinString = Property->GetMetaData(TEXT("ClampMin"));
		if(MinString.Len())
		{
			checkSlow(MinString.IsNumeric());
			Type MinValue;
			TTypeFromString<Type>::FromString(MinValue, *MinString);
			RetVal = FMath::Max<Type>(MinValue, RetVal);
		}
		//Enforce max 
		const FString& MaxString = Property->GetMetaData(TEXT("ClampMax"));
		if(MaxString.Len())
		{
			checkSlow(MaxString.IsNumeric());
			Type MaxValue;
			TTypeFromString<Type>::FromString(MaxValue, *MaxString);
			RetVal = FMath::Min<Type>(MaxValue, RetVal);
		}
	}

	return RetVal;
}

template <typename Type>
static Type ClampIntegerValueFromMetaData(Type InValue, FPropertyNode& InPropertyNode )
{
	Type RetVal = ClampValueFromMetaData<Type>( InValue, InPropertyNode );

	UProperty* Property = InPropertyNode.GetProperty();


	//if there is "Multiple" meta data, the selected number is a multiple
	const FString& MultipleString = Property->GetMetaData(TEXT("Multiple"));
	if (MultipleString.Len())
	{
		check(MultipleString.IsNumeric());
		Type MultipleValue;
		TTypeFromString<Type>::FromString(MultipleValue, *MultipleString);
		if (MultipleValue != 0)
		{
			RetVal -= Type(RetVal) % MultipleValue;
		}
	}

	//enforce array bounds
	const FString& ArrayClampString = Property->GetMetaData(TEXT("ArrayClamp"));
	if (ArrayClampString.Len())
	{
		FObjectPropertyNode* ObjectPropertyNode = InPropertyNode.FindObjectItemParent();
		if (ObjectPropertyNode && ObjectPropertyNode->GetNumObjects() == 1)
		{
			Type LastValidIndex = GetArrayPropertyLastValidIndex(ObjectPropertyNode, ArrayClampString);
			RetVal = FMath::Clamp<Type>(RetVal, 0, LastValidIndex);
		}
		else
		{
			UE_LOG(LogPropertyNode, Warning, TEXT("Array Clamping isn't supported in multi-select (Param Name: %s)"), *Property->GetName());
		}
	}

	return RetVal;
}


int32 FPropertyValueImpl::GetNumChildren() const
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if( PropertyNodePin.IsValid() )
	{
		return PropertyNodePin->GetNumChildNodes();
	}

	return 0;
}

TSharedPtr<FPropertyNode> FPropertyValueImpl::GetPropertyNode() const
{
	return PropertyNode.Pin();
}

TSharedPtr<FPropertyNode> FPropertyValueImpl::GetChildNode( FName ChildName, bool bRecurse ) const
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if( PropertyNodePin.IsValid() )
	{
		return PropertyNodePin->FindChildPropertyNode(ChildName, bRecurse);
	}

	return nullptr;
}


TSharedPtr<FPropertyNode> FPropertyValueImpl::GetChildNode( int32 ChildIndex ) const
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if( PropertyNodePin.IsValid() )
	{
		return PropertyNodePin->GetChildNode( ChildIndex );
	}

	return nullptr;
}

bool FPropertyValueImpl::GetChildNode(const int32 ChildArrayIndex, TSharedPtr<FPropertyNode>& OutChildNode) const
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if (PropertyNodePin.IsValid())
	{
		return PropertyNodePin->GetChildNode(ChildArrayIndex, OutChildNode);
	}

	return false;
}


void FPropertyValueImpl::ResetToDefault()
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if( PropertyNodePin.IsValid() && !PropertyNodePin->IsEditConst() && PropertyNodePin->GetDiffersFromDefault() )
	{
		PropertyNodePin->ResetToDefault( NotifyHook );
	}

}

bool FPropertyValueImpl::DiffersFromDefault() const
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if( PropertyNodePin.IsValid()  )
	{
		return PropertyNodePin->GetDiffersFromDefault();
	}

	return false;
}

bool FPropertyValueImpl::IsEditConst() const
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if( PropertyNodePin.IsValid()  )
	{
		return PropertyNodePin->IsEditConst();
	} 

	return false;
}

FText FPropertyValueImpl::GetResetToDefaultLabel() const
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if( PropertyNodePin.IsValid()  )
	{
		return PropertyNodePin->GetResetToDefaultLabel();
	} 

	return FText::GetEmpty();
}

void FPropertyValueImpl::AddChild()
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if ( PropertyNodePin.IsValid() )
	{
		UProperty* NodeProperty = PropertyNodePin->GetProperty();

		FReadAddressList ReadAddresses;
		PropertyNodePin->GetReadAddress( !!PropertyNodePin->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses, true, false, true );
		if ( ReadAddresses.Num() )
		{
			// determines whether we actually changed any values (if the user clicks the "emtpy" button when the array is already empty,
			// we don't want the objects to be marked dirty)
			bool bNotifiedPreChange = false;

			TArray< TMap<FString, int32> > ArrayIndicesPerObject;
			TArray< TMap<UObject*, bool> > PropagationResultPerObject;

			// List of top level objects sent to the PropertyChangedEvent
			TArray<const UObject*> TopLevelObjects;
			TopLevelObjects.Reserve(ReadAddresses.Num());

			// Begin a property edit transaction.
			FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "AddChild", "Add Child") );
			FObjectPropertyNode* ObjectNode = PropertyNodePin->FindObjectItemParent();
			UArrayProperty* Array = Cast<UArrayProperty>(NodeProperty);
			USetProperty* Set = Cast<USetProperty>(NodeProperty);
			UMapProperty* Map = Cast<UMapProperty>(NodeProperty);

			check(Array || Set || Map);
			
			for ( int32 i = 0 ; i < ReadAddresses.Num() ; ++i )
			{
				void* Addr = ReadAddresses.GetAddress(i);
				if ( Addr )
				{
					if ( !bNotifiedPreChange )
					{
						bNotifiedPreChange = true;

						// send the PreEditChange notification to all selected objects
						PropertyNodePin->NotifyPreChange( NodeProperty, NotifyHook );
					}

					//add on array index so we can tell which entry just changed
					ArrayIndicesPerObject.Add(TMap<FString,int32>());
					FPropertyValueImpl::GenerateArrayIndexMapToObjectNode(ArrayIndicesPerObject[i], PropertyNodePin.Get());

					UObject* Obj = ObjectNode ? ObjectNode->GetUObject(i) : nullptr;
					if (Obj)
					{
						if ((Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) ||
							(Obj->HasAnyFlags(RF_DefaultSubObject) && Obj->GetOuter()->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))) &&
							!FApp::IsGame())
						{
							FString OrgContent;
							NodeProperty->ExportText_Direct(OrgContent, Addr, Addr, nullptr, 0);

							TMap<UObject*, bool> PropagationResult;
							PropertyNodePin->PropagateContainerPropertyChange(Obj, OrgContent, EPropertyArrayChangeType::Add, -1, &PropagationResult);
							PropagationResultPerObject.Add(MoveTemp(PropagationResult));
						}

						TopLevelObjects.Add(Obj);
					}

					int32 Index = INDEX_NONE;

					if (Array)
					{
						FScriptArrayHelper	ArrayHelper(Array, Addr);
						Index = ArrayHelper.AddValue();
						FPropertyNode::AdditionalInitializationUDS(Array->Inner, ArrayHelper.GetRawPtr(Index));
					}
					else if (Set)
					{
						FScriptSetHelper	SetHelper(Set, Addr);
						Index = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
						SetHelper.Rehash();

						FPropertyNode::AdditionalInitializationUDS(Set->ElementProp, SetHelper.GetElementPtr(Index));
					}
					else if (Map)
					{
						FScriptMapHelper	MapHelper(Map, Addr);
						Index = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
						MapHelper.Rehash();

						uint8* PairPtr = MapHelper.GetPairPtr(Index);
						FPropertyNode::AdditionalInitializationUDS(Map->KeyProp, Map->KeyProp->ContainerPtrToValuePtr<uint8>(PairPtr));
						FPropertyNode::AdditionalInitializationUDS(Map->ValueProp, Map->ValueProp->ContainerPtrToValuePtr<uint8>(PairPtr));
					}

					ArrayIndicesPerObject[i].Add(NodeProperty->GetName(), Index);
				}
			}

			FPropertyChangedEvent ChangeEvent(NodeProperty, EPropertyChangeType::ArrayAdd, &TopLevelObjects);
			ChangeEvent.SetArrayIndexPerObject(ArrayIndicesPerObject);
			ChangeEvent.SetInstancesChangedResultPerArchetype(PropagationResultPerObject);

			if ( bNotifiedPreChange )
			{
				// send the PostEditChange notification; it will be propagated to all selected objects
				PropertyNodePin->NotifyPostChange(ChangeEvent, NotifyHook);
			}

			if (PropertyUtilities.IsValid())
			{
				PropertyNodePin->FixPropertiesInEvent(ChangeEvent);
				PropertyUtilities.Pin()->NotifyFinishedChangingProperties(ChangeEvent);
			}
		}
	}
}

void FPropertyValueImpl::ClearChildren()
{
	TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
	if ( PropertyNodePin.IsValid() )
	{
		UProperty* NodeProperty = PropertyNodePin->GetProperty();

		FReadAddressList ReadAddresses;
		PropertyNodePin->GetReadAddress( !!PropertyNodePin->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses,
			true, //bComparePropertyContents
			false, //bObjectForceCompare
			true ); //bArrayPropertiesCanDifferInSize

		if ( ReadAddresses.Num() )
		{
			// determines whether we actually changed any values (if the user clicks the "emtpy" button when the array is already empty,
			// we don't want the objects to be marked dirty)
			bool bNotifiedPreChange = false;

			// List of top level objects sent to the PropertyChangedEvent
			TArray<const UObject*> TopLevelObjects;
			TopLevelObjects.Reserve(ReadAddresses.Num());

			// Begin a property edit transaction.
			FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ClearChildren", "Clear Children") );
			FObjectPropertyNode* ObjectNode = PropertyNodePin->FindObjectItemParent();
			UArrayProperty* ArrayProperty = Cast<UArrayProperty>(NodeProperty);
			USetProperty* SetProperty = Cast<USetProperty>(NodeProperty);
			UMapProperty* MapProperty = Cast<UMapProperty>(NodeProperty);

			check(ArrayProperty || SetProperty || MapProperty);

			for ( int32 i = 0 ; i < ReadAddresses.Num() ; ++i )
			{
				void* Addr = ReadAddresses.GetAddress(i);
				if ( Addr )
				{
					if ( !bNotifiedPreChange )
					{
						bNotifiedPreChange = true;

						// send the PreEditChange notification to all selected objects
						PropertyNodePin->NotifyPreChange( NodeProperty, NotifyHook );
					}

					UObject* Obj = ObjectNode ? ObjectNode->GetUObject(i) : nullptr;
					if (Obj)
					{
						if ((Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) ||
							(Obj->HasAnyFlags(RF_DefaultSubObject) && Obj->GetOuter()->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))) &&
							!FApp::IsGame())
						{
							FString OrgContent;
							NodeProperty->ExportText_Direct(OrgContent, Addr, Addr, nullptr, 0);
							PropertyNodePin->PropagateContainerPropertyChange(Obj, OrgContent, EPropertyArrayChangeType::Clear, -1);
						}

						TopLevelObjects.Add(Obj);
					}

					if (ArrayProperty)
					{
						FScriptArrayHelper ArrayHelper(ArrayProperty, Addr);

						// If the inner property is an instanced component property we must move the old components to the 
						// transient package so resetting owned components on the parent doesn't find them
						UObjectProperty* InnerObjectProperty = Cast<UObjectProperty>(ArrayProperty->Inner);
						if (InnerObjectProperty && InnerObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference) && InnerObjectProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
						{
							const int32 ArraySize = ArrayHelper.Num();
							for (int32 Index = 0; Index < ArraySize; ++Index)
							{
								if (UActorComponent* Component = *reinterpret_cast<UActorComponent**>(ArrayHelper.GetRawPtr(Index)))
								{
									Component->Modify();
									Component->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
								}
							}
						}

						ArrayHelper.EmptyValues();
					}
					else if (SetProperty)
					{
						FScriptSetHelper SetHelper(SetProperty, Addr);

						// If the element property is an instanced component property we must move the old components to the 
						// transient package so resetting owned components on the parent doesn't find them
						UObjectProperty* ElementObjectProperty = Cast<UObjectProperty>(SetProperty->ElementProp);
						if (ElementObjectProperty && ElementObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference) && ElementObjectProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
						{
							int32 ElementsToRemove = SetHelper.Num();
							int32 Index = 0;
							while (ElementsToRemove > 0)
							{
								if (SetHelper.IsValidIndex(Index))
								{
									if (UActorComponent* Component = *reinterpret_cast<UActorComponent**>(SetHelper.GetElementPtr(Index)))
									{
										Component->Modify();
										Component->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
									}
									--ElementsToRemove;
								}
								++Index;
							}
						}

						SetHelper.EmptyElements();
					}
					else if (MapProperty)
					{
						FScriptMapHelper MapHelper(MapProperty, Addr);

						// If the map's value property is an instanced component property we must move the old components to the 
						// transient package so resetting owned components on the parent doesn't find them
						UObjectProperty* ValueObjectProperty = Cast<UObjectProperty>(MapProperty->ValueProp);
						if (ValueObjectProperty && ValueObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference) && ValueObjectProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
						{
							int32 ElementsToRemove = MapHelper.Num();
							int32 Index = 0;
							while (ElementsToRemove > 0)
							{
								if (MapHelper.IsValidIndex(Index))
								{
									if (UActorComponent* Component = *reinterpret_cast<UActorComponent**>(MapHelper.GetValuePtr(Index)))
									{
										Component->Modify();
										Component->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
									}
									--ElementsToRemove;
								}
								++Index;
							}
						}

						MapHelper.EmptyValues();
					}
				}
			}

			FPropertyChangedEvent ChangeEvent(NodeProperty, EPropertyChangeType::ArrayClear, &TopLevelObjects);

			if ( bNotifiedPreChange )
			{
				// send the PostEditChange notification; it will be propagated to all selected objects
				PropertyNodePin->NotifyPostChange( ChangeEvent, NotifyHook );
			}

			if (PropertyUtilities.IsValid())
			{
				PropertyNodePin->FixPropertiesInEvent(ChangeEvent);
				PropertyUtilities.Pin()->NotifyFinishedChangingProperties(ChangeEvent);
			}
		}
	}
}

void FPropertyValueImpl::InsertChild( int32 Index )
{
	if( PropertyNode.IsValid() )
	{
		InsertChild( PropertyNode.Pin()->GetChildNode(Index));
	}
}

void FPropertyValueImpl::InsertChild( TSharedPtr<FPropertyNode> ChildNodeToInsertAfter )
{
	FPropertyNode* ChildNodePtr = ChildNodeToInsertAfter.Get();

	FPropertyNode* ParentNode = ChildNodePtr->GetParentNode();
	FObjectPropertyNode* ObjectNode = ChildNodePtr->FindObjectItemParent();

	UProperty* NodeProperty = ChildNodePtr->GetProperty();
	UArrayProperty* ArrayProperty = CastChecked<UArrayProperty>(NodeProperty->GetOuter()); // Insert is not supported for sets or maps


	FReadAddressList ReadAddresses;
	void* Addr = nullptr;
	ParentNode->GetReadAddress( !!ParentNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses );
	if ( ReadAddresses.Num() )
	{
		Addr = ReadAddresses.GetAddress(0);
	}

	if( Addr )
	{
		// Begin a property edit transaction.
		FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "InsertChild", "Insert Child") );

		ChildNodePtr->NotifyPreChange( ParentNode->GetProperty(), NotifyHook );

		FScriptArrayHelper	ArrayHelper(ArrayProperty,Addr);
		int32 Index = ChildNodePtr->GetArrayIndex();

		TArray< TMap<UObject*, bool> > PropagationResultPerObject;

		// List of top level objects sent to the PropertyChangedEvent
		TArray<const UObject*> TopLevelObjects;

		UObject* Obj = ObjectNode ? ObjectNode->GetUObject(0) : nullptr;
		if (Obj)
		{
			if ((Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) ||
				(Obj->HasAnyFlags(RF_DefaultSubObject) && Obj->GetOuter()->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))) &&
				!FApp::IsGame())
			{
				FString OrgArrayContent;
				ArrayProperty->ExportText_Direct(OrgArrayContent, Addr, Addr, nullptr, 0);

				TMap<UObject*, bool> PropagationResult;
				ChildNodePtr->PropagateContainerPropertyChange(Obj, OrgArrayContent, EPropertyArrayChangeType::Insert, Index, &PropagationResult);
				PropagationResultPerObject.Add(MoveTemp(PropagationResult));
			}

			TopLevelObjects.Add(Obj);
		}

		ArrayHelper.InsertValues(Index, 1 );

		FPropertyNode::AdditionalInitializationUDS(ArrayProperty->Inner, ArrayHelper.GetRawPtr(Index));

		//set up indices for the coming events
		TArray< TMap<FString,int32> > ArrayIndicesPerObject;
		for (int32 ObjectIndex = 0; ObjectIndex < ReadAddresses.Num(); ++ObjectIndex)
		{
			//add on array index so we can tell which entry just changed
			ArrayIndicesPerObject.Add(TMap<FString,int32>());
			FPropertyValueImpl::GenerateArrayIndexMapToObjectNode(ArrayIndicesPerObject[ObjectIndex], ChildNodePtr );
		}

		FPropertyChangedEvent ChangeEvent(ParentNode->GetProperty(), EPropertyChangeType::ArrayAdd, &TopLevelObjects);
		ChangeEvent.SetArrayIndexPerObject(ArrayIndicesPerObject);
		ChangeEvent.SetInstancesChangedResultPerArchetype(PropagationResultPerObject);

		ChildNodePtr->NotifyPostChange(ChangeEvent, NotifyHook);

		if (PropertyUtilities.IsValid())
		{
			ChildNodePtr->FixPropertiesInEvent(ChangeEvent);
			PropertyUtilities.Pin()->NotifyFinishedChangingProperties(ChangeEvent);
		}
	}
}


void FPropertyValueImpl::DeleteChild( int32 Index )
{
	TSharedPtr<FPropertyNode> ArrayParentPin = PropertyNode.Pin();
	if( ArrayParentPin.IsValid()  )
	{
		DeleteChild( ArrayParentPin->GetChildNode( Index ) );
	}
}

void FPropertyValueImpl::DeleteChild( TSharedPtr<FPropertyNode> ChildNodeToDelete )
{
	FPropertyNode* ChildNodePtr = ChildNodeToDelete.Get();

	FPropertyNode* ParentNode = ChildNodePtr->GetParentNode();
	FObjectPropertyNode* ObjectNode = ChildNodePtr->FindObjectItemParent();

	UProperty* NodeProperty = ChildNodePtr->GetProperty();
	UArrayProperty* ArrayProperty = Cast<UArrayProperty>(NodeProperty->GetOuter());
	USetProperty* SetProperty = Cast<USetProperty>(NodeProperty->GetOuter());
	UMapProperty* MapProperty = Cast<UMapProperty>(NodeProperty->GetOuter());

	TArray< TMap<FString, int32> > ArrayIndicesPerObject;
	TArray< TMap<UObject*, bool> > PropagationResultPerObject;

	check(ArrayProperty || SetProperty || MapProperty);

	FReadAddressList ReadAddresses;
	ParentNode->GetReadAddress( !!ParentNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses ); 
	if ( ReadAddresses.Num() )
	{
		FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "DeleteChild", "Delete Child") );

		ChildNodePtr->NotifyPreChange( NodeProperty, NotifyHook );

		// List of top level objects sent to the PropertyChangedEvent
		TArray<const UObject*> TopLevelObjects;
		TopLevelObjects.Reserve(ReadAddresses.Num());

		// perform the operation on the array for all selected objects
		for ( int32 i = 0 ; i < ReadAddresses.Num() ; ++i )
		{
			uint8* Address = ReadAddresses.GetAddress(i);

			if( Address ) 
			{
				int32 Index = ChildNodePtr->GetArrayIndex();

				//add on array index so we can tell which entry just changed
				ArrayIndicesPerObject.Add(TMap<FString, int32>());
				FPropertyValueImpl::GenerateArrayIndexMapToObjectNode(ArrayIndicesPerObject[i], ChildNodePtr);

				UObject* Obj = ObjectNode ? ObjectNode->GetUObject(i) : nullptr;
				if (Obj)
				{
					if ((Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) ||
						(Obj->HasAnyFlags(RF_DefaultSubObject) && Obj->GetOuter()->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))) &&
						!FApp::IsGame())
					{
						FString OrgContent;
						Cast<UProperty>(NodeProperty->GetOuter())->ExportText_Direct(OrgContent, Address, Address, nullptr, 0);

						TMap<UObject*, bool> PropagationResult;
						ChildNodePtr->PropagateContainerPropertyChange(Obj, OrgContent, EPropertyArrayChangeType::Delete, Index, &PropagationResult);

						PropagationResultPerObject.Add(MoveTemp(PropagationResult));
					}

					TopLevelObjects.Add(Obj);
				}

				if (ArrayProperty)
				{
					FScriptArrayHelper ArrayHelper(ArrayProperty, Address);

					// If the inner property is an instanced component property we must move the old component to the 
					// transient package so resetting owned components on the parent doesn't find it
					UObjectProperty* InnerObjectProperty = Cast<UObjectProperty>(ArrayProperty->Inner);
					if (InnerObjectProperty && InnerObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference) && InnerObjectProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
					{
						if (UActorComponent* Component = *reinterpret_cast<UActorComponent**>(ArrayHelper.GetRawPtr(ChildNodePtr->GetArrayIndex())))
						{
							Component->Modify();
							Component->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
						}
					}

					ArrayHelper.RemoveValues(ChildNodePtr->GetArrayIndex());
				}
				else if (SetProperty)
				{
					FScriptSetHelper SetHelper(SetProperty, Address);

					// If the element property is an instanced component property we must move the old component to the 
					// transient package so resetting owned components on the parent doesn't find it
					UObjectProperty* ElementObjectProperty = Cast<UObjectProperty>(SetProperty->ElementProp);
					if (ElementObjectProperty && ElementObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference) && ElementObjectProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
					{
						if (UActorComponent* Component = *reinterpret_cast<UActorComponent**>(SetHelper.GetElementPtr(ChildNodePtr->GetArrayIndex())))
						{
							Component->Modify();
							Component->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
						}
					}

					SetHelper.RemoveAt(ChildNodePtr->GetArrayIndex());
					SetHelper.Rehash();
				}
				else if (MapProperty)
				{
					FScriptMapHelper MapHelper(MapProperty, Address);

					// If the map's value property is an instanced component property we must move the old component to the 
					// transient package so resetting owned components on the parent doesn't find it
					UObjectProperty* ValueObjectProperty = Cast<UObjectProperty>(MapProperty->ValueProp);
					if (ValueObjectProperty && ValueObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference) && ValueObjectProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
					{
						if (UActorComponent* Component = *reinterpret_cast<UActorComponent**>(MapHelper.GetValuePtr(ChildNodePtr->GetArrayIndex())))
						{
							Component->Modify();
							Component->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
						}
					}

					MapHelper.RemoveAt(ChildNodePtr->GetArrayIndex());
					MapHelper.Rehash();
				}

				ArrayIndicesPerObject[i].Add(NodeProperty->GetName(), Index);
			}
		}

		FPropertyChangedEvent ChangeEvent(ParentNode->GetProperty(), EPropertyChangeType::ArrayRemove, &TopLevelObjects);
		ChangeEvent.SetArrayIndexPerObject(ArrayIndicesPerObject);
		ChangeEvent.SetInstancesChangedResultPerArchetype(PropagationResultPerObject);

		ChildNodePtr->NotifyPostChange(ChangeEvent, NotifyHook);

		if (PropertyUtilities.IsValid())
		{
			ChildNodePtr->FixPropertiesInEvent(ChangeEvent);
			PropertyUtilities.Pin()->NotifyFinishedChangingProperties(ChangeEvent);
		}
	}
}

void FPropertyValueImpl::SwapChildren(int32 FirstIndex, int32 SecondIndex)
{
	TSharedPtr<FPropertyNode> ArrayParentPin = PropertyNode.Pin();
	if (ArrayParentPin.IsValid())
	{
		SwapChildren(ArrayParentPin->GetChildNode(FirstIndex), ArrayParentPin->GetChildNode(SecondIndex));
	}
}

void FPropertyValueImpl::SwapChildren( TSharedPtr<FPropertyNode> FirstChildNode, TSharedPtr<FPropertyNode> SecondChildNode)
{
	FPropertyNode* FirstChildNodePtr = FirstChildNode.Get();
	FPropertyNode* SecondChildNodePtr = SecondChildNode.Get();

	FPropertyNode* ParentNode = FirstChildNodePtr->GetParentNode();
	FObjectPropertyNode* ObjectNode = FirstChildNodePtr->FindObjectItemParent();

	UProperty* FirstNodeProperty = FirstChildNodePtr->GetProperty();
	UProperty* SecondNodeProperty = SecondChildNodePtr->GetProperty();
	UArrayProperty* ArrayProperty = Cast<UArrayProperty>(FirstNodeProperty->GetOuter());

	check(ArrayProperty);

	FReadAddressList ReadAddresses;
	ParentNode->GetReadAddress( !!ParentNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses ); 
	if ( ReadAddresses.Num() )
	{
		FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SwapChildren", "Swap Children") );

		FirstChildNodePtr->NotifyPreChange( FirstNodeProperty, NotifyHook );
		SecondChildNodePtr->NotifyPreChange( SecondNodeProperty, NotifyHook );
		
		// List of top level objects sent to the PropertyChangedEvent
		TArray<const UObject*> TopLevelObjects;
		TopLevelObjects.Reserve(ReadAddresses.Num());

		// perform the operation on the array for all selected objects
		for ( int32 i = 0 ; i < ReadAddresses.Num() ; ++i )
		{
			uint8* Address = ReadAddresses.GetAddress(i);

			if( Address ) 
			{
				int32 FirstIndex = FirstChildNodePtr->GetArrayIndex();
				int32 SecondIndex = SecondChildNodePtr->GetArrayIndex();

				UObject* Obj = ObjectNode ? ObjectNode->GetUObject(i) : nullptr;
				if (Obj)
				{
					if ((Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || (Obj->HasAnyFlags(RF_DefaultSubObject) && Obj->GetOuter()->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))) 
						&& !FApp::IsGame())
					{
						FString OrgContent;
						Cast<UProperty>(FirstNodeProperty->GetOuter())->ExportText_Direct(OrgContent, Address, Address, nullptr, 0);
						FirstChildNodePtr->PropagateContainerPropertyChange(Obj, OrgContent, EPropertyArrayChangeType::Swap, FirstIndex, nullptr, SecondIndex);
					}

					TopLevelObjects.Add(Obj);
				}

				if (ArrayProperty)
				{
					FScriptArrayHelper ArrayHelper(ArrayProperty, Address);

					// If the inner property is an instanced component property we must move the old component to the 
					// transient package so resetting owned components on the parent doesn't find it
					UObjectProperty* InnerObjectProperty = Cast<UObjectProperty>(ArrayProperty->Inner);
					if (InnerObjectProperty && InnerObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference) && InnerObjectProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
					{
						if (UActorComponent* Component = *reinterpret_cast<UActorComponent**>(ArrayHelper.GetRawPtr(FirstIndex)))
						{
							Component->Modify();
							Component->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
						}

						if (UActorComponent* Component = *reinterpret_cast<UActorComponent**>(ArrayHelper.GetRawPtr(SecondIndex)))
						{
							Component->Modify();
							Component->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
						}
					}

					ArrayHelper.SwapValues(FirstIndex, SecondIndex);
				}
			}
		}

		FPropertyChangedEvent ChangeEvent(ParentNode->GetProperty(), EPropertyChangeType::Unspecified, &TopLevelObjects);
		FirstChildNodePtr->NotifyPostChange(ChangeEvent, NotifyHook);
		SecondChildNodePtr->NotifyPostChange(ChangeEvent, NotifyHook);

		if (PropertyUtilities.IsValid())
		{
			FirstChildNodePtr->FixPropertiesInEvent(ChangeEvent);
			SecondChildNodePtr->FixPropertiesInEvent(ChangeEvent);
			PropertyUtilities.Pin()->NotifyFinishedChangingProperties(ChangeEvent);
		}
	}
}

void FPropertyValueImpl::MoveElementTo(int32 OriginalIndex, int32 NewIndex)
{
	FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "MoveRow", "Move Row"));

	GetPropertyNode()->NotifyPreChange(GetPropertyNode()->GetProperty(), NotifyHook);
	// Insert into the middle or add to the end
	if (NewIndex < GetPropertyNode()->GetNumChildNodes())
	{
		TSharedPtr<FPropertyNode> InsertAfterChild = PropertyNode.Pin()->GetChildNode(NewIndex);
		FPropertyNode* ChildNodePtr = InsertAfterChild.Get();

		FPropertyNode* ParentNode = ChildNodePtr->GetParentNode();
		FObjectPropertyNode* ObjectNode = ChildNodePtr->FindObjectItemParent();

		UProperty* NodeProperty = ChildNodePtr->GetProperty();
		UArrayProperty* ArrayProperty = CastChecked<UArrayProperty>(NodeProperty->GetOuter()); // Insert is not supported for sets or maps


		FReadAddressList ReadAddresses;
		void* Addr = nullptr;
		ParentNode->GetReadAddress(!!ParentNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses);
		if (ReadAddresses.Num())
		{
			Addr = ReadAddresses.GetAddress(0);
		}

		if (Addr)
		{
			FScriptArrayHelper	ArrayHelper(ArrayProperty, Addr);
			int32 Index = ChildNodePtr->GetArrayIndex();

			// List of top level objects sent to the PropertyChangedEvent
			TArray<const UObject*> TopLevelObjects;

			UObject* Obj = ObjectNode ? ObjectNode->GetUObject(0) : nullptr;
			if (Obj)
			{
				if ((Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) ||
					(Obj->HasAnyFlags(RF_DefaultSubObject) && Obj->GetOuter()->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))))
				{
					FString OrgArrayContent;
					ArrayProperty->ExportText_Direct(OrgArrayContent, Addr, Addr, nullptr, 0);

					ChildNodePtr->PropagateContainerPropertyChange(Obj, OrgArrayContent, EPropertyArrayChangeType::Insert, Index);
				}

				TopLevelObjects.Add(Obj);
			}

			ArrayHelper.InsertValues(Index, 1);

			FPropertyNode::AdditionalInitializationUDS(ArrayProperty->Inner, ArrayHelper.GetRawPtr(Index));

			//set up indices for the coming events
			TArray< TMap<FString, int32> > ArrayIndicesPerObject;
			for (int32 ObjectIndex = 0; ObjectIndex < ReadAddresses.Num(); ++ObjectIndex)
			{
				//add on array index so we can tell which entry just changed
				ArrayIndicesPerObject.Add(TMap<FString, int32>());
				FPropertyValueImpl::GenerateArrayIndexMapToObjectNode(ArrayIndicesPerObject[ObjectIndex], ChildNodePtr);
			}

			FPropertyChangedEvent ChangeEvent(ParentNode->GetProperty(), EPropertyChangeType::ArrayAdd, &TopLevelObjects);
			ChangeEvent.SetArrayIndexPerObject(ArrayIndicesPerObject);

			if (PropertyUtilities.IsValid())
			{
				ChildNodePtr->FixPropertiesInEvent(ChangeEvent);
			}
		}
	}
	else
	{
		TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
		if (PropertyNodePin.IsValid())
		{
			UProperty* NodeProperty = PropertyNodePin->GetProperty();

			FReadAddressList ReadAddresses;
			PropertyNodePin->GetReadAddress(!!PropertyNodePin->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses, true, false, true);
			if (ReadAddresses.Num())
			{
				// determines whether we actually changed any values (if the user clicks the "emtpy" button when the array is already empty,
				// we don't want the objects to be marked dirty)
				bool bNotifiedPreChange = false;

				TArray< TMap<FString, int32> > ArrayIndicesPerObject;

				// List of top level objects sent to the PropertyChangedEvent
				TArray<const UObject*> TopLevelObjects;
				TopLevelObjects.Reserve(ReadAddresses.Num());

				FObjectPropertyNode* ObjectNode = PropertyNodePin->FindObjectItemParent();
				UArrayProperty* Array = Cast<UArrayProperty>(NodeProperty);

				check(Array);

				for (int32 i = 0; i < ReadAddresses.Num(); ++i)
				{
					void* Addr = ReadAddresses.GetAddress(i);
					if (Addr)
					{
						//add on array index so we can tell which entry just changed
						ArrayIndicesPerObject.Add(TMap<FString, int32>());
						FPropertyValueImpl::GenerateArrayIndexMapToObjectNode(ArrayIndicesPerObject[i], PropertyNodePin.Get());

						UObject* Obj = ObjectNode ? ObjectNode->GetUObject(i) : nullptr;
						if (Obj)
						{
							if ((Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) ||
								(Obj->HasAnyFlags(RF_DefaultSubObject) && Obj->GetOuter()->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))))
							{
								FString OrgContent;
								NodeProperty->ExportText_Direct(OrgContent, Addr, Addr, nullptr, 0);
								PropertyNodePin->PropagateContainerPropertyChange(Obj, OrgContent, EPropertyArrayChangeType::Add, -1);
							}

							TopLevelObjects.Add(Obj);
						}

						int32 Index = INDEX_NONE;

						FScriptArrayHelper	ArrayHelper(Array, Addr);
						Index = ArrayHelper.AddValue();
						FPropertyNode::AdditionalInitializationUDS(Array->Inner, ArrayHelper.GetRawPtr(Index));
						

						ArrayIndicesPerObject[i].Add(NodeProperty->GetName(), Index);
					}
				}

				FPropertyChangedEvent ChangeEvent(NodeProperty, EPropertyChangeType::ArrayAdd, &TopLevelObjects);
				ChangeEvent.SetArrayIndexPerObject(ArrayIndicesPerObject);

				if (PropertyUtilities.IsValid())
				{
					PropertyNodePin->FixPropertiesInEvent(ChangeEvent);
				}
			}
		}
	}
	// We inserted an element above our original index
	if (NewIndex < OriginalIndex)
	{
		OriginalIndex += 1;
	}


	// Both Insert and Add are deferred so you need to rebuild the parent node's children
	GetPropertyNode()->RebuildChildren();

	//Swap
	{
		FPropertyNode* FirstChildNodePtr = GetPropertyNode()->GetChildNode(OriginalIndex).Get();
		FPropertyNode* SecondChildNodePtr = GetPropertyNode()->GetChildNode(NewIndex).Get();

		FPropertyNode* ParentNode = FirstChildNodePtr->GetParentNode();
		FObjectPropertyNode* ObjectNode = FirstChildNodePtr->FindObjectItemParent();

		UProperty* FirstNodeProperty = FirstChildNodePtr->GetProperty();
		UProperty* SecondNodeProperty = SecondChildNodePtr->GetProperty();
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(FirstNodeProperty->GetOuter());

		check(ArrayProperty);

		FReadAddressList ReadAddresses;
		ParentNode->GetReadAddress(!!ParentNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses);
		if (ReadAddresses.Num())
		{
			// List of top level objects sent to the PropertyChangedEvent
			TArray<const UObject*> TopLevelObjects;
			TopLevelObjects.Reserve(ReadAddresses.Num());

			// perform the operation on the array for all selected objects
			for (int32 i = 0; i < ReadAddresses.Num(); ++i)
			{
				uint8* Address = ReadAddresses.GetAddress(i);

				if (Address)
				{
					int32 FirstIndex = FirstChildNodePtr->GetArrayIndex();
					int32 SecondIndex = SecondChildNodePtr->GetArrayIndex();

					UObject* Obj = ObjectNode ? ObjectNode->GetUObject(i) : nullptr;
					if (Obj)
					{
						if ((Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) ||
							(Obj->HasAnyFlags(RF_DefaultSubObject) && Obj->GetOuter()->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))))
						{
							FString OrgContent;
							Cast<UProperty>(FirstNodeProperty->GetOuter())->ExportText_Direct(OrgContent, Address, Address, nullptr, 0);
							FirstChildNodePtr->PropagateContainerPropertyChange(Obj, OrgContent, EPropertyArrayChangeType::Swap, FirstIndex, nullptr, SecondIndex);
						}

						TopLevelObjects.Add(Obj);
					}

					if (ArrayProperty)
					{
						FScriptArrayHelper ArrayHelper(ArrayProperty, Address);

						// If the inner property is an instanced component property we must move the old component to the 
						// transient package so resetting owned components on the parent doesn't find it
						UObjectProperty* InnerObjectProperty = Cast<UObjectProperty>(ArrayProperty->Inner);
						if (InnerObjectProperty && InnerObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference) && InnerObjectProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
						{
							if (UActorComponent* Component = *reinterpret_cast<UActorComponent**>(ArrayHelper.GetRawPtr(FirstIndex)))
							{
								Component->Modify();
								Component->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
							}

							if (UActorComponent* Component = *reinterpret_cast<UActorComponent**>(ArrayHelper.GetRawPtr(SecondIndex)))
							{
								Component->Modify();
								Component->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
							}
						}

						ArrayHelper.SwapValues(FirstIndex, SecondIndex);
					}
				}
			}

			FPropertyChangedEvent ChangeEvent(ParentNode->GetProperty(), EPropertyChangeType::Unspecified, &TopLevelObjects);

			if (PropertyUtilities.IsValid())
			{
				FirstChildNodePtr->FixPropertiesInEvent(ChangeEvent);
				SecondChildNodePtr->FixPropertiesInEvent(ChangeEvent);
			}
		}
	}

	//Delete
	{
		FPropertyNode* ChildNodePtr = GetPropertyNode()->GetChildNode(OriginalIndex).Get();

		FPropertyNode* ParentNode = ChildNodePtr->GetParentNode();
		FObjectPropertyNode* ObjectNode = ChildNodePtr->FindObjectItemParent();

		UProperty* NodeProperty = ChildNodePtr->GetProperty();
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(NodeProperty->GetOuter());

		FReadAddressList ReadAddresses;
		ParentNode->GetReadAddress(!!ParentNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses);
		if (ReadAddresses.Num())
		{
			// List of top level objects sent to the PropertyChangedEvent
			TArray<const UObject*> TopLevelObjects;
			TopLevelObjects.Reserve(ReadAddresses.Num());

			// perform the operation on the array for all selected objects
			for (int32 i = 0; i < ReadAddresses.Num(); ++i)
			{
				uint8* Address = ReadAddresses.GetAddress(i);

				if (Address)
				{
					int32 Index = ChildNodePtr->GetArrayIndex();

					UObject* Obj = ObjectNode ? ObjectNode->GetUObject(i) : nullptr;
					if (Obj)
					{
						if ((Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) ||
							(Obj->HasAnyFlags(RF_DefaultSubObject) && Obj->GetOuter()->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))) &&
							!FApp::IsGame())
						{
							FString OrgContent;
							Cast<UProperty>(NodeProperty->GetOuter())->ExportText_Direct(OrgContent, Address, Address, nullptr, 0);
							ChildNodePtr->PropagateContainerPropertyChange(Obj, OrgContent, EPropertyArrayChangeType::Delete, Index);
						}

						TopLevelObjects.Add(Obj);
					}

					FScriptArrayHelper ArrayHelper(ArrayProperty, Address);

					// If the inner property is an instanced component property we must move the old component to the 
					// transient package so resetting owned components on the parent doesn't find it
					UObjectProperty* InnerObjectProperty = Cast<UObjectProperty>(ArrayProperty->Inner);
					if (InnerObjectProperty && InnerObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference) && InnerObjectProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
					{
						if (UActorComponent* Component = *reinterpret_cast<UActorComponent**>(ArrayHelper.GetRawPtr(ChildNodePtr->GetArrayIndex())))
						{
							Component->Modify();
							Component->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
						}
					}

					ArrayHelper.RemoveValues(ChildNodePtr->GetArrayIndex());
					
				}
			}

			FPropertyChangedEvent ChangeEvent(ParentNode->GetProperty(), EPropertyChangeType::Unspecified, &TopLevelObjects);
			if (PropertyUtilities.IsValid())
			{
				ChildNodePtr->FixPropertiesInEvent(ChangeEvent);
			}
		}
		FPropertyChangedEvent MoveEvent(ParentNode->GetProperty(), EPropertyChangeType::Unspecified);
		GetPropertyNode()->NotifyPostChange(MoveEvent, NotifyHook);
		if (PropertyUtilities.IsValid())
		{
			PropertyUtilities.Pin()->NotifyFinishedChangingProperties(MoveEvent);
		}
	}
}

void FPropertyValueImpl::DuplicateChild( int32 Index )
{
	TSharedPtr<FPropertyNode> ArrayParentPin = PropertyNode.Pin();
	if( ArrayParentPin.IsValid() )
	{
		DuplicateChild( ArrayParentPin->GetChildNode( Index ) );
	}
}

void FPropertyValueImpl::DuplicateChild( TSharedPtr<FPropertyNode> ChildNodeToDuplicate )
{
	FPropertyNode* ChildNodePtr = ChildNodeToDuplicate.Get();

	FPropertyNode* ParentNode = ChildNodePtr->GetParentNode();
	FObjectPropertyNode* ObjectNode = ChildNodePtr->FindObjectItemParent();

	UProperty* NodeProperty = ChildNodePtr->GetProperty();
	UArrayProperty* ArrayProperty = CastChecked<UArrayProperty>(NodeProperty->GetOuter()); // duplication is only supported for arrays

	FReadAddressList ReadAddresses;
	void* Addr = nullptr;
	ParentNode->GetReadAddress( !!ParentNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses );
	if ( ReadAddresses.Num() )
	{
		Addr = ReadAddresses.GetAddress(0);
	}

	if( Addr )
	{
		// List of top level objects sent to the PropertyChangedEvent
		TArray<const UObject*> TopLevelObjects;

		FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "DuplicateChild", "Duplicate Child") );

		ChildNodePtr->NotifyPreChange( ParentNode->GetProperty(), NotifyHook );

		int32 Index = ChildNodePtr->GetArrayIndex();
		UObject* Obj = ObjectNode ? ObjectNode->GetUObject(0) : nullptr;

		TArray< TMap<FString, int32> > ArrayIndicesPerObject;
		TArray< TMap<UObject*, bool> > PropagationResultPerObject;

		if (Obj)
		{
			if ((Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) ||
				(Obj->HasAnyFlags(RF_DefaultSubObject) && Obj->GetOuter()->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))) &&
				!FApp::IsGame())
			{
				FString OrgContent;
				Cast<UProperty>(NodeProperty->GetOuter())->ExportText_Direct(OrgContent, Addr, Addr, nullptr, 0);

				TMap<UObject*, bool> PropagationResult;
				ChildNodePtr->PropagateContainerPropertyChange(Obj, OrgContent, EPropertyArrayChangeType::Duplicate, Index, &PropagationResult);
				PropagationResultPerObject.Add(MoveTemp(PropagationResult));
			}

			TopLevelObjects.Add(Obj);
		}

		FScriptArrayHelper	ArrayHelper(ArrayProperty, Addr);
		ArrayHelper.InsertValues(Index);

		void* SrcAddress = ArrayHelper.GetRawPtr(Index + 1);
		void* DestAddress = ArrayHelper.GetRawPtr(Index);

		check(SrcAddress && DestAddress);

		// Copy the selected item's value to the new item.
		NodeProperty->CopyCompleteValue(DestAddress, SrcAddress);

		if (UObjectProperty* ObjProp = Cast<UObjectProperty>(NodeProperty))
		{
			UObject* CurrentObject = ObjProp->GetObjectPropertyValue(DestAddress);

			// For DefaultSubObjects and ArchetypeObjects we need to do a deep copy instead of a shallow copy
			if (CurrentObject && CurrentObject->HasAnyFlags(RF_DefaultSubObject | RF_ArchetypeObject))
			{
				// Make a deep copy and assign it into the array.
				UObject* DuplicatedObject = DuplicateObject(CurrentObject, CurrentObject->GetOuter());
				ObjProp->SetObjectPropertyValue(SrcAddress, DuplicatedObject);
			}
		}
		
		// Find the object that owns the array and instance any subobjects
		if (FObjectPropertyNode* ObjectPropertyNode = ChildNodePtr->FindObjectItemParent())
		{
			UObject* ArrayOwner = nullptr;
			for (TPropObjectIterator Itor(ObjectPropertyNode->ObjectIterator()); Itor && !ArrayOwner; ++Itor)
			{
				ArrayOwner = Itor->Get();
			}
			if (ArrayOwner)
			{
				ArrayOwner->InstanceSubobjectTemplates();
			}
		}

		if(Obj)
		{

			ArrayIndicesPerObject.Add(TMap<FString, int32>());
			FPropertyValueImpl::GenerateArrayIndexMapToObjectNode(ArrayIndicesPerObject[0], ChildNodePtr);
		}

		FPropertyChangedEvent ChangeEvent(ParentNode->GetProperty(), EPropertyChangeType::Duplicate, &TopLevelObjects);
		ChangeEvent.SetArrayIndexPerObject(ArrayIndicesPerObject);
		ChangeEvent.SetInstancesChangedResultPerArchetype(PropagationResultPerObject);

		ChildNodePtr->NotifyPostChange(ChangeEvent, NotifyHook);

		if (PropertyUtilities.IsValid())
		{
			ChildNodePtr->FixPropertiesInEvent(ChangeEvent);
			PropertyUtilities.Pin()->NotifyFinishedChangingProperties(ChangeEvent);
		}
	}
}

bool FPropertyValueImpl::HasValidPropertyNode() const
{
	return PropertyNode.IsValid();
}

FText FPropertyValueImpl::GetDisplayName() const
{
	return PropertyNode.IsValid() ? PropertyNode.Pin()->GetDisplayName() : FText::GetEmpty();
}

void FPropertyValueImpl::ShowInvalidOperationError(const FText& ErrorText)
{
	if (!InvalidOperationError.IsValid())
	{
		FNotificationInfo InvalidOperation(ErrorText);
		InvalidOperation.ExpireDuration = 3.0f;
		InvalidOperationError = FSlateNotificationManager::Get().AddNotification(InvalidOperation);
	}
}

#define IMPLEMENT_PROPERTY_ACCESSOR( ValueType ) \
	FPropertyAccess::Result FPropertyHandleBase::SetValue( ValueType const& InValue, EPropertyValueSetFlags::Type Flags ) \
	{ \
		return FPropertyAccess::Fail; \
	} \
	FPropertyAccess::Result FPropertyHandleBase::GetValue( ValueType& OutValue ) const \
	{ \
		return FPropertyAccess::Fail; \
	}

IMPLEMENT_PROPERTY_ACCESSOR( bool )
IMPLEMENT_PROPERTY_ACCESSOR( int8 )
IMPLEMENT_PROPERTY_ACCESSOR( int16 )
IMPLEMENT_PROPERTY_ACCESSOR( int32 )
IMPLEMENT_PROPERTY_ACCESSOR( int64 )
IMPLEMENT_PROPERTY_ACCESSOR( uint8 )
IMPLEMENT_PROPERTY_ACCESSOR( uint16 )
IMPLEMENT_PROPERTY_ACCESSOR( uint32 )
IMPLEMENT_PROPERTY_ACCESSOR( uint64 )
IMPLEMENT_PROPERTY_ACCESSOR( float )
IMPLEMENT_PROPERTY_ACCESSOR( double )
IMPLEMENT_PROPERTY_ACCESSOR( FString )
IMPLEMENT_PROPERTY_ACCESSOR( FText )
IMPLEMENT_PROPERTY_ACCESSOR( FName )
IMPLEMENT_PROPERTY_ACCESSOR( FVector )
IMPLEMENT_PROPERTY_ACCESSOR( FVector2D )
IMPLEMENT_PROPERTY_ACCESSOR( FVector4 )
IMPLEMENT_PROPERTY_ACCESSOR( FQuat )
IMPLEMENT_PROPERTY_ACCESSOR( FRotator )
IMPLEMENT_PROPERTY_ACCESSOR( UObject* )
IMPLEMENT_PROPERTY_ACCESSOR( const UObject* )
IMPLEMENT_PROPERTY_ACCESSOR( FAssetData )

FPropertyHandleBase::FPropertyHandleBase( TSharedPtr<FPropertyNode> PropertyNode, FNotifyHook* NotifyHook, TSharedPtr<IPropertyUtilities> PropertyUtilities )
	: Implementation( MakeShareable( new FPropertyValueImpl( PropertyNode, NotifyHook, PropertyUtilities ) ) )
{

}
 
bool FPropertyHandleBase::IsValidHandle() const
{
	return Implementation->HasValidPropertyNode();
}

FText FPropertyHandleBase::GetPropertyDisplayName() const
{
	return Implementation->GetDisplayName();
}
	
void FPropertyHandleBase::ResetToDefault()
{
	Implementation->ResetToDefault();
}

bool FPropertyHandleBase::DiffersFromDefault() const
{
	return Implementation->DiffersFromDefault();
}

FText FPropertyHandleBase::GetResetToDefaultLabel() const
{
	return Implementation->GetResetToDefaultLabel();
}

void FPropertyHandleBase::MarkHiddenByCustomization()
{
	if( Implementation->GetPropertyNode().IsValid() )
	{
		Implementation->GetPropertyNode()->SetNodeFlags( EPropertyNodeFlags::IsCustomized, true );
	}
}

void FPropertyHandleBase::MarkResetToDefaultCustomized()
{
	if (Implementation->GetPropertyNode().IsValid())
	{
		Implementation->GetPropertyNode()->SetNodeFlags(EPropertyNodeFlags::HasCustomResetToDefault, true);
	}
}

void FPropertyHandleBase::ClearResetToDefaultCustomized()
{
	if (Implementation->GetPropertyNode().IsValid())
	{
		Implementation->GetPropertyNode()->SetNodeFlags(EPropertyNodeFlags::HasCustomResetToDefault, false);
	}
}

bool FPropertyHandleBase::IsCustomized() const
{
	return Implementation->GetPropertyNode()->HasNodeFlags( EPropertyNodeFlags::IsCustomized ) != 0;
}

bool FPropertyHandleBase::IsResetToDefaultCustomized() const
{
	return Implementation->GetPropertyNode()->HasNodeFlags(EPropertyNodeFlags::HasCustomResetToDefault) != 0;
}

FString FPropertyHandleBase::GeneratePathToProperty() const
{
	FString OutPath;
	if( Implementation.IsValid() && Implementation->GetPropertyNode().IsValid() )
	{
		const bool bArrayIndex = true;
		const bool bIgnoreCategories = true;
		FPropertyNode* StopParent = Implementation->GetPropertyNode()->FindObjectItemParent();
		Implementation->GetPropertyNode()->GetQualifiedName( OutPath, bArrayIndex, StopParent, bIgnoreCategories );
	}

	return OutPath;

}

TSharedRef<SWidget> FPropertyHandleBase::CreatePropertyNameWidget( const FText& NameOverride, const FText& ToolTipOverride, bool bDisplayResetToDefault, bool bDisplayText, bool bDisplayThumbnail ) const
{
	if( Implementation.IsValid() && Implementation->GetPropertyNode().IsValid() )
	{
		struct FPropertyNodeDisplayNameOverrideHelper
		{
			FPropertyNodeDisplayNameOverrideHelper(TSharedPtr<FPropertyValueImpl> InImplementation, const FText& InNameOverride, const FText& InToolTipOverride)
				:Implementation(InImplementation)
				,bResetDisplayName(false)
				,bResetToolTipText(false)
			{
				if (!InNameOverride.IsEmpty())
				{
					bResetDisplayName = true;
					Implementation->GetPropertyNode()->SetDisplayNameOverride(InNameOverride);
				}
				
				if (!InToolTipOverride.IsEmpty())
				{
					bResetToolTipText = true;
					Implementation->GetPropertyNode()->SetToolTipOverride(InToolTipOverride);
				}
			}

			~FPropertyNodeDisplayNameOverrideHelper()
			{
				if (bResetDisplayName)
				{
					Implementation->GetPropertyNode()->SetDisplayNameOverride(FText::GetEmpty());
				}
				
				if (bResetToolTipText)
				{
					Implementation->GetPropertyNode()->SetToolTipOverride(FText::GetEmpty());
				}
			}

		private:
			TSharedPtr<FPropertyValueImpl> Implementation;
			bool bResetDisplayName;
			bool bResetToolTipText;
		};

		FPropertyNodeDisplayNameOverrideHelper TempPropertyNameOverride(Implementation, NameOverride, ToolTipOverride);

		TSharedPtr<FPropertyEditor> PropertyEditor = FPropertyEditor::Create( Implementation->GetPropertyNode().ToSharedRef(), Implementation->GetPropertyUtilities().ToSharedRef() );

		return SNew( SPropertyNameWidget, PropertyEditor )
				.DisplayResetToDefault( bDisplayResetToDefault );
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> FPropertyHandleBase::CreatePropertyValueWidget( bool bDisplayDefaultPropertyButtons ) const
{
	if( Implementation.IsValid() && Implementation->GetPropertyNode().IsValid() )
	{
		TSharedPtr<FPropertyEditor> PropertyEditor = FPropertyEditor::Create( Implementation->GetPropertyNode().ToSharedRef(), Implementation->GetPropertyUtilities().ToSharedRef() );

		return SNew( SPropertyValueWidget, PropertyEditor, Implementation->GetPropertyUtilities() )
				.ShowPropertyButtons( bDisplayDefaultPropertyButtons );
	}

	return SNullWidget::NullWidget;
}

bool FPropertyHandleBase::IsEditConst() const
{
	return Implementation->IsEditConst();
}

FPropertyAccess::Result FPropertyHandleBase::GetValueAsFormattedString( FString& OutValue, EPropertyPortFlags PortFlags ) const
{
	return Implementation->GetValueAsString(OutValue, PortFlags);
}

FPropertyAccess::Result FPropertyHandleBase::GetValueAsDisplayString( FString& OutValue, EPropertyPortFlags PortFlags ) const
{
	return Implementation->GetValueAsDisplayString(OutValue, PortFlags);
}

FPropertyAccess::Result FPropertyHandleBase::GetValueAsFormattedText( FText& OutValue ) const
{
	return Implementation->GetValueAsText(OutValue);
}

FPropertyAccess::Result FPropertyHandleBase::GetValueAsDisplayText( FText& OutValue ) const
{
	return Implementation->GetValueAsDisplayText(OutValue);
}

FPropertyAccess::Result FPropertyHandleBase::SetValueFromFormattedString( const FString& InValue, EPropertyValueSetFlags::Type Flags )
{
	return Implementation->SetValueAsString( InValue, Flags );
}

TSharedPtr<IPropertyHandle> FPropertyHandleBase::GetChildHandle( FName ChildName, bool bRecurse ) const
{
	// Container children cannot be accessed in this manner
	if( ! ( Implementation->IsPropertyTypeOf(UArrayProperty::StaticClass() ) || Implementation->IsPropertyTypeOf(USetProperty::StaticClass()) || Implementation->IsPropertyTypeOf(UMapProperty::StaticClass()) ) )
	{
		TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetChildNode( ChildName, bRecurse );

		if( PropertyNode.IsValid() )
		{
			return PropertyEditorHelpers::GetPropertyHandle( PropertyNode.ToSharedRef(), Implementation->GetNotifyHook(), Implementation->GetPropertyUtilities() );
		}
	}
	
	return nullptr;
}

TSharedPtr<IPropertyHandle> FPropertyHandleBase::GetChildHandle( uint32 ChildIndex ) const
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetChildNode( ChildIndex );

	if( PropertyNode.IsValid() )
	{
		return PropertyEditorHelpers::GetPropertyHandle( PropertyNode.ToSharedRef(), Implementation->GetNotifyHook(), Implementation->GetPropertyUtilities() );
	}
	return nullptr;
}

TSharedPtr<IPropertyHandle> FPropertyHandleBase::GetParentHandle() const
{
	TSharedPtr<FPropertyNode> ParentNode = Implementation->GetPropertyNode()->GetParentNodeSharedPtr();
	if( ParentNode.IsValid() )
	{
		return PropertyEditorHelpers::GetPropertyHandle( ParentNode.ToSharedRef(), Implementation->GetNotifyHook(), Implementation->GetPropertyUtilities() );
	}

	return nullptr;
}

TSharedPtr<IPropertyHandle> FPropertyHandleBase::GetKeyHandle() const
{
	TSharedPtr<FPropertyNode> KeyNode = Implementation->GetPropertyNode()->GetPropertyKeyNode();
	if (KeyNode.IsValid())
	{
		return PropertyEditorHelpers::GetPropertyHandle(KeyNode.ToSharedRef(), Implementation->GetNotifyHook(), Implementation->GetPropertyUtilities());
	}

	return nullptr;
}

FPropertyAccess::Result FPropertyHandleBase::GetNumChildren( uint32& OutNumChildren ) const
{
	OutNumChildren = Implementation->GetNumChildren();
	return FPropertyAccess::Success;
}

uint32 FPropertyHandleBase::GetNumOuterObjects() const
{
	FObjectPropertyNode* ObjectNode = Implementation->GetPropertyNode()->FindObjectItemParent();

	uint32 NumObjects = 0;
	if( ObjectNode )
	{
		NumObjects = ObjectNode->GetNumObjects();
	}

	return NumObjects;
}

void FPropertyHandleBase::GetOuterObjects( TArray<UObject*>& OuterObjects ) const
{
	FObjectPropertyNode* ObjectNode = Implementation->GetPropertyNode()->FindObjectItemParent();
	if( ObjectNode )
	{
		for( int32 ObjectIndex = 0; ObjectIndex < ObjectNode->GetNumObjects(); ++ObjectIndex )
		{
			OuterObjects.Add( ObjectNode->GetUObject( ObjectIndex ) );
		}
	}

}

void FPropertyHandleBase::GetOuterPackages(TArray<UPackage*>& OuterPackages) const
{
	FComplexPropertyNode* ComplexNode = Implementation->GetPropertyNode()->FindComplexParent();
	if (ComplexNode)
	{
		switch (ComplexNode->GetPropertyType())
		{
		case FComplexPropertyNode::EPT_Object:
			{
				FObjectPropertyNode* ObjectNode = static_cast<FObjectPropertyNode*>(ComplexNode);
				for (int32 ObjectIndex = 0; ObjectIndex < ObjectNode->GetNumObjects(); ++ObjectIndex)
				{
					OuterPackages.Add(ObjectNode->GetUPackage(ObjectIndex));
				}
			}
			break;

		case FComplexPropertyNode::EPT_StandaloneStructure:
			{
				FStructurePropertyNode* StructNode = static_cast<FStructurePropertyNode*>(ComplexNode);
				OuterPackages.Add(StructNode->GetOwnerPackage());
			}
			break;

		default:
			break;
		}
	}
}

void FPropertyHandleBase::EnumerateRawData( const EnumerateRawDataFuncRef& InRawDataCallback )
{
	Implementation->EnumerateRawData( InRawDataCallback );
}

void FPropertyHandleBase::EnumerateConstRawData( const EnumerateConstRawDataFuncRef& InRawDataCallback ) const
{
	Implementation->EnumerateConstRawData( InRawDataCallback );
}

void FPropertyHandleBase::AccessRawData( TArray<void*>& RawData )
{
	Implementation->AccessRawData( RawData );
}

void FPropertyHandleBase::AccessRawData( TArray<const void*>& RawData ) const
{
	Implementation->AccessRawData(RawData);
}

void FPropertyHandleBase::SetOnPropertyValueChanged( const FSimpleDelegate& InOnPropertyValueChanged )
{
	Implementation->SetOnPropertyValueChanged(InOnPropertyValueChanged);
}

void FPropertyHandleBase::SetOnChildPropertyValueChanged( const FSimpleDelegate& InOnChildPropertyValueChanged )
{
	Implementation->SetOnChildPropertyValueChanged( InOnChildPropertyValueChanged );
}

void FPropertyHandleBase::SetOnPropertyValuePreChange(const FSimpleDelegate& InOnPropertyValuePreChange)
{
	Implementation->SetOnPropertyValuePreChange(InOnPropertyValuePreChange);
}

void FPropertyHandleBase::SetOnChildPropertyValuePreChange(const FSimpleDelegate& InOnChildPropertyValuePreChange)
{
	Implementation->SetOnChildPropertyValuePreChange(InOnChildPropertyValuePreChange);
}

TSharedPtr<FPropertyNode> FPropertyHandleBase::GetPropertyNode() const
{
	return Implementation->GetPropertyNode();
}

void FPropertyHandleBase::OnCustomResetToDefault(const FResetToDefaultOverride& OnCustomResetToDefault)
{
	if (OnCustomResetToDefault.OnResetToDefaultClicked().IsBound())
	{
		Implementation->GetPropertyNode()->NotifyPreChange(Implementation->GetPropertyNode()->GetProperty(), Implementation->GetPropertyUtilities()->GetNotifyHook());

		OnCustomResetToDefault.OnResetToDefaultClicked().Execute(SharedThis(this));

		// Call PostEditchange on all the objects
		FPropertyChangedEvent ChangeEvent(Implementation->GetPropertyNode()->GetProperty());
		Implementation->GetPropertyNode()->NotifyPostChange(ChangeEvent, Implementation->GetPropertyUtilities()->GetNotifyHook());
	}
}

int32 FPropertyHandleBase::GetIndexInArray() const
{
	if( Implementation->GetPropertyNode().IsValid() )
	{
		return Implementation->GetPropertyNode()->GetArrayIndex();
	}

	return INDEX_NONE;
}

const UClass* FPropertyHandleBase::GetPropertyClass() const
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() && PropertyNode->GetProperty() )
	{
		return PropertyNode->GetProperty()->GetClass();
	}

	return nullptr;
}

UProperty* FPropertyHandleBase::GetProperty() const
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		return PropertyNode->GetProperty();
	}

	return nullptr;
}

UProperty* FPropertyHandleBase::GetMetaDataProperty() const
{
	UProperty* MetaDataProperty = nullptr;

	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		MetaDataProperty = PropertyNode->GetProperty();
		
		// If we are part of an array, we need to take our meta-data from the array property
		if( PropertyNode->GetArrayIndex() != INDEX_NONE )
		{
			TSharedPtr<FPropertyNode> ParentNode = PropertyNode->GetParentNodeSharedPtr();
			check(ParentNode.IsValid());
			MetaDataProperty = ParentNode->GetProperty();
		}
	}

	return MetaDataProperty;
}

bool FPropertyHandleBase::HasMetaData(const FName& Key) const
{
	UProperty* const MetaDataProperty = GetMetaDataProperty();
	return (MetaDataProperty) ? MetaDataProperty->HasMetaData(Key) : false;
}

const FString& FPropertyHandleBase::GetMetaData(const FName& Key) const
{
	// if not found, return a static empty string
	static const FString EmptyString = TEXT("");

	UProperty* const MetaDataProperty = GetMetaDataProperty();
	return (MetaDataProperty) ? MetaDataProperty->GetMetaData(Key) : EmptyString;
}

bool FPropertyHandleBase::GetBoolMetaData(const FName& Key) const
{
	UProperty* const MetaDataProperty = GetMetaDataProperty();
	return (MetaDataProperty) ? MetaDataProperty->GetBoolMetaData(Key) : false;
}

int32 FPropertyHandleBase::GetINTMetaData(const FName& Key) const
{
	UProperty* const MetaDataProperty = GetMetaDataProperty();
	return (MetaDataProperty) ? MetaDataProperty->GetINTMetaData(Key) : 0;
}

float FPropertyHandleBase::GetFLOATMetaData(const FName& Key) const
{
	UProperty* const MetaDataProperty = GetMetaDataProperty();
	return (MetaDataProperty) ? MetaDataProperty->GetFLOATMetaData(Key) : 0.0f;
}

UClass* FPropertyHandleBase::GetClassMetaData(const FName& Key) const
{
	UProperty* const MetaDataProperty = GetMetaDataProperty();
	return (MetaDataProperty) ? MetaDataProperty->GetClassMetaData(Key) : nullptr;
}


void FPropertyHandleBase::SetInstanceMetaData(const FName& Key, const FString& Value)
{
	GetPropertyNode()->SetInstanceMetaData(Key, Value);
}

const FString* FPropertyHandleBase::GetInstanceMetaData(const FName& Key) const
{
	return GetPropertyNode()->GetInstanceMetaData(Key);
}

FText FPropertyHandleBase::GetToolTipText() const
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		return PropertyNode->GetToolTipText();
	}

	return FText::GetEmpty();
}

void FPropertyHandleBase::SetToolTipText( const FText& ToolTip )
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if (PropertyNode.IsValid())
	{
		PropertyNode->SetToolTipOverride( ToolTip );
	}
}

uint8* FPropertyHandleBase::GetValueBaseAddress( uint8* Base )
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if (PropertyNode.IsValid())
	{
		return PropertyNode->GetValueBaseAddress(Base);
	}

	return nullptr;
}

int32 FPropertyHandleBase::GetNumPerObjectValues() const
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if (PropertyNode.IsValid() && PropertyNode->GetProperty())
	{
		FComplexPropertyNode* ComplexNode = PropertyNode->FindComplexParent();
		if (ComplexNode)
		{
			return ComplexNode->GetInstancesNum();
		}
	}
	return 0;
}

FPropertyAccess::Result FPropertyHandleBase::SetPerObjectValues( const TArray<FString>& InPerObjectValues, EPropertyValueSetFlags::Type Flags )
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() && PropertyNode->GetProperty() )
	{
		FComplexPropertyNode* ComplexNode = PropertyNode->FindComplexParent();

		if (ComplexNode && InPerObjectValues.Num() == ComplexNode->GetInstancesNum())
		{
			TArray<FObjectBaseAddress> ObjectsToModify;
			Implementation->GetObjectsToModify( ObjectsToModify, PropertyNode.Get() );

			if(ObjectsToModify.Num() > 0)
			{
				Implementation->ImportText( ObjectsToModify, InPerObjectValues, PropertyNode.Get(), Flags );
			}

			return FPropertyAccess::Success;
		}
	}

	return FPropertyAccess::Fail;
}

FPropertyAccess::Result FPropertyHandleBase::GetPerObjectValues( TArray<FString>& OutPerObjectValues ) const
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() && PropertyNode->GetProperty() )
	{
		// Get a list of addresses for objects handled by the property window.
		FReadAddressList ReadAddresses;
		PropertyNode->GetReadAddress( !!PropertyNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses, false );

		UProperty* NodeProperty = PropertyNode->GetProperty();

		if( ReadAddresses.Num() > 0 )
		{
			// Copy each object's value into the value list
			OutPerObjectValues.SetNum( ReadAddresses.Num(), /*bAllowShrinking*/false );
			for ( int32 AddrIndex = 0 ; AddrIndex < ReadAddresses.Num() ; ++AddrIndex )
			{
				uint8* Address = ReadAddresses.GetAddress(AddrIndex);
				if( Address )
				{
					NodeProperty->ExportText_Direct(OutPerObjectValues[AddrIndex], Address, Address, nullptr, 0 );
				}
				else
				{
					OutPerObjectValues[AddrIndex].Reset();
				}
			}

			Result = FPropertyAccess::Success;
		}
	}
	
	return Result;
}

FPropertyAccess::Result FPropertyHandleBase::SetPerObjectValue( const int32 ObjectIndex, const FString& ObjectValue, EPropertyValueSetFlags::Type Flags )
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;

	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if (PropertyNode.IsValid() && PropertyNode->GetProperty())
	{
		Implementation->EnumerateObjectsToModify(PropertyNode.Get(), [&](const FObjectBaseAddress& ObjectToModify, const int32 ObjIndex, const int32 NumObjects) -> bool
		{
			if (ObjIndex == ObjectIndex)
			{
				TArray<FObjectBaseAddress> ObjectsToModify;
				ObjectsToModify.Add(ObjectToModify);

				TArray<FString> PerObjectValues;
				PerObjectValues.Add(ObjectValue);

				Implementation->ImportText(ObjectsToModify, PerObjectValues, PropertyNode.Get(), Flags);

				Result = FPropertyAccess::Success;
				return false; // End enumeration
			}
			return true;
		});
	}

	return Result;
}

FPropertyAccess::Result FPropertyHandleBase::GetPerObjectValue( const int32 ObjectIndex, FString& OutObjectValue ) const
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;

	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if (PropertyNode.IsValid() && PropertyNode->GetProperty())
	{
		// Get a list of addresses for objects handled by the property window.
		FReadAddressList ReadAddresses;
		PropertyNode->GetReadAddress(!!PropertyNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses, false);

		UProperty* NodeProperty = PropertyNode->GetProperty();

		if (ReadAddresses.IsValidIndex(ObjectIndex))
		{
			uint8* Address = ReadAddresses.GetAddress(ObjectIndex);
			if (Address)
			{
				NodeProperty->ExportText_Direct(OutObjectValue, Address, Address, nullptr, 0);
			}
			else
			{
				OutObjectValue.Reset();
			}

			Result = FPropertyAccess::Success;
		}
	}

	return Result;
}


bool FPropertyHandleBase::GeneratePossibleValues(TArray< TSharedPtr<FString> >& OutOptionStrings, TArray< FText >& OutToolTips, TArray<bool>& OutRestrictedItems)
{
	UProperty* Property = GetProperty();

	bool bUsesAlternateDisplayValues = false;

	UEnum* Enum = nullptr;
	if( const UByteProperty* ByteProperty = Cast<const UByteProperty>( Property ) )
	{
		Enum = ByteProperty->Enum;
	}
	else if( const UEnumProperty* EnumProperty = Cast<const UEnumProperty>( Property ) )
	{
		Enum = EnumProperty->GetEnum();
	}
	else if ( Property->IsA(UStrProperty::StaticClass()) && Property->HasMetaData( TEXT("Enum") ) )
	{
		const FString& EnumName = Property->GetMetaData(TEXT("Enum"));
		Enum = FindObject<UEnum>(ANY_PACKAGE, *EnumName, true);
		check( Enum );
	}

	if( Enum )
	{
		const TArray<FName> ValidEnumValues = PropertyEditorHelpers::GetValidEnumsFromPropertyOverride(Property, Enum);

		//NumEnums() - 1, because the last item in an enum is the _MAX item
		for( int32 EnumIndex = 0; EnumIndex < Enum->NumEnums() - 1; ++EnumIndex )
		{
			// Ignore hidden enums
			bool bShouldBeHidden = Enum->HasMetaData(TEXT("Hidden"), EnumIndex ) || Enum->HasMetaData(TEXT("Spacer"), EnumIndex );
			if (!bShouldBeHidden && ValidEnumValues.Num() != 0)
			{
				bShouldBeHidden = ValidEnumValues.Find(Enum->GetNameByIndex(EnumIndex)) == INDEX_NONE;
			}

			if (!bShouldBeHidden)
			{
				bShouldBeHidden = IsHidden(Enum->GetNameStringByIndex(EnumIndex));
			}

			if( !bShouldBeHidden )
			{
				// See if we specified an alternate name for this value using metadata
				FString EnumName = Enum->GetNameStringByIndex(EnumIndex);
				FString EnumDisplayName = Enum->GetDisplayNameTextByIndex(EnumIndex).ToString();

				FText RestrictionTooltip;
				const bool bIsRestricted = GenerateRestrictionToolTip(EnumName, RestrictionTooltip);
				OutRestrictedItems.Add(bIsRestricted);

				if (EnumDisplayName.Len() == 0)
				{
					EnumDisplayName = MoveTemp(EnumName);
				}
				else
				{
					bUsesAlternateDisplayValues = true;
				}

				TSharedPtr< FString > EnumStr(new FString(EnumDisplayName));
				OutOptionStrings.Add(EnumStr);

				FText EnumValueToolTip = bIsRestricted ? RestrictionTooltip : Enum->GetToolTipTextByIndex(EnumIndex);
				OutToolTips.Add(MoveTemp(EnumValueToolTip));
			}
			else
			{
				OutToolTips.Add(FText());
			}
		}
	}
	else if( Property->IsA(UClassProperty::StaticClass()) || Property->IsA(USoftClassProperty::StaticClass()) )		
	{
		UClass* MetaClass = Property->IsA(UClassProperty::StaticClass()) 
			? CastChecked<UClassProperty>(Property)->MetaClass
			: CastChecked<USoftClassProperty>(Property)->MetaClass;

		TSharedPtr< FString > NoneStr( new FString( TEXT("None") ) );
		OutOptionStrings.Add( NoneStr );

		const bool bAllowAbstract = Property->GetOwnerProperty()->HasMetaData(TEXT("AllowAbstract"));
		const bool bBlueprintBaseOnly = Property->GetOwnerProperty()->HasMetaData(TEXT("BlueprintBaseOnly"));
		const bool bAllowOnlyPlaceable = Property->GetOwnerProperty()->HasMetaData(TEXT("OnlyPlaceable"));
		UClass* InterfaceThatMustBeImplemented = Property->GetOwnerProperty()->GetClassMetaData(TEXT("MustImplement"));

		if (!bAllowOnlyPlaceable || MetaClass->IsChildOf<AActor>())
		{
			for (TObjectIterator<UClass> It; It; ++It)
			{
				if (It->IsChildOf(MetaClass)
					&& PropertyEditorHelpers::IsEditInlineClassAllowed(*It, bAllowAbstract)
					&& (!bBlueprintBaseOnly || FKismetEditorUtilities::CanCreateBlueprintOfClass(*It))
					&& (!InterfaceThatMustBeImplemented || It->ImplementsInterface(InterfaceThatMustBeImplemented))
					&& (!bAllowOnlyPlaceable || !It->HasAnyClassFlags(CLASS_Abstract | CLASS_NotPlaceable)))
				{
					OutOptionStrings.Add(TSharedPtr< FString >(new FString(It->GetName())));
				}
			}
		}
	}

	return bUsesAlternateDisplayValues;
}

void FPropertyHandleBase::NotifyPreChange()
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		PropertyNode->NotifyPreChange( PropertyNode->GetProperty(), Implementation->GetNotifyHook() );
	}
}

void FPropertyHandleBase::NotifyPostChange( EPropertyChangeType::Type ChangeType )
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		TArray<const UObject*> ObjectsBeingChanged;
		FObjectPropertyNode* ObjectNode = PropertyNode->FindObjectItemParent();
		if(ObjectNode)
		{
			ObjectsBeingChanged.Reserve(ObjectNode->GetNumObjects());
			for(int32 ObjectIndex = 0; ObjectIndex < ObjectNode->GetNumObjects(); ++ObjectIndex)
			{
				ObjectsBeingChanged.Add(ObjectNode->GetUObject(ObjectIndex));
			}
		}

		FPropertyChangedEvent PropertyChangedEvent( PropertyNode->GetProperty(), ChangeType, &ObjectsBeingChanged );
		PropertyNode->NotifyPostChange( PropertyChangedEvent, Implementation->GetNotifyHook());
	}
}

void FPropertyHandleBase::NotifyFinishedChangingProperties()
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		FPropertyChangedEvent ChangeEvent(PropertyNode->GetProperty(), EPropertyChangeType::ValueSet);
		PropertyNode->FixPropertiesInEvent(ChangeEvent);
		Implementation->GetPropertyUtilities()->NotifyFinishedChangingProperties(ChangeEvent);
	}
}

FPropertyAccess::Result FPropertyHandleBase::SetObjectValueFromSelection()
{
	return Implementation->OnUseSelected();
}

void FPropertyHandleBase::AddRestriction( TSharedRef<const FPropertyRestriction> Restriction )
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		PropertyNode->AddRestriction(Restriction);
	}
}

bool FPropertyHandleBase::IsRestricted(const FString& Value) const
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		return PropertyNode->IsRestricted(Value);
	}
	return false;
}

bool FPropertyHandleBase::IsRestricted(const FString& Value, TArray<FText>& OutReasons) const
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		return PropertyNode->IsRestricted(Value, OutReasons);
	}
	return false;
}

bool FPropertyHandleBase::IsHidden(const FString& Value) const
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		return PropertyNode->IsHidden(Value);
	}
	return false;
}

bool FPropertyHandleBase::IsHidden(const FString& Value, TArray<FText>& OutReasons) const
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		return PropertyNode->IsHidden(Value, &OutReasons);
	}
	return false;
}

bool FPropertyHandleBase::IsDisabled(const FString& Value) const
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		return PropertyNode->IsDisabled(Value);
	}
	return false;
}

bool FPropertyHandleBase::IsDisabled(const FString& Value, TArray<FText>& OutReasons) const
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		return PropertyNode->IsDisabled(Value, &OutReasons);
	}
	return false;
}

bool FPropertyHandleBase::GenerateRestrictionToolTip(const FString& Value, FText& OutTooltip)const
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		return PropertyNode->GenerateRestrictionToolTip(Value, OutTooltip);
	}
	return false;
}

void FPropertyHandleBase::SetIgnoreValidation(bool bInIgnore)
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		PropertyNode->SetNodeFlags( EPropertyNodeFlags::SkipChildValidation, bInIgnore); 
	}
}

TArray<TSharedPtr<IPropertyHandle>> FPropertyHandleBase::AddChildStructure( TSharedRef<FStructOnScope> InStruct )
{
	TArray<TSharedPtr<IPropertyHandle>> PropertyHandles;

	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetPropertyNode();
	if( !PropertyNode.IsValid() )
	{
		return PropertyHandles;
	}

	TSharedPtr<FStructurePropertyNode> StructPropertyNode( new FStructurePropertyNode );
	StructPropertyNode->SetStructure(InStruct);

	FPropertyNodeInitParams RootInitParams;
	RootInitParams.ParentNode = PropertyNode;
	RootInitParams.Property = nullptr;
	RootInitParams.ArrayOffset = 0;
	RootInitParams.ArrayIndex = INDEX_NONE;
	RootInitParams.bAllowChildren = true;
	RootInitParams.bForceHiddenPropertyVisibility = FPropertySettings::Get().ShowHiddenProperties();
	RootInitParams.bCreateCategoryNodes = false;

	StructPropertyNode->InitNode(RootInitParams);

	const bool bShouldShowHiddenProperties = !!PropertyNode->HasNodeFlags(EPropertyNodeFlags::ShouldShowHiddenProperties);
	const bool bShouldShowDisableEditOnInstance = !!PropertyNode->HasNodeFlags(EPropertyNodeFlags::ShouldShowDisableEditOnInstance);

	for (TFieldIterator<UProperty> It(InStruct->GetStruct()); It; ++It)
	{
		UProperty* StructMember = *It;
		if (!StructMember)
		{
			continue;
		}

		static const FName Name_InlineEditConditionToggle("InlineEditConditionToggle");
		const bool bOnlyShowAsInlineEditCondition = StructMember->HasMetaData(Name_InlineEditConditionToggle);
		const bool bShowIfEditableProperty = StructMember->HasAnyPropertyFlags(CPF_Edit);
		const bool bShowIfDisableEditOnInstance = !StructMember->HasAnyPropertyFlags(CPF_DisableEditOnInstance) || bShouldShowDisableEditOnInstance;

		if (bShouldShowHiddenProperties || (bShowIfEditableProperty && !bOnlyShowAsInlineEditCondition && bShowIfDisableEditOnInstance))
		{
			TSharedRef<FItemPropertyNode> NewItemNode(new FItemPropertyNode);

			FPropertyNodeInitParams InitParams;
			InitParams.ParentNode = StructPropertyNode;
			InitParams.Property = StructMember;
			InitParams.ArrayOffset = 0;
			InitParams.ArrayIndex = INDEX_NONE;
			InitParams.bAllowChildren = true;
			InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;
			InitParams.bCreateDisableEditOnInstanceNodes = bShouldShowDisableEditOnInstance;
			InitParams.bCreateCategoryNodes = false;

			NewItemNode->InitNode(InitParams);
			StructPropertyNode->AddChildNode(NewItemNode);

			PropertyHandles.Add(PropertyEditorHelpers::GetPropertyHandle(NewItemNode, Implementation->GetNotifyHook(), Implementation->GetPropertyUtilities()));
		}
	}

	PropertyNode->AddChildNode(StructPropertyNode);

	return PropertyHandles;
}

bool FPropertyHandleBase::CanResetToDefault() const
{
	UProperty* Property = GetProperty();

	// Should not be able to reset fixed size arrays
	const bool bFixedSized = Property && Property->PropertyFlags & CPF_EditFixedSize;
	const bool bCanResetToDefault = !(Property && Property->PropertyFlags & CPF_Config);

	return Property && bCanResetToDefault && !bFixedSized && DiffersFromDefault();
}

void FPropertyHandleBase::ExecuteCustomResetToDefault(const FResetToDefaultOverride& InOnCustomResetToDefault)
{
	// This action must be deferred until next tick so that we avoid accessing invalid data before we have a chance to tick
	Implementation->GetPropertyUtilities()->EnqueueDeferredAction(FSimpleDelegate::CreateLambda([this, InOnCustomResetToDefault]() { OnCustomResetToDefault(InOnCustomResetToDefault); }));
}

/** Implements common property value functions */
#define IMPLEMENT_PROPERTY_VALUE( ClassName ) \
	ClassName::ClassName( TSharedRef<FPropertyNode> PropertyNode, FNotifyHook* NotifyHook, TSharedPtr<IPropertyUtilities> PropertyUtilities ) \
	: FPropertyHandleBase( PropertyNode, NotifyHook, PropertyUtilities )  \
	{}

IMPLEMENT_PROPERTY_VALUE( FPropertyHandleInt )
IMPLEMENT_PROPERTY_VALUE( FPropertyHandleFloat )
IMPLEMENT_PROPERTY_VALUE( FPropertyHandleDouble )
IMPLEMENT_PROPERTY_VALUE( FPropertyHandleBool )
IMPLEMENT_PROPERTY_VALUE( FPropertyHandleByte )
IMPLEMENT_PROPERTY_VALUE( FPropertyHandleString )
IMPLEMENT_PROPERTY_VALUE( FPropertyHandleObject )
IMPLEMENT_PROPERTY_VALUE( FPropertyHandleArray )
IMPLEMENT_PROPERTY_VALUE( FPropertyHandleText )
IMPLEMENT_PROPERTY_VALUE( FPropertyHandleSet )
IMPLEMENT_PROPERTY_VALUE( FPropertyHandleMap )

// int32 
bool FPropertyHandleInt::Supports( TSharedRef<FPropertyNode> PropertyNode )
{
	UProperty* Property = PropertyNode->GetProperty();

	if ( Property == nullptr )
	{
		return false;
	}

	const bool bIsInteger = 
			Property->IsA<UInt8Property>()
		||	Property->IsA<UInt16Property>()
		||  Property->IsA<UIntProperty>()
		||	Property->IsA<UInt64Property>()
		||  Property->IsA<UUInt16Property>()
		||  Property->IsA<UUInt32Property>()
		||  Property->IsA<UUInt64Property>();

	// The value is an integer
	return bIsInteger;
}

template <typename PropertyClass, typename ValueType>
ValueType GetIntegerValue(void* PropValue, FPropertyValueImpl& Implementation )
{
	check( Implementation.IsPropertyTypeOf( PropertyClass::StaticClass() ) );
	return Implementation.GetPropertyValue<PropertyClass>(PropValue);
}

FPropertyAccess::Result FPropertyHandleInt::GetValue(int8& OutValue) const
{
	void* PropValue = nullptr;
	FPropertyAccess::Result Res = Implementation->GetValueData(PropValue);

	if (Res == FPropertyAccess::Success)
	{
		OutValue = GetIntegerValue<UInt8Property, int8>(PropValue, *Implementation);
	}

	return Res;
}

FPropertyAccess::Result FPropertyHandleInt::GetValue(int16& OutValue) const
{
	void* PropValue = nullptr;
	FPropertyAccess::Result Res = Implementation->GetValueData(PropValue);

	if (Res == FPropertyAccess::Success)
	{
		OutValue = GetIntegerValue<UInt16Property, int16>(PropValue, *Implementation);
	}

	return Res;
}

FPropertyAccess::Result FPropertyHandleInt::GetValue(int32& OutValue) const
{
	void* PropValue = nullptr;
	FPropertyAccess::Result Res = Implementation->GetValueData(PropValue);

	if (Res == FPropertyAccess::Success)
	{
		OutValue = GetIntegerValue<UIntProperty, int32>(PropValue, *Implementation);
	}

	return Res;
}
FPropertyAccess::Result FPropertyHandleInt::GetValue( int64& OutValue ) const
{
	void* PropValue = nullptr;
	FPropertyAccess::Result Res = Implementation->GetValueData( PropValue );

	if( Res == FPropertyAccess::Success )
	{
		OutValue = GetIntegerValue<UInt64Property,int64>(PropValue, *Implementation);
	}

	return Res;
}

FPropertyAccess::Result FPropertyHandleInt::GetValue(uint16& OutValue) const
{
	void* PropValue = nullptr;
	FPropertyAccess::Result Res = Implementation->GetValueData(PropValue);

	if (Res == FPropertyAccess::Success)
	{
		OutValue = GetIntegerValue<UUInt16Property, uint16>(PropValue, *Implementation);
	}

	return Res;
}

FPropertyAccess::Result FPropertyHandleInt::GetValue(uint32& OutValue) const
{
	void* PropValue = nullptr;
	FPropertyAccess::Result Res = Implementation->GetValueData(PropValue);

	if (Res == FPropertyAccess::Success)
	{
		OutValue = GetIntegerValue<UUInt32Property, uint32>(PropValue, *Implementation);
	}

	return Res;
}

FPropertyAccess::Result FPropertyHandleInt::GetValue(uint64& OutValue) const
{
	void* PropValue = nullptr;
	FPropertyAccess::Result Res = Implementation->GetValueData(PropValue);

	if (Res == FPropertyAccess::Success)
	{
		OutValue = GetIntegerValue<UUInt64Property, uint64>(PropValue, *Implementation);
	}

	return Res;
}

FPropertyAccess::Result FPropertyHandleInt::SetValue(const int8& NewValue, EPropertyValueSetFlags::Type Flags)
{
	FPropertyAccess::Result Res;
	// Clamp the value from any meta data ranges stored on the property value
	int8 FinalValue = ClampIntegerValueFromMetaData<int8>( NewValue, *Implementation->GetPropertyNode() );

	const FString ValueStr = Lex::ToString(FinalValue);
	Res = Implementation->ImportText(ValueStr, Flags);

	return Res;
}


FPropertyAccess::Result FPropertyHandleInt::SetValue(const int16& NewValue, EPropertyValueSetFlags::Type Flags)
{
	FPropertyAccess::Result Res;
	// Clamp the value from any meta data ranges stored on the property value
	int16 FinalValue = ClampIntegerValueFromMetaData<int16>(NewValue, *Implementation->GetPropertyNode());

	const FString ValueStr = Lex::ToString(FinalValue);
	Res = Implementation->ImportText(ValueStr, Flags);

	return Res;
}


FPropertyAccess::Result FPropertyHandleInt::SetValue( const int32& NewValue, EPropertyValueSetFlags::Type Flags )
{
	FPropertyAccess::Result Res;
	// Clamp the value from any meta data ranges stored on the property value
	int32 FinalValue = ClampIntegerValueFromMetaData<int32>( NewValue, *Implementation->GetPropertyNode() );

	const FString ValueStr = Lex::ToString(FinalValue);
	Res = Implementation->ImportText( ValueStr, Flags );

	return Res;
}

FPropertyAccess::Result FPropertyHandleInt::SetValue(const int64& NewValue, EPropertyValueSetFlags::Type Flags)
{
	FPropertyAccess::Result Res;

	// Clamp the value from any meta data ranges stored on the property value
	int64 FinalValue = ClampIntegerValueFromMetaData<int64>(NewValue, *Implementation->GetPropertyNode());

	const FString ValueStr = Lex::ToString(FinalValue);
	Res = Implementation->ImportText(ValueStr, Flags);
	return Res;
}

FPropertyAccess::Result FPropertyHandleInt::SetValue(const uint16& NewValue, EPropertyValueSetFlags::Type Flags)
{
	FPropertyAccess::Result Res;
	// Clamp the value from any meta data ranges stored on the property value
	uint16 FinalValue = ClampIntegerValueFromMetaData<uint16>(NewValue, *Implementation->GetPropertyNode());

	const FString ValueStr = Lex::ToString(FinalValue);
	Res = Implementation->ImportText(ValueStr, Flags);

	return Res;
}


FPropertyAccess::Result FPropertyHandleInt::SetValue(const uint32& NewValue, EPropertyValueSetFlags::Type Flags)
{
	FPropertyAccess::Result Res;
	// Clamp the value from any meta data ranges stored on the property value
	uint32 FinalValue = ClampIntegerValueFromMetaData<uint32>(NewValue, *Implementation->GetPropertyNode());

	const FString ValueStr = Lex::ToString(FinalValue);
	Res = Implementation->ImportText(ValueStr, Flags);

	return Res;
}

FPropertyAccess::Result FPropertyHandleInt::SetValue(const uint64& NewValue, EPropertyValueSetFlags::Type Flags)
{
	FPropertyAccess::Result Res;
	// Clamp the value from any meta data ranges stored on the property value
	uint64 FinalValue = ClampIntegerValueFromMetaData<uint64>(NewValue, *Implementation->GetPropertyNode());

	const FString ValueStr = Lex::ToString(FinalValue);
	Res = Implementation->ImportText(ValueStr, Flags);
	return Res;
}

// float
bool FPropertyHandleFloat::Supports( TSharedRef<FPropertyNode> PropertyNode )
{
	UProperty* Property = PropertyNode->GetProperty();

	if ( Property == nullptr )
	{
		return false;
	}

	return Property->IsA(UFloatProperty::StaticClass());
}

FPropertyAccess::Result FPropertyHandleFloat::GetValue( float& OutValue ) const
{
	void* PropValue = nullptr;
	FPropertyAccess::Result Res = Implementation->GetValueData( PropValue );

	if( Res == FPropertyAccess::Success )
	{
		OutValue = Implementation->GetPropertyValue<UFloatProperty>(PropValue);
	}

	return Res;
}

FPropertyAccess::Result FPropertyHandleFloat::SetValue( const float& NewValue, EPropertyValueSetFlags::Type Flags )
{
	FPropertyAccess::Result Res;
	// Clamp the value from any meta data ranges stored on the property value
	float FinalValue = ClampValueFromMetaData<float>( NewValue, *Implementation->GetPropertyNode() );

	const FString ValueStr = FString::Printf( TEXT("%f"), FinalValue );
	Res = Implementation->ImportText( ValueStr, Flags );

	return Res;
}

// double
bool FPropertyHandleDouble::Supports( TSharedRef<FPropertyNode> PropertyNode )
{
	UProperty* Property = PropertyNode->GetProperty();

	if (Property == nullptr)
	{
		return false;
	}

	return Property->IsA(UDoubleProperty::StaticClass());
}

FPropertyAccess::Result FPropertyHandleDouble::GetValue( double& OutValue ) const
{
	void* PropValue = nullptr;
	FPropertyAccess::Result Res = Implementation->GetValueData( PropValue );

	if (Res == FPropertyAccess::Success)
	{
		OutValue = Implementation->GetPropertyValue<UDoubleProperty>(PropValue);
	}

	return Res;
}

FPropertyAccess::Result FPropertyHandleDouble::SetValue( const double& NewValue, EPropertyValueSetFlags::Type Flags )
{
	FPropertyAccess::Result Res;
	// Clamp the value from any meta data ranges stored on the property value
	float FinalValue = ClampValueFromMetaData<double>( NewValue, *Implementation->GetPropertyNode() );

	const FString ValueStr = FString::Printf( TEXT("%f"), FinalValue );
	Res = Implementation->ImportText( ValueStr, Flags );

	return Res;
}

// bool
bool FPropertyHandleBool::Supports( TSharedRef<FPropertyNode> PropertyNode )
{
	UProperty* Property = PropertyNode->GetProperty();

	if ( Property == nullptr )
	{
		return false;
	}

	return Property->IsA(UBoolProperty::StaticClass());
}

FPropertyAccess::Result FPropertyHandleBool::GetValue( bool& OutValue ) const
{
	void* PropValue = nullptr;
	FPropertyAccess::Result Res = Implementation->GetValueData( PropValue );

	if( Res == FPropertyAccess::Success )
	{
		OutValue = Implementation->GetPropertyValue<UBoolProperty>(PropValue);
	}

	return Res;
}

FPropertyAccess::Result FPropertyHandleBool::SetValue( const bool& NewValue, EPropertyValueSetFlags::Type Flags )
{
	FPropertyAccess::Result Res = FPropertyAccess::Fail;

	//These are not localized values because ImportText does not accept localized values!
	FString ValueStr; 
	if( NewValue == false )
	{
		ValueStr = TEXT("False");
	}
	else
	{
		ValueStr = TEXT("True");
	}

	Res = Implementation->ImportText( ValueStr, Flags );

	return Res;
}

bool FPropertyHandleByte::Supports( TSharedRef<FPropertyNode> PropertyNode )
{
	UProperty* Property = PropertyNode->GetProperty();

	if ( Property == nullptr )
	{
		return false;
	}

	return Property->IsA<UByteProperty>() || Property->IsA<UEnumProperty>();
}

FPropertyAccess::Result FPropertyHandleByte::GetValue( uint8& OutValue ) const
{
	void* PropValue = nullptr;
	FPropertyAccess::Result Res = Implementation->GetValueData( PropValue );

	if( Res == FPropertyAccess::Success )
	{
		TSharedPtr<FPropertyNode> PropertyNodePin = Implementation->GetPropertyNode();

		UProperty* Property = PropertyNodePin->GetProperty();
		if( Property->IsA<UByteProperty>() )
		{
			OutValue = Implementation->GetPropertyValue<UByteProperty>(PropValue);
		}
		else
		{
			check(PropertyNodePin.IsValid());
			OutValue = CastChecked<UEnumProperty>(Property)->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(PropValue);
		}
	}

	return Res;
}

FPropertyAccess::Result FPropertyHandleByte::SetValue( const uint8& NewValue, EPropertyValueSetFlags::Type Flags )
{
	FPropertyAccess::Result Res;
	FString ValueStr;

	UProperty* Property = GetProperty();

	UEnum* Enum = nullptr;
	if (UByteProperty* ByteProperty = Cast<UByteProperty>(Property))
	{
		Enum = ByteProperty->Enum;
	}
	else if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Enum = EnumProperty->GetEnum();
	}

	if (Enum)
	{
		// Handle Enums using enum names to make sure they're compatible with UByteProperty::ExportText.
		ValueStr = Enum->GetNameStringByValue(NewValue);
	}
	else
	{
		// Ordinary byte, convert value to string.
		ValueStr = FString::Printf( TEXT("%i"), NewValue );
	}
	Res = Implementation->ImportText( ValueStr, Flags );

	return Res;
}


// String
bool FPropertyHandleString::Supports( TSharedRef<FPropertyNode> PropertyNode )
{
	UProperty* Property = PropertyNode->GetProperty();

	if ( Property == nullptr )
	{
		return false;
	}

	// Supported if the property is a name, string or object/interface that can be set via string
	return	( Property->IsA(UNameProperty::StaticClass()) && Property->GetFName() != NAME_InitialState )
		||	Property->IsA( UStrProperty::StaticClass() )
		||	( Property->IsA( UObjectPropertyBase::StaticClass() ) && !Property->HasAnyPropertyFlags(CPF_InstancedReference) )
		||	Property->IsA(UInterfaceProperty::StaticClass());
}

FPropertyAccess::Result FPropertyHandleString::GetValue( FString& OutValue ) const
{
	return Implementation->GetValueAsString( OutValue );
}

FPropertyAccess::Result FPropertyHandleString::SetValue( const FString& NewValue, EPropertyValueSetFlags::Type Flags )
{
	return Implementation->SetValueAsString( NewValue, Flags );
}

FPropertyAccess::Result FPropertyHandleString::GetValue( FName& OutValue ) const
{
	void* PropValue = nullptr;
	FPropertyAccess::Result Res = Implementation->GetValueData( PropValue );

	if( Res == FPropertyAccess::Success )
	{
		OutValue = Implementation->GetPropertyValue<UNameProperty>(PropValue);
	}

	return Res;
}

FPropertyAccess::Result FPropertyHandleString::SetValue( const FName& NewValue, EPropertyValueSetFlags::Type Flags )
{
	return Implementation->SetValueAsString( NewValue.ToString(), Flags );
}

// Object

bool FPropertyHandleObject::Supports( TSharedRef<FPropertyNode> PropertyNode )
{
	UProperty* Property = PropertyNode->GetProperty();

	if ( Property == nullptr )
	{
		return false;
	}

	return Property->IsA( UObjectPropertyBase::StaticClass() ) || Property->IsA(UInterfaceProperty::StaticClass());
}

FPropertyAccess::Result FPropertyHandleObject::GetValue( UObject*& OutValue ) const
{
	return FPropertyHandleObject::GetValue((const UObject*&)OutValue);
}

FPropertyAccess::Result FPropertyHandleObject::GetValue( const UObject*& OutValue ) const
{
	void* PropValue = nullptr;
	FPropertyAccess::Result Res = Implementation->GetValueData( PropValue );

	if( Res == FPropertyAccess::Success )
	{
		UProperty* Property = GetProperty();

		if (Property->IsA(UObjectPropertyBase::StaticClass()))
		{
			OutValue = Implementation->GetObjectPropertyValue(PropValue);
		}
		else if (Property->IsA(UInterfaceProperty::StaticClass()))
		{
			UInterfaceProperty* InterfaceProp = Cast<UInterfaceProperty>(Property);
			const FScriptInterface& ScriptInterface = InterfaceProp->GetPropertyValue(PropValue);
			OutValue = ScriptInterface.GetObject();
		}
	}

	return Res;
}

FPropertyAccess::Result FPropertyHandleObject::SetValue( UObject* const& NewValue, EPropertyValueSetFlags::Type Flags )
{
	return FPropertyHandleObject::SetValue((const UObject*)NewValue);
}

FPropertyAccess::Result FPropertyHandleObject::SetValue( const UObject* const& NewValue, EPropertyValueSetFlags::Type Flags )
{
	UProperty* Property = Implementation->GetPropertyNode()->GetProperty();

	bool bResult = false;
	// Instanced references can not be set this way (most likely editinlinenew )
	if( !Property->HasAnyPropertyFlags(CPF_InstancedReference) )
	{
		FString ObjectPathName = NewValue ? NewValue->GetPathName() : TEXT("None");
		bResult = Implementation->SendTextToObjectProperty( ObjectPathName, Flags );
	}

	return bResult ? FPropertyAccess::Success : FPropertyAccess::Fail;
}

FPropertyAccess::Result FPropertyHandleObject::GetValue(FAssetData& OutValue) const
{
	UObject* ObjectValue = nullptr;
	FPropertyAccess::Result	Result = GetValue(ObjectValue);
	
	if ( Result == FPropertyAccess::Success )
	{
		OutValue = FAssetData(ObjectValue);
	}

	return Result;
}

FPropertyAccess::Result FPropertyHandleObject::SetValue(const FAssetData& NewValue, EPropertyValueSetFlags::Type Flags)
{
	UProperty* Property = Implementation->GetPropertyNode()->GetProperty();

	bool bResult = false;
	// Instanced references can not be set this way (most likely editinlinenew )
	if (!Property->HasAnyPropertyFlags(CPF_InstancedReference))
	{
		if ( !Property->IsA( USoftObjectProperty::StaticClass() ) )
		{
			// Make sure the asset is loaded if we are not a string asset reference.
			NewValue.GetAsset();
		}

		FString ObjectPathName = NewValue.IsValid() ? NewValue.ObjectPath.ToString() : TEXT("None");
		bResult = Implementation->SendTextToObjectProperty(ObjectPathName, Flags);
	}

	return bResult ? FPropertyAccess::Success : FPropertyAccess::Fail;
}

FPropertyAccess::Result FPropertyHandleObject::SetValueFromFormattedString(const FString& InValue, EPropertyValueSetFlags::Type Flags)
{
	UProperty* Property = GetProperty();
	const FString EmptyString = FString();
	const FString& AllowedClassesString = Property ? Property->GetMetaData("AllowedClasses") : EmptyString;

	if (Property && !AllowedClassesString.IsEmpty())
	{
		const TCHAR* ObjectBuffer = *InValue;
		UObject* QualifiedObject = nullptr;

		// Check to see if the object we're attempting to import has a class that's allowed for the property. If not, bail early.
		if (UObjectPropertyBase::ParseObjectPropertyValue(Property, Property->GetOuter(), UObject::StaticClass(), 0, ObjectBuffer, QualifiedObject))
		{
			TArray<FString> AllowedClassNames;
			AllowedClassesString.ParseIntoArray(AllowedClassNames, TEXT(","), true);
			bool bSupportedObject = false;

			for (auto ClassNameIt = AllowedClassNames.CreateConstIterator(); ClassNameIt; ++ClassNameIt)
			{
				const FString& ClassName = *ClassNameIt;
				UClass* AllowedClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);
				const bool bIsInterface = AllowedClass && AllowedClass->HasAnyClassFlags(CLASS_Interface);
				
				// Check if the object is an allowed class type this property supports
				// Note: QualifieObject may be null if we're clearing the value.  Allow clears to pass through without a supported class
				if (AllowedClass && (!QualifiedObject || QualifiedObject->IsA(AllowedClass) || (bIsInterface && QualifiedObject->GetClass()->ImplementsInterface(AllowedClass))))
				{
					bSupportedObject = true;
					break;
				}
			}

			if (!bSupportedObject)
			{
				return FPropertyAccess::Fail;
			}
		}
		else
		{
			// Not an object, so bail.
			return FPropertyAccess::Fail;
		}
	}

	return FPropertyHandleBase::SetValueFromFormattedString(InValue, Flags);
}

// Vector
bool FPropertyHandleVector::Supports( TSharedRef<FPropertyNode> PropertyNode )
{
	UProperty* Property = PropertyNode->GetProperty();

	if ( Property == nullptr )
	{
		return false;
	}

	UStructProperty* StructProp = Cast<UStructProperty>(Property);

	bool bSupported = false;
	if( StructProp && StructProp->Struct )
	{
		FName StructName = StructProp->Struct->GetFName();

		bSupported = StructName == NAME_Vector ||
			StructName == NAME_Vector2D ||
			StructName == NAME_Vector4 ||
			StructName == NAME_Quat;
	}

	return bSupported;
}

FPropertyHandleVector::FPropertyHandleVector( TSharedRef<class FPropertyNode> PropertyNode, class FNotifyHook* NotifyHook, TSharedPtr<IPropertyUtilities> PropertyUtilities )
	: FPropertyHandleBase( PropertyNode, NotifyHook, PropertyUtilities ) 
{
	const bool bRecurse = false;
	// A vector is a struct property that has 3 children.  We get/set the values from the children
	VectorComponents.Add( MakeShareable( new FPropertyHandleFloat( Implementation->GetChildNode("X", bRecurse).ToSharedRef(), NotifyHook, PropertyUtilities ) ) );

	VectorComponents.Add( MakeShareable( new FPropertyHandleFloat( Implementation->GetChildNode("Y", bRecurse).ToSharedRef(), NotifyHook, PropertyUtilities ) ) );

	if( Implementation->GetNumChildren() > 2 )
	{
		// at least a 3 component vector
		VectorComponents.Add( MakeShareable( new FPropertyHandleFloat( Implementation->GetChildNode("Z",bRecurse).ToSharedRef(), NotifyHook, PropertyUtilities ) ) );
	}
	if( Implementation->GetNumChildren() > 3 )
	{
		// a 4 component vector
		VectorComponents.Add( MakeShareable( new FPropertyHandleFloat( Implementation->GetChildNode("W",bRecurse).ToSharedRef(), NotifyHook, PropertyUtilities ) ) );
	}
}

FPropertyAccess::Result FPropertyHandleVector::GetValue( FVector2D& OutValue ) const
{
	if( VectorComponents.Num() == 2 )
	{
		// To get the value from the vector we read each child.  If reading a child fails, the value for that component is not set
		FPropertyAccess::Result ResX = VectorComponents[0]->GetValue( OutValue.X );
		FPropertyAccess::Result ResY = VectorComponents[1]->GetValue( OutValue.Y );

		if( ResX == FPropertyAccess::Fail || ResY == FPropertyAccess::Fail )
		{
			// If reading any value failed the entire value fails
			return FPropertyAccess::Fail;
		}
		else if( ResX == FPropertyAccess::MultipleValues || ResY == FPropertyAccess::MultipleValues )
		{
			// At least one component had multiple values
			return FPropertyAccess::MultipleValues;
		}
		else
		{
			return FPropertyAccess::Success;
		}
	}

	// Not a 2 component vector
	return FPropertyAccess::Fail;
}

FPropertyAccess::Result FPropertyHandleVector::SetValue( const FVector2D& NewValue, EPropertyValueSetFlags::Type Flags )
{
	// To set the value from the vector we set each child. 
	FPropertyAccess::Result ResX = VectorComponents[0]->SetValue( NewValue.X, Flags );
	FPropertyAccess::Result ResY = VectorComponents[1]->SetValue( NewValue.Y, Flags );

	if( ResX == FPropertyAccess::Fail || ResY == FPropertyAccess::Fail )
	{
		return FPropertyAccess::Fail;
	}
	else
	{
		return FPropertyAccess::Success;
	}
}

FPropertyAccess::Result FPropertyHandleVector::GetValue( FVector& OutValue ) const
{
	if( VectorComponents.Num() == 3 )
	{
		// To get the value from the vector we read each child.  If reading a child fails, the value for that component is not set
		FPropertyAccess::Result ResX = VectorComponents[0]->GetValue( OutValue.X );
		FPropertyAccess::Result ResY = VectorComponents[1]->GetValue( OutValue.Y );
		FPropertyAccess::Result ResZ = VectorComponents[2]->GetValue( OutValue.Z );

		if( ResX == FPropertyAccess::Fail || ResY == FPropertyAccess::Fail || ResZ == FPropertyAccess::Fail )
		{
			// If reading any value failed the entire value fails
			return FPropertyAccess::Fail;
		}
		else if( ResX == FPropertyAccess::MultipleValues || ResY == FPropertyAccess::MultipleValues || ResZ == FPropertyAccess::MultipleValues )
		{
			// At least one component had multiple values
			return FPropertyAccess::MultipleValues;
		}
		else
		{
			return FPropertyAccess::Success;
		}
	}

	// Not a 3 component vector
	return FPropertyAccess::Fail;
}

FPropertyAccess::Result FPropertyHandleVector::SetValue( const FVector& NewValue, EPropertyValueSetFlags::Type Flags )
{
	if( VectorComponents.Num() == 3)
	{
		// To set the value from the vector we set each child. 
		FPropertyAccess::Result ResX = VectorComponents[0]->SetValue( NewValue.X, Flags );
		FPropertyAccess::Result ResY = VectorComponents[1]->SetValue( NewValue.Y, Flags );
		FPropertyAccess::Result ResZ = VectorComponents[2]->SetValue( NewValue.Z, Flags );

		if( ResX == FPropertyAccess::Fail || ResY == FPropertyAccess::Fail || ResZ == FPropertyAccess::Fail )
		{
			return FPropertyAccess::Fail;
		}
		else
		{
			return FPropertyAccess::Success;
		}
	}

	return FPropertyAccess::Fail;
}

FPropertyAccess::Result FPropertyHandleVector::GetValue( FVector4& OutValue ) const
{
	if( VectorComponents.Num() == 4 )
	{
		// To get the value from the vector we read each child.  If reading a child fails, the value for that component is not set
		FPropertyAccess::Result ResX = VectorComponents[0]->GetValue( OutValue.X );
		FPropertyAccess::Result ResY = VectorComponents[1]->GetValue( OutValue.Y );
		FPropertyAccess::Result ResZ = VectorComponents[2]->GetValue( OutValue.Z );
		FPropertyAccess::Result ResW = VectorComponents[3]->GetValue( OutValue.W );

		if( ResX == FPropertyAccess::Fail || ResY == FPropertyAccess::Fail || ResZ == FPropertyAccess::Fail || ResW == FPropertyAccess::Fail )
		{
			// If reading any value failed the entire value fails
			return FPropertyAccess::Fail;
		}
		else if( ResX == FPropertyAccess::MultipleValues || ResY == FPropertyAccess::MultipleValues || ResZ == FPropertyAccess::MultipleValues || ResW == FPropertyAccess::MultipleValues )
		{
			// At least one component had multiple values
			return FPropertyAccess::MultipleValues;
		}
		else
		{
			return FPropertyAccess::Success;
		}
	}

	// Not a 4 component vector
	return FPropertyAccess::Fail;
}


FPropertyAccess::Result FPropertyHandleVector::SetValue( const FVector4& NewValue, EPropertyValueSetFlags::Type Flags )
{
	// To set the value from the vector we set each child. 
	FPropertyAccess::Result ResX = VectorComponents[0]->SetValue( NewValue.X, Flags );
	FPropertyAccess::Result ResY = VectorComponents[1]->SetValue( NewValue.Y, Flags );
	FPropertyAccess::Result ResZ = VectorComponents[2]->SetValue( NewValue.Z, Flags );
	FPropertyAccess::Result ResW = VectorComponents[3]->SetValue( NewValue.W, Flags );

	if( ResX == FPropertyAccess::Fail || ResY == FPropertyAccess::Fail || ResZ == FPropertyAccess::Fail || ResW == FPropertyAccess::Fail )
	{
		return FPropertyAccess::Fail;
	}
	else
	{
		return FPropertyAccess::Success;
	}
}

FPropertyAccess::Result FPropertyHandleVector::GetValue( FQuat& OutValue ) const
{
	FVector4 VectorProxy;
	FPropertyAccess::Result Res = GetValue(VectorProxy);
	if (Res == FPropertyAccess::Success)
	{
		OutValue.X = VectorProxy.X;
		OutValue.Y = VectorProxy.Y;
		OutValue.Z = VectorProxy.Z;
		OutValue.W = VectorProxy.W;
	}

	return Res;
}

FPropertyAccess::Result FPropertyHandleVector::SetValue( const FQuat& NewValue, EPropertyValueSetFlags::Type Flags )
{
	FVector4 VectorProxy;
	VectorProxy.X = NewValue.X;
	VectorProxy.Y = NewValue.Y;
	VectorProxy.Z = NewValue.Z;
	VectorProxy.W = NewValue.W;

	return SetValue(VectorProxy);
}

FPropertyAccess::Result FPropertyHandleVector::SetX( float InValue, EPropertyValueSetFlags::Type Flags )
{
	FPropertyAccess::Result Res = VectorComponents[0]->SetValue( InValue, Flags );

	return Res;
}

FPropertyAccess::Result FPropertyHandleVector::SetY( float InValue, EPropertyValueSetFlags::Type Flags )
{
	FPropertyAccess::Result Res = VectorComponents[1]->SetValue( InValue, Flags );

	return Res;
}

FPropertyAccess::Result FPropertyHandleVector::SetZ( float InValue, EPropertyValueSetFlags::Type Flags )
{
	if( VectorComponents.Num() > 2 )
	{
		FPropertyAccess::Result Res = VectorComponents[2]->SetValue( InValue, Flags );

		return Res;
	}
	
	return FPropertyAccess::Fail;
}

FPropertyAccess::Result FPropertyHandleVector::SetW( float InValue, EPropertyValueSetFlags::Type Flags )
{
	if( VectorComponents.Num() == 4 )
	{
		FPropertyAccess::Result Res = VectorComponents[3]->SetValue( InValue, Flags );
	}

	return FPropertyAccess::Fail;
}

// Rotator

bool FPropertyHandleRotator::Supports( TSharedRef<FPropertyNode> PropertyNode )
{
	UProperty* Property = PropertyNode->GetProperty();

	if ( Property == nullptr )
	{
		return false;
	}

	UStructProperty* StructProp = Cast<UStructProperty>(Property);
	return StructProp && StructProp->Struct->GetFName() == NAME_Rotator;
}

FPropertyHandleRotator::FPropertyHandleRotator( TSharedRef<class FPropertyNode> PropertyNode, FNotifyHook* NotifyHook, TSharedPtr<IPropertyUtilities> PropertyUtilities )
	: FPropertyHandleBase( PropertyNode, NotifyHook, PropertyUtilities ) 
{
	const bool bRecurse = false;
	// A vector is a struct property that has 3 children.  We get/set the values from the children
	RollValue = MakeShareable( new FPropertyHandleFloat( Implementation->GetChildNode("Roll", bRecurse).ToSharedRef(), NotifyHook, PropertyUtilities ) );

	PitchValue = MakeShareable( new FPropertyHandleFloat( Implementation->GetChildNode("Pitch", bRecurse).ToSharedRef(), NotifyHook, PropertyUtilities ) );

	YawValue = MakeShareable( new FPropertyHandleFloat( Implementation->GetChildNode("Yaw", bRecurse).ToSharedRef(), NotifyHook, PropertyUtilities ) );
}


FPropertyAccess::Result FPropertyHandleRotator::GetValue( FRotator& OutValue ) const
{
	// To get the value from the rotator we read each child.  If reading a child fails, the value for that component is not set
	FPropertyAccess::Result ResR = RollValue->GetValue( OutValue.Roll );
	FPropertyAccess::Result ResP = PitchValue->GetValue( OutValue.Pitch );
	FPropertyAccess::Result ResY = YawValue->GetValue( OutValue.Yaw );

	if( ResR == FPropertyAccess::MultipleValues || ResP == FPropertyAccess::MultipleValues || ResY == FPropertyAccess::MultipleValues )
	{
		return FPropertyAccess::MultipleValues;
	}
	else if( ResR == FPropertyAccess::Fail || ResP == FPropertyAccess::Fail || ResY == FPropertyAccess::Fail )
	{
		return FPropertyAccess::Fail;
	}
	else
	{
		return FPropertyAccess::Success;
	}
}

FPropertyAccess::Result FPropertyHandleRotator::SetValue( const FRotator& NewValue, EPropertyValueSetFlags::Type Flags )
{
	// To set the value from the rotator we set each child. 
	FPropertyAccess::Result ResR = RollValue->SetValue( NewValue.Roll, Flags );
	FPropertyAccess::Result ResP = PitchValue->SetValue( NewValue.Pitch, Flags );
	FPropertyAccess::Result ResY = YawValue->SetValue( NewValue.Yaw, Flags );

	if( ResR == FPropertyAccess::Fail || ResP == FPropertyAccess::Fail || ResY == FPropertyAccess::Fail )
	{
		return FPropertyAccess::Fail;
	}
	else
	{
		return FPropertyAccess::Success;
	}
}

FPropertyAccess::Result FPropertyHandleRotator::SetRoll( float InRoll, EPropertyValueSetFlags::Type Flags )
{
	FPropertyAccess::Result Res = RollValue->SetValue( InRoll, Flags );
	return Res;
}

FPropertyAccess::Result FPropertyHandleRotator::SetPitch( float InPitch, EPropertyValueSetFlags::Type Flags )
{
	FPropertyAccess::Result Res = PitchValue->SetValue( InPitch, Flags );
	return Res;
}

FPropertyAccess::Result FPropertyHandleRotator::SetYaw( float InYaw, EPropertyValueSetFlags::Type Flags )
{
	FPropertyAccess::Result Res = YawValue->SetValue( InYaw, Flags );
	return Res;
}


bool FPropertyHandleArray::Supports( TSharedRef<FPropertyNode> PropertyNode )
{
	UProperty* Property = PropertyNode->GetProperty();
	int32 ArrayIndex = PropertyNode->GetArrayIndex();

	// Static array or dynamic array
	return ( ( Property && Property->ArrayDim != 1 && ArrayIndex == -1 ) || Cast<const UArrayProperty>(Property) );
}


FPropertyAccess::Result FPropertyHandleArray::AddItem()
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;
	if( IsEditable() )
	{
		Implementation->AddChild();
		Result = FPropertyAccess::Success;
	}

	return Result;
}

FPropertyAccess::Result FPropertyHandleArray::EmptyArray()
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;
	if( IsEditable() )
	{
		Implementation->ClearChildren();
		Result = FPropertyAccess::Success;
	}

	return Result;
}

FPropertyAccess::Result FPropertyHandleArray::Insert( int32 Index )
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;
	if( IsEditable() && Index < Implementation->GetNumChildren()  )
	{
		Implementation->InsertChild( Index );
		Result = FPropertyAccess::Success;
	}

	return Result;
}

FPropertyAccess::Result FPropertyHandleArray::DuplicateItem( int32 Index )
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;
	if( IsEditable() && Index < Implementation->GetNumChildren() )
	{
		Implementation->DuplicateChild( Index );
		Result = FPropertyAccess::Success;
	}

	return Result;
}

FPropertyAccess::Result FPropertyHandleArray::DeleteItem( int32 Index )
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;
	if( IsEditable() && Index < Implementation->GetNumChildren() )
	{
		Implementation->DeleteChild( Index );
		Result = FPropertyAccess::Success;
	}

	return Result;
}

FPropertyAccess::Result FPropertyHandleArray::SwapItems(int32 FirstIndex, int32 SecondIndex)
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;
	if (IsEditable() && FirstIndex >= 0 && SecondIndex >= 0 && FirstIndex < Implementation->GetNumChildren() && SecondIndex < Implementation->GetNumChildren())
	{
		Implementation->SwapChildren(FirstIndex, SecondIndex);
		Result = FPropertyAccess::Success;
	}

	return Result;
}

FPropertyAccess::Result FPropertyHandleArray::GetNumElements( uint32 &OutNumItems ) const
{
	OutNumItems = Implementation->GetNumChildren();
	return FPropertyAccess::Success;
}

void FPropertyHandleArray::SetOnNumElementsChanged( FSimpleDelegate& OnChildrenChanged )
{
	Implementation->SetOnRebuildChildren( OnChildrenChanged );
}

TSharedPtr<IPropertyHandleArray> FPropertyHandleArray::AsArray()
{
	return SharedThis(this);
}

TSharedRef<IPropertyHandle> FPropertyHandleArray::GetElement( int32 Index ) const
{
	TSharedPtr<FPropertyNode> PropertyNode = Implementation->GetChildNode( Index );
	return PropertyEditorHelpers::GetPropertyHandle( PropertyNode.ToSharedRef(), Implementation->GetNotifyHook(), Implementation->GetPropertyUtilities() ).ToSharedRef();
}

FPropertyAccess::Result FPropertyHandleArray::MoveElementTo(int32 OriginalIndex, int32 NewIndex)
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;
	if (IsEditable() && OriginalIndex >= 0 && NewIndex >= 0)
	{
		Implementation->MoveElementTo(OriginalIndex, NewIndex);
		Result = FPropertyAccess::Success;
	}

	return Result;
}

bool FPropertyHandleArray::IsEditable() const
{
	// Property is editable if its a non-const dynamic array
	return Implementation->HasValidPropertyNode() && !Implementation->IsEditConst() && Implementation->IsPropertyTypeOf(UArrayProperty::StaticClass());
}

// Localized Text
bool FPropertyHandleText::Supports(TSharedRef<FPropertyNode> PropertyNode)
{
	UProperty* Property = PropertyNode->GetProperty();

	if ( Property == nullptr )
	{
		return false;
	}

	// Supported if the property is a text property only
	return Property->IsA(UTextProperty::StaticClass());
}

FPropertyAccess::Result FPropertyHandleText::GetValue(FText& OutValue) const
{
	return Implementation->GetValueAsText(OutValue);
}

FPropertyAccess::Result FPropertyHandleText::SetValue(const FText& NewValue, EPropertyValueSetFlags::Type Flags)
{
	FString StringValue;
	FTextStringHelper::WriteToString(StringValue, NewValue);
	return Implementation->ImportText(StringValue, Flags);
}

FPropertyAccess::Result FPropertyHandleText::SetValue(const FString& NewValue, EPropertyValueSetFlags::Type Flags)
{
	return SetValue(FText::FromString(NewValue), Flags);
}

// Sets

bool FPropertyHandleSet::Supports(TSharedRef<FPropertyNode> PropertyNode)
{
	UProperty* Property = PropertyNode->GetProperty();

	return Cast<const USetProperty>(Property) != nullptr;
}

bool FPropertyHandleSet::HasDefaultElement()
{
	TSharedPtr<FPropertyNode> PropNode = Implementation->GetPropertyNode();
	
	if (PropNode.IsValid())
	{
		TArray<FObjectBaseAddress> Addresses;
		Implementation->GetObjectsToModify(Addresses, PropNode.Get());

		if (Addresses.Num() > 0)
		{
			USetProperty* SetProperty = CastChecked<USetProperty>(PropNode->GetProperty());
			FScriptSetHelper SetHelper(SetProperty, PropNode->GetValueBaseAddress((uint8*)Addresses[0].Object));

			FDefaultConstructedPropertyElement DefaultElement(SetHelper.ElementProp);

			return SetHelper.FindElementIndex(DefaultElement.GetObjAddress()) != INDEX_NONE;
		}
	}

	return false;
}

FPropertyAccess::Result FPropertyHandleSet::AddItem()
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;
	if (IsEditable())
	{
		if (!HasDefaultElement())
		{
			Implementation->AddChild();
			Result = FPropertyAccess::Success;
		}
		else
		{
			Implementation->ShowInvalidOperationError(LOCTEXT("DuplicateSetElement_Add", "Cannot add a new element to the set while an element with the default value exists"));
		}
	}

	return Result;
}

FPropertyAccess::Result FPropertyHandleSet::Empty()
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;
	if (IsEditable())
	{
		Implementation->ClearChildren();
		Result = FPropertyAccess::Success;
	}

	return Result;
}

FPropertyAccess::Result FPropertyHandleSet::DeleteItem(int32 Index)
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;
	TSharedPtr<FPropertyNode> ItemNode;

	if (IsEditable() && Implementation->GetChildNode(Index, ItemNode))
	{
		Implementation->DeleteChild(ItemNode);
		Result = FPropertyAccess::Success;
	}

	return Result;
}

FPropertyAccess::Result FPropertyHandleSet::GetNumElements(uint32& OutNumChildren)
{
	OutNumChildren = Implementation->GetNumChildren();
	return FPropertyAccess::Success;
}

void FPropertyHandleSet::SetOnNumElementsChanged(FSimpleDelegate& OnChildrenChanged)
{
	Implementation->SetOnRebuildChildren(OnChildrenChanged);
}

TSharedPtr<IPropertyHandleSet> FPropertyHandleSet::AsSet()
{
	return SharedThis(this);
}

bool FPropertyHandleSet::IsEditable() const
{
	// Property is editable if its a non-const dynamic array
	return Implementation->HasValidPropertyNode() && !Implementation->IsEditConst() && Implementation->IsPropertyTypeOf(USetProperty::StaticClass());
}

// Maps

bool FPropertyHandleMap::Supports(TSharedRef<FPropertyNode> PropertyNode)
{
	UProperty* Property = PropertyNode->GetProperty();

	return Cast<const UMapProperty>(Property) != nullptr;
}

bool FPropertyHandleMap::HasDefaultKey()
{
	TSharedPtr<FPropertyNode> PropNode = Implementation->GetPropertyNode();

	if (PropNode.IsValid())
	{
		TArray<FObjectBaseAddress> Addresses;
		Implementation->GetObjectsToModify(Addresses, PropNode.Get());

		if (Addresses.Num() > 0)
		{
			UMapProperty* MapProperty = CastChecked<UMapProperty>(PropNode->GetProperty());
			FScriptMapHelper MapHelper(MapProperty, PropNode->GetValueBaseAddress((uint8*)Addresses[0].Object));

			FDefaultConstructedPropertyElement DefaultKey(MapHelper.KeyProp);

			return MapHelper.FindMapIndexWithKey(DefaultKey.GetObjAddress()) != INDEX_NONE;
		}
	}

	return false;
}

FPropertyAccess::Result FPropertyHandleMap::AddItem()
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;
	if (IsEditable())
	{
		if ( !HasDefaultKey() )
		{
			Implementation->AddChild();
			Result = FPropertyAccess::Success;
		}
		else
		{
			Implementation->ShowInvalidOperationError(LOCTEXT("DuplicateMapKey_Add", "Cannot add a new key to the map while a key with the default value exists"));
		}
	}

	return Result;
}

FPropertyAccess::Result FPropertyHandleMap::Empty()
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;
	if (IsEditable())
	{
		Implementation->ClearChildren();
		Result = FPropertyAccess::Success;
	}

	return Result;
}

FPropertyAccess::Result FPropertyHandleMap::DeleteItem(int32 Index)
{
	FPropertyAccess::Result Result = FPropertyAccess::Fail;
	TSharedPtr<FPropertyNode> ItemNode;

	if (IsEditable() && Implementation->GetChildNode(Index, ItemNode))
	{
		Implementation->DeleteChild(ItemNode);
		Result = FPropertyAccess::Success;
	}

	return Result;
}

FPropertyAccess::Result FPropertyHandleMap::GetNumElements(uint32& OutNumChildren)
{
	OutNumChildren = Implementation->GetNumChildren();
	return FPropertyAccess::Success;
}

void FPropertyHandleMap::SetOnNumElementsChanged(FSimpleDelegate& OnChildrenChanged)
{
	Implementation->SetOnRebuildChildren(OnChildrenChanged);
}

TSharedPtr<IPropertyHandleMap> FPropertyHandleMap::AsMap()
{
	return SharedThis(this);
}

bool FPropertyHandleMap::IsEditable() const
{
	// Property is editable if its a non-const dynamic array
	return Implementation->HasValidPropertyNode() && !Implementation->IsEditConst() && Implementation->IsPropertyTypeOf(UMapProperty::StaticClass());
}

#undef LOCTEXT_NAMESPACE