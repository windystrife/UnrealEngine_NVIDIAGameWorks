// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformCriticalSection.h"

/**
 * @todo html5 threads: Dummy critical section
 */
class FHTML5CriticalSection
{
public:

	FHTML5CriticalSection() {}

	/**
	 * Locks the critical section
	 */
	FORCEINLINE void Lock(void)
	{
	}
	
	/**
	 * Attempt to take a lock and returns whether or not a lock was taken.
	 *
	 * @return true if a lock was taken, false otherwise.
	 */
	FORCEINLINE bool TryLock()
	{
		return false;
	}

	/**
	 * Releases the lock on the critical seciton
	 */
	FORCEINLINE void Unlock(void)
	{
	}

private:
	FHTML5CriticalSection(const FHTML5CriticalSection&);
	FHTML5CriticalSection& operator=(const FHTML5CriticalSection&);
};

typedef FHTML5CriticalSection FCriticalSection;
typedef FSystemWideCriticalSectionNotImplemented FSystemWideCriticalSection;
typedef TGenericPlatformRWLock<FHTML5CriticalSection> FRWLock;
