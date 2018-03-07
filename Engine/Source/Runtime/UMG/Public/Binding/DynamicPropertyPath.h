// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "DynamicPropertyPath.generated.h"

/** */
template<typename T>
inline bool IsConcreteTypeCompatibleWithReflectedType(UProperty* Property)
{
	if ( UStructProperty* StructProperty = Cast<UStructProperty>(Property) )
	{
		return StructProperty->Struct == T::StaticStruct();
	}

	return false;
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType<bool>(UProperty* Property)
{
	return Property->GetClass() == UBoolProperty::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType<int8>(UProperty* Property)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Property = EnumProperty->GetUnderlyingProperty();
	}

	return Property->GetClass() == UInt8Property::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType<uint8>(UProperty* Property)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Property = EnumProperty->GetUnderlyingProperty();
	}

	return Property->GetClass() == UByteProperty::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType<int16>(UProperty* Property)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Property = EnumProperty->GetUnderlyingProperty();
	}

	return Property->GetClass() == UInt16Property::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType<uint16>(UProperty* Property)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Property = EnumProperty->GetUnderlyingProperty();
	}

	return Property->GetClass() == UUInt16Property::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType<int32>(UProperty* Property)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Property = EnumProperty->GetUnderlyingProperty();
	}

	return Property->GetClass() == UIntProperty::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType<uint32>(UProperty* Property)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Property = EnumProperty->GetUnderlyingProperty();
	}

	return Property->GetClass() == UUInt32Property::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType<int64>(UProperty* Property)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Property = EnumProperty->GetUnderlyingProperty();
	}

	return Property->GetClass() == UInt64Property::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType<uint64>(UProperty* Property)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Property = EnumProperty->GetUnderlyingProperty();
	}

	return Property->GetClass() == UUInt64Property::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType<float>(UProperty* Property)
{
	return Property->GetClass() == UFloatProperty::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType<double>(UProperty* Property)
{
	return Property->GetClass() == UDoubleProperty::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType<FText>(UProperty* Property)
{
	return Property->GetClass() == UTextProperty::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType<FString>(UProperty* Property)
{
	return Property->GetClass() == UStrProperty::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType<FLinearColor>(UProperty* Property)
{
	static const UScriptStruct* LinearColorStruct = TBaseStructure<FLinearColor>::Get();
	
	if ( UStructProperty* StructProperty = Cast<UStructProperty>(Property) )
	{
		return StructProperty->Struct == LinearColorStruct;
	}

	return false;
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType<UObject*>(UProperty* Property)
{
	if ( UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property) )
	{
		return true;
		//return ObjectProperty->PropertyClass->IsChildOf(T::StaticClass());
	}

	return false;
}

/** A struct used for caching part of a property path.  Don't use this class directly. */
USTRUCT()
struct UMG_API FPropertyPathSegment
{
	GENERATED_USTRUCT_BODY()

public:
	/** Implementation detail, don't use this constructor. */
	FPropertyPathSegment();

	/** Initializes the segment for particular name. */
	FPropertyPathSegment(FString SegmentName);

	/**
	 * Resolves the name on the given Struct.  Caches the resulting property so that future calls can be processed quickly.
	 * @param InStruct the ScriptStruct or Class to look for the property on.
	 */
	UField* Resolve(UStruct* InStruct) const;

public:

	/** The sub-component of the property path, a single value between .'s of the path */
	UPROPERTY()
	FName Name;

	/** The optional array index. */
	UPROPERTY()
	int32 ArrayIndex;

private:

	/** The cached Class or ScriptStruct that was used last to resolve Name to a property. */
	UPROPERTY(Transient)
	mutable UStruct* Struct;

	/**
	 * The cached property on the Struct that this Name resolved to on it last time Resolve was called, if 
	 * the Struct doesn't change, this value is returned to avoid performing another Field lookup.
	 */
	UPROPERTY(Transient)
	mutable UField* Field;
};

/** */
USTRUCT()
struct UMG_API FDynamicPropertyPath
{
	GENERATED_USTRUCT_BODY()

public:

	/** */
	FDynamicPropertyPath();

	/** */
	FDynamicPropertyPath(FString Path);

	/** */
	FDynamicPropertyPath(const TArray<FString>& PropertyChain);

	/** */
	bool IsValid() const { return Segments.Num() > 0; }

	/** */
	template<typename T>
	bool GetValue(UObject* InContainer, T& OutValue) const
	{
		UProperty* OutProperty;
		return GetValue<T>(InContainer, OutValue, OutProperty);
	}

	/** */
	template<typename T>
	bool GetValue(UObject* InContainer, T& OutValue, UProperty*& OutProperty) const
	{
		if ( InContainer && IsValid() )
		{
			return GetValueRecursive(InContainer->GetClass(), InContainer, INDEX_NONE, 0, OutValue, OutProperty);
		}

		return false;
	}

private:

	/**
	 * Evaluates the dynamic property path, and gets the value or calls the function at the end of the evaluation if possible.
	 * Otherwise returns false.
	 */
	template<typename T>
	bool GetValueRecursive(UStruct* InStruct, void* InContainer, int32 ArrayIndex, int32 SegmentIndex, T& OutValue, UProperty*& OutProperty) const
	{
		const FPropertyPathSegment& Segment = Segments[SegmentIndex];

		// Obtain the property info from the given structure definition
		if ( UField* Field = Segment.Resolve(InStruct) )
		{
			if ( UProperty* Property = Cast<UProperty>(Field) )
			{
				if ( SegmentIndex < ( Segments.Num() - 1 ) )
				{
					// Check first to see if this is a simple object (eg. not an array of objects)
					if ( UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property) )
					{
						// If it's an object we need to get the value of the property in the container first before we 
						// can continue, if the object is null we safely stop processing the chain of properties.
						if ( UObject* CurrentObject = ObjectProperty->GetPropertyValue_InContainer(InContainer) )
						{
							return GetValueRecursive(CurrentObject->GetClass(), CurrentObject, ArrayIndex, SegmentIndex + 1, OutValue, OutProperty);
						}
					}
					// Check to see if this is a simple weak object property (eg. not an array of weak objects).
					if ( UWeakObjectProperty* WeakObjectProperty = Cast<UWeakObjectProperty>(Property) )
					{
						FWeakObjectPtr WeakObject = WeakObjectProperty->GetPropertyValue_InContainer(InContainer);

						// If it's an object we need to get the value of the property in the container first before we 
						// can continue, if the object is null we safely stop processing the chain of properties.
						if ( UObject* CurrentObject = WeakObject.Get() )
						{
							return GetValueRecursive(CurrentObject->GetClass(), CurrentObject, ArrayIndex, SegmentIndex + 1, OutValue, OutProperty);
						}
					}
					// Check to see if this is a simple structure (eg. not an array of structures)
					else if ( UStructProperty* StructProp = Cast<UStructProperty>(Property) )
					{
						// Recursively call back into this function with the structure property and container value
						return GetValueRecursive(StructProp->Struct, StructProp->ContainerPtrToValuePtr<void>(InContainer), ArrayIndex, SegmentIndex + 1, OutValue, OutProperty);
					}
					else if ( UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property) )
					{
						// It is an array, now check to see if this is an array of structures
						if ( UStructProperty* ArrayOfStructsProp = Cast<UStructProperty>(ArrayProp->Inner) )
						{
							FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, InContainer);
							if ( ArrayHelper.IsValidIndex(Segment.ArrayIndex) )
							{
								// Recursively call back into this function with the array element and container value
								return GetValueRecursive(ArrayOfStructsProp->Struct, ArrayHelper.GetRawPtr(Segment.ArrayIndex), ArrayIndex, SegmentIndex + 1, OutValue, OutProperty);
							}
						}
						// if it's not an array of structs, maybe it's an array of classes
						//else if ( UObjectProperty* ObjectProperty = Cast<UObjectProperty>(ArrayProp->Inner) )
						{
							//TODO Add support for arrays of objects.
						}
					}
				}
				else
				{
					// We're on the final property in the path, it may be an array property, so check that first.
					if ( UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property) )
					{
						// if it's an array property, we need to see if we parsed an index as part of the segment
						// as a user may have baked the index directly into the property path.
						int32 Index = ArrayIndex != INDEX_NONE ? ArrayIndex : Segment.ArrayIndex;
						ensure(Index != INDEX_NONE);

						// Property is an array property, so make sure we have a valid index specified
						FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, InContainer);
						if ( ArrayHelper.IsValidIndex(Index) )
						{
							// Verify that the cpp type matches a known property type.
							if ( IsConcreteTypeCompatibleWithReflectedType<T>(ArrayProp->Inner) )
							{
								// Ensure that the element sizes are the same, prevents the user from doing something terribly wrong.
								if ( ArrayProp->Inner->ElementSize == sizeof(T) )
								{
									OutValue = *static_cast<T*>( static_cast<void*>( ArrayHelper.GetRawPtr(Index) ) );
									OutProperty = ArrayProp->Inner;
								}
							}
						}
					}
					else
					{
						// Verify that the cpp type matches a known property type.
						if ( IsConcreteTypeCompatibleWithReflectedType<T>(Property) )
						{
							// Ensure that the element sizes are the same, prevents the user from doing something terribly wrong.
							if ( Property->ElementSize == sizeof(T) )
							{
								// Property is a vector property, so access directly
								if ( T* ValuePtr = Property->ContainerPtrToValuePtr<T>(InContainer) )
								{
									OutValue = *ValuePtr;
									OutProperty = Property;
									return true;
								}
							}
						}
					}
				}
			}
			else
			{
				// Only allow functions as the final link in the chain.
				if ( SegmentIndex == ( Segments.Num() - 1 ) )
				{
					UFunction* Function = CastChecked<UFunction>(Field);
					UObject* ContainerObject = static_cast<UObject*>( InContainer );

					// We only support calling functions that return a single value and take no parameters.
					if ( Function->NumParms == 1 )
					{
						// Verify there's a return property.
						if ( UProperty* ReturnProperty = Function->GetReturnProperty() )
						{
							// Verify that the cpp type matches a known property type.
							if ( IsConcreteTypeCompatibleWithReflectedType<T>(ReturnProperty) )
							{
								// Ensure that the element sizes are the same, prevents the user from doing something terribly wrong.
								if ( ReturnProperty->ElementSize == sizeof(T) && !ContainerObject->IsUnreachable() )
								{
									ContainerObject->ProcessEvent(Function, &OutValue);
									return true;
								}
							}
						}
					}
				}
			}
		}

		return false;
	}

private:

	UPROPERTY()
	TArray<FPropertyPathSegment> Segments;
};
