// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/ChunkReferenceTracker.h"
#include "Algo/Sort.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FFakeChunkReferenceTracker
		: public IChunkReferenceTracker
	{
	public:
		virtual TSet<FGuid> GetReferencedChunks() const override
		{
			return ReferencedChunks;
		}

		virtual int32 GetReferenceCount(const FGuid& ChunkId) const override
		{
			return ReferenceCounts.FindRef(ChunkId);
		}

		virtual void SortByUseOrder(TArray<FGuid>& ChunkList, ESortDirection Direction) const override
		{
			switch (Direction)
			{
				case ESortDirection::Ascending:
					Algo::SortBy(ChunkList, [this](const FGuid& Element) { return NextReferences.IndexOfByKey(Element); }, TLess<int32>());
				break;
				case ESortDirection::Descending:
					Algo::SortBy(ChunkList, [this](const FGuid& Element) { return NextReferences.IndexOfByKey(Element); }, TGreater<int32>());
				break;
			}
		}

		virtual TArray<FGuid> GetNextReferences(int32 Count, const TFunction<bool(const FGuid&)>& SelectPredicate) const override
		{
			return NextReferences.FilterByPredicate([&Count, &SelectPredicate](const FGuid& Element)
			{
				bool bSelected = (Count > 0) && SelectPredicate(Element);
				if (bSelected)
				{
					--Count;
				}
				return bSelected;
			});
		}

		virtual bool PopReference(const FGuid& ChunkId)
		{
			if (NextReferences.Num() && NextReferences[0] == ChunkId)
			{
				NextReferences.RemoveAt(0);
				return true;
			}
			return false;
		}

	public:
		TSet<FGuid> ReferencedChunks;
		TMap<FGuid, int32> ReferenceCounts;
		TArray<FGuid> NextReferences;
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
