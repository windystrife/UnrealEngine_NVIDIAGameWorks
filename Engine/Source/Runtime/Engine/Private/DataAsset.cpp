// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/DataAsset.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Engine/AssetManager.h"
UDataAsset::UDataAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NativeClass = GetClass();
}

#if WITH_EDITORONLY_DATA
void UDataAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading() && (Ar.UE4Ver() < VER_UE4_ADD_TRANSACTIONAL_TO_DATA_ASSETS))
	{
		SetFlags(RF_Transactional);
	}
}

void UPrimaryDataAsset::UpdateAssetBundleData()
{
	AssetBundleData.Reset();

	// By default parse the metadata
	if (UAssetManager::IsValid())
	{
		UAssetManager::Get().InitializeAssetBundlesFromMetadata(this, AssetBundleData);
	}
}

void UPrimaryDataAsset::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);

	UpdateAssetBundleData();

	if (UAssetManager::IsValid())
	{
		// Bundles may have changed, refresh
		UAssetManager::Get().RefreshAssetData(this);
	}
}
#endif

FPrimaryAssetId UPrimaryDataAsset::GetPrimaryAssetId() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		UClass* SearchNativeClass = GetClass();

		while (SearchNativeClass && !SearchNativeClass->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic))
		{
			SearchNativeClass = SearchNativeClass->GetSuperClass();
		}

		if (SearchNativeClass && SearchNativeClass != GetClass())
		{
			// If blueprint, return native class and asset name
			return FPrimaryAssetId(SearchNativeClass->GetFName(), FPackageName::GetShortFName(GetOutermost()->GetFName()));
		}
		
		// Native CDO, return nothing
		return FPrimaryAssetId();
	}

	// Data assets use Class and ShortName by default, there's no inheritance so class works fine
	return FPrimaryAssetId(GetClass()->GetFName(), GetFName());
}

void UPrimaryDataAsset::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	FAssetBundleData OldData = AssetBundleData;
	
	UpdateAssetBundleData();

	if (UAssetManager::IsValid() && OldData != AssetBundleData)
	{
		// Bundles changed, refresh
		UAssetManager::Get().RefreshAssetData(this);
	}
#endif
}
