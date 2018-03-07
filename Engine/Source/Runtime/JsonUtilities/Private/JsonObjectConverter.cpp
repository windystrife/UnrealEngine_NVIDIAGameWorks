// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "JsonObjectConverter.h"
#include "Internationalization/Culture.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/PropertyPortFlags.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "JsonObjectWrapper.h"

FString FJsonObjectConverter::StandardizeCase(const FString &StringIn)
{
	// this probably won't work for all cases, consider downcaseing the string fully
	FString FixedString = StringIn;
	FixedString[0] = FChar::ToLower(FixedString[0]); // our json classes/variable start lower case
	FixedString.ReplaceInline(TEXT("ID"), TEXT("Id"), ESearchCase::CaseSensitive); // Id is standard instead of ID, some of our fnames use ID
	return FixedString;
}

namespace
{
/** Convert property to JSON, assuming either the property is not an array or the value is an individual array element */
TSharedPtr<FJsonValue> ConvertScalarUPropertyToJsonValue(UProperty* Property, const void* Value, int64 CheckFlags, int64 SkipFlags, const FJsonObjectConverter::CustomExportCallback* ExportCb)
{
	// See if there's a custom export callback first, so it can override default behavior
	if (ExportCb && ExportCb->IsBound())
	{
		TSharedPtr<FJsonValue> CustomValue = ExportCb->Execute(Property, Value);
		if (CustomValue.IsValid())
		{
			return CustomValue;
		}
		// fall through to default cases
	}

	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		// export enums as strings
		UEnum* EnumDef = EnumProperty->GetEnum();
		FString StringValue = EnumDef->GetNameStringByValue(EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value));
		return MakeShareable(new FJsonValueString(StringValue));
	}
	else if (UNumericProperty *NumericProperty = Cast<UNumericProperty>(Property))
	{
		// see if it's an enum
		UEnum* EnumDef = NumericProperty->GetIntPropertyEnum();
		if (EnumDef != NULL)
		{
			// export enums as strings
			FString StringValue = EnumDef->GetNameStringByValue(NumericProperty->GetSignedIntPropertyValue(Value));
			return MakeShareable(new FJsonValueString(StringValue));
		}

		// We want to export numbers as numbers
		if (NumericProperty->IsFloatingPoint())
		{
			return MakeShareable(new FJsonValueNumber(NumericProperty->GetFloatingPointPropertyValue(Value)));
		}
		else if (NumericProperty->IsInteger())
		{
			return MakeShareable(new FJsonValueNumber(NumericProperty->GetSignedIntPropertyValue(Value)));
		}

		// fall through to default
	}
	else if (UBoolProperty *BoolProperty = Cast<UBoolProperty>(Property))
	{
		// Export bools as bools
		return MakeShareable(new FJsonValueBoolean(BoolProperty->GetPropertyValue(Value)));
	}
	else if (UStrProperty *StringProperty = Cast<UStrProperty>(Property))
	{
		return MakeShareable(new FJsonValueString(StringProperty->GetPropertyValue(Value)));
	}
	else if (UTextProperty *TextProperty = Cast<UTextProperty>(Property))
	{
		return MakeShareable(new FJsonValueString(TextProperty->GetPropertyValue(Value).ToString()));
	}
	else if (UArrayProperty *ArrayProperty = Cast<UArrayProperty>(Property))
	{
		TArray< TSharedPtr<FJsonValue> > Out;
		FScriptArrayHelper Helper(ArrayProperty, Value);
		for (int32 i=0, n=Helper.Num(); i<n; ++i)
		{
			TSharedPtr<FJsonValue> Elem = FJsonObjectConverter::UPropertyToJsonValue(ArrayProperty->Inner, Helper.GetRawPtr(i), CheckFlags & ( ~CPF_ParmFlags ), SkipFlags, ExportCb);
			if ( Elem.IsValid() )
			{
				// add to the array
				Out.Push(Elem);
			}
		}
		return MakeShareable(new FJsonValueArray(Out));
	}
	else if ( USetProperty* SetProperty = Cast<USetProperty>(Property) )
	{
		TArray< TSharedPtr<FJsonValue> > Out;
		FScriptSetHelper Helper(SetProperty, Value);
		for ( int32 i=0, n=Helper.Num(); i < n; ++i )
		{
			if ( Helper.IsValidIndex(i) )
			{
				TSharedPtr<FJsonValue> Elem = FJsonObjectConverter::UPropertyToJsonValue(SetProperty->ElementProp, Helper.GetElementPtr(i), CheckFlags & ( ~CPF_ParmFlags ), SkipFlags, ExportCb);
				if ( Elem.IsValid() )
				{
					// add to the array
					Out.Push(Elem);
				}
			}
		}
		return MakeShareable(new FJsonValueArray(Out));
	}
	else if ( UMapProperty* MapProperty = Cast<UMapProperty>(Property) )
	{
		TSharedRef<FJsonObject> Out = MakeShareable(new FJsonObject());

		FScriptMapHelper Helper(MapProperty, Value);
		for ( int32 i=0, n = Helper.Num(); i < n; ++i )
		{
			if ( Helper.IsValidIndex(i) )
			{
				TSharedPtr<FJsonValue> KeyElement = FJsonObjectConverter::UPropertyToJsonValue(MapProperty->KeyProp, Helper.GetKeyPtr(i), CheckFlags & ( ~CPF_ParmFlags ), SkipFlags, ExportCb);
				TSharedPtr<FJsonValue> ValueElement = FJsonObjectConverter::UPropertyToJsonValue(MapProperty->ValueProp, Helper.GetValuePtr(i), CheckFlags & ( ~CPF_ParmFlags ), SkipFlags, ExportCb);
				if ( KeyElement.IsValid() && ValueElement.IsValid() )
				{
					FString KeyString = KeyElement->AsString();
					if (KeyString.IsEmpty())
					{
						MapProperty->KeyProp->ExportTextItem(KeyString, Helper.GetKeyPtr(i), nullptr, nullptr, 0);
						if (KeyString.IsEmpty())
						{
							UE_LOG(LogJson, Error, TEXT("Unable to convert key to string for property %s."), *MapProperty->GetName())
							KeyString = FString::Printf(TEXT("Unparsed Key %d"), i);
						}
					}

					Out->SetField(KeyString, ValueElement);
				}
			}
		}

		return MakeShareable(new FJsonValueObject(Out));
	}
	else if (UStructProperty *StructProperty = Cast<UStructProperty>(Property))
	{
		UScriptStruct::ICppStructOps* TheCppStructOps = StructProperty->Struct->GetCppStructOps();
		// Intentionally exclude the JSON Object wrapper, which specifically needs to export JSON in an object representation instead of a string
		if (StructProperty->Struct != FJsonObjectWrapper::StaticStruct() && TheCppStructOps && TheCppStructOps->HasExportTextItem())
		{
			FString OutValueStr;
			TheCppStructOps->ExportTextItem(OutValueStr, Value, nullptr, nullptr, PPF_None, nullptr);
			return MakeShareable(new FJsonValueString(OutValueStr));
		}

		TSharedRef<FJsonObject> Out = MakeShareable(new FJsonObject());
		if (FJsonObjectConverter::UStructToJsonObject(StructProperty->Struct, Value, Out, CheckFlags & (~CPF_ParmFlags), SkipFlags, ExportCb))
		{
			return MakeShareable(new FJsonValueObject(Out));
		}
		// fall through to default
	}
	else
	{
		// Default to export as string for everything else
		FString StringValue;
		Property->ExportTextItem(StringValue, Value, NULL, NULL, PPF_None);
		return MakeShareable(new FJsonValueString(StringValue));
	}

	// invalid
	return TSharedPtr<FJsonValue>();
}
}

TSharedPtr<FJsonValue> FJsonObjectConverter::UPropertyToJsonValue(UProperty* Property, const void* Value, int64 CheckFlags, int64 SkipFlags, const CustomExportCallback* ExportCb)
{
	if (Property->ArrayDim == 1)
	{
		return ConvertScalarUPropertyToJsonValue(Property, Value, CheckFlags, SkipFlags, ExportCb);
	}

	TArray< TSharedPtr<FJsonValue> > Array;
	for (int Index = 0; Index != Property->ArrayDim; ++Index)
	{
		Array.Add(ConvertScalarUPropertyToJsonValue(Property, (char*)Value + Index * Property->ElementSize, CheckFlags, SkipFlags, ExportCb));
	}
	return MakeShareable(new FJsonValueArray(Array));
}

bool FJsonObjectConverter::UStructToJsonObject(const UStruct* StructDefinition, const void* Struct, TSharedRef<FJsonObject> OutJsonObject, int64 CheckFlags, int64 SkipFlags, const CustomExportCallback* ExportCb)
{
	return UStructToJsonAttributes(StructDefinition, Struct, OutJsonObject->Values, CheckFlags, SkipFlags, ExportCb);
}

bool FJsonObjectConverter::UStructToJsonAttributes(const UStruct* StructDefinition, const void* Struct, TMap< FString, TSharedPtr<FJsonValue> >& OutJsonAttributes, int64 CheckFlags, int64 SkipFlags, const CustomExportCallback* ExportCb)
{
	if (SkipFlags == 0)
	{
		// If we have no specified skip flags, skip deprecated, transient and skip serialization by default when writing
		SkipFlags |= CPF_Deprecated | CPF_Transient;
	}

	if (StructDefinition == FJsonObjectWrapper::StaticStruct())
	{
		// Just copy it into the object
		const FJsonObjectWrapper* ProxyObject = (const FJsonObjectWrapper *)Struct;

		if (ProxyObject->JsonObject.IsValid())
		{
			OutJsonAttributes = ProxyObject->JsonObject->Values;
		}
		return true;
	}

	for (TFieldIterator<UProperty> It(StructDefinition); It; ++It)
	{
		UProperty* Property = *It;

		// Check to see if we should ignore this property
		if (CheckFlags != 0 && !Property->HasAnyPropertyFlags(CheckFlags))
		{
			continue;
		}
		if (Property->HasAnyPropertyFlags(SkipFlags))
		{
			continue;
		}

		FString VariableName = StandardizeCase(Property->GetName());
		const void* Value = Property->ContainerPtrToValuePtr<uint8>(Struct);

		// convert the property to a FJsonValue
		TSharedPtr<FJsonValue> JsonValue = UPropertyToJsonValue(Property, Value, CheckFlags, SkipFlags, ExportCb);
		if (!JsonValue.IsValid())
		{
			UClass* PropClass = Property->GetClass();
			UE_LOG(LogJson, Error, TEXT("UStructToJsonObject - Unhandled property type '%s': %s"), *PropClass->GetName(), *Property->GetPathName());
			return false;
		}

		// set the value on the output object
		OutJsonAttributes.Add(VariableName, JsonValue);
	}

	return true;
}

template<class CharType, class PrintPolicy>
bool UStructToJsonObjectStringInternal(const TSharedRef<FJsonObject>& JsonObject, FString& OutJsonString, int32 Indent)
{
	TSharedRef<TJsonWriter<CharType, PrintPolicy> > JsonWriter = TJsonWriterFactory<CharType, PrintPolicy>::Create(&OutJsonString, Indent);
	bool bSuccess = FJsonSerializer::Serialize(JsonObject, JsonWriter);
	JsonWriter->Close();
	return bSuccess;
}

bool FJsonObjectConverter::UStructToJsonObjectString(const UStruct* StructDefinition, const void* Struct, FString& OutJsonString, int64 CheckFlags, int64 SkipFlags, int32 Indent, const CustomExportCallback* ExportCb, bool bPrettyPrint)
{
	TSharedRef<FJsonObject> JsonObject = MakeShareable( new FJsonObject() );
	if (UStructToJsonObject(StructDefinition, Struct, JsonObject, CheckFlags, SkipFlags, ExportCb))
	{
		bool bSuccess = false;
		if (bPrettyPrint)
		{
			bSuccess = UStructToJsonObjectStringInternal<TCHAR, TPrettyJsonPrintPolicy<TCHAR> >(JsonObject, OutJsonString, Indent);
		}
		else
		{
			bSuccess = UStructToJsonObjectStringInternal<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >(JsonObject, OutJsonString, Indent);
		}
		if (bSuccess)
		{
			return true;
		}
		else
		{
			UE_LOG(LogJson, Warning, TEXT("UStructToJsonObjectString - Unable to write out json"));
		}
	}

	return false;
}

//static
bool FJsonObjectConverter::GetTextFromObject(const TSharedRef<FJsonObject>& Obj, FText& TextOut)
{
	// get the prioritized culture name list
	FCultureRef CurrentCulture = FInternationalization::Get().GetCurrentCulture();
	TArray<FString> CultureList = CurrentCulture->GetPrioritizedParentCultureNames();

	// try to follow the fall back chain that the engine uses
	FString TextString;
	for (const FString& CultureCode : CultureList)
	{
		if (Obj->TryGetStringField(CultureCode, TextString))
		{
			TextOut = FText::FromString(TextString);
			return true;
		}
	}

	// no luck, is this possibly an unrelated json object?
	return false;
}

//template bool FJsonObjectConverter::UStructToJsonObjectString<TCHAR, TCondensedJsonPrintPolicy>(const UStruct* StructDefinition, const void* Struct, FString& OutJsonString, int64 CheckFlags, int64 SkipFlags, int32 Indent, const CustomExportCallback* ExportCb);

namespace
{

/** Convert JSON to property, assuming either the property is not an array or the value is an individual array element */
bool ConvertScalarJsonValueToUProperty(TSharedPtr<FJsonValue> JsonValue, UProperty* Property, void* OutValue, int64 CheckFlags, int64 SkipFlags)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		if (JsonValue->Type == EJson::String)
		{
			// see if we were passed a string for the enum
			const UEnum* Enum = EnumProperty->GetEnum();
			check(Enum);
			FString StrValue = JsonValue->AsString();
			int64 IntValue = Enum->GetValueByName(FName(*StrValue));
			if (IntValue == INDEX_NONE)
			{
				UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable import enum %s from string value %s for property %s"), *Enum->CppType, *StrValue, *Property->GetNameCPP());
				return false;
			}
			EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(OutValue, IntValue);
		}
		else
		{
			// AsNumber will log an error for completely inappropriate types (then give us a default)
			EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(OutValue, (int64)JsonValue->AsNumber());
		}
	}
	else if (UNumericProperty *NumericProperty = Cast<UNumericProperty>(Property))
	{
		if (NumericProperty->IsEnum() && JsonValue->Type == EJson::String)
		{
			// see if we were passed a string for the enum
			const UEnum* Enum = NumericProperty->GetIntPropertyEnum();
			check(Enum); // should be assured by IsEnum()
			FString StrValue = JsonValue->AsString();
			int64 IntValue = Enum->GetValueByName(FName(*StrValue));
			if (IntValue == INDEX_NONE)
			{
				UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable import enum %s from string value %s for property %s"), *Enum->CppType, *StrValue, *Property->GetNameCPP());
				return false;
			}
			NumericProperty->SetIntPropertyValue(OutValue, IntValue);
		}
		else if(NumericProperty->IsFloatingPoint())
		{
			// AsNumber will log an error for completely inappropriate types (then give us a default)
			NumericProperty->SetFloatingPointPropertyValue(OutValue, JsonValue->AsNumber());
		}
		else if (NumericProperty->IsInteger())
		{
			if (JsonValue->Type == EJson::String)
			{
				// parse string -> int64 ourselves so we don't lose any precision going through AsNumber (aka double)
				NumericProperty->SetIntPropertyValue(OutValue, FCString::Atoi64(*JsonValue->AsString()));
			}
			else
			{
				// AsNumber will log an error for completely inappropriate types (then give us a default)
				NumericProperty->SetIntPropertyValue(OutValue, (int64)JsonValue->AsNumber());
			}
		}
		else 
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to set numeric property type %s for property %s"), *Property->GetClass()->GetName(), *Property->GetNameCPP());
			return false;
		}
	}		
	else if (UBoolProperty *BoolProperty = Cast<UBoolProperty>(Property))
	{
		// AsBool will log an error for completely inappropriate types (then give us a default)
		BoolProperty->SetPropertyValue(OutValue, JsonValue->AsBool());
	}
	else if (UStrProperty *StringProperty = Cast<UStrProperty>(Property))
	{
		// AsString will log an error for completely inappropriate types (then give us a default)
		StringProperty->SetPropertyValue(OutValue, JsonValue->AsString());
	}
	else if (UArrayProperty *ArrayProperty = Cast<UArrayProperty>(Property))
	{
		if (JsonValue->Type == EJson::Array)
		{
			TArray< TSharedPtr<FJsonValue> > ArrayValue = JsonValue->AsArray();
			int32 ArrLen = ArrayValue.Num(); 

			// make the output array size match
			FScriptArrayHelper Helper(ArrayProperty, OutValue);
			Helper.Resize(ArrLen);

			// set the property values
			for (int32 i=0;i<ArrLen;++i)
			{
				const TSharedPtr<FJsonValue>& ArrayValueItem = ArrayValue[i];
				if (ArrayValueItem.IsValid() && !ArrayValueItem->IsNull())
				{
					if (!FJsonObjectConverter::JsonValueToUProperty(ArrayValueItem, ArrayProperty->Inner, Helper.GetRawPtr(i), CheckFlags & (~CPF_ParmFlags), SkipFlags))
					{
						UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to deserialize array element [%d] for property %s"), i, *Property->GetNameCPP());
						return false;
					}
				}
			}
		}
		else
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Attempted to import TArray from non-array JSON key for property %s"), *Property->GetNameCPP());
			return false;
		}
	}
	else if ( UMapProperty* MapProperty = Cast<UMapProperty>(Property) )
	{
		if ( JsonValue->Type == EJson::Object )
		{
			TSharedPtr<FJsonObject> ObjectValue = JsonValue->AsObject();

			FScriptMapHelper Helper(MapProperty, OutValue);

			// set the property values
			for ( const auto& Entry : ObjectValue->Values )
			{
				if ( Entry.Value.IsValid() && !Entry.Value->IsNull() )
				{
					int32 NewIndex = Helper.AddDefaultValue_Invalid_NeedsRehash();
					
					TSharedPtr<FJsonValueString> TempKeyValue = MakeShareable(new FJsonValueString(Entry.Key));

					const bool bKeySuccess = FJsonObjectConverter::JsonValueToUProperty(TempKeyValue, MapProperty->KeyProp, Helper.GetKeyPtr(NewIndex), CheckFlags & ( ~CPF_ParmFlags ), SkipFlags);
					const bool bValueSuccess = FJsonObjectConverter::JsonValueToUProperty(Entry.Value, MapProperty->ValueProp, Helper.GetValuePtr(NewIndex), CheckFlags & ( ~CPF_ParmFlags ), SkipFlags);

					if ( !(bKeySuccess && bValueSuccess) )
					{
						UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to deserialize map element [key: %s] for property %s"), *Entry.Key, *Property->GetNameCPP());
						return false;
					}
				}
			}

			Helper.Rehash();
		}
		else
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Attempted to import TMap from non-object JSON key for property %s"), *Property->GetNameCPP());
			return false;
		}
	}
	else if ( USetProperty* SetProperty = Cast<USetProperty>(Property) )
	{
		if ( JsonValue->Type == EJson::Array )
		{
			TArray< TSharedPtr<FJsonValue> > ArrayValue = JsonValue->AsArray();
			int32 ArrLen = ArrayValue.Num();

			FScriptSetHelper Helper(SetProperty, OutValue);

			// set the property values
			for ( int32 i=0; i < ArrLen; ++i )
			{
				const TSharedPtr<FJsonValue>& ArrayValueItem = ArrayValue[i];
				if ( ArrayValueItem.IsValid() && !ArrayValueItem->IsNull() )
				{
					int32 NewIndex = Helper.AddDefaultValue_Invalid_NeedsRehash();
					if ( !FJsonObjectConverter::JsonValueToUProperty(ArrayValueItem, SetProperty->ElementProp, Helper.GetElementPtr(NewIndex), CheckFlags & ( ~CPF_ParmFlags ), SkipFlags) )
					{
						UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to deserialize set element [%d] for property %s"), i, *Property->GetNameCPP());
						return false;
					}
				}
			}

			Helper.Rehash();
		}
		else
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Attempted to import TSet from non-array JSON key for property %s"), *Property->GetNameCPP());
			return false;
		}
	}
	else if (UTextProperty* TextProperty = Cast<UTextProperty>(Property))
	{
		if (JsonValue->Type == EJson::String)
		{
			// assume this string is already localized, so import as invariant
			TextProperty->SetPropertyValue(OutValue, FText::FromString(JsonValue->AsString()));
		}
		else if (JsonValue->Type == EJson::Object)
		{
			TSharedPtr<FJsonObject> Obj = JsonValue->AsObject();
			check(Obj.IsValid()); // should not fail if Type == EJson::Object

			// import the subvalue as a culture invariant string
			FText Text;
			if (!FJsonObjectConverter::GetTextFromObject(Obj.ToSharedRef(), Text))
			{
				UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Attempted to import FText from JSON object with invalid keys for property %s"), *Property->GetNameCPP());
				return false;
			}
			TextProperty->SetPropertyValue(OutValue, Text);
		}
		else
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Attempted to import FText from JSON that was neither string nor object for property %s"), *Property->GetNameCPP());
			return false;
		}
	}
	else if (UStructProperty *StructProperty = Cast<UStructProperty>(Property))
	{
		static const FName NAME_DateTime(TEXT("DateTime"));
		static const FName NAME_Color(TEXT("Color"));
		static const FName NAME_LinearColor(TEXT("LinearColor"));
		if (JsonValue->Type == EJson::Object)
		{
			TSharedPtr<FJsonObject> Obj = JsonValue->AsObject();
			check(Obj.IsValid()); // should not fail if Type == EJson::Object
			if (!FJsonObjectConverter::JsonObjectToUStruct(Obj.ToSharedRef(), StructProperty->Struct, OutValue, CheckFlags & (~CPF_ParmFlags), SkipFlags))
			{
				UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - FJsonObjectConverter::JsonObjectToUStruct failed for property %s"), *Property->GetNameCPP());
				return false;
			}
		}
		else if (JsonValue->Type == EJson::String && StructProperty->Struct->GetFName() == NAME_LinearColor)
		{
			FLinearColor& ColorOut = *(FLinearColor*)OutValue;
			FString ColorString = JsonValue->AsString();

			FColor IntermediateColor;
			IntermediateColor = FColor::FromHex(ColorString);

			ColorOut = IntermediateColor;
		}
		else if (JsonValue->Type == EJson::String && StructProperty->Struct->GetFName() == NAME_Color)
		{
			FColor& ColorOut = *(FColor*)OutValue;
			FString ColorString = JsonValue->AsString();

			ColorOut = FColor::FromHex(ColorString);
		}
		else if (JsonValue->Type == EJson::String && StructProperty->Struct->GetFName() == NAME_DateTime)
		{
			FString DateString = JsonValue->AsString();
			FDateTime& DateTimeOut = *(FDateTime*)OutValue;
			if (DateString == TEXT("min"))
			{
				// min representable value for our date struct. Actual date may vary by platform (this is used for sorting)
				DateTimeOut = FDateTime::MinValue();
			}
			else if (DateString == TEXT("max"))
			{
				// max representable value for our date struct. Actual date may vary by platform (this is used for sorting)
				DateTimeOut = FDateTime::MaxValue();
			}
			else if (DateString == TEXT("now"))
			{
				// this value's not really meaningful from json serialization (since we don't know timezone) but handle it anyway since we're handling the other keywords
				DateTimeOut = FDateTime::UtcNow();
			}
			else if (FDateTime::ParseIso8601(*DateString, DateTimeOut))
			{
				// ok
			}
			else if (FDateTime::Parse(DateString, DateTimeOut))
			{
				// ok
			}
			else
			{
				UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import FDateTime for property %s"), *Property->GetNameCPP());
				return false;
			}
		}
		else if (JsonValue->Type == EJson::String && StructProperty->Struct->GetCppStructOps() && StructProperty->Struct->GetCppStructOps()->HasImportTextItem())
		{
			UScriptStruct::ICppStructOps* TheCppStructOps = StructProperty->Struct->GetCppStructOps();
			
			FString ImportTextString = JsonValue->AsString();
			const TCHAR* ImportTextPtr = *ImportTextString;
			if (!TheCppStructOps->ImportTextItem(ImportTextPtr, OutValue, PPF_None, nullptr, (FOutputDevice*)GWarn))
			{
				// Fall back to trying the tagged property approach if custom ImportTextItem couldn't get it done
				Property->ImportText(ImportTextPtr, OutValue, PPF_None, nullptr);
			}
		}
		else if (JsonValue->Type == EJson::String)
		{
			FString ImportTextString = JsonValue->AsString();
			const TCHAR* ImportTextPtr = *ImportTextString;
			Property->ImportText(ImportTextPtr, OutValue, PPF_None, nullptr);
		}
		else
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Attempted to import UStruct from non-object JSON key for property %s"), *Property->GetNameCPP());
			return false;
		}
	}
	else
	{
		// Default to expect a string for everything else
		if (Property->ImportText(*JsonValue->AsString(), OutValue, 0, NULL) == NULL)
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable import property type %s from string value for property %s"), *Property->GetClass()->GetName(), *Property->GetNameCPP());
			return false;
		}
	}

	return true;
}
}

bool FJsonObjectConverter::JsonValueToUProperty(TSharedPtr<FJsonValue> JsonValue, UProperty* Property, void* OutValue, int64 CheckFlags, int64 SkipFlags)
{
	if (!JsonValue.IsValid())
	{
		UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Invalid value JSON key"));
		return false;
	}

	bool bArrayProperty = Property->IsA<UArrayProperty>();
	bool bJsonArray = JsonValue->Type == EJson::Array;

	if (!bJsonArray)
	{
		if (bArrayProperty)
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Attempted to import TArray from non-array JSON key"));
			return false;			
		}

		if (Property->ArrayDim != 1)
		{
			UE_LOG(LogJson, Warning, TEXT("Ignoring excess properties when deserializing %s"), *Property->GetName());
		}

		return ConvertScalarJsonValueToUProperty(JsonValue, Property, OutValue, CheckFlags, SkipFlags);
	}

	// In practice, the ArrayDim == 1 check ought to be redundant, since nested arrays of UPropertys are not supported
	if (bArrayProperty && Property->ArrayDim == 1)
	{
		// Read into TArray
		return ConvertScalarJsonValueToUProperty(JsonValue, Property, OutValue, CheckFlags, SkipFlags);
	}

	// We're deserializing a JSON array
	const auto& ArrayValue = JsonValue->AsArray();
	if (Property->ArrayDim < ArrayValue.Num())
	{
		UE_LOG(LogJson, Warning, TEXT("Ignoring excess properties when deserializing %s"), *Property->GetName());
	}

	// Read into native array
	int ItemsToRead = FMath::Clamp(ArrayValue.Num(), 0, Property->ArrayDim);
	for (int Index = 0; Index != ItemsToRead; ++Index)
	{
		if (!ConvertScalarJsonValueToUProperty(ArrayValue[Index], Property, (char*)OutValue + Index * Property->ElementSize, CheckFlags, SkipFlags))
		{
			return false;
		}
	}
	return true;
}

bool FJsonObjectConverter::JsonObjectToUStruct(const TSharedRef<FJsonObject>& JsonObject, const UStruct* StructDefinition, void* OutStruct, int64 CheckFlags, int64 SkipFlags)
{
	return JsonAttributesToUStruct(JsonObject->Values, StructDefinition, OutStruct, CheckFlags, SkipFlags);
}

bool FJsonObjectConverter::JsonAttributesToUStruct(const TMap< FString, TSharedPtr<FJsonValue> >& JsonAttributes, const UStruct* StructDefinition, void* OutStruct, int64 CheckFlags, int64 SkipFlags)
{
	if (StructDefinition == FJsonObjectWrapper::StaticStruct())
	{
		// Just copy it into the object
		FJsonObjectWrapper* ProxyObject = (FJsonObjectWrapper *)OutStruct;
		ProxyObject->JsonObject = MakeShareable(new FJsonObject());
		ProxyObject->JsonObject->Values = JsonAttributes;
		return true;
	}

	// iterate over the struct properties
	for(TFieldIterator<UProperty> PropIt(StructDefinition); PropIt; ++PropIt)
	{
		UProperty* Property = *PropIt;
		FString PropertyName = Property->GetName();

		// Check to see if we should ignore this property
		if (CheckFlags != 0 && !Property->HasAnyPropertyFlags(CheckFlags))
		{
			continue;
		}
		if (Property->HasAnyPropertyFlags(SkipFlags))
		{
			continue;
		}

		// find a json value matching this property name
		TSharedPtr<FJsonValue> JsonValue;
		for (auto It = JsonAttributes.CreateConstIterator(); It; ++It)
		{
			// use case insensitive search sincd FName may change caseing strangely on us
			if (PropertyName.Equals(It.Key(), ESearchCase::IgnoreCase))
			{
				JsonValue = It.Value();
				break;
			}
		}
		if (!JsonValue.IsValid() || JsonValue->IsNull())
		{
			// we allow values to not be found since this mirrors the typical UObject mantra that all the fields are optional when deserializing
			continue;
		}

		void* Value = Property->ContainerPtrToValuePtr<uint8>(OutStruct);
		if (!JsonValueToUProperty(JsonValue, Property, Value, CheckFlags, SkipFlags))
		{
			UE_LOG(LogJson, Error, TEXT("JsonObjectToUStruct - Unable to parse %s.%s from JSON"), *StructDefinition->GetName(), *PropertyName);
			return false;
		}
	}
	
	return true;
}

FFormatNamedArguments FJsonObjectConverter::ParseTextArgumentsFromJson(const TSharedPtr<const FJsonObject>& JsonObject)
{
	FFormatNamedArguments NamedArgs;
	if (JsonObject.IsValid())
	{
		for (const auto& It : JsonObject->Values)
		{
			if (!It.Value.IsValid())
				continue;

			switch (It.Value->Type)
			{
			case EJson::Number:
				// number
				NamedArgs.Emplace(It.Key, It.Value->AsNumber());
				break;
			case EJson::String:
				// culture invariant string
				NamedArgs.Emplace(It.Key, FText::FromString(It.Value->AsString()));
				break;
			case EJson::Object:
			{
				// localized string
				FText TextOut;
				if (FJsonObjectConverter::GetTextFromObject(It.Value->AsObject().ToSharedRef(), TextOut))
				{
					NamedArgs.Emplace(It.Key, TextOut);
				}
				else
				{
					UE_LOG(LogJson, Error, TEXT("Unable to apply Json parameter %s (could not parse object)"), *It.Key);
				}
			}
			break;
			default:
				UE_LOG(LogJson, Error, TEXT("Unable to apply Json parameter %s (bad type)"), *It.Key);
				break;
			}
		}
	}
	return NamedArgs;
}