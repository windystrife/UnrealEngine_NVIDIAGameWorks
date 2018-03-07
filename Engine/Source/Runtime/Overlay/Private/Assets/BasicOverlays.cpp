// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BasicOverlays.h"

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif

void UBasicOverlays::PostInitProperties()
{
#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif

	Super::PostInitProperties();
}

void UBasicOverlays::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
#if WITH_EDITORONLY_DATA
	if (AssetImportData)
	{
		OutTags.Add(FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden));
	}
#endif	// WITH_EDITORONLY_DATA

	Super::GetAssetRegistryTags(OutTags);
}

TArray<FOverlayItem> UBasicOverlays::GetAllOverlays() const
{
	return Overlays;
}

void UBasicOverlays::GetOverlaysForTime(const FTimespan& Time, TArray<FOverlayItem>& OutOverlays) const
{
	OutOverlays.Empty();

	for (const FOverlayItem& Overlay : Overlays)
	{
		if (Overlay.StartTime <= Time && Time < Overlay.EndTime)
		{
			OutOverlays.Add(Overlay);
		}
	}
}