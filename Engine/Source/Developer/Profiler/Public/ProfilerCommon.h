// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Stats/StatsMisc.h"

DECLARE_LOG_CATEGORY_EXTERN(Profiler, Log, All);


#define DEBUG_PROFILER_PERFORMANCE 0

#if DEBUG_PROFILER_PERFORMANCE==1
	#define PROFILER_SCOPE_LOG_TIME(arg0,arg1) SCOPE_LOG_TIME(arg0,arg1)
#else
	#define PROFILER_SCOPE_LOG_TIME(arg0,arg1)
#endif


/** Time spent on graph drawing. */
DECLARE_CYCLE_STAT_EXTERN( TEXT("DataGraphOnPaint"),	STAT_DG_OnPaint,			STATGROUP_Profiler, );

/** Time spent on handling profiler data. */
DECLARE_CYCLE_STAT_EXTERN( TEXT("ProfilerHandleData"),	STAT_PM_HandleProfilerData,	STATGROUP_Profiler, );

/** Time spent on ticking profiler manager. */
DECLARE_CYCLE_STAT_EXTERN( TEXT("ProfilerTick"),		STAT_PM_Tick,				STATGROUP_Profiler, );

/** Number of bytes allocated by all profiler sessions. */
DECLARE_MEMORY_STAT_EXTERN( TEXT("ProfilerMemoryUsage"),STAT_PM_MemoryUsage,		STATGROUP_Profiler, );


/**
 * Enumerates graph styles.
 */
enum class EGraphStyles
{
	/** Line graph. */
	Line,

	/** Combined graph. */
	Combined,

	/** More to come in future. */
	/** Invalid enum type, may be used as a number of enumerations. */
	InvalidOrMax,
};


class FProfilerHelper
{
public:
	/** Shorten a name for stats display. */
	static FString ShortenName( const FString& NameToShorten, const int32 Limit = 16 )
	{
		FString Result(NameToShorten);
		if (Result.Len() > Limit)
		{
			Result = FString(TEXT("...")) + Result.Right(Limit);
		}
		return Result;
	}
};


/**
 * Implements binary search for the various cases.
 */
class FBinaryFindIndex
{
public:
	/**
	 * Executes a binary search for element Item in array Array using the <= operator
	 * (i.e. uses the comparison Array[i] <= Item). Assumes that Array is pre-sorted.
	 * 
	 * @param Array The Array to search
	 * @param Item The item to search for
	 * @param FirstIndex Optional. Function will not search before this index. Default is 0
	 * @param LastIndex Optional. Function will not search beyond this index. Default is INDEX_NONE
	 * @return Returns the last index of the element that is smaller than or equal to Item, or, if Item is not found, returns 0.
	 */
	template<class T>
	static int32 LessEqual( const TArray<T>& Array, const T& Item, const int32 FirstIndex = 0, const int32 LastIndex = INDEX_NONE )
	{
		const int32 LocalLastIndex = LastIndex == INDEX_NONE ? Array.Num() : LastIndex;
		int32 Length = LocalLastIndex - FirstIndex;
		int32 Middle = Length;
		int32 Offset = FirstIndex;

		while( Middle > 0 )
		{
			Middle = Length / 2;
			if( Array[Offset + Middle] <= Item )
			{
				Offset += Middle;
			}
			Length -= Middle;
		}
		return Offset;
	}

	/**
	 * Executes a binary search for element Item in array Array using the >= operator
	 * (i.e. uses the comparison Array[i] >= Item). Assumes that Array is pre-sorted.
	 * 
	 * @param Array The Array to search
	 * @param Item The item to search for
	 * @param FirstIndex Optional. Function will not search before this index. Default is 0
	 * @param LastIndex Optional. Function will not search beyond this index. Default is INDEX_NONE
	 * @return Returns the first index of the element that is greater than or equal to Item, or, if Item is not found, returns the last index + 1.
	 */
	template<class T>
	static int32 GreaterEqual( const TArray<T>& Array, const T& Item, const int32 FirstIndex = 0, const int32 LastIndex = INDEX_NONE )
	{
		const int32 LocalLastIndex = LastIndex == INDEX_NONE ? Array.Num() : LastIndex;
		int32 Length = LocalLastIndex - FirstIndex;
		int32 Middle = Length;
		int32 Offset = FirstIndex;
		int32 Edge = 0;

		while( Middle > 0 )
		{
			Middle = Length / 2;
			if( Array[Offset + Middle] >= Item )
			{
				Edge = 0;
			}
			else
			{
				Edge = 1;
				Offset += Middle;		
			}

			Length -= Middle;
		}
		return Offset+Edge;
	}
};
