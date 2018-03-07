// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineExternalUIInterfaceGameCircle.h"
#include "AGS/AchievementsClientInterface.h"
#include "AGS/LeaderboardsClientInterface.h"
#include "Async/TaskGraphInterfaces.h"
#include "OnlineSubsystemGameCircle.h"

FOnlineExternalUIGameCircle::FOnlineExternalUIGameCircle(FOnlineSubsystemGameCircle* InSubsystem)
	: Subsystem(InSubsystem)
{
	check(Subsystem != nullptr);
}

bool FOnlineExternalUIGameCircle::ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate)
{
	if(Delegate.IsBound())
	{
		ShowLoginDelegate = FOnLoginUIClosedDelegate(Delegate);
		ShowLoginControllerIndex = ControllerIndex;
	}

	AmazonGames::GameCircleClientInterface::showSignInPage(&ShowSignInPageCB);

	return true;
}

bool FOnlineExternalUIGameCircle::ShowFriendsUI(int32 LocalUserNum)
{
	return false;
}

bool FOnlineExternalUIGameCircle::ShowInviteUI(int32 LocalUserNum, FName SessionMame)
{
	return false;
}

bool FOnlineExternalUIGameCircle::ShowAchievementsUI(int32 LocalUserNum) 
{
	AmazonGames::AchievementsClientInterface::showAchievementsOverlay();
	return true;
}

bool FOnlineExternalUIGameCircle::ShowLeaderboardUI(const FString& LeaderboardName)
{
	std::string STDLeaderboardName = FOnlineSubsystemGameCircle::ConvertFStringToStdString(LeaderboardName);

	AmazonGames::LeaderboardsClientInterface::showLeaderboardOverlay(STDLeaderboardName.c_str());
	
	return true;
}

bool FOnlineExternalUIGameCircle::ShowWebURL(const FString& Url, const FShowWebUrlParams& ShowParams, const FOnShowWebUrlClosedDelegate& Delegate)
{
	return false;
}

bool FOnlineExternalUIGameCircle::CloseWebURL()
{
	return false;
}

bool FOnlineExternalUIGameCircle::ShowProfileUI( const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate )
{
	return false;
}

bool FOnlineExternalUIGameCircle::ShowAccountUpgradeUI(const FUniqueNetId& UniqueId)
{
	return false;
}

bool FOnlineExternalUIGameCircle::ShowStoreUI(int32 LocalUserNum, const FShowStoreParams& ShowParams, const FOnShowStoreUIClosedDelegate& Delegate)
{
	return false;
}

bool FOnlineExternalUIGameCircle::ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate)
{
	return false;
}

void FOnlineExternalUIGameCircle::GameActivityOnResume()
{
	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.GameActivityOnResume"), STAT_FSimpleDelegateGraphTask_GameActivityOnResume, STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=](){

			if (ShowLoginDelegate.IsBound())
			{
				TSharedPtr<const FUniqueNetIdString> PlayerId = Subsystem->GetIdentityGameCircle()->GetCurrentUserId();
				if (!PlayerId.IsValid())
				{
					FPlatformMisc::LowLevelOutputDebugString(TEXT("PlayerId from Identity Interface is Invalid"));
					PlayerId = MakeShareable(new FUniqueNetIdString());
					Subsystem->GetIdentityGameCircle()->SetCurrentUserId(PlayerId);
				}
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Executing ShowLoginUI Delegate if safe. PlayerId - %s  Index=%d"), *PlayerId->ToString(), ShowLoginControllerIndex);
				ShowLoginDelegate.ExecuteIfBound(PlayerId, ShowLoginControllerIndex);

				ShowLoginDelegate.Unbind();
			}

		}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_GameActivityOnResume),
		nullptr, 
		ENamedThreads::GameThread
	);
}
