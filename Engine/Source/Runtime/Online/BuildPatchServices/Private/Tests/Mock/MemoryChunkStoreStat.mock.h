// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/MemoryChunkStore.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FMockMemoryChunkStoreStat
		: public IMemoryChunkStoreStat
	{
	public:
		typedef TTuple<double, FGuid> FChunkStored;
		typedef TTuple<double, FGuid> FChunkReleased;
		typedef TTuple<double, FGuid> FChunkBooted;
		typedef TTuple<double, int32> FStoreUseUpdated;

	public:
		virtual void OnChunkStored(const FGuid& ChunkId) override
		{
			RxChunkStored.Emplace(FStatsCollector::GetSeconds(), ChunkId);
		}

		virtual void OnChunkReleased(const FGuid& ChunkId) override
		{
			RxChunkReleased.Emplace(FStatsCollector::GetSeconds(), ChunkId);
		}

		virtual void OnChunkBooted(const FGuid& ChunkId) override
		{
			RxChunkBooted.Emplace(FStatsCollector::GetSeconds(), ChunkId);
		}

		virtual void OnStoreUseUpdated(int32 ChunkCount) override
		{
			RxStoreUseUpdated.Emplace(FStatsCollector::GetSeconds(), ChunkCount);
		}
	public:
		TArray<FChunkStored> RxChunkStored;
		TArray<FChunkReleased> RxChunkReleased;
		TArray<FChunkBooted> RxChunkBooted;
		TArray<FStoreUseUpdated> RxStoreUseUpdated;
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
