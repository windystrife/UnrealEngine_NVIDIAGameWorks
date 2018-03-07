// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/DiskChunkStore.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FMockDiskChunkStoreStat
		: public IDiskChunkStoreStat
	{
	public:
		typedef TTuple<double, FGuid, FString, EChunkSaveResult> FChunkStored;
		typedef TTuple<double, FGuid, FString, EChunkLoadResult> FChunkLoaded;
		typedef TTuple<double, int32> FCacheUseUpdated;

	public:
		virtual void OnChunkStored(const FGuid& ChunkId, const FString& ChunkFilename, EChunkSaveResult SaveResult) override
		{
			RxChunkStored.Emplace(FStatsCollector::GetSeconds(), ChunkId, ChunkFilename, SaveResult);
		}

		virtual void OnChunkLoaded(const FGuid& ChunkId, const FString& ChunkFilename, EChunkLoadResult LoadResult) override
		{
			RxChunkLoaded.Emplace(FStatsCollector::GetSeconds(), ChunkId, ChunkFilename, LoadResult);
		}

		virtual void OnCacheUseUpdated(int32 ChunkCount) override
		{
			RxCacheUseUpdated.Emplace(FStatsCollector::GetSeconds(), ChunkCount);
		}

	public:
		TArray<FChunkStored> RxChunkStored;
		TArray<FChunkLoaded> RxChunkLoaded;
		TArray<FCacheUseUpdated> RxCacheUseUpdated;
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
