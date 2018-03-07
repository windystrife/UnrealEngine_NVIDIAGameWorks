// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"
#include "ExceptionHandling.h"
#include "MacPlatformCrashContext.h"
#include "CocoaThread.h"
#include "HAL/PlatformApplicationMisc.h"

/**
 * Because crash reporters can crash, too - only used for Sandboxed applications
 */
void CrashReporterCrashHandler(const FGenericCrashContext& GenericContext)
{
	// We never emit a crash report in Sandboxed builds from CRC as if we do, then the crashed application's
	// crash report is overwritten by the CRC's when trampolining into the Apple Crash Reporter.
	_Exit(0);
}

static FString GSavedCommandLine;

@interface UE4AppDelegate : NSObject <NSApplicationDelegate, NSFileManagerDelegate>
{
}

@end

@implementation UE4AppDelegate

//handler for the quit apple event used by the Dock menu
- (void)handleQuitEvent:(NSAppleEventDescriptor*)Event withReplyEvent:(NSAppleEventDescriptor*)ReplyEvent
{
    [self requestQuit:self];
}

- (IBAction)requestQuit:(id)Sender
{
	GIsRequestingExit = true;
}

- (void) runGameThread:(id)Arg
{
	FPlatformMisc::SetGracefulTerminationHandler();
	// For sandboxed applications CRC can never report a crash, or we break trampolining into Apple's crash reporter.
	if(FPlatformProcess::IsSandboxedApplication())
	{
		FPlatformMisc::SetCrashHandler(CrashReporterCrashHandler);
	}
	
	RunCrashReportClient(*GSavedCommandLine);
	
	[NSApp terminate: self];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)Sender;
{
	if(!GIsRequestingExit || ([NSThread gameThread] && [NSThread gameThread] != [NSThread mainThread]))
	{
		[self requestQuit:self];
		return NSTerminateLater;
	}
	else
	{
		return NSTerminateNow;
	}
}

- (void)applicationDidFinishLaunching:(NSNotification *)Notification
{
	//install the custom quit event handler
    NSAppleEventManager* appleEventManager = [NSAppleEventManager sharedAppleEventManager];
    [appleEventManager setEventHandler:self andSelector:@selector(handleQuitEvent:withReplyEvent:) forEventClass:kCoreEventClass andEventID:kAEQuitApplication];
	
	FPlatformApplicationMisc::ActivateApplication();
	RunGameThread(self, @selector(runGameThread:));
}

@end

int main(int argc, char *argv[])
{
	for (int32 Option = 1; Option < argc; Option++)
	{
		GSavedCommandLine += TEXT(" ");
		FString Argument(ANSI_TO_TCHAR(argv[Option]));
		if (Argument.Contains(TEXT(" ")))
		{
			if (Argument.Contains(TEXT("=")))
			{
				FString ArgName;
				FString ArgValue;
				Argument.Split( TEXT("="), &ArgName, &ArgValue );
				Argument = FString::Printf( TEXT("%s=\"%s\""), *ArgName, *ArgValue );
			}
			else
			{
				Argument = FString::Printf(TEXT("\"%s\""), *Argument);
			}
		}
		GSavedCommandLine += Argument;
	}
	
	SCOPED_AUTORELEASE_POOL;
	[NSApplication sharedApplication];
	[NSApp setDelegate:[UE4AppDelegate new]];
	[NSApp run];
	return 0;
}
