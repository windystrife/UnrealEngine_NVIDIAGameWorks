// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/GenericPlatformMemory.h"
#include "HAL/PlatformMemory.h"
#include "Misc/AssertionMacros.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/StringConv.h"
#include "UObject/NameTypes.h"
#include "Logging/LogMacros.h"
#include "Stats/Stats.h"
#include "Containers/Ticker.h"
#include "Misc/FeedbackContext.h"
#include "Async/Async.h"
#include "HAL/MallocAnsi.h"
#include "GenericPlatform/GenericPlatformMemoryPoolStats.h"
#include "HAL/MemoryMisc.h"
#include "Misc/CoreDelegates.h"
#include "HAL/LowLevelMemTracker.h"

#if PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS
	#include <sys/mman.h>
	// more mmap()-based platforms can be added
	#define UE4_PLATFORM_USES_MMAP_FOR_BINNED_OS_ALLOCS			1
#else
	#define UE4_PLATFORM_USES_MMAP_FOR_BINNED_OS_ALLOCS			0
#endif

// on 64 bit Linux, it is easier to run out of vm.max_map_count than of other limits. Due to that, trade VIRT (address space) size for smaller amount of distinct mappings
// by not leaving holes between them (kernel will coalesce the adjoining mappings into a single one)
#define UE4_PLATFORM_REDUCE_NUMBER_OF_MAPS					(PLATFORM_LINUX && PLATFORM_64BITS)

// check bookkeeping info against the passed in parameters in Debug and Development (the latter only in games and servers)
#define UE4_PLATFORM_SANITY_CHECK_OS_ALLOCATIONS			(UE_BUILD_DEBUG || (UE_BUILD_DEVELOPMENT && (UE_GAME || UE_SERVER)))

DEFINE_STAT(MCR_Physical);
DEFINE_STAT(MCR_PhysicalLLM);
DEFINE_STAT(MCR_GPU);
DEFINE_STAT(MCR_TexturePool);
DEFINE_STAT(MCR_StreamingPool);
DEFINE_STAT(MCR_UsedStreamingPool);

DEFINE_STAT(STAT_TotalPhysical);
DEFINE_STAT(STAT_TotalVirtual);
DEFINE_STAT(STAT_PageSize);
DEFINE_STAT(STAT_TotalPhysicalGB);

DEFINE_STAT(STAT_AvailablePhysical);
DEFINE_STAT(STAT_AvailableVirtual);
DEFINE_STAT(STAT_UsedPhysical);
DEFINE_STAT(STAT_PeakUsedPhysical);
DEFINE_STAT(STAT_UsedVirtual);
DEFINE_STAT(STAT_PeakUsedVirtual);

/** Helper class used to update platform memory stats. */
struct FGenericStatsUpdater
{
	/** Called once per second, enqueues stats update. */
	static bool EnqueueUpdateStats( float /*InDeltaTime*/ )
	{
		AsyncTask( ENamedThreads::AnyBackgroundThreadNormalTask, []()
		{
			DoUpdateStats();
		} );
		return true; // Tick again
	}

	/** Gathers and sets all platform memory statistics into the corresponding stats. */
	static void DoUpdateStats()
	{
		// This is slow, so do it on the task graph.
		FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
		SET_MEMORY_STAT( STAT_TotalPhysical, MemoryStats.TotalPhysical );
		SET_MEMORY_STAT( STAT_TotalVirtual, MemoryStats.TotalVirtual );
		SET_MEMORY_STAT( STAT_PageSize, MemoryStats.PageSize );
		SET_MEMORY_STAT( STAT_TotalPhysicalGB, MemoryStats.TotalPhysicalGB );

		SET_MEMORY_STAT( STAT_AvailablePhysical, MemoryStats.AvailablePhysical );
		SET_MEMORY_STAT( STAT_AvailableVirtual, MemoryStats.AvailableVirtual );
		SET_MEMORY_STAT( STAT_UsedPhysical, MemoryStats.UsedPhysical );
		SET_MEMORY_STAT( STAT_PeakUsedPhysical, MemoryStats.PeakUsedPhysical );
		SET_MEMORY_STAT( STAT_UsedVirtual, MemoryStats.UsedVirtual );
		SET_MEMORY_STAT( STAT_PeakUsedVirtual, MemoryStats.PeakUsedVirtual );

		// Platform specific stats.
		FPlatformMemory::InternalUpdateStats( MemoryStats );
	}
};

FGenericPlatformMemoryStats::FGenericPlatformMemoryStats()
	: FGenericPlatformMemoryConstants( FPlatformMemory::GetConstants() )
	, AvailablePhysical( 0 )
	, AvailableVirtual( 0 )
	, UsedPhysical( 0 )
	, PeakUsedPhysical( 0 )
	, UsedVirtual( 0 )
	, PeakUsedVirtual( 0 )
{}

bool FGenericPlatformMemory::bIsOOM = false;
uint64 FGenericPlatformMemory::OOMAllocationSize = 0;
uint32 FGenericPlatformMemory::OOMAllocationAlignment = 0;
FGenericPlatformMemory::EMemoryAllocatorToUse FGenericPlatformMemory::AllocatorToUse = Platform;
void* FGenericPlatformMemory::BackupOOMMemoryPool = nullptr;


void FGenericPlatformMemory::SetupMemoryPools()
{
	SET_MEMORY_STAT(MCR_Physical, 0); // "unlimited" physical memory for the CPU, we still need to make this call to set the short name, etc
	SET_MEMORY_STAT(MCR_PhysicalLLM, 0); // total "unlimited" physical memory, we still need to make this call to set the short name, etc
	SET_MEMORY_STAT(MCR_GPU, 0); // "unlimited" GPU memory, we still need to make this call to set the short name, etc
	SET_MEMORY_STAT(MCR_TexturePool, 0); // "unlimited" Texture memory, we still need to make this call to set the short name, etc
	SET_MEMORY_STAT(MCR_StreamingPool, 0);
	SET_MEMORY_STAT(MCR_UsedStreamingPool, 0);

	// if the platform chooses to have a BackupOOM pool, create it now
	if (FPlatformMemory::GetBackMemoryPoolSize() > 0)
	{
		LLM_PLATFORM_SCOPE(ELLMTag::BackupOOMMemoryPoolPlatform);
		LLM_SCOPE(ELLMTag::BackupOOMMemoryPool);

		BackupOOMMemoryPool = FPlatformMemory::BinnedAllocFromOS(FPlatformMemory::GetBackMemoryPoolSize());

		LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default, BackupOOMMemoryPool, FPlatformMemory::GetBackMemoryPoolSize()));
	}
}

void FGenericPlatformMemory::Init()
{
		SetupMemoryPools();

#if	STATS
	// Stats are updated only once per second.
	const float PollingInterval = 1.0f;
	FTicker::GetCoreTicker().AddTicker( FTickerDelegate::CreateStatic( &FGenericStatsUpdater::EnqueueUpdateStats ), PollingInterval );

	// Update for the first time.
	FGenericStatsUpdater::DoUpdateStats();
#endif // STATS
}

void FGenericPlatformMemory::OnOutOfMemory(uint64 Size, uint32 Alignment)
{
	// Update memory stats before we enter the crash handler.
	OOMAllocationSize = Size;
	OOMAllocationAlignment = Alignment;

	// only call this code one time - if already OOM, abort
	if (bIsOOM)
	{
		return;
	}
	bIsOOM = true;

	FPlatformMemoryStats PlatformMemoryStats = FPlatformMemory::GetStats();
	if (BackupOOMMemoryPool)
	{
		FPlatformMemory::BinnedFreeToOS(BackupOOMMemoryPool, FPlatformMemory::GetBackMemoryPoolSize());
		UE_LOG(LogMemory, Warning, TEXT("Freeing %d bytes from backup pool to handle out of memory."), FPlatformMemory::GetBackMemoryPoolSize());
		LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Default, BackupOOMMemoryPool, FPlatformMemory::GetBackMemoryPoolSize()));
	}

	UE_LOG(LogMemory, Warning, TEXT("MemoryStats:")\
		TEXT("\n\tAvailablePhysical %llu")\
		TEXT("\n\t AvailableVirtual %llu")\
		TEXT("\n\t     UsedPhysical %llu")\
		TEXT("\n\t PeakUsedPhysical %llu")\
		TEXT("\n\t      UsedVirtual %llu")\
		TEXT("\n\t  PeakUsedVirtual %llu"),
		(uint64)PlatformMemoryStats.AvailablePhysical,
		(uint64)PlatformMemoryStats.AvailableVirtual,
		(uint64)PlatformMemoryStats.UsedPhysical,
		(uint64)PlatformMemoryStats.PeakUsedPhysical,
		(uint64)PlatformMemoryStats.UsedVirtual,
		(uint64)PlatformMemoryStats.PeakUsedVirtual);
	if (GWarn)
	{
		GMalloc->DumpAllocatorStats(*GWarn);
	}

	// let any registered handlers go
	FCoreDelegates::GetMemoryTrimDelegate().Broadcast();

	UE_LOG(LogMemory, Fatal, TEXT("Ran out of memory allocating %llu bytes with alignment %u"), Size, Alignment);
}

FMalloc* FGenericPlatformMemory::BaseAllocator()
{
	return new FMallocAnsi();
}

FPlatformMemoryStats FGenericPlatformMemory::GetStats()
{
	UE_LOG(LogMemory, Warning, TEXT("FGenericPlatformMemory::GetStats not implemented on this platform"));
	return FPlatformMemoryStats();
}

void FGenericPlatformMemory::GetStatsForMallocProfiler( FGenericMemoryStats& out_Stats )
{
#if	STATS
	FPlatformMemoryStats Stats = FPlatformMemory::GetStats();

	// Base common stats for all platforms.
	out_Stats.Add( GET_STATDESCRIPTION( STAT_TotalPhysical ), Stats.TotalPhysical );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_TotalVirtual ), Stats.TotalVirtual );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_PageSize ), Stats.PageSize );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_TotalPhysicalGB ), (SIZE_T)Stats.TotalPhysicalGB );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_AvailablePhysical ), Stats.AvailablePhysical );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_AvailableVirtual ), Stats.AvailableVirtual );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_UsedPhysical ), Stats.UsedPhysical );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_PeakUsedPhysical ), Stats.PeakUsedPhysical );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_UsedVirtual ), Stats.UsedVirtual );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_PeakUsedVirtual ), Stats.PeakUsedVirtual );
#endif // STATS
}

const FPlatformMemoryConstants& FGenericPlatformMemory::GetConstants()
{
	UE_LOG(LogMemory, Warning, TEXT("FGenericPlatformMemory::GetConstants not implemented on this platform"));
	static FPlatformMemoryConstants MemoryConstants;
	return MemoryConstants;
}

uint32 FGenericPlatformMemory::GetPhysicalGBRam()
{
	return FPlatformMemory::GetConstants().TotalPhysicalGB;
}

bool FGenericPlatformMemory::PageProtect(void* const Ptr, const SIZE_T Size, const bool bCanRead, const bool bCanWrite)
{
	UE_LOG(LogMemory, Verbose, TEXT("FGenericPlatformMemory::PageProtect not implemented on this platform"));
	return false;
}

/** This structure is stored in the page before each OS allocation and checks that its properties are valid on Free. Should be less than the page size (4096 on all platforms we support) */
struct FOSAllocationDescriptor
{
	enum class MagicType : uint64
	{
		Marker = 0xd0c233ccf493dfb0
	};

	/** Magic that makes sure that we are not passed a pointer somewhere into the middle of the allocation (and/or the structure wasn't stomped). */
	MagicType	Magic;

	/** This should include the descriptor itself. */
	void*		PointerToUnmap;

	/** This should include the total size of allocation, so after unmapping it everything is gone, including the descriptor */
	SIZE_T		SizeToUnmap;

	/** Debug info that makes sure that the correct size is preserved. */
	SIZE_T		OriginalSizeAsPassed;
};

void* FGenericPlatformMemory::BinnedAllocFromOS( SIZE_T Size )
{
#if !UE4_PLATFORM_USES_MMAP_FOR_BINNED_OS_ALLOCS
	UE_LOG(LogMemory, Error, TEXT("FGenericPlatformMemory::BinnedAllocFromOS not implemented on this platform"));
	return nullptr;
#else

	static SIZE_T OSPageSize = FPlatformMemory::GetConstants().PageSize;
	// guard against someone not passing size in whole pages
	SIZE_T SizeInWholePages = (Size % OSPageSize) ? (Size + OSPageSize - (Size % OSPageSize)) : Size;
	void* Pointer = nullptr;

	// Binned expects OS allocations to be BinnedPageSize-aligned, and that page is at least 64KB. mmap() alone cannot do this, so carve out the needed chunks.
	const SIZE_T ExpectedAlignment = FPlatformMemory::GetConstants().BinnedPageSize;
	// Descriptor is only used if we're sanity checking. However, #ifdef'ing its use would make the code more fragile. Size needs to be at least one page.
	const SIZE_T DescriptorSize = (UE4_PLATFORM_REDUCE_NUMBER_OF_MAPS != 0 || UE4_PLATFORM_SANITY_CHECK_OS_ALLOCATIONS != 0) ? OSPageSize : 0;

	SIZE_T ActualSizeMapped = SizeInWholePages + ExpectedAlignment;

	// allocate with the descriptor, if any
	SIZE_T SizeWeMMaped = ActualSizeMapped + DescriptorSize;
	void* PointerWeGotFromMMap = mmap(nullptr, SizeWeMMaped, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	// store these values, since in UE4_PLATFORM_REDUCE_NUMBER_OF_MAPS we cannot rely on passed pointer and size and have to maintain our own bookkeeping info

	Pointer = PointerWeGotFromMMap;
	if (Pointer == MAP_FAILED)
	{
		const int ErrNo = errno;
		UE_LOG(LogHAL, Fatal, TEXT("mmap(len=%llu, size as passed %llu) failed with errno = %d (%s)"), (uint64)(ActualSizeMapped + DescriptorSize), (uint64)Size,
			ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
		// unreachable
		return nullptr;
	}

	SIZE_T Offset = (reinterpret_cast<SIZE_T>(Pointer) % ExpectedAlignment);

	// See if we need to unmap anything in the front. If the pointer happened to be aligned and we are not using the descriptor, we don't need to unmap anything.
	// If we're using a descriptor, we're fine if the pointer happened to be aligned on a offset that will allow us to get the expected alignment after accounting for the descriptor.
	bool bHasFrontPartToUnmap = (DescriptorSize != 0) ? (Offset != ExpectedAlignment - DescriptorSize) : (Offset != 0);

	if (LIKELY(bHasFrontPartToUnmap))
	{
		// figure out how much to unmap before the alignment. We won't unmap just enough to hold the descriptor, but this is accounted for later.
		SIZE_T SizeToNextAlignedPointer = ExpectedAlignment - Offset;
		checkf(SizeToNextAlignedPointer >= DescriptorSize, TEXT("Internal logic error in BinnedAllocFromOS, did not leave space for the allocation descriptor"));
		void* AlignedPointer = reinterpret_cast<void*>(reinterpret_cast<SIZE_T>(Pointer) + SizeToNextAlignedPointer);

		// do not unmap if we're trying to reduce the number of distinct maps, since holes prevent the Linux kernel from coalescing two adjoining mmap()s into a single VMA
		if (!UE4_PLATFORM_REDUCE_NUMBER_OF_MAPS)
		{
			// unmap the part before, but leave the space for the descriptor, if any
			if (munmap(Pointer, SizeToNextAlignedPointer - DescriptorSize) != 0)
			{
				const int ErrNo = errno;
				UE_LOG(LogHAL, Fatal, TEXT("munmap(addr=%p, len=%llu) failed with errno = %d (%s)"), Pointer, (uint64)(SizeToNextAlignedPointer - DescriptorSize),
					ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
				// unreachable
				return nullptr;
			}
		}

		// now, make it appear as if we initially got the allocation right
		Pointer = AlignedPointer;
		ActualSizeMapped -= SizeToNextAlignedPointer;
	}
	else
	{
		// we still need to advance the pointer to account for the descriptor, if any
		Pointer = reinterpret_cast<void*>(reinterpret_cast<SIZE_T>(Pointer) + DescriptorSize);
	}

	// at this point, Pointer is aligned at the expected alignment (and there's a descriptor right before it) - either we lucked out on the initial allocation
	// or we already got rid of the extra memory that was allocated in the front.
	checkf((reinterpret_cast<SIZE_T>(Pointer) % ExpectedAlignment) == 0, TEXT("BinnedAllocFromOS(): Internal error: did not align the pointer as expected."));

	// do not unmap if we're trying to reduce the number of distinct maps, since holes prevent the Linux kernel from coalescing two adjoining mmap()s into a single VMA
	if (!UE4_PLATFORM_REDUCE_NUMBER_OF_MAPS)
	{
		// Now unmap the tail only, if any. Presence or absence of the descriptor makes no difference here since neither ActualSizeMapper nor SizeInWholePages do not account for it.
		void* TailPtr = reinterpret_cast<void*>(reinterpret_cast<SIZE_T>(Pointer) + SizeInWholePages);
		SIZE_T TailSize = ActualSizeMapped - SizeInWholePages;
	
		if (LIKELY(TailSize > 0))
		{
			if (munmap(TailPtr, TailSize) != 0)
			{
				const int ErrNo = errno;
				UE_LOG(LogHAL, Fatal, TEXT("munmap(addr=%p, len=%llu) failed with errno = %d (%s)"), TailPtr, (uint64)TailSize,
					ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
				// unreachable
				return nullptr;
			}
		}
	}

	// we're done with this allocation, fill in the descriptor with the info
	if (LIKELY(DescriptorSize > 0))
	{
		FOSAllocationDescriptor* AllocDescriptor = reinterpret_cast<FOSAllocationDescriptor*>(reinterpret_cast<SIZE_T>(Pointer) - DescriptorSize);
		AllocDescriptor->Magic = FOSAllocationDescriptor::MagicType::Marker;
		if (!UE4_PLATFORM_REDUCE_NUMBER_OF_MAPS)
		{
			AllocDescriptor->PointerToUnmap = AllocDescriptor;
			AllocDescriptor->SizeToUnmap = SizeInWholePages + DescriptorSize;
		}
		else
		{
			AllocDescriptor->PointerToUnmap = PointerWeGotFromMMap;
			AllocDescriptor->SizeToUnmap = SizeWeMMaped;
		}
		AllocDescriptor->OriginalSizeAsPassed = Size;
	}

	return Pointer;
#endif // UE4_PLATFORM_USES_MMAP_FOR_BINNED_OS_ALLOCS
}

void FGenericPlatformMemory::BinnedFreeToOS( void* Ptr, SIZE_T Size )
{
#if !UE4_PLATFORM_USES_MMAP_FOR_BINNED_OS_ALLOCS
	UE_LOG(LogMemory, Error, TEXT("FGenericPlatformMemory::BinnedFreeToOS not implemented on this platform"));
#else

	// guard against someone not passing size in whole pages
	static SIZE_T OSPageSize = FPlatformMemory::GetConstants().PageSize;
	SIZE_T SizeInWholePages = (Size % OSPageSize) ? (Size + OSPageSize - (Size % OSPageSize)) : Size;

	if (UE4_PLATFORM_REDUCE_NUMBER_OF_MAPS || UE4_PLATFORM_SANITY_CHECK_OS_ALLOCATIONS)
	{
		const SIZE_T DescriptorSize = OSPageSize;

		FOSAllocationDescriptor* AllocDescriptor = reinterpret_cast<FOSAllocationDescriptor*>(reinterpret_cast<SIZE_T>(Ptr) - DescriptorSize);
		if (UNLIKELY(AllocDescriptor->Magic != FOSAllocationDescriptor::MagicType::Marker))
		{
			UE_LOG(LogHAL, Fatal, TEXT("BinnedFreeToOS() has been passed an address %p (size %llu) not allocated through it."), Ptr, (uint64)Size);
			// unreachable
			return;
		}

		void* PointerToUnmap = AllocDescriptor->PointerToUnmap;
		SIZE_T SizeToUnmap = AllocDescriptor->SizeToUnmap;

		// do checks, from most to least serious
		if (UE4_PLATFORM_SANITY_CHECK_OS_ALLOCATIONS != 0)
		{
			// this check only makes sense when we're not reducing number of maps, since the pointer will have to be different.
			if (UE4_PLATFORM_REDUCE_NUMBER_OF_MAPS == 0)
			{
				if (UNLIKELY(PointerToUnmap != AllocDescriptor || SizeToUnmap != SizeInWholePages + DescriptorSize))
				{
					UE_LOG(LogHAL, Fatal, TEXT("BinnedFreeToOS(): info mismatch: descriptor ptr: %p, size %llu, but our pointer is %p and size %llu."), PointerToUnmap, SizeToUnmap, AllocDescriptor, (uint64)(SizeInWholePages + DescriptorSize));
					// unreachable
					return;
				}
			}
	
			if (UNLIKELY(AllocDescriptor->OriginalSizeAsPassed != Size))
			{
				UE_LOG(LogHAL, Fatal, TEXT("BinnedFreeToOS(): info mismatch: descriptor original size %llu, our size is %llu for pointer %p"), AllocDescriptor->OriginalSizeAsPassed, Size, Ptr);
				// unreachable
				return;
			}
		}

		AllocDescriptor = nullptr;	// just so no one touches it

		if (UNLIKELY(munmap(PointerToUnmap, SizeToUnmap) != 0))
		{
			const int ErrNo = errno;
			UE_LOG(LogHAL, Fatal, TEXT("munmap(addr=%p, len=%llu, size as passed %llu) failed with errno = %d (%s)"), PointerToUnmap, (uint64)SizeToUnmap, (uint64)Size,
				ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
		}
	}
	else
	{
		if (UNLIKELY(munmap(Ptr, SizeInWholePages) != 0))
		{
			const int ErrNo = errno;
			UE_LOG(LogHAL, Fatal, TEXT("munmap(addr=%p, len=%llu, size as passed %llu) failed with errno = %d (%s)"), Ptr, (uint64)SizeInWholePages, (uint64)Size,
				ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
		}
	}
#endif // UE4_PLATFORM_USES_MMAP_FOR_BINNED_OS_ALLOCS
}

void FGenericPlatformMemory::DumpStats( class FOutputDevice& Ar )
{
	const float InvMB = 1.0f / 1024.0f / 1024.0f;
	FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
#if !NO_LOGGING
	const FName CategoryName(LogMemory.GetCategoryName());
#else
	const FName CategoryName(TEXT("LogMemory"));
#endif
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Platform Memory Stats for %s"), ANSI_TO_TCHAR(FPlatformProperties::PlatformName()));
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Process Physical Memory: %.2f MB used, %.2f MB peak"), MemoryStats.UsedPhysical*InvMB, MemoryStats.PeakUsedPhysical*InvMB);
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Process Virtual Memory: %.2f MB used, %.2f MB peak"), MemoryStats.UsedVirtual*InvMB, MemoryStats.PeakUsedVirtual*InvMB);

	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Physical Memory: %.2f MB used,  %.2f MB free, %.2f MB total"), 
		(MemoryStats.TotalPhysical - MemoryStats.AvailablePhysical)*InvMB, MemoryStats.AvailablePhysical*InvMB, MemoryStats.TotalPhysical*InvMB);
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Virtual Memory: %.2f MB used,  %.2f MB free, %.2f MB total"), 
		(MemoryStats.TotalVirtual - MemoryStats.AvailableVirtual)*InvMB, MemoryStats.AvailablePhysical*InvMB, MemoryStats.TotalVirtual*InvMB);

}

void FGenericPlatformMemory::DumpPlatformAndAllocatorStats( class FOutputDevice& Ar )
{
	FPlatformMemory::DumpStats( Ar );
	GMalloc->DumpAllocatorStats( Ar );
}

void FGenericPlatformMemory::MemswapGreaterThan8( void* RESTRICT Ptr1, void* RESTRICT Ptr2, SIZE_T Size )
{
	union PtrUnion
	{
		void*   PtrVoid;
		uint8*  Ptr8;
		uint16* Ptr16;
		uint32* Ptr32;
		uint64* Ptr64;
		UPTRINT PtrUint;
	};

	PtrUnion Union1 = { Ptr1 };
	PtrUnion Union2 = { Ptr2 };

	checkf(Union1.PtrVoid && Union2.PtrVoid, TEXT("Pointers must be non-null: %p, %p"), Union1.PtrVoid, Union2.PtrVoid);

	// We may skip up to 7 bytes below, so better make sure that we're swapping more than that
	// (8 is a common case that we also want to inline before we this call, so skip that too)
	check(Size > 8);

	if (Union1.PtrUint & 1)
	{
		Valswap(*Union1.Ptr8++, *Union2.Ptr8++);
		Size -= 1;
	}
	if (Union1.PtrUint & 2)
	{
		Valswap(*Union1.Ptr16++, *Union2.Ptr16++);
		Size -= 2;
	}
	if (Union1.PtrUint & 4)
	{
		Valswap(*Union1.Ptr32++, *Union2.Ptr32++);
		Size -= 4;
	}

	uint32 CommonAlignment = FMath::Min(FMath::CountTrailingZeros(Union1.PtrUint - Union2.PtrUint), 3u);
	switch (CommonAlignment)
	{
		default:
			for (; Size >= 8; Size -= 8)
			{
				Valswap(*Union1.Ptr64++, *Union2.Ptr64++);
			}

		case 2:
			for (; Size >= 4; Size -= 4)
			{
				Valswap(*Union1.Ptr32++, *Union2.Ptr32++);
			}

		case 1:
			for (; Size >= 2; Size -= 2)
			{
				Valswap(*Union1.Ptr16++, *Union2.Ptr16++);
			}

		case 0:
			for (; Size >= 1; Size -= 1)
			{
				Valswap(*Union1.Ptr8++, *Union2.Ptr8++);
			}
	}
}

FGenericPlatformMemory::FSharedMemoryRegion::FSharedMemoryRegion(const FString& InName, uint32 InAccessMode, void* InAddress, SIZE_T InSize)
	:	AccessMode(InAccessMode)
	,	Address(InAddress)
	,	Size(InSize)
{
	FCString::Strcpy(Name, sizeof(Name) - 1, *InName);
}

FGenericPlatformMemory::FSharedMemoryRegion * FGenericPlatformMemory::MapNamedSharedMemoryRegion(const FString& Name, bool bCreate, uint32 AccessMode, SIZE_T Size)
{
	UE_LOG(LogHAL, Error, TEXT("FGenericPlatformMemory::MapNamedSharedMemoryRegion not implemented on this platform"));
	return NULL;
}

bool FGenericPlatformMemory::UnmapNamedSharedMemoryRegion(FSharedMemoryRegion * MemoryRegion)
{
	UE_LOG(LogHAL, Error, TEXT("FGenericPlatformMemory::UnmapNamedSharedMemoryRegion not implemented on this platform"));
	return false;
}


void FGenericPlatformMemory::InternalUpdateStats( const FPlatformMemoryStats& MemoryStats )
{
	// Generic method is empty. Implement at platform level.
}

bool FGenericPlatformMemory::IsDebugMemoryEnabled()
{
	return false;
}

bool FGenericPlatformMemory::GetLLMAllocFunctions(void*(*&OutAllocFunction)(size_t), void(*&OutFreeFunction)(void*, size_t), int32& OutAlignment)
{
	return false;
}

