// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSDebugOutputDevice.h"
#include "HAL/PlatformMisc.h"
#include "CoreGlobals.h"
#include "Misc/OutputDeviceHelper.h"

FIOSDebugOutputDevice::FIOSDebugOutputDevice() 
{
}

void FIOSDebugOutputDevice::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s%s"),*FOutputDeviceHelper::FormatLogLine(Verbosity, Category, Msg, GPrintLogTimes),LINE_TERMINATOR); 
}

