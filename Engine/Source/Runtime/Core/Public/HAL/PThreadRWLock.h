// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include <pthread.h>
#include <errno.h>

/**
 * FPThreadsRWLock - Read/Write Mutex
 *	- Provides non-recursive Read/Write (or shared-exclusive) access.
 */
class FPThreadsRWLock
{
public:
	FPThreadsRWLock()
	{
		int Err = pthread_rwlock_init(&Mutex, nullptr);
		checkf(Err == 0, TEXT("pthread_rwlock_init failed with error: %d"), Err);
	}

	~FPThreadsRWLock()
	{
		int Err = pthread_rwlock_destroy(&Mutex);
		checkf(Err == 0, TEXT("pthread_rwlock_destroy failed with error: %d"), Err);
	}
	
	void ReadLock()
	{
		int Err = pthread_rwlock_rdlock(&Mutex);
		checkf(Err == 0, TEXT("pthread_rwlock_rdlock failed with error: %d"), Err);
	}
	
	void WriteLock()
	{
		int Err = pthread_rwlock_wrlock(&Mutex);
		checkf(Err == 0, TEXT("pthread_rwlock_wrlock failed with error: %d"), Err);
	}
	
	void ReadUnlock()
	{
		int Err = pthread_rwlock_unlock(&Mutex);
		checkf(Err == 0, TEXT("pthread_rwlock_unlock failed with error: %d"), Err);
	}
	
	void WriteUnlock()
	{
		int Err = pthread_rwlock_unlock(&Mutex);
		checkf(Err == 0, TEXT("pthread_rwlock_unlock failed with error: %d"), Err);
	}

private:
	pthread_rwlock_t Mutex;
};
