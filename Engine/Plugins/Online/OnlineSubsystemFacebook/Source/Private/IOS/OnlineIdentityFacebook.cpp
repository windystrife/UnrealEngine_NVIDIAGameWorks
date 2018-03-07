// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Module includes
#include "OnlineIdentityFacebook.h"
#include "OnlineSubsystemFacebookPrivate.h"
#include "OnlineSharingInterface.h"

#import <FBSDKCoreKit/FBSDKCoreKit.h>
#import <FBSDKLoginKit/FBSDKLoginKit.h>
#import "FacebookHelper.h"

// Other UE4 includes
#include "IOSAppDelegate.h"
#include "Misc/ConfigCacheIni.h"
#include "IOS/IOSAsyncTask.h"
#include "Misc/ConfigCacheIni.h"

///////////////////////////////////////////////////////////////////////////////////////
// FUserOnlineAccountFacebook implementation

void FUserOnlineAccountFacebook::Parse(const FBSDKAccessToken* AccessToken)
{
	const FString UserIdStr(AccessToken.userID);

	if (UserIdPtr->ToString().IsEmpty() ||
		UserIdPtr->ToString() != UserIdStr)
	{
		UserIdPtr = MakeShared<const FUniqueNetIdString>(UserIdStr);
	}

	const FString Token(AccessToken.tokenString);
	AuthTicket = Token;
}

void FUserOnlineAccountFacebook::Parse(const FBSDKProfile* NewProfile)
{
	ensure(NewProfile != nil);

	const FString NewProfileUserId(NewProfile.userID);
	if (UserIdPtr->ToString().IsEmpty() ||
		UserIdPtr->ToString() == NewProfileUserId)
	{
		UserIdPtr = MakeShared<const FUniqueNetIdString>(NewProfileUserId);

		RealName = FString(NewProfile.name);

		FirstName = FString(NewProfile.firstName);
		LastName = FString(NewProfile.lastName);
		
		SetAccountData(TEXT(ME_FIELD_ID), UserId);
		SetAccountData(TEXT(ME_FIELD_NAME), RealName);
		SetAccountData(TEXT(ME_FIELD_FIRSTNAME), FirstName);
		SetAccountData(TEXT(ME_FIELD_LASTNAME), LastName);

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
		NSISO8601DateFormatter* dateFormatter = [[NSISO8601DateFormatter alloc] init];
		dateFormatter.formatOptions = NSISO8601DateFormatWithInternetDateTime | NSISO8601DateFormatWithDashSeparatorInDate | NSISO8601DateFormatWithColonSeparatorInTime | NSISO8601DateFormatWithTimeZone;
#else
		// see https://developer.apple.com/library/content/qa/qa1480/_index.html
		NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
		NSLocale* enUSPOSIXLocale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
		[dateFormatter setLocale:enUSPOSIXLocale];
		[dateFormatter setDateFormat:@"yyyy-MM-dd'T'HH:mm:ssZZZZZ"];
#endif

		NSString* iso8601String = [dateFormatter stringFromDate: NewProfile.refreshDate];
		[dateFormatter release];

		SetAccountData(TEXT("refreshdate"), FString(iso8601String));
	}
}

///////////////////////////////////////////////////////////////////////////////////////
// FOnlineIdentityFacebook implementation

FOnlineIdentityFacebook::FOnlineIdentityFacebook(FOnlineSubsystemFacebook* InSubsystem)
	: FOnlineIdentityFacebookCommon(InSubsystem)
	, LoginStatus(ELoginStatus::NotLoggedIn)
{
	// Setup permission scope fields
	GConfig->GetArray(TEXT("OnlineSubsystemFacebook.OnlineIdentityFacebook"), TEXT("ScopeFields"), ScopeFields, GEngineIni);
	// always required fields
	ScopeFields.AddUnique(TEXT(PERM_PUBLIC_PROFILE));
}

bool FOnlineIdentityFacebook::Init()
{
	FacebookHelper = [[FFacebookHelper alloc] init];
	[FacebookHelper retain];

	FOnFacebookTokenChangeDelegate OnFacebookTokenChangeDelegate;
	OnFacebookTokenChangeDelegate.BindRaw(this, &FOnlineIdentityFacebook::OnFacebookTokenChange);
	[FacebookHelper AddOnFacebookTokenChange: OnFacebookTokenChangeDelegate];

	FOnFacebookUserIdChangeDelegate OnFacebookUserIdChangeDelegate;
	OnFacebookUserIdChangeDelegate.BindRaw(this, &FOnlineIdentityFacebook::OnFacebookUserIdChange);
	[FacebookHelper AddOnFacebookUserIdChange: OnFacebookUserIdChangeDelegate];

	FOnFacebookProfileChangeDelegate OnFacebookProfileChangeDelegate;
	OnFacebookProfileChangeDelegate.BindRaw(this, &FOnlineIdentityFacebook::OnFacebookProfileChange);
	[FacebookHelper AddOnFacebookProfileChange: OnFacebookProfileChangeDelegate];

	return true;
}

void FOnlineIdentityFacebook::Shutdown()
{
	[FacebookHelper release];
}

void FOnlineIdentityFacebook::OnFacebookTokenChange(FBSDKAccessToken* OldToken, FBSDKAccessToken* NewToken)
{
	UE_LOG(LogOnline, Warning, TEXT("FOnlineIdentityFacebook::OnFacebookTokenChange Old: %p New: %p"), OldToken, NewToken);
}

void FOnlineIdentityFacebook::OnFacebookUserIdChange()
{
	UE_LOG(LogOnline, Warning, TEXT("FOnlineIdentityFacebook::OnFacebookUserIdChange"));
}

void FOnlineIdentityFacebook::OnFacebookProfileChange(FBSDKProfile* OldProfile, FBSDKProfile* NewProfile)
{
	UE_LOG(LogOnline, Warning, TEXT("FOnlineIdentityFacebook::OnFacebookProfileChange Old: %p New: %p"), OldProfile, NewProfile);
}

bool FOnlineIdentityFacebook::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	bool bTriggeredLogin = true;

	if (GetLoginStatus(LocalUserNum) != ELoginStatus::NotLoggedIn)
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, true, *GetUniquePlayerId(LocalUserNum), TEXT("Already logged in"));
		return false;
	}

	ensure(LoginStatus == ELoginStatus::NotLoggedIn);

	dispatch_async(dispatch_get_main_queue(),^
		{
			FBSDKAccessToken *accessToken = [FBSDKAccessToken currentAccessToken];
			if (accessToken == nil)
			{
				FBSDKLoginManager* loginManager = [[FBSDKLoginManager alloc] init];
				// Start with iOS level account information, falls back to Native app, then web
				//loginManager.loginBehavior = FBSDKLoginBehaviorSystemAccount;
				NSMutableArray* Permissions = [[NSMutableArray alloc] initWithCapacity:ScopeFields.Num()];
				for (int32 ScopeIdx = 0; ScopeIdx < ScopeFields.Num(); ScopeIdx++)
				{
					NSString* ScopeStr = [NSString stringWithFString:ScopeFields[ScopeIdx]];
					[Permissions addObject: ScopeStr];
				}

				[loginManager logInWithReadPermissions:Permissions
					fromViewController:nil
					handler: ^(FBSDKLoginManagerLoginResult* result, NSError* error)
					{
						UE_LOG(LogOnline, Display, TEXT("[FBSDKLoginManager logInWithReadPermissions]"));
						bool bSuccessfulLogin = false;

						FString ErrorStr;
						if(error)
						{
							ErrorStr = FString::Printf(TEXT("[%d] %s"), [error code], [error localizedDescription]);
							UE_LOG(LogOnline, Display, TEXT("[FBSDKLoginManager logInWithReadPermissions = %s]"), *ErrorStr);

						}
						else if(result.isCancelled)
						{
							ErrorStr = FB_AUTH_CANCELED;
							UE_LOG(LogOnline, Display, TEXT("[FBSDKLoginManager logInWithReadPermissions = cancelled"));
						}						
						else
						{
							UE_LOG(LogOnline, Display, TEXT("[FBSDKLoginManager logInWithReadPermissions = true]"));
							bSuccessfulLogin = true;
						}

						const FString AccessToken([[result token] tokenString]);
						[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
						{
							// Trigger this on the game thread
							if (bSuccessfulLogin)
							{
								Login(LocalUserNum, AccessToken);
							}
							else
							{
								OnLoginAttemptComplete(LocalUserNum, ErrorStr);
							}

							return true;
						 }];
					}
				];
			}
			else
			{
				// Skip right to attempting to use the token to query user profile
				// Could fail with an expired auth token (eg. user revoked app)

				const FString AccessToken([accessToken tokenString]);
				[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
				{
					Login(LocalUserNum, AccessToken);
					return true;
				 }];
			}
		}
	);

	return bTriggeredLogin;	
}

void FOnlineIdentityFacebook::Login(int32 LocalUserNum, const FString& AccessToken)
{
	FOnProfileRequestComplete CompletionDelegate = FOnProfileRequestComplete::CreateLambda([this](int32 LocalUserNumFromRequest, bool bWasProfileRequestSuccessful, const FString& ErrorStr)
	{
		FOnRequestCurrentPermissionsComplete NextCompletionDelegate = FOnRequestCurrentPermissionsComplete::CreateLambda([this](int32 LocalUserNumFromPerms, bool bWerePermsSuccessful, const TArray<FSharingPermission>& Permissions)
		{
			OnRequestCurrentPermissionsComplete(LocalUserNumFromPerms, bWerePermsSuccessful, Permissions);
		});

		if (bWasProfileRequestSuccessful)
		{
			RequestCurrentPermissions(LocalUserNumFromRequest, NextCompletionDelegate);
		}
		else
		{
			OnLoginAttemptComplete(LocalUserNumFromRequest, ErrorStr);
		}
	});

	ProfileRequest(LocalUserNum, AccessToken, ProfileFields, CompletionDelegate);
}

void FOnlineIdentityFacebook::OnRequestCurrentPermissionsComplete(int32 LocalUserNum, bool bWasSuccessful, const TArray<FSharingPermission>& NewPermissions)
{
	FString ErrorStr;
	if (!bWasSuccessful)
	{
		ErrorStr = TEXT("Failure to request current sharing permissions");
	}

	LoginStatus = bWasSuccessful ? ELoginStatus::LoggedIn : ELoginStatus::NotLoggedIn;
	OnLoginAttemptComplete(LocalUserNum, ErrorStr);
}

void FOnlineIdentityFacebook::OnLoginAttemptComplete(int32 LocalUserNum, const FString& ErrorStr)
{
	if (LoginStatus == ELoginStatus::LoggedIn)
	{
		UE_LOG(LogOnline, Display, TEXT("Facebook login was successful"));
		TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
		check(UserId.IsValid());
		TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserId, ErrorStr);
		TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::NotLoggedIn, ELoginStatus::LoggedIn, *UserId);
	}
	else
	{
		const FString NewErrorStr(ErrorStr);
		// Clean up anything left behind from cached access tokens
		dispatch_async(dispatch_get_main_queue(),^
		{
			FBSDKLoginManager* loginManager = [[FBSDKLoginManager alloc] init];
			[loginManager logOut];

			[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
			 {
				// Trigger this on the game thread
				UE_LOG(LogOnline, Display, TEXT("Facebook login failed: %s"), *NewErrorStr);

				TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
				if (UserId.IsValid())
				{
					// remove cached user account
					UserAccounts.Remove(UserId->ToString());
				}
				else
				{
					UserId = GetEmptyUniqueId().AsShared();
				}
				// remove cached user id
				UserIds.Remove(LocalUserNum);

				TriggerOnLoginCompleteDelegates(LocalUserNum, false, *UserId, NewErrorStr);
				return true;
			 }];
		});
	}
}

bool FOnlineIdentityFacebook::Logout(int32 LocalUserNum)
{
	if ([FBSDKAccessToken currentAccessToken])
	{
		ensure(LoginStatus == ELoginStatus::LoggedIn);

		dispatch_async(dispatch_get_main_queue(),^
		{
			FBSDKLoginManager* loginManager = [[FBSDKLoginManager alloc] init];
			[loginManager logOut];

			[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
			{
				// Trigger this on the game thread
				TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
				if (UserId.IsValid())
				{
					// remove cached user account
					UserAccounts.Remove(UserId->ToString());
				}
				else
				{
					UserId = GetEmptyUniqueId().AsShared();
				}
				// remove cached user id
				UserIds.Remove(LocalUserNum);

				FacebookSubsystem->ExecuteNextTick([this, UserId, LocalUserNum]() {
					LoginStatus = ELoginStatus::NotLoggedIn;
					TriggerOnLogoutCompleteDelegates(LocalUserNum, true);
					TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::LoggedIn, ELoginStatus::NotLoggedIn, *UserId);
				});
				return true;
			 }];
		});
	}
	else
	{
		ensure(LoginStatus == ELoginStatus::NotLoggedIn);

		UE_LOG(LogOnline, Warning, TEXT("No logged in user found for LocalUserNum=%d."), LocalUserNum);
		FacebookSubsystem->ExecuteNextTick([this, LocalUserNum](){
			TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
		});
	}

	return true;
}


