// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

/** Base class for objects in TLS that support auto-cleanup. */
class CORE_API FTlsAutoCleanup
{
public:
	/** Virtual destructor. */
	virtual ~FTlsAutoCleanup()
	{}

	/** Register this instance to be auto-cleanup. */
	void Register();
};

/** Wrapper for values to be stored in TLS that support auto-cleanup. */
template< class T >
class TTlsAutoCleanupValue
	: public FTlsAutoCleanup
{
public:

	/** Constructor. */
	TTlsAutoCleanupValue(const T& InValue)
		: Value(InValue)
	{ }

	/** Gets the value. */
	T Get() const
	{
		return Value;
	}

private:

	/** The value. */
	T Value;
};
