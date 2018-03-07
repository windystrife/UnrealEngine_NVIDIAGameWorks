// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/ThreadSafeCounter.h"

/**
 * Thread safe bool, wraps FThreadSafeCounter
 */
class FThreadSafeBool
	: private FThreadSafeCounter
{
public:
	/**
	 * Constructor optionally takes value to initialize with, otherwise initializes false
	 * @param bValue	Value to initialize with
	 */
	FThreadSafeBool(bool bValue = false)
		: FThreadSafeCounter(bValue ? 1 : 0)
	{}

	/**
	 * Operator to use this struct as a bool with thread safety
	 */
	FORCEINLINE operator bool() const
	{
		return GetValue() != 0;
	}

	/**
	 * Operator to set the bool value with thread safety
	 */
	FORCEINLINE bool operator=(bool bNewValue)
	{
		Set(bNewValue ? 1 : 0);
		return bNewValue;
	}

	/**
	 * Sets a new value atomically, and returns the old value.
	 *
	 * @param bNewValue   Value to set
	 * @return The old value
	 */
	FORCEINLINE bool AtomicSet(bool bNewValue)
	{
		return Set(bNewValue ? 1 : 0) != 0;
	}
};
