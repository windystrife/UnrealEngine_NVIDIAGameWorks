// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/OutputDevice.h"
#include "MinimalWindowsApi.h"

class FName;

/**
 * Output device that writes to Windows Event Log
 */
class CORE_API FWindowsEventLogOutputDevice
	: public FOutputDevice
{
	/** Handle to the event log object */
	Windows::HANDLE EventLog;

public:
	/**
	 * Constructor, initializing member variables
	 */
	FWindowsEventLogOutputDevice();

	/** Destructor that cleans up any remaining resources */
	virtual ~FWindowsEventLogOutputDevice();

	virtual void Serialize(const TCHAR* Buffer, ELogVerbosity::Type Verbosity, const class FName& Category) override;
	
	/** Does nothing */
	virtual void Flush(void);

	/**
	 * Closes any event log handles that are open
	 */
	virtual void TearDown(void);
};
