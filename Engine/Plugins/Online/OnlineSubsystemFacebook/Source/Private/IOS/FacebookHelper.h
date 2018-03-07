// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

// Other UE4 includes
#include "IOSAppDelegate.h"
#include "IOS/IOSAsyncTask.h"

THIRD_PARTY_INCLUDES_START
#import <FBSDKCoreKit/FBSDKCoreKit.h>
#import <FBSDKLoginKit/FBSDKLoginKit.h>
THIRD_PARTY_INCLUDES_END

/**
 * Delegate fired when a FBSDKAccessToken token has changed
 *
 * @param OldToken previous access token, possibly null
 * @param NewToken current access token, possibly null
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFacebookTokenChange, FBSDKAccessToken* /* OldToken */, FBSDKAccessToken* /* NewToken */);
typedef FOnFacebookTokenChange::FDelegate FOnFacebookTokenChangeDelegate;

/**
 * Delegate fired when the SDK UserId has changed
 */
DECLARE_MULTICAST_DELEGATE(FOnFacebookUserIdChange);
typedef FOnFacebookUserIdChange::FDelegate FOnFacebookUserIdChangeDelegate;

/**
 * Delegate fired when the FBSDKProfile data has changed
 *
 * @param OldProfile previous profile, possibly null
 * @param NewProfile current profile, possibly null
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFacebookProfileChange, FBSDKProfile* /* OldProfile */, FBSDKProfile* /* NewProfile */);
typedef FOnFacebookProfileChange::FDelegate FOnFacebookProfileChangeDelegate;

/**
 * ObjC helper for communicating with the Facebook SDK, listens for events
 */
@interface FFacebookHelper : NSObject
{
	FOnFacebookTokenChange _OnFacebookTokenChange;
	FOnFacebookUserIdChange _OnFacebookUserIdChange;
	FOnFacebookProfileChange _OnFacebookProfileChange;
};

/** Add a listener to the token change event */
-(FDelegateHandle)AddOnFacebookTokenChange: (const FOnFacebookTokenChangeDelegate&) Delegate;
/** Add a listener to the user id change event */
-(FDelegateHandle)AddOnFacebookUserIdChange: (const FOnFacebookUserIdChangeDelegate&) Delegate;
/** Add a listener to the profile change event */
-(FDelegateHandle)AddOnFacebookProfileChange: (const FOnFacebookProfileChangeDelegate&) Delegate;

/** Delegate fired when a token change has occurred */
-(void)tokenChangeCallback: (NSNotification*) note;
/** Delegate fired when a user id change has occurred */
-(void)userIdChangeCallback: (NSNotification*) note;
/** Delegate fired when a profile change has occurred */
-(void)profileChangeCallback: (NSNotification*) note;

@end
