// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	LinuxPlatformMemory.h: Linux platform memory functions
==============================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformMemory.h"

/**
 *	Linux implementation of the FGenericPlatformMemoryStats.
 */
struct FPlatformMemoryStats : public FGenericPlatformMemoryStats
{};

/**
 *	Struct for more detailed stats that are slower to gather. Useful when using ForkAndWait().
 */
struct FExtendedPlatformMemoryStats
{
	/** */
	SIZE_T Shared_Clean;

	/** Shared memory used */
	SIZE_T Shared_Dirty;

	/** */
	SIZE_T Private_Clean;

	/** Private memory used */
	SIZE_T Private_Dirty;
};

/**
* Linux implementation of the memory OS functions
**/
struct CORE_API FLinuxPlatformMemory : public FGenericPlatformMemory
{
	/**
	 * Linux representation of a shared memory region
	 */
	struct FLinuxSharedMemoryRegion : public FSharedMemoryRegion
	{
		/** Returns file descriptor of a shared memory object */
		int GetFileDescriptor() const { return Fd; }

		/** Returns true if we need to unlink this region on destruction (no other process will be able to access it) */
		bool NeedsToUnlinkRegion() const { return bCreatedThisRegion; }

		FLinuxSharedMemoryRegion(const FString& InName, uint32 InAccessMode, void* InAddress, SIZE_T InSize, int InFd, bool bInCreatedThisRegion)
			:	FSharedMemoryRegion(InName, InAccessMode, InAddress, InSize)
			,	Fd(InFd)
			,	bCreatedThisRegion(bInCreatedThisRegion)
		{}

	protected:

		/** File descriptor of a shared region */
		int				Fd;

		/** Whether we created this region */
		bool			bCreatedThisRegion;
	};

	//~ Begin FGenericPlatformMemory Interface
	static void Init();
	static class FMalloc* BaseAllocator();
	static FPlatformMemoryStats GetStats();
	static FExtendedPlatformMemoryStats GetExtendedStats();
	static const FPlatformMemoryConstants& GetConstants();
	static bool PageProtect(void* const Ptr, const SIZE_T Size, const bool bCanRead, const bool bCanWrite);
	static void* BinnedAllocFromOS(SIZE_T Size);
	static void BinnedFreeToOS(void* Ptr, SIZE_T Size);
	static bool BinnedPlatformHasMemoryPoolForThisSize(SIZE_T Size);
	static FSharedMemoryRegion * MapNamedSharedMemoryRegion(const FString& InName, bool bCreate, uint32 AccessMode, SIZE_T Size);
	static bool UnmapNamedSharedMemoryRegion(FSharedMemoryRegion * MemoryRegion);
	//~ End FGenericPlatformMemory Interface
};

typedef FLinuxPlatformMemory FPlatformMemory;



