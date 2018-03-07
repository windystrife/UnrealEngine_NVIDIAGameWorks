// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetData.h"
#include "PackageDependencyData.h"

class FDiskCachedAssetData
{
public:
	FDateTime Timestamp;
	TArray<FAssetData> AssetDataList;
	FPackageDependencyData DependencyData;

	FDiskCachedAssetData()
	{}

	FDiskCachedAssetData(const FDateTime& InTimestamp)
		: Timestamp(InTimestamp)
	{}

	/**
	 * Serialize as part of the registry cache. This is not meant to be serialized as part of a package so  it does not handle versions normally
	 * To version this data change FAssetRegistryVersion or CacheSerializationVersion
	 */
	void SerializeForCache(FArchive& Ar)
	{
		Ar << Timestamp;
	
		int32 AssetDataCount = AssetDataList.Num();
		Ar << AssetDataCount;

		if (Ar.IsLoading())
		{
			AssetDataList.SetNum(AssetDataCount);
		}

		for (int32 i = 0; i < AssetDataCount; i++)
		{
			AssetDataList[i].SerializeForCache(Ar);
		}

		DependencyData.SerializeForCache(Ar);
	}
};
