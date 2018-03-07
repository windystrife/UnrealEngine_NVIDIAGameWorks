// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TextLocalizationResourceGenerator.h"
#include "TextLocalizationResource.h"
#include "Misc/Paths.h"
#include "Internationalization/Culture.h"
#include "Misc/ConfigCacheIni.h"
#include "LocTextHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogTextLocalizationResourceGenerator, Log, All);

bool FTextLocalizationResourceGenerator::GenerateLocMeta(const FLocTextHelper& InLocTextHelper, const FString& InResourceName, FTextLocalizationMetaDataResource& OutLocMeta)
{
	// Populate the meta-data
	OutLocMeta.NativeCulture = InLocTextHelper.GetNativeCulture();
	OutLocMeta.NativeLocRes = OutLocMeta.NativeCulture / InResourceName;

	return true;
}

bool FTextLocalizationResourceGenerator::GenerateLocRes(const FLocTextHelper& InLocTextHelper, const FString& InCultureToGenerate, const bool bSkipSourceCheck, const FString& InLocResID, FTextLocalizationResource& OutLocRes)
{
	// Add each manifest entry to the LocRes file
	InLocTextHelper.EnumerateSourceTexts([&InLocTextHelper, &InCultureToGenerate, &bSkipSourceCheck, &InLocResID, &OutLocRes](TSharedRef<FManifestEntry> InManifestEntry) -> bool
	{
		// For each context, we may need to create a different or even multiple LocRes entries.
		for (const FManifestContext& Context : InManifestEntry->Contexts)
		{
			// Find the correct translation based upon the native source text
			FLocItem TranslationText;
			InLocTextHelper.GetRuntimeText(InCultureToGenerate, InManifestEntry->Namespace, Context.Key, Context.KeyMetadataObj, ELocTextExportSourceMethod::NativeText, InManifestEntry->Source, TranslationText, bSkipSourceCheck);

			// Add this entry to the LocRes
			FTextLocalizationResource::FKeysTable& KeyTable = OutLocRes.Namespaces.FindOrAdd(*InManifestEntry->Namespace);
			FTextLocalizationResource::FEntryArray& EntryArray = KeyTable.FindOrAdd(*Context.Key);
			FTextLocalizationResource::FEntry& NewEntry = EntryArray[EntryArray.AddDefaulted()];
			NewEntry.LocResID = InLocResID;
			NewEntry.SourceStringHash = FCrc::StrCrc32(*InManifestEntry->Source.Text);
			NewEntry.LocalizedString = TranslationText.Text;
		}

		return true; // continue enumeration
	}, true);

	OutLocRes.DetectAndLogConflicts();

	return true;
}

bool FTextLocalizationResourceGenerator::GenerateLocResAndUpdateLiveEntriesFromConfig(const FString& InConfigFilePath, const bool bSkipSourceCheck)
{
	FInternationalization& I18N = FInternationalization::Get();

	const FString SectionName = TEXT("RegenerateResources");

	// Get native culture.
	FString NativeCulture;
	if (!GConfig->GetString(*SectionName, TEXT("NativeCulture"), NativeCulture, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No native culture specified."));
		return false;
	}

	// Get source path.
	FString SourcePath;
	if (!GConfig->GetString(*SectionName, TEXT("SourcePath"), SourcePath, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No source path specified."));
		return false;
	}

	// Get destination path.
	FString DestinationPath;
	if (!GConfig->GetString(*SectionName, TEXT("DestinationPath"), DestinationPath, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No destination path specified."));
		return false;
	}

	// Get manifest name.
	FString ManifestName;
	if (!GConfig->GetString(*SectionName, TEXT("ManifestName"), ManifestName, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No manifest name specified."));
		return false;
	}

	// Get archive name.
	FString ArchiveName;
	if (!GConfig->GetString(*SectionName, TEXT("ArchiveName"), ArchiveName, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No archive name specified."));
		return false;
	}

	// Get resource name.
	FString ResourceName;
	if (!GConfig->GetString(*SectionName, TEXT("ResourceName"), ResourceName, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No resource name specified."));
		return false;
	}

	// Source path needs to be relative to Engine or Game directory
	const FString ConfigFullPath = FPaths::ConvertRelativePathToFull(InConfigFilePath);
	const FString EngineFullPath = FPaths::ConvertRelativePathToFull(FPaths::EngineConfigDir());
	const bool IsEngineManifest = ConfigFullPath.StartsWith(EngineFullPath);

	if (IsEngineManifest)
	{
		SourcePath = FPaths::Combine(*FPaths::EngineDir(), *SourcePath);
		DestinationPath = FPaths::Combine(*FPaths::EngineDir(), *DestinationPath);
	}
	else
	{
		SourcePath = FPaths::Combine(*FPaths::ProjectDir(), *SourcePath);
		DestinationPath = FPaths::Combine(*FPaths::ProjectDir(), *DestinationPath);
	}

	TArray<FString> CulturesToGenerate;
	{
		const FString CultureName = I18N.GetCurrentCulture()->GetName();
		const TArray<FString> PrioritizedCultures = I18N.GetPrioritizedCultureNames(CultureName);
		for (const FString& PrioritizedCulture : PrioritizedCultures)
		{
			if (FPaths::FileExists(SourcePath / PrioritizedCulture / ArchiveName))
			{
				CulturesToGenerate.Add(PrioritizedCulture);
			}
		}
	}

	if (CulturesToGenerate.Num() == 0)
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No cultures to generate were specified."));
		return false;
	}

	// Load the manifest and all archives
	FLocTextHelper LocTextHelper(SourcePath, ManifestName, ArchiveName, NativeCulture, CulturesToGenerate, nullptr);
	{
		FText LoadError;
		if (!LocTextHelper.LoadAll(ELocTextHelperLoadFlags::LoadOrCreate, &LoadError))
		{
			UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("%s"), *LoadError.ToString());
			return false;
		}
	}

	TArray<FTextLocalizationResource> TextLocalizationResources;
	for (const FString& CultureName : CulturesToGenerate)
	{
		const FString CulturePath = DestinationPath / CultureName;
		const FString ResourceFilePath = FPaths::ConvertRelativePathToFull(CulturePath / ResourceName);

		FTextLocalizationResource& LocRes = TextLocalizationResources[TextLocalizationResources.AddDefaulted()];
		if (!GenerateLocRes(LocTextHelper, CultureName, bSkipSourceCheck, ResourceFilePath, LocRes))
		{
			UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("Failed to generate localization resource for culture '%s'."), *CultureName);
			return false;
		}
	}
	FTextLocalizationManager::Get().UpdateFromLocalizationResources(TextLocalizationResources);

	return true;
}
