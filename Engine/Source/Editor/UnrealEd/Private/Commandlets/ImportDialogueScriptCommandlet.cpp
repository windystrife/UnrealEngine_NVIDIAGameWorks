// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Commandlets/ImportDialogueScriptCommandlet.h"
#include "UObject/UnrealType.h"
#include "Misc/FileHelper.h"
#include "UObject/PropertyPortFlags.h"
#include "Sound/DialogueWave.h"
#include "Commandlets/ExportDialogueScriptCommandlet.h"
#include "Serialization/Csv/CsvParser.h"

DEFINE_LOG_CATEGORY_STATIC(LogImportDialogueScriptCommandlet, Log, All);

UImportDialogueScriptCommandlet::UImportDialogueScriptCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UImportDialogueScriptCommandlet::Main(const FString& Params)
{
	// Parse command line
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	// Set config path
	FString ConfigPath;
	{
		const FString* ConfigPathParamVal = ParamVals.Find(FString(TEXT("Config")));
		if (!ConfigPathParamVal)
		{
			UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No config specified."));
			return -1;
		}
		ConfigPath = *ConfigPathParamVal;
	}

	// Set config section
	FString SectionName;
	{
		const FString* SectionNameParamVal = ParamVals.Find(FString(TEXT("Section")));
		if (!SectionNameParamVal)
		{
			UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No config section specified."));
			return -1;
		}
		SectionName = *SectionNameParamVal;
	}

	// Source path to the root folder that dialogue script CSV files live in
	FString SourcePath;
	if (!GetPathFromConfig(*SectionName, TEXT("SourcePath"), SourcePath, ConfigPath))
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No source path specified."));
		return -1;
	}

	// Destination path to the root folder that manifest/archive files live in
	FString DestinationPath;
	if (!GetPathFromConfig(*SectionName, TEXT("DestinationPath"), DestinationPath, ConfigPath))
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No destination path specified."));
		return -1;
	}

	// Get culture directory setting, default to true if not specified (used to allow picking of export directory with windows file dialog from Translation Editor)
	bool bUseCultureDirectory = true;
	if (!GetBoolFromConfig(*SectionName, TEXT("bUseCultureDirectory"), bUseCultureDirectory, ConfigPath))
	{
		bUseCultureDirectory = true;
	}

	// Get the native culture
	FString NativeCulture;
	if (!GetStringFromConfig(*SectionName, TEXT("NativeCulture"), NativeCulture, ConfigPath))
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No native culture specified."));
		return -1;
	}

	// Get cultures to generate
	TArray<FString> CulturesToGenerate;
	if (GetStringArrayFromConfig(*SectionName, TEXT("CulturesToGenerate"), CulturesToGenerate, ConfigPath) == 0)
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No cultures specified for import."));
		return -1;
	}

	// Get the manifest name
	FString ManifestName;
	if (!GetStringFromConfig(*SectionName, TEXT("ManifestName"), ManifestName, ConfigPath))
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No manifest name specified."));
		return -1;
	}

	// Get the archive name
	FString ArchiveName;
	if (!GetStringFromConfig(*SectionName, TEXT("ArchiveName"), ArchiveName, ConfigPath))
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No archive name specified."));
		return -1;
	}

	// Get the dialogue script name
	FString DialogueScriptName;
	if (!GetStringFromConfig(*SectionName, TEXT("DialogueScriptName"), DialogueScriptName, ConfigPath))
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No dialogue script name specified."));
		return -1;
	}

	// We may only have a single culture if using this setting
	if (!bUseCultureDirectory && CulturesToGenerate.Num() > 1)
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("bUseCultureDirectory may only be used with a single culture."));
		return false;
	}

	// Load the manifest and all archives
	FLocTextHelper LocTextHelper(DestinationPath, ManifestName, ArchiveName, NativeCulture, CulturesToGenerate, MakeShareable(new FLocFileSCCNotifies(SourceControlInfo)));
	{
		FText LoadError;
		if (!LocTextHelper.LoadAll(ELocTextHelperLoadFlags::LoadOrCreate, &LoadError))
		{
			UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("%s"), *LoadError.ToString());
			return false;
		}
	}

	// Import the native culture first as this may trigger additional translations in foreign archives
	{
		const FString CultureSourcePath = SourcePath / (bUseCultureDirectory ? NativeCulture : TEXT(""));
		const FString CultureDestinationPath = DestinationPath / NativeCulture;
		ImportDialogueScriptForCulture(LocTextHelper, CultureSourcePath / DialogueScriptName, NativeCulture, true);
	}

	// Import any remaining cultures
	for (const FString& CultureName : CulturesToGenerate)
	{
		// Skip the native culture as we already processed it above
		if (CultureName == NativeCulture)
		{
			continue;
		}

		const FString CultureSourcePath = SourcePath / (bUseCultureDirectory ? CultureName : TEXT(""));
		const FString CultureDestinationPath = DestinationPath / CultureName;
		ImportDialogueScriptForCulture(LocTextHelper, CultureSourcePath / DialogueScriptName, CultureName, false);
	}

	return 0;
}

bool UImportDialogueScriptCommandlet::ImportDialogueScriptForCulture(FLocTextHelper& InLocTextHelper, const FString& InDialogueScriptFileName, const FString& InCultureName, const bool bIsNativeCulture)
{
	// Load dialogue script file contents to string
	FString DialogScriptFileContents;
	if (!FFileHelper::LoadFileToString(DialogScriptFileContents, *InDialogueScriptFileName))
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Failed to load contents of dialog script file '%s' for culture '%s'."), *InDialogueScriptFileName, *InCultureName);
		return false;
	}

	// Parse dialogue script file contents
	const FCsvParser DialogScriptFileParser(DialogScriptFileContents);
	const FCsvParser::FRows& Rows = DialogScriptFileParser.GetRows();

	// Validate dialogue script row count
	if (Rows.Num() <= 0)
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Dialogue script file has insufficient rows for culture '%s'. Expected at least 1 row, got %d."), *InCultureName, Rows.Num());
		return false;
	}

	const UProperty* SpokenDialogueProperty = FDialogueScriptEntry::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FDialogueScriptEntry, SpokenDialogue));
	const UProperty* LocalizationKeysProperty = FDialogueScriptEntry::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FDialogueScriptEntry, LocalizationKeys));

	// We need the SpokenDialogue and LocalizationKeys properties in order to perform the import, so find their respective columns in the CSV data
	int32 SpokenDialogueColumnIndex = INDEX_NONE;
	int32 LocalizationKeysColumnIndex = INDEX_NONE;
	{
		const FString SpokenDialogueColumnName = SpokenDialogueProperty->GetName();
		const FString LocalizationKeysColumnName = LocalizationKeysProperty->GetName();

		const auto& HeaderRowData = Rows[0];
		for (int32 ColumnIndex = 0; ColumnIndex < HeaderRowData.Num(); ++ColumnIndex)
		{
			const TCHAR* const CellData = HeaderRowData[ColumnIndex];
			if (FCString::Stricmp(CellData, *SpokenDialogueColumnName) == 0)
			{
				SpokenDialogueColumnIndex = ColumnIndex;
			}
			else if (FCString::Stricmp(CellData, *LocalizationKeysColumnName) == 0)
			{
				LocalizationKeysColumnIndex = ColumnIndex;
			}

			if (SpokenDialogueColumnIndex != INDEX_NONE && LocalizationKeysColumnIndex != INDEX_NONE)
			{
				break;
			}
		}
	}

	if (SpokenDialogueColumnIndex == INDEX_NONE)
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Dialogue script file is missing the required column '%s' for culture '%s'."), *SpokenDialogueProperty->GetName(), *InCultureName);
		return false;
	}

	if (LocalizationKeysColumnIndex == INDEX_NONE)
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Dialogue script file is missing the required column '%s' for culture '%s'."), *LocalizationKeysProperty->GetName(), *InCultureName);
		return false;
	}

	bool bHasUpdatedArchive = false;

	// Parse each row of the CSV data
	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		const auto& RowData = Rows[RowIndex];

		FDialogueScriptEntry ParsedScriptEntry;

		// Parse the SpokenDialogue data
		{
			const TCHAR* const CellData = RowData[SpokenDialogueColumnIndex];
			if (SpokenDialogueProperty->ImportText(CellData, SpokenDialogueProperty->ContainerPtrToValuePtr<void>(&ParsedScriptEntry), PPF_None, nullptr) == nullptr)
			{
				UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Failed to parse the required column '%s' for row '%d' for culture '%s'."), *SpokenDialogueProperty->GetName(), RowIndex, *InCultureName);
				continue;
			}
		}

		// Parse the LocalizationKeys data
		{
			const TCHAR* const CellData = RowData[LocalizationKeysColumnIndex];
			if (LocalizationKeysProperty->ImportText(CellData, LocalizationKeysProperty->ContainerPtrToValuePtr<void>(&ParsedScriptEntry), PPF_None, nullptr) == nullptr)
			{
				UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Failed to parse the required column '%s' for row '%d' for culture '%s'."), *LocalizationKeysProperty->GetName(), RowIndex, *InCultureName);
				continue;
			}
		}

		for (const FString& ContextLocalizationKey : ParsedScriptEntry.LocalizationKeys)
		{
			// Find the manifest entry so that we can find the corresponding archive entry
			TSharedPtr<FManifestEntry> ContextManifestEntry = InLocTextHelper.FindSourceText(FDialogueConstants::DialogueNamespace, ContextLocalizationKey);
			if (!ContextManifestEntry.IsValid())
			{
				UE_LOG(LogImportDialogueScriptCommandlet, Log, TEXT("No internationalization manifest entry was found for context '%s' in culture '%s'. This context will be skipped."), *ContextLocalizationKey, *InCultureName);
				continue;
			}

			// Find the correct entry for our context
			const FManifestContext* ContextManifestEntryContext = ContextManifestEntry->FindContextByKey(ContextLocalizationKey);
			check(ContextManifestEntryContext); // This should never fail as we pass in the key to FindSourceText

			// Get the text we would have exported
			FLocItem ExportedSource;
			FLocItem ExportedTranslation;
			InLocTextHelper.GetExportText(InCultureName, FDialogueConstants::DialogueNamespace, ContextManifestEntryContext->Key, ContextManifestEntryContext->KeyMetadataObj, ELocTextExportSourceMethod::NativeText, ContextManifestEntry->Source, ExportedSource, ExportedTranslation);

			// Attempt to import the new text (if required)
			if (!ExportedTranslation.Text.Equals(ParsedScriptEntry.SpokenDialogue, ESearchCase::CaseSensitive))
			{
				if (InLocTextHelper.ImportTranslation(InCultureName, FDialogueConstants::DialogueNamespace, ContextManifestEntryContext->Key, ContextManifestEntryContext->KeyMetadataObj, ExportedSource, FLocItem(ParsedScriptEntry.SpokenDialogue), ContextManifestEntryContext->bIsOptional))
				{
					bHasUpdatedArchive = true;
				}
			}
		}
	}

	// Write out the updated archive file
	if (bHasUpdatedArchive)
	{
		FText SaveError;
		if (!InLocTextHelper.SaveArchive(InCultureName, &SaveError))
		{
			UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("%s"), *SaveError.ToString());
			return false;
		}
	}

	return true;
}
