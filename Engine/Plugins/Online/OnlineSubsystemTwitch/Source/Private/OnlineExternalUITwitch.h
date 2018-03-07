// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemTwitch.h"
#include "OnlineSubsystemTwitchPackage.h"
#include "OnlineExternalUIInterface.h"

class FOnlineSubsystemTwitch;
class IHttpRequest;
struct FTwitchLoginURL;

/** 
 * Implementation for the Twitch external UIs
 */
class ONLINESUBSYSTEMTWITCH_API FOnlineExternalUITwitch : 
	public IOnlineExternalUI,
	public TSharedFromThis<FOnlineExternalUITwitch, ESPMode::ThreadSafe>
{
PACKAGE_SCOPE:

	/** 
	 * Constructor
	 * @param InSubsystem The owner of this external UI interface.
	 */
	FOnlineExternalUITwitch(FOnlineSubsystemTwitch* InSubsystem)
		: TwitchSubsystem(InSubsystem)
	{
	}

	/** Reference to the owning subsystem */
	FOnlineSubsystemTwitch* TwitchSubsystem;

public:

	/**
	 * Destructor.
	 */
	virtual ~FOnlineExternalUITwitch() = default;

	// IOnlineExternalUI
	virtual bool ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate = FOnLoginUIClosedDelegate()) override;
	virtual bool ShowFriendsUI(int32 LocalUserNum) override;
	virtual bool ShowInviteUI(int32 LocalUserNum, FName SessionName = NAME_GameSession) override;
	virtual bool ShowAchievementsUI(int32 LocalUserNum) override;
	virtual bool ShowLeaderboardUI(const FString& LeaderboardName) override;
	virtual bool ShowWebURL(const FString& Url, const FShowWebUrlParams& ShowParams, const FOnShowWebUrlClosedDelegate& Delegate = FOnShowWebUrlClosedDelegate()) override;
	virtual bool CloseWebURL() override;
	virtual bool ShowProfileUI(const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate = FOnProfileUIClosedDelegate()) override;
	virtual bool ShowAccountUpgradeUI(const FUniqueNetId& UniqueId) override;
	virtual bool ShowStoreUI(int32 LocalUserNum, const FShowStoreParams& ShowParams, const FOnShowStoreUIClosedDelegate& Delegate = FOnShowStoreUIClosedDelegate()) override;
	virtual bool ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate = FOnShowSendMessageUIClosedDelegate()) override;

private: 

	/**
	 * Parse a successful URL redirect from Twitch
	 *
	 * @param URLDetails URL config settings
	 * @param RedirectURL an URL knowns to start with the redirect URI
	 * @return login flow result populated with token or error
	 */
	FLoginFlowResult ParseRedirectResult(const FTwitchLoginURL& URLDetails, const FString& RedirectURL);

	/**
	 * Delegate fired when redirect URLs from the login flow are passed back for parsing
	 * We are looking for the success or error completion state from Twitch to grab the access token or complete the flow
	 *
	 * @param RedirectURL URL received from the login flow for parsing
	 * @return login flow result populated with token or error
	 */
	FLoginFlowResult OnLoginRedirectURL(const FString& RedirectURL);

	/** 
	 * Called when login flow or console ShowWebURL completes
	 *
	 * @param Result final result of the login flow action
	 * @param ControllerIndex index of the local user initiating the request
	 * @param Delegate UI closed delegate to fire, completing the external UIs part in the login process
	 */
	void OnLoginUIComplete(const FLoginFlowResult& Result, int ControllerIndex, const FOnLoginUIClosedDelegate& Delegate);

	/**
	 * Delegate fired when the login flow is complete
	 *
	 * @param Result final result of the login flow action
	 * @param ControllerIndex index of the local user initiating the request
	 * @param Delegate UI closed delegate to fire, completing the external UIs part in the login process
	 */
	void OnExternalLoginFlowComplete(const FLoginFlowResult& Result, int ControllerIndex, const FOnLoginUIClosedDelegate Delegate);

	/**
	 * Delegate fired when the console's ShowWebURL completes
	 *
	 * @param FinalUrl the url that was used as the final redirect before closing
	 * @param ControllerIndex index of the local user initiating the request
	 * @param Delegate UI closed delegate to fire, completing the external UIs part in the login process
	 */
	void OnConsoleShowWebUrlComplete(const FString& FinalUrl, int ControllerIndex, const FOnLoginUIClosedDelegate Delegate);

	/**
	 * Delegate fired when the Twitch identity interface has completed login using the token retrieved from the login flow
	 *
	 * @param LocalUserNum index of the local user initiating the request
	 * @param bWasSuccessful was the login call successful
	 * @param UserId user id of the logged in user, or null if login failed
	 * @param Error error string if applicable
	 * @param Delegate UI closed delegate to fire, completing the external UIs part in the login process
	 */
	void OnAccessTokenLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error, FOnLoginUIClosedDelegate Delegate);
};

typedef TSharedPtr<FOnlineExternalUITwitch, ESPMode::ThreadSafe> FOnlineExternalUITwitchPtr;

