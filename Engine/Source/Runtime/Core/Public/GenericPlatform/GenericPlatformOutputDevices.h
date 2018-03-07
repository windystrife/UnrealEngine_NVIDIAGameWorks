// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"

class FOutputDeviceConsole;
class FOutputDeviceError;

/**
 * Generic implementation for most platforms
 */
struct CORE_API FGenericPlatformOutputDevices
{
	/** Add output devices which can vary depending on platform, configuration, command line parameters. */
	static void							SetupOutputDevices();
	static FString						GetAbsoluteLogFilename();
	static FOutputDevice*				GetLog();

	static FOutputDevice*				GetEventLog()
	{
		return nullptr; // normally only used for dedicated servers
	}

protected:
	static TCHAR CachedAbsoluteFilename[1024];
};
