// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5PlatformMemory.cpp: HTML5 platform memory functions
=============================================================================*/

#include "HTML5PlatformMemory.h"
#include "MallocBinned.h"
#include "MallocAnsi.h"
#include "Misc/CoreStats.h"
#include "CoreGlobals.h"

void FHTML5PlatformMemory::Init()
{
	FGenericPlatformMemory::Init();

	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	UE_LOG(LogInit, Log, TEXT("Memory total: Physical=%.1fGB (%dGB approx) "),
		(double)MemoryConstants.TotalPhysical / 1024.0f / 1024.0f / 1024.0f,
		MemoryConstants.TotalPhysicalGB);
}

const FPlatformMemoryConstants& FHTML5PlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if( MemoryConstants.TotalPhysical == 0 )
	{
		// Gather platform memory stats.
		uint64 GTotalMemoryAvailable =  EM_ASM_INT_V({ return Module.TOTAL_MEMORY; });

		MemoryConstants.TotalPhysical = GTotalMemoryAvailable;
		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
	}

	return MemoryConstants;
}

FPlatformMemoryStats FHTML5PlatformMemory::GetStats()
{
	// @todo
	FPlatformMemoryStats MemoryStats;
	return MemoryStats;
}


FMalloc* FHTML5PlatformMemory::BaseAllocator()
{
	return new FMallocAnsi();
}

void* FHTML5PlatformMemory::BinnedAllocFromOS( SIZE_T Size )
{
	return FMemory::Malloc(Size, 16);
}

void FHTML5PlatformMemory::BinnedFreeToOS( void* Ptr, SIZE_T Size )
{
	FMemory::Free(Ptr);
}

