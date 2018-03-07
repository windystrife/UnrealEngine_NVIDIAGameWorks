// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidErrorOutputDevice.h"

#include "CoreGlobals.h"
#include "OutputDeviceHelper.h"
#include "HAL/PlatformMisc.h"
#include "Misc/OutputDeviceRedirector.h"

FAndroidErrorOutputDevice::FAndroidErrorOutputDevice()
{
}

void FAndroidErrorOutputDevice::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	FPlatformMisc::LowLevelOutputDebugString(*FOutputDeviceHelper::FormatLogLine(Verbosity, Category, Msg, GPrintLogTimes));
	if (GIsGuarded)
	{
		FPlatformMisc::DebugBreak();
	}
	else
	{
		HandleError();
		FPlatformMisc::RequestExit(true);
	}
}

void FAndroidErrorOutputDevice::HandleError()
{
	static int32 CallCount = 0;
	int32 NewCallCount = FPlatformAtomics::InterlockedIncrement(&CallCount);

	if (NewCallCount != 1)
	{
		UE_LOG(LogAndroid, Error, TEXT("HandleError re-entered."));
		return;
	}
	
	GIsGuarded = 0;
	GIsRunning = 0;
	GIsCriticalError = 1;
	GLogConsole = NULL;

	GLog->Flush();
}
