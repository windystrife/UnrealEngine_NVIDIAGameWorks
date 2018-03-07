// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Overlays.h"

#include "LocalizedOverlays.generated.h"

class UBasicOverlays;
class UAssetImportData;

/**
 * Implements an asset that contains a set of Basic Overlays that will be displayed in accordance with
 * the current locale, or a default set if an appropriate locale is not found
 */
UCLASS(BlueprintType, hidecategories = (Object))
class OVERLAY_API ULocalizedOverlays
	: public UOverlays
{
	GENERATED_BODY()

public:

	/** The overlays to use if no overlays are found for the current culture */
	UPROPERTY(EditAnywhere, Category="Overlay Data")
	UBasicOverlays* DefaultOverlays;

	/**
	 * Maps a set of cultures to specific BasicOverlays assets.
	 * Cultures are comprised of three hyphen-separated parts:
	 *		A two-letter ISO 639-1 language code (e.g., "zh")
	 *		An optional four-letter ISO 15924 script code (e.g., "Hans")
	 *		An optional two-letter ISO 3166-1 country code  (e.g., "CN")
	 */
	UPROPERTY(EditAnywhere, Category="Overlay Data")
	TMap<FString, UBasicOverlays*> LocaleToOverlaysMap;

#if WITH_EDITORONLY_DATA

	/** The import data used to make this overlays asset */
	UPROPERTY(VisibleAnywhere, Instanced, Category="Import Settings")
	UAssetImportData* AssetImportData;

#endif	// WITH_EDITORONLY_DATA

public:

	//~ UOverlays interface

	virtual TArray<FOverlayItem> GetAllOverlays() const override;
	virtual void GetOverlaysForTime(const FTimespan& Time, TArray<FOverlayItem>& OutOverlays) const override;


public:

	//~ UObject interface

	virtual void PostInitProperties() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

private:
	
	/**
	 * Retrieves the overlays object for the current locale
	 */
	UBasicOverlays* GetCurrentLocaleOverlays() const;
};