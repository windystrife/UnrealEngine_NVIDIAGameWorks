// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "JsonObjectWrapper.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool FJsonObjectWrapper::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	// read JSON string from Buffer
	FString Json;
	if (*Buffer == TCHAR('"'))
	{
		int32 NumCharsRead = 0;
		if (!FParse::QuotedString(Buffer, Json, &NumCharsRead))
		{
			ErrorText->Logf(ELogVerbosity::Warning, TEXT("FJsonObjectWrapper::ImportTextItem: Bad quoted string: %s\n"), Buffer);
			return false;
		}
		Buffer += NumCharsRead;
	}
	else
	{
		// consume the rest of the string (this happens on Paste)
		Json = Buffer;
		Buffer += Json.Len();
	}

	// empty string yields empty shared pointer
	if (Json.IsEmpty())
	{
		JsonString.Empty();
		JsonObject.Reset();
		return true;
	}

	// parse the json
	TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
	{
		if (ErrorText)
		{
			ErrorText->Logf(ELogVerbosity::Warning, TEXT("FJsonObjectWrapper::ImportTextItem - Unable to parse json: %s\n"), *Json);
		}
		return false;
	}
	JsonString = Json;
	return true;
}

bool FJsonObjectWrapper::ExportTextItem(FString& ValueStr, FJsonObjectWrapper const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	// empty pointer yields empty string
	if (!JsonObject.IsValid())
	{
		ValueStr.Empty();
		return true;
	}

	// serialize the json
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&ValueStr, 0);
	if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), JsonWriter, true))
	{
		return false;
	}
	return true;
}

void FJsonObjectWrapper::PostSerialize(const FArchive& Ar)
{
	if (!JsonString.IsEmpty())
	{
		// try to parse JsonString
		TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(JsonReader, JsonObject))
		{
			// do not abide a string that won't parse
			JsonString.Empty();
		}
	}
}
