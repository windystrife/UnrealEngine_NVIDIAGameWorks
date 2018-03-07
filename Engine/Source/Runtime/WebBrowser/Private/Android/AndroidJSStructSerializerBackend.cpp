// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidJSStructSerializerBackend.h"
#include "AndroidJSScripting.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyPortFlags.h"
#include "Templates/Casts.h"

void FAndroidJSStructSerializerBackend::WriteProperty(const FStructSerializerState& State, int32 ArrayIndex)
{
	// The parent class serialzes UObjects as NULLs
	if (State.ValueType == UObjectProperty::StaticClass())
	{
		WriteUObject(State, CastChecked<UObjectProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}
	// basic property type (json serializable)
	else
	{
		FJsonStructSerializerBackend::WriteProperty(State, ArrayIndex);
	}
}

void FAndroidJSStructSerializerBackend::WriteUObject(const FStructSerializerState& State, UObject* Value)
{
	// Note this function uses WriteRawJSONValue to append non-json data to the output stream.
	FString RawValue = Scripting->ConvertObject(Value);
	if ((State.ValueProperty == nullptr) || (State.ValueProperty->ArrayDim > 1) || (State.ValueProperty->GetOuter()->GetClass() == UArrayProperty::StaticClass()))
	{
		GetWriter()->WriteRawJSONValue(RawValue);
	}
	else if (State.KeyProperty != nullptr)
	{
		FString KeyString;
		State.KeyProperty->ExportTextItem(KeyString, State.KeyData, nullptr, nullptr, PPF_None);
		GetWriter()->WriteRawJSONValue(KeyString, RawValue);
	}
	else
	{
		GetWriter()->WriteRawJSONValue(Scripting->GetBindingName(State.ValueProperty), RawValue);
	}
}

FString FAndroidJSStructSerializerBackend::ToString()
{
	ReturnBuffer.Add(0);
	ReturnBuffer.Add(0);
	auto CastObject = StringCast<TCHAR>((UCS2CHAR*)ReturnBuffer.GetData());
	return FString(CastObject.Get(), CastObject.Length());
}

FAndroidJSStructSerializerBackend::FAndroidJSStructSerializerBackend(TSharedRef<class FAndroidJSScripting> InScripting)
	: Scripting(InScripting)
	, ReturnBuffer()
	, Writer(ReturnBuffer)
	, FJsonStructSerializerBackend(Writer)
{
}
