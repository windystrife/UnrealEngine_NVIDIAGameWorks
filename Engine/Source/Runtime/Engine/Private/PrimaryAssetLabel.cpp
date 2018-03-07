// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/PrimaryAssetLabel.h"
#include "Engine/DataAsset.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Engine/AssetManager.h"
#include "AssetRegistryModule.h"

#if WITH_EDITOR
#include "CollectionManagerTypes.h"
#include "ICollectionManager.h"
#include "CollectionManagerModule.h"
#endif

const FName UPrimaryAssetLabel::DirectoryBundle = FName("Directory");
const FName UPrimaryAssetLabel::CollectionBundle = FName("Collection");

UPrimaryAssetLabel::UPrimaryAssetLabel()
{
	bLabelAssetsInMyDirectory = false;
	bIsRuntimeLabel = false;

	// By default have low priority and don't recurse
	Rules.bApplyRecursively = false;
	Rules.Priority = 0;
}

#if WITH_EDITORONLY_DATA
void UPrimaryAssetLabel::UpdateAssetBundleData()
{
	Super::UpdateAssetBundleData();

	if (!UAssetManager::IsValid())
	{
		return;
	}

	UAssetManager& Manager = UAssetManager::Get();
	IAssetRegistry& AssetRegistry = Manager.GetAssetRegistry();

	if (bLabelAssetsInMyDirectory)
	{
		FName PackagePath = FName(*FPackageName::GetLongPackagePath(GetOutermost()->GetName()));

		TArray<FAssetData> DirectoryAssets;
		AssetRegistry.GetAssetsByPath(PackagePath, DirectoryAssets, true);

		TArray<FSoftObjectPath> NewPaths;

		for (const FAssetData& AssetData : DirectoryAssets)
		{
			FSoftObjectPath AssetRef = Manager.GetAssetPathForData(AssetData);

			if (!AssetRef.IsNull())
			{
				NewPaths.Add(AssetRef);
			}
		}

		// Fast set, destroys NewPaths
		AssetBundleData.SetBundleAssets(DirectoryBundle, MoveTemp(NewPaths));
	}

	if (AssetCollection.CollectionName != NAME_None)
	{
		TArray<FSoftObjectPath> NewPaths;
		TArray<FName> CollectionAssets;
		ICollectionManager& CollectionManager = FCollectionManagerModule::GetModule().Get();
		CollectionManager.GetAssetsInCollection(AssetCollection.CollectionName, ECollectionShareType::CST_All, CollectionAssets);
		for (int32 Index = 0; Index < CollectionAssets.Num(); ++Index)
		{
			FAssetData FoundAsset = Manager.GetAssetRegistry().GetAssetByObjectPath(CollectionAssets[Index]);

			FSoftObjectPath AssetRef = Manager.GetAssetPathForData(FoundAsset);

			if (!AssetRef.IsNull())
			{
				NewPaths.Add(AssetRef);
			}
		}

		// Fast set, destroys NewPaths
		AssetBundleData.SetBundleAssets(CollectionBundle, MoveTemp(NewPaths));
	}
	
	// Update rules
	FPrimaryAssetId PrimaryAssetId = GetPrimaryAssetId();
	Manager.SetPrimaryAssetRules(PrimaryAssetId, Rules);
}
#endif
