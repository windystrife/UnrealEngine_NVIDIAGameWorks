// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetData.h"

/** Represents a group of related assets, e.g. a skeleton, its animations and skeletal meshes */
class IAssetFamily
{
public:
	/** Virtual destructor */
	virtual ~IAssetFamily() {}

	/** Get all the asset classes this family supports (doesnt need to include derived classes) */
	virtual void GetAssetTypes(TArray<UClass*>& OutAssetTypes) const = 0;

	/** Find the most relevant asset of a specified type */
	virtual FAssetData FindAssetOfType(UClass* AssetType) const = 0;

	/** Find the most relevant asset of a specified type */
	template<typename AssetType>
	FAssetData GetAsset()
	{
		return FindAssetOfType(AssetType::StaticClass());
	}

	/** Find all assets of a specified type */
	virtual void FindAssetsOfType(UClass* AssetType, TArray<FAssetData>& OutAssets) const = 0;

	/** Find all assets of a specified type */
	template<typename AssetType>
	void GetAssets(TArray<FAssetData>& OutAssets)
	{
		FindAssetsOfType(AssetType::StaticClass(), OutAssets);
	}

	/** Gets the name of an asset that will be displayed to a user */
	virtual FText GetAssetTypeDisplayName(UClass* InAssetClass) const = 0;

	/** Check whether an asset is compatible with this family */
	virtual bool IsAssetCompatible(const FAssetData& InAssetData) const = 0;

	/** @return the outermost superclass of the passed-in class for this asset family */
	virtual UClass* GetAssetFamilyClass(UClass* InClass) const = 0;

	/** Record that an asset was opened */
	virtual void RecordAssetOpened(const FAssetData& InAssetData) = 0;

	/** Event fired when an asset is opened */
	DECLARE_EVENT_OneParam(IAssetFamily, FOnAssetOpened, UObject*)
	virtual FOnAssetOpened& GetOnAssetOpened() = 0;
};
