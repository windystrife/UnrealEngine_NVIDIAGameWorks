// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PThreadCriticalSection.h"
#include "PThreadRWLock.h"
#include "Containers/UnrealString.h"
#include "Misc/Timespan.h"

/**
 * Mac implementation of the FSystemWideCriticalSection. Uses exclusive file locking.
 **/
class FMacSystemWideCriticalSection
{
public:
	/** Construct a named, system-wide critical section and attempt to get access/ownership of it */
	explicit FMacSystemWideCriticalSection(const FString& InName, FTimespan InTimeout = FTimespan::Zero());

	/** Destructor releases system-wide critical section if it is currently owned */
	~FMacSystemWideCriticalSection();

	/**
	 * Does the calling thread have ownership of the system-wide critical section?
	 *
	 * @return True if the system-wide lock is obtained. WARNING: Returns true for abandoned locks so shared resources can be in undetermined states.
	 */
	bool IsValid() const;

	/** Releases system-wide critical section if it is currently owned */
	void Release();

private:
	FMacSystemWideCriticalSection(const FMacSystemWideCriticalSection&);
	FMacSystemWideCriticalSection& operator=(const FMacSystemWideCriticalSection&);

private:
	int32 FileHandle;
};

typedef FPThreadsCriticalSection FCriticalSection;
typedef FMacSystemWideCriticalSection FSystemWideCriticalSection;
typedef FPThreadsRWLock FRWLock;
