// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetBundleData.h"
#include "AssetData.h"
#include "UObject/PropertyPortFlags.h"

bool FAssetBundleData::SetFromAssetData(const FAssetData& AssetData)
{
	FString TagValue;

	// Register that we're reading string assets for a specific package
	FSoftObjectPathSerializationScope SerializationScope(AssetData.PackageName, FAssetBundleData::StaticStruct()->GetFName(), ESoftObjectPathCollectType::AlwaysCollect);

	if (AssetData.GetTagValue(FAssetBundleData::StaticStruct()->GetFName(), TagValue))
	{
		if (FAssetBundleData::StaticStruct()->ImportText(*TagValue, this, nullptr, PPF_None, (FOutputDevice*)GWarn, AssetData.AssetName.ToString()))
		{
			FPrimaryAssetId FoundId = AssetData.GetPrimaryAssetId();

			if (FoundId.IsValid())
			{
				// Update the primary asset id if valid
				for (FAssetBundleEntry& Bundle : Bundles)
				{
					Bundle.BundleScope = FoundId;
				}
			}

			return true;
		}
	}
	return false;
}

FAssetBundleEntry* FAssetBundleData::FindEntry(const FPrimaryAssetId& SearchScope, FName SearchName)
{
	for (FAssetBundleEntry& Entry : Bundles)
	{
		if (Entry.BundleScope == SearchScope && Entry.BundleName == SearchName)
		{
			return &Entry;
		}
	}
	return nullptr;
}

void FAssetBundleData::AddBundleAsset(FName BundleName, const FSoftObjectPath& AssetPath)
{
	if (!AssetPath.IsValid())
	{
		return;
	}

	FAssetBundleEntry* FoundEntry = FindEntry(FPrimaryAssetId(), BundleName);

	if (!FoundEntry)
	{
		FoundEntry = new(Bundles) FAssetBundleEntry(FPrimaryAssetId(), BundleName);
	}

	FoundEntry->BundleAssets.AddUnique(AssetPath);
}

void FAssetBundleData::AddBundleAssets(FName BundleName, const TArray<FSoftObjectPath>& AssetPaths)
{
	FAssetBundleEntry* FoundEntry = FindEntry(FPrimaryAssetId(), BundleName);

	for (const FSoftObjectPath& Path : AssetPaths)
	{
		if (Path.IsValid())
		{
			// Only create if required
			if (!FoundEntry)
			{
				FoundEntry = new(Bundles) FAssetBundleEntry(FPrimaryAssetId(), BundleName);
			}

			FoundEntry->BundleAssets.AddUnique(Path);
		}
	}
}

void FAssetBundleData::SetBundleAssets(FName BundleName, TArray<FSoftObjectPath>&& AssetPaths)
{
	FAssetBundleEntry* FoundEntry = FindEntry(FPrimaryAssetId(), BundleName);

	if (!FoundEntry)
	{
		FoundEntry = new(Bundles) FAssetBundleEntry(FPrimaryAssetId(), BundleName);
	}

	FoundEntry->BundleAssets = AssetPaths;
}

void FAssetBundleData::Reset()
{
	Bundles.Reset();
}

bool FAssetBundleData::ExportTextItem(FString& ValueStr, FAssetBundleData const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	if (Bundles.Num() == 0)
	{
		// Empty, don't write anything to avoid it cluttering the asset registry tags
		return true;
	}
	// Not empty, do normal export
	return false;
}

bool FAssetBundleData::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	if (*Buffer != TEXT('('))
	{
		// Empty, don't read/write anything
		return true;
	}
	// Full structure, do normal parse
	return false;
}