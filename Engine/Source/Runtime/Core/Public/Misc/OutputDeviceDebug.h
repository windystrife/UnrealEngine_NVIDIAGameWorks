// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/OutputDevice.h"

class CORE_API FOutputDeviceDebug : public FOutputDevice
{
public:
	/**
	* Serializes the passed in data unless the current event is suppressed.
	*
	* @param	Data	Text to log
	* @param	Event	Event name used for suppression purposes
	*/
	virtual void Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time) override;

	virtual void Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category) override;

	virtual bool CanBeUsedOnAnyThread() const override
	{
		return true;
	}
};

