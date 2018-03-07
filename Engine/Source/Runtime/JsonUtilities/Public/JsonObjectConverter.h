// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "Serialization/JsonTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectWrapper.h"

/** Class that handles converting Json objects to and from UStructs */
class JSONUTILITIES_API FJsonObjectConverter
{
public:

	/** FName case insensitivity can make the casing of UPROPERTIES unpredictable. Attempt to standardize output. */
	static FString StandardizeCase(const FString &StringIn);

	/** Parse an FText from a json object (assumed to be of the form where keys are culture codes and values are strings) */
	static bool GetTextFromObject(const TSharedRef<FJsonObject>& Obj, FText& TextOut);

public: // UStruct -> JSON

	/**
	 * Optional callback to run when exporting a type which we don't already understand.
	 * If this returns a valid pointer it will be inserted into the export chain. If not, or if this is not
	 * passed in, then we will call the generic ToString on the type and export as a JSON string.
	 */
	DECLARE_DELEGATE_RetVal_TwoParams(TSharedPtr<FJsonValue>, CustomExportCallback, UProperty* /* Property */, const void* /* Value */);

	/**
	 * Templated version of UStructToJsonObject to try and make most of the params. Also serves as an example use case
	 *
	 * @param InStruct The UStruct instance to read from
	 * @param ExportCb Optional callback for types we don't understand. This is called right before falling back to the generic ToString()
	 * @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip properties that match any of these flags
	 * @return FJsonObject pointer. Invalid if an error occurred.
	 */
	template<typename InStructType>
	static TSharedPtr<FJsonObject> UStructToJsonObject(const InStructType& InStruct, int64 CheckFlags = 0, int64 SkipFlags = 0, const CustomExportCallback* ExportCb = nullptr)
	{
		TSharedRef<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
		if (UStructToJsonObject(InStructType::StaticStruct(), &InStruct, JsonObject, CheckFlags, SkipFlags, ExportCb))
		{
			return JsonObject;
		}
		return TSharedPtr<FJsonObject>(); // something went wrong
	}

	/**
	 * Converts from a UStruct to a Json Object, using exportText
	 *
	 * @param StructDefinition UStruct definition that is looked over for properties
	 * @param Struct The UStruct instance to copy out of
	 * @param JsonObject Json Object to be filled in with data from the ustruct
	 * @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip properties that match any of these flags
	 * @param ExportCb Optional callback for types we don't understand. This is called right before falling back to the generic ToString()
	 *
	 * @return False if any properties failed to write
	 */
	static bool UStructToJsonObject(const UStruct* StructDefinition, const void* Struct, TSharedRef<FJsonObject> OutJsonObject, int64 CheckFlags, int64 SkipFlags, const CustomExportCallback* ExportCb = nullptr);

	/**
	 * Converts from a UStruct to a json string containing an object, using exportText
	 *
	 * @param StructDefinition UStruct definition that is looked over for properties
	 * @param Struct The UStruct instance to copy out of
	 * @param JsonObject Json Object to be filled in with data from the ustruct
	 * @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip properties that match any of these flags
	 * @param Indent How many tabs to add to the json serializer
	 * @param ExportCb Optional callback for types we don't understand. This is called right before falling back to the generic ToString()
	 * @param bPrettyPrint Option to use pretty print (e.g., adds line endings) or condensed print
	 *
	 * @return False if any properties failed to write
	 */
	static bool UStructToJsonObjectString(const UStruct* StructDefinition, const void* Struct, FString& OutJsonString, int64 CheckFlags, int64 SkipFlags, int32 Indent = 0, const CustomExportCallback* ExportCb = nullptr, bool bPrettyPrint = true);

	/**
	 * Templated version; Converts from a UStruct to a json string containing an object, using exportText
	 *
	 * @param Struct The UStruct instance to copy out of
	 * @param JsonObject Json Object to be filled in with data from the ustruct
	 * @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip properties that match any of these flags
	 * @param Indent How many tabs to add to the json serializer
	 * @param ExportCb Optional callback for types we don't understand. This is called right before falling back to the generic ToString()
	 * @param bPrettyPrint Option to use pretty print (e.g., adds line endings) or condensed print
	 *
	 * @return False if any properties failed to write
	 */
	template<typename InStructType>
	static bool UStructToJsonObjectString(const InStructType& InStruct, FString& OutJsonString, int64 CheckFlags = 0, int64 SkipFlags = 0, int32 Indent = 0, const CustomExportCallback* ExportCb = nullptr, bool bPrettyPrint = true)
	{
		return UStructToJsonObjectString(InStructType::StaticStruct(), &InStruct, OutJsonString, CheckFlags, SkipFlags, Indent, ExportCb, bPrettyPrint);
	}

	/**
	 * Wrapper to UStructToJsonObjectString that allows a print policy to be specified.
	 */
	template<typename CharType, template<typename> class PrintPolicy>
	static bool UStructToFormattedJsonObjectString(const UStruct* StructDefinition, const void* Struct, FString& OutJsonString, int64 CheckFlags, int64 SkipFlags, int32 Indent = 0, const CustomExportCallback* ExportCb = nullptr)
	{
		TSharedRef<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
		if (UStructToJsonObject(StructDefinition, Struct, JsonObject, CheckFlags, SkipFlags, ExportCb))
		{
			TSharedRef<TJsonWriter<CharType, PrintPolicy<CharType>>> JsonWriter = TJsonWriterFactory<CharType, PrintPolicy<CharType>>::Create(&OutJsonString, Indent);

			if (FJsonSerializer::Serialize(JsonObject, JsonWriter))
			{
				JsonWriter->Close();
				return true;
			}
			else
			{
				UE_LOG(LogJson, Warning, TEXT("UStructToFormattedObjectString - Unable to write out json"));
				JsonWriter->Close();
			}
		}

		return false;
	}

	/**
	 * Converts from a UStruct to a set of json attributes (possibly from within a JsonObject)
	 *
	 * @param StructDefinition UStruct definition that is looked over for properties
	 * @param Struct The UStruct instance to copy out of
	 * @param JsonAttributes Map of attributes to copy in to
	 * @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip properties that match any of these flags
	 * @param ExportCb Optional callback for types we don't understand. This is called right before falling back to the generic ToString()
	 *
	 * @return False if any properties failed to write
	 */
	static bool UStructToJsonAttributes(const UStruct* StructDefinition, const void* Struct, TMap< FString, TSharedPtr<FJsonValue> >& OutJsonAttributes, int64 CheckFlags, int64 SkipFlags, const CustomExportCallback* ExportCb = nullptr);

	/* * Converts from a UProperty to a Json Value using exportText
	 *
	 * @param Property			The property to export
	 * @param Value				Pointer to the value of the property
	 * @param CheckFlags		Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags			Skip properties that match any of these flags
	 * @param ExportCb			Optional callback for types we don't understand. This is called right before falling back to the generic ToString()
	 *
	 * @return					The constructed JsonValue from the property
	 */
	static TSharedPtr<FJsonValue> UPropertyToJsonValue(UProperty* Property, const void* Value, int64 CheckFlags, int64 SkipFlags, const CustomExportCallback* ExportCb = nullptr);

public: // JSON -> UStruct

	/**
	 * Converts from a Json Object to a UStruct, using importText
	 *
	 * @param JsonObject Json Object to copy data out of
	 * @param StructDefinition UStruct definition that is looked over for properties
	 * @param Struct The UStruct instance to copy in to
	 * @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip properties that match any of these flags
	 *
	 * @return False if any properties matched but failed to deserialize
	 */
	static bool JsonObjectToUStruct(const TSharedRef<FJsonObject>& JsonObject, const UStruct* StructDefinition, void* OutStruct, int64 CheckFlags = 0, int64 SkipFlags = 0);

	/**
	 * Templated version of JsonObjectToUStruct
	 *
	 * @param JsonObject Json Object to copy data out of
	 * @param OutStruct The UStruct instance to copy in to
	 * @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip properties that match any of these flags
	 *
	 * @return False if any properties matched but failed to deserialize
	 */
	template<typename OutStructType>
	static bool JsonObjectToUStruct(const TSharedRef<FJsonObject>& JsonObject, OutStructType* OutStruct, int64 CheckFlags = 0, int64 SkipFlags = 0)
	{
		return JsonObjectToUStruct(JsonObject, OutStructType::StaticStruct(), OutStruct, CheckFlags, SkipFlags);
	}

	/**
	 * Converts a set of json attributes (possibly from within a JsonObject) to a UStruct, using importText
	 *
	 * @param JsonAttributes Json Object to copy data out of
	 * @param StructDefinition UStruct definition that is looked over for properties
	 * @param Struct The UStruct instance to copy in to
	 * @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip properties that match any of these flags
	 *
	 * @return False if any properties matched but failed to deserialize
	 */
	static bool JsonAttributesToUStruct(const TMap< FString, TSharedPtr<FJsonValue> >& JsonAttributes, const UStruct* StructDefinition, void* OutStruct, int64 CheckFlags, int64 SkipFlags);

	/**
	 * Converts a single JsonValue to the corresponding UProperty (this may recurse if the property is a UStruct for instance).
	 *
	 * @param JsonValue The value to assign to this property
	 * @param Property The UProperty definition of the property we're setting.
	 * @param OutValue Pointer to the property instance to be modified.
	 * @param CheckFlags Only convert sub-properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip sub-properties that match any of these flags
	 *
	 * @return False if the property failed to serialize
	 */
	static bool JsonValueToUProperty(const TSharedPtr<FJsonValue> JsonValue, UProperty* Property, void* OutValue, int64 CheckFlags, int64 SkipFlags);

	/**
	 * Converts from a json string containing an object to a UStruct
	 *
	 * @param JsonString String containing JSON formatted data.
	 * @param OutStruct The UStruct instance to copy in to
	 * @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip properties that match any of these flags
	 *
	 * @return False if any properties matched but failed to deserialize
	 */
	template<typename OutStructType>
	static bool JsonObjectStringToUStruct(const FString& JsonString, OutStructType* OutStruct, int64 CheckFlags, int64 SkipFlags)
	{
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
		{
			UE_LOG(LogJson, Warning, TEXT("JsonObjectStringToUStruct - Unable to parse json=[%s]"), *JsonString);
			return false;
		}
		if (!FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), OutStruct, CheckFlags, SkipFlags))
		{
			UE_LOG(LogJson, Warning, TEXT("JsonObjectStringToUStruct - Unable to deserialize. json=[%s]"), *JsonString);
			return false;
		}
		return true;
	}

	/**
	* Converts from a json string containing an array to an array of UStructs
	*
	* @param JsonString String containing JSON formatted data.
	* @param OutStructArray The UStruct array to copy in to
	* @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	* @param SkipFlags Skip properties that match any of these flags.
	*
	* @return False if any properties matched but failed to deserialize.
	*/
	template<typename OutStructType>
	static bool JsonArrayStringToUStruct(const FString& JsonString, TArray<OutStructType>* OutStructArray, int64 CheckFlags, int64 SkipFlags)
	{
		TArray<TSharedPtr<FJsonValue> > JsonArray;
		TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(JsonReader, JsonArray))
		{
			UE_LOG(LogJson, Warning, TEXT("JsonArrayStringToUStruct - Unable to parse. json=[%s]"), *JsonString);
			return false;
		}
		if (!JsonArrayToUStruct(JsonArray, OutStructArray, CheckFlags, SkipFlags))
		{
			UE_LOG(LogJson, Warning, TEXT("JsonArrayStringToUStruct - Error parsing one of the elements. json=[%s]"), *JsonString);
			return false;
		}
		return true;
	}

	/**
	* Converts from an array of json values to an array of UStructs.
	*
	* @param JsonArray Array containing json values to convert.
	* @param OutStructArray The UStruct array to copy in to
	* @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	* @param SkipFlags Skip properties that match any of these flags.
	*
	* @return False if any of the matching elements are not an object, or if one of the matching elements could not be converted to the specified UStruct type.
	*/
	template<typename OutStructType>
	static bool JsonArrayToUStruct(const TArray<TSharedPtr<FJsonValue>>& JsonArray, TArray<OutStructType>* OutStructArray, int64 CheckFlags, int64 SkipFlags)
	{
		OutStructArray->SetNum(JsonArray.Num());
		for (int32 i = 0; i < JsonArray.Num(); ++i)
		{
			const auto& Value = JsonArray[i];
			if (Value->Type != EJson::Object)
			{
				UE_LOG(LogJson, Warning, TEXT("JsonArrayToUStruct - Array element [%i] was not an object."), i);
				return false;
			}
			if (!FJsonObjectConverter::JsonObjectToUStruct(Value->AsObject().ToSharedRef(), OutStructType::StaticStruct(), &(*OutStructArray)[i], CheckFlags, SkipFlags))
			{
				UE_LOG(LogJson, Warning, TEXT("JsonArrayToUStruct - Unable to convert element [%i]."), i);
				return false;
			}
		}
		return true;
	}

	/*
	* Parses text arguments from Json into a map
	* @param JsonObject Object to parse arguments from
	*/
	static FFormatNamedArguments ParseTextArgumentsFromJson(const TSharedPtr<const FJsonObject>& JsonObject);
};
