// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformAffinity.h"

class IQueuedWork;

/**
 * Interface for queued thread pools.
 *
 * This interface is used by all queued thread pools. It used as a callback by
 * FQueuedThreads and is used to queue asynchronous work for callers.
 */
class CORE_API FQueuedThreadPool
{
public:

	/**
	 * Creates the thread pool with the specified number of threads
	 *
	 * @param InNumQueuedThreads Specifies the number of threads to use in the pool
	 * @param StackSize The size of stack the threads in the pool need (32K default)
	 * @param ThreadPriority priority of new pool thread
	 * @return Whether the pool creation was successful or not
	 */
	virtual bool Create( uint32 InNumQueuedThreads, uint32 StackSize = (32 * 1024), EThreadPriority ThreadPriority=TPri_Normal ) = 0;

	/** Tells the pool to clean up all background threads */
	virtual void Destroy() = 0;

	/**
	 * Checks to see if there is a thread available to perform the task. If not,
	 * it queues the work for later. Otherwise it is immediately dispatched.
	 *
	 * @param InQueuedWork The work that needs to be done asynchronously
	 * @see RetractQueuedWork
	 */
	virtual void AddQueuedWork( IQueuedWork* InQueuedWork ) = 0;

	/**
	 * Attempts to retract a previously queued task.
	 *
	 * @param InQueuedWork The work to try to retract
	 * @return true if the work was retracted
	 * @see AddQueuedWork
	 */
	virtual bool RetractQueuedWork( IQueuedWork* InQueuedWork ) = 0;

	/**
	 * Places a thread back into the available pool
	 *
	 * @param InQueuedThread The thread that is ready to be pooled
	 * @return next job or null if there is no job available now
	 */
	virtual IQueuedWork* ReturnToPoolOrGetNextJob( class FQueuedThread* InQueuedThread ) = 0;

	/**
	 * Get the number of queued threads
	 */
	virtual int32 GetNumThreads() const = 0;

public:

	/** Virtual destructor. */
	virtual ~FQueuedThreadPool() { }

public:

	/**
	 * Allocates a thread pool
	 *
	 * @return A new thread pool.
	 */
	static FQueuedThreadPool* Allocate();

	/**
	 *	Stack size for threads created for the thread pool. 
	 *	Can be overridden by other projects.
	 *	If 0 means to use the value passed in the Create method.
	 */
	static uint32 OverrideStackSize;
};


/**
 *  Global thread pool for shared async operations
 */
extern CORE_API FQueuedThreadPool* GThreadPool;

extern CORE_API FQueuedThreadPool* GIOThreadPool;

#if WITH_EDITOR
extern CORE_API FQueuedThreadPool* GLargeThreadPool;
#endif
