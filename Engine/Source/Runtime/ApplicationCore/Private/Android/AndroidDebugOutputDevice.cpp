// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidDebugOutputDevice.h"
#include "OutputDeviceHelper.h"
#include "CoreGlobals.h"
#include "UnrealString.h"

FAndroidDebugOutputDevice::FAndroidDebugOutputDevice()
{
}

void FAndroidDebugOutputDevice::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	FPlatformMisc::LowLevelOutputDebugString(*FOutputDeviceHelper::FormatLogLine(Verbosity, Category, Msg, GPrintLogTimes));
}
