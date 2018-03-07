// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
 
#include "OnlineIdentityGoogleCommon.h"
#include "OnlineSubsystemGoogleTypes.h"
#include "OnlineAccountGoogleCommon.h"
#include "OnlineSubsystemGooglePackage.h"

class FOnlineSubsystemGoogle;

#define GOOGLE_AUTH_CANCELED	  TEXT("com.epicgames.login.canceled");

/** Tied to GoogleLogin.java */
enum class EGoogleLoginResponse : uint8
{
	/** Google Sign In SDK ok response */
	RESPONSE_OK = 0,
	/** Google Sign In  SDK user cancellation */
	RESPONSE_CANCELED = 1,
	/** Google Sign In  SDK error */
	RESPONSE_ERROR = 2,
};

inline const TCHAR* ToString(EGoogleLoginResponse Response)
{
	switch (Response)
	{
		case EGoogleLoginResponse::RESPONSE_OK:
			return TEXT("RESPONSE_OK");
		case EGoogleLoginResponse::RESPONSE_CANCELED:
			return TEXT("RESPONSE_CANCELED");
		case EGoogleLoginResponse::RESPONSE_ERROR:
			return TEXT("RESPONSE_ERROR");
	}

	return TEXT("");
}

/**
 * Delegate fired when the Google Android SDK has completed a login request
 *
 * @param InResponseCode response from the Google SDK
 * @param InAccessToken access token if the response was RESPONSE_OK
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGoogleLoginComplete, EGoogleLoginResponse /*InResponseCode*/, const FString& /*InAccessToken*/);
typedef FOnGoogleLoginComplete::FDelegate FOnGoogleLoginCompleteDelegate;

/**
 * Delegate fired when the Google Android SDK has completed a logout request
 *
 * @param InResponseCode response from the Google SDK
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoogleLogoutComplete, EGoogleLoginResponse /*InResponseCode*/);
typedef FOnGoogleLogoutComplete::FDelegate FOnGoogleLogoutCompleteDelegate;

/** Android implementation of a Google user account */
class FUserOnlineAccountGoogle : public FUserOnlineAccountGoogleCommon
{
public:

	explicit FUserOnlineAccountGoogle(const FString& InUserId = FString(), const FAuthTokenGoogle& InAuthToken = FAuthTokenGoogle())
		: FUserOnlineAccountGoogleCommon(InUserId, InAuthToken)
	{
	}

	virtual ~FUserOnlineAccountGoogle()
	{
	}
};

/**
 * Google service implementation of the online identity interface
 */
class FOnlineIdentityGoogle :
	public FOnlineIdentityGoogleCommon
{

public:
	// IOnlineIdentity
	
	virtual bool Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials) override;
	virtual bool Logout(int32 LocalUserNum) override;

	// FOnlineIdentityGoogle

	FOnlineIdentityGoogle(FOnlineSubsystemGoogle* InSubsystem);

	/**
	 * Destructor
	 */
	virtual ~FOnlineIdentityGoogle()
	{
	}

PACKAGE_SCOPE:

	/**
	 * Delegate fired internally when the Java Google SDK has completed, notifying any OSS listeners
	 * Not meant for external use
	 *
	 * @param InResponseCode response from the Google SDK
	 * @param InAccessToken access token from the Google SDK if login was successful
	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnGoogleLoginComplete, EGoogleLoginResponse /*InResponseCode*/, const FString& /*InAccessToken*/);

	/**
	 * Delegate fired internally when the Java Google SDK has completed, notifying any OSS listeners
	 * Not meant for external use
	 *
	 * @param InResponseCode response from the Google SDK
	 */
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnGoogleLogoutComplete, EGoogleLoginResponse /*InResponseCode*/);

	/** Last function called for any single login attempt */
	void OnLoginAttemptComplete(int32 LocalUserNum, const FString& ErrorStr);

	/** Generic handler for the Java SDK login callback */
	void OnLoginComplete(EGoogleLoginResponse InResponseCode, const FString& InAccessToken);
	/** Generic handler for the Java SDK logout callback */
	void OnLogoutComplete(EGoogleLoginResponse InResponseCode);
	
private:

	/** Config based list of permission scopes to use when logging in */
	TArray<FString> ScopeFields;

	FDelegateHandle OnGoogleLoginCompleteHandle;
	FDelegateHandle OnGoogleLogoutCompleteHandle;

	/** Delegate holder for all internal related login callbacks */
	DECLARE_DELEGATE_TwoParams(FOnInternalLoginComplete, EGoogleLoginResponse /*InLoginResponse*/, const FString& /*AccessToken*/);
	FOnInternalLoginComplete LoginCompletionDelegate;
	/** Delegate holder for all internal related logout callbacks */
	DECLARE_DELEGATE_OneParam(FOnInternalLogoutComplete, EGoogleLoginResponse /*InLoginResponse*/);
	FOnInternalLogoutComplete LogoutCompletionDelegate;
};

typedef TSharedPtr<FOnlineIdentityGoogle, ESPMode::ThreadSafe> FOnlineIdentityGooglePtr;