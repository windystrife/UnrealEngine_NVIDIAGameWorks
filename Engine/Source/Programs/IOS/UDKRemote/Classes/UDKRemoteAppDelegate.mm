// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//
//  UDKRemoteAppDelegate.m
//  UDKRemote
//
//  Created by jadams on 7/28/10.
//

#import "UDKRemoteAppDelegate.h"
#import "MainViewController.h"
#include "IPhoneObjCWrapper.h"
#include "IPhoneHome.h"

@implementation UDKRemoteAppDelegate

/** Port to send to */
#define DEFAULT_PORT_NUMBER 41765


@synthesize window;
@synthesize mainViewController;
@synthesize PCAddress;
@synthesize Port;
@synthesize bShouldIgnoreTilt;
@synthesize bShouldIgnoreTouch;
@synthesize bLockOrientation;
@synthesize LockedOrientation;
@synthesize RecentComputers;

extern time_t GAppInvokeTime;

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions 
{    
	NSUserDefaults* Defaults = [NSUserDefaults standardUserDefaults];
    // initialize the (saved) PC address	
	self.PCAddress = [Defaults stringForKey:@"PCAddress"];
	
	// load tilt setting
	self.bShouldIgnoreTilt = [Defaults boolForKey:@"bShouldIgnoreTilt"];
	self.bShouldIgnoreTouch = [Defaults boolForKey:@"bShouldIgnoreTouch"];
	self.bLockOrientation = [Defaults boolForKey:@"bLockOrientation"];
	self.LockedOrientation = (UIInterfaceOrientation)[Defaults integerForKey:@"LockedOrientation"];
	
	// load port setting
	self.Port = [Defaults integerForKey:@"Port"];
	if (self.Port == 0)
	{
		self.Port = DEFAULT_PORT_NUMBER;
	}
	self.RecentComputers = [NSMutableArray arrayWithArray:[Defaults stringArrayForKey:@"RecentComputers"]];

	// load either iphone or ipad interface
	MainViewController *aController;
	if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
	{
		aController = [[MainViewController alloc] initWithNibName:@"MainViewLarge" bundle:nil];
	}
	else
	{
		aController = [[MainViewController alloc] initWithNibName:@"MainView" bundle:nil];
	}

	// set the mainviewcontroller to the one loaded from a .nib
	self.mainViewController = aController;
	[aController release];
	
    mainViewController.view.frame = [UIScreen mainScreen].applicationFrame;
	[window addSubview:[mainViewController view]];
    [window makeKeyAndVisible];

	if (self.PCAddress == nil || [self.PCAddress compare:@""] == NSOrderedSame)
	{
		[aController showInfo];
	}
	
	// Try to phone home (the code inside is self-throttling)
	[IPhoneHome queueRequest];
	
	return YES;
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication *)application
{
	IPhoneIncrementUserSettingU64("IPhoneHome::NumMemoryWarnings");
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
	IPhoneIncrementUserSettingU64("IPhoneHome::NumInvocations");
	IPhoneSaveUserSettingU64("IPhoneHome::LastRunCrashed", 1);

	// Try to phone home (the code inside is self-throttling)
	[IPhoneHome queueRequest];
}

- (void)applicationWillResignActive:(UIApplication *)application
{
	// Accumulate the active time and clear the 'crashed' flag, since unloading while not active is fine
	time_t Now = time(NULL);
	IPhoneIncrementUserSettingU64("IPhoneHome::AppPlaytimeSecs", (uint64_t)(Now - GAppInvokeTime));
	IPhoneSaveUserSettingU64("IPhoneHome::LastRunCrashed", 0);
}

- (void)applicationWillTerminate:(UIApplication *)application
{
	// Not a crash, so clear the crash flag
	IPhoneSaveUserSettingU64("IPhoneHome::LastRunCrashed", 0);
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
	[mainViewController UpdateSocketAddr];
}

- (void)dealloc {
    [mainViewController release];
    [window release];
    [super dealloc];
}

@end
