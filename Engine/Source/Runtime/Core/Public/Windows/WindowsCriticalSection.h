// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Timespan.h"
#include "HAL/PlatformMemory.h"

class FString;

/**
 * This is the Windows version of a critical section. It uses an aggregate
 * CRITICAL_SECTION to implement its locking.
 */
class FWindowsCriticalSection
{
	/**
	 * The windows specific critical section
	 */
	Windows::CRITICAL_SECTION CriticalSection;

public:

	/**
	 * Constructor that initializes the aggregated critical section
	 */
	FORCEINLINE FWindowsCriticalSection()
	{
		CA_SUPPRESS(28125);
		Windows::InitializeCriticalSection(&CriticalSection);
		Windows::SetCriticalSectionSpinCount(&CriticalSection,4000);
	}

	/**
	 * Destructor cleaning up the critical section
	 */
	FORCEINLINE ~FWindowsCriticalSection()
	{
		Windows::DeleteCriticalSection(&CriticalSection);
	}

	/**
	 * Locks the critical section
	 */
	FORCEINLINE void Lock()
	{
		// Spin first before entering critical section, causing ring-0 transition and context switch.
		if(Windows::TryEnterCriticalSection(&CriticalSection) == 0 )
		{
			Windows::EnterCriticalSection(&CriticalSection);
		}
	}

	/**
	 * Attempt to take a lock and returns whether or not a lock was taken.
	 *
	 * @return true if a lock was taken, false otherwise.
	 */
	FORCEINLINE bool TryLock()
	{
		if (Windows::TryEnterCriticalSection(&CriticalSection))
		{
			return true;
		}
		return false;
	}

	/**
	 * Releases the lock on the critical section
	 */
	FORCEINLINE void Unlock()
	{
		Windows::LeaveCriticalSection(&CriticalSection);
	}

private:
	FWindowsCriticalSection(const FWindowsCriticalSection&);
	FWindowsCriticalSection& operator=(const FWindowsCriticalSection&);
};

/** System-Wide Critical Section for windows using mutex */
class CORE_API FWindowsSystemWideCriticalSection
{
public:
	/** Construct a named, system-wide critical section and attempt to get access/ownership of it */
	explicit FWindowsSystemWideCriticalSection(const class FString& InName, FTimespan InTimeout = FTimespan::Zero());

	/** Destructor releases system-wide critical section if it is currently owned */
	~FWindowsSystemWideCriticalSection();

	/**
	 * Does the calling thread have ownership of the system-wide critical section?
	 *
	 * @return True if obtained. WARNING: Returns true for an owned but previously abandoned locks so shared resources can be in undetermined states. You must handle shared data robustly.
	 */
	bool IsValid() const;

	/** Releases system-wide critical section if it is currently owned */
	void Release();

private:
	FWindowsSystemWideCriticalSection(const FWindowsSystemWideCriticalSection&);
	FWindowsSystemWideCriticalSection& operator=(const FWindowsSystemWideCriticalSection&);

private:
	Windows::HANDLE Mutex;
};

/**
 * FWindowsRWLock - Read/Write Mutex
 *	- Provides non-recursive Read/Write (or shared-exclusive) access.
 *	- Windows specific lock structures/calls Ref: https://msdn.microsoft.com/en-us/library/windows/desktop/aa904937(v=vs.85).aspx
 */
class FWindowsRWLock
{
public:
	FORCEINLINE FWindowsRWLock(uint32 Level = 0)
	{
		Windows::InitializeSRWLock(&Mutex);
	}
	
	FORCEINLINE ~FWindowsRWLock()
	{
	}
	
	FORCEINLINE void ReadLock()
	{
		Windows::AcquireSRWLockShared(&Mutex);
	}
	
	FORCEINLINE void WriteLock()
	{
		Windows::AcquireSRWLockExclusive(&Mutex);
	}
	
	FORCEINLINE void ReadUnlock()
	{
		Windows::ReleaseSRWLockShared(&Mutex);
	}
	
	FORCEINLINE void WriteUnlock()
	{
		Windows::ReleaseSRWLockExclusive(&Mutex);
	}
	
private:
	Windows::SRWLOCK Mutex;
};

typedef FWindowsCriticalSection FCriticalSection;
typedef FWindowsSystemWideCriticalSection FSystemWideCriticalSection;
typedef FWindowsRWLock FRWLock;
