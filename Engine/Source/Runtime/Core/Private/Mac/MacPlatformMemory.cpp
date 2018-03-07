// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MacPlatformMemory.cpp: Mac platform memory functions
=============================================================================*/

#include "MacPlatformMemory.h"
#include "HAL/PlatformMemory.h"
#include "MallocTBB.h"
#include "MallocAnsi.h"
#include "MallocBinned.h"
#include "MallocBinned2.h"
#include "MallocStomp.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CoreStats.h"
#include "CoreGlobals.h"

#include <sys/param.h>
#include <sys/mount.h>

// Set rather to use BinnedMalloc2 for binned malloc, can be overridden below
#define USE_MALLOC_BINNED2 (1)

FMalloc* FMacPlatformMemory::BaseAllocator()
{
	bool bIsMavericks = false;

	char OSRelease[PATH_MAX] = {};
	size_t OSReleaseBufferSize = PATH_MAX;
	if (sysctlbyname("kern.osrelease", OSRelease, &OSReleaseBufferSize, NULL, 0) == 0)
	{
		int32 OSVersionMajor = 0;
		if (sscanf(OSRelease, "%d", &OSVersionMajor) == 1)
		{
			bIsMavericks = OSVersionMajor <= 13;
		}
	}

	if (FORCE_ANSI_ALLOCATOR || IS_PROGRAM)
	{
		AllocatorToUse = EMemoryAllocatorToUse::Ansi;
	}
	else if (USE_MALLOC_STOMP)
	{
		AllocatorToUse = EMemoryAllocatorToUse::Stomp;
	}
	else if ((WITH_EDITORONLY_DATA || IS_PROGRAM) && TBB_ALLOCATOR_ALLOWED)
	{
		AllocatorToUse = EMemoryAllocatorToUse::TBB;
	}
	else if (USE_MALLOC_BINNED2)
	{
		AllocatorToUse = EMemoryAllocatorToUse::Binned2;
	}
	else
	{
		AllocatorToUse = EMemoryAllocatorToUse::Binned;
	}

	// Force ansi malloc in some cases
	if(getenv("UE4_FORCE_MALLOC_ANSI") != nullptr || bIsMavericks)
	{
		AllocatorToUse = EMemoryAllocatorToUse::Ansi;
	}

	switch (AllocatorToUse)
	{
	case EMemoryAllocatorToUse::Ansi:
		return new FMallocAnsi();
#if USE_MALLOC_STOMP
	case EMemoryAllocatorToUse::Stomp:
		return new FMallocStomp();
#endif
	case EMemoryAllocatorToUse::TBB:
		return new FMallocTBB();
	case EMemoryAllocatorToUse::Binned2:
		return new FMallocBinned2();

	default:	// intentional fall-through
	case EMemoryAllocatorToUse::Binned:
		// [RCL] 2017-03-06 FIXME: perhaps BinnedPageSize should be used here, but leaving this change to the Mac platform owner.
		return new FMallocBinned((uint32)(GetConstants().PageSize&MAX_uint32), 0x100000000);
	}

}

FPlatformMemoryStats FMacPlatformMemory::GetStats()
{
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();

	FPlatformMemoryStats MemoryStats;

	// Gather platform memory stats.
	vm_statistics Stats;
	mach_msg_type_number_t StatsSize = sizeof(Stats);
	host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&Stats, &StatsSize);
	uint64_t FreeMem = Stats.free_count * MemoryConstants.PageSize;
	MemoryStats.AvailablePhysical = FreeMem;
	
	// Get swap file info
	xsw_usage SwapUsage;
	SIZE_T Size = sizeof(SwapUsage);
	sysctlbyname("vm.swapusage", &SwapUsage, &Size, NULL, 0);
	MemoryStats.AvailableVirtual = FreeMem + SwapUsage.xsu_avail;

	// Just get memory information for the process and report the working set instead
	mach_task_basic_info_data_t TaskInfo;
	mach_msg_type_number_t TaskInfoCount = MACH_TASK_BASIC_INFO_COUNT;
	task_info( mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&TaskInfo, &TaskInfoCount );
	MemoryStats.UsedPhysical = TaskInfo.resident_size;
	if(MemoryStats.UsedPhysical > MemoryStats.PeakUsedPhysical)
	{
		MemoryStats.PeakUsedPhysical = MemoryStats.UsedPhysical;
	}
	MemoryStats.UsedVirtual = TaskInfo.virtual_size;
	if(MemoryStats.UsedVirtual > MemoryStats.PeakUsedVirtual)
	{
		MemoryStats.PeakUsedVirtual = MemoryStats.UsedVirtual;
	}
	

	return MemoryStats;
}

const FPlatformMemoryConstants& FMacPlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if( MemoryConstants.TotalPhysical == 0 )
	{
		// Gather platform memory constants.

		// Get page size.
		vm_size_t PageSize;
		host_page_size(mach_host_self(), &PageSize);

		// Get swap file info
		xsw_usage SwapUsage;
		SIZE_T Size = sizeof(SwapUsage);
		sysctlbyname("vm.swapusage", &SwapUsage, &Size, NULL, 0);

		// Get memory.
		int64 AvailablePhysical = 0;
		int Mib[] = {CTL_HW, HW_MEMSIZE};
		size_t Length = sizeof(int64);
		sysctl(Mib, 2, &AvailablePhysical, &Length, NULL, 0);
		
		MemoryConstants.TotalPhysical = AvailablePhysical;
		MemoryConstants.TotalVirtual = AvailablePhysical + SwapUsage.xsu_total;
		MemoryConstants.PageSize = (uint32)PageSize;
		MemoryConstants.OsAllocationGranularity = (uint32)PageSize;
		MemoryConstants.BinnedPageSize = FMath::Max((SIZE_T)65536, (SIZE_T)PageSize);

		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
	}

	return MemoryConstants;	
}
