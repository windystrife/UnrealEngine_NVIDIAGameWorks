// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AsyncActionLoadPrimaryAsset.h"
#include "Engine/AssetManager.h"

void UAsyncActionLoadPrimaryAssetBase::Activate()
{
	if (UAssetManager* Manager = UAssetManager::GetIfValid())
	{
		switch (Operation)
		{
		case EAssetManagerOperation::Load:
			LoadHandle = Manager->LoadPrimaryAssets(AssetsToLoad, LoadBundles);
			break;
		case EAssetManagerOperation::ChangeBundleStateMatching:
			LoadHandle = Manager->ChangeBundleStateForMatchingPrimaryAssets(LoadBundles, OldBundles);
			break;
		case EAssetManagerOperation::ChangeBundleStateList:
			LoadHandle = Manager->ChangeBundleStateForPrimaryAssets(AssetsToLoad, LoadBundles, OldBundles);
			break;
		}
		
		if (LoadHandle.IsValid())
		{
			if (!LoadHandle->HasLoadCompleted())
			{
				LoadHandle->BindCompleteDelegate(FStreamableDelegate::CreateUObject(this, &UAsyncActionLoadPrimaryAssetBase::HandleLoadCompleted));
				return;
			}
		}
	}

	// Either load already succeeded, or it failed
	HandleLoadCompleted();
}

void UAsyncActionLoadPrimaryAssetBase::HandleLoadCompleted()
{
	LoadHandle.Reset();
	SetReadyToDestroy();
}

UAsyncActionLoadPrimaryAsset* UAsyncActionLoadPrimaryAsset::AsyncLoadPrimaryAsset(FPrimaryAssetId PrimaryAsset, const TArray<FName>& LoadBundles)
{
	UAsyncActionLoadPrimaryAsset* Action = NewObject<UAsyncActionLoadPrimaryAsset>();
	Action->AssetsToLoad.Add(PrimaryAsset);
	Action->LoadBundles = LoadBundles;
	Action->Operation = EAssetManagerOperation::Load;

	return Action;
}

void UAsyncActionLoadPrimaryAsset::HandleLoadCompleted()
{
	UObject* AssetLoaded = nullptr;
	if (LoadHandle.IsValid())
	{
		AssetLoaded = LoadHandle->GetLoadedAsset();
	}

	Super::HandleLoadCompleted();
	Completed.Broadcast(AssetLoaded);
}

UAsyncActionLoadPrimaryAssetClass* UAsyncActionLoadPrimaryAssetClass::AsyncLoadPrimaryAssetClass(FPrimaryAssetId PrimaryAsset, const TArray<FName>& LoadBundles)
{
	UAsyncActionLoadPrimaryAssetClass* Action = NewObject<UAsyncActionLoadPrimaryAssetClass>();
	Action->AssetsToLoad.Add(PrimaryAsset);
	Action->LoadBundles = LoadBundles;
	Action->Operation = EAssetManagerOperation::Load;

	return Action;
}

void UAsyncActionLoadPrimaryAssetClass::HandleLoadCompleted()
{
	TSubclassOf<UObject> AssetLoaded = nullptr;
	if (LoadHandle.IsValid())
	{
		AssetLoaded = Cast<UClass>(LoadHandle->GetLoadedAsset());
	}

	Super::HandleLoadCompleted();
	Completed.Broadcast(AssetLoaded);
}

UAsyncActionLoadPrimaryAssetList* UAsyncActionLoadPrimaryAssetList::AsyncLoadPrimaryAssetList(const TArray<FPrimaryAssetId>& PrimaryAssetList, const TArray<FName>& LoadBundles)
{
	UAsyncActionLoadPrimaryAssetList* Action = NewObject<UAsyncActionLoadPrimaryAssetList>();
	Action->AssetsToLoad = PrimaryAssetList;
	Action->LoadBundles = LoadBundles;
	Action->Operation = EAssetManagerOperation::Load;

	return Action;
}

void UAsyncActionLoadPrimaryAssetList::HandleLoadCompleted()
{
	TArray<UObject*> AssetList;

	if (LoadHandle.IsValid())
	{
		LoadHandle->GetLoadedAssets(AssetList);
	}

	Super::HandleLoadCompleted();
	Completed.Broadcast(AssetList);
}

UAsyncActionLoadPrimaryAssetClassList* UAsyncActionLoadPrimaryAssetClassList::AsyncLoadPrimaryAssetClassList(const TArray<FPrimaryAssetId>& PrimaryAssetList, const TArray<FName>& LoadBundles)
{
	UAsyncActionLoadPrimaryAssetClassList* Action = NewObject<UAsyncActionLoadPrimaryAssetClassList>();
	Action->AssetsToLoad = PrimaryAssetList;
	Action->LoadBundles = LoadBundles;
	Action->Operation = EAssetManagerOperation::Load;

	return Action;
}

void UAsyncActionLoadPrimaryAssetClassList::HandleLoadCompleted()
{
	TArray<TSubclassOf<UObject>> AssetClassList;
	TArray<UObject*> AssetList;

	if (LoadHandle.IsValid())
	{
		LoadHandle->GetLoadedAssets(AssetList);

		for (UObject* LoadedAsset : AssetList)
		{
			UClass* LoadedClass = Cast<UClass>(LoadedAsset);

			if (LoadedClass)
			{
				AssetClassList.Add(LoadedClass);
			}
		}
	}

	Super::HandleLoadCompleted();
	Completed.Broadcast(AssetClassList);
}

UAsyncActionChangePrimaryAssetBundles* UAsyncActionChangePrimaryAssetBundles::AsyncChangeBundleStateForMatchingPrimaryAssets(const TArray<FName>& NewBundles, const TArray<FName>& OldBundles)
{
	UAsyncActionChangePrimaryAssetBundles* Action = NewObject<UAsyncActionChangePrimaryAssetBundles>();
	Action->LoadBundles = NewBundles;
	Action->OldBundles = OldBundles;
	Action->Operation = EAssetManagerOperation::ChangeBundleStateMatching;
	
	return Action;
}

UAsyncActionChangePrimaryAssetBundles* UAsyncActionChangePrimaryAssetBundles::AsyncChangeBundleStateForPrimaryAssetList(const TArray<FPrimaryAssetId>& PrimaryAssetList, const TArray<FName>& AddBundles, const TArray<FName>& RemoveBundles)
{
	UAsyncActionChangePrimaryAssetBundles* Action = NewObject<UAsyncActionChangePrimaryAssetBundles>();
	Action->LoadBundles = AddBundles;
	Action->OldBundles = RemoveBundles;
	Action->AssetsToLoad = PrimaryAssetList;
	Action->Operation = EAssetManagerOperation::ChangeBundleStateList;

	return Action;
}

void UAsyncActionChangePrimaryAssetBundles::HandleLoadCompleted()
{
	Super::HandleLoadCompleted();
	Completed.Broadcast();
}