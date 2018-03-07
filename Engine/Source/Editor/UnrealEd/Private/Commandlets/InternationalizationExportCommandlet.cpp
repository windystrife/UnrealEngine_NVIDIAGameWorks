// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Commandlets/InternationalizationExportCommandlet.h"
#include "PortableObjectPipeline.h"
#include "UObjectGlobals.h"
#include "Package.h"
#include "Class.h"

DEFINE_LOG_CATEGORY_STATIC(LogInternationalizationExportCommandlet, Log, All);

/**
*	UInternationalizationExportCommandlet
*/
int32 UInternationalizationExportCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	// Set config
	FString ConfigPath;
	if (const FString* ConfigParamVal = ParamVals.Find(FString(TEXT("Config"))))
	{
		ConfigPath = *ConfigParamVal;
	}
	else
	{
		UE_LOG(LogInternationalizationExportCommandlet, Error, TEXT("No config specified."));
		return -1;
	}

	// Set config section
	FString SectionName;
	if (const FString* ConfigSectionParamVal = ParamVals.Find(FString(TEXT("Section"))))
	{
		SectionName = *ConfigSectionParamVal;
	}
	else
	{
		UE_LOG(LogInternationalizationExportCommandlet, Error, TEXT("No config section specified."));
		return -1;
	}

	// Get native culture.
	FString NativeCultureName;
	if (!GetStringFromConfig(*SectionName, TEXT("NativeCulture"), NativeCultureName, ConfigPath))
	{
		UE_LOG(LogInternationalizationExportCommandlet, Error, TEXT("No native culture specified."));
		return false;
	}

	// Get manifest name.
	FString ManifestName;
	if (!GetStringFromConfig(*SectionName, TEXT("ManifestName"), ManifestName, ConfigPath))
	{
		UE_LOG(LogInternationalizationExportCommandlet, Error, TEXT("No manifest name specified."));
		return false;
	}

	// Get archive name.
	FString ArchiveName;
	if (!GetStringFromConfig(*SectionName, TEXT("ArchiveName"), ArchiveName, ConfigPath))
	{
		UE_LOG(LogInternationalizationExportCommandlet, Error, TEXT("No archive name specified."));
		return false;
	}

	// Source path to the root folder that manifest/archive files live in.
	FString SourcePath;
	if (!GetPathFromConfig(*SectionName, TEXT("SourcePath"), SourcePath, ConfigPath))
	{
		UE_LOG(LogInternationalizationExportCommandlet, Error, TEXT("No source path specified."));
		return -1;
	}

	// Destination path that we will write files to.
	FString DestinationPath;
	if (!GetPathFromConfig(*SectionName, TEXT("DestinationPath"), DestinationPath, ConfigPath))
	{
		UE_LOG(LogInternationalizationExportCommandlet, Error, TEXT("No destination path specified."));
		return -1;
	}

	// Name of the file to read or write from.
	FString Filename;
	if (!GetStringFromConfig(*SectionName, TEXT("PortableObjectName"), Filename, ConfigPath))
	{
		UE_LOG(LogInternationalizationExportCommandlet, Error, TEXT("No portable object name specified."));
		return -1;
	}

	// Get cultures to generate.
	TArray<FString> CulturesToGenerate;
	if (GetStringArrayFromConfig(*SectionName, TEXT("CulturesToGenerate"), CulturesToGenerate, ConfigPath) == 0)
	{
		UE_LOG(LogInternationalizationExportCommandlet, Error, TEXT("No cultures specified for generation."));
		return -1;
	}

	// Get culture directory setting, default to true if not specified (used to allow picking of import directory with file open dialog from Translation Editor)
	bool bUseCultureDirectory = true;
	if (!GetBoolFromConfig(*SectionName, TEXT("bUseCultureDirectory"), bUseCultureDirectory, ConfigPath))
	{
		bUseCultureDirectory = true;
	}

	// Read in the text collapse mode to use
	ELocalizedTextCollapseMode TextCollapseMode = ELocalizedTextCollapseMode::IdenticalTextIdAndSource;
	{
		FString TextCollapseModeName;
		if (GetStringFromConfig(*SectionName, TEXT("LocalizedTextCollapseMode"), TextCollapseModeName, ConfigPath))
		{
			UEnum* LocalizedTextCollapseModeEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("ELocalizedTextCollapseMode"));
			const int64 TextCollapseModeInt = LocalizedTextCollapseModeEnum->GetValueByName(*TextCollapseModeName);
			if (TextCollapseModeInt != INDEX_NONE)
			{
				TextCollapseMode = (ELocalizedTextCollapseMode)TextCollapseModeInt;
			}
		}
	}

	bool bDoImport = false;
	GetBoolFromConfig(*SectionName, TEXT("bImportLoc"), bDoImport, ConfigPath);

	bool bDoExport = false;
	GetBoolFromConfig(*SectionName, TEXT("bExportLoc"), bDoExport, ConfigPath);

	if (!bDoImport && !bDoExport)
	{
		UE_LOG(LogInternationalizationExportCommandlet, Error, TEXT("Import/Export operation not detected.  Use bExportLoc or bImportLoc in config section."));
		return -1;
	}

	if (bDoImport)
	{
		// Load the manifest and all archives
		FLocTextHelper LocTextHelper(DestinationPath, ManifestName, ArchiveName, NativeCultureName, CulturesToGenerate, MakeShareable(new FLocFileSCCNotifies(SourceControlInfo)));
		{
			FText LoadError;
			if (!LocTextHelper.LoadAll(ELocTextHelperLoadFlags::LoadOrCreate, &LoadError))
			{
				UE_LOG(LogInternationalizationExportCommandlet, Error, TEXT("%s"), *LoadError.ToString());
				return false;
			}
		}

		// Import all PO files
		if (!PortableObjectPipeline::ImportAll(LocTextHelper, SourcePath, Filename, TextCollapseMode, bUseCultureDirectory))
		{
			UE_LOG(LogInternationalizationExportCommandlet, Error, TEXT("Failed to import localization files."));
			return -1;
		}
	}

	if (bDoExport)
	{
		bool bShouldPersistComments = false;
		GetBoolFromConfig(*SectionName, TEXT("ShouldPersistCommentsOnExport"), bShouldPersistComments, ConfigPath);

		// Load the manifest and all archives
		FLocTextHelper LocTextHelper(SourcePath, ManifestName, ArchiveName, NativeCultureName, CulturesToGenerate, MakeShareable(new FLocFileSCCNotifies(SourceControlInfo)));
		{
			FText LoadError;
			if (!LocTextHelper.LoadAll(ELocTextHelperLoadFlags::LoadOrCreate, &LoadError))
			{
				UE_LOG(LogInternationalizationExportCommandlet, Error, TEXT("%s"), *LoadError.ToString());
				return false;
			}
		}

		// Export all PO files
		if (!PortableObjectPipeline::ExportAll(LocTextHelper, DestinationPath, Filename, TextCollapseMode, bShouldPersistComments, bUseCultureDirectory))
		{
			UE_LOG(LogInternationalizationExportCommandlet, Error, TEXT("Failed to export localization files."));
			return -1;
		}
	}

	return 0;
}
