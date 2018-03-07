// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Timespan.h"
#include "HAL/PThreadCriticalSection.h"
#include "PThreadRWLock.h"

/**
  * Linux implementation of the FSystemWideCriticalSection. Uses exclusive file locking.
  **/
class FLinuxSystemWideCriticalSection
{
public:
	/** Construct a named, system-wide critical section and attempt to get access/ownership of it */
	explicit FLinuxSystemWideCriticalSection(const FString& InName, FTimespan InTimeout = FTimespan::Zero());

	/** Destructor releases system-wide critical section if it is currently owned */
	~FLinuxSystemWideCriticalSection();

	/**
	 * Does the calling thread have ownership of the system-wide critical section?
	 *
	 * @return True if the system-wide lock is obtained. WARNING: Returns true for abandoned locks so shared resources can be in undetermined states.
	 */
	bool IsValid() const;

	/** Releases system-wide critical section if it is currently owned */
	void Release();

private:
	FLinuxSystemWideCriticalSection(const FLinuxSystemWideCriticalSection&);
	FLinuxSystemWideCriticalSection& operator=(const FLinuxSystemWideCriticalSection&);

private:
	int32 FileHandle;
};

typedef FPThreadsCriticalSection FCriticalSection;
typedef FLinuxSystemWideCriticalSection FSystemWideCriticalSection;
typedef FPThreadsRWLock FRWLock;
