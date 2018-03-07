// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MallocBinned.cpp: Binned memory allocator
=============================================================================*/

#include "HAL/MallocBinned.h"
#include "Misc/ScopeLock.h"
#include "Misc/BufferedOutputDevice.h"

#include "HAL/MemoryMisc.h"

/** Malloc binned allocator specific stats. */
DEFINE_STAT(STAT_Binned_OsCurrent);
DEFINE_STAT(STAT_Binned_OsPeak);
DEFINE_STAT(STAT_Binned_WasteCurrent);
DEFINE_STAT(STAT_Binned_WastePeak);
DEFINE_STAT(STAT_Binned_UsedCurrent);
DEFINE_STAT(STAT_Binned_UsedPeak);
DEFINE_STAT(STAT_Binned_CurrentAllocs);
DEFINE_STAT(STAT_Binned_TotalAllocs);
DEFINE_STAT(STAT_Binned_SlackCurrent);

/** Information about a piece of free memory. 8 bytes */
struct FMallocBinned::FFreeMem
{
	/** Next or MemLastPool[], always in order by pool. */
	FFreeMem*	Next;
	/** Number of consecutive free blocks here, at least 1. */
	uint32		NumFreeBlocks;
};

// Memory pool info. 32 bytes.
struct FMallocBinned::FPoolInfo
{
	/** Number of allocated elements in this pool, when counts down to zero can free the entire pool. */
	uint16			Taken;		// 2
	/** Index of pool. Index into MemSizeToPoolTable[]. Valid when < MAX_POOLED_ALLOCATION_SIZE, MAX_POOLED_ALLOCATION_SIZE is OsTable.
		When AllocSize is 0, this is the number of pages to step back to find the base address of an allocation. See FindPoolInfoInternal()
	*/
	uint16			TableIndex; // 4		
	/** Number of bytes allocated */
	uint32			AllocSize;	// 8
	/** Pointer to first free memory in this pool or the OS Allocation Size in bytes if this allocation is not binned*/
	FFreeMem*		FirstMem;   // 12/16
	FPoolInfo*		Next;		// 16/24
	FPoolInfo**		PrevLink;	// 20/32
#if PLATFORM_32BITS
	/** Explicit padding for 32 bit builds */
	uint8 Padding[12]; // 32
#endif

	void SetAllocationSizes( uint32 InBytes, UPTRINT InOsBytes, uint32 InTableIndex, uint32 SmallAllocLimt )
	{
		TableIndex=InTableIndex;
		AllocSize=InBytes;
		if (TableIndex == SmallAllocLimt)
		{
			FirstMem=(FFreeMem*)InOsBytes;
		}
	}

	uint32 GetBytes() const
	{
		return AllocSize;
	}

	UPTRINT GetOsBytes( uint32 InPageSize, uint32 SmallAllocLimt ) const
	{
		if (TableIndex == SmallAllocLimt)
		{
			return (UPTRINT)FirstMem;
		}
		else
		{
			return Align(AllocSize, InPageSize);
		}
	}

	void Link( FPoolInfo*& Before )
	{
		if( Before )
		{
			Before->PrevLink = &Next;
		}
		Next     = Before;
		PrevLink = &Before;
		Before   = this;
	}

	void Unlink()
	{
		if( Next )
		{
			Next->PrevLink = PrevLink;
		}
		*PrevLink = Next;
	}
};

/** Hash table struct for retrieving allocation book keeping information */
struct FMallocBinned::PoolHashBucket
{
	UPTRINT			Key;
	FPoolInfo*		FirstPool;
	PoolHashBucket* Prev;
	PoolHashBucket* Next;

	PoolHashBucket()
	{
		Key=0;
		FirstPool=nullptr;
		Prev=this;
		Next=this;
	}

	void Link( PoolHashBucket* After )
	{
		Link(After, Prev, this);
	}

	static void Link( PoolHashBucket* Node, PoolHashBucket* Before, PoolHashBucket* After )
	{
		Node->Prev=Before;
		Node->Next=After;
		Before->Next=Node;
		After->Prev=Node;
	}

	void Unlink()
	{
		Next->Prev = Prev;
		Prev->Next = Next;
		Prev=this;
		Next=this;
	}
};

struct FMallocBinned::Private
{
	/** Default alignment for binned allocator */
	enum { DEFAULT_BINNED_ALLOCATOR_ALIGNMENT = sizeof(FFreeMem) };
	enum { PAGE_SIZE_LIMIT = 65536 };
	// BINNED_ALLOC_POOL_SIZE can be increased beyond 64k to cause binned malloc to allocate
	// the small size bins in bigger chunks. If OS Allocation is slow, increasing
	// this number *may* help performance but YMMV.
	enum { BINNED_ALLOC_POOL_SIZE = 65536 }; 

	// Implementation. 
	static CA_NO_RETURN void OutOfMemory(uint64 Size, uint32 Alignment=0)
	{
		// this is expected not to return
		FPlatformMemory::OnOutOfMemory(Size, Alignment);
	}

	static FORCEINLINE void TrackStats(FPoolTable* Table, SIZE_T Size)
	{
#if STATS
		// keep track of memory lost to padding
		Table->TotalWaste += Table->BlockSize - Size;
		Table->TotalRequests++;
		Table->ActiveRequests++;
		Table->MaxActiveRequests = FMath::Max(Table->MaxActiveRequests, Table->ActiveRequests);
		Table->MaxRequest = Size > Table->MaxRequest ? Size : Table->MaxRequest;
		Table->MinRequest = Size < Table->MinRequest ? Size : Table->MinRequest;
#endif
	}

	/** 
	 * Create a 64k page of FPoolInfo structures for tracking allocations
	 */
	static FPoolInfo* CreateIndirect(FMallocBinned& Allocator)
	{
		uint64 IndirectPoolBlockSizeBytes = Allocator.IndirectPoolBlockSize * sizeof(FPoolInfo);

		checkSlow(IndirectPoolBlockSizeBytes <= Allocator.PageSize);
		FPoolInfo* Indirect = (FPoolInfo*)FPlatformMemory::BinnedAllocFromOS(IndirectPoolBlockSizeBytes);
		if( !Indirect )
		{
			OutOfMemory(IndirectPoolBlockSizeBytes);
		}
		FMemory::Memset(Indirect, 0, IndirectPoolBlockSizeBytes);

		BINNED_PEAK_STATCOUNTER(Allocator.OsPeak,    BINNED_ADD_STATCOUNTER(Allocator.OsCurrent,    (int64)(Align(IndirectPoolBlockSizeBytes, Allocator.PageSize))));
		BINNED_PEAK_STATCOUNTER(Allocator.WastePeak, BINNED_ADD_STATCOUNTER(Allocator.WasteCurrent, (int64)(Align(IndirectPoolBlockSizeBytes, Allocator.PageSize))));

		return Indirect;
	}

	/** 
	* Gets the FPoolInfo for a memory address. If no valid info exists one is created. 
	* NOTE: This function requires a mutex across threads, but its is the callers responsibility to 
	* acquire the mutex before calling
	*/
	static FORCEINLINE FPoolInfo* GetPoolInfo(FMallocBinned& Allocator, UPTRINT Ptr)
	{
		if (!Allocator.HashBuckets)
		{
			// Init tables.
			Allocator.HashBuckets = (PoolHashBucket*)FPlatformMemory::BinnedAllocFromOS(Align(Allocator.MaxHashBuckets * sizeof(PoolHashBucket), Allocator.PageSize));

			for (uint32 i = 0; i < Allocator.MaxHashBuckets; ++i) 
			{
				new (Allocator.HashBuckets + i) PoolHashBucket();
			}
		}

		UPTRINT Key       = Ptr >> Allocator.HashKeyShift;
		UPTRINT Hash      = Key & (Allocator.MaxHashBuckets - 1);
		UPTRINT PoolIndex = ((UPTRINT)Ptr >> Allocator.PoolBitShift) & Allocator.PoolMask;

		PoolHashBucket* Collision = &Allocator.HashBuckets[Hash];
		do
		{
			if (Collision->Key == Key || !Collision->FirstPool)
			{
				if (!Collision->FirstPool)
				{
					Collision->Key = Key;
					InitializeHashBucket(Allocator, Collision);
					CA_ASSUME(Collision->FirstPool);
				}
				return &Collision->FirstPool[PoolIndex];
			}

			Collision = Collision->Next;
		} while (Collision != &Allocator.HashBuckets[Hash]);

		//Create a new hash bucket entry
		PoolHashBucket* NewBucket = CreateHashBucket(Allocator);
		NewBucket->Key = Key;
		Allocator.HashBuckets[Hash].Link(NewBucket);

		return &NewBucket->FirstPool[PoolIndex];
	}

	static FORCEINLINE FPoolInfo* FindPoolInfo(FMallocBinned& Allocator, UPTRINT Ptr1, UPTRINT& AllocationBase)
	{
		uint16  NextStep = 0;
		UPTRINT Ptr      = Ptr1 &~ ((UPTRINT)Allocator.PageSize - 1);
		for (uint32 i = 0, n = (BINNED_ALLOC_POOL_SIZE / Allocator.PageSize) + 1; i < n; ++i)
		{
			FPoolInfo* Pool = FindPoolInfoInternal(Allocator, Ptr, NextStep);
			if (Pool)
			{
				AllocationBase = Ptr;
				//checkSlow(Ptr1 >= AllocationBase && Ptr1 < AllocationBase + Pool->GetBytes());
				return Pool;
			}
			Ptr = ((Ptr - (Allocator.PageSize * NextStep)) - 1) &~ ((UPTRINT)Allocator.PageSize - 1);
		}
		AllocationBase = 0;
		return nullptr;
	}

	static FORCEINLINE FPoolInfo* FindPoolInfoInternal(FMallocBinned& Allocator, UPTRINT Ptr, uint16& JumpOffset)
	{
		checkSlow(Allocator.HashBuckets);

		uint32 Key       = Ptr >> Allocator.HashKeyShift;
		uint32 Hash      = Key & (Allocator.MaxHashBuckets - 1);
		uint32 PoolIndex = ((UPTRINT)Ptr >> Allocator.PoolBitShift) & Allocator.PoolMask;

		JumpOffset = 0;

		PoolHashBucket* Collision = &Allocator.HashBuckets[Hash];
		do
		{
			if (Collision->Key==Key)
			{
				if (!Collision->FirstPool[PoolIndex].AllocSize)
				{
					JumpOffset = Collision->FirstPool[PoolIndex].TableIndex;
					return nullptr;
				}
				return &Collision->FirstPool[PoolIndex];
			}
			Collision = Collision->Next;
		} while (Collision != &Allocator.HashBuckets[Hash]);

		return nullptr;
	}

	/**
	 *	Returns a newly created and initialized PoolHashBucket for use.
	 */
	static FORCEINLINE PoolHashBucket* CreateHashBucket(FMallocBinned& Allocator) 
	{
		PoolHashBucket* Bucket = AllocateHashBucket(Allocator);
		InitializeHashBucket(Allocator, Bucket);
		return Bucket;
	}

	/**
	 *	Initializes bucket with valid parameters
	 *	@param bucket pointer to be initialized
	 */
	static FORCEINLINE void InitializeHashBucket(FMallocBinned& Allocator, PoolHashBucket* Bucket)
	{
		if (!Bucket->FirstPool)
		{
			Bucket->FirstPool = CreateIndirect(Allocator);
		}
	}

	/**
	 * Allocates a hash bucket from the free list of hash buckets
	*/
	static PoolHashBucket* AllocateHashBucket(FMallocBinned& Allocator)
	{
		if (!Allocator.HashBucketFreeList) 
		{
			const uint32 PageSize = Allocator.PageSize;

			Allocator.HashBucketFreeList = (PoolHashBucket*)FPlatformMemory::BinnedAllocFromOS(PageSize);
			BINNED_PEAK_STATCOUNTER(Allocator.OsPeak,    BINNED_ADD_STATCOUNTER(Allocator.OsCurrent,    PageSize));
			BINNED_PEAK_STATCOUNTER(Allocator.WastePeak, BINNED_ADD_STATCOUNTER(Allocator.WasteCurrent, PageSize));
			
			for (UPTRINT i = 0, n = PageSize / sizeof(PoolHashBucket); i < n; ++i)
			{
				Allocator.HashBucketFreeList->Link(new (Allocator.HashBucketFreeList + i) PoolHashBucket());
			}
		}

		PoolHashBucket* NextFree = Allocator.HashBucketFreeList->Next;
		PoolHashBucket* Free     = Allocator.HashBucketFreeList;

		Free->Unlink();
		if (NextFree == Free) 
		{
			NextFree = nullptr;
		}
		Allocator.HashBucketFreeList = NextFree;

		return Free;
	}

	static FPoolInfo* AllocatePoolMemory(FMallocBinned& Allocator, FPoolTable* Table, uint32 PoolSize, uint16 TableIndex)
	{
		const uint32 PageSize = Allocator.PageSize;

		// Must create a new pool.
		uint32 Blocks   = PoolSize / Table->BlockSize;
		uint32 Bytes    = Blocks * Table->BlockSize;
		UPTRINT OsBytes = Align(Bytes, PageSize);

		checkSlow(Blocks >= 1);
		checkSlow(Blocks * Table->BlockSize <= Bytes && PoolSize >= Bytes);

		// Allocate memory.
		FFreeMem* Free = nullptr;
		SIZE_T ActualPoolSize; //TODO: use this to reduce waste?
		Free = (FFreeMem*)OSAlloc(Allocator, OsBytes, ActualPoolSize);

		checkSlow(!((UPTRINT)Free & (PageSize - 1)));
		if( !Free )
		{
			OutOfMemory(OsBytes);
		}

		// Create pool in the indirect table.
		FPoolInfo* Pool;
		{
#ifdef USE_FINE_GRAIN_LOCKS
			FScopeLock PoolInfoLock(&Allocator.AccessGuard);
#endif
			Pool = GetPoolInfo(Allocator, (UPTRINT)Free);
			for (UPTRINT i = (UPTRINT)PageSize, Offset = 0; i < OsBytes; i += PageSize, ++Offset)
			{
				FPoolInfo* TrailingPool = GetPoolInfo(Allocator, ((UPTRINT)Free) + i);
				check(TrailingPool);

				//Set trailing pools to point back to first pool
				TrailingPool->SetAllocationSizes(0, 0, Offset, Allocator.BinnedOSTableIndex);
			}

			
			BINNED_PEAK_STATCOUNTER(Allocator.OsPeak,    BINNED_ADD_STATCOUNTER(Allocator.OsCurrent,    OsBytes));
			BINNED_PEAK_STATCOUNTER(Allocator.WastePeak, BINNED_ADD_STATCOUNTER(Allocator.WasteCurrent, (OsBytes - Bytes)));
		}

		// Init pool.
		Pool->Link( Table->FirstPool );
		Pool->SetAllocationSizes(Bytes, OsBytes, TableIndex, Allocator.BinnedOSTableIndex);
		Pool->Taken		 = 0;
		Pool->FirstMem   = Free;

#if STATS
		Table->NumActivePools++;
		Table->MaxActivePools = FMath::Max(Table->MaxActivePools, Table->NumActivePools);
#endif
		// Create first free item.
		Free->NumFreeBlocks = Blocks;
		Free->Next          = nullptr;

		return Pool;
	}

	static FORCEINLINE FFreeMem* AllocateBlockFromPool(FMallocBinned& Allocator, FPoolTable* Table, FPoolInfo* Pool, uint32 Alignment)
	{
		// Pick first available block and unlink it.
		Pool->Taken++;
		checkSlow(Pool->TableIndex < Allocator.BinnedOSTableIndex); // if this is false, FirstMem is actually a size not a pointer
		checkSlow(Pool->FirstMem);
		checkSlow(Pool->FirstMem->NumFreeBlocks > 0);
		checkSlow(Pool->FirstMem->NumFreeBlocks < PAGE_SIZE_LIMIT);
		FFreeMem* Free = (FFreeMem*)((uint8*)Pool->FirstMem + --Pool->FirstMem->NumFreeBlocks * Table->BlockSize);
		if( !Pool->FirstMem->NumFreeBlocks )
		{
			Pool->FirstMem = Pool->FirstMem->Next;
			if( !Pool->FirstMem )
			{
				// Move to exhausted list.
				Pool->Unlink();
				Pool->Link( Table->ExhaustedPool );
			}
		}
		BINNED_PEAK_STATCOUNTER(Allocator.UsedPeak, BINNED_ADD_STATCOUNTER(Allocator.UsedCurrent, Table->BlockSize));
		return Align(Free, Alignment);
	}

	/**
	* Releases memory back to the system. This is not protected from multi-threaded access and it's
	* the callers responsibility to Lock AccessGuard before calling this.
	*/
	static void FreeInternal(FMallocBinned& Allocator, void* Ptr)
	{
		MEM_TIME(MemTime -= FPlatformTime::Seconds());
		BINNED_DECREMENT_STATCOUNTER(Allocator.CurrentAllocs);

		UPTRINT BasePtr;
		FPoolInfo* Pool = FindPoolInfo(Allocator, (UPTRINT)Ptr, BasePtr);
#if PLATFORM_IOS || PLATFORM_MAC
        if (Pool == NULL)
        {
            UE_LOG(LogMemory, Warning, TEXT("Attempting to free a pointer we didn't allocate!"));
            return;
        }
#endif
		checkSlow(Pool);
		checkSlow(Pool->GetBytes() != 0);
		if (Pool->TableIndex < Allocator.BinnedOSTableIndex)
		{
			FPoolTable* Table = Allocator.MemSizeToPoolTable[Pool->TableIndex];
#ifdef USE_FINE_GRAIN_LOCKS
			FScopeLock TableLock(&Table->CriticalSection);
#endif
#if STATS
			Table->ActiveRequests--;
#endif
			// If this pool was exhausted, move to available list.
			if( !Pool->FirstMem )
			{
				Pool->Unlink();
				Pool->Link( Table->FirstPool );
			}

			void* BaseAddress = (void*)BasePtr;
			uint32 BlockSize = Table->BlockSize;
			PTRINT OffsetFromBase = (PTRINT)Ptr - (PTRINT)BaseAddress;
			check(OffsetFromBase >= 0);
			uint32 AlignOffset = OffsetFromBase % BlockSize;

			// Patch pointer to include previously applied alignment.
			Ptr = (void*)((PTRINT)Ptr - (PTRINT)AlignOffset);

			// Free a pooled allocation.
			FFreeMem* Free		= (FFreeMem*)Ptr;
			Free->NumFreeBlocks	= 1;
			Free->Next			= Pool->FirstMem;
			Pool->FirstMem		= Free;
			BINNED_ADD_STATCOUNTER(Allocator.UsedCurrent, -(int64)(Table->BlockSize));

			// Free this pool.
			checkSlow(Pool->Taken >= 1);
			if( --Pool->Taken == 0 )
			{
#if STATS
				Table->NumActivePools--;
#endif
				// Free the OS memory.
				SIZE_T OsBytes = Pool->GetOsBytes(Allocator.PageSize, Allocator.BinnedOSTableIndex);
				BINNED_ADD_STATCOUNTER(Allocator.OsCurrent,    -(int64)OsBytes);
				BINNED_ADD_STATCOUNTER(Allocator.WasteCurrent, -(int64)(OsBytes - Pool->GetBytes()));
				Pool->Unlink();
				Pool->SetAllocationSizes(0, 0, 0, Allocator.BinnedOSTableIndex);
				OSFree(Allocator, (void*)BasePtr, OsBytes);
			}
		}
		else
		{
			// Free an OS allocation.
			checkSlow(!((UPTRINT)Ptr & (Allocator.PageSize - 1)));
			SIZE_T OsBytes = Pool->GetOsBytes(Allocator.PageSize, Allocator.BinnedOSTableIndex);

			BINNED_ADD_STATCOUNTER(Allocator.UsedCurrent,  -(int64)Pool->GetBytes());
			BINNED_ADD_STATCOUNTER(Allocator.OsCurrent,    -(int64)OsBytes);
			BINNED_ADD_STATCOUNTER(Allocator.WasteCurrent, -(int64)(OsBytes - Pool->GetBytes()));
			OSFree(Allocator, (void*)BasePtr, OsBytes);
		}

		MEM_TIME(MemTime += FPlatformTime::Seconds());
	}

	static void PushFreeLockless(FMallocBinned& Allocator, void* Ptr)
	{
#ifdef USE_LOCKFREE_DELETE
		Allocator.PendingFreeList->Push(Ptr);
#else
#ifdef USE_COARSE_GRAIN_LOCKS
		FScopeLock ScopedLock(&AccessGuard);
#endif
		FreeInternal(Allocator, Ptr);
#endif
	}

	/**
	* Clear and Process the list of frees to be deallocated. It's the callers
	* responsibility to Lock AccessGuard before calling this
	*/
	static void FlushPendingFrees(FMallocBinned& Allocator)
	{
#ifdef USE_LOCKFREE_DELETE
		if (!Allocator.PendingFreeList && !Allocator.bDoneFreeListInit)
		{
			Allocator.bDoneFreeListInit = true;
			Allocator.PendingFreeList = new ((void*)Allocator.PendingFreeListMemory) TLockFreePointerList<void>();
		}

		// Because a lockless list and TArray calls new/malloc internally, need to guard against re-entry
		if (Allocator.bFlushingFrees || !Allocator.PendingFreeList)
		{
			return;
		}
		Allocator.bFlushingFrees = true;
		Allocator.PendingFreeList->PopAll(Allocator.FlushedFrees);
		for (uint32 i = 0, n = Allocator.FlushedFrees.Num(); i < n; ++i)
		{
			FreeInternal(Allocator, Allocator.FlushedFrees[i]);
		}
		Allocator.FlushedFrees.Reset();
		Allocator.bFlushingFrees = false;
#endif
	}

	static FORCEINLINE void OSFree(FMallocBinned& Allocator, void* Ptr, SIZE_T Size)
	{
#ifdef CACHE_FREED_OS_ALLOCS
#ifdef USE_FINE_GRAIN_LOCKS
		FScopeLock MainLock(&Allocator.AccessGuard);
#endif
		if (Size > MAX_CACHED_OS_FREES_BYTE_LIMIT / 4)
		{
			FPlatformMemory::BinnedFreeToOS(Ptr, Size);
			return;
		}

		while (Allocator.FreedPageBlocksNum && (Allocator.FreedPageBlocksNum >= MAX_CACHED_OS_FREES || Allocator.CachedTotal + Size > MAX_CACHED_OS_FREES_BYTE_LIMIT)) 
		{
			//Remove the oldest one
			void* FreePtr = Allocator.FreedPageBlocks[0].Ptr;
			SIZE_T FreeSize = Allocator.FreedPageBlocks[0].ByteSize;
			Allocator.CachedTotal -= FreeSize;
			Allocator.FreedPageBlocksNum--;
			if (Allocator.FreedPageBlocksNum)
			{
				FMemory::Memmove(&Allocator.FreedPageBlocks[0], &Allocator.FreedPageBlocks[1], sizeof(FFreePageBlock) * Allocator.FreedPageBlocksNum);
			}
			FPlatformMemory::BinnedFreeToOS(FreePtr, FreeSize);
		}
		Allocator.FreedPageBlocks[Allocator.FreedPageBlocksNum].Ptr      = Ptr;
		Allocator.FreedPageBlocks[Allocator.FreedPageBlocksNum].ByteSize = Size;
		Allocator.CachedTotal += Size;
		++Allocator.FreedPageBlocksNum;
#else
		FPlatformMemory::BinnedFreeToOS(Ptr, Size);
#endif
	}

	static FORCEINLINE void* OSAlloc(FMallocBinned& Allocator, SIZE_T NewSize, SIZE_T& OutActualSize)
	{
#ifdef CACHE_FREED_OS_ALLOCS
		{
#ifdef USE_FINE_GRAIN_LOCKS
			// We want to hold the lock a little as possible so release it
			// before the big call to the OS
			FScopeLock MainLock(&Allocator.AccessGuard);
#endif
			for (uint32 i=0; i < Allocator.FreedPageBlocksNum; ++i)
			{
				// look for exact matches first, these are aligned to the page size, so it should be quite common to hit these on small pages sizes
				if (Allocator.FreedPageBlocks[i].ByteSize == NewSize)
				{
					void* Ret = Allocator.FreedPageBlocks[i].Ptr;
					UE_CLOG(!Ret, LogMemory, Fatal, TEXT("OS memory allocation cache has been corrupted!"));
					OutActualSize = Allocator.FreedPageBlocks[i].ByteSize;
					Allocator.CachedTotal -= Allocator.FreedPageBlocks[i].ByteSize;
					if (i < Allocator.FreedPageBlocksNum - 1)
					{
						FMemory::Memmove(&Allocator.FreedPageBlocks[i], &Allocator.FreedPageBlocks[i + 1], sizeof(FFreePageBlock) * (Allocator.FreedPageBlocksNum - i - 1));
					}
					Allocator.FreedPageBlocksNum--;
					return Ret;
				}
			};
			// Note the code below was similar to code removed by Arciel in his MallocBinned2 update and its removal fixes a memory leak
/*			for (uint32 i=0; i < Allocator.FreedPageBlocksNum; ++i)
			{
				// is it possible (and worth i.e. <25% overhead) to use this block
				if (Allocator.FreedPageBlocks[i].ByteSize >= NewSize && Allocator.FreedPageBlocks[i].ByteSize * 3 <= NewSize * 4)
				{
					void* Ret = Allocator.FreedPageBlocks[i].Ptr;
					UE_CLOG(!Ret, LogMemory, Fatal, TEXT("OS memory allocation cache has been corrupted!"));
					OutActualSize = Allocator.FreedPageBlocks[i].ByteSize;
					Allocator.CachedTotal -= Allocator.FreedPageBlocks[i].ByteSize;
					if (i < Allocator.FreedPageBlocksNum - 1)
					{
						FMemory::Memmove(&Allocator.FreedPageBlocks[i], &Allocator.FreedPageBlocks[i + 1], sizeof(FFreePageBlock) * (Allocator.FreedPageBlocksNum - i - 1));
					}
					Allocator.FreedPageBlocksNum--;
					return Ret;
				}
			};*/
		}
		OutActualSize = NewSize;
		void* Ptr = FPlatformMemory::BinnedAllocFromOS(NewSize);
		if (!Ptr)
		{
			//Are we holding on to much mem? Release it all.
			FlushAllocCache(Allocator);
			Ptr = FPlatformMemory::BinnedAllocFromOS(NewSize);
		}
		return Ptr;
#else
		(void)OutActualSize;
		return FPlatformMemory::BinnedAllocFromOS(NewSize);
#endif
	}

#ifdef CACHE_FREED_OS_ALLOCS
	static void FlushAllocCache(FMallocBinned& Allocator)
	{
#ifdef USE_FINE_GRAIN_LOCKS
		FScopeLock MainLock(&Allocator.AccessGuard);
#endif
		for (int i = 0, n = Allocator.FreedPageBlocksNum; i<n; ++i) 
		{
			//Remove allocs
			FPlatformMemory::BinnedFreeToOS(Allocator.FreedPageBlocks[i].Ptr, Allocator.FreedPageBlocks[i].ByteSize);
			Allocator.FreedPageBlocks[i].Ptr = nullptr;
			Allocator.FreedPageBlocks[i].ByteSize = 0;
		}
		Allocator.FreedPageBlocksNum = 0;
		Allocator.CachedTotal        = 0;
	}
#endif

	static void UpdateSlackStat(FMallocBinned& Allocator)
	{
	#if	STATS
		SIZE_T LocalWaste = Allocator.WasteCurrent;
		double Waste = 0.0;
		for( int32 PoolIndex = 0; PoolIndex < POOL_COUNT; PoolIndex++ )
		{
			FPoolTable& Table = Allocator.PoolTable[PoolIndex];

			Waste += ( ( double )Table.TotalWaste / ( double )Table.TotalRequests ) * ( double )Table.ActiveRequests;
			Waste += Table.NumActivePools * ( BINNED_ALLOC_POOL_SIZE - ( ( BINNED_ALLOC_POOL_SIZE / Table.BlockSize ) * Table.BlockSize ) );
		}
		LocalWaste += ( uint32 )Waste;
		Allocator.SlackCurrent = Allocator.OsCurrent - LocalWaste - Allocator.UsedCurrent;
	#endif // STATS
	}
};

void FMallocBinned::GetAllocatorStats( FGenericMemoryStats& out_Stats )
{
	FMalloc::GetAllocatorStats( out_Stats );

#if	STATS
	SIZE_T	LocalOsCurrent = 0;
	SIZE_T	LocalOsPeak = 0;
	SIZE_T	LocalWasteCurrent = 0;
	SIZE_T	LocalWastePeak = 0;
	SIZE_T	LocalUsedCurrent = 0;
	SIZE_T	LocalUsedPeak = 0;
	SIZE_T	LocalCurrentAllocs = 0;
	SIZE_T	LocalTotalAllocs = 0;
	SIZE_T	LocalSlackCurrent = 0;

	{
#ifdef USE_INTERNAL_LOCKS
		FScopeLock ScopedLock( &AccessGuard );
#endif

		Private::UpdateSlackStat(*this);

		// Copy memory stats.
		LocalOsCurrent = OsCurrent;
		LocalOsPeak = OsPeak;
		LocalWasteCurrent = WasteCurrent;
		LocalWastePeak = WastePeak;
		LocalUsedCurrent = UsedCurrent;
		LocalUsedPeak = UsedPeak;
		LocalCurrentAllocs = CurrentAllocs;
		LocalTotalAllocs = TotalAllocs;
		LocalSlackCurrent = SlackCurrent;
	}

	// Malloc binned stats.
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_OsCurrent ), LocalOsCurrent );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_OsPeak ), LocalOsPeak );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_WasteCurrent ), LocalWasteCurrent );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_WastePeak ), LocalWastePeak );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_UsedCurrent ), LocalUsedCurrent );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_UsedPeak ), LocalUsedPeak );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_CurrentAllocs ), LocalCurrentAllocs );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_TotalAllocs ), LocalTotalAllocs );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_SlackCurrent ), LocalSlackCurrent );
#endif // STATS
}

void FMallocBinned::InitializeStatsMetadata()
{
	FMalloc::InitializeStatsMetadata();

	// Initialize stats metadata here instead of UpdateStats.
	// Mostly to avoid dead-lock when stats malloc profiler is enabled.
	GET_STATFNAME(STAT_Binned_OsCurrent);
	GET_STATFNAME(STAT_Binned_OsPeak);
	GET_STATFNAME(STAT_Binned_WasteCurrent);
	GET_STATFNAME(STAT_Binned_WastePeak);
	GET_STATFNAME(STAT_Binned_UsedCurrent);
	GET_STATFNAME(STAT_Binned_UsedPeak);
	GET_STATFNAME(STAT_Binned_CurrentAllocs);
	GET_STATFNAME(STAT_Binned_TotalAllocs);
	GET_STATFNAME(STAT_Binned_SlackCurrent);
}

FMallocBinned::FMallocBinned(uint32 InPageSize, uint64 AddressLimit)
	:	TableAddressLimit(AddressLimit)
#ifdef USE_LOCKFREE_DELETE
	,	PendingFreeList(nullptr)
	,	bFlushingFrees(false)
	,	bDoneFreeListInit(false)
#endif
	,	HashBuckets(nullptr)
	,	HashBucketFreeList(nullptr)
	,	PageSize		(InPageSize)
#ifdef CACHE_FREED_OS_ALLOCS
	,	FreedPageBlocksNum(0)
	,	CachedTotal(0)
#endif
#if STATS
	,	OsCurrent		( 0 )
	,	OsPeak			( 0 )
	,	WasteCurrent	( 0 )
	,	WastePeak		( 0 )
	,	UsedCurrent		( 0 )
	,	UsedPeak		( 0 )
	,	CurrentAllocs	( 0 )
	,	TotalAllocs		( 0 )
	,	SlackCurrent	( 0 )
	,	MemTime			( 0.0 )
#endif
{
	check(!(PageSize & (PageSize - 1)));
	check(!(AddressLimit & (AddressLimit - 1)));
	check(PageSize <= 65536); // There is internal limit on page size of 64k
	check(AddressLimit > PageSize); // Check to catch 32 bit overflow in AddressLimit

	/** Shift to get the reference from the indirect tables */
	PoolBitShift = FPlatformMath::CeilLogTwo(PageSize);
	IndirectPoolBitShift = FPlatformMath::CeilLogTwo(PageSize/sizeof(FPoolInfo));
	IndirectPoolBlockSize = PageSize/sizeof(FPoolInfo);

	MaxHashBuckets = AddressLimit >> (IndirectPoolBitShift+PoolBitShift); 
	MaxHashBucketBits = FPlatformMath::CeilLogTwo(MaxHashBuckets);
	MaxHashBucketWaste = (MaxHashBuckets*sizeof(PoolHashBucket))/1024;
	MaxBookKeepingOverhead = ((AddressLimit/PageSize)*sizeof(PoolHashBucket))/(1024*1024);
	/** 
	* Shift required to get required hash table key.
	*/
	HashKeyShift = PoolBitShift+IndirectPoolBitShift;
	/** Used to mask off the bits that have been used to lookup the indirect table */
	PoolMask =  ( ( 1ull << ( HashKeyShift - PoolBitShift ) ) - 1 );
	BinnedSizeLimit = Private::PAGE_SIZE_LIMIT/2;
	BinnedOSTableIndex = BinnedSizeLimit+EXTENDED_PAGE_POOL_ALLOCATION_COUNT;

	check((BinnedSizeLimit & (BinnedSizeLimit-1)) == 0);


	// Init tables.
	OsTable.FirstPool = nullptr;
	OsTable.ExhaustedPool = nullptr;
	OsTable.BlockSize = 0;

	/** The following options are not valid for page sizes less than 64k. They are here to reduce waste*/
	PagePoolTable[0].FirstPool = nullptr;
	PagePoolTable[0].ExhaustedPool = nullptr;
	PagePoolTable[0].BlockSize = PageSize == Private::PAGE_SIZE_LIMIT ? BinnedSizeLimit+(BinnedSizeLimit/2) : 0;

	PagePoolTable[1].FirstPool = nullptr;
	PagePoolTable[1].ExhaustedPool = nullptr;
	PagePoolTable[1].BlockSize = PageSize == Private::PAGE_SIZE_LIMIT ? PageSize+BinnedSizeLimit : 0;

	// Block sizes are based around getting the maximum amount of allocations per pool, with as little alignment waste as possible.
	// Block sizes should be close to even divisors of the POOL_SIZE, and well distributed. They must be 16-byte aligned as well.
	static const uint32 BlockSizes[POOL_COUNT] =
	{
		8,		16,		32,		48,		64,		80,		96,		112,
		128,	160,	192,	224,	256,	288,	320,	384,
		448,	512,	576,	640,	704,	768,	896,	1024,
		1168,	1360,	1632,	2048,	2336,	2720,	3264,	4096,
		4672,	5456,	6544,	8192,	9360,	10912,	13104,	16384,
		21840,	32768
	};

	for( uint32 i = 0; i < POOL_COUNT; i++ )
	{
		PoolTable[i].FirstPool = nullptr;
		PoolTable[i].ExhaustedPool = nullptr;
		PoolTable[i].BlockSize = BlockSizes[i];
#if STATS
		PoolTable[i].MinRequest = PoolTable[i].BlockSize;
#endif
	}

	for( uint32 i=0; i<MAX_POOLED_ALLOCATION_SIZE; i++ )
	{
		uint32 Index = 0;
		while( PoolTable[Index].BlockSize < i )
		{
			++Index;
		}
		checkSlow(Index < POOL_COUNT);
		MemSizeToPoolTable[i] = &PoolTable[Index];
	}

	MemSizeToPoolTable[BinnedSizeLimit] = &PagePoolTable[0];
	MemSizeToPoolTable[BinnedSizeLimit+1] = &PagePoolTable[1];

	check(MAX_POOLED_ALLOCATION_SIZE - 1 == PoolTable[POOL_COUNT - 1].BlockSize);
}

FMallocBinned::~FMallocBinned()
{
}

bool FMallocBinned::IsInternallyThreadSafe() const
{ 
#ifdef USE_INTERNAL_LOCKS
	return true;
#else
	return false;
#endif
}

void* FMallocBinned::Malloc(SIZE_T Size, uint32 Alignment)
{
#ifdef USE_COARSE_GRAIN_LOCKS
	FScopeLock ScopedLock(&AccessGuard);
#endif

	Private::FlushPendingFrees(*this);

	// Handle DEFAULT_ALIGNMENT for binned allocator.
	if (Alignment == DEFAULT_ALIGNMENT)
	{
		Alignment = Private::DEFAULT_BINNED_ALLOCATOR_ALIGNMENT;
	}

	Alignment = FMath::Max<uint32>(Alignment, Private::DEFAULT_BINNED_ALLOCATOR_ALIGNMENT);
	SIZE_T SpareBytesCount = FMath::Min<SIZE_T>(Private::DEFAULT_BINNED_ALLOCATOR_ALIGNMENT, Size);
	Size = FMath::Max<SIZE_T>(PoolTable[0].BlockSize, Size + (Alignment - SpareBytesCount));
	MEM_TIME(MemTime -= FPlatformTime::Seconds());
	
	BINNED_INCREMENT_STATCOUNTER(CurrentAllocs);
	BINNED_INCREMENT_STATCOUNTER(TotalAllocs);
	
	FFreeMem* Free;
	if( Size < BinnedSizeLimit )
	{
		// Allocate from pool.
		FPoolTable* Table = MemSizeToPoolTable[Size];
#ifdef USE_FINE_GRAIN_LOCKS
		FScopeLock TableLock(&Table->CriticalSection);
#endif
		checkSlow(Size <= Table->BlockSize);

		Private::TrackStats(Table, Size);

		FPoolInfo* Pool = Table->FirstPool;
		if( !Pool )
		{
			Pool = Private::AllocatePoolMemory(*this, Table, Private::BINNED_ALLOC_POOL_SIZE/*PageSize*/, Size);
		}

		Free = Private::AllocateBlockFromPool(*this, Table, Pool, Alignment);
	}
	else if ( ((Size >= BinnedSizeLimit && Size <= PagePoolTable[0].BlockSize) ||
		(Size > PageSize && Size <= PagePoolTable[1].BlockSize)))
	{
		// Bucket in a pool of 3*PageSize or 6*PageSize
		uint32 BinType = Size < PageSize ? 0 : 1;
		uint32 PageCount = 3*BinType + 3;
		FPoolTable* Table = &PagePoolTable[BinType];
#ifdef USE_FINE_GRAIN_LOCKS
		FScopeLock TableLock(&Table->CriticalSection);
#endif
		checkSlow(Size <= Table->BlockSize);

		Private::TrackStats(Table, Size);

		FPoolInfo* Pool = Table->FirstPool;
		if( !Pool )
		{
			Pool = Private::AllocatePoolMemory(*this, Table, PageCount*PageSize, BinnedSizeLimit+BinType);
		}

		Free = Private::AllocateBlockFromPool(*this, Table, Pool, Alignment);
	}
	else
	{
		// Use OS for large allocations.
		UPTRINT AlignedSize = Align(Size,PageSize);
		SIZE_T ActualPoolSize; //TODO: use this to reduce waste?
		Free = (FFreeMem*)Private::OSAlloc(*this, AlignedSize, ActualPoolSize);
		if( !Free )
		{
			Private::OutOfMemory(AlignedSize);
		}

		void* AlignedFree = Align(Free, Alignment);

		// Create indirect.
		FPoolInfo* Pool;
		{
#ifdef USE_FINE_GRAIN_LOCKS
			FScopeLock PoolInfoLock(&AccessGuard);
#endif
			Pool = Private::GetPoolInfo(*this, (UPTRINT)Free);

			if ((UPTRINT)Free != ((UPTRINT)AlignedFree & ~((UPTRINT)PageSize - 1)))
			{
				// Mark the FPoolInfo for AlignedFree to jump back to the FPoolInfo for ptr.
				for (UPTRINT i = (UPTRINT)PageSize, Offset = 0; i < AlignedSize; i += PageSize, ++Offset)
				{
					FPoolInfo* TrailingPool = Private::GetPoolInfo(*this, ((UPTRINT)Free) + i);
					check(TrailingPool);
					//Set trailing pools to point back to first pool
					TrailingPool->SetAllocationSizes(0, 0, Offset, BinnedOSTableIndex);
				}
			}
		}
		Free = (FFreeMem*)AlignedFree;
		Pool->SetAllocationSizes(Size, AlignedSize, BinnedOSTableIndex, BinnedOSTableIndex);
		BINNED_PEAK_STATCOUNTER(OsPeak, BINNED_ADD_STATCOUNTER(OsCurrent, AlignedSize));
		BINNED_PEAK_STATCOUNTER(UsedPeak, BINNED_ADD_STATCOUNTER(UsedCurrent, Size));
		BINNED_PEAK_STATCOUNTER(WastePeak, BINNED_ADD_STATCOUNTER(WasteCurrent, (int64)(AlignedSize - Size)));
	}

	MEM_TIME(MemTime += FPlatformTime::Seconds());
	return Free;
}

void* FMallocBinned::Realloc( void* Ptr, SIZE_T NewSize, uint32 Alignment )
{
	// Handle DEFAULT_ALIGNMENT for binned allocator.
	if (Alignment == DEFAULT_ALIGNMENT)
	{
		Alignment = Private::DEFAULT_BINNED_ALLOCATOR_ALIGNMENT;
	}

	Alignment = FMath::Max<uint32>(Alignment, Private::DEFAULT_BINNED_ALLOCATOR_ALIGNMENT);
	const uint32 NewSizeUnmodified = NewSize;
	SIZE_T SpareBytesCount = FMath::Min<SIZE_T>(Private::DEFAULT_BINNED_ALLOCATOR_ALIGNMENT, NewSize);
	if (NewSize)
	{
		NewSize = FMath::Max<SIZE_T>(PoolTable[0].BlockSize, NewSize + (Alignment - SpareBytesCount));
	}
	MEM_TIME(MemTime -= FPlatformTime::Seconds());
	UPTRINT BasePtr;
	void* NewPtr = Ptr;
	if( Ptr && NewSize )
	{
		FPoolInfo* Pool = Private::FindPoolInfo(*this, (UPTRINT)Ptr, BasePtr);

		if( Pool->TableIndex < BinnedOSTableIndex )
		{
			// Allocated from pool, so grow or shrink if necessary.
			check(Pool->TableIndex > 0); // it isn't possible to allocate a size of 0, Malloc will increase the size to DEFAULT_BINNED_ALLOCATOR_ALIGNMENT
			if (NewSizeUnmodified > MemSizeToPoolTable[Pool->TableIndex]->BlockSize || NewSizeUnmodified <= MemSizeToPoolTable[Pool->TableIndex - 1]->BlockSize)
			{
				NewPtr = Malloc(NewSizeUnmodified, Alignment);
				FMemory::Memcpy(NewPtr, Ptr, FMath::Min<SIZE_T>(NewSizeUnmodified, MemSizeToPoolTable[Pool->TableIndex]->BlockSize - (Alignment - SpareBytesCount)));
				Free( Ptr );
			}
			else if (((UPTRINT)Ptr & (UPTRINT)(Alignment - 1)) != 0)
			{
				NewPtr = Align(Ptr, Alignment);
				FMemory::Memmove(NewPtr, Ptr, NewSize);
			}
		}
		else
		{
			// Allocated from OS.
			if( NewSize > Pool->GetOsBytes(PageSize, BinnedOSTableIndex) || NewSize * 3 < Pool->GetOsBytes(PageSize, BinnedOSTableIndex) * 2 )
			{
				// Grow or shrink.
				NewPtr = Malloc(NewSizeUnmodified, Alignment);
				FMemory::Memcpy(NewPtr, Ptr, FMath::Min<SIZE_T>(NewSizeUnmodified, Pool->GetBytes()));
				Free( Ptr );
			}
			else
			{
//need a lock to cover the SetAllocationSizes()
#ifdef USE_FINE_GRAIN_LOCKS
				FScopeLock PoolInfoLock(&AccessGuard);
#endif
				int32 UsedChange = (NewSize - Pool->GetBytes());
				
				// Keep as-is, reallocation isn't worth the overhead.
				BINNED_ADD_STATCOUNTER(UsedCurrent, UsedChange);
				BINNED_PEAK_STATCOUNTER(UsedPeak, UsedCurrent);
				BINNED_ADD_STATCOUNTER(WasteCurrent, (Pool->GetBytes() - NewSize));
				Pool->SetAllocationSizes(NewSizeUnmodified, Pool->GetOsBytes(PageSize, BinnedOSTableIndex), BinnedOSTableIndex, BinnedOSTableIndex);
			}
		}
	}
	else if( Ptr == nullptr )
	{
		NewPtr = Malloc(NewSizeUnmodified, Alignment);
	}
	else
	{
		Free( Ptr );
		NewPtr = nullptr;
	}

	MEM_TIME(MemTime += FPlatformTime::Seconds());
	return NewPtr;
}

void FMallocBinned::Free( void* Ptr )
{
	if( !Ptr )
	{
		return;
	}

	Private::PushFreeLockless(*this, Ptr);
}

bool FMallocBinned::GetAllocationSize(void *Original, SIZE_T &SizeOut)
{
	if (!Original)
	{
		return false;
	}

	UPTRINT BasePtr;
	FPoolInfo* Pool = Private::FindPoolInfo(*this, (UPTRINT)Original, BasePtr);

#if PLATFORM_IOS || PLATFORM_MAC
	if (Pool == NULL)
	{
		UE_LOG(LogMemory, Warning, TEXT("Attempting to access memory pool info for a pointer we didn't allocate!"));
		return false;
	}
#endif

	PTRINT OffsetFromBase = (PTRINT)Original - (PTRINT)BasePtr;
	check(OffsetFromBase >= 0);

	if (Pool->TableIndex < BinnedOSTableIndex)
	{
		FPoolTable* Table = MemSizeToPoolTable[Pool->TableIndex];

		uint32 AlignOffset = OffsetFromBase % Table->BlockSize;

		SizeOut = Table->BlockSize - AlignOffset;
	}
	else
	{
		// if we padded out the allocation for alignment, and then offset the returned pointer from the actual allocation
		// we need to adjust for that offset. Pool->GetOsBytes() returns the entire size of the allocation, not just the 
		// usable part that was returned to the caller
		SizeOut = Pool->GetOsBytes(PageSize, BinnedOSTableIndex) - OffsetFromBase;
	}
	return true;
}

bool FMallocBinned::ValidateHeap()
{
#ifdef USE_COARSE_GRAIN_LOCKS
	FScopeLock ScopedLock(&AccessGuard);
#endif
	for( int32 i = 0; i < POOL_COUNT; i++ )
	{
		FPoolTable* Table = &PoolTable[i];
#ifdef USE_FINE_GRAIN_LOCKS
		FScopeLock TableLock(&Table->CriticalSection);
#endif
		for( FPoolInfo** PoolPtr = &Table->FirstPool; *PoolPtr; PoolPtr = &(*PoolPtr)->Next )
		{
			FPoolInfo* Pool = *PoolPtr;
			check(Pool->PrevLink == PoolPtr);
			check(Pool->FirstMem);
			for( FFreeMem* Free = Pool->FirstMem; Free; Free = Free->Next )
			{
				check(Free->NumFreeBlocks > 0);
			}
		}
		for( FPoolInfo** PoolPtr = &Table->ExhaustedPool; *PoolPtr; PoolPtr = &(*PoolPtr)->Next )
		{
			FPoolInfo* Pool = *PoolPtr;
			check(Pool->PrevLink == PoolPtr);
			check(!Pool->FirstMem);
		}
	}

	return( true );
}

void FMallocBinned::UpdateStats()
{
	FMalloc::UpdateStats();
#if STATS
	SIZE_T	LocalOsCurrent = 0;
	SIZE_T	LocalOsPeak = 0;
	SIZE_T	LocalWasteCurrent = 0;
	SIZE_T	LocalWastePeak = 0;
	SIZE_T	LocalUsedCurrent = 0;
	SIZE_T	LocalUsedPeak = 0;
	SIZE_T	LocalCurrentAllocs = 0;
	SIZE_T	LocalTotalAllocs = 0;
	SIZE_T	LocalSlackCurrent = 0;

	{
#ifdef USE_INTERNAL_LOCKS
		FScopeLock ScopedLock( &AccessGuard );
#endif
		Private::UpdateSlackStat(*this);

		// Copy memory stats.
		LocalOsCurrent = OsCurrent;
		LocalOsPeak = OsPeak;
		LocalWasteCurrent = WasteCurrent;
		LocalWastePeak = WastePeak;
		LocalUsedCurrent = UsedCurrent;
		LocalUsedPeak = UsedPeak;
		LocalCurrentAllocs = CurrentAllocs;
		LocalTotalAllocs = TotalAllocs;
		LocalSlackCurrent = SlackCurrent;
	}

	SET_MEMORY_STAT( STAT_Binned_OsCurrent, LocalOsCurrent );
	SET_MEMORY_STAT( STAT_Binned_OsPeak, LocalOsPeak );
	SET_MEMORY_STAT( STAT_Binned_WasteCurrent, LocalWasteCurrent );
	SET_MEMORY_STAT( STAT_Binned_WastePeak, LocalWastePeak );
	SET_MEMORY_STAT( STAT_Binned_UsedCurrent, LocalUsedCurrent );
	SET_MEMORY_STAT( STAT_Binned_UsedPeak, LocalUsedPeak );
	SET_DWORD_STAT( STAT_Binned_CurrentAllocs, LocalCurrentAllocs );
	SET_DWORD_STAT( STAT_Binned_TotalAllocs, LocalTotalAllocs );
	SET_MEMORY_STAT( STAT_Binned_SlackCurrent, LocalSlackCurrent );
#endif
}

void FMallocBinned::DumpAllocatorStats( class FOutputDevice& Ar )
{
	FBufferedOutputDevice BufferedOutput;
	{
#ifdef USE_COARSE_GRAIN_LOCKS
		FScopeLock ScopedLock( &AccessGuard );
#endif
		ValidateHeap();
#if STATS
		Private::UpdateSlackStat(*this);
#if !NO_LOGGING
		// This is all of the memory including stuff too big for the pools
		BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Allocator Stats for %s:" ), GetDescriptiveName() );
		// Waste is the total overhead of the memory system
		BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Current Memory %.2f MB used, plus %.2f MB waste" ), UsedCurrent / (1024.0f * 1024.0f), (OsCurrent - UsedCurrent) / (1024.0f * 1024.0f) );
		BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Peak Memory %.2f MB used, plus %.2f MB waste" ), UsedPeak / (1024.0f * 1024.0f), (OsPeak - UsedPeak) / (1024.0f * 1024.0f) );

		BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Current OS Memory %.2f MB, peak %.2f MB" ), OsCurrent / (1024.0f * 1024.0f), OsPeak / (1024.0f * 1024.0f) );
		BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Current Waste %.2f MB, peak %.2f MB" ), WasteCurrent / (1024.0f * 1024.0f), WastePeak / (1024.0f * 1024.0f) );
		BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Current Used %.2f MB, peak %.2f MB" ), UsedCurrent / (1024.0f * 1024.0f), UsedPeak / (1024.0f * 1024.0f) );
		BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Current Slack %.2f MB" ), SlackCurrent / (1024.0f * 1024.0f) );

		BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Allocs      % 6i Current / % 6i Total" ), CurrentAllocs, TotalAllocs );
		MEM_TIME( BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Seconds     % 5.3f" ), MemTime ) );
		MEM_TIME( BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "MSec/Allc   % 5.5f" ), 1000.0 * MemTime / MemAllocs ) );

		// This is the memory tracked inside individual allocation pools
		BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "" ) );
		BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Block Size Num Pools Max Pools Cur Allocs Total Allocs Min Req Max Req Mem Used Mem Slack Mem Waste Efficiency" ) );
		BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "---------- --------- --------- ---------- ------------ ------- ------- -------- --------- --------- ----------" ) );

		uint32 TotalMemory = 0;
		uint32 TotalWaste = 0;
		uint32 TotalActiveRequests = 0;
		uint32 TotalTotalRequests = 0;
		uint32 TotalPools = 0;
		uint32 TotalSlack = 0;

		FPoolTable* Table = nullptr;
		for( int32 i = 0; i < BinnedSizeLimit + EXTENDED_PAGE_POOL_ALLOCATION_COUNT; i++ )
		{
			if( Table == MemSizeToPoolTable[i] || MemSizeToPoolTable[i]->BlockSize == 0 )
				continue;

			Table = MemSizeToPoolTable[i];

#ifdef USE_FINE_GRAIN_LOCKS
			Table->CriticalSection.Lock();
#endif

			uint32 TableAllocSize = (Table->BlockSize > BinnedSizeLimit ? (((3 * (i - BinnedSizeLimit)) + 3)*Private::BINNED_ALLOC_POOL_SIZE) : Private::BINNED_ALLOC_POOL_SIZE);
			// The amount of memory allocated from the OS
			uint32 MemAllocated = (Table->NumActivePools * TableAllocSize) / 1024;
			// Amount of memory actually in use by allocations
			uint32 MemUsed = (Table->BlockSize * Table->ActiveRequests) / 1024;
			// Wasted memory due to pool size alignment
			uint32 PoolMemWaste = Table->NumActivePools * (TableAllocSize - ((TableAllocSize / Table->BlockSize) * Table->BlockSize)) / 1024;
			// Wasted memory due to individual allocation alignment. This is an estimate.
			uint32 MemWaste = (uint32)(((double)Table->TotalWaste / (double)Table->TotalRequests) * (double)Table->ActiveRequests) / 1024 + PoolMemWaste;
			// Memory that is reserved in active pools and ready for future use
			uint32 MemSlack = MemAllocated - MemUsed - PoolMemWaste;
			// Copy the other stats before releasing the lock and calling CategorizedLogf
			uint32 TableBlockSize = Table->BlockSize;
			uint32 TableNumActivePools = Table->NumActivePools;
			uint32 TableMaxActivePools = Table->MaxActivePools;
			uint32 TableActiveRequests = Table->ActiveRequests;
			uint32 TableTotalRequests = (uint32)Table->TotalRequests;
			uint32 TableMinRequest = Table->MinRequest;
			uint32 TableMaxRequest = Table->MaxRequest;

#ifdef USE_FINE_GRAIN_LOCKS
			Table->CriticalSection.Unlock();
#endif

			BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "% 10i % 9i % 9i % 10i % 12i % 7i % 7i % 7iK % 8iK % 8iK % 9.2f%%" ),
										  TableBlockSize,
										  TableNumActivePools,
										  TableMaxActivePools,
										  TableActiveRequests,
										  TableTotalRequests,
										  TableMinRequest,
										  TableMaxRequest,
										  MemUsed,
										  MemSlack,
										  MemWaste,
										  MemAllocated ? 100.0f * (MemAllocated - MemWaste) / MemAllocated : 100.0f );

			TotalMemory += MemAllocated;
			TotalWaste += MemWaste;
			TotalSlack += MemSlack;
			TotalActiveRequests += TableActiveRequests;
			TotalTotalRequests += TableTotalRequests;
			TotalPools += TableNumActivePools;
		}

		BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "" ) );
		BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "%iK allocated in pools (with %iK slack and %iK waste). Efficiency %.2f%%" ), TotalMemory, TotalSlack, TotalWaste, TotalMemory ? 100.0f * (TotalMemory - TotalWaste) / TotalMemory : 100.0f );
		BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Allocations %i Current / %i Total (in %i pools)" ), TotalActiveRequests, TotalTotalRequests, TotalPools );
		BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "" ) );
#endif
#endif
	}

	BufferedOutput.RedirectTo( Ar );
}

const TCHAR* FMallocBinned::GetDescriptiveName()
{
	return TEXT("binned");
}
