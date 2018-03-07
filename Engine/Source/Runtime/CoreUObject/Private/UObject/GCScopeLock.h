// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Garbage Collection scope lock. 
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeLock.h"
#include "HAL/ThreadSafeBool.h"

/** Locks all UObject hash tables when performing GC */
class FGCScopeLock
{
	/** Previous value of the GetGarbageCollectingFlag() */
	bool bPreviousGabageCollectingFlagValue;
public:

	static FThreadSafeBool& GetGarbageCollectingFlag();

	/** 
	 * We're storing the value of GetGarbageCollectingFlag in the constructor, it's safe as only 
	 * one thread is ever going to be setting it and calling this code - the game thread.
	 **/
	FORCEINLINE FGCScopeLock()
		: bPreviousGabageCollectingFlagValue(GetGarbageCollectingFlag())
	{		
		void LockUObjectHashTables();
		LockUObjectHashTables();		
		GetGarbageCollectingFlag() = true;
	}
	FORCEINLINE ~FGCScopeLock()
	{		
		GetGarbageCollectingFlag() = bPreviousGabageCollectingFlagValue;
		void UnlockUObjectHashTables();
		UnlockUObjectHashTables();		
	}
};


/**
* Garbage Collection synchronization objects
* Will not lock other threads if GC is not running.
* Has the ability to only lock for GC if no other locks are present.
*/
class FGCCSyncObject
{
	FThreadSafeCounter AsyncCounter;
	FThreadSafeCounter GCCounter;
	FCriticalSection Critical;
public:
	/** Lock on non-game thread. Will block if GC is running. */
	void LockAsync()
	{
		if (!IsInGameThread())
		{
			FScopeLock CriticalLock(&Critical);

			// Wait until GC is done if it's currently running
			FPlatformProcess::ConditionalSleep([&]()
			{
				return GCCounter.GetValue() == 0;
			});

			AsyncCounter.Increment();
		}
	}
	/** Release lock from non-game thread */
	void UnlockAsync()
	{
		if (!IsInGameThread())
		{
			AsyncCounter.Decrement();
		}
	}
	/** Lock for GC. Will block if any other thread has locked. */
	void GCLock()
	{
		FScopeLock CriticalLock(&Critical);

		// Wait until all other threads are done if they're currently holding the lock
		FPlatformProcess::ConditionalSleep([&]()
		{
			return AsyncCounter.GetValue() == 0;
		});

		GCCounter.Increment();
	}
	/** Checks if any async thread has a lock */
	bool IsAsyncLocked()
	{
		return AsyncCounter.GetValue() != 0;
	}
	/** Lock for GC. Will not block and return false if any other thread has already locked. */
	bool TryGCLock()
	{
		bool bSuccess = false;
		FScopeLock CriticalLock(&Critical);
		// If any other thread is currently locking we just exit
		if (AsyncCounter.GetValue() == 0)
		{
			GCCounter.Increment();
			bSuccess = true;
		}
		return bSuccess;
	}
	/** Unlock GC */
	void GCUnlock()
	{
		GCCounter.Decrement();
	}
};
