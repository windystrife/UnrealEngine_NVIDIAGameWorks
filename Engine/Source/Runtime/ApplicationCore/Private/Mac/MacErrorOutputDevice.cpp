// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MacErrorOutputDevice.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ConfigCacheIni.h"
#include "FeedbackContextAnsi.h"
#include "MacApplication.h"
#include "MacConsoleOutputDevice.h"
#include "MacPlatformApplicationMisc.h"
#include "CocoaThread.h"

#include "HAL/PlatformApplicationMisc.h"

FMacErrorOutputDevice::FMacErrorOutputDevice()
:	ErrorPos(0)
{}

void FMacErrorOutputDevice::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	FPlatformMisc::DebugBreak();

	if( !GIsCriticalError )
	{   
		// First appError.
		GIsCriticalError = 1;
		TCHAR ErrorBuffer[1024];
		ErrorBuffer[0] = 0;
		// pop up a crash window if we are not in unattended mode
		if( FApp::IsUnattended() == false )
		{ 
			UE_LOG(LogMac, Error, TEXT("appError called: %s"), Msg );
		} 
		// log the warnings to the log
		else
		{
			UE_LOG(LogMac, Error, TEXT("appError called: %s"), Msg );
		}
		FCString::Strncpy( GErrorHist, Msg, ARRAY_COUNT(GErrorHist) - 5 );
		FCString::Strncat( GErrorHist, TEXT("\r\n\r\n"), ARRAY_COUNT(GErrorHist) - 1  );
		ErrorPos = FCString::Strlen(GErrorHist);
	}
	else
	{
		UE_LOG(LogMac, Error, TEXT("Error reentered: %s"), Msg );
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

void FMacErrorOutputDevice::HandleError()
{
	// make sure we don't report errors twice
	static int32 CallCount = 0;
	int32 NewCallCount = FPlatformAtomics::InterlockedIncrement(&CallCount);
	if (NewCallCount != 1)
	{
		UE_LOG(LogMac, Error, TEXT("HandleError re-entered.") );
		return;
	}

	// Trigger the OnSystemFailure hook if it exists
	FCoreDelegates::OnHandleSystemError.Broadcast();

	GIsGuarded				= 0;
	GIsRunning				= 0;
	GIsCriticalError		= 1;
	GLogConsole				= NULL;
	GErrorHist[ARRAY_COUNT(GErrorHist)-1]=0;

	// Dump the error and flush the log.
	UE_LOG(LogMac, Log, TEXT("=== Critical error: ===") LINE_TERMINATOR TEXT("%s") LINE_TERMINATOR, GErrorExceptionDescription);
	UE_LOG(LogMac, Log, GErrorHist);

	GLog->Flush();

	// Unhide the mouse.
	// @TODO: Remove usage of deprecated CGCursorIsVisible function
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	while (!CGCursorIsVisible())
	{
		CGDisplayShowCursor(kCGDirectMainDisplay);
	}
#pragma clang diagnostic pop
	// Release capture and allow mouse to freely roam around.
	CGAssociateMouseAndMouseCursorPosition(true);

	FPlatformApplicationMisc::ClipboardCopy(GErrorHist);

	FPlatformMisc::SubmitErrorReport( GErrorHist, EErrorReportMode::Interactive );

	FCoreDelegates::OnShutdownAfterError.Broadcast();
}

@implementation FMacConsoleWindow
- (void)windowWillClose:(NSNotification*)Notification
{
	if (!MacApplication && [[NSApp orderedWindows] count] == 1)
	{
		_Exit(0);
	}
}
@end
