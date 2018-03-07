// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Misc/NoopCounter.h"
#include "Containers/LockFreeList.h"

/**
* Thread safe, lock free pooling allocator of fixed size blocks that
* never returns free space, even at shutdown
* alignment isn't handled, assumes FMemory::Malloc will work
*/

#define USE_NIEVE_TLockFreeFixedSizeAllocator_TLSCacheBase (0) // this is useful for find who really leaked
template<int32 SIZE, typename TBundleRecycler, typename TTrackingCounter = FNoopCounter>
class TLockFreeFixedSizeAllocator_TLSCacheBase : public FNoncopyable
{
	enum
	{
		NUM_PER_BUNDLE = 32,
	};
public:

	TLockFreeFixedSizeAllocator_TLSCacheBase()
	{
		static_assert(SIZE >= sizeof(void*) && SIZE % sizeof(void*) == 0, "Blocks in TLockFreeFixedSizeAllocator must be at least the size of a pointer.");
		check(IsInGameThread());
		TlsSlot = FPlatformTLS::AllocTlsSlot();
		check(FPlatformTLS::IsValidTlsSlot(TlsSlot));
	}
	/** Destructor, leaks all of the memory **/
	~TLockFreeFixedSizeAllocator_TLSCacheBase()
	{
		FPlatformTLS::FreeTlsSlot(TlsSlot);
		TlsSlot = 0;
	}

	/**
	* Allocates a memory block of size SIZE.
	*
	* @return Pointer to the allocated memory.
	* @see Free
	*/
	FORCEINLINE void* Allocate()
	{
#if USE_NIEVE_TLockFreeFixedSizeAllocator_TLSCacheBase || PLATFORM_HTML5
		return FMemory::Malloc(SIZE);
#else
		FThreadLocalCache& TLS = GetTLS();

		if (!TLS.PartialBundle)
		{
			if (TLS.FullBundle)
			{
				TLS.PartialBundle = TLS.FullBundle;
				TLS.FullBundle = nullptr;
			}
			else
			{
				TLS.PartialBundle = GlobalFreeListBundles.Pop();
				if (!TLS.PartialBundle)
				{
					TLS.PartialBundle = (void**)FMemory::Malloc(SIZE * NUM_PER_BUNDLE);
					void **Next = TLS.PartialBundle;
					for (int32 Index = 0; Index < NUM_PER_BUNDLE - 1; Index++)
					{
						void* NextNext = (void*)(((uint8*)Next) + SIZE);
						*Next = NextNext;
						Next = (void**)NextNext;
					}
					*Next = nullptr;
					NumFree.Add(NUM_PER_BUNDLE);
				}
			}
			TLS.NumPartial = NUM_PER_BUNDLE;
		}
		NumUsed.Increment();
		NumFree.Decrement();
		void* Result = (void*)TLS.PartialBundle;
		TLS.PartialBundle = (void**)*TLS.PartialBundle;
		TLS.NumPartial--;
		check(TLS.NumPartial >= 0 && ((!!TLS.NumPartial) == (!!TLS.PartialBundle)));
		return Result;
#endif
	}

	/**
	* Puts a memory block previously obtained from Allocate() back on the free list for future use.
	*
	* @param Item The item to free.
	* @see Allocate
	*/
	FORCEINLINE void Free(void *Item)
	{
#if USE_NIEVE_TLockFreeFixedSizeAllocator_TLSCacheBase || PLATFORM_HTML5
		return FMemory::Free(Item);
#else
		NumUsed.Decrement();
		NumFree.Increment();
		FThreadLocalCache& TLS = GetTLS();
		if (TLS.NumPartial >= NUM_PER_BUNDLE)
		{
			if (TLS.FullBundle)
			{
				GlobalFreeListBundles.Push(TLS.FullBundle);
				//TLS.FullBundle = nullptr;
			}
			TLS.FullBundle = TLS.PartialBundle;
			TLS.PartialBundle = nullptr;
			TLS.NumPartial = 0;
		}
		*(void**)Item = (void*)TLS.PartialBundle;
		TLS.PartialBundle = (void**)Item;
		TLS.NumPartial++;
#endif
	}

	/**
	* Gets the number of allocated memory blocks that are currently in use.
	*
	* @return Number of used memory blocks.
	* @see GetNumFree
	*/
	const TTrackingCounter& GetNumUsed() const
	{
		return NumUsed;
	}

	/**
	* Gets the number of allocated memory blocks that are currently unused.
	*
	* @return Number of unused memory blocks.
	* @see GetNumUsed
	*/
	const TTrackingCounter& GetNumFree() const
	{
		return NumFree;
	}

private:

	/** struct for the TLS cache. */
	struct FThreadLocalCache
	{
		void **FullBundle;
		void **PartialBundle;
		int32 NumPartial;

		FThreadLocalCache()
			: FullBundle(nullptr)
			, PartialBundle(nullptr)
			, NumPartial(0)
		{
		}
	};

	FThreadLocalCache& GetTLS()
	{
		checkSlow(FPlatformTLS::IsValidTlsSlot(TlsSlot));
		FThreadLocalCache* TLS = (FThreadLocalCache*)FPlatformTLS::GetTlsValue(TlsSlot);
		if (!TLS)
		{
			TLS = new FThreadLocalCache();
			FPlatformTLS::SetTlsValue(TlsSlot, TLS);
		}
		return *TLS;
	}

	/** Slot for TLS struct. */
	uint32 TlsSlot;

	/** Lock free list of free memory blocks, these are all linked into a bundle of NUM_PER_BUNDLE. */
	TBundleRecycler GlobalFreeListBundles;

	/** Total number of blocks outstanding and not in the free list. */
	TTrackingCounter NumUsed;

	/** Total number of blocks in the free list. */
	TTrackingCounter NumFree;
};

/**
 * Thread safe, lock free pooling allocator of fixed size blocks that
 * never returns free space until program shutdown.
 * alignment isn't handled, assumes FMemory::Malloc will work
 */
template<int32 SIZE, int TPaddingForCacheContention, typename TTrackingCounter = FNoopCounter>
class TLockFreeFixedSizeAllocator
{
public:

	/** Destructor, returns all memory via FMemory::Free **/
	~TLockFreeFixedSizeAllocator()
	{
		check(!NumUsed.GetValue());
		while (void* Mem = FreeList.Pop())
		{
			FMemory::Free(Mem);
			NumFree.Decrement();
		}
		check(!NumFree.GetValue());
	}

	/**
	 * Allocates a memory block of size SIZE.
	 *
	 * @return Pointer to the allocated memory.
	 * @see Free
	 */
	void* Allocate()
	{
		NumUsed.Increment();
		void *Memory = FreeList.Pop();
		if (Memory)
		{
			NumFree.Decrement();
		}
		else
		{
			Memory = FMemory::Malloc(SIZE);
		}
		return Memory;
	}

	/**
	 * Puts a memory block previously obtained from Allocate() back on the free list for future use.
	 *
	 * @param Item The item to free.
	 * @see Allocate
	 */
	void Free(void *Item)
	{
		NumUsed.Decrement();
		FreeList.Push(Item);
		NumFree.Increment();
	}

	/**
	 * Gets the number of allocated memory blocks that are currently in use.
	 *
	 * @return Number of used memory blocks.
	 * @see GetNumFree
	 */
	const TTrackingCounter& GetNumUsed() const
	{
		return NumUsed;
	}

	/**
	 * Gets the number of allocated memory blocks that are currently unused.
	 *
	 * @return Number of unused memory blocks.
	 * @see GetNumUsed
	 */
	const TTrackingCounter& GetNumFree() const
	{
		return NumFree;
	}

private:

	/** Lock free list of free memory blocks. */
	TLockFreePointerListUnordered<void, TPaddingForCacheContention> FreeList;

	/** Total number of blocks outstanding and not in the free list. */
	TTrackingCounter NumUsed; 

	/** Total number of blocks in the free list. */
	TTrackingCounter NumFree;
};

/**
 * Thread safe, lock free pooling allocator of fixed size blocks that
 * never returns free space, even at shutdown
 * alignment isn't handled, assumes FMemory::Malloc will work
 */
template<int32 SIZE, int TPaddingForCacheContention, typename TTrackingCounter = FNoopCounter>
class TLockFreeFixedSizeAllocator_TLSCache : public TLockFreeFixedSizeAllocator_TLSCacheBase<SIZE, TLockFreePointerListUnordered<void*, TPaddingForCacheContention>, TTrackingCounter>
{
};

/**
 * Thread safe, lock free pooling allocator of memory for instances of T.
 *
 * Never returns free space until program shutdown.
 */
template<class T, int TPaddingForCacheContention>
class TLockFreeClassAllocator : private TLockFreeFixedSizeAllocator<sizeof(T), TPaddingForCacheContention>
{
public:
	/**
	 * Returns a memory block of size sizeof(T).
	 *
	 * @return Pointer to the allocated memory.
	 * @see Free, New
	 */
	void* Allocate()
	{
		return TLockFreeFixedSizeAllocator<sizeof(T), TPaddingForCacheContention>::Allocate();
	}

	/**
	 * Returns a new T using the default constructor.
	 *
	 * @return Pointer to the new object.
	 * @see Allocate, Free
	 */
	T* New()
	{
		return new (Allocate()) T();
	}

	/**
	 * Calls a destructor on Item and returns the memory to the free list for recycling.
	 *
	 * @param Item The item whose memory to free.
	 * @see Allocate, New
	 */
	void Free(T *Item)
	{
		Item->~T();
		TLockFreeFixedSizeAllocator<sizeof(T), TPaddingForCacheContention>::Free(Item);
	}
};

/**
 * Thread safe, lock free pooling allocator of memory for instances of T.
 *
 * Never returns free space until program shutdown.
 */
template<class T, int TPaddingForCacheContention>
class TLockFreeClassAllocator_TLSCache : private TLockFreeFixedSizeAllocator_TLSCache<sizeof(T), TPaddingForCacheContention>
{
public:
	/**
	 * Returns a memory block of size sizeof(T).
	 *
	 * @return Pointer to the allocated memory.
	 * @see Free, New
	 */
	void* Allocate()
	{
		return TLockFreeFixedSizeAllocator_TLSCache<sizeof(T), TPaddingForCacheContention>::Allocate();
	}

	/**
	 * Returns a new T using the default constructor.
	 *
	 * @return Pointer to the new object.
	 * @see Allocate, Free
	 */
	T* New()
	{
		return new (Allocate()) T();
	}

	/**
	 * Calls a destructor on Item and returns the memory to the free list for recycling.
	 *
	 * @param Item The item whose memory to free.
	 * @see Allocate, New
	 */
	void Free(T *Item)
	{
		Item->~T();
		TLockFreeFixedSizeAllocator_TLSCache<sizeof(T), TPaddingForCacheContention>::Free(Item);
	}
};
