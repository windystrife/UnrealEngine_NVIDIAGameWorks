// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GoogleHelper.h"
#include "OnlineSubsystemGooglePrivate.h"

#include "CoreDelegates.h"
#include "IOSAppDelegate.h"
#include "IOS/IOSAsyncTask.h"

// Explicit refresh token
//refreshTokensWithHandler:

// Possibly refresh token, handled by SDK
//getTokensWithHandler:

bool GetAuthTokenFromGoogleUser(GIDGoogleUser* user, FAuthTokenGoogle& OutAuthToken)
{
	bool bSuccess = false;

	OutAuthToken.AccessToken = user.authentication.accessToken;
	if (!OutAuthToken.AccessToken.IsEmpty())
	{
		OutAuthToken.IdToken = user.authentication.idToken;
		if (!OutAuthToken.IdToken.IsEmpty() && OutAuthToken.IdTokenJWT.Parse(OutAuthToken.IdToken))
		{
			OutAuthToken.TokenType = TEXT("Bearer");
			OutAuthToken.ExpiresIn = 3600.0;
			OutAuthToken.RefreshToken = user.authentication.refreshToken;

			OutAuthToken.AuthData.Add("refresh_token", OutAuthToken.RefreshToken);
			OutAuthToken.AuthData.Add("access_token", OutAuthToken.AccessToken);
			OutAuthToken.AuthData.Add("id_token", OutAuthToken.IdToken);
			OutAuthToken.AuthType = EGoogleAuthTokenType::AccessToken;
			OutAuthToken.ExpiresInUTC = FDateTime::UtcNow() + FTimespan(OutAuthToken.ExpiresIn * ETimespan::TicksPerSecond);
			bSuccess = true;
		}
		else
		{
			UE_LOG_ONLINE(Verbose, TEXT("GetAuthTokenFromGoogleUser: Failed to parse id token"));
		}
	}
	else
	{
		UE_LOG_ONLINE(Verbose, TEXT("GetAuthTokenFromGoogleUser: Access token missing"));
	}

	return bSuccess;
}

@implementation FGoogleHelper

- (id) initwithClientId: (NSString*) inClientId withBasicProfile: (bool) bWithProfile
{
	self = [super init];
	if (inClientId && ([inClientId length] > 0))
	{
		[self printAuthStatus];

		GIDSignIn* signIn = [GIDSignIn sharedInstance];
		signIn.shouldFetchBasicProfile = bWithProfile;
		signIn.delegate = self;
		signIn.uiDelegate = self;
		signIn.clientID = inClientId;

		dispatch_async(dispatch_get_main_queue(), ^
		{
			// Try to automatically sign in the user
			[signIn signInSilently];
		});
	}
	else
	{
		UE_LOG(LogOnline, Error, TEXT("Google init missing clientId"));
	}

	return self;
}

-(FDelegateHandle)AddOnGoogleSignInComplete: (const FOnGoogleSignInCompleteDelegate&) Delegate
{
	_OnSignInComplete.Add(Delegate);
	return Delegate.GetHandle();
}

-(FDelegateHandle)AddOnGoogleSignOutComplete: (const FOnGoogleSignOutCompleteDelegate&) Delegate
{
	_OnSignOutComplete.Add(Delegate);
	return Delegate.GetHandle();
}

-(FDelegateHandle)AddOnGoogleDisconnectComplete: (const FOnGoogleSignOutCompleteDelegate&) Delegate
{
	_OnDisconnectComplete.Add(Delegate);
	return Delegate.GetHandle();
}

- (void) login: (NSArray*) InScopes
{
	[self printAuthStatus];

	dispatch_async(dispatch_get_main_queue(), ^
	{
		GIDSignIn* signIn = [GIDSignIn sharedInstance];
		[signIn setScopes: InScopes];
		[signIn signIn];
	});
}

- (void) logout
{
	UE_LOG(LogOnline, Display, TEXT("logout"));

	dispatch_async(dispatch_get_main_queue(), ^
	{
		GIDSignIn* signIn = [GIDSignIn sharedInstance];
		[signIn signOut];
		
		FGoogleSignOutData SignOutData;
		SignOutData.Response = EGoogleLoginResponse::RESPONSE_OK;

		[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
		{
			UE_LOG(LogOnline, Display, TEXT("logoutComplete: %s"), *SignOutData.ToDebugString());

			// Notify on the game thread
			_OnSignOutComplete.Broadcast(SignOutData);
			return true;
		}];

		// Revokes access (use to clear keychain/cache), triggers didDisconnectWithUser
		// [signIn disconnect];
	});
}

- (void)signIn:(GIDSignIn *)signIn didSignInForUser:(GIDGoogleUser *)user withError:(NSError *)error
{
	FGoogleSignInData SignInData;
	SignInData.ErrorStr = MoveTemp(FString([error localizedDescription]));

	UE_LOG(LogOnline, Display, TEXT("signIn didSignInForUser GID:%p User:%p Error:%s"), signIn, user, *SignInData.ErrorStr);
	[self printAuthStatus];

	if (user)
	{
		if (GetAuthTokenFromGoogleUser(user, SignInData.AuthToken))
		{
			SignInData.Response = EGoogleLoginResponse::RESPONSE_OK;
		}
		else
		{
			SignInData.Response = EGoogleLoginResponse::RESPONSE_ERROR;
		}
	}
	else if (error.code == kGIDSignInErrorCodeHasNoAuthInKeychain)
	{
		SignInData.Response = EGoogleLoginResponse::RESPONSE_NOAUTH;
	}
	else if (error.code == kGIDSignInErrorCodeCanceled)
	{
		SignInData.Response = EGoogleLoginResponse::RESPONSE_CANCELED;
	}
	else
	{
		SignInData.Response = EGoogleLoginResponse::RESPONSE_ERROR;
	}

	UE_LOG(LogOnline, Display, TEXT("SignIn: %s"), *SignInData.ToDebugString());

	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		// Notify on the game thread
		_OnSignInComplete.Broadcast(SignInData);
		return true;
	}];
}

- (void)signIn:(GIDSignIn *)signIn didDisconnectWithUser:(GIDGoogleUser *)user withError:(NSError *)error
{
	FGoogleSignOutData SignOutData;
	SignOutData.ErrorStr = MoveTemp(FString([error localizedDescription]));
	SignOutData.Response = SignOutData.ErrorStr.IsEmpty() ? EGoogleLoginResponse::RESPONSE_OK : EGoogleLoginResponse::RESPONSE_ERROR;

	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		UE_LOG(LogOnline, Display, TEXT("didDisconnectWithUser Complete: %s"), *SignOutData.ToDebugString());

		// Notify on the game thread
		_OnDisconnectComplete.Broadcast(SignOutData);
		return true;
	}];
}

- (void)signInWillDispatch:(GIDSignIn *)signIn error:(NSError *)error
{
	UE_LOG(LogOnline, Display, TEXT("signInWillDispatch %p %s"), signIn, *FString([error localizedDescription]));

	// Google flow has figured out how to proceed, any engine related "please wait" is no longer necessary
}

- (void)signIn:(GIDSignIn *)signIn presentViewController:(UIViewController *)viewController
{
	UE_LOG(LogOnline, Display, TEXT("presentViewController %p"), signIn);

	// Google has provided a view controller for us to login, we present it.
	[[IOSAppDelegate GetDelegate].IOSController
	 presentViewController:viewController animated: YES completion: nil];
}

- (void)signIn:(GIDSignIn *)signIn dismissViewController:(UIViewController *)viewController
{
	UE_LOG(LogOnline, Display, TEXT("dismissViewController %p"), signIn);

	// Dismiss the Google sign in view
	[viewController dismissViewControllerAnimated: YES completion: nil];
}

- (void)printAuthStatus
{
	GIDSignIn* signIn = [GIDSignIn sharedInstance];
	GIDGoogleUser* googleUser = [signIn currentUser];

	bool bHasAuth = [signIn hasAuthInKeychain];
	UE_LOG(LogOnline, Display, TEXT("HasAuth: %d"), bHasAuth);

	UE_LOG(LogOnline, Display, TEXT("Authentication:"));
	if (googleUser.authentication)
	{
		UE_LOG(LogOnline, Display, TEXT("- Access: %s"), *FString(googleUser.authentication.accessToken));
		UE_LOG(LogOnline, Display, TEXT("- Refresh: %s"), *FString(googleUser.authentication.refreshToken));
	}
	else
	{
		UE_LOG(LogOnline, Display, TEXT("- None"));
	}

	UE_LOG(LogOnline, Display, TEXT("Scopes:"));
	for (NSString* scope in signIn.scopes)
	{
		UE_LOG(LogOnline, Display, TEXT("- %s"), *FString(scope));
	}

	UE_LOG(LogOnline, Display, TEXT("User:"));
	if (googleUser)
	{
		UE_LOG(LogOnline, Display, TEXT("- UserId: %s RealName: %s FirstName: %s LastName: %s Email: %s"),
			   *FString(googleUser.userID),
			   *FString(googleUser.profile.name),
			   *FString(googleUser.profile.givenName),
			   *FString(googleUser.profile.familyName),
			   *FString(googleUser.profile.email));
			   //*FString(googleUser.authentication.idToken));
	}
	else
	{
		UE_LOG(LogOnline, Display, TEXT("- None"));
	}
}

@end
