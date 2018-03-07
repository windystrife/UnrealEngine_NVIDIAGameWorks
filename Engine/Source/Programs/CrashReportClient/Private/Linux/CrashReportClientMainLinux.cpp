// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Containers/StringConv.h"
#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/OutputDeviceError.h"
#include "Misc/FeedbackContext.h"
#include "CrashReportClientApp.h"
#include "Linux/LinuxPlatformCrashContext.h"
#include <locale.h>

extern int32 ReportCrash(const FLinuxCrashContext& Context);	// FIXME: handle expose it someplace else?

/**
 * Because crash reporters can crash, too
 */
void CrashReporterCrashHandler(const FGenericCrashContext& GenericContext)
{
	// at this point we should already be using malloc crash handler (see PlatformCrashHandler)

	const FLinuxCrashContext& Context = static_cast< const FLinuxCrashContext& >( GenericContext );

	printf("CrashHandler: Signal=%d\n", Context.Signal);
	const_cast< FLinuxCrashContext& >(Context).CaptureStackTrace();
	if (GLog)
	{
		GLog->Flush();
	}
	if (GWarn)
	{
		GWarn->Flush();
	}
	if (GError)
	{
		GError->Flush();
		GError->HandleError();
	}
	FPlatformMisc::RequestExit(true);
}


/**
 * main(), called when the application is started
 */
int main(int argc, const char *argv[])
{
	FPlatformMisc::SetGracefulTerminationHandler();
	FPlatformMisc::SetCrashHandler(CrashReporterCrashHandler);

	FString SavedCommandLine;

	setlocale(LC_CTYPE, "");
	for (int32 Option = 1; Option < argc; Option++)
	{
		SavedCommandLine += TEXT(" ");
		SavedCommandLine += UTF8_TO_TCHAR(argv[Option]);	// note: technically it depends on locale
	}

	// Run the app
	RunCrashReportClient(*SavedCommandLine);

	return 0;
}
