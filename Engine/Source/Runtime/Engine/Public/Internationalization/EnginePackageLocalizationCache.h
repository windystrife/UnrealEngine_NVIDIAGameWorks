// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/PackageLocalizationCache.h"

struct FAssetData;

/** Implementation of a package localization cache that takes advantage of the asset registry. */
class ENGINE_API FEnginePackageLocalizationCache : public FPackageLocalizationCache
{
public:
	FEnginePackageLocalizationCache();
	virtual ~FEnginePackageLocalizationCache();

protected:
	//~ FPackageLocalizationCache interface
	virtual void FindLocalizedPackages(const FString& InSourceRoot, const FString& InLocalizedRoot, TMap<FName, TArray<FName>>& InOutSourcePackagesToLocalizedPackages) override;
	virtual void FindAssetGroupPackages(const FName InAssetGroupName, const FName InAssetClassName) override;

private:
	/**
	 * Callback handler for when a new asset is added to the asset registry.
	 *
	 * @param InAssetData		Data about the asset that was added.
	 */
	void HandleAssetAdded(const FAssetData& InAssetData);

	/**
	 * Callback handler for when an existing asset is removed from the asset registry.
	 *
	 * @param InAssetData		Data about the asset that was removed.
	 */
	void HandleAssetRemoved(const FAssetData& InAssetData);

	/**
	 * Callback handler for when an existing asset is renamed in the asset registry.
	 *
	 * @param InAssetData		Data about the asset under its new name.
	 * @param InOldObjectPath	The old name of the asset.
	 */
	void HandleAssetRenamed(const FAssetData& InAssetData, const FString& InOldObjectPath);

	/** True if we are currently within an asset registry ScanPathsSynchronous call. */
	bool bIsScanningPath;
};
