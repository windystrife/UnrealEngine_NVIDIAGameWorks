// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformTime.h"
#include "WindowsSystemIncludes.h"


/**
 * Windows implementation of the Time OS functions.
 *
 * Please see following UDN post about using rdtsc on processors that support
 * result being invariant across cores.
 *
 * https://udn.epicgames.com/lists/showpost.php?id=46794&list=unprog3
 */
struct CORE_API FWindowsPlatformTime
	: public FGenericPlatformTime
{
	static double InitTiming();

	static FORCEINLINE double Seconds()
	{
		Windows::LARGE_INTEGER Cycles;
		Windows::QueryPerformanceCounter(&Cycles);

		// add big number to make bugs apparent where return value is being passed to float
		return Cycles.QuadPart * GetSecondsPerCycle() + 16777216.0;
	}

	static FORCEINLINE uint32 Cycles()
	{
		Windows::LARGE_INTEGER Cycles;
		Windows::QueryPerformanceCounter(&Cycles);
		return Cycles.QuadPart;
	}

	static FORCEINLINE uint64 Cycles64()
	{
		Windows::LARGE_INTEGER Cycles;
		QueryPerformanceCounter(&Cycles);
		return Cycles.QuadPart;
	}


	static void SystemTime( int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec );
	static void UtcTime( int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec );

	static bool UpdateCPUTime( float DeltaTime );
	static FCPUTime GetCPUTime();	

protected:

	/** Percentage CPU utilization for the last interval relative to one core. */
	static float CPUTimePctRelative;
};


typedef FWindowsPlatformTime FPlatformTime;
