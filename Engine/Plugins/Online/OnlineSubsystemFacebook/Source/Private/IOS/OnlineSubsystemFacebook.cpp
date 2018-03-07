// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemFacebook.h"
#include "OnlineSubsystemFacebookPrivate.h"

#include "OnlineFriendsFacebook.h"
#include "OnlineIdentityFacebook.h"
#include "OnlineSharingFacebook.h"
#include "OnlineUserFacebook.h"

#import <FBSDKCoreKit/FBSDKCoreKit.h>

#include "CoreDelegates.h"
#include "IOSAppDelegate.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"

FOnlineSubsystemFacebook::FOnlineSubsystemFacebook()
{
}

FOnlineSubsystemFacebook::FOnlineSubsystemFacebook(FName InInstanceName)
	: FOnlineSubsystemFacebookCommon(InInstanceName)
{
	FString IOSFacebookAppID;
	if (!GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("FacebookAppID"), IOSFacebookAppID, GEngineIni))
	{
		UE_LOG(LogOnline, Warning, TEXT("The [IOSRuntimeSettings]:FacebookAppID has not been set"));
	}

	if (ClientId.IsEmpty() || IOSFacebookAppID.IsEmpty() || (IOSFacebookAppID != ClientId))
	{
		UE_LOG(LogOnline, Warning, TEXT("Inconsistency between OnlineSubsystemFacebook AppId [%s] and IOSRuntimeSettings AppId [%s]"), *ClientId, *IOSFacebookAppID);
	}
}

FOnlineSubsystemFacebook::~FOnlineSubsystemFacebook()
{
}

static void OnFacebookOpenURL(UIApplication* application, NSURL* url, NSString* sourceApplication, id annotation)
{
	[[FBSDKApplicationDelegate sharedInstance] application:application
		openURL : url
		sourceApplication : sourceApplication
		annotation : annotation];
}

static void OnFacebookAppDidBecomeActive()
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		[FBSDKAppEvents activateApp];
	});
}

bool FOnlineSubsystemFacebook::Init()
{
	bool bSuccessfullyStartedUp = false;
	if (FOnlineSubsystemFacebookCommon::Init())
	{
		FIOSCoreDelegates::OnOpenURL.AddStatic(&OnFacebookOpenURL);
		FCoreDelegates::ApplicationHasReactivatedDelegate.AddStatic(&OnFacebookAppDidBecomeActive);

		FOnlineIdentityFacebookPtr TempPtr = MakeShareable(new FOnlineIdentityFacebook(this));
		if (TempPtr->Init())
		{
			FacebookIdentity = TempPtr;
		}
		FacebookSharing = MakeShareable(new FOnlineSharingFacebook(this));
		FacebookFriends = MakeShareable(new FOnlineFriendsFacebook(this));
		FacebookUser = MakeShareable(new FOnlineUserFacebook(this));
		
		// Trigger Facebook SDK last now that everything is setup
		dispatch_async(dispatch_get_main_queue(), ^
		{
			UIApplication* sharedApp = [UIApplication sharedApplication];
			NSDictionary* launchDict = [IOSAppDelegate GetDelegate].launchOptions;
			[FBSDKAppEvents activateApp];
			[[FBSDKApplicationDelegate sharedInstance] application:sharedApp didFinishLaunchingWithOptions : launchDict];
		});

		bSuccessfullyStartedUp = FacebookIdentity.IsValid() && FacebookSharing.IsValid() && FacebookFriends.IsValid() && FacebookUser.IsValid();
	}
	return bSuccessfullyStartedUp;
}

bool FOnlineSubsystemFacebook::Shutdown() 
{
	bool bSuccessfullyShutdown = true;
	StaticCastSharedPtr<FOnlineIdentityFacebook>(FacebookIdentity)->Shutdown();

	bSuccessfullyShutdown = FOnlineSubsystemFacebookCommon::Shutdown();
	return bSuccessfullyShutdown;
}

bool FOnlineSubsystemFacebook::IsEnabled() const
{
	bool bEnableFacebookSupport = false;

	// IOSRuntimeSettings holds a value for editor ease of use
	if (!GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bEnableFacebookSupport"), bEnableFacebookSupport, GEngineIni))
	{
		UE_LOG(LogOnline, Warning, TEXT("The [IOSRuntimeSettings]:bEnableFacebookSupport flag has not been set"));

		// Fallback to regular OSS location
		bEnableFacebookSupport = FOnlineSubsystemFacebookCommon::IsEnabled();
	}

	return bEnableFacebookSupport;
}
