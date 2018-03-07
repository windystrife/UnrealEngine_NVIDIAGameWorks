// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "ItemPropertyNode.h"
#include "Misc/ConfigCacheIni.h"
#include "EditorStyleSettings.h"
#include "ObjectPropertyNode.h"
#include "PropertyEditorHelpers.h"

#define LOCTEXT_NAMESPACE "ItemPropertyNode"

FItemPropertyNode::FItemPropertyNode(void)
	: FPropertyNode()
{
	bCanDisplayFavorite = false;
}

FItemPropertyNode::~FItemPropertyNode(void)
{

}

/**
 * Calculates the memory address for the data associated with this item's property.  This is typically the value of a UProperty or a UObject address.
 *
 * @param	StartAddress	the location to use as the starting point for the calculation; typically the address of the object that contains this property.
 *
 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to an FArray*)
 */
uint8* FItemPropertyNode::GetValueBaseAddress( uint8* StartAddress )
{
	UProperty* MyProperty = GetProperty();
	if( MyProperty )
	{
		UArrayProperty* OuterArrayProp = Cast<UArrayProperty>(MyProperty->GetOuter());
		USetProperty* OuterSetProp = Cast<USetProperty>(MyProperty->GetOuter());
		UMapProperty* OuterMapProp = Cast<UMapProperty>(MyProperty->GetOuter());

		uint8* ValueBaseAddress = ParentNode->GetValueBaseAddress(StartAddress);

		if ( OuterArrayProp != NULL )
		{
			FScriptArrayHelper ArrayHelper(OuterArrayProp, ValueBaseAddress);
			if ( ValueBaseAddress != NULL && ArrayIndex < ArrayHelper.Num() )
			{
				return ArrayHelper.GetRawPtr() + ArrayOffset;
			}

			return NULL;
		}
		else if (OuterSetProp != NULL)
		{
			FScriptSetHelper SetHelper(OuterSetProp, ValueBaseAddress);
			if (ValueBaseAddress != NULL && SetHelper.IsValidIndex(ArrayIndex))
			{
				return SetHelper.GetElementPtr(ArrayIndex);
			}

			return NULL;
		}
		else if (OuterMapProp != NULL)
		{
			FScriptMapHelper MapHelper(OuterMapProp, ValueBaseAddress);
			if (ValueBaseAddress != NULL && MapHelper.IsValidIndex(ArrayIndex))
			{
				uint8* PairPtr = MapHelper.GetPairPtr(ArrayIndex);

				return MyProperty->ContainerPtrToValuePtr<uint8>(PairPtr);
			}

			return NULL;
		}
		else
		{
			uint8* ValueAddress = ParentNode->GetValueAddress(StartAddress);
			if (ValueAddress != NULL && ParentNode->GetProperty() != MyProperty)
			{
				// if this is not a fixed size array (in which the parent property and this property are the same), we need to offset from the property (otherwise, the parent already did that for us)
				ValueAddress = Property->ContainerPtrToValuePtr<uint8>(ValueAddress);
			}
			if ( ValueAddress != NULL )
			{
				ValueAddress += ArrayOffset;
			}
			return ValueAddress;
		}
	}

	return NULL;
}

/**
 * Calculates the memory address for the data associated with this item's value.  For most properties, identical to GetValueBaseAddress.  For items corresponding
 * to dynamic array elements, the pointer returned will be the location for that element's data. 
 *
 * @param	StartAddress	the location to use as the starting point for the calculation; typically the address of the object that contains this property.
 *
 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to whatever type is the Inner for the dynamic array)
 */
uint8* FItemPropertyNode::GetValueAddress( uint8* StartAddress )
{
	uint8* Result = GetValueBaseAddress(StartAddress);

	UProperty* MyProperty = GetProperty();

	UArrayProperty* ArrayProperty = Cast<UArrayProperty>(MyProperty);
	USetProperty* SetProperty = Cast<USetProperty>(MyProperty);
	UMapProperty* MapProperty = Cast<UMapProperty>(MyProperty);

	if( Result && ArrayProperty)
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty,Result);
		Result = ArrayHelper.GetRawPtr();
	}

	return Result;
}

/**
 * Overridden function for special setup
 */
void FItemPropertyNode::InitExpansionFlags (void)
{
	
	UProperty* MyProperty = GetProperty();

	FReadAddressList Addresses;

	bool bExpandableType = Cast<UStructProperty>(MyProperty) 
		|| ( ( Cast<UArrayProperty>(MyProperty) || Cast<USetProperty>(MyProperty) || Cast<UMapProperty>(MyProperty) ) && GetReadAddress(false,Addresses) );

	if(	bExpandableType
		|| HasNodeFlags(EPropertyNodeFlags::EditInlineNew)
		|| HasNodeFlags(EPropertyNodeFlags::ShowInnerObjectProperties)
		||	( MyProperty->ArrayDim > 1 && ArrayIndex == -1 ) )
	{
		SetNodeFlags(EPropertyNodeFlags::CanBeExpanded, true);
	}
}
/**
 * Overridden function for Creating Child Nodes
 */
void FItemPropertyNode::InitChildNodes()
{
	//NOTE - this is only turned off as to not invalidate child object nodes.
	UProperty* MyProperty = GetProperty();
	UStructProperty* StructProperty = Cast<UStructProperty>(MyProperty);
	UArrayProperty* ArrayProperty = Cast<UArrayProperty>(MyProperty);
	USetProperty* SetProperty = Cast<USetProperty>(MyProperty);
	UMapProperty* MapProperty = Cast<UMapProperty>(MyProperty);
	UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(MyProperty);

	const bool bShouldShowHiddenProperties = !!HasNodeFlags(EPropertyNodeFlags::ShouldShowHiddenProperties);
	const bool bShouldShowDisableEditOnInstance = !!HasNodeFlags(EPropertyNodeFlags::ShouldShowDisableEditOnInstance);

	if( MyProperty->ArrayDim > 1 && ArrayIndex == -1 )
	{
		// Do not add array children which are defined by an enum but the enum at the array index is hidden
		// This only applies to static arrays
		static const FName NAME_ArraySizeEnum("ArraySizeEnum");
		UEnum* ArraySizeEnum = NULL; 
		if (MyProperty->HasMetaData(NAME_ArraySizeEnum))
		{
			ArraySizeEnum	= FindObject<UEnum>(NULL, *MyProperty->GetMetaData(NAME_ArraySizeEnum));
		}

		// Expand array.
		for( int32 Index = 0 ; Index < MyProperty->ArrayDim ; Index++ )
		{
			bool bShouldBeHidden = false;
			if( ArraySizeEnum )
			{
				// The enum at this array index is hidden
				bShouldBeHidden = ArraySizeEnum->HasMetaData(TEXT("Hidden"), Index );
			}

			if( !bShouldBeHidden )
			{
				TSharedPtr<FItemPropertyNode> NewItemNode( new FItemPropertyNode);
				FPropertyNodeInitParams InitParams;
				InitParams.ParentNode = SharedThis(this);
				InitParams.Property = MyProperty;
				InitParams.ArrayOffset = Index*MyProperty->ElementSize;
				InitParams.ArrayIndex = Index;
				InitParams.bAllowChildren = true;
				InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;
				InitParams.bCreateDisableEditOnInstanceNodes = bShouldShowDisableEditOnInstance;

				NewItemNode->InitNode( InitParams );
				AddChildNode(NewItemNode);
			}
		}
	}
	else if( ArrayProperty )
	{
		void* Array = NULL;
		FReadAddressList Addresses;
		if ( GetReadAddress(!!HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses ) )
		{
			Array = Addresses.GetAddress(0);
		}

		if( Array )
		{
			for( int32 Index = 0 ; Index < FScriptArrayHelper::Num(Array) ; Index++ )
			{
				TSharedPtr<FItemPropertyNode> NewItemNode( new FItemPropertyNode );

				FPropertyNodeInitParams InitParams;
				InitParams.ParentNode = SharedThis(this);
				InitParams.Property = ArrayProperty->Inner;
				InitParams.ArrayOffset = Index*ArrayProperty->Inner->ElementSize;
				InitParams.ArrayIndex = Index;
				InitParams.bAllowChildren = true;
				InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;
				InitParams.bCreateDisableEditOnInstanceNodes = bShouldShowDisableEditOnInstance;

				NewItemNode->InitNode( InitParams );
				AddChildNode(NewItemNode);
			}
		}
	}
	else if ( SetProperty )
	{
		void* Set = NULL;
		FReadAddressList Addresses;
		if (GetReadAddress(!!HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses))
		{
			Set = Addresses.GetAddress(0);
		}

		if (Set)
		{
			FScriptSetHelper SetHelper(SetProperty, Set);

			for (int32 TrueIndex = 0, ItemsLeft = SetHelper.Num(); ItemsLeft > 0; ++TrueIndex)
			{
				if (SetHelper.IsValidIndex(TrueIndex))
				{
					--ItemsLeft;

					TSharedPtr<FItemPropertyNode> NewItemNode(new FItemPropertyNode);

					FPropertyNodeInitParams InitParams;
					InitParams.ParentNode = SharedThis(this);
					InitParams.Property = SetProperty->ElementProp;
					InitParams.ArrayIndex = TrueIndex;
					InitParams.bAllowChildren = true;
					InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;
					InitParams.bCreateDisableEditOnInstanceNodes = bShouldShowDisableEditOnInstance;

					NewItemNode->InitNode(InitParams);
					AddChildNode(NewItemNode);
				}
			}
		}
	}
	else if ( MapProperty )
	{
		void* Map = NULL;
		FReadAddressList Addresses;
		if (GetReadAddress(!!HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses))
		{
			Map = Addresses.GetAddress(0);
		}

		if ( Map )
		{
			FScriptMapHelper MapHelper(MapProperty, Map);

			for (int32 TrueIndex = 0, ItemsLeft = MapHelper.Num(); ItemsLeft > 0; ++TrueIndex)
			{
				if ( MapHelper.IsValidIndex(TrueIndex) )
				{
					--ItemsLeft;

					// Construct the key node first
					TSharedPtr<FPropertyNode> KeyNode(new FItemPropertyNode());
					
					FPropertyNodeInitParams InitParams;
					InitParams.ParentNode = SharedThis(this);	// Key Node needs to point to this node to ensure its data is set correctly
					InitParams.Property = MapHelper.KeyProp;
					InitParams.ArrayIndex = TrueIndex;
					InitParams.ArrayOffset = 0;
					InitParams.bAllowChildren = true;
					InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;
					InitParams.bCreateDisableEditOnInstanceNodes = bShouldShowDisableEditOnInstance;

					KeyNode->InitNode(InitParams);
					// Not adding the key node as a child, otherwise it'll show up in the wrong spot.

					// Then the value node
					TSharedPtr<FPropertyNode> ValueNode(new FItemPropertyNode());
					
					// Reuse the init params
					InitParams.ParentNode = SharedThis(this);
					InitParams.Property = MapHelper.ValueProp;

					ValueNode->InitNode(InitParams);
					AddChildNode(ValueNode);

					FPropertyNode::SetupKeyValueNodePair(KeyNode, ValueNode);
				}
			}
		}
	}
	else if( StructProperty )
	{
		// Expand struct.
		for( TFieldIterator<UProperty> It(StructProperty->Struct); It; ++It )
		{
			UProperty* StructMember = *It;
			static const FName Name_InlineEditConditionToggle("InlineEditConditionToggle");
			const bool bOnlyShowAsInlineEditCondition = StructMember->HasMetaData(Name_InlineEditConditionToggle);
			const bool bShowIfEditableProperty = StructMember->HasAnyPropertyFlags(CPF_Edit);
			const bool bShowIfDisableEditOnInstance = !StructMember->HasAnyPropertyFlags(CPF_DisableEditOnInstance) || bShouldShowDisableEditOnInstance;
			if (bShouldShowHiddenProperties || (bShowIfEditableProperty && !bOnlyShowAsInlineEditCondition && bShowIfDisableEditOnInstance))
			{
				TSharedPtr<FItemPropertyNode> NewItemNode( new FItemPropertyNode );//;//CreatePropertyItem(StructMember,INDEX_NONE,this);
		
				FPropertyNodeInitParams InitParams;
				InitParams.ParentNode = SharedThis(this);
				InitParams.Property = StructMember;
				InitParams.ArrayOffset = 0;
				InitParams.ArrayIndex = INDEX_NONE;
				InitParams.bAllowChildren = true;
				InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;
				InitParams.bCreateDisableEditOnInstanceNodes = bShouldShowDisableEditOnInstance;

				NewItemNode->InitNode( InitParams );
				AddChildNode(NewItemNode);

				if ( FPropertySettings::Get().ExpandDistributions() == false)
				{
					// auto-expand distribution structs
					if ( Cast<UObjectProperty>(StructMember) || Cast<UWeakObjectProperty>(StructMember) || Cast<ULazyObjectProperty>(StructMember) || Cast<USoftObjectProperty>(StructMember) )
					{
						const FName StructName = StructProperty->Struct->GetFName();
						if (StructName == NAME_RawDistributionFloat || StructName == NAME_RawDistributionVector)
						{
							NewItemNode->SetNodeFlags(EPropertyNodeFlags::Expanded, true);
						}
					}
				}
			}
		}
	}
	else if( ObjectProperty )
	{
		uint8* ReadValue = NULL;

		FReadAddressList ReadAddresses;
		if( GetReadAddress(!!HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses, false ) )
		{
			// We've got some addresses, and we know they're all NULL or non-NULL.
			// Have a peek at the first one, and only build an objects node if we've got addresses.
			if( UObject* Obj = (ReadAddresses.Num() > 0) ? ObjectProperty->GetObjectPropertyValue(ReadAddresses.GetAddress(0)) : nullptr )
			{
				//verify it's not above in the hierarchy somewhere
				FObjectPropertyNode* ParentObjectNode = FindObjectItemParent();
				while (ParentObjectNode)
				{
					for ( TPropObjectIterator Itor( ParentObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
					{
						if (*Itor == Obj)
						{
							SetNodeFlags(EPropertyNodeFlags::NoChildrenDueToCircularReference, true);
							//stop the circular loop!!!
							return;
						}
					}
					FPropertyNode* UpwardTravesalNode = ParentObjectNode->GetParentNode();
					ParentObjectNode = (UpwardTravesalNode==NULL) ? NULL : UpwardTravesalNode->FindObjectItemParent();
				}

			
				TSharedPtr<FObjectPropertyNode> NewObjectNode( new FObjectPropertyNode );
				for ( int32 AddressIndex = 0 ; AddressIndex < ReadAddresses.Num() ; ++AddressIndex )
				{
					NewObjectNode->AddObject( ObjectProperty->GetObjectPropertyValue(ReadAddresses.GetAddress(AddressIndex) ) );
				}

				FPropertyNodeInitParams InitParams;
				InitParams.ParentNode = SharedThis(this);
				InitParams.Property = MyProperty;
				InitParams.ArrayOffset = 0;
				InitParams.ArrayIndex = INDEX_NONE;
				InitParams.bAllowChildren = true;
				InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;
				InitParams.bCreateDisableEditOnInstanceNodes = bShouldShowDisableEditOnInstance;

				NewObjectNode->InitNode( InitParams );
				AddChildNode(NewObjectNode);
			}
		}
	}
}

void FItemPropertyNode::SetFavorite(bool FavoriteValue)
{
	const FObjectPropertyNode* CurrentObjectNode = FindObjectItemParent();
	if (CurrentObjectNode == nullptr || CurrentObjectNode->GetNumObjects() <= 0)
		return;
	const UClass *ObjectClass = CurrentObjectNode->GetObjectBaseClass();
	if (ObjectClass == nullptr)
		return;
	FString FullPropertyPath = ObjectClass->GetName() + TEXT(":") + PropertyPath;
	if (FavoriteValue)
	{
		GConfig->SetBool(TEXT("DetailPropertyFavorites"), *FullPropertyPath, FavoriteValue, GEditorPerProjectIni);
	}
	else
	{
		GConfig->RemoveKey(TEXT("DetailPropertyFavorites"), *FullPropertyPath, GEditorPerProjectIni);
	}
}

bool FItemPropertyNode::IsFavorite() const
{
	const FObjectPropertyNode* CurrentObjectNode = FindObjectItemParent();
	if (CurrentObjectNode == nullptr ||CurrentObjectNode->GetNumObjects() <= 0)
		return false;
	const UClass *ObjectClass = CurrentObjectNode->GetObjectBaseClass();
	if (ObjectClass == nullptr)
		return false;
	FString FullPropertyPath = ObjectClass->GetName() + TEXT(":") + PropertyPath;
	bool FavoritesPropertyValue = false;
	if (!GConfig->GetBool(TEXT("DetailPropertyFavorites"), *FullPropertyPath, FavoritesPropertyValue, GEditorPerProjectIni))
		return false;
	return FavoritesPropertyValue;
}

/**
* Set the permission to display the favorite icon
*/
void FItemPropertyNode::SetCanDisplayFavorite(bool CanDisplayFavoriteIcon)
{
	bCanDisplayFavorite = CanDisplayFavoriteIcon;
}

/**
* Set the permission to display the favorite icon
*/
bool FItemPropertyNode::CanDisplayFavorite() const
{
	return bCanDisplayFavorite;
}

void FItemPropertyNode::SetDisplayNameOverride( const FText& InDisplayNameOverride )
{
	DisplayNameOverride = InDisplayNameOverride;
}

FText FItemPropertyNode::GetDisplayName() const
{
	FText FinalDisplayName;

	if( !DisplayNameOverride.IsEmpty() )
	{
		FinalDisplayName = DisplayNameOverride;
	}
	else 
	{
		const UProperty* PropertyPtr = GetProperty();
		if( GetArrayIndex()==-1 && PropertyPtr != NULL  )
		{
			// This item is not a member of an array, get a traditional display name
			if ( FPropertySettings::Get().ShowFriendlyPropertyNames() )
			{
				//We are in "readable display name mode"../ Make a nice name
				FinalDisplayName = PropertyPtr->GetDisplayNameText();
				if ( FinalDisplayName.IsEmpty() )
				{
					FString PropertyDisplayName;
					bool bIsBoolProperty = Cast<const UBoolProperty>(PropertyPtr) != NULL;
					const UStructProperty* ParentStructProperty = Cast<const UStructProperty>(ParentNode->GetProperty());
					if( ParentStructProperty && ParentStructProperty->Struct->GetFName() == NAME_Rotator )
					{
						if( Property->GetFName() == "Roll" )
						{
							PropertyDisplayName = TEXT("X");
						}
						else if( Property->GetFName() == "Pitch" )
						{
							PropertyDisplayName = TEXT("Y");
						}
						else if( Property->GetFName() == "Yaw" )
						{
							PropertyDisplayName = TEXT("Z");
						}
						else
						{
							check(0);
						}
					}
					else
					{
						PropertyDisplayName = Property->GetName();
					}
					if( GetDefault<UEditorStyleSettings>()->bShowFriendlyNames )
					{
						PropertyDisplayName = FName::NameToDisplayString( PropertyDisplayName, bIsBoolProperty );
					}

					FinalDisplayName = FText::FromString( PropertyDisplayName );
				}
			}
			else
			{
				FinalDisplayName =  FText::FromString( PropertyPtr->GetName() );
			}
		}
		else
		{
			// Get the ArraySizeEnum class from meta data.
			static const FName NAME_ArraySizeEnum("ArraySizeEnum");
			UEnum* ArraySizeEnum = NULL; 
			if (PropertyPtr && PropertyPtr->HasMetaData(NAME_ArraySizeEnum))
			{
				ArraySizeEnum	= FindObject<UEnum>(NULL, *Property->GetMetaData(NAME_ArraySizeEnum));
			}
			
			// Sets and maps do not have a display index.
			UProperty* ParentProperty = ParentNode->GetProperty();
			if (Cast<USetProperty>(ParentProperty) == nullptr &&  Cast<UMapProperty>(ParentProperty) == nullptr)
			{
				// This item is a member of an array, its display name is its index 
				if (PropertyPtr == NULL || ArraySizeEnum == NULL)
				{
					FinalDisplayName = FText::AsNumber(GetArrayIndex());
				}
				else
				{
					FinalDisplayName = ArraySizeEnum->GetDisplayNameTextByIndex(GetArrayIndex());
				}
			}
			// Maps should have display names that reflect the key and value types
			else if (PropertyPtr != nullptr && Cast<UMapProperty>(ParentNode->GetProperty()) != nullptr)
			{
				FText FormatText = GetPropertyKeyNode().IsValid()
					? LOCTEXT("MapValueDisplayFormat", "Value ({0})")
					: LOCTEXT("MapKeyDisplayFormat", "Key ({0})");

				FString TypeName;

				if (const UStructProperty* StructProp = Cast<UStructProperty>(PropertyPtr))
				{
					// For struct props, use the name of the struct itself
					TypeName = StructProp->Struct->GetName();
				}
				else if (const UEnumProperty* EnumProp = Cast<UEnumProperty>(PropertyPtr))
				{
					// For enum props, use the name of the enum
					if (EnumProp->GetEnum() != nullptr)
					{
						TypeName = EnumProp->GetEnum()->GetName();
					}
					else
					{
						TypeName = TEXT("Enum");
					}
				}
				else if(PropertyPtr->IsA<UStrProperty>())
				{
					// For strings, actually return "String" and not "Str"
					TypeName = TEXT("String");
				}
				else
				{
					// For any other property, get the type from the property class
					TypeName = PropertyPtr->GetClass()->GetName();

					int32 EndIndex = TypeName.Find(TEXT("Property"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);

					if (EndIndex != -1)
					{
						TypeName = TypeName.Mid(0, EndIndex);
					}
				}

				if (FPropertySettings::Get().ShowFriendlyPropertyNames())
				{
					TypeName = FName::NameToDisplayString(TypeName, false);
				}

				FinalDisplayName = FText::Format(FormatText, FText::FromString(TypeName));
			}
		}
	}
	
	return FinalDisplayName;
}

void FItemPropertyNode::SetToolTipOverride( const FText& InToolTipOverride )
{
	ToolTipOverride = InToolTipOverride;
}

FText FItemPropertyNode::GetToolTipText() const
{
	if(!ToolTipOverride.IsEmpty())
	{
		return ToolTipOverride;
	}

	return PropertyEditorHelpers::GetToolTipText(GetProperty());
}

#undef LOCTEXT_NAMESPACE