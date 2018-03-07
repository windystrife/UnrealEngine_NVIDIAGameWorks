// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "KeyPropertyParams.h"
#include "PropertyHandle.h"

FPropertyPath PropertyHandleToPropertyPath(const UClass* OwnerClass, const IPropertyHandle& InPropertyHandle)
{
	TArray<FPropertyInfo> PropertyInfo;
	PropertyInfo.Insert(FPropertyInfo(InPropertyHandle.GetProperty(), InPropertyHandle.GetIndexInArray()), 0);

	TSharedPtr<IPropertyHandle> CurrentHandle = InPropertyHandle.GetParentHandle();
	while (CurrentHandle.IsValid() && CurrentHandle->GetProperty() != nullptr)
	{
		// IPropertyHandle chains contain arrays in a manner designed for display in the property editor, e.g.
		// Container.Array.Array[ArrayIndex].StructInner.
		// We need to collapse adjacent array properties as we are looking for Container.Array[ArrayIndex].StructInner
		// to for a well-formed 'property path'
		TSharedPtr<IPropertyHandle> ParentHandle = CurrentHandle->GetParentHandle();
		if (ParentHandle.IsValid() && ParentHandle->GetProperty() != nullptr && ParentHandle->GetProperty()->IsA<UArrayProperty>())
		{
			UArrayProperty* ArrayProperty = CastChecked<UArrayProperty>(ParentHandle->GetProperty());

			PropertyInfo.Insert(FPropertyInfo(ParentHandle->GetProperty(), CurrentHandle->GetIndexInArray()), 0);
			CurrentHandle = ParentHandle->GetParentHandle();
		}
		else
		{
			PropertyInfo.Insert(FPropertyInfo(CurrentHandle->GetProperty(), CurrentHandle->GetIndexInArray()), 0);
			CurrentHandle = CurrentHandle->GetParentHandle();
		}
	}

	FPropertyPath PropertyPath;
	for (const FPropertyInfo& Info : PropertyInfo)
	{
		PropertyPath.AddProperty(Info);
	}
	return PropertyPath;
}

FCanKeyPropertyParams::FCanKeyPropertyParams(UClass* InObjectClass, const FPropertyPath& InPropertyPath)
	: ObjectClass(InObjectClass)
	, PropertyPath(InPropertyPath)
{
}

FCanKeyPropertyParams::FCanKeyPropertyParams(UClass* InObjectClass, const IPropertyHandle& InPropertyHandle)
	: ObjectClass(InObjectClass)
	, PropertyPath(PropertyHandleToPropertyPath(InObjectClass, InPropertyHandle))
{
}

const UStruct* FCanKeyPropertyParams::FindPropertyContainer(const UProperty* ForProperty) const
{
	check(ForProperty);

	bool bFoundProperty = false;
	for (int32 Index = PropertyPath.GetNumProperties() - 1; Index >= 0; --Index)
	{
		UProperty* Property = PropertyPath.GetPropertyInfo(Index).Property.Get();
		if (!bFoundProperty)
		{
			bFoundProperty = Property == ForProperty;
			if (bFoundProperty)
			{
				return ObjectClass;
			}
		}
		else if (const UStructProperty* StructProperty = Cast<const UStructProperty>(Property))
		{
			return StructProperty->Struct;
		}
	}
	return ObjectClass;
}

FKeyPropertyParams::FKeyPropertyParams(TArray<UObject*> InObjectsToKey, const FPropertyPath& InPropertyPath, ESequencerKeyMode InKeyMode)
	: ObjectsToKey(InObjectsToKey)
	, PropertyPath(InPropertyPath)
	, KeyMode(InKeyMode)

{
}

FKeyPropertyParams::FKeyPropertyParams(TArray<UObject*> InObjectsToKey, const IPropertyHandle& InPropertyHandle, ESequencerKeyMode InKeyMode)
	: ObjectsToKey(InObjectsToKey)
	, PropertyPath(PropertyHandleToPropertyPath(InObjectsToKey[0]->GetClass(), InPropertyHandle))
	, KeyMode(InKeyMode)
{
}

FPropertyChangedParams::FPropertyChangedParams(TArray<UObject*> InObjectsThatChanged, const FPropertyPath& InPropertyPath, FName InStructPropertyNameToKey, ESequencerKeyMode InKeyMode)
	: ObjectsThatChanged(InObjectsThatChanged)
	, PropertyPath(InPropertyPath)
	, StructPropertyNameToKey(InStructPropertyNameToKey)
	, KeyMode(InKeyMode)
{
}

template<> bool FPropertyChangedParams::GetPropertyValueImpl<bool>(void* Data, const FPropertyInfo& PropertyInfo)
{
	// Bool property values are stored in a bit field so using a straight cast of the pointer to get their value does not
	// work.  Instead use the actual property to get the correct value.
	if ( const UBoolProperty* BoolProperty = Cast<const UBoolProperty>( PropertyInfo.Property.Get() ) )
	{
		return BoolProperty->GetPropertyValue(Data);
	}
	else
	{
		return *((bool*)Data);
	}
}

FString FPropertyChangedParams::GetPropertyPathString() const
{
	return PropertyPath.ToString(TEXT("."));
}
