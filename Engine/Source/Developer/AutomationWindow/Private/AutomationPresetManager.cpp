// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AutomationPresetManager.h"
#include "HAL/FileManager.h"
#include "UObject/Class.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"

FAutomationTestPresetManager::FAutomationTestPresetManager()
{
	// Add the None Option
	Presets.Add(AutomationPresetPtr());
}

AutomationPresetPtr FAutomationTestPresetManager::AddNewPreset( const FText& PresetName, const TArray<FString>& SelectedTests )
{
	if ( PresetName.IsEmpty() )
	{
		return nullptr;
	}

	const FName NewNameSlug = MakeObjectNameFromDisplayLabel(PresetName.ToString(), NAME_None);

	if ( !Presets.FindByPredicate([&NewNameSlug] (const AutomationPresetPtr& Preset) { return Preset.IsValid() && Preset->GetID() == NewNameSlug; }) )
	{
		AutomationPresetRef NewPreset = MakeShareable(new FAutomationTestPreset(NewNameSlug));
		NewPreset->SetName(PresetName);
		NewPreset->SetEnabledTests(SelectedTests);

		Presets.Add(NewPreset);

		SavePreset(NewPreset);

		return NewPreset;
	}

	return nullptr;
}

TArray<AutomationPresetPtr>& FAutomationTestPresetManager::GetAllPresets()
{
	return Presets;
}

AutomationPresetPtr FAutomationTestPresetManager::LoadPreset( FArchive& Archive )
{
	TSharedPtr<FJsonObject> JsonPreset;

	TSharedRef< TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(&Archive);
	if ( !FJsonSerializer::Deserialize(JsonReader, JsonPreset) )
	{
		return nullptr;
	}

	FAutomationTestPreset* NewPreset = new FAutomationTestPreset();

	if ( FJsonObjectConverter::JsonObjectToUStruct(JsonPreset.ToSharedRef(), NewPreset, 0, 0) )
	{
		return MakeShareable(NewPreset);
	}

	delete NewPreset;

	return nullptr;
}

void FAutomationTestPresetManager::RemovePreset( const AutomationPresetRef Preset )
{
	if (Presets.Remove(Preset) > 0)
	{
		// Find the name of the preset on disk
		FString PresetFileName = GetPresetFolder() / Preset->GetID().ToString() + TEXT(".json");

		// delete the preset on disk
		IFileManager::Get().Delete(*PresetFileName);
	}
}

void FAutomationTestPresetManager::SavePreset( const AutomationPresetRef Preset )
{
	TSharedPtr<FJsonObject> PresetJson = FJsonObjectConverter::UStructToJsonObject(Preset.Get());

	if ( PresetJson.IsValid() )
	{
		FString PresetFileName = GetPresetFolder() / Preset->GetID().ToString() + TEXT(".json");
		FArchive* PresetFileWriter = IFileManager::Get().CreateFileWriter(*PresetFileName);

		if ( PresetFileWriter != nullptr )
		{
			TSharedRef<TJsonWriter<> > JsonWriter = TJsonWriterFactory<>::Create(PresetFileWriter, 0);
			FJsonSerializer::Serialize(PresetJson.ToSharedRef(), JsonWriter);

			delete PresetFileWriter;
		}
	}
}

void FAutomationTestPresetManager::LoadPresets()
{
	TArray<FString> PresetFileNames;

	IFileManager::Get().FindFiles(PresetFileNames, *(GetPresetFolder() / TEXT("*.json")), true, false);

	for (TArray<FString>::TConstIterator It(PresetFileNames); It; ++It)
	{
		FString PresetFilePath = GetPresetFolder() / *It;
		FArchive* PresetFileReader = IFileManager::Get().CreateFileReader(*PresetFilePath);
		
		if (PresetFileReader != nullptr)
		{
			AutomationPresetPtr LoadedPreset = LoadPreset(*PresetFileReader);

			if (LoadedPreset.IsValid())
			{
				Presets.Add(LoadedPreset);
			}
			else
			{
				IFileManager::Get().Delete(*PresetFilePath);
			}

			delete PresetFileReader;
		}
	}
}
