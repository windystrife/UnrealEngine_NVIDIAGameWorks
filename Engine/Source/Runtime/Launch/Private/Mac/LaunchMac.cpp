// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/App.h"
#include "Misc/OutputDeviceError.h"
#include "Misc/FeedbackContext.h"
#include "LaunchEngineLoop.h"
#include "ExceptionHandling.h"
#include "MacPlatformCrashContext.h"

#if WITH_ENGINE
	#include "Engine/Engine.h"
	#include "EngineGlobals.h"
#endif

#if WITH_EDITOR
	#include "Interfaces/IMainFrameModule.h"
	#include "ISettingsModule.h"
#endif

#include <signal.h>
#include "CocoaThread.h"


static FString GSavedCommandLine;
extern int32 GuardedMain( const TCHAR* CmdLine );
extern void LaunchStaticShutdownAfterError();
static int32 GGuardedMainErrorLevel = 0;

/**
 * Game-specific crash reporter
 */
void EngineCrashHandler(const FGenericCrashContext& GenericContext)
{
	const FMacCrashContext& Context = static_cast< const FMacCrashContext& >( GenericContext );
	
	Context.ReportCrash();
	if (GLog)
	{
		GLog->SetCurrentThreadAsMasterThread();
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
	LaunchStaticShutdownAfterError();
	return Context.GenerateCrashInfoAndLaunchReporter();
}

static int32 MacOSVersionCompare(const NSOperatingSystemVersion& VersionA, const NSOperatingSystemVersion& VersionB)
{
	NSInteger ValuesA[3] = { VersionA.majorVersion, VersionA.minorVersion, VersionA.patchVersion };
	NSInteger ValuesB[3] = { VersionB.majorVersion, VersionB.minorVersion, VersionB.patchVersion };

	for (uint32 i = 0; i < 3; i++)
	{
		if (ValuesA[i] < ValuesB[i])
		{
			return -1;
		}
		else if (ValuesA[i] > ValuesB[i])
		{
			return 1;
		}
	}

	return 0;
}

@interface UE4AppDelegate : NSObject <NSApplicationDelegate, NSFileManagerDelegate>
{
#if WITH_EDITOR
	NSString* Filename;
	bool bHasFinishedLaunching;
#endif
}

#if WITH_EDITOR
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename;
#endif

@end

@implementation UE4AppDelegate

- (void)awakeFromNib
{
#if WITH_EDITOR
	Filename = nil;
	bHasFinishedLaunching = false;
#endif
}

#if WITH_EDITOR
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	if(!bHasFinishedLaunching && (GSavedCommandLine.IsEmpty() || GSavedCommandLine.Contains(FString(filename))))
	{
		if ([[NSFileManager defaultManager] fileExistsAtPath:filename])
		{
			Filename = filename;
		}
		return YES;
	}
	else if ([[NSFileManager defaultManager] fileExistsAtPath:filename])
	{
		NSString* ProjectName = [[filename stringByDeletingPathExtension] lastPathComponent];
		
		NSURL* BundleURL = [[NSRunningApplication currentApplication] bundleURL];
		
		NSDictionary* Configuration = [NSDictionary dictionaryWithObject: [NSArray arrayWithObject: ProjectName] forKey: NSWorkspaceLaunchConfigurationArguments];
		
		NSError* Error = nil;
		
		NSRunningApplication* NewInstance = [[NSWorkspace sharedWorkspace] launchApplicationAtURL:BundleURL options:(NSWorkspaceLaunchOptions)(NSWorkspaceLaunchAsync|NSWorkspaceLaunchNewInstance) configuration:Configuration error:&Error];
		
		return (NewInstance != nil);
	}
	else
	{
		return YES;
	}
}
#endif

- (IBAction)requestQuit:(id)Sender
{
	GameThreadCall(^{
		if (GEngine)
		{
			if (GIsEditor)
			{
				if (IsRunningCommandlet())
				{
					GIsRequestingExit = true;
				}
				else
				{
					GEngine->DeferredCommands.Add(TEXT("CLOSE_SLATE_MAINFRAME"));
				}
			}
			else
			{
				GEngine->DeferredCommands.Add(TEXT("EXIT"));
			}
		}
	}, @[ NSDefaultRunLoopMode ], false);
}

- (IBAction)showAboutWindow:(id)Sender
{
#if WITH_EDITOR
	GameThreadCall(^{
		if (FModuleManager::Get().IsModuleLoaded(TEXT("MainFrame")))
		{
			FModuleManager::GetModuleChecked<IMainFrameModule>(TEXT("MainFrame")).ShowAboutWindow();
		}
	}, @[ NSDefaultRunLoopMode ], false);
#else
	[NSApp orderFrontStandardAboutPanel:Sender];
#endif
}

#if WITH_EDITOR
- (IBAction)showPreferencesWindow:(id)Sender
{
	GameThreadCall(^{
		if (FModuleManager::Get().IsModuleLoaded(TEXT("Settings")))
		{
			FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer(FName("Editor"), FName("General"), FName("Appearance"));
		}
	}, @[ NSDefaultRunLoopMode ], false);
}
#endif

//handler for the quit apple event used by the Dock menu
- (void)handleQuitEvent:(NSAppleEventDescriptor*)Event withReplyEvent:(NSAppleEventDescriptor*)ReplyEvent
{
    [self requestQuit:self];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)Sender;
{
	if(!GIsRequestingExit || ([NSThread gameThread] && [NSThread gameThread] != [NSThread mainThread]))
	{
		if (!GIsRequestingExit)
		{
			[self requestQuit:self];
		}
		return NSTerminateLater;
	}
	else
	{
		return NSTerminateNow;
	}
}

- (void) runGameThread:(id)Arg
{
	bool bIsBuildMachine = false;
#if !UE_BUILD_SHIPPING
	if (FParse::Param(*GSavedCommandLine, TEXT("BUILDMACHINE")))
	{
		bIsBuildMachine = true;
	}
#endif
	
#if UE_BUILD_DEBUG
	if( true && !GAlwaysReportCrash )
#else
	if( bIsBuildMachine || ( FPlatformMisc::IsDebuggerPresent() && !GAlwaysReportCrash ) )
#endif
	{
		// Don't use exception handling when a debugger is attached to exactly trap the crash. This does NOT check
		// whether we are the first instance or not!
		GGuardedMainErrorLevel = GuardedMain( *GSavedCommandLine );
	}
	else
	{
		if (!bIsBuildMachine)
		{
			FPlatformMisc::SetCrashHandler(EngineCrashHandler);
		}
		GIsGuarded = 1;
		// Run the guarded code.
		GGuardedMainErrorLevel = GuardedMain( *GSavedCommandLine );
		GIsGuarded = 0;
	}

	FEngineLoop::AppExit();

	[NSApp terminate: nil];
}

- (void)applicationDidFinishLaunching:(NSNotification *)Notification
{
	SCOPED_AUTORELEASE_POOL;

	// Make sure we're running on a supported version of macOS. In some situations we cannot depend on the OS to perform the check for us.
	NSDictionary* InfoDictionary = [[NSBundle mainBundle] infoDictionary];
	NSString* MinimumSystemVersionString = (NSString*)InfoDictionary[@"LSMinimumSystemVersion"];
	NSOperatingSystemVersion MinimumSystemVersion = { 0 };
	NSOperatingSystemVersion CurrentSystemVersion = [[NSProcessInfo processInfo] operatingSystemVersion];
	NSOperatingSystemVersion LatestSierraSystemVersion = { 10, 12, 6 };
	NSArray<NSString*>* VersionComponents = [MinimumSystemVersionString componentsSeparatedByString:@"."];
	MinimumSystemVersion.majorVersion = [[VersionComponents objectAtIndex:0] integerValue];
	MinimumSystemVersion.minorVersion = VersionComponents.count > 1 ? [[VersionComponents objectAtIndex:1] integerValue] : 0;
	MinimumSystemVersion.patchVersion = VersionComponents.count > 2 ? [[VersionComponents objectAtIndex:2] integerValue] : 0;

	// Make sure that the min version in Info.plist is at least 10.12.6, as that's the absolute minimum
	if (MacOSVersionCompare(MinimumSystemVersion, LatestSierraSystemVersion) < 0)
	{
		MinimumSystemVersion = LatestSierraSystemVersion;
		MinimumSystemVersionString = @"10.12.6";
	}

	if (MacOSVersionCompare(CurrentSystemVersion, MinimumSystemVersion) < 0)
	{
		CFDictionaryRef SessionDictionary = CGSessionCopyCurrentDictionary();
		const bool bIsWindowServerAvailable = SessionDictionary != nullptr;
		if (bIsWindowServerAvailable)
		{
			NSAlert* AlertPanel = [NSAlert new];
			[AlertPanel setAlertStyle:NSAlertStyleCritical];
			[AlertPanel setInformativeText:[NSString stringWithFormat:@"You have macOS %d.%d.%d. The application requires macOS %@ or later.",
											(int32)CurrentSystemVersion.majorVersion, (int32)CurrentSystemVersion.minorVersion, (int32)CurrentSystemVersion.patchVersion, MinimumSystemVersionString]];
			[AlertPanel setMessageText:@"You cannot use this application with this version of macOS"];
			[AlertPanel addButtonWithTitle:@"OK"];
			[AlertPanel runModal];
			[AlertPanel release];

			CFRelease(SessionDictionary);
		}

		fprintf(stderr, "You cannot use this application with this version of macOS. You have macOS %d.%d.%d. The application requires macOS %s or later.\n",
				(int32)CurrentSystemVersion.majorVersion, (int32)CurrentSystemVersion.minorVersion, (int32)CurrentSystemVersion.patchVersion, [MinimumSystemVersionString UTF8String]);

		_Exit(1);
	}

	//install the custom quit event handler
    NSAppleEventManager* appleEventManager = [NSAppleEventManager sharedAppleEventManager];
    [appleEventManager setEventHandler:self andSelector:@selector(handleQuitEvent:withReplyEvent:) forEventClass:kCoreEventClass andEventID:kAEQuitApplication];
	
	FPlatformMisc::SetGracefulTerminationHandler();
#if !(UE_BUILD_SHIPPING && WITH_EDITOR) && WITH_EDITORONLY_DATA
	if ( FParse::Param( *GSavedCommandLine,TEXT("crashreports") ) )
	{
		GAlwaysReportCrash = true;
	}
#endif
	
#if WITH_EDITOR
	bHasFinishedLaunching = true;
	
	if(Filename != nil && !GSavedCommandLine.Contains(FString(Filename)))
	{
		GSavedCommandLine += TEXT(" ");
		FString Argument(Filename);
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
#endif
	
	RunGameThread(self, @selector(runGameThread:));
}

@end

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	for (int32 Option = 1; Option < ArgC; Option++)
	{
		GSavedCommandLine += TEXT(" ");
		FString Argument(ArgV[Option]);
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

#if UE_GAME
	// On Mac we always want games to save files to user dir instead of inside the app bundle
	GSavedCommandLine += TEXT(" -installed");
#endif

	SCOPED_AUTORELEASE_POOL;
	[NSApplication sharedApplication];
	[NSApp setDelegate:[UE4AppDelegate new]];
	[NSApp run];
	return GGuardedMainErrorLevel;
}
