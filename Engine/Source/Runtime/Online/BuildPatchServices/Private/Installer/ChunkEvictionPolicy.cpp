// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Installer/ChunkEvictionPolicy.h"
#include "Installer/ChunkReferenceTracker.h"

namespace BuildPatchServices
{
	class FChunkEvictionPolicy : public IChunkEvictionPolicy
	{
	public:
		FChunkEvictionPolicy(IChunkReferenceTracker* InChunkReferenceTracker);

		~FChunkEvictionPolicy();

		// IChunkEvictionPolicy interface begin.
		virtual void Query(const TMap<FGuid, TUniquePtr<IChunkDataAccess>>& CurrentMap, int32 DesiredMax, TSet<FGuid>& OutCleanable, TSet<FGuid>& OutBootable) const override;
		// IChunkEvictionPolicy interface end.

	private:
		IChunkReferenceTracker* ChunkReferenceTracker;
	};

	FChunkEvictionPolicy::FChunkEvictionPolicy(IChunkReferenceTracker* InChunkReferenceTracker)
		: ChunkReferenceTracker(InChunkReferenceTracker)
	{
	}

	FChunkEvictionPolicy::~FChunkEvictionPolicy()
	{
	}

	void FChunkEvictionPolicy::Query(const TMap<FGuid, TUniquePtr<IChunkDataAccess>>& CurrentMap, int32 DesiredMax, TSet<FGuid>& OutCleanable, TSet<FGuid>& OutBootable) const
	{
		for (const TPair<FGuid, TUniquePtr<IChunkDataAccess>>& Pair : CurrentMap)
		{
			if (ChunkReferenceTracker->GetReferenceCount(Pair.Key) == 0)
			{
				OutCleanable.Add(Pair.Key);
			}
		}
		int32 BootsNeeded = (CurrentMap.Num() - OutCleanable.Num()) - DesiredMax;
		if (BootsNeeded > 0)
		{
			TArray<FGuid> ChunkUseOrder;
			for (const TPair<FGuid, TUniquePtr<IChunkDataAccess>>& Pair : CurrentMap)
			{
				if (!OutCleanable.Contains(Pair.Key))
				{
					ChunkUseOrder.Add(Pair.Key);
				}
			}
			ChunkReferenceTracker->SortByUseOrder(ChunkUseOrder, IChunkReferenceTracker::ESortDirection::Descending);
			for (int32 ChunkUseOrderIdx = 0; ChunkUseOrderIdx < ChunkUseOrder.Num() && BootsNeeded > 0; ++ChunkUseOrderIdx)
			{
				--BootsNeeded;
				OutBootable.Add(ChunkUseOrder[ChunkUseOrderIdx]);
			}
		}
	}

	IChunkEvictionPolicy* FChunkEvictionPolicyFactory::Create(IChunkReferenceTracker* ChunkReferenceTracker)
	{
		check(ChunkReferenceTracker != nullptr);
		return new FChunkEvictionPolicy(ChunkReferenceTracker);
	}
}
