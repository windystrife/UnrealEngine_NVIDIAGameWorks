// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"

class FLocTextHelper;
class FTextLocalizationMetaDataResource;
class FTextLocalizationResource;

/** Utility functions for generating compiled LocMeta (Localization MetaData Resource) and LocRes (Localization Resource) files from source localization data */
class FTextLocalizationResourceGenerator
{
public:
	/**
	 * Given a loc text helper, generate a compiled LocMeta resource.
	 */
	LOCALIZATION_API static bool GenerateLocMeta(const FLocTextHelper& InLocTextHelper, const FString& InResourceName, FTextLocalizationMetaDataResource& OutLocMeta);

	/**
	 * Given a loc text helper, generate a compiled LocRes resource for the given culture.
	 */
	LOCALIZATION_API static bool GenerateLocRes(const FLocTextHelper& InLocTextHelper, const FString& InCultureToGenerate, const bool bSkipSourceCheck, const FString& InLocResID, FTextLocalizationResource& OutLocRes);

	/**
	 * Given a config file, generate a compiled LocRes resource for the active culture and use it to update the live-entries in the localization manager.
	 */
	LOCALIZATION_API static bool GenerateLocResAndUpdateLiveEntriesFromConfig(const FString& InConfigFilePath, const bool bSkipSourceCheck);
};
