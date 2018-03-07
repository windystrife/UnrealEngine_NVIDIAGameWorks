// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/OutputDeviceDebug.h"
#include "CoreGlobals.h"
#include "Misc/OutputDeviceHelper.h"

/**
 * Serializes the passed in data unless the current event is suppressed.
 *
 * @param	Data	Text to log
 * @param	Event	Event name used for suppression purposes
 */
void FOutputDeviceDebug::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time )
{
	static bool Entry=false;
	if( !GIsCriticalError || Entry )
	{
		if (Verbosity != ELogVerbosity::SetColor)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s%s"),*FOutputDeviceHelper::FormatLogLine(Verbosity, Category, Data, GPrintLogTimes, Time),LINE_TERMINATOR);
		}
	}
	else
	{
		Entry=true;
		Serialize( Data, Verbosity, Category );
		Entry=false;
	}
}

void FOutputDeviceDebug::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	Serialize( Data, Verbosity, Category, -1.0 );
}
