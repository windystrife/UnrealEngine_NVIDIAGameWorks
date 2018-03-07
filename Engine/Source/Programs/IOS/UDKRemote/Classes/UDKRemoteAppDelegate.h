// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//
//  UDKRemoteAppDelegate.h
//  UDKRemote
//
//  Created by jadams on 7/28/10.
//

#import <UIKit/UIKit.h>

@class MainViewController;

@interface UDKRemoteAppDelegate : NSObject <UIApplicationDelegate> 
{
	UIWindow *window;
	MainViewController *mainViewController;
}

@property (nonatomic, retain) IBOutlet UIWindow *window;
@property (nonatomic, retain) MainViewController *mainViewController;

/** Properties set in the settings view */
@property (retain) NSString* PCAddress;
@property int Port;
@property BOOL bShouldIgnoreTilt;
@property BOOL bShouldIgnoreTouch;
@property BOOL bLockOrientation;
@property UIInterfaceOrientation LockedOrientation;
@property (retain) NSMutableArray* RecentComputers;


@end

