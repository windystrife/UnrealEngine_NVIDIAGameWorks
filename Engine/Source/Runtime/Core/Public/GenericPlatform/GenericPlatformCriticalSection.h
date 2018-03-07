// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Timespan.h"

/** Platforms that don't need a working FSystemWideCriticalSection can just typedef this one */
class FSystemWideCriticalSectionNotImplemented
{
public:
	/** Construct a named, system-wide critical section and attempt to get access/ownership of it */
	explicit FSystemWideCriticalSectionNotImplemented(const FString& Name, FTimespan Timeout = FTimespan::Zero());

	/** Destructor releases system-wide critical section if it is currently owned */
	~FSystemWideCriticalSectionNotImplemented() {}

	/**
	 * Does the calling thread have ownership of the system-wide critical section?
	 *
	 * @return True if the system-wide lock is obtained.
	 */
	bool IsValid() const { return false; }

	/** Releases system-wide critical section if it is currently owned */
	void Release() {}

private:
	FSystemWideCriticalSectionNotImplemented(const FSystemWideCriticalSectionNotImplemented&);
	FSystemWideCriticalSectionNotImplemented& operator=(const FSystemWideCriticalSectionNotImplemented&);
};

/**
 * TGenericPlatformRWLock - Read/Write Mutex
 *	- Provides non-recursive Read/Write (or shared-exclusive) access.
 *	- As a fallback default for non implemented platforms, using a single FCriticalSection to provide complete single mutual exclusion - no seperate Read/Write access.
 */
template<class CriticalSection>
class TGenericPlatformRWLock
{
public:
	FORCEINLINE TGenericPlatformRWLock()
	{
	}
	
	FORCEINLINE ~TGenericPlatformRWLock()
	{
	}
	
	FORCEINLINE void ReadLock()
	{
		Mutex.Lock();
	}
	
	FORCEINLINE void WriteLock()
	{
		Mutex.Lock();
	}
	
	FORCEINLINE void ReadUnlock()
	{
		Mutex.Unlock();
	}
	
	FORCEINLINE void WriteUnlock()
	{
		Mutex.Unlock();
	}
	
private:
	CriticalSection Mutex;
};
