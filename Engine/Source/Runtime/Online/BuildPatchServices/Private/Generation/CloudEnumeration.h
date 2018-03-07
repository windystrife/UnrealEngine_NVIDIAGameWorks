// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "Common/StatsCollector.h"

namespace BuildPatchServices
{
	class ICloudEnumeration
	{
	public:
		virtual TSet<FGuid> GetChunkSet(uint64 ChunkHash) const = 0;
		virtual TMap<uint64, TSet<FGuid>> GetChunkInventory() const = 0;
		virtual TMap<FGuid, int64> GetChunkFileSizes() const = 0;
		virtual TMap<FGuid, FSHAHash> GetChunkShaHashes() const = 0;
	};

	typedef TSharedRef<ICloudEnumeration, ESPMode::ThreadSafe> ICloudEnumerationRef;
	typedef TSharedPtr<ICloudEnumeration, ESPMode::ThreadSafe> ICloudEnumerationPtr;

	class FCloudEnumerationFactory
	{
	public:
		static ICloudEnumerationRef Create(const FString& CloudDirectory, const FDateTime& ManifestAgeThreshold, const FStatsCollectorRef& StatsCollector);
	};
}
