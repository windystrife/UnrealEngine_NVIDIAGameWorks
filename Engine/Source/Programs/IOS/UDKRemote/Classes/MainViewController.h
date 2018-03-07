// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//
//  MainViewController.h
//  UDKRemote
//
//  Created by jadams on 7/28/10.
//

#import "FlipsideViewController.h"
#import <Foundation/NSDictionary.h>

@class UDKRemoteAppDelegate;
@class CMMotionManager;
@class CMAttitude;

@interface MainViewController : UIViewController <UINavigationControllerDelegate, UIAccelerometerDelegate> 
{
@private
	/** Socket to send touch/tilt data through */
	CFSocketRef PushSocket;

	/** Socket to listen to data through */
	CFSocketRef ReplySocket;
	
	/** SocketAddr packaged in a CFDataRef */
	CFDataRef SocketAddrData;
		
	/** Have we initialized the acceleration filter yet? */
	BOOL bHasInitializedFilter;
	
	/** Filtered acceleration (this all mimics UE3's iPhone accel stuff */
	float FilteredAccelerometer[3];
	
	/** Centered pitch and roll, for calibration */
	float CenterPitch, CenterRoll;
	
	/** If YES, the next acceleration function will take the current pitch and roll and make them "zero" */
	BOOL bRecenterPitchAndRoll;
	
	/** Async infight host resolution object */
	CFHostRef ResolvingHost;
	
	/** Block of data to push across, the bytes will be updated each push */
	NSMutableData* PushData;
	
	/** Unique ID per message, for ordering purposes */
	unsigned short MessageID;
	
	/** Cache the app delegate pointer */
	UDKRemoteAppDelegate* AppDelegate;	
	
	/** Tag to give the next unique view */
	int NextTag;
	
	/** Track the touches that are known, to get a reusable ID */
	UITouch* AllTouches[5];
	
	/** Images for the touches */
	UIImageView* TouchImageViews[5];
	
	/** Count how many ping timers have fired without there being any replies */
	int PingsWithoutReply;
	
	/** Are we currently connected (as much as we can via UDP) */
	BOOL bIsConnected;
}

/**
 * Set the current tilt to be the "zero" rotation
 */
- (void)CalibrateTilt;


/**
 * Flip the view to the back view
 */
- (IBAction)showInfo;

/**
 * Resolve a network name to IP address
 */
- (BOOL)UpdateSocketAddr;

- (void)flipsideViewControllerDidFinish:(FlipsideViewController *)controller;

/**
 * Label properties
 */
@property (nonatomic, retain) IBOutlet UILabel* HostNameLabel;
@property (nonatomic, retain) IBOutlet UILabel* ResolvedNameLabel;
@property (nonatomic, retain) IBOutlet UILabel* HelpLabel;
@property (nonatomic, retain) IBOutlet UINavigationController* NavController;
@property (nonatomic, retain) IBOutlet UIImageView* Background;
@property (retain) CMMotionManager* MotionManager;
@property (retain) CMAttitude* ReferenceAttitude;
@property (retain) NSTimer* MotionTimer;
@property (retain) NSTimer* PingTimer;
@property (retain) NSString* ResolvedAddrString;
@property (retain) NSMutableData* ReceiveData;


@end
