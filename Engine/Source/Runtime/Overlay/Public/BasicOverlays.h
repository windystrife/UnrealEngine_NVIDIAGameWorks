// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Overlays.h"

#include "BasicOverlays.generated.h"

class UAssetImportData;

/**
 * Implements an asset that contains a set of overlay data (which includes timing, text, and position) to be displayed for any
 * given source (including, but not limited to, audio, dialog, and movies)
 */
UCLASS(BlueprintType, hidecategories = (Object))
class OVERLAY_API UBasicOverlays
	: public UOverlays
{
	GENERATED_BODY()

public:

	/** The overlay data held by this asset. Contains info on timing, position, and the subtitle to display */
	UPROPERTY(EditAnywhere, Category="Overlay Data")
	TArray<FOverlayItem> Overlays;

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
	// End UObject interface

private:

};