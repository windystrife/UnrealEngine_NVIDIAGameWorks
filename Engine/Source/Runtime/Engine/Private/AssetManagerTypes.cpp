// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/AssetManagerTypes.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"

bool FPrimaryAssetTypeInfo::FillRuntimeData()
{
	// Hot reload may have messed up asset pointer
	AssetBaseClass.ResetWeakPtr();
	AssetBaseClassLoaded = AssetBaseClass.LoadSynchronous();

	if (!ensureMsgf(AssetBaseClassLoaded, TEXT("Failed to load Primary Asset Type class %s!"), *AssetBaseClass.ToString()))
	{
		return false;
	}

	for (const FSoftObjectPath& AssetRef : SpecificAssets)
	{
		if (!AssetRef.IsNull())
		{
			AssetScanPaths.AddUnique(AssetRef.ToString());
		}
	}

	for (const FDirectoryPath& PathRef : Directories)
	{
		if (!PathRef.Path.IsEmpty())
		{
			AssetScanPaths.AddUnique(PathRef.Path);
		}
	}

	if (AssetScanPaths.Num() == 0)
	{
		// No scan locations picked out
		return false;
	}

	if (PrimaryAssetType == NAME_None)
	{
		// Invalid type
		return false;
	}

	return true;
}

bool FPrimaryAssetRules::IsDefault() const
{
	return *this == FPrimaryAssetRules();
}

void FPrimaryAssetRules::OverrideRules(const FPrimaryAssetRules& OverrideRules)
{
	static FPrimaryAssetRules DefaultRules;

	if (OverrideRules.Priority != DefaultRules.Priority)
	{
		Priority = OverrideRules.Priority;
	}

	if (OverrideRules.bApplyRecursively != DefaultRules.bApplyRecursively)
	{
		bApplyRecursively = OverrideRules.bApplyRecursively;
	}

	if (OverrideRules.ChunkId != DefaultRules.ChunkId)
	{
		ChunkId = OverrideRules.ChunkId;
	}

	if (OverrideRules.CookRule != DefaultRules.CookRule)
	{
		CookRule = OverrideRules.CookRule;
	}
}

void FPrimaryAssetRules::PropagateCookRules(const FPrimaryAssetRules& ParentRules)
{
	static FPrimaryAssetRules DefaultRules;

	if (ParentRules.ChunkId != DefaultRules.ChunkId && ChunkId == DefaultRules.ChunkId)
	{
		ChunkId = ParentRules.ChunkId;
	}

	if (ParentRules.CookRule != DefaultRules.CookRule && CookRule == DefaultRules.CookRule)
	{
		CookRule = ParentRules.CookRule;
	}
}

#if WITH_EDITOR
void UAssetManagerSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property && UAssetManager::IsValid())
	{
		UAssetManager::Get().ReinitializeFromConfig();
	}
}
#endif