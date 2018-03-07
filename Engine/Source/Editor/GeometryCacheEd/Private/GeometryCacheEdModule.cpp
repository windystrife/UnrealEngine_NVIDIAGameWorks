// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheEdModule.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "ComponentAssetBroker.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions_GeometryCache.h"
#include "GeometryCacheAssetBroker.h"
#include "GeometryCache.h"
#include "GeometryCacheComponent.h"
#include "GeometryCacheThumbnailRenderer.h"

IMPLEMENT_MODULE(FGeometryCacheEdModule, GeometryCacheEd)

void FGeometryCacheEdModule::StartupModule()
{
	FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();

	IAssetTools& AssetTools = AssetToolsModule.Get();
	AssetAction = new FAssetTypeActions_GeometryCache();
	AssetTools.RegisterAssetTypeActions(MakeShareable(AssetAction));

	AssetBroker = new FGeometryCacheAssetBroker();
	FComponentAssetBrokerage::RegisterBroker(MakeShareable(AssetBroker), UGeometryCacheComponent::StaticClass(), true, true);

	UThumbnailManager::Get().RegisterCustomRenderer(UGeometryCache::StaticClass(), UGeometryCacheThumbnailRenderer::StaticClass());
}

void FGeometryCacheEdModule::ShutdownModule()
{
	if (UObjectInitialized())
	{
		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
		IAssetTools& AssetTools = AssetToolsModule.Get();
		AssetTools.UnregisterAssetTypeActions(AssetAction->AsShared());
		FComponentAssetBrokerage::UnregisterBroker(MakeShareable(AssetBroker));
		UThumbnailManager::Get().UnregisterCustomRenderer(UGeometryCache::StaticClass());
	}
}
