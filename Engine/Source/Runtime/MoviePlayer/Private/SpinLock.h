// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"

/**
 * Simple primitive for setting spin locks up.
 * Is lockable from any thread, and any thread can block on the lock.
 * However, the spin lock should never be destroyed while any thread is using it.
 *
 * Spin locks are useful when the amount of time spinning is expected to be minor,
 * so no time is wasted doing a context switch. An adaptive mutex would probably work
 * better though.
 */
class FSpinLock
{
public:
	FSpinLock(float InSpinTimeInSeconds = 0.1f)
		: LockValue(0)
		, SpinTimeInSeconds(InSpinTimeInSeconds)
	{
	}

	~FSpinLock()
	{
		// A spinlock should never be destroyed while others are blocked on it!
		Unlock();
	}

	/** Locks the primitive so no one can proceed past a block point */
	void Lock()
	{
		FPlatformAtomics::InterlockedExchange(&LockValue, 1);
	}
	
	/** Unlocks the primitive so other threads can proceed past a block point */
	void Unlock()
	{
		FPlatformAtomics::InterlockedExchange(&LockValue, 0);
	}

	/** Blocks this thread until the primitive is unlocked */
	void BlockUntilUnlocked() const
	{
		while (LockValue != 0)
		{
			FPlatformProcess::Sleep(SpinTimeInSeconds);
		}
	}

	/** Queries the state of the spinlock */
	bool IsLocked() const
	{
		return LockValue != 0;
	}
	
private:
	// Hidden on purpose as usage wouldn't be thread safe.
	void operator=(const FSpinLock& Other){}

private:
	/** Thread safe lock value */
	volatile int32 LockValue;

	/** The time spent sleeping between queries while blocked */
	float SpinTimeInSeconds;
};

