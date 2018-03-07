// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSPlatformOutputDevices.mm: iOS implementations of OutputDevices functions
=============================================================================*/

#include "IOSErrorOutputDevice.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformAtomics.h"
#include "Misc/OutputDeviceRedirector.h"
#include "CoreGlobals.h"

FIOSErrorOutputDevice::FIOSErrorOutputDevice()
:	ErrorPos(0)
{
}

void FIOSErrorOutputDevice::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	// @todo iosmerge: this was a big mess copied from Mac that we don't want at all. can we use default like other consoles?
	if( GIsGuarded )
	{
//		FOutputDevice::Serialize(Msg, Verbosity, Category);
		FPlatformMisc::DebugBreak();
	}
	else
	{
		// We crashed outside the guarded code (e.g. appExit).
		HandleError();
		FPlatformMisc::RequestExit( true );
	}
}

void FIOSErrorOutputDevice::HandleError()
{
	// make sure we don't report errors twice
	static int32 CallCount = 0;
	int32 NewCallCount = FPlatformAtomics::InterlockedIncrement(&CallCount);
	if (NewCallCount != 1)
	{
		UE_LOG(LogIOS, Error, TEXT("HandleError re-entered.") );
		return;
	}

	GIsGuarded = 0;
	GIsRunning = 0;
	GIsCriticalError = 1;
	GLogConsole = NULL;

	GLog->Flush();
}
