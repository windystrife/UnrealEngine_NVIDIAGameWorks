// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "PropertyPath.h"

class IPropertyHandle;
enum class ESequencerKeyMode;

/**
 * Parameters for determining if a property can be keyed.
 */
struct SEQUENCER_API FCanKeyPropertyParams
{
	/**
	 * Creates new can key property parameters.
	 * @param InObjectClass the class of the object which has the property to be keyed.
	 * @param InPropertyPath path get from the root object to the property to be keyed.
	 */
	FCanKeyPropertyParams(UClass* InObjectClass, const FPropertyPath& InPropertyPath);

	/**
	* Creates new can key property parameters.
	* @param InObjectClass the class of the object which has the property to be keyed.
	* @param InPropertyHandle a handle to the property to be keyed.
	*/
	FCanKeyPropertyParams(UClass* InObjectClass, const IPropertyHandle& InPropertyHandle);

	const UStruct* FindPropertyContainer(const UProperty* ForProperty) const;

	/** The class of the object which has the property to be keyed. */
	const UClass* ObjectClass;

	/** A path of properties to get from the root object to the property to be keyed. */
	FPropertyPath PropertyPath;
};

/**
 * Parameters for keying a property.
 */
struct SEQUENCER_API FKeyPropertyParams
{
	/**
	* Creates new key property parameters for a manually triggered property change.
	* @param InObjectsToKey an array of the objects who's property will be keyed.
	* @param InPropertyPath path get from the root object to the property to be keyed.
	*/
	FKeyPropertyParams(TArray<UObject*> InObjectsToKey, const FPropertyPath& InPropertyPath, ESequencerKeyMode InKeyMode);

	/**
	* Creates new key property parameters from an actual property change notification with a property handle.
	* @param InObjectsToKey an array of the objects who's property will be keyed.
	* @param InPropertyHandle a handle to the property to be keyed.
	*/
	FKeyPropertyParams(TArray<UObject*> InObjectsToKey, const IPropertyHandle& InPropertyHandle, ESequencerKeyMode InKeyMode);

	/** An array of the objects who's property will be keyed. */
	const TArray<UObject*> ObjectsToKey;

	/** A path of properties to get from the root object to the property to be keyed. */
	FPropertyPath PropertyPath;

	/** Keyframing params */
	const ESequencerKeyMode KeyMode;
};

/**
 * Parameters for the property changed callback.
 */
class SEQUENCER_API FPropertyChangedParams
{
public:
	FPropertyChangedParams(TArray<UObject*> InObjectsThatChanged, const FPropertyPath& InPropertyPath, FName InStructPropertyNameToKey, ESequencerKeyMode InKeyMode);

	/**
	 * Gets the value of the property that changed.
	 */
	template<typename ValueType>
	ValueType GetPropertyValue() const
	{
		void* ContainerPtr = ObjectsThatChanged[0];
		for (int32 i = 0; i < PropertyPath.GetNumProperties(); i++)
		{
			const FPropertyInfo& PropertyInfo = PropertyPath.GetPropertyInfo(i);
			if (UProperty* Property = PropertyInfo.Property.Get())
			{
				int32 ArrayIndex = FMath::Max(0, PropertyInfo.ArrayIndex);
				if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property))
				{
					FScriptArrayHelper ParentArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(ContainerPtr));
					if (!ParentArrayHelper.IsValidIndex(ArrayIndex))
					{
						return ValueType();
					}
					ContainerPtr = ParentArrayHelper.GetRawPtr(ArrayIndex);
				}
				else if (ArrayIndex >= 0 && ArrayIndex < Property->ArrayDim)
				{
					ContainerPtr = Property->ContainerPtrToValuePtr<ValueType>(ContainerPtr, ArrayIndex);
				}
			}
		}

		return GetPropertyValueImpl<ValueType>(ContainerPtr, PropertyPath.GetLeafMostProperty());
	}

	/** Gets the property path as a period seperated string of property names. */
	FString GetPropertyPathString() const;

	/** An array of the objects that changed. */
	const TArray<UObject*> ObjectsThatChanged;

	/** A path of properties to get from the root object to the property to be keyed. */
	FPropertyPath PropertyPath;

	/** Represents the FName of an inner property which should be keyed for a struct property.  If all inner 
	properties should be keyed, this will be FName::None. */
	const FName StructPropertyNameToKey;

	/** Keyframing params */
	const ESequencerKeyMode KeyMode;

private:

	template<typename ValueType>
	static ValueType GetPropertyValueImpl(void* Data, const FPropertyInfo& Info)
	{
		return *((ValueType*)Data);
	}
};

template<> SEQUENCER_API bool FPropertyChangedParams::GetPropertyValueImpl<bool>(void* Data, const FPropertyInfo& Info);