// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/CriticalSection.h"

/**
 * Implements a scope lock.
 *
 * This is a utility class that handles scope level locking. It's very useful
 * to keep from causing deadlocks due to exceptions being caught and knowing
 * about the number of locks a given thread has on a resource. Example:
 *
 * <code>
 *	{
 *		// Synchronize thread access to the following data
 *		FScopeLock ScopeLock(SynchObject);
 *		// Access data that is shared among multiple threads
 *		...
 *		// When ScopeLock goes out of scope, other threads can access data
 *	}
 * </code>
 */
class FScopeLock
{
public:

	/**
	 * Constructor that performs a lock on the synchronization object
	 *
	 * @param InSynchObject The synchronization object to manage
	 */
	FScopeLock( FCriticalSection* InSynchObject )
		: SynchObject(InSynchObject)
	{
		check(SynchObject);
		SynchObject->Lock();
	}

	/** Destructor that performs a release on the synchronization object. */
	~FScopeLock()
	{
		check(SynchObject);
		SynchObject->Unlock();
	}
private:

	/** Default constructor (hidden on purpose). */
	FScopeLock();

	/** Copy constructor( hidden on purpose). */
	FScopeLock(const FScopeLock& InScopeLock);

	/** Assignment operator (hidden on purpose). */
	FScopeLock& operator=( FScopeLock& InScopeLock )
	{
		return *this;
	}

private:

	// Holds the synchronization object to aggregate and scope manage.
	FCriticalSection* SynchObject;
};
