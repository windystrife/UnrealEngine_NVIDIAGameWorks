// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	LinuxPlatformTime.h: Linux platform Time functions
==============================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformTime.h"

/**
 * Linux implementation of the Time OS functions
 */
struct CORE_API FLinuxTime : public FGenericPlatformTime
{
	static FORCEINLINE double Seconds()
	{
		struct timespec ts;
		clock_gettime(ClockSource, &ts);
		return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
	}

	static FORCEINLINE uint32 Cycles()
	{
		struct timespec ts;
		clock_gettime(ClockSource, &ts);
		return static_cast<uint32>(static_cast<uint64>(ts.tv_sec) * 1000000ULL + static_cast<uint64>(ts.tv_nsec) / 1000ULL);
	}

	static FORCEINLINE uint64 Cycles64()
	{
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
		return static_cast<uint64>(static_cast<uint64>(ts.tv_sec) * 1000000ULL + static_cast<uint64>(ts.tv_nsec) / 1000ULL);
	}

	static FCPUTime GetCPUTime();	

	/**
	 * Calibration log to be printed at later time
	 */
	static void PrintCalibrationLog();

private:

	/** Clock source to use */
	static int ClockSource;

	/** Log information about calibrating the clock. */
	static char CalibrationLog[4096];

	/**
	 * Benchmarks clock_gettime(), possibly switches to something else is too slow.
	 * Linux-specific.
	 */
	static int CalibrateAndSelectClock();

	/**
	 * Returns number of time we can call the clock per second.
	 */
	static uint64 CallsPerSecondBenchmark(clockid_t BenchClockId, const char * BenchClockIdName);
};

typedef FLinuxTime FPlatformTime;
