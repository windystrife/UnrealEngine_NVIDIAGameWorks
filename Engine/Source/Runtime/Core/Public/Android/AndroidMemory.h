// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	AndroidPlatformMemory.h: Android platform memory functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformMemory.h"

/**
 *	Android implementation of the FGenericPlatformMemoryStats.
 */
struct FPlatformMemoryStats : public FGenericPlatformMemoryStats
{};

/**
* Android implementation of the memory OS functions
**/
struct CORE_API FAndroidPlatformMemory : public FGenericPlatformMemory
{
	//~ Begin FGenericPlatformMemory Interface
	static void Init();
	static FPlatformMemoryStats GetStats();
	static const FPlatformMemoryConstants& GetConstants();
	static FMalloc* BaseAllocator();
	static void* BinnedAllocFromOS( SIZE_T Size );
	static void BinnedFreeToOS( void* Ptr, SIZE_T Size );
	//~ End FGenericPlatformMemory Interface
};

typedef FAndroidPlatformMemory FPlatformMemory;



