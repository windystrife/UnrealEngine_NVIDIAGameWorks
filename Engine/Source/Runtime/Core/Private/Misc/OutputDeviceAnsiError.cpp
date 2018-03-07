// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/OutputDeviceAnsiError.h"
#include "Templates/UnrealTemplate.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"

/** Constructor, initializing member variables */
FOutputDeviceAnsiError::FOutputDeviceAnsiError()
:	ErrorPos(0)
{}

/**
 * Serializes the passed in data unless the current event is suppressed.
 *
 * @param	Data	Text to log
 * @param	Event	Event name used for suppression purposes
 */
void FOutputDeviceAnsiError::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	// Display the error and exit.
	FPlatformMisc::LocalPrint( TEXT("\nappError called: \n") );
	FPlatformMisc::LocalPrint( Msg );
	FPlatformMisc::LocalPrint( TEXT("\n") );

	if( !GIsCriticalError )
	{
		// First appError.
		GIsCriticalError = 1;
		UE_LOG(LogHAL, Error, TEXT("appError called: %s"), Msg );
		FCString::Strncpy( GErrorHist, Msg, ARRAY_COUNT(GErrorHist) );
		FCString::Strncat( GErrorHist, TEXT("\r\n\r\n"), ARRAY_COUNT(GErrorHist) );
		ErrorPos = FCString::Strlen(GErrorHist);
	}
	else
	{
		UE_LOG(LogHAL, Error, TEXT("Error reentered: %s"), Msg );
	}

	FPlatformMisc::DebugBreak();

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
		// pop up a crash window if we are not in unattended mode
		if( FApp::IsUnattended() == false )
		{
			FPlatformMisc::RequestExit( true );
		}
		else
		{
			UE_LOG(LogHAL, Error, TEXT("%s"), Msg );
		}		
	}
}

/**
 * Error handling function that is being called from within the system wide global
 * error handler, e.g. using structured exception handling on the PC.
 */
void FOutputDeviceAnsiError::HandleError()
{
	GIsGuarded			= 0;
	GIsRunning			= 0;
	GIsCriticalError	= 1;
	GLogConsole			= NULL;
	GErrorHist[ARRAY_COUNT(GErrorHist)-1]=0;

	if (GLog)
	{
		// print to log and flush it
		UE_LOG( LogHAL, Log, TEXT( "=== Critical error: ===" ) LINE_TERMINATOR LINE_TERMINATOR TEXT( "%s" ) LINE_TERMINATOR, GErrorExceptionDescription );
		UE_LOG(LogHAL, Log, GErrorHist);

		GLog->Flush();
	}
	else
	{
		FPlatformMisc::LocalPrint( GErrorHist );
	}

	FPlatformMisc::LocalPrint( TEXT("\n\nExiting due to error\n") );

	FCoreDelegates::OnShutdownAfterError.Broadcast();
}
