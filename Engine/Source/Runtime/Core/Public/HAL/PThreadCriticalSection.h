// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include <pthread.h>
#include <errno.h>


/**
 * This is the PThreads version of a critical section. It uses pthreads
 * to implement its locking.
 */
class FPThreadsCriticalSection
{
	/**
	 * The pthread-specific critical section
	 */
	pthread_mutex_t Mutex;

public:

	/**
	 * Constructor that initializes the aggregated critical section
	 */
	FORCEINLINE FPThreadsCriticalSection(void)
	{
		// make a recursive mutex
		pthread_mutexattr_t MutexAttributes;
		pthread_mutexattr_init(&MutexAttributes);
		pthread_mutexattr_settype(&MutexAttributes, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&Mutex, &MutexAttributes);
		pthread_mutexattr_destroy(&MutexAttributes);
	}

	/**
	 * Destructor cleaning up the critical section
	 */
	FORCEINLINE ~FPThreadsCriticalSection(void)
	{
		pthread_mutex_destroy(&Mutex);
	}

	/**
	 * Locks the critical section
	 */
	FORCEINLINE void Lock(void)
	{
		pthread_mutex_lock(&Mutex);
	}
	
	/**
	 * Attempt to take a lock and returns whether or not a lock was taken.
	 *
	 * @return true if a lock was taken, false otherwise.
	 */
	FORCEINLINE bool TryLock()
	{
		return 0 == pthread_mutex_trylock(&Mutex);
	}

	/**
	 * Releases the lock on the critical seciton
	 */
	FORCEINLINE void Unlock(void)
	{
		pthread_mutex_unlock(&Mutex);
	}

private:
	FPThreadsCriticalSection(const FPThreadsCriticalSection&);
	FPThreadsCriticalSection& operator=(const FPThreadsCriticalSection&);
};
