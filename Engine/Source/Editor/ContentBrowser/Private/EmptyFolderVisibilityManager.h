// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetData.h"

/** Class that controls which empty folders should be visible in the Content Browser */
class FEmptyFolderVisibilityManager
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFolderPopulated, const FString& /*InPath*/);

	FEmptyFolderVisibilityManager();
	~FEmptyFolderVisibilityManager();

	/** Check to see whether the given path should be shown in the Content Browser */
	bool ShouldShowPath(const FString& InPath) const;

	/** Set whether the given path should always be shown, even if it's currently empty */
	void SetAlwaysShowPath(const FString& InPath);

	/** Delegate called when a folder is populated and should appear in the Content Browser */
	FOnFolderPopulated& OnFolderPopulated()
	{
		return OnFolderPopulatedDelegate;
	}

private:
	/** Handles updating the content browser when an asset path is removed from the asset registry */
	void OnAssetRegistryPathRemoved(const FString& InPath);

	/** Handles updating the content browser when an asset is added to the asset registry */
	void OnAssetRegistryAssetAdded(const FAssetData& InAssetData);

	/** Set set of paths that should always be shown, even if they're currently empty */
	TSet<FString> PathsToAlwaysShow;

	/** Delegate called when a folder is populated and should appear in the Content Browser */
	FOnFolderPopulated OnFolderPopulatedDelegate;
};
