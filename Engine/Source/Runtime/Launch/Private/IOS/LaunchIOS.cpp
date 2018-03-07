// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#import <UIKit/UIKit.h>

#include "CoreMinimal.h"
#include "Misc/App.h"
#include "Misc/OutputDeviceError.h"
#include "LaunchEngineLoop.h"
#include "IMessagingModule.h"
#include "IOSAppDelegate.h"
#include "IOSView.h"
#include "IOSCommandLineHelper.h"
#include "GameLaunchDaemonMessageHandler.h"
#include "AudioDevice.h"
#include "GenericPlatformChunkInstall.h"
#include "IOSAudioDevice.h"
#include "LocalNotification.h"
#include "ModuleManager.h"
#include "RenderingThread.h"
#include "GenericPlatform/GenericApplication.h"
#include "Misc/ConfigCacheIni.h"


FEngineLoop GEngineLoop;
FGameLaunchDaemonMessageHandler GCommandSystem;

void FAppEntry::Suspend()
{
	if (GEngine && GEngine->GetMainAudioDevice())
	{
		GEngine->GetMainAudioDevice()->SuspendContext();
	}
	else
	{
		int32& SuspendCounter = FIOSAudioDevice::GetSuspendCounter();
		if (SuspendCounter == 0)
		{
			FPlatformAtomics::InterlockedIncrement(&SuspendCounter);
		}
	}
}

void FAppEntry::Resume()
{
	if (GEngine && GEngine->GetMainAudioDevice())
	{
		GEngine->GetMainAudioDevice()->ResumeContext();
	}
	else
	{
		int32& SuspendCounter = FIOSAudioDevice::GetSuspendCounter();
		if (SuspendCounter > 0)
		{
			FPlatformAtomics::InterlockedDecrement(&SuspendCounter);
		}
	}
}

void FAppEntry::PreInit(IOSAppDelegate* AppDelegate, UIApplication* Application)
{
	// make a controller object
	AppDelegate.IOSController = [[IOSViewController alloc] init];
	
#if PLATFORM_TVOS
	// @todo tvos: This may need to be exposed to the game so that when you click Menu it will background the app
	// this is basically the same way Android handles the Back button (maybe we should pass Menu button as back... maybe)
	AppDelegate.IOSController.controllerUserInteractionEnabled = NO;
#endif
	
	// property owns it now
	[AppDelegate.IOSController release];

	// point to the GL view we want to use
	AppDelegate.RootView = [AppDelegate.IOSController view];

	if (AppDelegate.OSVersion >= 6.0f)
	{
		// this probably works back to OS4, but would need testing
		[AppDelegate.Window setRootViewController:AppDelegate.IOSController];
	}
	else
	{
		[AppDelegate.Window addSubview:AppDelegate.RootView];
	}

#if !PLATFORM_TVOS
	// reset badge count on launch
	Application.applicationIconBadgeNumber = 0;
#endif
}

static void MainThreadInit()
{
	IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];

	// Size the view appropriately for any potentially dynamically attached displays,
	// prior to creating any framebuffers
	CGRect MainFrame = [[UIScreen mainScreen] bounds];

	// we need to swap if compiled with ios7, or compiled with ios8 and running on 7
#ifndef __IPHONE_8_0
	bool bDoLandscapeSwap = true;
#else
	bool bDoLandscapeSwap = AppDelegate.OSVersion < 8.0f;
#endif

	if (bDoLandscapeSwap && !AppDelegate.bDeviceInPortraitMode)
	{
		Swap(MainFrame.size.width, MainFrame.size.height);
	}
	
	// @todo: use code similar for presizing for secondary screens
// 	CGRect FullResolutionRect =
// 		CGRectMake(
// 		0.0f,
// 		0.0f,
// 		GSystemSettings.bAllowSecondaryDisplays ?
// 		Max<float>(MainFrame.size.width, GSystemSettings.SecondaryDisplayMaximumWidth)	:
// 		MainFrame.size.width,
// 		GSystemSettings.bAllowSecondaryDisplays ?
// 		Max<float>(MainFrame.size.height, GSystemSettings.SecondaryDisplayMaximumHeight) :
// 		MainFrame.size.height
// 		);

	CGRect FullResolutionRect = MainFrame;

	AppDelegate.IOSView = [[FIOSView alloc] initWithFrame:FullResolutionRect];
	AppDelegate.IOSView.clearsContextBeforeDrawing = NO;
#if !PLATFORM_TVOS
	AppDelegate.IOSView.multipleTouchEnabled = YES;
#endif

	// add it to the window
	[AppDelegate.RootView addSubview:AppDelegate.IOSView];

	// initialize the backbuffer of the view (so the RHI can use it)
	[AppDelegate.IOSView CreateFramebuffer:YES];
}



void FAppEntry::PlatformInit()
{

	// call a function in the main thread to do some processing that needs to happen there, now that the .ini files are loaded
	dispatch_async(dispatch_get_main_queue(), ^{ MainThreadInit(); });

	// wait until the GLView is fully initialized, so the RHI can be initialized
	IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];

	while (!AppDelegate.IOSView || !AppDelegate.IOSView->bIsInitialized)
	{
		FPlatformProcess::Sleep(0.001f);
	}

	// set the GL context to this thread
	[AppDelegate.IOSView MakeCurrent];

	// Set GSystemResolution now that we have the size.
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
	FSystemResolution::RequestResolutionChange(DisplayMetrics.PrimaryDisplayWidth, DisplayMetrics.PrimaryDisplayHeight, EWindowMode::Fullscreen);
	IConsoleManager::Get().CallAllConsoleVariableSinks();
}

void FAppEntry::Init()
{
	FPlatformProcess::SetRealTimeMode();

	//extern TCHAR GCmdLine[16384];
	GEngineLoop.PreInit(FCommandLine::Get());

	// initialize messaging subsystem
	FModuleManager::LoadModuleChecked<IMessagingModule>("Messaging");

	//Set up the message handling to interface with other endpoints on our end.
	NSLog(@"%s", "Initializing ULD Communications in game mode\n");
	GCommandSystem.Init();

	GLog->SetCurrentThreadAsMasterThread();
	
	// Send the launch local notification to the local notification service now that the engine module system has been initialized
	if(gAppLaunchedWithLocalNotification)
	{
		ILocalNotificationService* notificationService = NULL;

		// Get the module name from the .ini file
		FString ModuleName;
		GConfig->GetString(TEXT("LocalNotification"), TEXT("DefaultPlatformService"), ModuleName, GEngineIni);

		if (ModuleName.Len() > 0)
		{			
			// load the module by name retrieved from the .ini
			ILocalNotificationModule* module = FModuleManager::LoadModulePtr<ILocalNotificationModule>(*ModuleName);

			// does the module exist?
			if (module != nullptr)
			{
				notificationService = module->GetLocalNotificationService();
				if(notificationService != NULL)
				{
					notificationService->SetLaunchNotification(gLaunchLocalNotificationActivationEvent, gLaunchLocalNotificationFireDate);
				}
			}
		}
	}

	// start up the engine
	GEngineLoop.Init();
}

static FSuspendRenderingThread* SuspendThread = NULL;

void FAppEntry::Tick()
{
    if (SuspendThread != NULL)
    {
        delete SuspendThread;
        SuspendThread = NULL;
        FPlatformProcess::SetRealTimeMode();
    }
    
	// tick the engine
	GEngineLoop.Tick();
}

void FAppEntry::SuspendTick()
{
    if (!SuspendThread)
    {
        SuspendThread = new FSuspendRenderingThread(true);
    }
    
    FPlatformProcess::Sleep(0.1f);
}

void FAppEntry::Shutdown()
{
	NSLog(@"%s", "Shutting down Game ULD Communications\n");
	GCommandSystem.Shutdown();
    
    // kill the engine
    GEngineLoop.Exit();
}

bool	FAppEntry::gAppLaunchedWithLocalNotification;
FString	FAppEntry::gLaunchLocalNotificationActivationEvent;
int32	FAppEntry::gLaunchLocalNotificationFireDate;

FString GSavedCommandLine;

int main(int argc, char *argv[])
{
    for(int Option = 1; Option < argc; Option++)
	{
		GSavedCommandLine += TEXT(" ");
		GSavedCommandLine += UTF8_TO_TCHAR(argv[Option]);
	}

	FIOSCommandLineHelper::InitCommandArgs(FString());
	
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("WaitForDebugger")))
    {
        while(!FPlatformMisc::IsDebuggerPresent())
        {
            FPlatformMisc::LowLevelOutputDebugString(TEXT("Waiting for debugger...\n"));
            FPlatformProcess::Sleep(1.f);
        }
        FPlatformMisc::LowLevelOutputDebugString(TEXT("Debugger attached.\n"));
    }
#endif
    
    
	@autoreleasepool {
	    return UIApplicationMain(argc, argv, nil, NSStringFromClass([IOSAppDelegate class]));
	}
}