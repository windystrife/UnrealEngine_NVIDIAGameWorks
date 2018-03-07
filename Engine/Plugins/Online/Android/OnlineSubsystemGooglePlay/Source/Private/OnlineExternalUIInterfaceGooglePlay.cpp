// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineExternalUIInterfaceGooglePlay.h"
#include "AndroidRuntimeSettings.h"
#include "UObject/Class.h"
#include "OnlineSubsystemGooglePlay.h"

THIRD_PARTY_INCLUDES_START
#include "gpg/achievement_manager.h"
#include "gpg/leaderboard_manager.h"
THIRD_PARTY_INCLUDES_END

FOnlineExternalUIGooglePlay::FOnlineExternalUIGooglePlay(FOnlineSubsystemGooglePlay* InSubsystem)
	: Subsystem(InSubsystem)
{
	check(Subsystem != nullptr);
}

bool FOnlineExternalUIGooglePlay::ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate)
{
	Subsystem->StartShowLoginUITask(ControllerIndex, Delegate);	
	return true;
}

bool FOnlineExternalUIGooglePlay::ShowFriendsUI(int32 LocalUserNum)
{
	return false;
}

bool FOnlineExternalUIGooglePlay::ShowInviteUI(int32 LocalUserNum, FName SessionName)
{
	return false;
}

bool FOnlineExternalUIGooglePlay::ShowAchievementsUI(int32 LocalUserNum) 
{
	if (Subsystem->GetGameServices() == nullptr)
	{
		return false;
	}

	if (!Subsystem->GetGameServices()->IsAuthorized())
	{
		return false;
	}

	Subsystem->GetGameServices()->Achievements().ShowAllUI(nullptr);
	return true;
}

bool FOnlineExternalUIGooglePlay::ShowLeaderboardUI(const FString& LeaderboardName)
{
	if (Subsystem->GetGameServices() == nullptr)
	{
		return false;
	}

	if (!Subsystem->GetGameServices()->IsAuthorized())
	{
		return false;
	}

	auto DefaultSettings = GetDefault<UAndroidRuntimeSettings>();

	for(const auto& Mapping : DefaultSettings->LeaderboardMap)
	{
		if(Mapping.Name == LeaderboardName)
		{
			auto ConvertedId = FOnlineSubsystemGooglePlay::ConvertFStringToStdString(Mapping.LeaderboardID);
			Subsystem->GetGameServices()->Leaderboards().ShowUI(ConvertedId, nullptr);

			return true;
		}
	}
	
	return false;
}

bool FOnlineExternalUIGooglePlay::ShowWebURL(const FString& Url, const FShowWebUrlParams& ShowParams, const FOnShowWebUrlClosedDelegate& Delegate)
{
	return false;
}

bool FOnlineExternalUIGooglePlay::CloseWebURL()
{
	return false;
}

bool FOnlineExternalUIGooglePlay::ShowProfileUI( const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate )
{
	return false;
}

bool FOnlineExternalUIGooglePlay::ShowAccountUpgradeUI(const FUniqueNetId& UniqueId)
{
	return false;
}

bool FOnlineExternalUIGooglePlay::ShowStoreUI(int32 LocalUserNum, const FShowStoreParams& ShowParams, const FOnShowStoreUIClosedDelegate& Delegate)
{
	return false;
}

bool FOnlineExternalUIGooglePlay::ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate)
{
	return false;
}