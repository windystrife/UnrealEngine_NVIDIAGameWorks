// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OutputDevice.h"

/**
 * Generic Output device that writes to Windows Event Log
 */
class FAndroidDebugOutputDevice : public FOutputDevice
{
public:
	/** Constructor, initializing member variables */
	APPLICATIONCORE_API FAndroidDebugOutputDevice();

	/**
	 * Serializes the passed in data unless the current event is suppressed.
	 *
	 * @param	Data	Text to log
	 * @param	Event	Event name used for suppression purposes
	 */
	virtual void Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category ) override;
};

