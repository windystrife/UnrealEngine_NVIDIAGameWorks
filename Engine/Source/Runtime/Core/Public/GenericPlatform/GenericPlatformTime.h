// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"

#if PLATFORM_HAS_BSD_TIME 
	#include <sys/time.h>
#endif


/** Contains CPU utilization data. */
struct FCPUTime
{
	/** Initialization constructor. */
	FCPUTime( float InCPUTimePct, float InCPUTimePctRelative ) 
		: CPUTimePct( InCPUTimePct )
		, CPUTimePctRelative( InCPUTimePctRelative )
	{}

	FCPUTime& operator+=( const FCPUTime& Other )
	{
		CPUTimePct += Other.CPUTimePct;
		CPUTimePctRelative += Other.CPUTimePctRelative;
		return *this;
	}

	/** Percentage CPU utilization for the last interval. */
	float CPUTimePct;

	/** Percentage CPU utilization for the last interval relative to one core, 
	 ** so if CPUTimePct is 8.0% and you have 6 core this value will be 48.0%. */
	float CPUTimePctRelative;
};


/**
* Generic implementation for most platforms
**/
struct CORE_API FGenericPlatformTime
{
#if PLATFORM_HAS_BSD_TIME 
	/**
	 * Does per platform initialization of timing information and returns the current time. This function is
	 * called before the execution of main as GStartTime is statically initialized by it. The function also
	 * internally sets SecondsPerCycle, which is safe to do as static initialization order enforces complex
	 * initialization after the initial 0 initialization of the value.
	 *
	 * @return	current time
	 */
	static double InitTiming();

	static FORCEINLINE double Seconds()
	{
		struct timeval tv;
		gettimeofday( &tv, NULL );
		return ((double) tv.tv_sec) + (((double) tv.tv_usec) / 1000000.0);
	}

	static FORCEINLINE uint32 Cycles()
	{
		struct timeval tv;
		gettimeofday( &tv, NULL );
		return (uint32) ((((uint64)tv.tv_sec) * 1000000ULL) + (((uint64)tv.tv_usec)));
	}
	static FORCEINLINE uint64 Cycles64()
	{
		struct timeval tv;
		gettimeofday( &tv, NULL );
		return ((((uint64)tv.tv_sec) * 1000000ULL) + (((uint64)tv.tv_usec)));
	}

	/** Returns the system time. */
	static void SystemTime( int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec );

	/** Returns the UTC time. */
	static void UtcTime( int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec );
#endif

	/**
	 * Get the system date
	 * 
	 * @param Dest Destination buffer to copy to
	 * @param DestSize Size of destination buffer in characters
	 * @return Date string
	 */
	static TCHAR* StrDate( TCHAR* Dest, SIZE_T DestSize );
	/**
	 * Get the system time
	 * 
	 * @param Dest Destination buffer to copy to
	 * @param DestSize Size of destination buffer in characters
	 * @return Time string
	 */
	static TCHAR* StrTime( TCHAR* Dest, SIZE_T DestSize );

	/**
	 * Returns a timestamp string built from the current date and time.
	 * NOTE: Only one return value is valid at a time!
	 *
	 * @return timestamp string
	 */
	static const TCHAR* StrTimestamp();

	/**
	 * Returns a pretty-string for a time given in seconds. (I.e. "4:31 min", "2:16:30 hours", etc)
	 *
	 * @param Seconds Time in seconds
	 * @return Time in a pretty formatted string
	 */
	static FString PrettyTime( double Seconds );

	/** Updates CPU utilization, called through a delegate from the Core ticker. */
	static bool UpdateCPUTime( float DeltaTime )
	{
		return false;
	}

	/**
	 * @return structure that contains CPU utilization data.
	 */
	static FCPUTime GetCPUTime()
	{
		return FCPUTime( 0.0f, 0.0f );
	}

	/**
	* @return seconds per cycle.
	*/
	static double GetSecondsPerCycle()
	{
		return SecondsPerCycle;
	}
	/** Converts cycles to milliseconds. */
	static float ToMilliseconds( const uint32 Cycles )
	{
		return (float)double( SecondsPerCycle * 1000.0 * Cycles );
	}

	/** Converts cycles to seconds. */
	static float ToSeconds( const uint32 Cycles )
	{
		return (float)double( SecondsPerCycle * Cycles );
	}
	/**
	 * @return seconds per cycle.
	 */
	static double GetSecondsPerCycle64();
	/** Converts cycles to milliseconds. */
	static double ToMilliseconds64(const uint64 Cycles)
	{
		return ToSeconds64(Cycles) * 1000.0;
	}

	/** Converts cycles to seconds. */
	static double ToSeconds64(const uint64 Cycles)
	{
		return GetSecondsPerCycle64() * double(Cycles);
	}

protected:

	static double SecondsPerCycle;
	static double SecondsPerCycle64;
};
