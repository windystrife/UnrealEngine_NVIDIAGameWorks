// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/*
 *  Set this to 1 if BinnedAllocFromOS()/BinnedFreeToOS() need to be pooled.
 * This makes most sense for 32-bit platforms where address space fragmentation can make BAFO() run out of memory.
 * On 64-bit Linux, this speeds up allocations several times (see TestPAL mallocthreadtest), however has some side effects for 64-bit servers, so it is off for them atm. Non-x86(-64) platforms and non-editor builds are also excluded because of RAM usage concerns.
 */
#define UE4_POOL_BAFO_ALLOCATIONS						(((PLATFORM_LINUX && PLATFORM_32BITS) || (PLATFORM_LINUX && UE_EDITOR)) && PLATFORM_CPU_X86_FAMILY)

/*
 *  Normally pools will just reserve the memory and then commit/evict it when needed. However, Commit/Evict calls on the platform
 * can be heavy-weight, so the memory can be instead committed immediately and the OS is left to determine what is actually being used.
 * This avoids an overhead of making a syscall on on each Alloc/Free - the downside is potentially larger resident RAM consumption if the OS has bad heuristics.
 * On Linux, avoiding frequent madvise() gives better speed and the OS is good enough about figuring out the real need in resident RAM,
 * so leaving it on except for the server where memory consumption is more important.
 */
#define UE4_POOL_BAFO_ALLOCATIONS_COMMIT_ON_RESERVATION	(PLATFORM_LINUX && !UE_SERVER)

/*
 *  Perform checks that ensure that the pools are working as intended. This is not necessary in builds that are used for Shipping/Test or for the Development editor.
 */
#define UE4_POOL_BAFO_ALLOCATIONS_DO_SANITY_CHECKS		(UE4_POOL_BAFO_ALLOCATIONS && (UE_BUILD_DEBUG || (UE_BUILD_DEVELOPMENT && !UE_EDITOR)))

/*
 *  This enables a debugging code that creates a histogram of allocations that spilled over pool. Can be useful to figure out the pool sizes or to debug OOM.
 * With this code enabled, the engine will not crash on OOM but will enter an infinite loop, allowing you to attach the debugger and see the details.
 * Enably only if you need that, and only do it manually. Never ship with this enabled (this is enforced below)
 */
#define UE4_POOL_BAFO_ALLOCATIONS_DEBUG_OOM				(0 && UE4_POOL_BAFO_ALLOCATIONS_DO_SANITY_CHECKS)

#if UE_BUILD_SHIPPING
	#if UE4_POOL_BAFO_ALLOCATIONS_DEBUG_OOM
		#error "UE4_POOL_BAFO_ALLOCATIONS_DEBUG_OOM enabled in a Shipping build!"
	#endif
#endif

#if UE4_POOL_BAFO_ALLOCATIONS
/**
* Class for managing allocations of the size no larger than BlockSize.
*
* @param CommitAddressRange function to let the OS know that a range needs to be backed by physical RAM
* @param EvictAddressRange function to let the OS know that the RAM pages backing an address range can be evicted.
* @param RequiredAlignment alignment for the addresses returned from this pool
*/
template<
	bool(*CommitAddressRange)(void* AddrStart, SIZE_T Size),
	bool(*EvictAddressRange)(void* AddrStart, SIZE_T Size),
	SIZE_T RequiredAlignment
>
class TMemoryPool
{
protected:

	/** Size of a single block */
	SIZE_T		BlockSize;

	/** Beginning of the pool (an address in memory), cast to SIZE_T for arithmetic ops. */
	SIZE_T		AlignedPoolStart;

	/** End of the pool (an address in memory), cast to SIZE_T for arithmetic ops. */
	SIZE_T		AlignedPoolEnd;

	/** Num of the blocks to cache */
	SIZE_T		NumBlocks;

	/** A stack of free blocks (addresses are precalculated). We pop from it on alloc and push on free, never having to search. */
	void**		FreeBlocks;

	/** Current length of the stack. */
	SIZE_T		NumFreeBlocks;

#if UE4_POOL_BAFO_ALLOCATIONS_DO_SANITY_CHECKS
	FThreadSafeCounter NoConcurrentAccess; // Tests that this is not being accessed on multiple threads at once, see TestPAL mallocthreadtest
#endif // UE4_POOL_BAFO_ALLOCATIONS_DO_SANITY_CHECKS

public:

	TMemoryPool(SIZE_T InBlockSize, SIZE_T InAlignedPoolStart, SIZE_T InNumBlocks, void** InFreeBlocks)
		: BlockSize(InBlockSize)
		, AlignedPoolStart(InAlignedPoolStart)
		, AlignedPoolEnd(InAlignedPoolStart + InBlockSize * (SIZE_T)InNumBlocks)
		, NumBlocks(InNumBlocks)
		, FreeBlocks(InFreeBlocks)
		, NumFreeBlocks(InNumBlocks)
	{
		checkf((AlignedPoolStart % RequiredAlignment) == 0, TEXT("Non-aligned pool address passed to a TMemoryPool"));

		// pre-populate list of blocks. Push them into stack in a reverse order so they are allocated from lowest to highest
		for (SIZE_T IdxBlock = NumBlocks; IdxBlock > 0; --IdxBlock)
		{
			FreeBlocks[NumBlocks - IdxBlock] = reinterpret_cast<void *>(AlignedPoolStart + (IdxBlock - 1) * BlockSize);
		}

		if (!UE4_POOL_BAFO_ALLOCATIONS_COMMIT_ON_RESERVATION)
		{
			// decommit all the memory
			EvictAddressRange(reinterpret_cast<void *>(AlignedPoolStart), NumBlocks * BlockSize);
		}
	}

	/** We always allocate in BlockSize chunks, Size is only passed for more accurate Commit() */
	void* Allocate(SIZE_T Size)
	{
#if UE4_POOL_BAFO_ALLOCATIONS_DO_SANITY_CHECKS
		checkf(Size <= BlockSize, TEXT("Attempting to allocate %llu bytes from a memory pool of %llu byte blocks"), (uint64)Size, (uint64)BlockSize);

		checkf(NoConcurrentAccess.Increment() == 1, TEXT("TMemoryPool is being accessed on multiple threads. The class is not thread safe, add locking!."));
#endif // UE4_POOL_BAFO_ALLOCATIONS_DO_SANITY_CHECKS

		void* Address = nullptr;
		if (LIKELY(NumFreeBlocks > 0))
		{
			Address = FreeBlocks[--NumFreeBlocks];

			if (!UE4_POOL_BAFO_ALLOCATIONS_COMMIT_ON_RESERVATION)
			{
				// make sure this address range is backed by the actual RAM
				CommitAddressRange(Address, Size);
			}
		}

#if UE4_POOL_BAFO_ALLOCATIONS_DO_SANITY_CHECKS
		checkf(NoConcurrentAccess.Decrement() == 0, TEXT("TMemoryPool is being accessed on multiple threads. The class is not thread safe, add locking!."));
#endif // UE4_POOL_BAFO_ALLOCATIONS_DO_SANITY_CHECKS

		return Address;
	}

	/** We always free BlockSize-d chunks, no need to pass the size. */
	void Free(void *Ptr)
	{
		// first, check if the block is ours at all. This may happen if allocations spilled over to regular BAFO() when the pool is full, but it is unlikely.
#if UE4_POOL_BAFO_ALLOCATIONS_DO_SANITY_CHECKS
		checkf(WasAllocatedFromThisPool(Ptr, BlockSize), TEXT("Address passed to Free() of a pool of block size %llu was not allocated in it (address: %p, boundaries: %p - %p"),
			(uint64)BlockSize,
			Ptr,
			reinterpret_cast<void *>(AlignedPoolStart),
			reinterpret_cast<void *>(AlignedPoolEnd)
			);

		checkf((reinterpret_cast<SIZE_T>(Ptr) % RequiredAlignment == 0), TEXT("Address passed to Free() of a pool of block size %llu was not aligned to %llu bytes (address: %p)"),
			(uint64)BlockSize,
			(uint64)RequiredAlignment,
			Ptr
			);

		checkf(NoConcurrentAccess.Increment() == 1, TEXT("TMemoryPool is being accessed on multiple threads. The class is not thread safe, add locking!."));
#endif // UE4_POOL_BAFO_ALLOCATIONS_DO_SANITY_CHECKS

		// add the pointer back to the pool
		FreeBlocks[NumFreeBlocks++] = Ptr;

		if (!UE4_POOL_BAFO_ALLOCATIONS_COMMIT_ON_RESERVATION)
		{
			// evict this memory
			EvictAddressRange(Ptr, BlockSize);
		}

#if UE4_POOL_BAFO_ALLOCATIONS_DO_SANITY_CHECKS
		// check that we never free more than we allocated
		checkf(NumFreeBlocks <= NumBlocks, TEXT("Too many frees!"));

		// check that the block is not already in the stack
		for (SIZE_T IdxAlreadyFree = 0; IdxAlreadyFree < NumFreeBlocks - 1; ++IdxAlreadyFree)
		{
			if (FreeBlocks[IdxAlreadyFree] == Ptr)
			{
				UE_LOG(LogInit, Fatal, TEXT("Freeing the same block %p twice! New index in stack would be %d, but it is already at index %d"), Ptr, NumFreeBlocks - 1, IdxAlreadyFree);
				// unreachable
				return;
			}
		}

		checkf(NoConcurrentAccess.Decrement() == 0, TEXT("TMemoryPool is being accessed on multiple threads. The class is not thread safe, add locking!."));
#endif // UE4_POOL_BAFO_ALLOCATIONS_DO_SANITY_CHECKS
	}

	/** Returns true if we can allocate this much memory from this pool. */
	bool CanAllocateFromThisPool(SIZE_T Size)
	{
		return BlockSize >= Size;
	}

	/** Returns true if this allocation came from this pool. */
	bool WasAllocatedFromThisPool(void* Ptr, SIZE_T Size)
	{
		return BlockSize >= Size && reinterpret_cast<SIZE_T>(Ptr) >= AlignedPoolStart && reinterpret_cast<SIZE_T>(Ptr) < AlignedPoolEnd;
	}

	bool IsEmpty() const
	{
		return NumFreeBlocks == NumBlocks;
	}

	void PrintDebugInfo()
	{
		printf("BlockSize: %llu NumAllocated/TotalBlocks = %d/%d\n", (uint64)BlockSize, (NumBlocks - NumFreeBlocks), NumBlocks);
	}
};

/**
 * Class for managing a (small) number of pools.
 *
 * @param ReserveAddressRange function to reserve the address range (mmap() on Linux, VirtualAlloc(MEM_RESERVE) on Windows). Failure is fatal.
 * @param FreeAddressRange function to free the address range (munmap on Linux, VirtualFree() on Windows). It is guaranteed that Size will exactly match the size requested in ReserveAddressRange,
 *                         Address will be the same as the pointer returned from RAR, and the number of RAR/FAR calls will match. Address range being freed does not have to be evicted first.
 * @param CommitAddressRange function to let the OS know that a range needs to be backed by physical RAM
 * @param EvictAddressRange function to let the OS know that the RAM pages backing an address range can be evicted.
 * @param RequiredAlignment alignment for the addresses returned from this pool
 * @param ExtraSizeToAllocate if ReserveAddressRange does not return addresses aligned to RequiredAlignment, pass the size of slack that we need to add to the allocation so we can find an aligned address
			in it. For instance, if RequiredAlignment is 64KB and RAR() returns 4KB-aligned addresses (Linux situation with mmap()), pass 60KB.
			If RequiredAlignment is 64KB and RAR() returns 64KB-aligned addresses (Windows situation with VirtualAlloc), pass 0.
			General formula is ExtraSizeToAllocate = RequiredAlignment - AlignmentOfAddressesReturnedFromRAR.
*/
template<
	bool(*ReserveAddressRange)(void** OutReturnedPointer, SIZE_T Size),
	bool(*FreeAddressRange)(void* Address, SIZE_T Size),
	bool(*CommitAddressRange)(void* AddrStart, SIZE_T Size),
	bool(*EvictAddressRange)(void* AddrStart, SIZE_T Size),
	SIZE_T RequiredAlignment,
	SIZE_T ExtraSizeToAllocate
>
class TMemoryPoolArray
{
protected:

	typedef TMemoryPool<
		CommitAddressRange,
		EvictAddressRange,
		RequiredAlignment
	>
		TBlockPool;

	/** Pointer to the pool (possibly misaligned). */
	void*		SinglePoolStart;

	/** Size of the total allocated memory */
	SIZE_T		TotalAllocatedSize;

	/** Total number of pools in the array*/
	SIZE_T		NumPools;

	/** Pointers to pools that hold block-sized allocations. */
	TBlockPool** Pools;

	/** Internal variable to speed up allocation when the size requested is larger than any pool. */
	SIZE_T		LargestPooledBlockSize;

	/** Rounds the memory */
	SIZE_T		RoundToRequiredAlignment(SIZE_T Size)
	{
		if ((Size % RequiredAlignment) != 0)
		{
			Size += (RequiredAlignment - (Size % RequiredAlignment));
		}
		return Size;
	}

	void CalculateTotalAllocationSize(int32* PoolTable, SIZE_T& OutTotalMemoryNeeded, SIZE_T& OutNumPools)
	{
		OutTotalMemoryNeeded = 0;
		OutNumPools = 0;
		for (int32* PoolPtr = PoolTable; *PoolPtr != -1; PoolPtr += 2, ++OutNumPools)
		{
			SIZE_T BlockSize = static_cast<SIZE_T>(PoolPtr[0]);
			SIZE_T NumBlocks = static_cast<SIZE_T>(PoolPtr[1]);

			// memory for the pool itself
			OutTotalMemoryNeeded += (BlockSize * NumBlocks);

			// and for its bookkeeping data
			OutTotalMemoryNeeded += RoundToRequiredAlignment(NumBlocks * sizeof(void *));
		}

		// account for possible need to align the starting address
		OutTotalMemoryNeeded += ExtraSizeToAllocate;

		// add memory needed to TBlockPool* array
		OutTotalMemoryNeeded += RoundToRequiredAlignment(OutNumPools * sizeof(TBlockPool*));

		// add memory needed to hold TBlockPool-s themselves
		OutTotalMemoryNeeded += RoundToRequiredAlignment(OutNumPools * sizeof(TBlockPool));
	}

public:

	/**
	 * PoolTable is used to describe the array of pools.
	 *	Format:
	 *	  Each entry is two int32, one is block size in bytes, another is number of such blocks in pool.
	 *    Block size should be divisible by RequiredAlignment
	 *	  -1 for the first number is the end marker.
	 *	  Entries should be sorted ascending by block size.
	 */
	TMemoryPoolArray(int32 *PoolTable)
		: SinglePoolStart(nullptr)
		, TotalAllocatedSize(0)
		, NumPools(0)
		, Pools(nullptr)
		, LargestPooledBlockSize(0)
	{
		checkf(PoolTable, TEXT("MemoryPoolArray should be initialized with a valid pool table."));

		// this all needs to be done on constructor because only constructor is thread-safe
		CalculateTotalAllocationSize(PoolTable, TotalAllocatedSize, NumPools);

		checkf(NumPools > 0, TEXT("MemoryPoolArray should be initialized number of pools > 0."));
		checkf(TotalAllocatedSize > 0, TEXT("Overall pool size should be non-zero."));

		if (!ReserveAddressRange(&SinglePoolStart, TotalAllocatedSize) || SinglePoolStart == nullptr)
		{
			UE_LOG(LogInit, Fatal, TEXT("Could not allocate memory (%llu bytes, %llu KB, %llu MB) for MemoryPool."), (uint64)TotalAllocatedSize, (uint64)TotalAllocatedSize / 1024, (uint64)TotalAllocatedSize / (1024 * 1024));
			return; // unreachable
		}

		if (UE4_POOL_BAFO_ALLOCATIONS_COMMIT_ON_RESERVATION)
		{
			// commit everything. Note that it will happen more than once due to other Commit calls
			CommitAddressRange(SinglePoolStart, TotalAllocatedSize);
		}

		SIZE_T AlignedPoolStart = reinterpret_cast<SIZE_T>(SinglePoolStart);
		if ((AlignedPoolStart % RequiredAlignment) != 0)
		{
			AlignedPoolStart += (RequiredAlignment - (AlignedPoolStart % RequiredAlignment));
		}

		// create pools
		// use part of pooled memory to store pointers to them
		Pools = reinterpret_cast<TBlockPool **>(AlignedPoolStart);
		SIZE_T PoolPointersMemorySize = RoundToRequiredAlignment(NumPools * sizeof(TBlockPool*));
		CommitAddressRange(Pools, PoolPointersMemorySize);
		AlignedPoolStart += PoolPointersMemorySize;

		// use part of pooled memory to store their contents
		TBlockPool* PreallocatedMemoryForPools = reinterpret_cast<TBlockPool *>(AlignedPoolStart);
		SIZE_T PreallocatedMemoryForPoolsSize = RoundToRequiredAlignment(NumPools * sizeof(TBlockPool));
		CommitAddressRange(PreallocatedMemoryForPools, PreallocatedMemoryForPoolsSize);
		AlignedPoolStart += PreallocatedMemoryForPoolsSize;

		int PoolIdx = 0;
		SIZE_T PreviousBlockSize = 0;
		for (int32* PoolPtr = PoolTable; *PoolPtr != -1; PoolPtr += 2, ++PoolIdx)
		{
			checkf(PoolIdx < NumPools, TEXT("Internal error: pool table contains more elements than we calculated initially."));

			SIZE_T BlockSize = static_cast<SIZE_T>(PoolPtr[0]);
			SIZE_T NumBlocksInPool = static_cast<SIZE_T>(PoolPtr[1]);

			checkf(PreviousBlockSize < BlockSize, TEXT("Pools in the pool table should be sorted ascending by block sizes"));
			PreviousBlockSize = BlockSize;
			LargestPooledBlockSize = BlockSize;	// by the end of the loop will be the largest due to above requirement

			checkf((BlockSize % RequiredAlignment) == 0, TEXT("Block size should be divisible by required alignment since blocks will be tightly packed."));

			// put book-keeping memory first
			void* BookkeepingMemory = reinterpret_cast<void *>(AlignedPoolStart);
			SIZE_T BookkeepingMemorySize = RoundToRequiredAlignment(NumBlocksInPool * sizeof(void *));
			CommitAddressRange(BookkeepingMemory, BookkeepingMemorySize);
			AlignedPoolStart += BookkeepingMemorySize;

			Pools[PoolIdx] = new (&PreallocatedMemoryForPools[PoolIdx]) TBlockPool(BlockSize, AlignedPoolStart, NumBlocksInPool, reinterpret_cast<void**>(BookkeepingMemory));
			AlignedPoolStart += NumBlocksInPool * BlockSize; // aligned pool start should be aligned because we require block size to be aligned by RequiredAlignment
		}
	}

	~TMemoryPoolArray()
	{
		for (SIZE_T IdxPool = 0; IdxPool < NumPools; ++IdxPool)
		{
			// since those were allocated with placement new, do not call delete
			Pools[IdxPool]->~TBlockPool();
		}

		if (SinglePoolStart)
		{
			FreeAddressRange(SinglePoolStart, TotalAllocatedSize);
			SinglePoolStart = nullptr;
		}
	}

	/** Tries to allocate in pooled blocks. If it cannot, BAFO() needs to deal with this using regular OS allocs. */
	void* Allocate(SIZE_T Size)
	{
		// early return if no pool will accommodate this size
		if (UNLIKELY(LargestPooledBlockSize < Size))
		{
			return nullptr;
		}

		// relying on the fact that the pools are sorted ascending by pool sizes
		for (int32 IdxPool = 0; IdxPool < NumPools; ++IdxPool)
		{
			if (LIKELY(Pools[IdxPool]->CanAllocateFromThisPool(Size)))
			{
				// allocate can return null when the pool is full
				void* Ret = Pools[IdxPool]->Allocate(Size);
				if (LIKELY(Ret != nullptr))
				{
					return Ret;
				}

				// otherwise, try a block of larger size
			}
		}

		return nullptr;
	}

	/** Tries to free in pooled blocks. If the allocation is not from a pooled block, will return false, allowing BAFO() to free it. */
	bool Free(void* Ptr, SIZE_T Size)
	{
		// relying on the fact that the pools are sorted ascending by pool sizes
		for (SIZE_T IdxPool = 0; IdxPool < NumPools; ++IdxPool)
		{
			if (LIKELY(Pools[IdxPool]->WasAllocatedFromThisPool(Ptr, Size)))
			{
				Pools[IdxPool]->Free(Ptr);
				return true;
			}
		}

		return false;
	}

	void PrintDebugInfo()
	{
		for (SIZE_T IdxPool = 0; IdxPool < NumPools; ++IdxPool)
		{
			Pools[IdxPool]->PrintDebugInfo();
		}
	}
};

#endif // UE4_POOL_BAFO_ALLOCATIONS


// This histogram is a separate helper code not intended to be used in production unlike the above pool. It helps understand what pool sizes are needed.

/** Top block sizes as of now:

Bucket     0:	hits  7146,	size            65536 (0x       10000),	Total memory requested: 446 MB,   457344 KB,  468320256 bytes
Bucket     1:	hits   394,	size            49152 (0x        c000),	Total memory requested:  18 MB,    18912 KB,   19365888 bytes
Bucket     2:	hits    99,	size            81920 (0x       14000),	Total memory requested:   7 MB,     7920 KB,    8110080 bytes
Bucket     3:	hits    97,	size           262144 (0x       40000),	Total memory requested:  24 MB,    24832 KB,   25427968 bytes
Bucket     4:	hits    84,	size            98304 (0x       18000),	Total memory requested:   7 MB,     8064 KB,    8257536 bytes
Bucket     5:	hits    67,	size           131072 (0x       20000),	Total memory requested:   8 MB,     8576 KB,    8781824 bytes
Bucket     6:	hits    46,	size           147456 (0x       24000),	Total memory requested:   6 MB,     6624 KB,    6782976 bytes
Bucket     7:	hits    42,	size           114688 (0x       1c000),	Total memory requested:   4 MB,     4704 KB,    4816896 bytes
Bucket     8:	hits    35,	size           212992 (0x       34000),	Total memory requested:   7 MB,     7280 KB,    7454720 bytes
Bucket     9:	hits    28,	size           180224 (0x       2c000),	Total memory requested:   4 MB,     4928 KB,    5046272 bytes
Bucket    10:	hits    23,	size           196608 (0x       30000),	Total memory requested:   4 MB,     4416 KB,    4521984 bytes
*/

#if UE4_POOL_BAFO_ALLOCATIONS_DEBUG_OOM
namespace AllocationHistogram
{
	enum
	{
		MaxAllocs = 4 * 1024 * 1024
	};

	SIZE_T		Sizes[MaxAllocs];
	int			CurAlloc = 0;

	void PrintDebugInfo()
	{
		printf("Totaling size of %d allocations\n", CurAlloc);

		struct Bucket
		{
			SIZE_T Size;
			int	NumHits;
		};

		Bucket Histogram[MaxAllocs / 1024];
		int NumUsedBuckets = 0;

		// slow but we don't care, this is debug code optimized for ease of writing
		for (int IdxAlloc = 0; IdxAlloc < CurAlloc; ++IdxAlloc)
		{
			SIZE_T ThisAllocSize = Sizes[IdxAlloc];

			// find or create a bucket
			int IdxBucket = 0;
			for (; IdxBucket < NumUsedBuckets; ++IdxBucket)
			{
				if (Histogram[IdxBucket].Size == ThisAllocSize)
				{
					++Histogram[IdxBucket].NumHits;
					break;
				}
			}

			// if there was no bucket, create one
			if (IdxBucket == NumUsedBuckets)
			{
				Histogram[IdxBucket].Size = ThisAllocSize;
				Histogram[IdxBucket].NumHits = 1;
				++NumUsedBuckets;

				if (NumUsedBuckets >= ARRAY_COUNT(Histogram))
				{
					fprintf(stderr, "Too few (%d) buckets for %d allocations! Increase the number of buckets. Crashing.\n", NumUsedBuckets - 1, CurAlloc);
					FPlatformMisc::RaiseException(1);
				}
			}
		}

		// print the histogram
		printf("Total different buckets: %d\n", NumUsedBuckets);

		// find the largest one and print, then exclude one by one.
		// Again, slow but easy to write
		for (int IdxBucket = 0; IdxBucket < NumUsedBuckets; ++IdxBucket)
		{
			// find max
			int MaxBucket = 0;
			for (int IdxPossiblyMaxBucket = 0; IdxPossiblyMaxBucket < NumUsedBuckets; ++IdxPossiblyMaxBucket)
			{
				if (Histogram[MaxBucket].NumHits < Histogram[IdxPossiblyMaxBucket].NumHits)
				{
					MaxBucket = IdxPossiblyMaxBucket;
				}
			}

			// print and zero out to ensure we won't print it again
			uint64 TotalMemoryInBucket = (uint64)Histogram[MaxBucket].Size * (uint64)Histogram[MaxBucket].NumHits;
			printf("Bucket %5d:\thits %5d,\tsize %16llu (0x%12llx),\tTotal memory requested: %3llu MB, %8llu KB, %10llu bytes\n", IdxBucket, Histogram[MaxBucket].NumHits, (uint64)Histogram[MaxBucket].Size, (uint64)Histogram[MaxBucket].Size, TotalMemoryInBucket / (1024 * 1024), TotalMemoryInBucket / 1024, TotalMemoryInBucket);
			Histogram[MaxBucket].NumHits = 0;
		}

		printf("Done!\n");
	}

	struct TallyAllocHistogram
	{
		~TallyAllocHistogram()
		{
			PrintDebugInfo();
		}
	}
	ExecOnShutdown;
};
#endif // UE4_MEMORY_POOL_DEBUG_OOM
