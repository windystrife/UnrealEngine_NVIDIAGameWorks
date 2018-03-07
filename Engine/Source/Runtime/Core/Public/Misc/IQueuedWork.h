// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

/**
 * Interface for queued work objects.
 *
 * This interface is a type of runnable object that requires no per thread
 * initialization. It is meant to be used with pools of threads in an
 * abstract way that prevents the pool from needing to know any details
 * about the object being run. This allows queuing of disparate tasks and
 * servicing those tasks with a generic thread pool.
 */
class IQueuedWork
{
public:

	/**
	 * This is where the real thread work is done. All work that is done for
	 * this queued object should be done from within the call to this function.
	 */
	virtual void DoThreadedWork() = 0;

	/**
	 * Tells the queued work that it is being abandoned so that it can do
	 * per object clean up as needed. This will only be called if it is being
	 * abandoned before completion. NOTE: This requires the object to delete
	 * itself using whatever heap it was allocated in.
	 */
	virtual void Abandon() = 0;

public:

	/**
	 * Virtual destructor so that child implementations are guaranteed a chance
	 * to clean up any resources they allocated.
	 */
	virtual ~IQueuedWork() { }
};
