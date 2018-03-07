// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemTwitchPrivate.h"
#include "OnlineExternalUITwitch.h"
#include "OnlineIdentityTwitch.h"
#include "OnlineError.h"

#define TWITCH_STATE_TOKEN TEXT("state")
#define TWITCH_ACCESS_TOKEN TEXT("access_token")

#define LOGIN_ERROR_UNKNOWN TEXT("com.epicgames.login.unknown")

bool FOnlineExternalUITwitch::ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate)
{
	bool bStarted = false;
	if (ControllerIndex >= 0 && ControllerIndex < MAX_LOCAL_PLAYERS)
	{
		FOnlineIdentityTwitchPtr IdentityInt = TwitchSubsystem->GetTwitchIdentityService();
		if (IdentityInt.IsValid())
		{
			const FTwitchLoginURL& URLDetails = IdentityInt->GetLoginURLDetails();
			if (URLDetails.IsValid())
			{
				const FString& CurrentLoginNonce = IdentityInt->GetCurrentLoginNonce();
				const FString RequestedURL = URLDetails.GetAuthUrl(CurrentLoginNonce);
				if (OnLoginFlowUIRequiredDelegates.IsBound())
				{
					bool bShouldContinueLoginFlow = false;
					FOnLoginRedirectURL OnRedirectURLDelegate = FOnLoginRedirectURL::CreateThreadSafeSP(this, &FOnlineExternalUITwitch::OnLoginRedirectURL);
					FOnLoginFlowComplete OnExternalLoginFlowCompleteDelegate = FOnLoginFlowComplete::CreateThreadSafeSP(this, &FOnlineExternalUITwitch::OnExternalLoginFlowComplete, ControllerIndex, Delegate);
					TriggerOnLoginFlowUIRequiredDelegates(RequestedURL, OnRedirectURLDelegate, OnExternalLoginFlowCompleteDelegate, bShouldContinueLoginFlow);
					bStarted = bShouldContinueLoginFlow;
				}
				else
				{
					IOnlineSubsystem* PlatformSubsystem = IOnlineSubsystem::GetByPlatform();
					IOnlineExternalUIPtr PlatformExternalUI = PlatformSubsystem ? PlatformSubsystem->GetExternalUIInterface() : nullptr;
					if (PlatformExternalUI.IsValid())
					{
						FShowWebUrlParams ShowParams;
						ShowParams.bEmbedded = false; // TODO:  See if we can pipe along the embedded coordinates
						ShowParams.bShowBackground = true;
						ShowParams.bShowCloseButton = true;
						ShowParams.bResetCookies = true; // potential for previously logged in user
						ShowParams.CallbackPath = URLDetails.GetLoginRedirectUrl();

						FOnShowWebUrlClosedDelegate OnConsoleShowWebUrlCompleteDelegate = FOnShowWebUrlClosedDelegate::CreateThreadSafeSP(this, &FOnlineExternalUITwitch::OnConsoleShowWebUrlComplete, ControllerIndex, Delegate);
						bStarted = PlatformExternalUI->ShowWebURL(RequestedURL, ShowParams, OnConsoleShowWebUrlCompleteDelegate);
						if (!bStarted)
						{
							UE_LOG_ONLINE(Warning, TEXT("FOnlineExternalUITwitch::ShowLoginUI: Console ShowWebURL failed"));
						}
					}
				}
			}
			else
			{
				UE_LOG_ONLINE(Error, TEXT("ShowLoginUI: Url Details not properly configured"));
			}
		}
		else
		{
			UE_LOG_ONLINE(Error, TEXT("ShowLoginUI: Missing identity interface"));
		}
	}
	else
	{
		UE_LOG_ONLINE(Error, TEXT("ShowLoginUI: Invalid controller index (%d)"), ControllerIndex);
	}

	if (!bStarted)
	{
		TwitchSubsystem->ExecuteNextTick([ControllerIndex, Delegate]()
		{
			Delegate.ExecuteIfBound(nullptr, ControllerIndex);
		});
	}

	return bStarted;
}

FLoginFlowResult FOnlineExternalUITwitch::ParseRedirectResult(const FTwitchLoginURL& URLDetails, const FString& RedirectURL)
{
	FLoginFlowResult Result;

	TMap<FString, FString> ParamsMap;

	FString ResponseStr = RedirectURL.Mid(URLDetails.GetLoginRedirectUrl().Len() + 1);
	{
		TArray<FString> Params;
		ResponseStr.ParseIntoArray(Params, TEXT("&"));
		for (FString& Param : Params)
		{
			FString Key, Value;
			if (Param.Split(TEXT("="), &Key, &Value))
			{
				ParamsMap.Add(MoveTemp(Key), MoveTemp(Value));
			}
		}
	}

	const FString* State = ParamsMap.Find(TWITCH_STATE_TOKEN);
	if (State)
	{
		FOnlineIdentityTwitchPtr IdentityInt = TwitchSubsystem->GetTwitchIdentityService();
		check(IdentityInt.IsValid());

		const FString& CurrentLoginNonce = IdentityInt->GetCurrentLoginNonce();
		if (CurrentLoginNonce == URLDetails.ParseNonce(*State))
		{
			const FString* AccessToken = ParamsMap.Find(TWITCH_ACCESS_TOKEN);
			if (AccessToken)
			{
				Result.Error.bSucceeded = true;
				Result.Token = *AccessToken;
			}
			else
			{
				// Set some default in case parsing fails
				Result.Error.ErrorRaw = LOGIN_ERROR_UNKNOWN;
				Result.Error.ErrorMessage = FText::FromString(LOGIN_ERROR_UNKNOWN);
				Result.Error.ErrorCode = TEXT("-1");
				Result.Error.NumericErrorCode = -1;
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("FOnlineExternalUITwitch::ParseRedirectResult: State does not match (received=%s, expected=%s)"), **State, *CurrentLoginNonce);
		}
	}

	return Result;
}

FLoginFlowResult FOnlineExternalUITwitch::OnLoginRedirectURL(const FString& RedirectURL)
{
	FLoginFlowResult Result;

	FOnlineIdentityTwitchPtr IdentityInt = TwitchSubsystem->GetTwitchIdentityService();
	if (IdentityInt.IsValid())
	{  
		const FTwitchLoginURL& URLDetails = IdentityInt->GetLoginURLDetails();
		if (URLDetails.IsValid())
		{
			// Wait for the RedirectURI to appear
			if (RedirectURL.StartsWith(URLDetails.GetLoginRedirectUrl()))
			{
				Result = ParseRedirectResult(URLDetails, RedirectURL);
			}
			else
			{
				// TODO:  Does Twitch redirect the user to a URL we would want to close on?  I haven't found any, it just keeps the user on the login page
			}
		}
	}

	return Result;
}

void FOnlineExternalUITwitch::OnLoginUIComplete(const FLoginFlowResult& Result, int ControllerIndex, const FOnLoginUIClosedDelegate& Delegate)
{
	bool bStarted = false;
	if (Result.IsValid())
	{
		FOnlineIdentityTwitchPtr IdentityInt = TwitchSubsystem->GetTwitchIdentityService();
		if (IdentityInt.IsValid())
		{
			bStarted = true;

			FOnLoginCompleteDelegate CompletionDelegate;
			CompletionDelegate = FOnLoginCompleteDelegate::CreateThreadSafeSP(this, &FOnlineExternalUITwitch::OnAccessTokenLoginComplete, Delegate);
			IdentityInt->LoginWithAccessToken(ControllerIndex, Result.Token, CompletionDelegate);
		}
	}

	if (!bStarted)
	{
		TwitchSubsystem->ExecuteNextTick([ControllerIndex, Delegate]()
		{
			Delegate.ExecuteIfBound(nullptr, ControllerIndex);
		});
	}
}

void FOnlineExternalUITwitch::OnExternalLoginFlowComplete(const FLoginFlowResult& Result, int ControllerIndex, const FOnLoginUIClosedDelegate Delegate)
{
	UE_LOG_ONLINE(Log, TEXT("OnExternalLoginFlowComplete %s"), *Result.ToDebugString());
	OnLoginUIComplete(Result, ControllerIndex, Delegate);
}

void FOnlineExternalUITwitch::OnConsoleShowWebUrlComplete(const FString& FinalUrl, int ControllerIndex, const FOnLoginUIClosedDelegate Delegate)
{
	FLoginFlowResult Result = OnLoginRedirectURL(FinalUrl);

	UE_LOG_ONLINE(Log, TEXT("OnConsoleShowWebUrlComplete %s"), *Result.ToDebugString());
	OnLoginUIComplete(Result, ControllerIndex, Delegate);
}

void FOnlineExternalUITwitch::OnAccessTokenLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error, FOnLoginUIClosedDelegate Delegate)
{
	TSharedPtr<const FUniqueNetId> StrongUserId = UserId.AsShared();
	TwitchSubsystem->ExecuteNextTick([StrongUserId, LocalUserNum, Delegate]()
	{
		Delegate.ExecuteIfBound(StrongUserId, LocalUserNum);
	});
}

bool FOnlineExternalUITwitch::ShowFriendsUI(int32 LocalUserNum)
{
	return false;
}

bool FOnlineExternalUITwitch::ShowInviteUI(int32 LocalUserNum, FName SessionName)
{
	return false;
}

bool FOnlineExternalUITwitch::ShowAchievementsUI(int32 LocalUserNum)
{
	return false;
}

bool FOnlineExternalUITwitch::ShowLeaderboardUI(const FString& LeaderboardName)
{
	return false;
}

bool FOnlineExternalUITwitch::ShowWebURL(const FString& Url, const FShowWebUrlParams& ShowParams, const FOnShowWebUrlClosedDelegate& Delegate)
{
	return false;
}

bool FOnlineExternalUITwitch::CloseWebURL()
{
	return false;
}

bool FOnlineExternalUITwitch::ShowAccountUpgradeUI(const FUniqueNetId& UniqueId)
{
	return false;
}

bool FOnlineExternalUITwitch::ShowStoreUI(int32 LocalUserNum, const FShowStoreParams& ShowParams, const FOnShowStoreUIClosedDelegate& Delegate)
{
	return false;
}

bool FOnlineExternalUITwitch::ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate)
{
	return false;
}

bool FOnlineExternalUITwitch::ShowProfileUI(const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate)
{
	return false;
}
