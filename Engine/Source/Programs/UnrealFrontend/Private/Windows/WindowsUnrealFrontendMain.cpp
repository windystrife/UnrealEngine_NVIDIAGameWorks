// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UnrealFrontendMain.h"
#include "ExceptionHandling.h"
#include "LaunchEngineLoop.h"
#include "WindowsHWrapper.h"
#include "Misc/CommandLine.h"
#include "Misc/OutputDeviceError.h"

/**
 * The main application entry point for Windows platforms.
 *
 * @param hInInstance Handle to the current instance of the application.
 * @param hPrevInstance Handle to the previous instance of the application (always NULL).
 * @param lpCmdLine Command line for the application.
 * @param nShowCmd Specifies how the window is to be shown.
 * @return Application's exit value.
 */
int32 WINAPI WinMain( HINSTANCE hInInstance, HINSTANCE hPrevInstance, char* lpCmdLine, int32 nShowCmd )
{
	hInstance = hInInstance;

	const TCHAR* CmdLine = ::GetCommandLineW();
	CmdLine = FCommandLine::RemoveExeName(CmdLine);

#if !UE_BUILD_SHIPPING
	if (FParse::Param(CmdLine, TEXT("crashreports")))
	{
		GAlwaysReportCrash = true;
	}
#endif

#if WINVER > 0x502	// Windows Error Reporting is not supported on Windows XP
	if (FParse::Param(CmdLine, TEXT("useautoreporter")))
#endif
	{
		GUseCrashReportClient = false;
	}

	int32 ErrorLevel = 0;

#if UE_BUILD_DEBUG
	if (!GAlwaysReportCrash)
#else
	if (FPlatformMisc::IsDebuggerPresent() && !GAlwaysReportCrash)
#endif
	{
		ErrorLevel = UnrealFrontendMain(CmdLine);
	}
	else
	{
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
		__try
#endif
 		{
			GIsGuarded = 1;
			ErrorLevel = UnrealFrontendMain(CmdLine);
			GIsGuarded = 0;
		}
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
		__except (ReportCrash(GetExceptionInformation()))
		{
			ErrorLevel = 1;
			GError->HandleError();
			FPlatformMisc::RequestExit(true);
		}
#endif
	}

	FEngineLoop::AppExit();

	return ErrorLevel;
}
