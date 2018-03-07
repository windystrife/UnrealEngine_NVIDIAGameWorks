// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineExternalUIInterface.h"
#include "OnlineSubsystemFacebookPackage.h"

class FOnlineSubsystemFacebookCommon;
class IHttpRequest;

/** 
 * Implementation for the Facebook external UIs
 */
class FOnlineExternalUIFacebookCommon : public IOnlineExternalUI
{
private:

PACKAGE_SCOPE:

	/** 
	 * Constructor
	 * @param InSubsystem The owner of this external UI interface.
	 */
	FOnlineExternalUIFacebookCommon(FOnlineSubsystemFacebookCommon* InSubsystem) :
		FacebookSubsystem(InSubsystem)
	{
	}

	/** Reference to the owning subsystem */
	FOnlineSubsystemFacebookCommon* FacebookSubsystem;

public:

	/**
	 * Destructor.
	 */
	virtual ~FOnlineExternalUIFacebookCommon()
	{
	}

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
};

typedef TSharedPtr<FOnlineExternalUIFacebookCommon, ESPMode::ThreadSafe> FOnlineExternalUIFacebookCommonPtr;

