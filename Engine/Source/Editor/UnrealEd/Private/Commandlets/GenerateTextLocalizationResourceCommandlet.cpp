// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Commandlets/GenerateTextLocalizationResourceCommandlet.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Templates/ScopedPointer.h"
#include "TextLocalizationResource.h"
#include "TextLocalizationResourceGenerator.h"
#include "UniquePtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogGenerateTextLocalizationResourceCommandlet, Log, All);

UGenerateTextLocalizationResourceCommandlet::UGenerateTextLocalizationResourceCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UGenerateTextLocalizationResourceCommandlet::Main(const FString& Params)
{
	// Parse command line - we're interested in the param vals
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	// Set config file.
	const FString* ParamVal = ParamVals.Find(FString(TEXT("Config")));
	FString GatherTextConfigPath;

	if ( ParamVal )
	{
		GatherTextConfigPath = *ParamVal;
	}
	else
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No config specified."));
		return -1;
	}

	// Set config section.
	ParamVal = ParamVals.Find(FString(TEXT("Section")));
	FString SectionName;

	if ( ParamVal )
	{
		SectionName = *ParamVal;
	}
	else
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No config section specified."));
		return -1;
	}

	// Get source path.
	FString SourcePath;
	if( !( GetPathFromConfig( *SectionName, TEXT("SourcePath"), SourcePath, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No source path specified."));
		return -1;
	}

	// Get manifest name.
	FString ManifestName;
	if( !( GetStringFromConfig( *SectionName, TEXT("ManifestName"), ManifestName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No manifest name specified."));
		return -1;
	}

	// Get archive name.
	FString ArchiveName;
	if (!GetStringFromConfig(*SectionName, TEXT("ArchiveName"), ArchiveName, GatherTextConfigPath))
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No archive name specified."));
		return -1;
	}

	// Get cultures to generate.
	FString NativeCultureName;
	if( !( GetStringFromConfig( *SectionName, TEXT("NativeCulture"), NativeCultureName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No native culture specified."));
		return -1;
	}

	// Get cultures to generate.
	TArray<FString> CulturesToGenerate;
	GetStringArrayFromConfig( *SectionName, TEXT("CulturesToGenerate"), CulturesToGenerate, GatherTextConfigPath );

	if( CulturesToGenerate.Num() == 0 )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No cultures specified for generation."));
		return -1;
	}

	for(int32 i = 0; i < CulturesToGenerate.Num(); ++i)
	{
		if( FInternationalization::Get().GetCulture( CulturesToGenerate[i] ).IsValid() )
		{
			UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Verbose, TEXT("Specified culture is not a valid runtime culture, but may be a valid base language: %s"), *(CulturesToGenerate[i]));
		}
	}

	// Get destination path.
	FString DestinationPath;
	if( !( GetPathFromConfig( *SectionName, TEXT("DestinationPath"), DestinationPath, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No destination path specified."));
		return -1;
	}

	// Get resource name.
	FString ResourceName;
	if( !( GetStringFromConfig( *SectionName, TEXT("ResourceName"), ResourceName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No resource name specified."));
		return -1;
	}

	// Get whether to skip the source check.
	bool bSkipSourceCheck = false;
	if (!GetBoolFromConfig(*SectionName, TEXT("bSkipSourceCheck"), bSkipSourceCheck, GatherTextConfigPath))
	{
		bSkipSourceCheck = false;
	}

	// Load the manifest and all archives
	FLocTextHelper LocTextHelper(SourcePath, ManifestName, ArchiveName, NativeCultureName, CulturesToGenerate, MakeShareable(new FLocFileSCCNotifies(SourceControlInfo)));
	{
		FText LoadError;
		if (!LocTextHelper.LoadAll(ELocTextHelperLoadFlags::LoadOrCreate, &LoadError))
		{
			UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("%s"), *LoadError.ToString());
			return false;
		}
	}

	// Generate the LocMeta file for all cultures
	{
		const FString TextLocalizationMetaDataResourcePath = DestinationPath / FPaths::GetBaseFilename(ResourceName) + TEXT(".locmeta");

		const bool bLocMetaFileSaved = FLocalizedAssetSCCUtil::SaveFileWithSCC(SourceControlInfo, TextLocalizationMetaDataResourcePath, [&LocTextHelper, &ResourceName](const FString& InSaveFileName) -> bool
		{
			FTextLocalizationMetaDataResource LocMeta;
			return FTextLocalizationResourceGenerator::GenerateLocMeta(LocTextHelper, ResourceName, LocMeta) && LocMeta.SaveToFile(InSaveFileName);
		});

		if (!bLocMetaFileSaved)
		{
			UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("Could not write file %s"), *TextLocalizationMetaDataResourcePath);
			return false;
		}
	}

	// Generate the LocRes file for each culture
	for (const FString& CultureName : CulturesToGenerate)
	{
		const FString TextLocalizationResourcePath = DestinationPath / CultureName / ResourceName;

		const bool bLocResFileSaved = FLocalizedAssetSCCUtil::SaveFileWithSCC(SourceControlInfo, TextLocalizationResourcePath, [&LocTextHelper, &CultureName, &bSkipSourceCheck](const FString& InSaveFileName) -> bool
		{
			FTextLocalizationResource LocRes;
			return FTextLocalizationResourceGenerator::GenerateLocRes(LocTextHelper, CultureName, bSkipSourceCheck, InSaveFileName, LocRes) && LocRes.SaveToFile(InSaveFileName);
		});

		if (!bLocResFileSaved)
		{
			UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("Could not write file %s"), *TextLocalizationResourcePath);
			return false;
		}
	}

	return 0;
}
