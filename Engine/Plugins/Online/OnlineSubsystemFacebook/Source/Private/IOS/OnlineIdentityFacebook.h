// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
 
// Module includes
#include "OnlineIdentityFacebookCommon.h"
#include "OnlineSharingFacebook.h"
#include "OnlineAccountFacebookCommon.h"
#include "OnlineSubsystemFacebookPackage.h"

#include "Interfaces/IHttpRequest.h"

class FOnlineSubsystemFacebook;
@class FBSDKAccessToken;
@class FBSDKProfile;
@class FFacebookHelper;

/** iOS implementation of a Facebook user account */
class FUserOnlineAccountFacebook : public FUserOnlineAccountFacebookCommon
{
public:

	explicit FUserOnlineAccountFacebook(const FString& InUserId = FString(), const FString& InAuthTicket = FString())
		: FUserOnlineAccountFacebookCommon(InUserId, InAuthTicket)
	{
	}

	virtual ~FUserOnlineAccountFacebook()
	{
	}

	/**
	 * Parse FBSDKAccessToken data into the user account
	 *
	 * @param AccessToken current access token, can't be null
	 */
	void Parse(const FBSDKAccessToken* AccessToken);

	/**
	 * Parse FBSDKProfile data into the user account
	 *
	 * @param InProfile current profile data from the SDK
	 */
	void Parse(const FBSDKProfile* InProfile);

	using FUserOnlineAccountFacebookCommon::Parse;
	friend class FOnlineIdentityFacebook;
};

/**
 * Facebook service implementation of the online identity interface
 */
class FOnlineIdentityFacebook :
	public FOnlineIdentityFacebookCommon
{

public:

	//~ Begin IOnlineIdentity Interface	
	virtual bool Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials) override;
	virtual bool Logout(int32 LocalUserNum) override;
	//~ End IOnlineIdentity Interface

public:

	/**
	 * Default constructor
	 */
	FOnlineIdentityFacebook(FOnlineSubsystemFacebook* InSubsystem);

	/**
	 * Destructor
	 */
	virtual ~FOnlineIdentityFacebook()
	{
	}

PACKAGE_SCOPE:

	/**	
	 * Initialize the interface
	 * 
	 * @return true if successful, false if there was an issue
	 */
	bool Init();
	/** Shutdown the interface */
	void Shutdown();

	/**
	 * Login user to Facebook, given a valid access token
	 *
	 * @param LocalUserNum local id of the requesting user
	 * @param AccessToken opaque Facebook supplied access token
	 */
	void Login(int32 LocalUserNum, const FString& AccessToken);

private:

	/**
	 * Delegate called when current permission request completes
	 *
	 * @param LocalUserNum user that made the request
	 * @param bWasSuccesful was the request successful
	 * @param NewPermissions array of all known permissions
	 */
	void OnRequestCurrentPermissionsComplete(int32 LocalUserNum, bool bWasSuccessful, const TArray<FSharingPermission>& NewPermissions);

	/**
	 * Generic callback for all attempts at login, called to end the attempt
	 *
	 * @param local id of the requesting user
	 * @param ErrorStr any error as a result of the login attempt
	 */
	void OnLoginAttemptComplete(int32 LocalUserNum, const FString& ErrorStr);

	/**
	 * Callback from the SDK that the FBSDKAccessToken has changed
	 *
	 * @param OldToken previous access token, possibly null
	 * @param NewToken current access token, possibly null
	 */
	void OnFacebookTokenChange(FBSDKAccessToken* OldToken, FBSDKAccessToken* NewToken);

	/**
	 * Callback from the SDK when the UserId has changed
	 */
	void OnFacebookUserIdChange();

	/**
	 * Callback from the SDK when the FBSDKProfile data has changed
	 *
	 * @param OldProfile previous profile, possibly null
	 * @param NewProfile current profile, possibly null
	 */
	void OnFacebookProfileChange(FBSDKProfile* OldProfile, FBSDKProfile* NewProfile);

	/** ObjC helper for access to SDK methods and callbacks */
	FFacebookHelper* FacebookHelper;

	/** The current state of our login */
	ELoginStatus::Type LoginStatus;

	/** Config based list of permission scopes to use when logging in */
	TArray<FString> ScopeFields;
};

typedef TSharedPtr<FOnlineIdentityFacebook, ESPMode::ThreadSafe> FOnlineIdentityFacebookPtr;
