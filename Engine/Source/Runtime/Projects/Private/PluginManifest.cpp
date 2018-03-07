// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PluginManifest.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "PluginManifest"

bool FPluginManifest::Load(const FString& FileName, FText& OutFailReason)
{
	// Read the file to a string
	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *FileName))
	{
		OutFailReason = FText::Format(LOCTEXT("FailedToLoadDescriptorFile", "Failed to open descriptor file '{0}'"), FText::FromString(FileName));
		return false;
	}

	// Deserialize a JSON object from the string
	TSharedPtr< FJsonObject > ObjectPtr;
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(FileContents);
	if (!FJsonSerializer::Deserialize(Reader, ObjectPtr) || !ObjectPtr.IsValid() )
	{
		OutFailReason = FText::Format(LOCTEXT("FailedToReadDescriptorFile", "Failed to read file. {0}"), FText::FromString(Reader->GetErrorMessage()));
		return false;
	}

	// Parse the object
	return Read(*ObjectPtr.Get(), OutFailReason);
}

bool FPluginManifest::Read(const FJsonObject& Object, FText& OutFailReason)
{
	TArray<TSharedPtr<FJsonValue>> JsonContents = Object.GetArrayField(TEXT("Contents"));
	for (const TSharedPtr<FJsonValue> JsonEntryValue : JsonContents)
	{
		TSharedPtr<FJsonObject> JsonEntry = JsonEntryValue->AsObject();

		FPluginManifestEntry Entry;
		Entry.File = JsonEntry->GetStringField(TEXT("File"));
		if (!Entry.Descriptor.Read(*JsonEntry->GetObjectField(TEXT("Descriptor")).Get(), OutFailReason))
		{
			return false;
		}
		Contents.Add(Entry);
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
