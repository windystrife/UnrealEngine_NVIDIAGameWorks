// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineExternalUIInterfaceGoogle.h"
#include "OnlineSubsystemGoogle.h"
#include "OnlineIdentityGoogle.h"
#include "OnlineError.h"

#define GOOGLE_STATE_TOKEN TEXT("state")
#define GOOGLE_ACCESS_TOKEN TEXT("code")
#define GOOGLE_ERRORCODE_TOKEN TEXT("error")
#define GOOGLE_ERRORCODE_DENY TEXT("access_denied")
#define LOGIN_ERROR_CANCEL TEXT("com.epicgames.login.canceled")
#define LOGIN_ERROR_UNKNOWN TEXT("com.epicgames.login.unknown")


bool FOnlineExternalUIGoogle::ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate)
{
	bool bStarted = false;
	if (ControllerIndex >= 0 && ControllerIndex < MAX_LOCAL_PLAYERS)
	{
		FOnlineIdentityGooglePtr IdentityInt = StaticCastSharedPtr<FOnlineIdentityGoogle>(GoogleSubsystem->GetIdentityInterface());
		if (IdentityInt.IsValid())
		{
			const FGoogleLoginURL& URLDetails = IdentityInt->GetLoginURLDetails();
			if (URLDetails.IsValid())
			{
				const FString RequestedURL = URLDetails.GetURL();
				bool bShouldContinueLoginFlow = false;
				FOnLoginRedirectURL OnRedirectURLDelegate = FOnLoginRedirectURL::CreateRaw(this, &FOnlineExternalUIGoogle::OnLoginRedirectURL);
				FOnLoginFlowComplete OnExternalLoginFlowCompleteDelegate = FOnLoginFlowComplete::CreateRaw(this, &FOnlineExternalUIGoogle::OnExternalLoginFlowComplete, ControllerIndex, Delegate);
				TriggerOnLoginFlowUIRequiredDelegates(RequestedURL, OnRedirectURLDelegate, OnExternalLoginFlowCompleteDelegate, bShouldContinueLoginFlow);
				bStarted = bShouldContinueLoginFlow;
			}
		}
	}

	if (!bStarted)
	{
		GoogleSubsystem->ExecuteNextTick([ControllerIndex, Delegate]()
		{
			Delegate.ExecuteIfBound(nullptr, ControllerIndex);
		});
	}

	return bStarted;
}

FLoginFlowResult FOnlineExternalUIGoogle::OnLoginRedirectURL(const FString& RedirectURL)
{
	FLoginFlowResult Result;
	FOnlineIdentityGooglePtr IdentityInt = StaticCastSharedPtr<FOnlineIdentityGoogle>(GoogleSubsystem->GetIdentityInterface());
	if (IdentityInt.IsValid())
	{  
		const FGoogleLoginURL& URLDetails = IdentityInt->GetLoginURLDetails();
		if (URLDetails.IsValid())
		{
			// Wait for the RedirectURI to appear
			if (!RedirectURL.Contains(FPlatformHttp::UrlEncode(URLDetails.LoginUrl)) && RedirectURL.StartsWith(URLDetails.LoginRedirectUrl))
			{
				TMap<FString, FString> ParamsMap;

				{
					FString URLPrefix;
					FString ParamsOnly;
					if (!RedirectURL.Split(TEXT("?"), &URLPrefix, &ParamsOnly))
					{
						ParamsOnly = RedirectURL;
					}

					if (ParamsOnly[ParamsOnly.Len() - 1] == TEXT('#'))
					{
						ParamsOnly[ParamsOnly.Len() - 1] = TEXT('\0');
					}

					TArray<FString> Params;
					ParamsOnly.ParseIntoArray(Params, TEXT("&"));
					for (FString& Param : Params)
					{
						FString Key, Value;
						if (Param.Split(TEXT("="), &Key, &Value))
						{
							ParamsMap.Add(Key, Value);
						}
					}
				}

				const FString* State = ParamsMap.Find(GOOGLE_STATE_TOKEN);
				if (State)
				{
					if (URLDetails.State == *State)
					{
						const FString* AccessToken = ParamsMap.Find(GOOGLE_ACCESS_TOKEN);
						if (AccessToken)
						{
							Result.Error.bSucceeded = true;
							Result.Token = *AccessToken;
						}
						else
						{
							const FString* ErrorCode = ParamsMap.Find(GOOGLE_ERRORCODE_TOKEN);
							if (ErrorCode)
							{
								if (*ErrorCode == GOOGLE_ERRORCODE_DENY)
								{
									Result.Error.ErrorRaw = LOGIN_ERROR_CANCEL;
									Result.Error.ErrorMessage = FText::FromString(LOGIN_ERROR_CANCEL);
									Result.Error.ErrorCode = TEXT("-1");
									Result.Error.ErrorMessage = NSLOCTEXT("GoogleAuth", "GoogleAuthDeny", "Google Auth Denied");
									Result.Error.NumericErrorCode = -1;
								}
								else
								{
									Result.Error.ErrorRaw = RedirectURL;
									Result.Error.ErrorCode = *ErrorCode;
									// there is no descriptive error text
									Result.Error.ErrorMessage = NSLOCTEXT("GoogleAuth", "GoogleAuthError", "Google Auth Error");
									// there is no error code
									Result.Error.NumericErrorCode = 0;
								}
							}
							else
							{
								// Set some default in case parsing fails
								Result.Error.ErrorRaw = LOGIN_ERROR_UNKNOWN;
								Result.Error.ErrorMessage = FText::FromString(LOGIN_ERROR_UNKNOWN);
								Result.Error.ErrorCode = TEXT("-2");
								Result.Error.NumericErrorCode = -2;
							}
						}
					}
				}
			}
		}
	}

	return Result;
}

void FOnlineExternalUIGoogle::OnExternalLoginFlowComplete(const FLoginFlowResult& Result, int ControllerIndex, const FOnLoginUIClosedDelegate Delegate)
{
	UE_LOG(LogOnline, Log, TEXT("OnExternalLoginFlowComplete %s"), *Result.ToDebugString());

	bool bStarted = false;
	if (Result.IsValid())
	{
		FOnlineIdentityGooglePtr IdentityInt = StaticCastSharedPtr<FOnlineIdentityGoogle>(GoogleSubsystem->GetIdentityInterface());
		if (IdentityInt.IsValid())
		{
			bStarted = true;

			FOnLoginCompleteDelegate CompletionDelegate;
			CompletionDelegate = FOnLoginCompleteDelegate::CreateRaw(this, &FOnlineExternalUIGoogle::OnAccessTokenLoginComplete, Delegate);

			FAuthTokenGoogle AuthToken(Result.Token, EGoogleExchangeToken::GoogleExchangeToken);
			IdentityInt->Login(ControllerIndex, AuthToken, CompletionDelegate);
		}
	}

	if (!bStarted)
	{
		GoogleSubsystem->ExecuteNextTick([ControllerIndex, Delegate]()
		{
			Delegate.ExecuteIfBound(nullptr, ControllerIndex);
		});
	}
}

void FOnlineExternalUIGoogle::OnAccessTokenLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error, FOnLoginUIClosedDelegate Delegate)
{
	TSharedPtr<const FUniqueNetId> StrongUserId = UserId.AsShared();
	GoogleSubsystem->ExecuteNextTick([StrongUserId, LocalUserNum, Delegate]()
	{
		Delegate.ExecuteIfBound(StrongUserId, LocalUserNum);
	});
}

