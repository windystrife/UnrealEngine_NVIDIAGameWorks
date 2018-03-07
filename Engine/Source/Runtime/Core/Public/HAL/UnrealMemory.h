// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/IsPointer.h"
#include "HAL/PlatformMemory.h"
#include "HAL/MemoryBase.h"

// Sizes.

#if STATS
#define MALLOC_GT_HOOKS 1
#else
#define MALLOC_GT_HOOKS 0
#endif

#if MALLOC_GT_HOOKS
CORE_API void DoGamethreadHook(int32 Index);
#else
FORCEINLINE void DoGamethreadHook(int32 Index)
{ 
}
#endif

#define TIME_MALLOC (0 && PLATFORM_PS4)

#if TIME_MALLOC

struct CORE_API FScopedMallocTimer
{
	static uint64 GTotalCycles[4];
	static uint64 GTotalCount[4];
	static uint64 GTotalMisses[4];

	int32 Index;
	uint64 Cycles;

	FORCEINLINE FScopedMallocTimer(int32 InIndex)
		: Index(InIndex)
		, Cycles(FPlatformTime::Cycles64())
	{
	}
	FORCEINLINE ~FScopedMallocTimer()
	{
		uint64 Add = uint64(FPlatformTime::Cycles64() - Cycles);
		FPlatformAtomics::InterlockedAdd((volatile int64*)&GTotalCycles[Index], Add);
		uint64 Was = FPlatformAtomics::InterlockedAdd((volatile int64*)&GTotalCount[Index], 1);

		extern CORE_API bool GIsRunning;
		if (GIsRunning && Index == 1 && (Was & 0xffff) == 0)
		{
			Spew();
		}
	}
	static FORCEINLINE void Miss(int32 InIndex)
	{
		FPlatformAtomics::InterlockedAdd((volatile int64*)&GTotalMisses[InIndex], 1);
	}
	static void Spew();
}; 
#else
struct FScopedMallocTimer
{
	FORCEINLINE FScopedMallocTimer(int32 InIndex)
	{
	}
	FORCEINLINE ~FScopedMallocTimer()
	{
	}
	FORCEINLINE void Hit(int32 InIndex)
	{
	}
}; 
#endif




/*-----------------------------------------------------------------------------
	FMemory.
-----------------------------------------------------------------------------*/

struct CORE_API FMemory
{
	/** @name Memory functions (wrapper for FPlatformMemory) */

	static FORCEINLINE void* Memmove( void* Dest, const void* Src, SIZE_T Count )
	{
		return FPlatformMemory::Memmove( Dest, Src, Count );
	}

	static FORCEINLINE int32 Memcmp( const void* Buf1, const void* Buf2, SIZE_T Count )
	{
		return FPlatformMemory::Memcmp( Buf1, Buf2, Count );
	}

	static FORCEINLINE void* Memset(void* Dest, uint8 Char, SIZE_T Count)
	{
		return FPlatformMemory::Memset( Dest, Char, Count );
	}

	template< class T > 
	static FORCEINLINE void Memset( T& Src, uint8 ValueToSet )
	{
		static_assert( !TIsPointer<T>::Value, "For pointers use the three parameters function");
		Memset( &Src, ValueToSet, sizeof( T ) );
	}

	static FORCEINLINE void* Memzero(void* Dest, SIZE_T Count)
	{
		return FPlatformMemory::Memzero( Dest, Count );
	}

	template< class T > 
	static FORCEINLINE void Memzero( T& Src )
	{
		static_assert( !TIsPointer<T>::Value, "For pointers use the two parameters function");
		Memzero( &Src, sizeof( T ) );
	}

	static FORCEINLINE void* Memcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return FPlatformMemory::Memcpy(Dest,Src,Count);
	}

	template< class T > 
	static FORCEINLINE void Memcpy( T& Dest, const T& Src )
	{
		static_assert( !TIsPointer<T>::Value, "For pointers use the three parameters function");
		Memcpy( &Dest, &Src, sizeof( T ) );
	}

	static FORCEINLINE void* BigBlockMemcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return FPlatformMemory::BigBlockMemcpy(Dest,Src,Count);
	}

	static FORCEINLINE void* StreamingMemcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return FPlatformMemory::StreamingMemcpy(Dest,Src,Count);
	}

	static FORCEINLINE void Memswap( void* Ptr1, void* Ptr2, SIZE_T Size )
	{
		FPlatformMemory::Memswap(Ptr1,Ptr2,Size);
	}

	//
	// C style memory allocation stubs that fall back to C runtime
	//
	static FORCEINLINE void* SystemMalloc(SIZE_T Size)
	{
		return ::malloc(Size);
	}

	static FORCEINLINE void SystemFree(void* Ptr)
	{
		::free(Ptr);
	}

	//
	// C style memory allocation stubs.
	//

	static void* Malloc(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
	static void* Realloc(void* Original, SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
	static void Free(void* Original);
	static SIZE_T GetAllocSize(void* Original);
	/**
	* For some allocators this will return the actual size that should be requested to eliminate
	* internal fragmentation. The return value will always be >= Count. This can be used to grow
	* and shrink containers to optimal sizes.
	* This call is always fast and threadsafe with no locking.
	*/
	static SIZE_T QuantizeSize(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);

	/**
	* Releases as much memory as possible. Must be called from the main thread.
	*/
	static void Trim();

	/**
	* Set up TLS caches on the current thread. These are the threads that we can trim.
	*/
	static void SetupTLSCachesOnCurrentThread();

	/**
	* Clears the TLS caches on the current thread and disables any future caching.
	*/
	static void ClearAndDisableTLSCachesOnCurrentThread();

	//
	// Malloc for GPU mapped memory on UMA systems (XB1/PS4/etc)
	// It is expected that the RHI on platforms that use these knows what to 
	// do with the memory and avoids unnecessary copies into GPU resources, etc.
	//
	static void* GPUMalloc(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
	static void* GPURealloc(void* Original, SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
	static void GPUFree(void* Original);

	/**
	 * A helper function that will perform a series of random heap allocations to test
	 * the internal validity of the heap. Note, this function will "leak" memory, but another call
	 * will clean up previously allocated blocks before returning. This will help to A/B testing
	 * where you call it in a good state, do something to corrupt memory, then call this again
	 * and hopefully freeing some pointers will trigger a crash.
	 */
	static void TestMemory();
	/**
	* Called once main is started and we have -purgatorymallocproxy.
	* This uses the purgatory malloc proxy to check if things are writing to stale pointers.
	*/
	static void EnablePurgatoryTests();
	/**
	* Called once main is started and we have -purgatorymallocproxy.
	* This uses the purgatory malloc proxy to check if things are writing to stale pointers.
	*/
	static void EnablePoisonTests();
private:
	static void GCreateMalloc();
	// These versions are called either at startup or in the event of a crash
	static void* MallocExternal(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
	static void* ReallocExternal(void* Original, SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
	static void FreeExternal(void* Original);
	static SIZE_T GetAllocSizeExternal(void* Original);
	static SIZE_T QuantizeSizeExternal(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
};

#define INLINE_FMEMORY_OPERATION (0) // untested, but should work. Inlines FMemory::Malloc, etc

#if INLINE_FMEMORY_OPERATION
	#if PLATFORM_USES_FIXED_GMalloc_CLASS
		#error "PLATFORM_USES_FIXED_GMalloc_CLASS and INLINE_FMEMORY_OPERATION are not compatible. PLATFORM_USES_FIXED_GMalloc_CLASS is inlined below."
	#endif

	#define FMEMORY_INLINE_FUNCTION_DECORATOR FORCEINLINE
	#include "FMemory.inl"
#endif

#if PLATFORM_USES_FIXED_GMalloc_CLASS && !FORCE_ANSI_ALLOCATOR && USE_MALLOC_BINNED2
	#include "MallocBinned2.h"
#endif
