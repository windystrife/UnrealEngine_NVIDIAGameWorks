// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/CriticalSection.h"

/**
 * Implements a scope lock using TryLock.
 *
 * This is a utility class that handles scope level locking using TryLock.
 * Scope locking helps to avoid programming errors by which a lock is acquired
 * and never released.
 *
 * <code>
 *	{
 *		// Try to acquire a lock on CriticalSection for the current scope.
 *		FScopeTryLock ScopeTryLock(CriticalSection);
 *		// Check that the lock was acquired.
 *		if (ScopeTryLock.IsLocked())
 *		{
 *			// If the lock was acquired, we can safely access resources protected
 *			// by the lock.
 *		}
 *		// When ScopeTryLock goes out of scope, the critical section will be
 *		// released if it was ever acquired.
 *	}
 * </code>
 */
class FScopeTryLock
{
public:
	// Constructor that tries to lock the critical section. Note that you must
	// call IsLocked to test that the lock was acquired.
	FScopeTryLock(FCriticalSection *InCriticalSection)
		: CriticalSection(InCriticalSection)
	{
		check(CriticalSection);
		if (!CriticalSection->TryLock())
		{
			CriticalSection = nullptr;
		}
	}

	// Destructor that will release the lock if it was ever acquired.
	~FScopeTryLock()
	{
		if (CriticalSection)
		{
			CriticalSection->Unlock();
		}
	}

	FORCEINLINE bool IsLocked() const
	{
		return nullptr != CriticalSection;
	}

private:
	FScopeTryLock(FScopeTryLock const &InScopeLock) = delete;
	FScopeTryLock &operator=(FScopeTryLock &Rhs) = delete;

	FCriticalSection *CriticalSection;
};
