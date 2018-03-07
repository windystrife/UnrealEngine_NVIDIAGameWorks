// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HAL/MallocStomp.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/UnrealMemory.h"
#include "HAL/IConsoleManager.h"

#if PLATFORM_LINUX
	#include <sys/mman.h>
#endif

#if USE_MALLOC_STOMP

#if PLATFORM_WINDOWS || PLATFORM_XBOXONE
const uint32 FMallocStomp::NoAccessProtectMode = PAGE_NOACCESS;
#elif PLATFORM_LINUX || PLATFORM_MAC
const uint32 FMallocStomp::NoAccessProtectMode = PROT_NONE;
#else
#error The stomp allocator isn't supported in this platform.
#endif

static void MallocStompOverrunTest()
{
	const uint32 ArraySize = 4;
	uint8* Pointer = new uint8[ArraySize];
	// Overrun.
	Pointer[ArraySize+1] = 0;
}

FAutoConsoleCommand MallocStompTestCommand
(
	TEXT( "MallocStomp.OverrunTest" ),
	TEXT( "Overrun test for the FMallocStomp" ),
	FConsoleCommandDelegate::CreateStatic( &MallocStompOverrunTest )
);


void* FMallocStomp::Malloc(SIZE_T Size, uint32 Alignment)
{
	if(Size == 0U)
	{
		Size = 1U;
	}

	const SIZE_T AlignedSize = (Alignment > 0U) ? ((Size + Alignment - 1U) & -static_cast<int32>(Alignment)) : Size;
	const SIZE_T AllocFullPageSize = AlignedSize + sizeof(FAllocationData) + (PageSize - 1) & ~(PageSize - 1U);

#if PLATFORM_LINUX || PLATFORM_MAC
	// Note: can't implement BinnedAllocFromOS as a mmap call. See Free() for the reason.
	void *FullAllocationPointer = mmap(nullptr, AllocFullPageSize + PageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
#else
	void *FullAllocationPointer = FPlatformMemory::BinnedAllocFromOS(AllocFullPageSize + PageSize);
#endif // PLATFORM_LINUX || PLATFORM_MAC

	void *ReturnedPointer = nullptr;
	static const SIZE_T AllocationDataSize = sizeof(FAllocationData);

	if(bUseUnderrunMode)
	{
		const SIZE_T AlignedAllocationData = (Alignment > 0U) ? ((AllocationDataSize + Alignment - 1U) & -static_cast<int32>(Alignment)) : AllocationDataSize;
		ReturnedPointer = reinterpret_cast<void*>(reinterpret_cast<uint8*>(FullAllocationPointer) + PageSize + AlignedAllocationData);

		FAllocationData *AllocDataPtr = reinterpret_cast<FAllocationData*>(reinterpret_cast<uint8*>(FullAllocationPointer) + PageSize);
		FAllocationData AllocData = { FullAllocationPointer, AllocFullPageSize + PageSize, AlignedSize, SentinelExpectedValue };
		*AllocDataPtr = AllocData;

		// Page protect the first page, this will cause the exception in case the is an underrun.
		FPlatformMemory::PageProtect(FullAllocationPointer, PageSize, false, false);
	}
	else
	{
		ReturnedPointer = reinterpret_cast<void*>(reinterpret_cast<uint8*>(FullAllocationPointer) + AllocFullPageSize - AlignedSize);

		FAllocationData *AllocDataPtr = reinterpret_cast<FAllocationData*>(reinterpret_cast<uint8*>(ReturnedPointer) - AllocationDataSize);
		FAllocationData AllocData = { FullAllocationPointer, AllocFullPageSize + PageSize, AlignedSize, SentinelExpectedValue };
		*AllocDataPtr = AllocData;

		// Page protect the last page, this will cause the exception in case the is an overrun.
		FPlatformMemory::PageProtect(reinterpret_cast<void*>(reinterpret_cast<uint8*>(FullAllocationPointer) + AllocFullPageSize), PageSize, false, false);
	}

	return ReturnedPointer;
}

void* FMallocStomp::Realloc(void* InPtr, SIZE_T NewSize, uint32 Alignment)
{
	if(NewSize == 0U)
	{
		Free(InPtr);
		return nullptr;
	}

	void *ReturnPtr = nullptr;
	if(InPtr != nullptr)
	{
		ReturnPtr = Malloc(NewSize, Alignment);

		FAllocationData *AllocDataPtr = reinterpret_cast<FAllocationData*>(reinterpret_cast<uint8*>(InPtr) - sizeof(FAllocationData));
		FMemory::Memcpy(ReturnPtr, InPtr, FMath::Min(AllocDataPtr->Size, NewSize));
		Free(InPtr);
	}
	else
	{
		ReturnPtr = Malloc(NewSize, Alignment);
	}

	return ReturnPtr;
}

void FMallocStomp::Free(void* InPtr)
{
	if(InPtr == nullptr)
	{
		return;
	}

	FAllocationData *AllocDataPtr = reinterpret_cast<FAllocationData*>(InPtr);
	AllocDataPtr--;

	// Check that our sentinel is intact.
	if(AllocDataPtr->Sentinel != SentinelExpectedValue)
	{
		// There was a memory underrun related to this allocation.
		FPlatformMisc::DebugBreak();
	}

#if PLATFORM_LINUX || PLATFORM_MAC
	// Note: Can't wrap munmap inside BinnedFreeToOS() because the code doesn't
	// expect the size of the allocation to be freed to be available, nor the 
	// pointer be aligned with the page size. We can guarantee that here so that's
	// why we can do it.
	munmap(AllocDataPtr->FullAllocationPointer, AllocDataPtr->FullSize);
#else
	FPlatformMemory::BinnedFreeToOS(AllocDataPtr->FullAllocationPointer, AllocDataPtr->FullSize);
#endif // PLATFORM_LINUX || PLATFORM_MAC
}

bool FMallocStomp::GetAllocationSize(void *Original, SIZE_T &SizeOut) 
{
	if(Original == nullptr)
	{
		SizeOut = 0U;
	}
	else
	{
		FAllocationData *AllocDataPtr = reinterpret_cast<FAllocationData*>(Original);
		AllocDataPtr--;
		SizeOut = AllocDataPtr->Size;
	}

	return true;
}

#endif // USE_MALLOC_STOMP
