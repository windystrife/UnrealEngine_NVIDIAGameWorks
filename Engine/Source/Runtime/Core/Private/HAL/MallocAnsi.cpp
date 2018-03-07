// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MallocAnsi.cpp: Binned memory allocator
=============================================================================*/

#include "HAL/MallocAnsi.h"
#include "Misc/AssertionMacros.h"
#include "Math/UnrealMathUtility.h"
#include "Templates/AlignmentTemplates.h"

#if PLATFORM_LINUX
	#include <malloc.h>
#endif // PLATFORM_LINUX

#if PLATFORM_IOS
	#include "mach/mach.h"
#endif

#if PLATFORM_WINDOWS
	#include "WindowsHWrapper.h"
#endif

FMallocAnsi::FMallocAnsi()
{
#if PLATFORM_WINDOWS
	// Enable low fragmentation heap - http://msdn2.microsoft.com/en-US/library/aa366750.aspx
	intptr_t	CrtHeapHandle = _get_heap_handle();
	ULONG		EnableLFH = 2;
	HeapSetInformation((void*)CrtHeapHandle, HeapCompatibilityInformation, &EnableLFH, sizeof(EnableLFH));
#endif
}

void* FMallocAnsi::Malloc( SIZE_T Size, uint32 Alignment )
{
	IncrementTotalMallocCalls();
	Alignment = FMath::Max(Size >= 16 ? (uint32)16 : (uint32)8, Alignment);

#if USE_ALIGNED_MALLOC
	void* Result = _aligned_malloc( Size, Alignment );
#elif PLATFORM_LINUX
	void* Result;
	if (UNLIKELY(posix_memalign(&Result, Alignment, Size) != 0))
	{
		Result = nullptr;
	}
#else
	void* Ptr = malloc( Size + Alignment + sizeof(void*) + sizeof(SIZE_T) );
	check(Ptr);
	void* Result = Align( (uint8*)Ptr + sizeof(void*) + sizeof(SIZE_T), Alignment );
	*((void**)( (uint8*)Result - sizeof(void*)					))	= Ptr;
	*((SIZE_T*)( (uint8*)Result - sizeof(void*) - sizeof(SIZE_T)	))	= Size;
#endif

	if (Result == nullptr)
	{
		FPlatformMemory::OnOutOfMemory(Size, Alignment);
	}
	return Result;
}

void* FMallocAnsi::Realloc( void* Ptr, SIZE_T NewSize, uint32 Alignment )
{
	IncrementTotalReallocCalls();
	void* Result;
	Alignment = FMath::Max(NewSize >= 16 ? (uint32)16 : (uint32)8, Alignment);

#if USE_ALIGNED_MALLOC
	if( Ptr && NewSize )
	{
		Result = _aligned_realloc( Ptr, NewSize, Alignment );
	}
	else if( Ptr == nullptr )
	{
		Result = _aligned_malloc( NewSize, Alignment );
	}
	else
	{
		_aligned_free( Ptr );
		Result = nullptr;
	}
#elif PLATFORM_LINUX
	if( Ptr && NewSize )
	{
		SIZE_T UsableSize = malloc_usable_size( Ptr );
		if (UNLIKELY(posix_memalign(&Result, Alignment, NewSize) != 0))
		{
			Result = nullptr;
		}
		if (LIKELY(UsableSize))
		{
			FMemory::Memcpy( Result, Ptr, FMath::Min( NewSize, UsableSize ) );
		}
		free( Ptr );
	}
	else if( Ptr == nullptr )
	{
		if (UNLIKELY(posix_memalign(&Result, Alignment, NewSize) != 0))
		{
			Result = nullptr;
		}
	}
	else
	{
		free( Ptr );
		Result = nullptr;
	}
#else
	if( Ptr && NewSize )
	{
		// Can't use realloc as it might screw with alignment.
		Result = Malloc( NewSize, Alignment );
		SIZE_T PtrSize = 0;
		GetAllocationSize(Ptr,PtrSize);
		FMemory::Memcpy( Result, Ptr, FMath::Min(NewSize, PtrSize ) );
		Free( Ptr );
	}
	else if( Ptr == nullptr )
	{
		Result = Malloc( NewSize, Alignment);
	}
	else
	{
		free( *((void**)((uint8*)Ptr-sizeof(void*))) );
		Result = nullptr;
	}
#endif
	if (Result == nullptr && NewSize != 0)
	{
		FPlatformMemory::OnOutOfMemory(NewSize, Alignment);
	}

	return Result;
}

void FMallocAnsi::Free( void* Ptr )
{
	IncrementTotalFreeCalls();
#if USE_ALIGNED_MALLOC
	_aligned_free( Ptr );
#elif PLATFORM_LINUX
	free( Ptr );
#else
	if( Ptr )
	{
		free( *((void**)((uint8*)Ptr-sizeof(void*))) );
	}
#endif
}

bool FMallocAnsi::GetAllocationSize( void *Original, SIZE_T &SizeOut )
{
	if (!Original)
	{
		return false;
	}
#if	USE_ALIGNED_MALLOC
	SizeOut = _aligned_msize( Original,16,0 ); // Assumes alignment of 16
#elif PLATFORM_LINUX
	SizeOut = malloc_usable_size( Original );
#else
	SizeOut = *( (SIZE_T*)( (uint8*)Original - sizeof(void*) - sizeof(SIZE_T)) );	
#endif // USE_ALIGNED_MALLOC
	return true;
}

bool FMallocAnsi::IsInternallyThreadSafe() const
{
#if PLATFORM_MAC
		return true;
#elif PLATFORM_IOS
		return true;
#elif PLATFORM_LINUX
		return true;	// malloc()/free() is thread-safe on Linux
#else
		return false;
#endif
}

bool FMallocAnsi::ValidateHeap()
{
#if PLATFORM_WINDOWS
	int32 Result = _heapchk();
	check(Result != _HEAPBADBEGIN);
	check(Result != _HEAPBADNODE);
	check(Result != _HEAPBADPTR);
	check(Result != _HEAPEMPTY);
	check(Result == _HEAPOK);
#else
	return true;
#endif
	return true;
}
