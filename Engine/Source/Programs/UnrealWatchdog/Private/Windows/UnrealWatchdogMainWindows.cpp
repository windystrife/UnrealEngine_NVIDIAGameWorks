// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealWatchdog.h"
#include "WatchdogAnalytics.h"
#include "HAL/PlatformProcess.h"
#include "Windows/WindowsHWrapper.h"

/**
* WinMain, called when the application is started
*/
INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	FString CommandLine;
	for (int32 Option = 1; Option < ArgC; Option++)
	{
		CommandLine += TEXT(" ");
		FString Argument(ArgV[Option]);
		if (Argument.Contains(TEXT(" ")))
		{
			if (Argument.Contains(TEXT("=")))
			{
				FString ArgName;
				FString ArgValue;
				Argument.Split(TEXT("="), &ArgName, &ArgValue);
				Argument = FString::Printf(TEXT("%s=\"%s\""), *ArgName, *ArgValue);
			}
			else
			{
				Argument = FString::Printf(TEXT("\"%s\""), *Argument);
			}
		}
		CommandLine += Argument;
	}

	// hide the console window
	ShowWindow(GetConsoleWindow(), SW_HIDE);

	// Run the app
	RunUnrealWatchdog(*CommandLine);

	return 0;
}

FProcHandle GetProcessHandle(const FWatchdogCommandLine& CommandLine)
{
	return FPlatformProcess::OpenProcess(CommandLine.ParentProcessId);
}