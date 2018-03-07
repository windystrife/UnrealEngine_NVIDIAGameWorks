// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Backends/JsonStructSerializerBackend.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/PropertyPortFlags.h"


/* Internal helpers
 *****************************************************************************/

namespace JsonStructSerializerBackend
{
	// Writes a property value to the serialization output.
	template<typename ValueType>
	void WritePropertyValue(const TSharedRef<TJsonWriter<UCS2CHAR>> JsonWriter, const FStructSerializerState& State, const ValueType& Value)
	{
		if ((State.ValueProperty == nullptr) || (State.ValueProperty->ArrayDim > 1) || (State.ValueProperty->GetOuter()->GetClass() == UArrayProperty::StaticClass()))
		{
			JsonWriter->WriteValue(Value);
		}
		else if (State.KeyProperty != nullptr)
		{
			FString KeyString;
			State.KeyProperty->ExportTextItem(KeyString, State.KeyData, nullptr, nullptr, PPF_None);
			JsonWriter->WriteValue(KeyString, Value);
		}
		else
		{
			JsonWriter->WriteValue(State.ValueProperty->GetName(), Value);
		}
	}

	// Writes a null value to the serialization output.
	void WriteNull(const TSharedRef<TJsonWriter<UCS2CHAR>> JsonWriter, const FStructSerializerState& State)
	{
		if ((State.ValueProperty == nullptr) || (State.ValueProperty->ArrayDim > 1) || (State.ValueProperty->GetOuter()->GetClass() == UArrayProperty::StaticClass()))
		{
			JsonWriter->WriteNull();
		}
		else if (State.KeyProperty != nullptr)
		{
			FString KeyString;
			State.KeyProperty->ExportTextItem(KeyString, State.KeyData, nullptr, nullptr, PPF_None);
			JsonWriter->WriteNull(KeyString);
		}
		else
		{
			JsonWriter->WriteNull(State.ValueProperty->GetName());
		}
	}
}


/* IStructSerializerBackend interface
 *****************************************************************************/

void FJsonStructSerializerBackend::BeginArray(const FStructSerializerState& State)
{
	UObject* Outer = State.ValueProperty->GetOuter();

	if ((Outer != nullptr) && (Outer->GetClass() == UArrayProperty::StaticClass()))
	{
		JsonWriter->WriteArrayStart();
	}
	else if (State.KeyProperty != nullptr)
	{
		FString KeyString;
		State.KeyProperty->ExportTextItem(KeyString, State.KeyData, nullptr, nullptr, PPF_None);
		JsonWriter->WriteArrayStart(KeyString);
	}
	else
	{
		JsonWriter->WriteArrayStart(State.ValueProperty->GetName());
	}
}


void FJsonStructSerializerBackend::BeginStructure(const FStructSerializerState& State)
{
	if (State.ValueProperty != nullptr)
	{
		UObject* Outer = State.ValueProperty->GetOuter();

		if ((Outer != nullptr) && (Outer->GetClass() == UArrayProperty::StaticClass()))
		{
			JsonWriter->WriteObjectStart();
		}
		else if (State.KeyProperty != nullptr)
		{
			FString KeyString;
			State.KeyProperty->ExportTextItem(KeyString, State.KeyData, nullptr, nullptr, PPF_None);
			JsonWriter->WriteObjectStart(KeyString);
		}
		else
		{
			JsonWriter->WriteObjectStart(State.ValueProperty->GetName());
		}
	}
	else
	{
		JsonWriter->WriteObjectStart();
	}
}


void FJsonStructSerializerBackend::EndArray(const FStructSerializerState& /*State*/)
{
	JsonWriter->WriteArrayEnd();
}


void FJsonStructSerializerBackend::EndStructure(const FStructSerializerState& /*State*/)
{
	JsonWriter->WriteObjectEnd();
}


void FJsonStructSerializerBackend::WriteComment(const FString& Comment)
{
	// Json does not support comments
}


void FJsonStructSerializerBackend::WriteProperty(const FStructSerializerState& State, int32 ArrayIndex)
{
	using namespace JsonStructSerializerBackend;

	// booleans
	if (State.ValueType == UBoolProperty::StaticClass())
	{
		WritePropertyValue(JsonWriter, State, CastChecked<UBoolProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}

	// unsigned bytes & enumerations
	else if (State.ValueType == UEnumProperty::StaticClass())
	{
		UEnumProperty* EnumProperty = CastChecked<UEnumProperty>(State.ValueProperty);

		WritePropertyValue(JsonWriter, State, EnumProperty->GetEnum()->GetNameStringByValue(EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(State.ValueData, ArrayIndex))));
	}
	else if (State.ValueType == UByteProperty::StaticClass())
	{
		UByteProperty* ByteProperty = CastChecked<UByteProperty>(State.ValueProperty);

		if (ByteProperty->IsEnum())
		{
			WritePropertyValue(JsonWriter, State, ByteProperty->Enum->GetNameStringByValue(ByteProperty->GetPropertyValue_InContainer(State.ValueData, ArrayIndex)));
		}
		else
		{
			WritePropertyValue(JsonWriter, State, (double)ByteProperty->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
		}
	}

	// floating point numbers
	else if (State.ValueType == UDoubleProperty::StaticClass())
	{
		WritePropertyValue(JsonWriter, State, CastChecked<UDoubleProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}
	else if (State.ValueType == UFloatProperty::StaticClass())
	{
		WritePropertyValue(JsonWriter, State, CastChecked<UFloatProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}

	// signed integers
	else if (State.ValueType == UIntProperty::StaticClass())
	{
		WritePropertyValue(JsonWriter, State, (double)CastChecked<UIntProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}
	else if (State.ValueType == UInt8Property::StaticClass())
	{
		WritePropertyValue(JsonWriter, State, (double)CastChecked<UInt8Property>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}
	else if (State.ValueType == UInt16Property::StaticClass())
	{
		WritePropertyValue(JsonWriter, State, (double)CastChecked<UInt16Property>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}
	else if (State.ValueType == UInt64Property::StaticClass())
	{
		WritePropertyValue(JsonWriter, State, (double)CastChecked<UInt64Property>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}

	// unsigned integers
	else if (State.ValueType == UUInt16Property::StaticClass())
	{
		WritePropertyValue(JsonWriter, State, (double)CastChecked<UUInt16Property>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}
	else if (State.ValueType == UUInt32Property::StaticClass())
	{
		WritePropertyValue(JsonWriter, State, (double)CastChecked<UUInt32Property>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}
	else if (State.ValueType == UUInt64Property::StaticClass())
	{
		WritePropertyValue(JsonWriter, State, (double)CastChecked<UUInt64Property>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}

	// names, strings & text
	else if (State.ValueType == UNameProperty::StaticClass())
	{
		WritePropertyValue(JsonWriter, State, CastChecked<UNameProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex).ToString());
	}
	else if (State.ValueType == UStrProperty::StaticClass())
	{
		WritePropertyValue(JsonWriter, State, CastChecked<UStrProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}
	else if (State.ValueType == UTextProperty::StaticClass())
	{
		WritePropertyValue(JsonWriter, State, CastChecked<UTextProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex).ToString());
	}

	// classes & objects
	else if (State.ValueType == UClassProperty::StaticClass())
	{
		WritePropertyValue(JsonWriter, State, CastChecked<UClassProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex)->GetPathName());
	}
	else if (State.ValueType == UObjectProperty::StaticClass())
	{
		WriteNull(JsonWriter, State);
	}

	// unsupported property type
	else
	{
		UE_LOG(LogSerialization, Verbose, TEXT("FJsonStructSerializerBackend: Property %s cannot be serialized, because its type (%s) is not supported"), *State.ValueProperty->GetFName().ToString(), *State.ValueType->GetFName().ToString());
	}
}
