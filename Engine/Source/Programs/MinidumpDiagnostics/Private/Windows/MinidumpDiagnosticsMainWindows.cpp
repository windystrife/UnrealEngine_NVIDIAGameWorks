// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MinidumpDiagnosticsApp.h"

#include "CoreMinimal.h"
#include "RequiredProgramMainCPPInclude.h"
#include "ExceptionHandling.h"

IMPLEMENT_APPLICATION( MinidumpDiagnostics, "MinidumpDiagnostics" )

	
/** 
 * A simple crash handler that prints the callstack to the log
 */
int32 SimpleCrashHandler( LPEXCEPTION_POINTERS ExceptionInfo )
{
	const SIZE_T StackTraceSize = 65535;
	ANSICHAR* StackTrace = (ANSICHAR*)GMalloc->Malloc( StackTraceSize );
	StackTrace[0] = 0;
	FPlatformStackWalk::StackWalkAndDump( StackTrace, StackTraceSize, 0, ExceptionInfo->ContextRecord );

	FCString::Strncat( GErrorHist, TEXT( "\r\n\r\n" ), ARRAY_COUNT( GErrorHist ) );
	FCString::Strncat( GErrorHist, ANSI_TO_TCHAR( StackTrace ), ARRAY_COUNT( GErrorHist ) );

	GMalloc->Free( StackTrace );

	GError->HandleError();

	FPlatformMisc::RequestExit( true );

	return EXCEPTION_EXECUTE_HANDLER;
}

// More Windows glue
int32 GuardedMain(int32 Argc, TCHAR* Argv[])
{
	GEngineLoop.PreInit( Argc, Argv );

#if PLATFORM_WINDOWS
	SetConsoleTitle( TEXT( "MinidumpDiagnostics" ) );
#endif

	int32 Result = RunMinidumpDiagnostics( Argc, Argv );
	return Result;
}

// Windows glue
int32 GuardedMainWrapper(int32 ArgC, TCHAR* ArgV[])
{
	int32 ReturnCode = 0;

	if (FPlatformMisc::IsDebuggerPresent())
	{
		ReturnCode = GuardedMain( ArgC, ArgV );
	}
	else
	{
		__try
		{
			GIsGuarded = 1;
			ReturnCode = GuardedMain( ArgC, ArgV );
			GIsGuarded = 0;
		}
		__except( SimpleCrashHandler( GetExceptionInformation() ) )
		{
		}
	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();

	return ReturnCode;
}

 // Main entry point to the application
INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	const int32 Result = GuardedMainWrapper( ArgC, ArgV );
	return Result;
}
