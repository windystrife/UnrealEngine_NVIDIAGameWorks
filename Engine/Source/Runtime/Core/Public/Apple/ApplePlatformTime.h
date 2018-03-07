// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	ApplePlatformTime.h: Apple platform Time functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformTime.h"
#include "CoreTypes.h"
#if PLATFORM_MAC
#include "Mac/MacSystemIncludes.h"
#elif PLATFORM_IOS
#include "IOS/IOSSystemIncludes.h"
#endif

/**
 * Please see following UDN post about using rdtsc on processors that support
 * result being invariant across cores.
 *
 * https://udn.epicgames.com/lists/showpost.php?id=46794&list=unprog3
 */



/**
* Apple platform implementation of the Time OS functions
**/
struct CORE_API FApplePlatformTime : public FGenericPlatformTime
{
	static double InitTiming();

	static FORCEINLINE double Seconds()
	{
		uint64 Cycles = mach_absolute_time();
		// Add big number to make bugs apparent where return value is being passed to float
		return Cycles * GetSecondsPerCycle() + 16777216.0;
	}

	static FORCEINLINE uint32 Cycles()
	{
		uint64 Cycles = mach_absolute_time();
		return Cycles;
	}

	static FORCEINLINE uint64 Cycles64()
	{
		uint64 Cycles = mach_absolute_time();
		return Cycles;
	}

	static FCPUTime GetCPUTime();
};

typedef FApplePlatformTime FPlatformTime;
