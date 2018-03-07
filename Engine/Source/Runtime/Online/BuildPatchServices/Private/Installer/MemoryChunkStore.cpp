// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Installer/MemoryChunkStore.h"
#include "Installer/ChunkEvictionPolicy.h"
#include "Data/ChunkData.h"
#include "Misc/ScopeLock.h"

namespace BuildPatchServices
{
	class FMemoryChunkStore
		: public IMemoryChunkStore
	{
	public:
		FMemoryChunkStore(int32 InStoreSize, IChunkEvictionPolicy* InEvictionPolicy, IChunkStore* InOverflowStore, IMemoryChunkStoreStat* InMemoryChunkStoreStat);
		~FMemoryChunkStore() {}

		// IChunkStore interface begin.
		virtual void Put(const FGuid& DataId, TUniquePtr<IChunkDataAccess> ChunkData) override;
		virtual IChunkDataAccess* Get(const FGuid& DataId) override;
		virtual TUniquePtr<IChunkDataAccess> Remove(const FGuid& DataId) override;
		virtual int32 GetSlack() const override;
		// IChunkStore interface end.

		// IMemoryChunkStore interface begin.
		virtual void DumpToOverflow() override;
		// IMemoryChunkStore interface end.

	private:
		void PutInternal(const FGuid& DataId, TUniquePtr<IChunkDataAccess> ChunkData);
		void UpdateStoreUsage() const;

	private:
		int32 StoreSize;
		TMap<FGuid, TUniquePtr<IChunkDataAccess>> Store;
		IChunkEvictionPolicy* EvictionPolicy;
		IChunkStore* OverflowStore;
		IMemoryChunkStoreStat* MemoryChunkStoreStat;
		FGuid LastGetId;
		TUniquePtr<IChunkDataAccess> LastGetData;
		mutable FCriticalSection ThreadLockCs;
	};

	FMemoryChunkStore::FMemoryChunkStore(int32 InStoreSize, IChunkEvictionPolicy* InEvictionPolicy, IChunkStore* InOverflowStore, IMemoryChunkStoreStat* InMemoryChunkStoreStat)
		: StoreSize(InStoreSize)
		, Store()
		, EvictionPolicy(InEvictionPolicy)
		, OverflowStore(InOverflowStore)
		, MemoryChunkStoreStat(InMemoryChunkStoreStat)
		, LastGetId()
		, LastGetData(nullptr)
		, ThreadLockCs()
	{
	}

	void FMemoryChunkStore::Put(const FGuid& DataId, TUniquePtr<IChunkDataAccess> ChunkData)
	{
		PutInternal(DataId, MoveTemp(ChunkData));
		MemoryChunkStoreStat->OnChunkStored(DataId);
	}

	IChunkDataAccess* FMemoryChunkStore::Get(const FGuid& DataId)
	{
		// Thread lock to protect access to Store, LastGetId, and LastGetData.
		FScopeLock ThreadLock(&ThreadLockCs);
		if (LastGetId != DataId)
		{
			// Put back last get.
			if (LastGetData.IsValid() && LastGetId.IsValid())
			{
				if (Store.Contains(LastGetId) == false)
				{
					PutInternal(LastGetId, MoveTemp(LastGetData));
				}
			}
			// Invalidate last get.
			LastGetId.Invalidate();
			LastGetData.Reset();
			// Retrieve requested data.
			if (Store.Contains(DataId))
			{
				LastGetData = MoveTemp(Store[DataId]);
				Store.Remove(DataId);
			}
			else if (OverflowStore != nullptr)
			{
				LastGetData = OverflowStore->Remove(DataId);
			}
			// Save ID if successful.
			if (LastGetData.IsValid())
			{
				LastGetId = DataId;
			}
		}
		return LastGetData.Get();
	}

	TUniquePtr<IChunkDataAccess> FMemoryChunkStore::Remove(const FGuid& DataId)
	{
		// Thread lock to protect access to Store, LastGetId, and LastGetData.
		FScopeLock ThreadLock(&ThreadLockCs);
		TUniquePtr<IChunkDataAccess> Rtn = nullptr;
		if (LastGetId == DataId)
		{
			LastGetId.Invalidate();
			Rtn = MoveTemp(LastGetData);
		}
		if (Store.Contains(DataId))
		{
			Rtn = MoveTemp(Store[DataId]);
			Store.Remove(DataId);
		}
		UpdateStoreUsage();
		return MoveTemp(Rtn);
	}

	int32 FMemoryChunkStore::GetSlack() const
	{
		// Thread lock to protect access to Store.
		FScopeLock ThreadLock(&ThreadLockCs);
		TSet<FGuid> Cleanable;
		TSet<FGuid> Bootable;
		EvictionPolicy->Query(Store, StoreSize, Cleanable, Bootable);
		return StoreSize - (Store.Num() - Cleanable.Num());
	}

	void FMemoryChunkStore::DumpToOverflow()
	{
		// Thread lock to protect access to Store.
		FScopeLock ThreadLock(&ThreadLockCs);
		if (OverflowStore != nullptr)
		{
			for (TPair<FGuid, TUniquePtr<IChunkDataAccess>>& Entry : Store)
			{
				OverflowStore->Put(Entry.Key, MoveTemp(Entry.Value));
			}
			if (LastGetData.IsValid())
			{
				OverflowStore->Put(LastGetId, MoveTemp(LastGetData));
			}
		}
		Store.Empty();
		LastGetData.Reset();
		LastGetId.Invalidate();
		UpdateStoreUsage();
	}

	void FMemoryChunkStore::PutInternal(const FGuid& DataId, TUniquePtr<IChunkDataAccess> ChunkData)
	{
		// Thread lock to protect access to Store, LastGetId, and LastGetData.
		FScopeLock ThreadLock(&ThreadLockCs);
		// Add this new chunk.
		Store.Add(DataId, MoveTemp(ChunkData));
		// Clean out our store.
		TSet<FGuid> Cleanable;
		TSet<FGuid> Bootable;
		EvictionPolicy->Query(Store, StoreSize, Cleanable, Bootable);
		// Perform clean.
		for (const FGuid& CleanId : Cleanable)
		{
			Store.Remove(CleanId);
			MemoryChunkStoreStat->OnChunkReleased(CleanId);
		}
		// Perform boot.
		for (const FGuid& BootId : Bootable)
		{
			if (OverflowStore != nullptr)
			{
				OverflowStore->Put(BootId, MoveTemp(Store[BootId]));
			}
			Store.Remove(BootId);
			MemoryChunkStoreStat->OnChunkBooted(BootId);
		}
		UpdateStoreUsage();
	}

	void FMemoryChunkStore::UpdateStoreUsage() const
	{
		const int32 LastGetCount = LastGetId.IsValid() && !Store.Contains(LastGetId);
		MemoryChunkStoreStat->OnStoreUseUpdated(Store.Num() + LastGetCount);
	}

	IMemoryChunkStore* FMemoryChunkStoreFactory::Create(int32 StoreSize, IChunkEvictionPolicy* EvictionPolicy, IChunkStore* OverflowStore, IMemoryChunkStoreStat* MemoryChunkStoreStat)
	{
		check(EvictionPolicy != nullptr);
		check(MemoryChunkStoreStat != nullptr);
		return new FMemoryChunkStore(StoreSize, EvictionPolicy, OverflowStore, MemoryChunkStoreStat);
	}
}
