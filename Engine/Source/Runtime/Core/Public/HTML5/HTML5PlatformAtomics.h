// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	HTML5PlatformAtomics.h: HTML5 platform Atomics functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformAtomics.h"

/**
 * HTML5 implementation of the Atomics OS functions (no actual atomics, as HTML5 has no threads)
 * @todo html5 threads: support this if we get threads
 */
struct CORE_API FHTML5PlatformAtomics	: public FGenericPlatformAtomics
{
	static FORCEINLINE int32 InterlockedIncrement( volatile int32* Value )
	{
		*Value += 1;
		return *Value;
	}

	static FORCEINLINE int64 InterlockedIncrement( volatile int64* Value )
	{
		*Value += 1;
		return *Value;
	}

	static FORCEINLINE int32 InterlockedDecrement( volatile int32* Value )
	{
       *Value -= 1;
		return *Value;
	}

	static FORCEINLINE int64 InterlockedDecrement( volatile int64* Value )
	{
		*Value -=1;
		return *Value;
	}

	static FORCEINLINE int32 InterlockedAdd( volatile int32* Value, int32 Amount )
	{
		int32 Result = *Value;
		*Value += Amount;
		return Result;
	}

	static FORCEINLINE int64 InterlockedAdd( volatile int64* Value, int64 Amount )
	{
		int64 Result = *Value;
		*Value += Amount;
		return Result;
	}

	static FORCEINLINE int32 InterlockedExchange( volatile int32* Value, int32 Exchange )
	{
		int32 Result = *Value;
		*Value = Exchange;
		return Result;
	}

	static FORCEINLINE int64 InterlockedExchange( volatile int64* Value, int64 Exchange )
	{
		int64 Result = *Value;
		*Value = Exchange;
		return Result;
	}

	static FORCEINLINE void* InterlockedExchangePtr( void** Dest, void* Exchange )
	{
		void* Result = *Dest;
		*Dest = Exchange;
		return Result;
	}

	static FORCEINLINE int32 InterlockedCompareExchange( volatile int32* Dest, int32 Exchange, int32 Comperand )
	{
		int32 Result = *Dest;
		if (Result == Comperand)
		{
			*Dest = Exchange;
		}
		return Result;
	}

	static FORCEINLINE int64 InterlockedCompareExchange( volatile int64* Dest, int64 Exchange, int64 Comperand )
	{
		int64 Result = *Dest;
		if (Result == Comperand)
		{
			*Dest = Exchange;
		}
		return Result;
	}

	static FORCEINLINE int64 AtomicRead64(volatile const int64* Src)
	{
		return *Src;
	}

	static FORCEINLINE void* InterlockedCompareExchangePointer( void** Dest, void* Exchange, void* Comperand )
	{
		void* Result = *Dest;
		if (Result == Comperand)
		{
			*Dest = Exchange;
		}
		return Result;
	}
};


typedef FHTML5PlatformAtomics FPlatformAtomics;