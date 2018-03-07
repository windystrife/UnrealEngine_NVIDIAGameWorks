// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Backends/JsonStructDeserializerBackend.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"


/* Internal helpers
 *****************************************************************************/

namespace JsonStructDeserializerBackend
{
	/**
	 * Clears the value of the given property.
	 *
	 * @param Property The property to clear.
	 * @param Outer The property that contains the property to be cleared, if any.
	 * @param Data A pointer to the memory holding the property's data.
	 * @param ArrayIndex The index of the element to clear (if the property is an array).
	 * @return true on success, false otherwise.
	 * @see SetPropertyValue
	 */
	bool ClearPropertyValue( UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex )
	{
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Outer);

		if (ArrayProperty != nullptr)
		{
			if (ArrayProperty->Inner != Property)
			{
				return false;
			}

			FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->template ContainerPtrToValuePtr<void>(Data));
			ArrayIndex = ArrayHelper.AddValue();
		}

		Property->ClearValue_InContainer(Data, ArrayIndex);

		return true;
	}


	/**
	 * Gets a pointer to object of the given property.
	 *
	 * @param Property The property to get.
	 * @param Outer The property that contains the property to be get, if any.
	 * @param Data A pointer to the memory holding the property's data.
	 * @param ArrayIndex The index of the element to set (if the property is an array).
	 * @return A pointer to the object represented by the property, null otherwise..
	 * @see ClearPropertyValue
	 */
	void* GetPropertyValuePtr( UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex )
	{
		check(Property);

		if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Outer))
		{
			if (ArrayProperty->Inner != Property)
			{
				return nullptr;
			}

			FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->template ContainerPtrToValuePtr<void>(Data));
			int32 Index = ArrayHelper.AddValue();

			return ArrayHelper.GetRawPtr(Index);
		}

		if (ArrayIndex >= Property->ArrayDim)
		{
			return nullptr;
		}

		return Property->template ContainerPtrToValuePtr<void>(Data, ArrayIndex);
	}

	/**
	 * Sets the value of the given property.
	 *
	 * @param Property The property to set.
	 * @param Outer The property that contains the property to be set, if any.
	 * @param Data A pointer to the memory holding the property's data.
	 * @param ArrayIndex The index of the element to set (if the property is an array).
	 * @return true on success, false otherwise.
	 * @see ClearPropertyValue
	 */
	template<typename PropertyType, typename ValueType>
	bool SetPropertyValue( PropertyType* Property, UProperty* Outer, void* Data, int32 ArrayIndex, const ValueType& Value )
	{
		if (void* Ptr = GetPropertyValuePtr(Property, Outer, Data, ArrayIndex))
		{
			*(ValueType*)Ptr = Value;
			return true;
		}

		return false;
	}
}


/* IStructDeserializerBackend interface
 *****************************************************************************/

const FString& FJsonStructDeserializerBackend::GetCurrentPropertyName() const
{
	return JsonReader->GetIdentifier();
}


FString FJsonStructDeserializerBackend::GetDebugString() const
{
	return FString::Printf(TEXT("Line: %u, Ch: %u"), JsonReader->GetLineNumber(), JsonReader->GetCharacterNumber());
}


const FString& FJsonStructDeserializerBackend::GetLastErrorMessage() const
{
	return JsonReader->GetErrorMessage();
}


bool FJsonStructDeserializerBackend::GetNextToken( EStructDeserializerBackendTokens& OutToken )
{
	if (!JsonReader->ReadNext(LastNotation))
	{
		return false;
	}

	switch (LastNotation)
	{
	case EJsonNotation::ArrayEnd:
		OutToken = EStructDeserializerBackendTokens::ArrayEnd;
		break;

	case EJsonNotation::ArrayStart:
		OutToken = EStructDeserializerBackendTokens::ArrayStart;
		break;

	case EJsonNotation::Boolean:
	case EJsonNotation::Null:
	case EJsonNotation::Number:
	case EJsonNotation::String:
		{
			OutToken = EStructDeserializerBackendTokens::Property;
		}
		break;

	case EJsonNotation::Error:
		OutToken = EStructDeserializerBackendTokens::Error;
		break;

	case EJsonNotation::ObjectEnd:
		OutToken = EStructDeserializerBackendTokens::StructureEnd;
		break;

	case EJsonNotation::ObjectStart:
		OutToken = EStructDeserializerBackendTokens::StructureStart;
		break;

	default:
		OutToken = EStructDeserializerBackendTokens::None;
	}

	return true;
}


bool FJsonStructDeserializerBackend::ReadProperty( UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex )
{
	using namespace JsonStructDeserializerBackend;

	switch (LastNotation)
	{
	// boolean values
	case EJsonNotation::Boolean:
		{
			bool BoolValue = JsonReader->GetValueAsBoolean();

			if (UBoolProperty* BoolProperty = Cast<UBoolProperty>(Property))
			{
				return SetPropertyValue(BoolProperty, Outer, Data, ArrayIndex, BoolValue);
			}

			UE_LOG(LogSerialization, Verbose, TEXT("Boolean field %s with value '%s' is not supported in UProperty type %s (%s)"), *Property->GetFName().ToString(), BoolValue ? *(GTrue.ToString()) : *(GFalse.ToString()), *Property->GetClass()->GetName(), *GetDebugString());

			return false;
		}
		break;

	// numeric values
	case EJsonNotation::Number:
		{
			double NumericValue = JsonReader->GetValueAsNumber();

			if (UByteProperty* ByteProperty = Cast<UByteProperty>(Property))
			{
				return SetPropertyValue(ByteProperty, Outer, Data, ArrayIndex, (int8)NumericValue);
			}

			if (UDoubleProperty* DoubleProperty = Cast<UDoubleProperty>(Property))
			{
				return SetPropertyValue(DoubleProperty, Outer, Data, ArrayIndex, (double)NumericValue);
			}

			if (UFloatProperty* FloatProperty = Cast<UFloatProperty>(Property))
			{
				return SetPropertyValue(FloatProperty, Outer, Data, ArrayIndex, (float)NumericValue);
			}

			if (UIntProperty* IntProperty = Cast<UIntProperty>(Property))
			{
				return SetPropertyValue(IntProperty, Outer, Data, ArrayIndex, (int32)NumericValue);
			}

			if (UUInt32Property* UInt32Property = Cast<UUInt32Property>(Property))
			{
				return SetPropertyValue(UInt32Property, Outer, Data, ArrayIndex, (uint32)NumericValue);
			}

			if (UInt16Property* Int16Property = Cast<UInt16Property>(Property))
			{
				return SetPropertyValue(Int16Property, Outer, Data, ArrayIndex, (int16)NumericValue);
			}

			if (UUInt16Property* UInt16Property = Cast<UUInt16Property>(Property))
			{
				return SetPropertyValue(UInt16Property, Outer, Data, ArrayIndex, (uint16)NumericValue);
			}

			if (UInt64Property* Int64Property = Cast<UInt64Property>(Property))
			{
				return SetPropertyValue(Int64Property, Outer, Data, ArrayIndex, (int64)NumericValue);
			}

			if (UUInt64Property* UInt64Property = Cast<UUInt64Property>(Property))
			{
				return SetPropertyValue(UInt64Property, Outer, Data, ArrayIndex, (uint64)NumericValue);
			}

			if (UInt8Property* Int8Property = Cast<UInt8Property>(Property))
			{
				return SetPropertyValue(Int8Property, Outer, Data, ArrayIndex, (int8)NumericValue);
			}

			UE_LOG(LogSerialization, Verbose, TEXT("Numeric field %s with value '%f' is not supported in UProperty type %s (%s)"), *Property->GetFName().ToString(), NumericValue, *Property->GetClass()->GetName(), *GetDebugString());

			return false;
		}
		break;

	// null values
	case EJsonNotation::Null:
		return ClearPropertyValue(Property, Outer, Data, ArrayIndex);

	// strings, names & enumerations
	case EJsonNotation::String:
		{
			const FString& StringValue = JsonReader->GetValueAsString();

			if (UStrProperty* StrProperty = Cast<UStrProperty>(Property))
			{
				return SetPropertyValue(StrProperty, Outer, Data, ArrayIndex, StringValue);
			}

			if (UNameProperty* NameProperty = Cast<UNameProperty>(Property))
			{
				return SetPropertyValue(NameProperty, Outer, Data, ArrayIndex, FName(*StringValue));
			}

			if (UTextProperty* TextProperty = Cast<UTextProperty>(Property))
			{
				return SetPropertyValue(TextProperty, Outer, Data, ArrayIndex, FText::FromString(StringValue));
			}

			if (UByteProperty* ByteProperty = Cast<UByteProperty>(Property))
			{
				if (!ByteProperty->Enum)
				{
					return false;
				}

				int32 Value = ByteProperty->Enum->GetValueByName(*StringValue);
				if (Value == INDEX_NONE)
				{
					return false;
				}

				return SetPropertyValue(ByteProperty, Outer, Data, ArrayIndex, (uint8)Value);
			}

			if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
			{
				int64 Value = EnumProperty->GetEnum()->GetValueByName(*StringValue);
				if (Value == INDEX_NONE)
				{
					return false;
				}

				if (void* ElementPtr = GetPropertyValuePtr(EnumProperty, Outer, Data, ArrayIndex))
				{
					EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(ElementPtr, Value);
					return true;
				}

				return false;
			}

			if (UClassProperty* ClassProperty = Cast<UClassProperty>(Property))
			{
				return SetPropertyValue(ClassProperty, Outer, Data, ArrayIndex, LoadObject<UClass>(NULL, *StringValue, NULL, LOAD_NoWarn));
			}

			UE_LOG(LogSerialization, Verbose, TEXT("String field %s with value '%s' is not supported in UProperty type %s (%s)"), *Property->GetFName().ToString(), *StringValue, *Property->GetClass()->GetName(), *GetDebugString());

			return false;
		}
		break;
	}

	return true;
}


void FJsonStructDeserializerBackend::SkipArray()
{
	JsonReader->SkipArray();
}


void FJsonStructDeserializerBackend::SkipStructure()
{
	JsonReader->SkipObject();
}
