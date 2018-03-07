// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemTwitchPrivate.h"
#include "OnlineIdentityTwitch.h"
#include "OnlineAccountTwitch.h"
#include "OnlineExternalUIInterface.h"
#include "TwitchTokenValidationResponse.h"

#include "Http.h"
#include "Misc/ConfigCacheIni.h"

bool FTwitchLoginURL::IsValid() const
{
	return !GetLoginUrl().IsEmpty() && !GetLoginRedirectUrl().IsEmpty() && !Subsystem->GetAppId().IsEmpty();
}

FString FTwitchLoginURL::GetAuthUrl(const FString& Nonce) const
{
	const bool bForceVerify(GetForceVerify());
	const FString LoginUrl(GetLoginUrl());
	const FString LoginRedirectUrl(GetLoginRedirectUrl());
	const FString StatePrefix(GetStatePrefix());
	const FString ClientId(Subsystem->GetAppId());
	const TArray<FString> ScopeFields(GetScopeFields());

	const FString Scopes = FString::Join(ScopeFields, TEXT(" "));
	FString State;
	if (!StatePrefix.IsEmpty())
	{
		State = FString::Printf(TEXT("%s-%s"), *StatePrefix, *Nonce);
	}
	else
	{
		State = Nonce;
	}

	return FString::Printf(TEXT("%s?force_verify=%s&response_type=token&client_id=%s&scope=%s&state=%s&redirect_uri=%s"),
		*LoginUrl,
		bForceVerify ? TEXT("true") : TEXT("false"),
		*FGenericPlatformHttp::UrlEncode(ClientId),
		*FGenericPlatformHttp::UrlEncode(Scopes),
		*FGenericPlatformHttp::UrlEncode(State),
		*FGenericPlatformHttp::UrlEncode(LoginRedirectUrl));
}

FString FTwitchLoginURL::ParseNonce(const FString& State)
{
	const FString DecodedState(FGenericPlatformHttp::UrlDecode(State));
	FString Nonce;
	// Get everything after the last '-'
	static const FString HyphenString(TEXT("-"));
	if (DecodedState.Split(HyphenString, nullptr, &Nonce, ESearchCase::CaseSensitive, ESearchDir::FromEnd) &&
		!Nonce.IsEmpty())
	{
		return Nonce;
	}
	// Return the url decoded State parameter
	return DecodedState;
}

bool FTwitchLoginURL::GetForceVerify() const
{
	bool bForceVerify = false;
	if (!GConfig->GetBool(TEXT("OnlineSubsystemTwitch.OnlineIdentityTwitch"), TEXT("bForceVerify"), bForceVerify, GEngineIni))
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG_ONLINE(Warning, TEXT("Missing bForceVerify= in [OnlineSubsystemTwitch.OnlineIdentityTwitch] of DefaultEngine.ini"));
		}
	}
	return bForceVerify;
}

FString FTwitchLoginURL::GetLoginUrl() const
{
	FString LoginUrl;
	if (!GConfig->GetString(TEXT("OnlineSubsystemTwitch.OnlineIdentityTwitch"), TEXT("LoginUrl"), LoginUrl, GEngineIni) ||
		LoginUrl.IsEmpty())
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG_ONLINE(Warning, TEXT("Missing LoginUrl= in [OnlineSubsystemTwitch.OnlineIdentityTwitch] of DefaultEngine.ini"));
		}
	}
	return LoginUrl;
}

FString FTwitchLoginURL::GetStatePrefix() const
{
	// See if the app has overridden the state prefix
	if (!StatePrefixOverride.IsEmpty())
	{
		return StatePrefixOverride;
	}

	FString StatePrefix;
	GConfig->GetString(TEXT("OnlineSubsystemTwitch.OnlineIdentityTwitch"), TEXT("StatePrefix"), StatePrefix, GEngineIni);
	return StatePrefix;
}

void FTwitchLoginURL::OverrideStatePrefix(const FString& InStatePrefixOverride)
{
	StatePrefixOverride = InStatePrefixOverride;
}

TArray<FString> FTwitchLoginURL::GetScopeFields() const
{
	TArray<FString> ScopeFields;
	GConfig->GetArray(TEXT("OnlineSubsystemTwitch.OnlineIdentityTwitch"), TEXT("ScopeFields"), ScopeFields, GEngineIni);
	return ScopeFields;
}

FString FTwitchLoginURL::GetLoginRedirectUrl() const
{
	FString LoginRedirectUrl;
	if (!GConfig->GetString(TEXT("OnlineSubsystemTwitch.OnlineIdentityTwitch"), TEXT("LoginRedirectUrl"), LoginRedirectUrl, GEngineIni) ||
		LoginRedirectUrl.IsEmpty())
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG_ONLINE(Warning, TEXT("Missing LoginRedirectUrl= in [OnlineSubsystemTwitch.OnlineIdentityTwitch] of DefaultEngine.ini"));
		}
	}
	return LoginRedirectUrl;
}

TSharedPtr<FUserOnlineAccountTwitch> FOnlineIdentityTwitch::GetUserAccountTwitch(const FUniqueNetId& UserId) const
{
	TSharedPtr<FUserOnlineAccountTwitch> Result;

	const TSharedRef<FUserOnlineAccountTwitch>* FoundUserAccount = UserAccounts.Find(UserId.ToString());
	if (FoundUserAccount != nullptr)
	{
		Result = *FoundUserAccount;
	}

	return Result;
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityTwitch::GetUserAccount(const FUniqueNetId& UserId) const
{
	return GetUserAccountTwitch(UserId);
}

TArray<TSharedPtr<FUserOnlineAccount> > FOnlineIdentityTwitch::GetAllUserAccounts() const
{
	TArray<TSharedPtr<FUserOnlineAccount> > Result;

	for (const TPair<FString, TSharedRef<FUserOnlineAccountTwitch> >& It : UserAccounts)
	{
		Result.Add(It.Value);
	}

	return Result;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityTwitch::GetUniquePlayerId(int32 LocalUserNum) const
{
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		const TSharedPtr<const FUniqueNetId>* FoundId = UserIds.Find(LocalUserNum);
		if (FoundId != nullptr)
		{
			return *FoundId;
		}
	}
	return nullptr;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityTwitch::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
	if (Bytes != nullptr && Size > 0)
	{
		FString StrId(Size, (TCHAR*)Bytes);
		return MakeShared<FUniqueNetIdString>(MoveTemp(StrId));
	}
	return nullptr;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityTwitch::CreateUniquePlayerId(const FString& Str)
{
	return MakeShared<FUniqueNetIdString>(Str);
}

bool FOnlineIdentityTwitch::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	FString ErrorStr;

	if (bHasLoginOutstanding)
	{
		ErrorStr = FString::Printf(TEXT("Login already pending for user"));
	}
	else if (!LoginURLDetails.IsValid())
	{
		ErrorStr = FString::Printf(TEXT("OnlineSubsystemTwitch is improperly configured in DefaultEngine.ini LoginURL=%s LoginRedirectUrl=%s ClientId=%s"),
			*LoginURLDetails.GetLoginUrl(), *LoginURLDetails.GetLoginRedirectUrl(), *Subsystem->GetAppId());
	}
	else if (LocalUserNum < 0 || LocalUserNum >= MAX_LOCAL_PLAYERS)
	{
		ErrorStr = FString::Printf(TEXT("Invalid LocalUserNum=%d"), LocalUserNum);
	}
	else
	{
		if (!AccountCredentials.Token.IsEmpty() && AccountCredentials.Type == GetAuthType())
		{
			bHasLoginOutstanding = true;

			// Validate the provided auth token and get our current scope permissions
			LoginWithAccessToken(LocalUserNum, AccountCredentials.Token, FOnLoginCompleteDelegate::CreateThreadSafeSP(this, &FOnlineIdentityTwitch::OnAccessTokenLoginComplete));
		}
		else
		{
			IOnlineExternalUIPtr OnlineExternalUI = Subsystem->GetExternalUIInterface();
			if (OnlineExternalUI.IsValid())
			{
				CurrentLoginNonce = LoginURLDetails.GenerateNonce();

				bHasLoginOutstanding = true;

				FOnLoginUIClosedDelegate CompletionDelegate = FOnLoginUIClosedDelegate::CreateThreadSafeSP(this, &FOnlineIdentityTwitch::OnExternalUILoginComplete);
				OnlineExternalUI->ShowLoginUI(LocalUserNum, true, true, CompletionDelegate);
			}
			else
			{
				ErrorStr = FString::Printf(TEXT("External interface missing"));
			}
		}
	}

	if (!ErrorStr.IsEmpty())
	{
		UE_LOG_ONLINE(Error, TEXT("Login for user %d failed: %s"), LocalUserNum, *ErrorStr);

		OnLoginAttemptComplete(LocalUserNum, ErrorStr);
		return false;
	}
	return true;
}

void FOnlineIdentityTwitch::LoginWithAccessToken(int32 LocalUserNum, const FString& AccessToken, const FOnLoginCompleteDelegate& InCompletionDelegate)
{
	// Validate the provided auth token and get our current scope permissions
	const FOnlineAccountCredentials AccountCredentials(GetAuthType(), FString(), AccessToken);
	ValidateAuthToken(LocalUserNum, AccountCredentials, FOnValidateAuthTokenComplete::CreateThreadSafeSP(this, &FOnlineIdentityTwitch::OnValidateAuthTokenComplete, InCompletionDelegate));
}

void FOnlineIdentityTwitch::OnValidateAuthTokenComplete(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials, TSharedPtr<FUserOnlineAccountTwitch> User, const FString& InErrorStr, const FOnLoginCompleteDelegate InCompletionDelegate)
{
	FString ErrorStr = InErrorStr;

	if (ErrorStr.IsEmpty())
	{
		// Confirm the specified user ID matches the auth token
		if (AccountCredentials.Id.IsEmpty() ||
			AccountCredentials.Id == User->GetUserId()->ToString())
		{
			// Confirm we have all of the scope permissions we require
			bool bHasAllPermissions = true;
			FString MissingScopeFields;
			const TArray<FString> RequiredScopeFields(LoginURLDetails.GetScopeFields());
			for (const FString& ScopeField : RequiredScopeFields)
			{
				const bool bHasPermission = User->GetScopePermissions().Contains(ScopeField);
				if (!bHasPermission)
				{
					bHasAllPermissions = false;
					if (!MissingScopeFields.IsEmpty())
					{
						MissingScopeFields += TEXT(" ");
					}
					MissingScopeFields += ScopeField;
				}
			}
			if (!MissingScopeFields.IsEmpty())
			{
				UE_LOG_ONLINE(Log, TEXT("FOnlineIdentityTwitch::OnValidateAuthTokenComplete: User %d missing scope field(s) [%s]"), LocalUserNum, *MissingScopeFields);
				ErrorStr = TWITCH_LOGIN_ERROR_MISSING_PERMISSIONS;
				ErrorStr += TEXT(" ");
				ErrorStr += MissingScopeFields;
			}
		}
		else
		{
			ErrorStr = TWITCH_LOGIN_ERROR_TOKEN_NOT_FOR_USER;
		}
	}

	if (ErrorStr.IsEmpty())
	{
		// update/add cached entry for user
		UserAccounts.Add(User->GetUserId()->ToString(), User.ToSharedRef());
		// keep track of user ids for local users
		UserIds.Add(LocalUserNum, User->GetUserId());
	}
	else
	{
		UE_LOG_ONLINE(Log, TEXT("ValidateAuthToken for user %d failed: %s"), LocalUserNum, *ErrorStr);
	}

	const bool bWasSuccessful = ErrorStr.IsEmpty();
	InCompletionDelegate.ExecuteIfBound(LocalUserNum, bWasSuccessful, bWasSuccessful ? *User->GetUserId() : *ZeroId, ErrorStr);
}

void FOnlineIdentityTwitch::ValidateAuthToken(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials, const FOnValidateAuthTokenComplete& InCompletionDelegate)
{
	// kick off http request to validate access token
	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	FString TokenValidateUrl;
	if (!GConfig->GetString(TEXT("OnlineSubsystemTwitch.OnlineIdentityTwitch"), TEXT("TokenValidateUrl"), TokenValidateUrl, GEngineIni) ||
		TokenValidateUrl.IsEmpty())
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG_ONLINE(Warning, TEXT("Missing TokenValidateUrl= in [OnlineSubsystemTwitch.OnlineIdentityTwitch] of DefaultEngine.ini"));
		}
	}
	FString Url(FString::Printf(TEXT("%s?client_id=%s"), *TokenValidateUrl, *FGenericPlatformHttp::UrlEncode(Subsystem->GetAppId())));

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineIdentityTwitch::ValidateAuthToken_HttpRequestComplete, LocalUserNum, AccountCredentials, InCompletionDelegate);
	HttpRequest->SetURL(MoveTemp(Url));
	HttpRequest->SetHeader(TEXT("Accept"), Subsystem->GetTwitchApiVersion());
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("OAuth %s"), *AccountCredentials.Token));
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->ProcessRequest();
}

void FOnlineIdentityTwitch::ValidateAuthToken_HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, int32 LocalUserNum, const FOnlineAccountCredentials AccountCredentials, const FOnValidateAuthTokenComplete InCompletionDelegate)
{
	TSharedPtr<FUserOnlineAccountTwitch> User;
	FString ErrorStr;

	if (bSucceeded &&
		HttpResponse.IsValid())
	{
		if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()) &&
			HttpResponse->GetContentType().StartsWith(TEXT("application/json")))
		{
			const FString ResponseStr = HttpResponse->GetContentAsString();
			UE_LOG_ONLINE(Verbose, TEXT("FOnlineIdentityTwitch::ValidateAuthToken_HttpRequestComplete: request complete for user %d. url=%s code=%d response=%s"),
				LocalUserNum, *HttpRequest->GetURL(), HttpResponse->GetResponseCode(), *ResponseStr);

			if (!ResponseStr.IsEmpty())
			{
				TSharedPtr<FJsonObject> JsonUser;
				const TSharedRef< TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(ResponseStr);

				if (FJsonSerializer::Deserialize(JsonReader, JsonUser) &&
					JsonUser.IsValid() &&
					JsonUser->HasTypedField<EJson::Object>(TEXT("token")))
				{
					FTwitchTokenValidationResponse ValidationResponse;
					if (ValidationResponse.FromJson(JsonUser->GetObjectField(TEXT("token"))))
					{
						if (ValidationResponse.bTokenIsValid)
						{
							User = MakeShared<FUserOnlineAccountTwitch>();
							if (User->Parse(AccountCredentials.Token, MoveTemp(ValidationResponse)))
							{
								UE_LOG_ONLINE(Log, TEXT("FOnlineIdentityTwitch::ValidateAuthToken_HttpRequestComplete: Auth token validated"));
							}
							else
							{
								UE_LOG_ONLINE(Warning, TEXT("FOnlineIdentityTwitch::ValidateAuthToken_HttpRequestComplete: Failed to initialize user. payload=%s"), *ResponseStr);
								ErrorStr = FString::Printf(TEXT("Error parsing login. payload=%s"), *ResponseStr);
							}
						}
						else
						{
							UE_LOG_ONLINE(Warning, TEXT("FOnlineIdentityTwitch::ValidateAuthToken_HttpRequestComplete: Auth token is not valid"));
							ErrorStr = TWITCH_LOGIN_ERROR_TOKEN_NOT_VALID;
						}
					}
					else
					{
						UE_LOG_ONLINE(Warning, TEXT("FOnlineIdentityTwitch::ValidateAuthToken_HttpRequestComplete: JSON response missing field 'token': payload=%s"), *ResponseStr);
						ErrorStr = TWITCH_LOGIN_ERROR_INVALID_RESPONSE;
					}
				}
				else
				{
					UE_LOG_ONLINE(Warning, TEXT("FOnlineIdentityTwitch::ValidateAuthToken_HttpRequestComplete: Failed to parse JSON response: payload=%s"), *ResponseStr);
					ErrorStr = TWITCH_LOGIN_ERROR_INVALID_RESPONSE;
				}
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("FOnlineIdentityTwitch::ValidateAuthToken_HttpRequestComplete: Empty JSON"));
				ErrorStr = TWITCH_LOGIN_ERROR_INVALID_RESPONSE;
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("FOnlineIdentityTwitch::ValidateAuthToken_HttpRequestComplete: Invalid response. code=%d contentType=%s body=%s"),
				HttpResponse->GetResponseCode(), *HttpResponse->GetContentType(), *HttpResponse->GetContentAsString());
			ErrorStr = TWITCH_LOGIN_ERROR_INVALID_RESPONSE;
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineIdentityTwitch::ValidateAuthToken_HttpRequestComplete: Request not successful"));
		ErrorStr = TWITCH_LOGIN_ERROR_REQUEST_FAILED;
	}

	InCompletionDelegate.ExecuteIfBound(LocalUserNum, AccountCredentials, User, ErrorStr);
}

void FOnlineIdentityTwitch::OnAccessTokenLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UniqueId, const FString& Error)
{
	bHasLoginOutstanding = false;
	CurrentLoginNonce.Empty();
	OnLoginAttemptComplete(LocalUserNum, Error);
}

void FOnlineIdentityTwitch::OnLoginAttemptComplete(int32 LocalUserNum, const FString& ErrorStr)
{
	TWeakPtr<FOnlineIdentityTwitch, ESPMode::ThreadSafe> WeakThisPtr(AsShared());
	const FString ErrorStrCopy(ErrorStr);
	if (GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
	{
		UE_LOG_ONLINE(Log, TEXT("Twitch login for user %d was successful."), LocalUserNum);
		TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
		check(UserId.IsValid());

		Subsystem->ExecuteNextTick([WeakThisPtr, UserId, LocalUserNum, ErrorStrCopy]()
		{
			FOnlineIdentityTwitchPtr This(WeakThisPtr.Pin());
			if (This.IsValid())
			{
				This->TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserId, ErrorStrCopy);
				This->TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::NotLoggedIn, ELoginStatus::LoggedIn, *UserId);
			}
		});
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("Twitch login for user %d failed: %s"), LocalUserNum, *ErrorStr);
		Subsystem->ExecuteNextTick([WeakThisPtr, LocalUserNum, ErrorStrCopy]()
		{
			FOnlineIdentityTwitchPtr This(WeakThisPtr.Pin());
			if (This.IsValid())
			{
				This->TriggerOnLoginCompleteDelegates(LocalUserNum, false, *This->ZeroId, ErrorStrCopy);
			}
		});
	}
}

void FOnlineIdentityTwitch::OnExternalUILoginComplete(TSharedPtr<const FUniqueNetId> UniqueId, const int ControllerIndex)
{
	FString ErrorStr;
	bool bWasSuccessful = UniqueId.IsValid() && UniqueId->IsValid();
	OnAccessTokenLoginComplete(ControllerIndex, bWasSuccessful, bWasSuccessful ? *UniqueId : *ZeroId, ErrorStr);
}

bool FOnlineIdentityTwitch::Logout(int32 LocalUserNum)
{
	TWeakPtr<FOnlineIdentityTwitch, ESPMode::ThreadSafe> WeakThisPtr(AsShared());
	bool bResult = false;
	TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		Subsystem->ExecuteNextTick([WeakThisPtr, UserId]()
		{
			FOnlineIdentityTwitchPtr This(WeakThisPtr.Pin());
			if (This.IsValid())
			{
				This->OnTwitchLogoutComplete(*UserId);
			}
		});
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("No logged in user found for LocalUserNum=%d"), LocalUserNum);
		Subsystem->ExecuteNextTick([WeakThisPtr, LocalUserNum]()
		{
			FOnlineIdentityTwitchPtr This(WeakThisPtr.Pin());
			if (This.IsValid())
			{
				This->TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
			}
		});
	}
	return bResult;
}

void FOnlineIdentityTwitch::OnTwitchLogoutComplete(const FUniqueNetId& UserId)
{
	const FString UserIdString(UserId.ToString());
	UE_LOG_ONLINE(Log, TEXT("Twitch logout for user %s complete"), *UserIdString);

	if (UserAccounts.Contains(UserIdString))
	{
		const int32 LocalUserNum = GetLocalUserNumberFromUserId(UserId);
		ensure(LocalUserNum != INDEX_NONE); // Shouldn't be in UserAccounts if we don't have a user number
		// remove cached user account
		UserAccounts.Remove(UserIdString);
		// remove cached user id
		UserIds.Remove(LocalUserNum);

		TArray<FString> LoginDomains;
		GConfig->GetArray(TEXT("OnlineSubsystemTwitch.OnlineIdentityTwitch"), TEXT("LoginDomains"), LoginDomains, GEngineIni);

		TriggerOnLoginFlowLogoutDelegates(LoginDomains);

		TSharedRef<const FUniqueNetId> UserIdRef(UserId.AsShared());
		TWeakPtr<FOnlineIdentityTwitch, ESPMode::ThreadSafe> WeakThisPtr(AsShared());
		Subsystem->ExecuteNextTick([WeakThisPtr, UserIdRef, LocalUserNum]()
		{
			FOnlineIdentityTwitchPtr This(WeakThisPtr.Pin());
			if (This.IsValid())
			{
				This->TriggerOnLogoutCompleteDelegates(LocalUserNum, true);
				This->TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::LoggedIn, ELoginStatus::NotLoggedIn, *This->ZeroId);
			}
		});
	}
	else
	{
		UE_LOG_ONLINE(Log, TEXT("FOnlineIdentityTwitch::OnTwitchLogoutComplete: Missing user %s"), *UserIdString);
	}
}

void FOnlineIdentityTwitch::RevokeAuthToken(const FUniqueNetId& UserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate)
{
	TSharedPtr<FUserOnlineAccountTwitch> FoundUserAccount = GetUserAccountTwitch(UserId);
	if (FoundUserAccount.IsValid())
	{
		const FString AuthToken = FoundUserAccount->GetAccessToken();
		RevokeAuthTokenInternal(UserId, AuthToken, Delegate);
	}
	else
	{
		if (Delegate.IsBound())
		{
			TSharedRef<const FUniqueNetId> UserIdRef(UserId.AsShared());
			Subsystem->ExecuteNextTick([UserIdRef, Delegate]()
			{
				Delegate.Execute(*UserIdRef, FOnlineError(FString(TEXT("User not found"))));
			});
		}
	}
}

void FOnlineIdentityTwitch::RevokeAuthTokenInternal(const FUniqueNetId& UserId, const FString& AuthToken, const FOnRevokeAuthTokenCompleteDelegate& InCompletionDelegate)
{
	// kick off http request to validate access token
	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	FString TokenRevokeUrl;
	if (!GConfig->GetString(TEXT("OnlineSubsystemTwitch.OnlineIdentityTwitch"), TEXT("TokenRevokeUrl"), TokenRevokeUrl, GEngineIni) ||
		TokenRevokeUrl.IsEmpty())
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG_ONLINE(Warning, TEXT("Missing TokenRevokeUrl= in [OnlineSubsystemTwitch.OnlineIdentityTwitch] of DefaultEngine.ini"));
		}
	}
	FString PostData(FString::Printf(TEXT("client_id=%s&token=%s"), *FGenericPlatformHttp::UrlEncode(Subsystem->GetAppId()), *FGenericPlatformHttp::UrlEncode(AuthToken)));

	TSharedRef<const FUniqueNetId> UserIdRef(UserId.AsShared());
	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineIdentityTwitch::RevokeAuthToken_HttpRequestComplete, UserIdRef, InCompletionDelegate);
	HttpRequest->SetURL(MoveTemp(TokenRevokeUrl));
	HttpRequest->SetHeader(TEXT("Accept"), Subsystem->GetTwitchApiVersion());
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetContentAsString(PostData);
	HttpRequest->ProcessRequest();
}

void FOnlineIdentityTwitch::RevokeAuthToken_HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, TSharedRef<const FUniqueNetId> UserId, const FOnRevokeAuthTokenCompleteDelegate InCompletionDelegate)
{
	FOnlineError OnlineError;

	if (bSucceeded &&
		HttpResponse.IsValid())
	{
		const FString ResponseStr = HttpResponse->GetContentAsString();
		if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()) &&
			HttpResponse->GetContentType().StartsWith(TEXT("application/json")))
		{
			OnlineError.bSucceeded = true;
			UE_LOG_ONLINE(Verbose, TEXT("Revoke auth token request complete for user %s. url=%s code=%d response=%s"),
				*UserId->ToString(), *HttpRequest->GetURL(), HttpResponse->GetResponseCode(), *ResponseStr);
		}
		else
		{
			OnlineError.SetFromErrorCode(FString::Printf(TEXT("Invalid response. code=%d contentType=%s response=%s"),
				HttpResponse->GetResponseCode(), *HttpResponse->GetContentType(), *ResponseStr));
		}
	}
	else
	{
		OnlineError.SetFromErrorCode(TEXT("No response"));
	}
	
	if (OnlineError.bSucceeded)
	{
		UE_LOG_ONLINE(Log, TEXT("User %s successfully revoked their auth token"), *UserId->ToString());
	}
	else
	{
		UE_LOG_ONLINE(Log, TEXT("User %s failed to revoke their auth token with error %s"), *UserId->ToString(), *OnlineError.ErrorCode);
	}
	
	InCompletionDelegate.ExecuteIfBound(*UserId, OnlineError);
	// Log out the user
	OnTwitchLogoutComplete(*UserId);
}

int32 FOnlineIdentityTwitch::GetLocalUserNumberFromUserId(const FUniqueNetId& UserId) const
{
	int32 LocalUserNum = INDEX_NONE;
	for (const TPair<int32, TSharedPtr<const FUniqueNetId>>& UserInfo : UserIds)
	{
		if (*UserInfo.Value == UserId)
		{
			LocalUserNum = UserInfo.Key;
			break;
		}
	}
	return LocalUserNum;
}

bool FOnlineIdentityTwitch::AutoLogin(int32 LocalUserNum)
{
	return false;
}

ELoginStatus::Type FOnlineIdentityTwitch::GetLoginStatus(int32 LocalUserNum) const
{
	TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		return GetLoginStatus(*UserId);
	}
	return ELoginStatus::NotLoggedIn;
}

ELoginStatus::Type FOnlineIdentityTwitch::GetLoginStatus(const FUniqueNetId& UserId) const
{
	TSharedPtr<FUserOnlineAccount> UserAccount = GetUserAccount(UserId);
	if (UserAccount.IsValid() &&
		UserAccount->GetUserId()->IsValid() &&
		!UserAccount->GetAccessToken().IsEmpty())
	{
		return ELoginStatus::LoggedIn;
	}
	return ELoginStatus::NotLoggedIn;
}

FString FOnlineIdentityTwitch::GetPlayerNickname(int32 LocalUserNum) const
{
	TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		return GetPlayerNickname(*UserId);
	}
	return TEXT("InvalidTwitchUser");
}

FString FOnlineIdentityTwitch::GetPlayerNickname(const FUniqueNetId& UserId) const
{
	// display name will be cached for users that registered or logged in manually
	const TSharedRef<FUserOnlineAccountTwitch>* FoundUserAccount = UserAccounts.Find(UserId.ToString());
	if (FoundUserAccount != nullptr)
	{
		FString DisplayName = (*FoundUserAccount)->GetDisplayName();
		if (!DisplayName.IsEmpty())
		{
			return DisplayName;
		}
	}
	return TEXT("InvalidTwitchUser");
}

FString FOnlineIdentityTwitch::GetAuthToken(int32 LocalUserNum) const
{
	FString AuthToken;

	TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		TSharedPtr<FUserOnlineAccountTwitch> FoundUserAccount = GetUserAccountTwitch(*UserId);
		if (FoundUserAccount.IsValid())
		{
			AuthToken = FoundUserAccount->GetAccessToken();
		}
	}

	return AuthToken;
}

FOnlineIdentityTwitch::FOnlineIdentityTwitch(FOnlineSubsystemTwitch* InSubsystem)
	: Subsystem(InSubsystem)
	, LoginURLDetails(InSubsystem)
	, bHasLoginOutstanding(false)
	, ZeroId(MakeShared<FUniqueNetIdString>())
{
	check(InSubsystem);
}

FPlatformUserId FOnlineIdentityTwitch::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const
{
	for (int PlayerIdx = 0; PlayerIdx < MAX_LOCAL_PLAYERS; ++PlayerIdx)
	{
		TSharedPtr<const FUniqueNetId> CurrentUniqueId = GetUniquePlayerId(PlayerIdx);
		if (CurrentUniqueId.IsValid() && (*CurrentUniqueId == UniqueNetId))
		{
			return PlayerIdx;
		}
	}

	return PLATFORMUSERID_NONE;
}

void FOnlineIdentityTwitch::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate)
{
	Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::NoFailures);
}

FString FOnlineIdentityTwitch::GetAuthType() const
{
	return TEXT("twitch");
}

void FOnlineIdentityTwitch::SetStatePrefix(const FString& StatePrefix)
{
	UE_LOG_ONLINE(Log, TEXT("FOnlineIdentityTwitch::SetStatePrefix: Setting StatePrefix to %s"), *StatePrefix);
	LoginURLDetails.OverrideStatePrefix(StatePrefix);
}
