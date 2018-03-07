// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineIdentityGoogle.h"
#include "OnlineSubsystemGooglePrivate.h"
#include "GoogleHelper.h"

#include "Misc/ConfigCacheIni.h"

FOnlineIdentityGoogle::FOnlineIdentityGoogle(FOnlineSubsystemGoogle* InSubsystem)
	: FOnlineIdentityGoogleCommon(InSubsystem)
{
	if (!GConfig->GetString(TEXT("OnlineSubsystemGoogle.OnlineIdentityGoogle"), TEXT("ReversedClientId"), ReversedClientId, GEngineIni))
	{
		UE_LOG(LogOnline, Warning, TEXT("Missing ReversedClientId= in [OnlineSubsystemGoogle.OnlineIdentityGoogle] of DefaultEngine.ini"));
	}

	// Setup permission scope fields
	GConfig->GetArray(TEXT("OnlineSubsystemGoogle.OnlineIdentityGoogle"), TEXT("ScopeFields"), ScopeFields, GEngineIni);
	// always required login access fields
	ScopeFields.AddUnique(TEXT(GOOGLE_PERM_PUBLIC_PROFILE));
}

bool FOnlineIdentityGoogle::Init()
{
	GoogleHelper = [[FGoogleHelper alloc] initwithClientId: GoogleSubsystem->GetAppId().GetNSString() withBasicProfile: true];
	[GoogleHelper retain];

	FOnGoogleSignInCompleteDelegate OnSignInDelegate;
	OnSignInDelegate.BindRaw(this, &FOnlineIdentityGoogle::OnSignInComplete);
	[GoogleHelper AddOnGoogleSignInComplete: OnSignInDelegate];

	FOnGoogleSignOutCompleteDelegate OnSignOutDelegate;
	OnSignOutDelegate.BindRaw(this, &FOnlineIdentityGoogle::OnSignOutComplete);
	[GoogleHelper AddOnGoogleSignOutComplete: OnSignOutDelegate];

	return true;
}

void FOnlineIdentityGoogle::OnSignInComplete(const FGoogleSignInData& InSignInData)
{
	UE_LOG_ONLINE(Verbose, TEXT("OnSignInComplete %s"), ToString(InSignInData.Response));

	// @todo verify that SignInSilently is working right
	//if (InSignInData.Response != EGoogleLoginResponse::RESPONSE_NOAUTH)
	if (LoginCompletionDelegate.IsBound())
	{
		//ensure(LoginCompletionDelegate.IsBound());
		LoginCompletionDelegate.ExecuteIfBound(InSignInData.Response, InSignInData.AuthToken);
		LoginCompletionDelegate.Unbind();
	}
}

void FOnlineIdentityGoogle::OnSignOutComplete(const FGoogleSignOutData& InSignOutData)
{
	UE_LOG_ONLINE(Verbose, TEXT("OnSignOutComplete %s"), ToString(InSignOutData.Response));
	ensure(LogoutCompletionDelegate.IsBound());
	LogoutCompletionDelegate.ExecuteIfBound(InSignOutData.Response);
	LogoutCompletionDelegate.Unbind();
}

bool FOnlineIdentityGoogle::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	UE_LOG_ONLINE(Verbose, TEXT("FOnlineIdentityGoogle::Login"));

	bool bTriggeredLogin = false;
	bool bPendingOp = LoginCompletionDelegate.IsBound() || LogoutCompletionDelegate.IsBound();
	if (!bPendingOp)
	{
		ELoginStatus::Type LoginStatus = GetLoginStatus(LocalUserNum);
		if (LoginStatus == ELoginStatus::NotLoggedIn)
		{
			PendingLoginRequestCb PendingLoginFn = [this, LocalUserNum](bool bWasSuccessful)
			{
				if (bWasSuccessful)
				{
					NSMutableArray* Permissions = [[NSMutableArray alloc] initWithCapacity:ScopeFields.Num()];
					for (int32 ScopeIdx = 0; ScopeIdx < ScopeFields.Num(); ScopeIdx++)
					{
						NSString* ScopeStr = [NSString stringWithFString:ScopeFields[ScopeIdx]];
						[Permissions addObject: ScopeStr];
					}

					LoginCompletionDelegate = FOnInternalLoginComplete::CreateLambda([this, LocalUserNum](EGoogleLoginResponse InResponseCode, const FAuthTokenGoogle& InAccessToken)
	                {
						if (InResponseCode == EGoogleLoginResponse::RESPONSE_OK)
						{
							Login(LocalUserNum, InAccessToken, FOnLoginCompleteDelegate::CreateRaw(this, &FOnlineIdentityGoogle::OnAccessTokenLoginComplete));
						}
						else
						{
							FString ErrorStr;
							if (InResponseCode == EGoogleLoginResponse::RESPONSE_CANCELED)
							{
								ErrorStr = GOOGLE_AUTH_CANCELED;
							}
							else
							{
								ErrorStr = FString::Printf(TEXT("Login failure %s"), ToString(InResponseCode));
							}

							OnLoginAttemptComplete(LocalUserNum, ErrorStr);
						}
					});

					[GoogleHelper login: Permissions];
					[Permissions release];
				}
				else
				{
					const FString ErrorStr = TEXT("Error retrieving discovery service");
					OnLoginAttemptComplete(LocalUserNum, ErrorStr);
				}
			};

			RetrieveDiscoveryDocument(MoveTemp(PendingLoginFn));
		}
		else
		{
			TriggerOnLoginCompleteDelegates(LocalUserNum, true, *GetUniquePlayerId(LocalUserNum), TEXT("Already logged in"));
		}
	}
	else
	{
		UE_LOG_ONLINE(Verbose, TEXT("FOnlineIdentityGoogle::Login Operation already in progress!"));
		FString ErrorStr = FString::Printf(TEXT("Operation already in progress"));
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, GetEmptyUniqueId(), ErrorStr);
	}

	return bTriggeredLogin;
}

void FOnlineIdentityGoogle::Login(int32 LocalUserNum, const FAuthTokenGoogle& InToken, const FOnLoginCompleteDelegate& InCompletionDelegate)
{
	FOnProfileRequestComplete ProfileCompletionDelegate = FOnProfileRequestComplete::CreateLambda([this, InCompletionDelegate](int32 ProfileLocalUserNum, bool bWasSuccessful, const FString& ErrorStr)
	{
		if (bWasSuccessful)
		{
			InCompletionDelegate.ExecuteIfBound(ProfileLocalUserNum, bWasSuccessful, *GetUniquePlayerId(ProfileLocalUserNum), ErrorStr);
		}
		else
		{
			InCompletionDelegate.ExecuteIfBound(ProfileLocalUserNum, bWasSuccessful, GetEmptyUniqueId(), ErrorStr);
		}
	});

	ProfileRequest(LocalUserNum, InToken, ProfileCompletionDelegate);
}

void FOnlineIdentityGoogle::OnAccessTokenLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UniqueId, const FString& Error)
{
	OnLoginAttemptComplete(LocalUserNum, Error);
}

void FOnlineIdentityGoogle::OnLoginAttemptComplete(int32 LocalUserNum, const FString& ErrorStr)
{
	const FString ErrorStrCopy(ErrorStr);
	if (GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
	{
		UE_LOG(LogOnline, Display, TEXT("Google login was successful"));
		TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
		check(UserId.IsValid());

		GoogleSubsystem->ExecuteNextTick([this, UserId, LocalUserNum, ErrorStrCopy]()
		{
			TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserId, ErrorStrCopy);
			TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::NotLoggedIn, ELoginStatus::LoggedIn, *UserId);
		});
	}
	else
	{
		GoogleSubsystem->ExecuteNextTick([this, LocalUserNum, ErrorStrCopy]()
	    {
			TriggerOnLoginCompleteDelegates(LocalUserNum, false, GetEmptyUniqueId(), ErrorStrCopy);
		});
	}
}

bool FOnlineIdentityGoogle::Logout(int32 LocalUserNum)
{
	bool bTriggeredLogout = false;

	ELoginStatus::Type LoginStatus = GetLoginStatus(LocalUserNum);
	bool bPendingOp = LoginCompletionDelegate.IsBound() || LogoutCompletionDelegate.IsBound();
	if (!bPendingOp)
	{
		if (LoginStatus == ELoginStatus::LoggedIn)
		{
			LogoutCompletionDelegate = FOnInternalLogoutComplete::CreateLambda([this, LocalUserNum](EGoogleLoginResponse InResponseCode)
	        {
				UE_LOG_ONLINE(Verbose, TEXT("FOnInternalLogoutComplete %s"), ToString(InResponseCode));
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

				GoogleSubsystem->ExecuteNextTick([this, UserId, LocalUserNum]()
			    {
					TriggerOnLogoutCompleteDelegates(LocalUserNum, true);
					TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::LoggedIn, ELoginStatus::NotLoggedIn, *UserId);
				});
			});

			bTriggeredLogout = true;
			[GoogleHelper logout];
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("No logged in user found for LocalUserNum=%d."), LocalUserNum);
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineIdentityGoogle::Logout - Operation already in progress"));
	}

	if (!bTriggeredLogout)
	{
		UE_LOG_ONLINE(Verbose, TEXT("FOnlineIdentityGoogle::Logout didn't trigger logout"));
		GoogleSubsystem->ExecuteNextTick([this, LocalUserNum]()
	    {
			TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
		});
	}

	return bTriggeredLogout;
}

