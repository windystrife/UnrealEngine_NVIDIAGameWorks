// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LinuxErrorOutputDevice.h"
#include "Containers/StringConv.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "Linux/LinuxApplication.h"
#include "Linux/LinuxPlatformApplicationMisc.h"

FLinuxErrorOutputDevice::FLinuxErrorOutputDevice()
:	ErrorPos(0)
{
}

void FLinuxErrorOutputDevice::Serialize(const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	FPlatformMisc::DebugBreak();

	if (!GIsCriticalError)
	{
		// First appError.
		GIsCriticalError = 1;
		TCHAR ErrorBuffer[1024];
		ErrorBuffer[0] = 0;
		// pop up a crash window if we are not in unattended mode; 
		if (WITH_EDITOR && FApp::IsUnattended() == false)
		{
			// @TODO Not implemented
			UE_LOG(LogLinux, Error, TEXT("appError called: %s"), Msg );
		}
		// log the warnings to the log
		else
		{
			UE_LOG(LogLinux, Error, TEXT("appError called: %s"), Msg );
		}
		FCString::Strncpy( GErrorHist, Msg, ARRAY_COUNT(GErrorHist) - 5 );
		FCString::Strncat( GErrorHist, TEXT("\r\n\r\n"), ARRAY_COUNT(GErrorHist) - 1 );
		ErrorPos = FCString::Strlen(GErrorHist);
	}
	else
	{
		UE_LOG(LogLinux, Error, TEXT("Error reentered: %s"), Msg );
	}

	if( GIsGuarded )
	{
		// Propagate error so structured exception handler can perform necessary work.
#if PLATFORM_EXCEPTIONS_DISABLED
		FPlatformMisc::DebugBreak();
#endif
		FPlatformMisc::RaiseException(1);
	}
	else
	{
		// We crashed outside the guarded code (e.g. appExit).
		HandleError();
		FPlatformMisc::RequestExit(true);
	}
}

void FLinuxErrorOutputDevice::HandleError()
{
	// make sure we don't report errors twice
	static int32 CallCount = 0;
	int32 NewCallCount = FPlatformAtomics::InterlockedIncrement(&CallCount);
	if (NewCallCount != 1)
	{
		UE_LOG(LogLinux, Error, TEXT("HandleError re-entered.") );
		return;
	}

	// Trigger the OnSystemFailure hook if it exists
	FCoreDelegates::OnHandleSystemError.Broadcast();

#if !PLATFORM_EXCEPTIONS_DISABLED
	try
	{
#endif // !PLATFORM_EXCEPTIONS_DISABLED
		GIsGuarded = 0;
		GIsRunning = 0;
		GIsCriticalError = 1;
		GLogConsole = NULL;
		GErrorHist[ARRAY_COUNT(GErrorHist)-1] = 0;

		// Dump the error and flush the log.
		UE_LOG(LogLinux, Log, TEXT("=== Critical error: ===") LINE_TERMINATOR TEXT("%s") LINE_TERMINATOR, GErrorExceptionDescription);
		UE_LOG(LogLinux, Log, GErrorHist);

		GLog->Flush();

		// do not copy if graphics have not been initialized or if we're on the wrong thread
		if (FApp::CanEverRender() && IsInGameThread())
		{
			FPlatformApplicationMisc::ClipboardCopy(GErrorHist);
		}

		FPlatformMisc::SubmitErrorReport(GErrorHist, EErrorReportMode::Interactive);
		FCoreDelegates::OnShutdownAfterError.Broadcast();
#if !PLATFORM_EXCEPTIONS_DISABLED
	}
	catch(...)
	{}
#endif // !PLATFORM_EXCEPTIONS_DISABLED
}
