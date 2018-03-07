// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Installer/ChunkReferenceTracker.h"
#include "Algo/Sort.h"
#include "Misc/ScopeLock.h"

DECLARE_LOG_CATEGORY_EXTERN(LogChunkReferenceTracker, Warning, All);
DEFINE_LOG_CATEGORY(LogChunkReferenceTracker);

namespace BuildPatchServices
{
	class FChunkReferenceTracker : public IChunkReferenceTracker
	{
	public:
		FChunkReferenceTracker(const FBuildPatchAppManifestRef& InstallManifest, const TSet<FString>& FilesToConstruct);
		FChunkReferenceTracker(const FBuildPatchAppManifestRef& InstallManifest);

		~FChunkReferenceTracker();

		// IChunkReferenceTracker interface begin.
		virtual TSet<FGuid> GetReferencedChunks() const override;
		virtual int32 GetReferenceCount(const FGuid& ChunkId) const override;
		virtual void SortByUseOrder(TArray<FGuid>& ChunkList, ESortDirection Direction) const override;
		virtual TArray<FGuid> GetNextReferences(int32 Count, const TFunction<bool(const FGuid&)>& SelectPredicate) const override;
		virtual bool PopReference(const FGuid& ChunkId) override;
		// IChunkReferenceTracker interface end.

	private:
		TMap<FGuid, int32> ReferenceCount;
		TArray<FGuid> UseStack;
		mutable FCriticalSection ThreadLockCs;
	};

	FChunkReferenceTracker::FChunkReferenceTracker(const FBuildPatchAppManifestRef& InstallManifest, const TSet<FString>& FilesToConstruct)
		: ReferenceCount()
		, UseStack()
		, ThreadLockCs()
	{
		// Create our full list of chunks, including dupe references, and track the reference count of each chunk.
		for (const FString& File : FilesToConstruct)
		{
			const FFileManifestData* NewFileManifest = InstallManifest->GetFileManifest(File);
			if (NewFileManifest != nullptr)
			{
				for (const FChunkPartData& ChunkPart : NewFileManifest->FileChunkParts)
				{
					ReferenceCount.FindOrAdd(ChunkPart.Guid)++;
					UseStack.Add(ChunkPart.Guid);
				}
			}
		}
		// Reverse the order of UseStack so it can be used as a stack.
		Algo::Reverse(UseStack);
		UE_LOG(LogChunkReferenceTracker, VeryVerbose, TEXT("Created. Total references:%d. Unique chunks:%d"), UseStack.Num(), ReferenceCount.Num());
	}

	FChunkReferenceTracker::FChunkReferenceTracker(const FBuildPatchAppManifestRef& InstallManifest)
		: ReferenceCount()
		, UseStack()
		, ThreadLockCs()
	{
		// Create our full list of chunks, no dupes, just one reference per chunk in the correct order.
		TArray<FString> AllFiles;
		TSet<FGuid> AllChunks;
		InstallManifest->GetFileList(AllFiles);
		for (const FString& File : AllFiles)
		{
			const FFileManifestData* NewFileManifest = InstallManifest->GetFileManifest(File);
			if (NewFileManifest != nullptr)
			{
				for (const FChunkPartData& ChunkPart : NewFileManifest->FileChunkParts)
				{
					bool bWasAlreadyInSet = false;
					AllChunks.Add(ChunkPart.Guid, &bWasAlreadyInSet);
					if (!bWasAlreadyInSet)
					{
						ReferenceCount.Add(ChunkPart.Guid, 1);
						UseStack.Add(ChunkPart.Guid);
					}
				}
			}
		}
		// Reverse the order of UseStack so it can be used as a stack.
		Algo::Reverse(UseStack);
		UE_LOG(LogChunkReferenceTracker, VeryVerbose, TEXT("Created. Total references:%d. Unique chunks:%d"), UseStack.Num(), ReferenceCount.Num());
	}

	FChunkReferenceTracker::~FChunkReferenceTracker()
	{
	}

	TSet<FGuid> FChunkReferenceTracker::GetReferencedChunks() const
	{
		// Thread lock to protect access to ReferenceCount.
		FScopeLock ThreadLock(&ThreadLockCs);
		TSet<FGuid> ReferencedChunks;
		for (const TPair<FGuid, int32>& Pair : ReferenceCount)
		{
			if (Pair.Value > 0)
			{
				ReferencedChunks.Add(Pair.Key);
			}
		}
		return ReferencedChunks;
	}

	int32 FChunkReferenceTracker::GetReferenceCount(const FGuid& ChunkId) const
	{
		// Thread lock to protect access to ReferenceCount.
		FScopeLock ThreadLock(&ThreadLockCs);
		return ReferenceCount.Contains(ChunkId) ? ReferenceCount[ChunkId] : 0;
	}

	void FChunkReferenceTracker::SortByUseOrder(TArray<FGuid>& ChunkList, ESortDirection Direction) const
	{
		// Thread lock to protect access to UseStack.
		FScopeLock ThreadLock(&ThreadLockCs);
		struct FIndexCache
		{
			FIndexCache(const TArray<FGuid>& InArray)
				: Array(InArray)
			{}

			int32 GetIndex(const FGuid& Id)
			{
				if (!IndexCache.Contains(Id))
				{
					IndexCache.Add(Id, Array.FindLast(Id));
				}
				return IndexCache[Id];
			}

			const TArray<FGuid>& Array;
			TMap<FGuid, int32> IndexCache;
		};
		FIndexCache ChunkUseIndexes(UseStack);
		switch (Direction)
		{
		case ESortDirection::Ascending:
			Algo::SortBy(ChunkList, [&ChunkUseIndexes](const FGuid& Id) { return ChunkUseIndexes.GetIndex(Id); }, TGreater<int32>());
			break;
		case ESortDirection::Descending:
			Algo::SortBy(ChunkList, [&ChunkUseIndexes](const FGuid& Id) { return ChunkUseIndexes.GetIndex(Id); }, TLess<int32>());
			break;
		}
	}

	TArray<FGuid> FChunkReferenceTracker::GetNextReferences(int32 Count, const TFunction<bool(const FGuid&)>& SelectPredicate) const
	{
		// Thread lock to protect access to UseStack.
		FScopeLock ThreadLock(&ThreadLockCs);
		TSet<FGuid> AddedIds;
		TArray<FGuid> NextReferences;
		for (int32 UseStackIdx = UseStack.Num() - 1; UseStackIdx >= 0 && Count > NextReferences.Num(); --UseStackIdx)
		{
			const FGuid& UseId = UseStack[UseStackIdx];
			if (AddedIds.Contains(UseId) == false && SelectPredicate(UseId))
			{
				AddedIds.Add(UseId);
				NextReferences.Add(UseId);
			}
		}
		return NextReferences;
	}

	bool FChunkReferenceTracker::PopReference(const FGuid& ChunkId)
	{
		// Thread lock to protect access to ReferenceCount and UseStack.
		FScopeLock ThreadLock(&ThreadLockCs);
		if (UseStack.Last() == ChunkId)
		{
			ReferenceCount[ChunkId]--;
			UseStack.Pop();
			return true;
		}
		return false;
	}

	IChunkReferenceTracker* FChunkReferenceTrackerFactory::Create(const FBuildPatchAppManifestRef& InstallManifest, const TSet<FString>& FilesToConstruct)
	{
		return new FChunkReferenceTracker(InstallManifest, FilesToConstruct);
	}

	IChunkReferenceTracker* FChunkReferenceTrackerFactory::Create(const FBuildPatchAppManifestRef& InstallManifest)
	{
		return new FChunkReferenceTracker(InstallManifest);
	}
}