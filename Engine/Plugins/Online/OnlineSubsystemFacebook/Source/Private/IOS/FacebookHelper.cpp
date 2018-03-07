// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FacebookHelper.h"
#include "OnlineSubsystemFacebookPrivate.h"

@implementation FFacebookHelper

- (id)init
{
	self = [super init];

	NSLog(@"Facebook SDK Version: %@", [FBSDKSettings sdkVersion]);

	NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
	NSOperationQueue* mainQueue = [NSOperationQueue mainQueue];

	[center addObserver:self selector:@selector(tokenChangeCallback:) name: FBSDKAccessTokenDidChangeNotification object:nil];
	[center addObserver:self selector:@selector(userIdChangeCallback:) name: FBSDKAccessTokenDidChangeUserID object:nil];

	[FBSDKProfile enableUpdatesOnAccessTokenChange:YES];
	[center addObserver:self selector:@selector(profileChangeCallback:) name: FBSDKProfileDidChangeNotification object:nil];

	return self;
}

-(FDelegateHandle)AddOnFacebookTokenChange: (const FOnFacebookTokenChangeDelegate&) Delegate
{
	_OnFacebookTokenChange.Add(Delegate);
	return Delegate.GetHandle();
}

-(FDelegateHandle)AddOnFacebookUserIdChange: (const FOnFacebookUserIdChangeDelegate&) Delegate
{
	_OnFacebookUserIdChange.Add(Delegate);
	return Delegate.GetHandle();
}

-(FDelegateHandle)AddOnFacebookProfileChange: (const FOnFacebookProfileChangeDelegate&) Delegate
{
	_OnFacebookProfileChange.Add(Delegate);
	return Delegate.GetHandle();
}

-(void)tokenChangeCallback: (NSNotification*) note
{
	const FString UserId([FBSDKAccessToken currentAccessToken].userID);
	const FString Token([FBSDKAccessToken currentAccessToken].tokenString);
	UE_LOG(LogOnline, Warning, TEXT("Facebook Token Change UserId: %s Token: %s"), *UserId, *Token);

#if !UE_BUILD_SHIPPING
	for(NSString *key in [[note userInfo] allKeys])
	{
		NSLog(@"Key: %@ Value: %@", key, [[note userInfo] objectForKey:key]);
	}
#endif

	// header mentions FBSDKAccessTokenChangeOldKey FBSDKAccessTokenChangeNewKey
	NSNumber* DidChange = [[note userInfo] objectForKey:@"FBSDKAccessTokenDidChangeUserID"];
	bool bDidTokenChange = [DidChange boolValue];
	FBSDKAccessToken* NewToken = [[note userInfo] objectForKey:@"FBSDKAccessToken"];
	FBSDKAccessToken* OldToken = [[note userInfo] objectForKey:@"FBSDKAccessTokenOld"];

	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		// Notify on the game thread
		_OnFacebookTokenChange.Broadcast(OldToken, NewToken);
		return true;
	}];
}

-(void)userIdChangeCallback: (NSNotification*) note
{
	const FString UserId([FBSDKAccessToken currentAccessToken].userID);
	const FString Token([FBSDKAccessToken currentAccessToken].tokenString);
	UE_LOG(LogOnline, Warning, TEXT("Facebook UserId Change UserId: %s Token: %s"), *UserId, *Token);

#if !UE_BUILD_SHIPPING
	for(NSString *key in [[note userInfo] allKeys])
	{
		NSLog(@"Key: %@ Value: %@", key, [[note userInfo] objectForKey:key]);
	}
#endif

	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		// Notify on the game thread
		_OnFacebookUserIdChange.Broadcast();
		return true;
	}];

}

-(void)profileChangeCallback: (NSNotification*) note
{
	const FString UserId([FBSDKAccessToken currentAccessToken].userID);
	const FString Token([FBSDKAccessToken currentAccessToken].tokenString);
	UE_LOG(LogOnline, Warning, TEXT("Facebook Profile Change UserId: %s Token: %s"), *UserId, *Token);

#if !UE_BUILD_SHIPPING
	for(NSString *key in [[note userInfo] allKeys])
	{
		NSLog(@"Key: %@ Value: %@", key, [[note userInfo] objectForKey:key]);
	}
#endif

	// header mentions FBSDKProfileChangeOldKey FBSDKProfileChangeNewKey
	FBSDKProfile* NewProfile = [[note userInfo] objectForKey:@"FBSDKProfileNew"];
	FBSDKProfile* OldProfile = [[note userInfo] objectForKey:@"FBSDKProfileOld"];

	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		// Notify on the game thread
		_OnFacebookProfileChange.Broadcast(OldProfile, NewProfile);
		return true;
	}];
}

@end
