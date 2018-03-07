// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WindowsErrorOutputDevice.h"
#include "Logging/LogMacros.h"
#include "Misc/OutputDevice.h"
#include "HAL/PlatformTime.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "CoreGlobals.h"
#include "Misc/CString.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Internationalization/Internationalization.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/CoreDelegates.h"
#include "GenericPlatform/GenericApplication.h"
#include "Misc/App.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/FeedbackContextAnsi.h"
#include "HAL/ThreadHeartBeat.h"
#include "Windows/WindowsHWrapper.h"

FWindowsErrorOutputDevice::FWindowsErrorOutputDevice()
{
}

void FWindowsErrorOutputDevice::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	FPlatformMisc::DebugBreak();
   
	if( !GIsCriticalError )
	{   
		const int32 LastError = ::GetLastError();

		// First appError.
		GIsCriticalError = 1;
		TCHAR ErrorBuffer[1024];
		ErrorBuffer[0] = 0;

		// Windows error.
		if (LastError == 0)
		{
			UE_LOG(LogWindows, Log, TEXT("Windows GetLastError: %s (%i)"), FPlatformMisc::GetSystemErrorMessage(ErrorBuffer, 1024, LastError), LastError);
		}
		else
		{
			UE_LOG(LogWindows, Error, TEXT("Windows GetLastError: %s (%i)"), FPlatformMisc::GetSystemErrorMessage(ErrorBuffer, 1024, LastError), LastError);
		}
	}
	else
	{
		UE_LOG(LogWindows, Error, TEXT("Error reentered: %s"), Msg );
	}

	if( GIsGuarded )
	{
		// Propagate error so structured exception handler can perform necessary work.
#if PLATFORM_EXCEPTIONS_DISABLED
		FPlatformMisc::DebugBreak();
#endif
		FPlatformMisc::RaiseException( 1 );
	}
	else
	{
		// We crashed outside the guarded code (e.g. appExit).
		HandleError();
		FPlatformMisc::RequestExit( true );
	}
}

void FWindowsErrorOutputDevice::HandleError()
{
	// make sure we don't report errors twice
	static int32 CallCount = 0;
	int32 NewCallCount = FPlatformAtomics::InterlockedIncrement(&CallCount);
	if (NewCallCount != 1)
	{
		UE_LOG(LogWindows, Error, TEXT("HandleError re-entered.") );
		return;
	}
	
	GIsGuarded				= 0;
	GIsRunning				= 0;
	GIsCriticalError		= 1;
	GLogConsole				= NULL;
	GErrorHist[ARRAY_COUNT(GErrorHist)-1]=0;

	// Trigger the OnSystemFailure hook if it exists
	// make sure it happens after GIsGuarded is set to 0 in case this hook crashes
	FCoreDelegates::OnHandleSystemError.Broadcast();

	// Dump the error and flush the log.
#if !NO_LOGGING
	FDebug::LogFormattedMessageWithCallstack(LogWindows.GetCategoryName(), __FILE__, __LINE__, TEXT("=== Critical error: ==="), GErrorHist, ELogVerbosity::Error);
#endif
	GLog->PanicFlushThreadedLogs();

	// Unhide the mouse.
	while( ::ShowCursor(true)<0 );
	// Release capture.
	::ReleaseCapture();
	// Allow mouse to freely roam around.
	::ClipCursor(NULL);
	
	// Copy to clipboard in non-cooked editor builds.
	FPlatformApplicationMisc::ClipboardCopy(GErrorHist);

	FPlatformMisc::SubmitErrorReport( GErrorHist, EErrorReportMode::Interactive );

	FCoreDelegates::OnShutdownAfterError.Broadcast();
}
