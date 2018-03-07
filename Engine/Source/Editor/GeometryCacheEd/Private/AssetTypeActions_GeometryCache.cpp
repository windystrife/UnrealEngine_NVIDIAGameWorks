// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_GeometryCache.h"
#include "EditorFramework/AssetImportData.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "GeometryCache.h"

FText FAssetTypeActions_GeometryCache::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_GeometryCache", "GeometryCache");
}

FColor FAssetTypeActions_GeometryCache::GetTypeColor() const
{
	return FColor(0, 255, 255);
}

UClass* FAssetTypeActions_GeometryCache::GetSupportedClass() const
{
	return UGeometryCache::StaticClass();
}

bool FAssetTypeActions_GeometryCache::HasActions(const TArray<UObject*>& InObjects) const
{
	return false;
}

void FAssetTypeActions_GeometryCache::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	FAssetTypeActions_Base::GetActions(InObjects, MenuBuilder);
}

void FAssetTypeActions_GeometryCache::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor /*= TSharedPtr<IToolkitHost>()*/)
{
	FAssetTypeActions_Base::OpenAssetEditor(InObjects, EditWithinLevelEditor);
}

uint32 FAssetTypeActions_GeometryCache::GetCategories()
{
	return EAssetTypeCategories::Animation;
}

class UThumbnailInfo* FAssetTypeActions_GeometryCache::GetThumbnailInfo(UObject* Asset) const
{
	UGeometryCache* GeometryCache = CastChecked<UGeometryCache>(Asset);
	UThumbnailInfo* ThumbnailInfo = GeometryCache->ThumbnailInfo;
	if (ThumbnailInfo == NULL)
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(GeometryCache, NAME_None, RF_Transactional);
		GeometryCache->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

bool FAssetTypeActions_GeometryCache::IsImportedAsset() const
{
	return true;
}

void FAssetTypeActions_GeometryCache::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto GeometryCache = CastChecked<UGeometryCache>(Asset);
		GeometryCache->AssetImportData->ExtractFilenames(OutSourceFilePaths);
	}
}
