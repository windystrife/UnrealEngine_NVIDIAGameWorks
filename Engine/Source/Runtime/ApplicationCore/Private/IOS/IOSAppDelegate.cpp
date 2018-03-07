// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSAppDelegate.h"
#include "IOSCommandLineHelper.h"
#include "ExceptionHandling.h"
#include "ModuleBoilerplate.h"
#include "CallbackDevice.h"
#include "IOSView.h"
#include "TaskGraphInterfaces.h"
#include "GenericPlatformChunkInstall.h"
#include "IOSPlatformMisc.h"
#include "HAL/PlatformStackWalk.h"
#include "HAL/PlatformProcess.h"
#include "Misc/OutputDeviceError.h"
#include "Misc/CommandLine.h"
#include "IOS/IOSPlatformFramePacer.h"
#include "IOS/IOSAsyncTask.h"
#include "Misc/ConfigCacheIni.h"
#include "IOSPlatformCrashContext.h"
#include "Misc/OutputDeviceError.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/FeedbackContext.h"

#include <AudioToolbox/AudioToolbox.h>
#include <AVFoundation/AVAudioSession.h>

// this is the size of the game thread stack, it must be a multiple of 4k
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#define GAME_THREAD_STACK_SIZE 1024 * 1024
#else
#define GAME_THREAD_STACK_SIZE 16 * 1024 * 1024
#endif

DEFINE_LOG_CATEGORY(LogIOSAudioSession);

extern bool GShowSplashScreen;

FIOSCoreDelegates::FOnOpenURL FIOSCoreDelegates::OnOpenURL;

/*
	From: https://developer.apple.com/library/ios/documentation/UIKit/Reference/UIApplicationDelegate_Protocol/#//apple_ref/occ/intfm/UIApplicationDelegate/applicationDidEnterBackground:
	"In practice, you should return from applicationDidEnterBackground: as quickly as possible. If the method does not return before time runs out your app is terminated and purged from memory."
*/
const double cMaxThreadWaitTime = 2.0;	// Setting this to be 2 seconds since this wait has to be done twice (once for sending the enter background event to the game thread, and another for waiting on the suspend msg
										// I could not find a reference for this but in the past I believe the timeout was 5 seconds

static void SignalHandler(int32 Signal, struct __siginfo* Info, void* Context)
{
	static int32 bHasEntered = 0;
	if (FPlatformAtomics::InterlockedCompareExchange(&bHasEntered, 1, 0) == 0)
	{
		const SIZE_T StackTraceSize = 65535;
		ANSICHAR* StackTrace = (ANSICHAR*)FMemory::Malloc(StackTraceSize);
		StackTrace[0] = 0;
		
		// Walk the stack and dump it to the allocated memory.
		FPlatformStackWalk::StackWalkAndDump(StackTrace, StackTraceSize, 0, (ucontext_t*)Context);
		UE_LOG(LogIOS, Error, TEXT("%s"), ANSI_TO_TCHAR(StackTrace));
		FMemory::Free(StackTrace);
		
		GError->HandleError();
		FPlatformMisc::RequestExit(true);
	}
}

void InstallSignalHandlers()
{
	struct sigaction Action;
	FMemory::Memzero(&Action, sizeof(struct sigaction));
	
	Action.sa_sigaction = SignalHandler;
	sigemptyset(&Action.sa_mask);
	Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
	
	sigaction(SIGQUIT, &Action, NULL);
	sigaction(SIGILL, &Action, NULL);
	sigaction(SIGEMT, &Action, NULL);
	sigaction(SIGFPE, &Action, NULL);
	sigaction(SIGBUS, &Action, NULL);
	sigaction(SIGSEGV, &Action, NULL);
	sigaction(SIGSYS, &Action, NULL);
}

void EngineCrashHandler(const FGenericCrashContext& GenericContext)
{
    const FIOSCrashContext& Context = static_cast<const FIOSCrashContext&>(GenericContext);
    
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
    return Context.GenerateCrashInfo();
}

@implementation IOSAppDelegate

#if !UE_BUILD_SHIPPING && !PLATFORM_TVOS
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_9_0
	@synthesize ConsoleAlert;
#endif
#ifdef __IPHONE_8_0
    @synthesize ConsoleAlertController;
#endif
	@synthesize ConsoleHistoryValues;
	@synthesize ConsoleHistoryValuesIndex;
#endif

@synthesize AlertResponse;
@synthesize bDeviceInPortraitMode;
@synthesize bEngineInit;
@synthesize OSVersion;

@synthesize Window;
@synthesize IOSView;
@synthesize IOSController;
@synthesize SlateController;
@synthesize timer;
@synthesize IdleTimerEnableTimer;
@synthesize IdleTimerEnablePeriod;

-(void)dealloc
{
#if !UE_BUILD_SHIPPING && !PLATFORM_TVOS
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_9_0
	[ConsoleAlert release];
#endif
#ifdef __IPHONE_8_0
	[ConsoleAlertController release];
#endif
	[ConsoleHistoryValues release];
#endif
	[Window release];
	[IOSView release];
	[IOSController release];
	[SlateController release];
	[timer release];
	[super dealloc];
}

-(void) ParseCommandLineOverrides
{
	//// Check for device type override
	//FString IOSDeviceName;
	//if ( Parse( appCmdLine(), TEXT("-IOSDevice="), IOSDeviceName) )
	//{
	//	for ( int32 DeviceTypeIndex = 0; DeviceTypeIndex < IOS_Unknown; ++DeviceTypeIndex )
	//	{
	//		if ( IOSDeviceName == IPhoneGetDeviceTypeString( (EIOSDevice) DeviceTypeIndex) )
	//		{
	//			GOverrideIOSDevice = (EIOSDevice) DeviceTypeIndex;
	//		}
	//	}
	//}

	//// Check for iOS version override
	//FLOAT IOSVersion = 0.0f;
	//if ( Parse( appCmdLine(), TEXT("-IOSVersion="), IOSVersion) )
	//{
	//	GOverrideIOSVersion = IOSVersion;
	//}

	// check to see if we are using the network file system, if so, disable the idle timer
	FString HostIP;
	if (FParse::Value(FCommandLine::Get(), TEXT("-FileHostIP="), HostIP))
	{
		[UIApplication sharedApplication].idleTimerDisabled = YES;
	}
}

-(void)MainAppThread:(NSDictionary*)launchOptions
{
    self.bHasStarted = true;
	GIsGuarded = false;
	GStartTime = FPlatformTime::Seconds();

	// make sure this thread has an auto release pool setup 
	NSAutoreleasePool* AutoreleasePool = [[NSAutoreleasePool alloc] init];

	while(!self.bCommandLineReady)
	{
		usleep(100);
	}



	// Look for overrides specified on the command-line
	[self ParseCommandLineOverrides];

	FAppEntry::Init();
	
	[self InitIdleTimerSettings];

	bEngineInit = true;
	// @PJS - need a better way to allow the game to turn off the splash screen
    GShowSplashScreen = false;

	while( !GIsRequestingExit )
	{
        if (self.bIsSuspended)
        {
            FAppEntry::SuspendTick();
            
            self.bHasSuspended = true;
        }
        else
        {
            FAppEntry::Tick();
        
            // free any autoreleased objects every once in awhile to keep memory use down (strings, splash screens, etc)
            if (((GFrameCounter) & 31) == 0)
            {
                [AutoreleasePool release];
                AutoreleasePool = [[NSAutoreleasePool alloc] init];
            }
        }

        // drain the async task queue from the game thread
        [FIOSAsyncTask ProcessAsyncTasks];
	}

	[UIApplication sharedApplication].idleTimerDisabled = NO;

	[AutoreleasePool release];
	FAppEntry::Shutdown();
    
    self.bHasStarted = false;
}

-(void)timerForSplashScreen
{
    if (!GShowSplashScreen)
    {
        if ([self.Window viewWithTag:2] != nil)
        {
            [[self.Window viewWithTag:2] removeFromSuperview];
        }
        [timer invalidate];
    }
}

-(void)RecordPeakMemory
{
    FIOSPlatformMemory::GetStats();
}

-(void)InitIdleTimerSettings
{
	float TimerDuration = 0.0F;
	GConfig->GetFloat(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("IdleTimerEnablePeriod"), TimerDuration, GEngineIni);
	IdleTimerEnablePeriod = TimerDuration;
	
	self.IdleTimerEnableTimer = nil;
}

-(void)DeferredEnableIdleTimer
{
	[UIApplication sharedApplication].idleTimerDisabled = NO;
	self.IdleTimerEnableTimer = nil;
}

-(void)EnableIdleTimer:(bool)bEnabled
{
	dispatch_async(dispatch_get_main_queue(),^
	{
		if (bEnabled)
		{
			// Nothing needs to be done, if the enable timer is already running.
			if (self.IdleTimerEnableTimer == nil)
			{
				self.IdleTimerEnableTimer = [NSTimer scheduledTimerWithTimeInterval:IdleTimerEnablePeriod target:self selector:@selector(DeferredEnableIdleTimer) userInfo:nil repeats:NO];
			}
		}
		else
		{
			// Ensure pending attempts to enable the idle timer are cancelled.
			if (self.IdleTimerEnableTimer != nil)
			{
				[self.IdleTimerEnableTimer invalidate];
				self.IdleTimerEnableTimer = nil;
			}

			[UIApplication sharedApplication].idleTimerDisabled = NO;
			[UIApplication sharedApplication].idleTimerDisabled = YES;
		}
	});
}

-(void)NoUrlCommandLine
{
	//Since it is non-repeating, the timer should kill itself.
	self.bCommandLineReady = true;
}

-(void)AudioInterrupted:(NSNotification*)notification
{
	// pull the type of notification out
	NSDictionary* Info = [notification userInfo];
	NSNumber* Type = (NSNumber*)[Info valueForKey:AVAudioSessionInterruptionTypeKey];

	NSLog(@"AUDIO INTERRUPTION NOTIFICATION: %d, (began = %d, ended = %d)", (int)[Type integerValue], (int)AVAudioSessionInterruptionTypeBegan, (int)AVAudioSessionInterruptionTypeEnded);
	if ([Type integerValue] == AVAudioSessionInterruptionTypeBegan)
	{
		FAppEntry::Suspend();
		[self ToggleAudioSession:false];
	}
	else if ([Type integerValue] == AVAudioSessionInterruptionTypeEnded)
	{
		[self ToggleAudioSession:true];
	    FAppEntry::Resume();
	}
}

- (void)InitializeAudioSession
{
	// get notified about interruptions
	// @todo ios8: Test this (I can't make a notification happen so far)
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(AudioInterrupted:) name:AVAudioSessionInterruptionNotification object:[AVAudioSession sharedInstance]];
	
	self.bUsingBackgroundMusic = [self IsBackgroundAudioPlaying];
	if (!self.bUsingBackgroundMusic)
	{
		NSError* ActiveError = nil;
		[[AVAudioSession sharedInstance] setActive:YES error:&ActiveError];
		if (ActiveError)
		{
			UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session as active!"));
		}
		ActiveError = nil;
		
		[[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategorySoloAmbient error:&ActiveError];
		if (ActiveError)
		{
			UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session category to AVAudioSessionCategorySoloAmbient!"));
		}
		ActiveError = nil;
	}
	else
	{
		// Allow iPod music to continue playing in the background
		NSError* ActiveError = nil;
		[[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryAmbient error:&ActiveError];
		if (ActiveError)
		{
			UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session category to AVAudioSessionCategoryAmbient!"));
		}
		ActiveError = nil;
	}

    self.bAudioActive = true;
}

- (void)ToggleAudioSession:(bool)bActive
{
	if (bActive)
	{
        if (!self.bAudioActive)
        {
			bool bWasUsingBackgroundMusic = self.bUsingBackgroundMusic;
			self.bUsingBackgroundMusic = [self IsBackgroundAudioPlaying];
	
			if (bWasUsingBackgroundMusic != self.bUsingBackgroundMusic)
			{
				if (!self.bUsingBackgroundMusic)
				{
					NSError* ActiveError = nil;
					[[AVAudioSession sharedInstance] setActive:YES error:&ActiveError];
					if (ActiveError)
					{
						UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session as active! [Error = %s]"), *FString([ActiveError description]));
					}
					ActiveError = nil;
	
					[[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategorySoloAmbient error:&ActiveError];
					if (ActiveError)
					{
						UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session category to AVAudioSessionCategorySoloAmbient! [Error = %s]"), *FString([ActiveError description]));
					}
					ActiveError = nil;
	
					/* TODO::JTM - Jan 16, 2013 05:05PM - Music player support */
				}
				else
				{
					/* TODO::JTM - Jan 16, 2013 05:05PM - Music player support */
	
					// Allow iPod music to continue playing in the background
					NSError* ActiveError = nil;
					[[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryAmbient error:&ActiveError];
					if (ActiveError)
					{
						UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session category to AVAudioSessionCategoryAmbient! [Error = %s]"), *FString([ActiveError description]));
					}
					ActiveError = nil;
				}
			}
			else if (!self.bUsingBackgroundMusic)
			{
				NSError* ActiveError = nil;
				[[AVAudioSession sharedInstance] setActive:YES error:&ActiveError];
				if (ActiveError)
				{
					UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session as active! [Error = %s]"), *FString([ActiveError description]));
				}
				ActiveError = nil;
				
				[[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategorySoloAmbient error:&ActiveError];
				if (ActiveError)
				{
					UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session category to AVAudioSessionCategorySoloAmbient! [Error = %s]"), *FString([ActiveError description]));
				}
				ActiveError = nil;
			}
        }
	}
	else if (self.bAudioActive && !self.bUsingBackgroundMusic)
	{
		NSError* ActiveError = nil;
		[[AVAudioSession sharedInstance] setActive:NO error:&ActiveError];
		if (ActiveError)
		{
			UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session as inactive! [Error = %s]"), *FString([ActiveError description]));
		}
		ActiveError = nil;
        
		// Necessary to prevent audio from getting killing when setup for background iPod audio playback
		[[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryAmbient error:&ActiveError];
		if (ActiveError)
		{
			UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session category to AVAudioSessionCategoryAmbient! [Error = %s]"), *FString([ActiveError description]));
		}
		ActiveError = nil;
 	}
    self.bAudioActive = bActive;
}

- (bool)IsBackgroundAudioPlaying
{
	AVAudioSession* Session = [AVAudioSession sharedInstance];
	return Session.otherAudioPlaying;
}

- (int)GetAudioVolume
{
	float vol = [[AVAudioSession sharedInstance] outputVolume];
	int roundedVol = (int)((vol * 100.0f) + 0.5f);
	return roundedVol;
}

- (bool)AreHeadphonesPluggedIn
{
	AVAudioSessionRouteDescription *route = [[AVAudioSession sharedInstance] currentRoute];

	bool headphonesFound = false;
	for (AVAudioSessionPortDescription *portDescription in route.outputs)
	{
		//compare to the iOS constant for headphones
		if ([portDescription.portType isEqualToString : AVAudioSessionPortHeadphones])
		{
			headphonesFound = true;
			break;
		}
	}
	return headphonesFound;
}

- (int)GetBatteryLevel
{
#if PLATFORM_TVOS
	// TVOS does not have a battery, return fully charged
	return 100;
#else
	UIDevice* Device = [UIDevice currentDevice];
	Device.batteryMonitoringEnabled = YES;
	
	// Battery level is from 0.0 to 1.0, get it in terms of 0-100
	int Level = ((int)([Device batteryLevel] * 100));
	
	Device.batteryMonitoringEnabled = NO;
	return Level;
#endif
}

- (bool)IsRunningOnBattery
{
#if PLATFORM_TVOS
	// TVOS does not have a battery, return plugged in
	return false;
#else
	UIDevice* Device = [UIDevice currentDevice];
	Device.batteryMonitoringEnabled = YES;

	UIDeviceBatteryState State = Device.batteryState;

	Device.batteryMonitoringEnabled = NO;
	return State == UIDeviceBatteryStateUnplugged || State == UIDeviceBatteryStateUnknown;
#endif
}

/**
 * @return the single app delegate object
 */
+ (IOSAppDelegate*)GetDelegate
{
	return (IOSAppDelegate*)[UIApplication sharedApplication].delegate;
}

- (void)ToggleSuspend:(bool)bSuspend
{
    self.bHasSuspended = !bSuspend;
    self.bIsSuspended = bSuspend;

	if (bSuspend)
	{
		FAppEntry::Suspend();
	}
	else
	{
        FIOSPlatformRHIFramePacer::Resume();
		FAppEntry::Resume();
	}
    
	if (IOSView && IOSView->bIsInitialized)
	{
		// Don't deadlock here because a msg box may appear super early blocking the game thread and then the app may go into the background
		double	startTime = FPlatformTime::Seconds();
		while(!self.bHasSuspended && (FPlatformTime::Seconds() - startTime) < cMaxThreadWaitTime)
		{
            FIOSPlatformRHIFramePacer::Suspend();
			FPlatformProcess::Sleep(0.05f);
		}
	}
}

- (BOOL)application:(UIApplication *)application willFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
	self.bDeviceInPortraitMode = false;
	bEngineInit = false;

	return YES;
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
	// save launch options
	self.launchOptions = launchOptions;

#if PLATFORM_TVOS
	self.bDeviceInPortraitMode = false;
#else
	// use the status bar orientation to properly determine landscape vs portrait
	self.bDeviceInPortraitMode = UIInterfaceOrientationIsPortrait([[UIApplication sharedApplication] statusBarOrientation]);
	printf("========= This app is in %s mode\n", self.bDeviceInPortraitMode ? "PORTRAIT" : "LANDSCAPE");
#endif

		// check OS version to make sure we have the API
	OSVersion = [[[UIDevice currentDevice] systemVersion] floatValue];
	if (!FPlatformMisc::IsDebuggerPresent() || GAlwaysReportCrash)
	{
//        FPlatformMisc::SetCrashHandler(EngineCrashHandler);
        InstallSignalHandlers();
	}

	// create the main landscape window object
	CGRect MainFrame = [[UIScreen mainScreen] bounds];
	self.Window = [[UIWindow alloc] initWithFrame:MainFrame];
	self.Window.screen = [UIScreen mainScreen];
    
    // get the native scale
    const float NativeScale = [[UIScreen mainScreen] scale];
    
	//Make this the primary window, and show it.
	[self.Window makeKeyAndVisible];

	FAppEntry::PreInit(self, application);
    
    // add the default image as a subview
    NSMutableString* path = [[NSMutableString alloc]init];
    [path setString: [[NSBundle mainBundle] resourcePath]];
    UIImageOrientation orient = UIImageOrientationUp;
    NSMutableString* ImageString = [[NSMutableString alloc]init];
	NSMutableString* PngString = [[NSMutableString alloc] init];
    [ImageString appendString:@"Default"];

	FPlatformMisc::EIOSDevice Device = FPlatformMisc::GetIOSDeviceType();

	// iphone6 has specially named files, this seems to be needed for every iphone since, so let's see if we can find a better way to do this which isn't device specific
    if (Device == FPlatformMisc::IOS_IPhone6 || Device == FPlatformMisc::IOS_IPhone6S || Device == FPlatformMisc::IOS_IPhone7 || Device == FPlatformMisc::IOS_IPhone8)
	{
		[ImageString appendString:@"-IPhone6"];
		if (!self.bDeviceInPortraitMode)
		{
			[ImageString appendString : @"-Landscape"];
		}
	}
    else if (Device == FPlatformMisc::IOS_IPhone6Plus || Device == FPlatformMisc::IOS_IPhone6SPlus || Device == FPlatformMisc::IOS_IPhone7Plus || Device == FPlatformMisc::IOS_IPhone8Plus)
	{
		[ImageString appendString : @"-IPhone6Plus"];
		if (!self.bDeviceInPortraitMode)
		{
			[ImageString appendString : @"-Landscape"];
		}
		else
		{
			[ImageString appendString : @"-Portrait"];
		}
	}
	else if (Device == FPlatformMisc::IOS_IPhoneX)
	{
		[ImageString appendString : @"-IPhoneX"];
		if (!self.bDeviceInPortraitMode)
		{
			[ImageString appendString : @"-Landscape"];
		}
		else
		{
			[ImageString appendString : @"-Portrait"];
		}
	}
	else if (Device == FPlatformMisc::IOS_AppleTV)
	{
		// @todo tvos: Make an AppleTV one?
		// use IPhone6 image for now
		[ImageString appendString : @"-IPhone6Plus-Landscape"];
	}
	else if (Device == FPlatformMisc::IOS_IPadPro_129)
	{
		if (!self.bDeviceInPortraitMode)
		{
			[ImageString appendString : @"-Landscape-1336"];
		}
		else
		{
			[ImageString appendString : @"-Portrait-1336"];
		}
        
        if (NativeScale > 1.0f)
        {
            [ImageString appendString:@"@2x"];
        }
	}
	else
	{
		if (MainFrame.size.height == 320 && MainFrame.size.width != 480 && !self.bDeviceInPortraitMode)
		{
			[ImageString appendString:@"-568h"];
			orient = UIImageOrientationRight;
		}
		else if (MainFrame.size.height == 320 && MainFrame.size.width == 480 && !self.bDeviceInPortraitMode)
		{
			orient = UIImageOrientationRight;
		}
        else if (MainFrame.size.height == 568 || Device == FPlatformMisc::IOS_IPodTouch6)
		{
			[ImageString appendString:@"-568h"];
		}
		else if (MainFrame.size.height == 1024 && !self.bDeviceInPortraitMode)
		{
			[ImageString appendString:@"-Landscape"];
			orient = UIImageOrientationRight;
		}
		else if (MainFrame.size.height == 1024)
		{
			[ImageString appendString:@"-Portrait"];
		}
		else if (MainFrame.size.height == 768 && !self.bDeviceInPortraitMode)
		{
			[ImageString appendString:@"-Landscape"];
		}
        
        if (NativeScale > 1.0f)
        {
            [ImageString appendString:@"@2x"];
        }
	}

	[PngString appendString : ImageString];
	[PngString appendString : @".png"];
	[ImageString appendString : @".jpg"];
    [path setString: [path stringByAppendingPathComponent:ImageString]];
    UIImage* image = [[UIImage alloc] initWithContentsOfFile: path];
	if (image == nil)
	{
        [path setString: [[NSBundle mainBundle] resourcePath]];
		[path setString : [path stringByAppendingPathComponent : PngString]];
		image = [[UIImage alloc] initWithContentsOfFile:path];
	}
	[path release];
	
    UIImage* imageToDisplay = [UIImage imageWithCGImage: [image CGImage] scale: 1.0 orientation: orient];
    UIImageView* imageView = [[UIImageView alloc] initWithImage: imageToDisplay];
    imageView.frame = MainFrame;
    imageView.tag = 2;
    [self.Window addSubview: imageView];
    GShowSplashScreen = true;
	
#if !PLATFORM_TVOS
	// Save launch local notification so the app can check for it when it is ready
	UILocalNotification *notification = [launchOptions objectForKey:UIApplicationLaunchOptionsLocalNotificationKey];
	if ( notification != nullptr )
	{
		NSDictionary*	userInfo = [notification userInfo];
		if(userInfo != nullptr)
		{
			NSString*	activationEvent = (NSString*)[notification.userInfo objectForKey: @"ActivationEvent"];
			
			if(activationEvent != nullptr)
			{
				FAppEntry::gAppLaunchedWithLocalNotification = true;
				FAppEntry::gLaunchLocalNotificationActivationEvent = FString(activationEvent);
				FAppEntry::gLaunchLocalNotificationFireDate = [notification.fireDate timeIntervalSince1970];
			}
		}
	}
#endif
	
    timer = [NSTimer scheduledTimerWithTimeInterval: 0.05f target:self selector:@selector(timerForSplashScreen) userInfo:nil repeats:YES];
    
    self.PeakMemoryTimer = [NSTimer scheduledTimerWithTimeInterval:0.1f target:self selector:@selector(RecordPeakMemory) userInfo:nil repeats:YES];
    
	// create a new thread (the pointer will be retained forever)
	NSThread* GameThread = [[NSThread alloc] initWithTarget:self selector:@selector(MainAppThread:) object:launchOptions];
	[GameThread setStackSize:GAME_THREAD_STACK_SIZE];
	[GameThread start];

	self.CommandLineParseTimer = [NSTimer scheduledTimerWithTimeInterval:0.01f target:self selector:@selector(NoUrlCommandLine) userInfo:nil repeats:NO];
#if !UE_BUILD_SHIPPING && !PLATFORM_TVOS
	// make a history buffer
	self.ConsoleHistoryValues = [[NSMutableArray alloc] init];

	// load saved history from disk
	NSArray* SavedHistory = [[NSUserDefaults standardUserDefaults] objectForKey:@"ConsoleHistory"];
	if (SavedHistory != nil)
	{
		[self.ConsoleHistoryValues addObjectsFromArray:SavedHistory];
	}
	self.ConsoleHistoryValuesIndex = -1;
#endif

	[self InitializeAudioSession];
	
#if !PLATFORM_TVOS
	// Register for device orientation changes
	[[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didRotate:) name:UIDeviceOrientationDidChangeNotification object:nil];
#endif
	
	return YES;
}

- (void) didRotate:(NSNotification *)notification
{   
#if !PLATFORM_TVOS
	UIDeviceOrientation orientation = [[UIDevice currentDevice] orientation];

    if (bEngineInit)
    {
		FFunctionGraphTask::CreateAndDispatchWhenReady([orientation]()
		{
			FCoreDelegates::ApplicationReceivedScreenOrientationChangedNotificationDelegate.Broadcast((int32)orientation);
		}, TStatId(), NULL, ENamedThreads::GameThread);
	}
#endif
}

- (BOOL)application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation
{
#if !NO_LOGGING
	NSLog(@"%s", "IOSAppDelegate openURL\n");
#endif

	NSString* EncdodedURLString = [url absoluteString];
	NSString* URLString = [EncdodedURLString stringByRemovingPercentEncoding];
	FString CommandLineParameters(URLString);

	// Strip the "URL" part of the URL before treating this like args. It comes in looking like so:
	// "MyGame://arg1 arg2 arg3 ..."
	// So, we're going to make it look like:
	// "arg1 arg2 arg3 ..."
	int32 URLTerminator = CommandLineParameters.Find( TEXT("://"));
	if ( URLTerminator > -1 )
	{
		CommandLineParameters = CommandLineParameters.RightChop(URLTerminator + 3);
	}

	FIOSCommandLineHelper::InitCommandArgs(CommandLineParameters);
	self.bCommandLineReady = true;
	[self.CommandLineParseTimer invalidate];
	self.CommandLineParseTimer = nil;
	
	FIOSCoreDelegates::OnOpenURL.Broadcast(application, url, sourceApplication, annotation);
	
	return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application
{
	/*
	 Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
	 Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
	 */
    if (bEngineInit)
    {
        FGraphEventRef ResignTask = FFunctionGraphTask::CreateAndDispatchWhenReady([]()
        {
            FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
        }, TStatId(), NULL, ENamedThreads::GameThread);
		
		// Do not wait forever for this task to complete since the game thread may be stuck on waiting for user input from a modal dialog box
		double	startTime = FPlatformTime::Seconds();
		while((FPlatformTime::Seconds() - startTime) < cMaxThreadWaitTime)
		{
			FPlatformProcess::Sleep(0.05f);
			if(ResignTask->IsComplete())
			{
				break;
			}
		}
    }
    
	[self ToggleSuspend:true];
	[self ToggleAudioSession:false];
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
	/*
	 Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later. 
	 If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
	 */
    FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
	/*
	 Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
	 */
    FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
	/*
	 Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
	 */
    [self ToggleSuspend:false];
	[self ToggleAudioSession:true];

    if (bEngineInit)
    {
        FGraphEventRef ResignTask = FFunctionGraphTask::CreateAndDispatchWhenReady([]()
        {
            FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
        }, TStatId(), NULL, ENamedThreads::GameThread);

		// Do not wait forever for this task to complete since the game thread may be stuck on waiting for user input from a modal dialog box
		double	startTime = FPlatformTime::Seconds();
 		while((FPlatformTime::Seconds() - startTime) < cMaxThreadWaitTime)
		{
			FPlatformProcess::Sleep(0.05f);
			if(ResignTask->IsComplete())
			{
				break;
			}
		}
    }
}

- (void)applicationWillTerminate:(UIApplication *)application
{
	/*
	 Called when the application is about to terminate.
	 Save data if appropriate.
	 See also applicationDidEnterBackground:.
	 */
	FCoreDelegates::ApplicationWillTerminateDelegate.Broadcast();
    
    // note that we are shutting down
    GIsRequestingExit = true;
    
    // wait for the game thread to shut down
    while (self.bHasStarted == true)
    {
        usleep(3);
    }
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication *)application
{
	/*
	Tells the delegate when the application receives a memory warning from the system
	*/
	FPlatformMisc::HandleLowMemoryWarning();
}

#if !PLATFORM_TVOS && NOTIFICATIONS_ENABLED

#ifdef __IPHONE_8_0
- (void)application:(UIApplication *)application didRegisterUserNotificationSettings:(UIUserNotificationSettings *)notificationSettings
{
	[application registerForRemoteNotifications];
	int32 types = (int32)[notificationSettings types];
    FFunctionGraphTask::CreateAndDispatchWhenReady([types]()
    {
		FCoreDelegates::ApplicationRegisteredForUserNotificationsDelegate.Broadcast(types);
    }, TStatId(), NULL, ENamedThreads::GameThread);
}
#endif

- (void)application:(UIApplication *)application didRegisterForRemoteNotificationsWithDeviceToken:(NSData *)deviceToken
{
	TArray<uint8> Token;
	Token.AddUninitialized([deviceToken length]);
	memcpy(Token.GetData(), [deviceToken bytes], [deviceToken length]);

	const char *data = (const char*)([deviceToken bytes]);
	NSMutableString *token = [NSMutableString string];

	for (NSUInteger i = 0; i < [deviceToken length]; i++) {
		[token appendFormat : @"%02.2hhX", data[i]];
	}

	UE_LOG(LogTemp, Display, TEXT("Device Token: %s"), *FString(token));

    FFunctionGraphTask::CreateAndDispatchWhenReady([Token]()
    {
		FCoreDelegates::ApplicationRegisteredForRemoteNotificationsDelegate.Broadcast(Token);
    }, TStatId(), NULL, ENamedThreads::GameThread);
}

-(void)application:(UIApplication *)application didFailtoRegisterForRemoteNotificationsWithError:(NSError *)error
{
	FString errorDescription([error description]);
	
    FFunctionGraphTask::CreateAndDispatchWhenReady([errorDescription]()
    {
		FCoreDelegates::ApplicationFailedToRegisterForRemoteNotificationsDelegate.Broadcast(errorDescription);
    }, TStatId(), NULL, ENamedThreads::GameThread);
}

-(void)application : (UIApplication *)application didReceiveRemoteNotification:(NSDictionary *)userInfo fetchCompletionHandler:(void(^)(UIBackgroundFetchResult result))handler
{
	if (bEngineInit)
	{
		NSString* JsonString = @"{}";
		NSError* JsonError;
		NSData* JsonData = [NSJSONSerialization dataWithJSONObject : userInfo
						options : 0
						error : &JsonError];

		if (JsonData)
		{
			JsonString = [[[NSString alloc] initWithData:JsonData encoding : NSUTF8StringEncoding] autorelease];
		}
		
		FString	jsonFString(JsonString);
		int AppState;
		if (application.applicationState == UIApplicationStateInactive)
		{
			AppState = 1; // EApplicationState::Inactive;
		}
		else if (application.applicationState == UIApplicationStateBackground)
		{
			AppState = 2; // EApplicationState::Background;
		}
		else
		{
			AppState = 3; // EApplicationState::Active;
		}

		FFunctionGraphTask::CreateAndDispatchWhenReady([jsonFString, AppState]()
		{
			FCoreDelegates::ApplicationReceivedRemoteNotificationDelegate.Broadcast(jsonFString, AppState);
		}, TStatId(), NULL, ENamedThreads::GameThread);
	}
	
	// According to Apple documentation:
	//   As soon as you finish processing the notification, you must call the
	//   block in the handler parameter or your app will be terminated. Your app
	//   has up to 30 seconds of wall-clock time to process the notification and
	//   call the specified completion handler block. In practice, you should
	//   call the handler block as soon as you are done processing the
	//   notification.
	handler(UIBackgroundFetchResultNoData);
}

#endif

#if !PLATFORM_TVOS

- (void)application:(UIApplication *)application didReceiveLocalNotification:(UILocalNotification *)notification
{
	NSString*	activationEvent = (NSString*)[notification.userInfo objectForKey: @"ActivationEvent"];
	
	if(activationEvent != nullptr)
	{
		FString	activationEventFString(activationEvent);
		int32	fireDate = [notification.fireDate timeIntervalSince1970];
		
		int AppState;
		if (application.applicationState == UIApplicationStateInactive)
		{
			AppState = 1;// EApplicationState::Inactive;
		}
		else if (application.applicationState == UIApplicationStateBackground)
		{
			AppState = 2; // EApplicationState::Background;
		}
		else
		{
			AppState = 3; // EApplicationState::Active;
		}

		FFunctionGraphTask::CreateAndDispatchWhenReady([activationEventFString, fireDate, AppState]()
		{
			FCoreDelegates::ApplicationReceivedLocalNotificationDelegate.Broadcast(activationEventFString, fireDate, AppState);
		}, TStatId(), NULL, ENamedThreads::GameThread);
	}
	else
	{
		NSLog(@"Warning: Missing local notification activation event");
	}
}

#endif

/**
 * Shows the given Game Center supplied controller on the screen
 *
 * @param Controller The Controller object to animate onto the screen
 */
-(void)ShowController:(UIViewController*)Controller
{
	// slide it onto the screen
	[[IOSAppDelegate GetDelegate].IOSController presentViewController : Controller animated : YES completion : nil];
	
	// stop drawing the 3D world for faster UI speed
	//FViewport::SetGameRenderingEnabled(false);
}

/**
 * Hides the given Game Center supplied controller from the screen, optionally controller
 * animation (sliding off)
 *
 * @param Controller The Controller object to animate off the screen
 * @param bShouldAnimate YES to slide down, NO to hide immediately
 */
-(void)HideController:(UIViewController*)Controller Animated:(BOOL)bShouldAnimate
{
    // slide it off
    [Controller dismissViewControllerAnimated : bShouldAnimate completion : nil];

	// stop drawing the 3D world for faster UI speed
	//FViewport::SetGameRenderingEnabled(true);
}

/**
 * Hides the given Game Center supplied controller from the screen
 *
 * @param Controller The Controller object to animate off the screen
 */
-(void)HideController:(UIViewController*)Controller
{
	// call the other version with default animation of YES
	[self HideController : Controller Animated : YES];
}

-(void)gameCenterViewControllerDidFinish:(GKGameCenterViewController*)GameCenterDisplay
{
	// close the view 
	[self HideController : GameCenterDisplay];
}

/**
 * Show the leaderboard interface (call from iOS main thread)
 */
-(void)ShowLeaderboard:(NSString*)Category
{
	// create the leaderboard display object 
	GKGameCenterViewController* GameCenterDisplay = [[[GKGameCenterViewController alloc] init] autorelease];
#if !PLATFORM_TVOS
	GameCenterDisplay.viewState = GKGameCenterViewControllerStateLeaderboards;
#endif
#ifdef __IPHONE_7_0
    if ([GameCenterDisplay respondsToSelector:@selector(leaderboardIdentifier)] == YES)
    {
#if !PLATFORM_TVOS // @todo tvos: Why not??
        GameCenterDisplay.leaderboardIdentifier = Category;
#endif
    }
    else
#endif
    {
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_7_0
        GameCenterDisplay.leaderboardCategory = Category;
#endif
    }
	GameCenterDisplay.gameCenterDelegate = self;

	// show it 
	[self ShowController : GameCenterDisplay];
}

/**
 * Show the achievements interface (call from iOS main thread)
 */
-(void)ShowAchievements
{
	// create the leaderboard display object 
	GKGameCenterViewController* GameCenterDisplay = [[[GKGameCenterViewController alloc] init] autorelease];
#if !PLATFORM_TVOS
	GameCenterDisplay.viewState = GKGameCenterViewControllerStateAchievements;
#endif
	GameCenterDisplay.gameCenterDelegate = self;

	// show it 
	[self ShowController : GameCenterDisplay];
}

/**
 * Show the leaderboard interface (call from game thread)
 */
CORE_API bool IOSShowLeaderboardUI(const FString& CategoryName)
{
	// route the function to iOS thread, passing the category string along as the object
	NSString* CategoryToShow = [NSString stringWithFString:CategoryName];
	[[IOSAppDelegate GetDelegate] performSelectorOnMainThread:@selector(ShowLeaderboard:) withObject:CategoryToShow waitUntilDone : NO];

	return true;
}

/**
* Show the achievements interface (call from game thread)
*/
CORE_API bool IOSShowAchievementsUI()
{
	
	// route the function to iOS thread
	[[IOSAppDelegate GetDelegate] performSelectorOnMainThread:@selector(ShowAchievements) withObject:nil waitUntilDone : NO];

	return true;
}

-(UIWindow*)window
{
	return Window;
}

@end
