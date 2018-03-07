// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineDelegateMacros.h"

#include "OnlineExternalUIInterface.h"
#include "OnlineSubsystemGameCirclePackage.h"

#include "OnlineAGSGameCircleClientCallbacks.h"

class FOnlineSubsystemGameCircle;

/** 
 * Interface definition for the online services external UIs
 * Any online service that provides extra UI overlays will implement the relevant functions
 */
class FOnlineExternalUIGameCircle :
	public IOnlineExternalUI
{
public:
	FOnlineExternalUIGameCircle(FOnlineSubsystemGameCircle* InSubsystem);

	virtual bool ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate = FOnLoginUIClosedDelegate()) override;
	virtual bool ShowFriendsUI(int32 LocalUserNum) override;
	virtual bool ShowInviteUI(int32 LocalUserNum, FName SessionMame) override;
	virtual bool ShowAchievementsUI(int32 LocalUserNum) override;
	virtual bool ShowLeaderboardUI(const FString& LeaderboardName) override;
	virtual bool ShowWebURL(const FString& Url, const FShowWebUrlParams& ShowParams, const FOnShowWebUrlClosedDelegate& Delegate = FOnShowWebUrlClosedDelegate()) override;
	virtual bool CloseWebURL() override;
	virtual bool ShowProfileUI(const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate = FOnProfileUIClosedDelegate()) override;
	virtual bool ShowAccountUpgradeUI(const FUniqueNetId& UniqueId) override;
	virtual bool ShowStoreUI(int32 LocalUserNum, const FShowStoreParams& ShowParams, const FOnShowStoreUIClosedDelegate& Delegate = FOnShowStoreUIClosedDelegate()) override;
	virtual bool ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate = FOnShowSendMessageUIClosedDelegate()) override;

PACKAGE_SCOPE:

	void GameActivityOnResume();

private:

	FOnlineSubsystemGameCircle* Subsystem;
	
	FOnlineShowSignInPageCallback ShowSignInPageCB;

	FOnLoginUIClosedDelegate ShowLoginDelegate;
	int	ShowLoginControllerIndex;
};

typedef TSharedPtr<FOnlineExternalUIGameCircle, ESPMode::ThreadSafe> FOnlineExternalUIGameCirclePtr;

