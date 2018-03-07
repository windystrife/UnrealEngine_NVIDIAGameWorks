// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CEF/CEFJSStructSerializerBackend.h"
#if WITH_CEF3

#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/PropertyPortFlags.h"


/* Private methods
 *****************************************************************************/

void FCEFJSStructSerializerBackend::AddNull(const FStructSerializerState& State)
{
	StackItem& Current = Stack.Top();
	switch (Current.Kind) {
		case StackItem::STYPE_DICTIONARY:
			Current.DictionaryValue->SetNull(*Scripting->GetBindingName(State.ValueProperty));
			break;
		case StackItem::STYPE_LIST:
			Current.ListValue->SetNull(Current.ListValue->GetSize());
		break;
	}
}


void FCEFJSStructSerializerBackend::Add(const FStructSerializerState& State, bool Value)
{
	StackItem& Current = Stack.Top();
	switch (Current.Kind) {
		case StackItem::STYPE_DICTIONARY:
			Current.DictionaryValue->SetBool(*Scripting->GetBindingName(State.ValueProperty), Value);
			break;
		case StackItem::STYPE_LIST:
			Current.ListValue->SetBool(Current.ListValue->GetSize(), Value);
		break;
	}
}


void FCEFJSStructSerializerBackend::Add(const FStructSerializerState& State, int32 Value)
{
	StackItem& Current = Stack.Top();
	switch (Current.Kind) {
		case StackItem::STYPE_DICTIONARY:
			Current.DictionaryValue->SetInt(*Scripting->GetBindingName(State.ValueProperty), Value);
			break;
		case StackItem::STYPE_LIST:
			Current.ListValue->SetInt(Current.ListValue->GetSize(), Value);
		break;
	}
}


void FCEFJSStructSerializerBackend::Add(const FStructSerializerState& State, double Value)
{
	StackItem& Current = Stack.Top();
	switch (Current.Kind) {
		case StackItem::STYPE_DICTIONARY:
			Current.DictionaryValue->SetDouble(*Scripting->GetBindingName(State.ValueProperty), Value);
			break;
		case StackItem::STYPE_LIST:
			Current.ListValue->SetDouble(Current.ListValue->GetSize(), Value);
		break;
	}
}


void FCEFJSStructSerializerBackend::Add(const FStructSerializerState& State, FString Value)
{
	StackItem& Current = Stack.Top();
	switch (Current.Kind) {
		case StackItem::STYPE_DICTIONARY:
			Current.DictionaryValue->SetString(*Scripting->GetBindingName(State.ValueProperty), *Value);
			break;
		case StackItem::STYPE_LIST:
			Current.ListValue->SetString(Current.ListValue->GetSize(), *Value);
		break;
	}
}


void FCEFJSStructSerializerBackend::Add(const FStructSerializerState& State, UObject* Value)
{
	StackItem& Current = Stack.Top();
	switch (Current.Kind) {
		case StackItem::STYPE_DICTIONARY:
			Current.DictionaryValue->SetDictionary(*Scripting->GetBindingName(State.ValueProperty), Scripting->ConvertObject(Value));
			break;
		case StackItem::STYPE_LIST:
			Current.ListValue->SetDictionary(Current.ListValue->GetSize(), Scripting->ConvertObject(Value));
		break;
	}
}


/* IStructSerializerBackend interface
 *****************************************************************************/

void FCEFJSStructSerializerBackend::BeginArray(const FStructSerializerState& State)
{
	CefRefPtr<CefListValue> ListValue = CefListValue::Create();
	Stack.Push(StackItem(Scripting->GetBindingName(State.ValueProperty), ListValue));
}


void FCEFJSStructSerializerBackend::BeginStructure(const FStructSerializerState& State)
{
	if (State.KeyProperty != nullptr)
	{
		FString KeyString;
		State.KeyProperty->ExportTextItem(KeyString, State.KeyData, nullptr, nullptr, PPF_None);

		CefRefPtr<CefDictionaryValue> DictionaryValue = CefDictionaryValue::Create();
		Stack.Push(StackItem(KeyString, DictionaryValue));
	}
	else if (State.ValueProperty != nullptr)
	{
		CefRefPtr<CefDictionaryValue> DictionaryValue = CefDictionaryValue::Create();
		Stack.Push(StackItem(Scripting->GetBindingName(State.ValueProperty), DictionaryValue));
	}
	else
	{
		Result = CefDictionaryValue::Create();
		Stack.Push(StackItem(FString(), Result));
	}
}


void FCEFJSStructSerializerBackend::EndArray(const FStructSerializerState& /*State*/)
{
	StackItem Previous = Stack.Pop();
	check(Previous.Kind == StackItem::STYPE_LIST);
	check(Stack.Num() > 0); // The root level object is always a struct
	StackItem& Current = Stack.Top();

	switch (Current.Kind) {
		case StackItem::STYPE_DICTIONARY:
			Current.DictionaryValue->SetList(*Previous.Name, Previous.ListValue);
			break;
		case StackItem::STYPE_LIST:
			Current.ListValue->SetList(Current.ListValue->GetSize(), Previous.ListValue);
		break;
	}
}


void FCEFJSStructSerializerBackend::EndStructure(const FStructSerializerState& /*State*/)
{
	StackItem Previous = Stack.Pop();
	check(Previous.Kind == StackItem::STYPE_DICTIONARY);

	if (Stack.Num() > 0)
	{
		StackItem& Current = Stack.Top();

		switch (Current.Kind) {
			case StackItem::STYPE_DICTIONARY:
				Current.DictionaryValue->SetDictionary(*Previous.Name, Previous.DictionaryValue);
				break;
			case StackItem::STYPE_LIST:
				Current.ListValue->SetDictionary(Current.ListValue->GetSize(), Previous.DictionaryValue);
			break;
		}
	}
	else
	{
		check(Result == Previous.DictionaryValue);
	}
}


void FCEFJSStructSerializerBackend::WriteComment(const FString& Comment)
{
	// Cef values do not support comments
}


void FCEFJSStructSerializerBackend::WriteProperty(const FStructSerializerState& State, int32 ArrayIndex)
{
	// booleans
	if (State.ValueType == UBoolProperty::StaticClass())
	{
		Add(State, CastChecked<UBoolProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}

	// unsigned bytes & enumerations
	else if (State.ValueType == UEnumProperty::StaticClass())
	{
		UEnumProperty* EnumProperty = CastChecked<UEnumProperty>(State.ValueProperty);

		Add(State, EnumProperty->GetEnum()->GetNameStringByValue(EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(State.ValueData, ArrayIndex))));
	}
	else if (State.ValueType == UByteProperty::StaticClass())
	{
		UByteProperty* ByteProperty = CastChecked<UByteProperty>(State.ValueProperty);

		if (ByteProperty->IsEnum())
		{
			Add(State, ByteProperty->Enum->GetNameStringByValue(ByteProperty->GetPropertyValue_InContainer(State.ValueData, ArrayIndex)));
		}
		else
		{
			Add(State, (double)ByteProperty->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
		}
	}

	// floating point numbers
	else if (State.ValueType == UDoubleProperty::StaticClass())
	{
		Add(State, CastChecked<UDoubleProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}
	else if (State.ValueType == UFloatProperty::StaticClass())
	{
		Add(State, CastChecked<UFloatProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}

	// signed integers
	else if (State.ValueType == UIntProperty::StaticClass())
	{
		Add(State, (int32)CastChecked<UIntProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}
	else if (State.ValueType == UInt8Property::StaticClass())
	{
		Add(State, (int32)CastChecked<UInt8Property>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}
	else if (State.ValueType == UInt16Property::StaticClass())
	{
		Add(State, (int32)CastChecked<UInt16Property>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}
	else if (State.ValueType == UInt64Property::StaticClass())
	{
		Add(State, (double)CastChecked<UInt64Property>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}

	// unsigned integers
	else if (State.ValueType == UUInt16Property::StaticClass())
	{
		Add(State, (int32)CastChecked<UUInt16Property>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}
	else if (State.ValueType == UUInt32Property::StaticClass())
	{
		Add(State, (double)CastChecked<UUInt32Property>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}
	else if (State.ValueType == UUInt64Property::StaticClass())
	{
		Add(State, (double)CastChecked<UUInt64Property>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}

	// names & strings
	else if (State.ValueType == UNameProperty::StaticClass())
	{
		Add(State, CastChecked<UNameProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex).ToString());
	}
	else if (State.ValueType == UStrProperty::StaticClass())
	{
		Add(State, CastChecked<UStrProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}
	else if (State.ValueType == UTextProperty::StaticClass())
	{
		Add(State, CastChecked<UTextProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex).ToString());
	}

	// classes & objects
	else if (State.ValueType == UClassProperty::StaticClass())
	{
		Add(State, CastChecked<UClassProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex)->GetPathName());
	}
	else if (State.ValueType == UObjectProperty::StaticClass())
	{
		Add(State, CastChecked<UObjectProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}

	// unsupported property type
	else
	{
		GLog->Logf(ELogVerbosity::Warning, TEXT("FCEFJSStructSerializerBackend: Property %s cannot be serialized, because its type (%s) is not supported"), *State.ValueProperty->GetName(), *State.ValueType->GetName());
	}
}


#endif
