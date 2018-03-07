// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/InstallChunkSource.h"
#include "Common/StatsCollector.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FMockInstallChunkSourceStat
		: public IInstallChunkSourceStat
	{
	public:
		typedef TTuple<double, FGuid> FLoadStarted;
		typedef TTuple<double, FGuid, ELoadResult> FLoadComplete;

	public:
		virtual void OnLoadStarted(const FGuid& ChunkId) override
		{
			if (OnLoadStartedFunc)
			{
				OnLoadStartedFunc(ChunkId);
			}
			RxLoadStarted.Emplace(FStatsCollector::GetSeconds(), ChunkId);
		}

		virtual void OnLoadComplete(const FGuid& ChunkId, ELoadResult Result) override
		{
			if (OnLoadCompleteFunc)
			{
				OnLoadCompleteFunc(ChunkId, Result);
			}
			RxLoadComplete.Emplace(FStatsCollector::GetSeconds(), ChunkId, Result);
		}

	public:
		TArray<FLoadStarted> RxLoadStarted;
		TArray<FLoadComplete> RxLoadComplete;
		TFunction<void(const FGuid&)> OnLoadStartedFunc;
		TFunction<void(const FGuid&, ELoadResult)> OnLoadCompleteFunc;
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
