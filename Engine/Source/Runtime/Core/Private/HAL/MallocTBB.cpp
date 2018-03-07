// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MallocTTB.cpp: IntelTTB Malloc
=============================================================================*/

#include "HAL/MallocTBB.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/UnrealMemory.h"
#if PLATFORM_MAC
#include "Templates/AlignmentTemplates.h"
#endif

// Only use for supported platforms
#if PLATFORM_SUPPORTS_TBB && TBB_ALLOCATOR_ALLOWED

/** Value we fill a memory block with after it is free, in UE_BUILD_DEBUG **/
#define DEBUG_FILL_FREED (0xdd)

/** Value we fill a new memory block with, in UE_BUILD_DEBUG **/
#define DEBUG_FILL_NEW (0xcd)

// Statically linked tbbmalloc requires tbbmalloc_debug.lib in debug
#if UE_BUILD_DEBUG && !defined(NDEBUG)	// Use !defined(NDEBUG) to check to see if we actually are linking with Debug third party libraries (bDebugBuildsActuallyUseDebugCRT)
	#ifndef TBB_USE_DEBUG
		#define TBB_USE_DEBUG 1
	#endif
#endif
THIRD_PARTY_INCLUDES_START
#include <tbb/scalable_allocator.h>
THIRD_PARTY_INCLUDES_END

#define MEM_TIME(st)


void* FMallocTBB::Malloc( SIZE_T Size, uint32 Alignment )
{
	IncrementTotalMallocCalls();

	MEM_TIME(MemTime -= FPlatformTime::Seconds());

	void* NewPtr = NULL;
#if PLATFORM_MAC
	// macOS expects all allocations to be aligned to 16 bytes, but TBBs default alignment is 8, so on Mac we always have to use scalable_aligned_malloc.
	// Contrary to scalable_malloc, scalable_aligned_malloc returns nullptr when trying to allocate 0 bytes, which is inconsistent with system malloc, so
	// for 0 bytes requests we actually allocate sizeof(size_t), which is exactly what scalable_malloc does internally in such case.
	// scalable_aligned_realloc and scalable_realloc behave the same in this regard, so this is only needed here.
	Alignment = AlignArbitrary(FMath::Max((uint32)16, Alignment), (uint32)16);
	NewPtr = scalable_aligned_malloc(Size ? Size : sizeof(size_t), Alignment);
#else
	if( Alignment != DEFAULT_ALIGNMENT )
	{
		Alignment = FMath::Max(Size >= 16 ? (uint32)16 : (uint32)8, Alignment);
		NewPtr = scalable_aligned_malloc( Size, Alignment );
	}
	else
	{
		NewPtr = scalable_malloc( Size );
	}
#endif

	if( !NewPtr && Size )
	{
		OutOfMemory(Size, Alignment);
	}
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	else if (Size)
	{
		FMemory::Memset(NewPtr, DEBUG_FILL_NEW, Size); 
	}
#endif
	MEM_TIME(MemTime += FPlatformTime::Seconds());
	return NewPtr;
}

void* FMallocTBB::Realloc( void* Ptr, SIZE_T NewSize, uint32 Alignment )
{
	IncrementTotalReallocCalls();

	MEM_TIME(MemTime -= FPlatformTime::Seconds())
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	SIZE_T OldSize = 0;
	if (Ptr)
	{
		OldSize = scalable_msize(Ptr);
		if (NewSize < OldSize)
		{
			FMemory::Memset((uint8*)Ptr + NewSize, DEBUG_FILL_FREED, OldSize - NewSize); 
		}
	}
#endif
	void* NewPtr = NULL;
#if PLATFORM_MAC
	// macOS expects all allocations to be aligned to 16 bytes, but TBBs default alignment is 8, so on Mac we always have to use scalable_aligned_realloc
	Alignment = AlignArbitrary(FMath::Max((uint32)16, Alignment), (uint32)16);
	NewPtr = scalable_aligned_realloc(Ptr, NewSize, Alignment);
#else
	if (Alignment != DEFAULT_ALIGNMENT)
	{
		Alignment = FMath::Max(NewSize >= 16 ? (uint32)16 : (uint32)8, Alignment);
		NewPtr = scalable_aligned_realloc(Ptr, NewSize, Alignment);
	}
	else
	{
		NewPtr = scalable_realloc(Ptr, NewSize);
	}
#endif
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	if (NewPtr && NewSize > OldSize )
	{
		FMemory::Memset((uint8*)NewPtr + OldSize, DEBUG_FILL_NEW, NewSize - OldSize); 
	}
#endif
	if( !NewPtr && NewSize )
	{
		OutOfMemory(NewSize, Alignment);
	}
	MEM_TIME(MemTime += FPlatformTime::Seconds())
	return NewPtr;
}

void FMallocTBB::Free( void* Ptr )
{
	if( !Ptr )
	{
		return;
	}
	MEM_TIME(MemTime -= FPlatformTime::Seconds())
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	FMemory::Memset(Ptr, DEBUG_FILL_FREED, scalable_msize(Ptr)); 
#endif
	IncrementTotalFreeCalls();
	scalable_free(Ptr);

	MEM_TIME(MemTime += FPlatformTime::Seconds())
}

bool FMallocTBB::GetAllocationSize(void *Original, SIZE_T &SizeOut)
{
	SizeOut = scalable_msize(Original);
	return true;
}

#undef MEM_TIME

#endif // PLATFORM_SUPPORTS_TBB && TBB_ALLOCATOR_ALLOWED
