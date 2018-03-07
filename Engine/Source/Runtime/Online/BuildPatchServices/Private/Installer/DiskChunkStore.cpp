// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Installer/DiskChunkStore.h"
#include "Data/ChunkData.h"
#include "Misc/ScopeLock.h"

namespace BuildPatchServices
{
	class FDiskChunkStore
		: public IDiskChunkStore
	{
	public:
		FDiskChunkStore(IChunkDataSerialization* InSerializer, IDiskChunkStoreStat* InDiskChunkStoreStat, FString InStoreRootPath);
		~FDiskChunkStore();

		// IChunkStore interface begin.
		virtual void Put(const FGuid& DataId, TUniquePtr<IChunkDataAccess> ChunkData) override;
		virtual IChunkDataAccess* Get(const FGuid& DataId) override;
		virtual TUniquePtr<IChunkDataAccess> Remove(const FGuid& DataId) override;
		virtual int32 GetSlack() const override;
		// IChunkStore interface end.

	private:
		FString GetChunkFilename(const FGuid& DataId) const;

	private:
		IChunkDataSerialization* Serializer;
		IDiskChunkStoreStat* DiskChunkStoreStat;
		FString StoreRootPath;
		mutable FCriticalSection ThreadLockCs;
		FGuid LastGetId;
		TUniquePtr<IChunkDataAccess> LastGetData;
		TSet<FGuid> PlacedInStore;
	};

	FDiskChunkStore::FDiskChunkStore(IChunkDataSerialization* InSerializer, IDiskChunkStoreStat* InDiskChunkStoreStat, FString InStoreRootPath)
		: Serializer(InSerializer)
		, DiskChunkStoreStat(InDiskChunkStoreStat)
		, StoreRootPath(InStoreRootPath)
		, ThreadLockCs()
		, LastGetId()
		, LastGetData(nullptr)
		, PlacedInStore()
	{
	}

	FDiskChunkStore::~FDiskChunkStore()
	{
	}

	void FDiskChunkStore::Put(const FGuid& DataId, TUniquePtr<IChunkDataAccess> ChunkData)
	{
		// Thread lock to protect access to PlacedInStore.
		FScopeLock ThreadLock(&ThreadLockCs);
		if (!PlacedInStore.Contains(DataId))
		{
			const FString ChunkFilename = GetChunkFilename(DataId);
			const EChunkSaveResult SaveResult = Serializer->SaveToFile(ChunkFilename, ChunkData.Get());
			if (SaveResult == EChunkSaveResult::Success)
			{
				PlacedInStore.Add(DataId);
				DiskChunkStoreStat->OnCacheUseUpdated(PlacedInStore.Num());
			}
			DiskChunkStoreStat->OnChunkStored(DataId, ChunkFilename, SaveResult);
		}
	}

	IChunkDataAccess* FDiskChunkStore::Get(const FGuid& DataId)
	{
		// Thread lock to protect access to PlacedInStore, LastGetId, and LastGetData.
		FScopeLock ThreadLock(&ThreadLockCs);
		// Load different chunk if necessary.
		IChunkDataAccess* ChunkData = nullptr;
		if (LastGetId != DataId)
		{
			if (PlacedInStore.Contains(DataId))
			{
				const FString ChunkFilename = GetChunkFilename(DataId);
				EChunkLoadResult LoadResult;
				ChunkData = Serializer->LoadFromFile(ChunkFilename, LoadResult);
				if (LoadResult == EChunkLoadResult::Success)
				{
					LastGetId = DataId;
					LastGetData.Reset(ChunkData);
				}
				else
				{
					PlacedInStore.Remove(DataId);
					DiskChunkStoreStat->OnCacheUseUpdated(PlacedInStore.Num());
				}
				DiskChunkStoreStat->OnChunkLoaded(DataId, ChunkFilename, LoadResult);
			}
		}
		else
		{
			ChunkData = LastGetData.Get();
		}
		return ChunkData;
	}

	TUniquePtr<IChunkDataAccess> FDiskChunkStore::Remove(const FGuid& DataId)
	{
		// Thread lock to protect access to LastGetId and LastGetData.
		FScopeLock ThreadLock(&ThreadLockCs);
		TUniquePtr<IChunkDataAccess> ReturnValue;
		if (LastGetId != DataId)
		{
			if (PlacedInStore.Contains(DataId))
			{
				const FString ChunkFilename = GetChunkFilename(DataId);
				EChunkLoadResult LoadResult;
				ReturnValue.Reset(Serializer->LoadFromFile(ChunkFilename, LoadResult));
				if (LoadResult != EChunkLoadResult::Success)
				{
					PlacedInStore.Remove(DataId);
					DiskChunkStoreStat->OnCacheUseUpdated(PlacedInStore.Num());
				}
				DiskChunkStoreStat->OnChunkLoaded(DataId, ChunkFilename, LoadResult);
			}
		}
		else
		{
			LastGetId.Invalidate();
			ReturnValue = MoveTemp(LastGetData);
		}
		return ReturnValue;
	}

	int32 FDiskChunkStore::GetSlack() const
	{
		// We are not configured with a max, so as per API spec, return max int32
		return MAX_int32;
	}

	FString FDiskChunkStore::GetChunkFilename(const FGuid& DataId) const
	{
		return StoreRootPath / DataId.ToString() + TEXT(".chunk");
	}

	IDiskChunkStore* FDiskChunkStoreFactory::Create(IChunkDataSerialization* Serializer, IDiskChunkStoreStat* DiskChunkStoreStat, FString StoreRootPath)
	{
		check(Serializer != nullptr);
		check(DiskChunkStoreStat != nullptr);
		check(StoreRootPath.IsEmpty() == false);
		return new FDiskChunkStore(Serializer, DiskChunkStoreStat, MoveTemp(StoreRootPath));
	}
}