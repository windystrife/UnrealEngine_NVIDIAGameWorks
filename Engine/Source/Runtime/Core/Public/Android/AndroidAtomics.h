// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	AndroidPlatformAtomics.h: Android platform Atomics functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformAtomics.h"
#include "Android/AndroidSystemIncludes.h"

/**
 * Android implementation of the Atomics OS functions.
 */
struct CORE_API FAndroidPlatformAtomics
	: public FGenericPlatformAtomics
{
	static FORCEINLINE int32 InterlockedIncrement( volatile int32* Value )
	{
		return __sync_fetch_and_add(Value, 1) + 1;
	}

	static FORCEINLINE int64 InterlockedIncrement( volatile int64* Value )
	{
		return __sync_fetch_and_add(Value, 1) + 1;
	}

	static FORCEINLINE int32 InterlockedDecrement( volatile int32* Value )
	{
		return __sync_fetch_and_sub(Value, 1) - 1;
	}

	static FORCEINLINE int64 InterlockedDecrement( volatile int64* Value )
	{
		return __sync_fetch_and_sub(Value, 1) - 1;
	}

	static FORCEINLINE int32 InterlockedAdd( volatile int32* Value, int32 Amount )
	{
		return __sync_fetch_and_add(Value, Amount);
	}

	static FORCEINLINE int64 InterlockedAdd( volatile int64* Value, int64 Amount )
	{
		return __sync_fetch_and_add(Value, Amount);
	}

	static FORCEINLINE int32 InterlockedExchange( volatile int32* Value, int32 Exchange )
	{
		return __sync_lock_test_and_set(Value,Exchange);	
	}

	static FORCEINLINE int64 InterlockedExchange( volatile int64* Value, int64 Exchange )
	{
		return __sync_lock_test_and_set(Value, Exchange);	
	}

	static FORCEINLINE void* InterlockedExchangePtr( void** Dest, void* Exchange )
	{
		return __sync_lock_test_and_set(Dest, Exchange);
	}

	static FORCEINLINE int32 InterlockedCompareExchange( volatile int32* Dest, int32 Exchange, int32 Comperand )
	{
		return __sync_val_compare_and_swap(Dest, Comperand, Exchange);
	}

	static FORCEINLINE int64 InterlockedCompareExchange( volatile int64* Dest, int64 Exchange, int64 Comperand )
	{
		return __sync_val_compare_and_swap(Dest, Comperand, Exchange);
	}

	static FORCEINLINE int64 AtomicRead64(volatile const int64* Src)
	{
		return InterlockedCompareExchange((volatile int64*)Src, 0, 0);
	}

	static FORCEINLINE void* InterlockedCompareExchangePointer( void** Dest, void* Exchange, void* Comperand )
	{
		return __sync_val_compare_and_swap(Dest, Comperand, Exchange);
	}
};


typedef FAndroidPlatformAtomics FPlatformAtomics;