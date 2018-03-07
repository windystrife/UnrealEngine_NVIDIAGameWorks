// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
 
#include "OnlineIdentityInterface.h"
#include "OnlineSubsystemTwitchPackage.h"
#include "Http.h"
#include "Guid.h"

typedef TSharedPtr<class IHttpRequest> FHttpRequestPtr;
typedef TSharedPtr<class IHttpResponse, ESPMode::ThreadSafe> FHttpResponsePtr;
class FUserOnlineAccountTwitch;
class FOnlineSubsystemTwitch;
typedef TMap<FString, TSharedRef<FUserOnlineAccountTwitch> > FUserOnlineAccountTwitchMap;

/** This string will be followed by space separated permissions that are missing, so use FString.StartsWith to check for this error */
#define TWITCH_LOGIN_ERROR_MISSING_PERMISSIONS TEXT("errors.com.epicgames.oss.twitch.identity.missing_permissions")
/** The specified user doesn't match the specified auth token */
#define TWITCH_LOGIN_ERROR_TOKEN_NOT_FOR_USER TEXT("errors.com.epicgames.oss.twitch.identity.token_not_for_user")
/** The provided auth token is not valid */
#define TWITCH_LOGIN_ERROR_TOKEN_NOT_VALID TEXT("errors.com.epicgames.oss.twitch.identity.token_not_valid")
/** Invalid response received from Twitch */
#define TWITCH_LOGIN_ERROR_INVALID_RESPONSE TEXT("errors.com.epicgames.oss.twitch.identity.invalid_response")
/** Http request failed */
#define TWITCH_LOGIN_ERROR_REQUEST_FAILED TEXT("errors.com.epicgames.oss.twitch.identity.request_failed")


/**
 * Contains URL details for Twitch interaction
 */
struct ONLINESUBSYSTEMTWITCH_API FTwitchLoginURL
{
	FTwitchLoginURL(FOnlineSubsystemTwitch* InSubsystem)
		: Subsystem(InSubsystem)
	{
	}

	/** 
	 * @return whether this is properly configured or not
	 */
	bool IsValid() const;

	/** 
	 * @return the auth url to spawn in the browser
	 */
	FString GetAuthUrl(const FString& Nonce) const;

	/** 
	 * @return a random nonce
	 */
	static FString GenerateNonce()
	{
		// random guid to represent client generated state for verification on login
		return FGuid::NewGuid().ToString();
	}

	/**
	 * Get the Nonce from the specified State
	 */
	static FString ParseNonce(const FString& State);

	// Settings accessors
	/** 
	 * @return the login url
	 */
	FString GetLoginUrl() const;

	/**
	 * @return the ForceVerify setting
	 */
	bool GetForceVerify() const;
	
	/** 
	 * @return the redirect url
	 */
	FString GetLoginRedirectUrl() const;

	/** 
	 * @return the state prefix to put before the nonce
	 */
	FString GetStatePrefix() const;

	/** 
	 * Override the state prefix
	 * @param InStatePrefixOverride the state prefix override
	 */
	void OverrideStatePrefix(const FString& InStatePrefixOverride);

	/**
	 * @return the scope fields we are requesting permissions for
	 */
	TArray<FString> GetScopeFields() const;

private:
	/** Owning subsystem */
	FOnlineSubsystemTwitch* Subsystem;

	/** Overridden StatePrefix */
	FString StatePrefixOverride;
};

/**
 * Twitch service implementation of the online identity interface
 */
class ONLINESUBSYSTEMTWITCH_API FOnlineIdentityTwitch :
	public IOnlineIdentity,
	public TSharedFromThis<FOnlineIdentityTwitch, ESPMode::ThreadSafe>
{
public:

	// IOnlineIdentity

	virtual bool Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials) override;
	virtual bool Logout(int32 LocalUserNum) override;
	virtual bool AutoLogin(int32 LocalUserNum) override;
	virtual TSharedPtr<FUserOnlineAccount> GetUserAccount(const FUniqueNetId& UserId) const override;
	virtual TArray<TSharedPtr<FUserOnlineAccount> > GetAllUserAccounts() const override;
	virtual TSharedPtr<const FUniqueNetId> GetUniquePlayerId(int32 LocalUserNum) const override;
	virtual TSharedPtr<const FUniqueNetId> CreateUniquePlayerId(uint8* Bytes, int32 Size) override;
	virtual TSharedPtr<const FUniqueNetId> CreateUniquePlayerId(const FString& Str) override;
	virtual ELoginStatus::Type GetLoginStatus(int32 LocalUserNum) const override;
	virtual ELoginStatus::Type GetLoginStatus(const FUniqueNetId& UserId) const override;
	virtual FString GetPlayerNickname(int32 LocalUserNum) const override;
	virtual FString GetPlayerNickname(const FUniqueNetId& UserId) const override;
	virtual FString GetAuthToken(int32 LocalUserNum) const override;
	virtual void RevokeAuthToken(const FUniqueNetId& UserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate) override;
	virtual void GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate) override;
	virtual FPlatformUserId GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const override;
	virtual FString GetAuthType() const override;

	// FOnlineIdentityTwitch
	/** 
	 * Set the state prefix
	 * @param StatePrefix the state prefix
	 */
	void SetStatePrefix(const FString& StatePrefix);

PACKAGE_SCOPE:

	/**
	 * Constructor
	 *
	 * @param InSubsystem Twitch subsystem being used
	 */
	FOnlineIdentityTwitch(FOnlineSubsystemTwitch* InSubsystem);

	/** Default constructor unavailable */
	FOnlineIdentityTwitch() = delete;

	/**
	 * Destructor
	 */
	virtual ~FOnlineIdentityTwitch() = default;

	/** @return the Twitch user account for the specified user id */
	TSharedPtr<FUserOnlineAccountTwitch> GetUserAccountTwitch(const FUniqueNetId& UserId) const;

	/** @return the login configuration details */
	const FTwitchLoginURL& GetLoginURLDetails() const { return LoginURLDetails; }

	/** @return the current login attempt's nonce */
	const FString& GetCurrentLoginNonce() const { return CurrentLoginNonce; }

PACKAGE_SCOPE:

	/**
	 * Login with an existing access token
	 *
	 * @param LocalUserNum id of the local user initiating the request
	 * @param AccessToken access token already received from Twitch
	 * @param InCompletionDelegate delegate to fire when operation completes
	 */
	void LoginWithAccessToken(int32 LocalUserNum, const FString& AccessToken, const FOnLoginCompleteDelegate& InCompletionDelegate);

private:
	/**
	 * Delegate fired after a Twitch token has been validated
	 *
	 * @param LocalUserNum the controller number of the associated user
	 * @param AccountCredentials user account credentials needed to sign in to the online service
	 * @param OnlineAccount online account if successfully parsed
	 * @param ErrorStr error associated with the request
	 */
	DECLARE_DELEGATE_FourParams(FOnValidateAuthTokenComplete, int32 /*LocalUserNum*/, const FOnlineAccountCredentials& /*AccountCredentials*/, TSharedPtr<FUserOnlineAccountTwitch> /*User*/, const FString& /*ErrorStr*/);

	/** 
	 * Validate a Twitch auth token
	 * 
	 * @param LocalUserNum the controller number of the associated user
	 * @param AccountCredentials user account credentials needed to sign in to the online service
	 * @param InCompletionDelegate delegate to fire when operation completes
	 */
	void ValidateAuthToken(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials, const FOnValidateAuthTokenComplete& InCompletionDelegate);

	/** 
	 * Delegate fired when the http request from ValidateAuthToken completes
	 */
	void ValidateAuthToken_HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, int32 LocalUserNum, const FOnlineAccountCredentials AccountCredentials, const FOnValidateAuthTokenComplete InCompletionDelegate);

	/** 
	 * Delegate fired when the call to ValidateAuthToken completes
	 */
	void OnValidateAuthTokenComplete(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials, TSharedPtr<FUserOnlineAccountTwitch> User, const FString& ErrorStr, const FOnLoginCompleteDelegate InCompletionDelegate);

	/**
	 * Delegate fired when the internal call to Login with AccessToken is specified
	 */
	void OnAccessTokenLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

	/** 
	 * Function called when call to Login completes
	 */
	void OnLoginAttemptComplete(int32 LocalUserNum, const FString& ErrorStr);
	
	/**
	 * Delegate fired when the call to ShowLoginUI completes
	 */
	void OnExternalUILoginComplete(TSharedPtr<const FUniqueNetId> UniqueId, const int ControllerIndex);

	/** 
	 * Function called after logging out has completed, or if the user revoked their auth token
	 * @param UserId the unique net of the associated user
	 */
	void OnTwitchLogoutComplete(const FUniqueNetId& UserId);

	/** 
	 * Revokes the auth token associated with an account
	 * @param UserId the unique net of the associated user
	 * @param AuthToken auth token for the user that needs to be revoked
	 * @param InCompletionDelegate delegate to fire when operation completes
	 */
	void RevokeAuthTokenInternal(const FUniqueNetId& UserId, const FString& AuthToken, const FOnRevokeAuthTokenCompleteDelegate& Delegate);

	/** 
	 * Delegate fired when the http request from RevokeAuthToken completes
	 */
	void RevokeAuthToken_HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, TSharedRef<const FUniqueNetId> UserId, const FOnRevokeAuthTokenCompleteDelegate InCompletionDelegate);

	/** 
	 * Get the controller number associated with the specified user id
	 * @param UserId the unique net of the user
	 * @return the controller number, or INDEX_NONE if not found
	 */
	int32 GetLocalUserNumberFromUserId(const FUniqueNetId& UserId) const;

	/** Users that have been registered */
	FUserOnlineAccountTwitchMap UserAccounts;
	/** Ids mapped to locally registered users */
	TMap<int32, TSharedPtr<const FUniqueNetId>> UserIds;

	/** Reference to the main subsystem */
	FOnlineSubsystemTwitch* Subsystem;

	/** Const details about communicating with twitch API */
	FTwitchLoginURL LoginURLDetails;
	/** Nonce for current login attempt */
	FString CurrentLoginNonce;
	/** Whether we have a registration in flight or not */
	bool bHasLoginOutstanding;

	/** Re-usable empty unique id for errors */
	TSharedRef<FUniqueNetId> ZeroId;
};

typedef TSharedPtr<FOnlineIdentityTwitch, ESPMode::ThreadSafe> FOnlineIdentityTwitchPtr;
