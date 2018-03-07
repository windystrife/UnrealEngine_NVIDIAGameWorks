// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Module includes
#include "OnlineSubsystemIOSPrivatePCH.h"
#include "OnlineIdentityInterfaceIOS.h"

FOnlineExternalUIIOS::FOnlineExternalUIIOS(FOnlineSubsystemIOS* InSubsystem)
	: Subsystem(InSubsystem)
{
	check(InSubsystem != nullptr);
}

bool FOnlineExternalUIIOS::ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate)
{
	FOnlineIdentityIOS* IdentityInterface = static_cast<FOnlineIdentityIOS*>(Subsystem->GetIdentityInterface().Get());
	
	check(IdentityInterface != nullptr);
	
	if (IdentityInterface->GetLocalGameCenterUser() == nullptr)
	{
		UE_LOG(LogOnline, Log, TEXT("Game Center localPlayer is null."));
		Delegate.ExecuteIfBound(nullptr, ControllerIndex);
		return true;
	}
	
	if (IdentityInterface->GetLocalGameCenterUser().isAuthenticated)
	{
		Delegate.ExecuteIfBound(IdentityInterface->GetLocalPlayerUniqueId(), ControllerIndex);
		return true;
	}
	
	// Not authenticated, set a handler
	
	// Copy the delegate so that the block can still access it when it runs.
	CopiedDelegate = Delegate;
	
	// add a Login Complete delegat to the Identity Interface and attempt to Login
	CompleteDelegate = IdentityInterface->AddOnLoginCompleteDelegate_Handle(ControllerIndex, FOnLoginCompleteDelegate::CreateRaw(this, &FOnlineExternalUIIOS::OnLoginComplete));
	IdentityInterface->Login(ControllerIndex, FOnlineAccountCredentials());
	
	return true;
}

bool FOnlineExternalUIIOS::ShowFriendsUI(int32 LocalUserNum)
{
	return false;
}

bool FOnlineExternalUIIOS::ShowInviteUI(int32 LocalUserNum, FName SessionName)
{
	return false;
}

bool FOnlineExternalUIIOS::ShowAchievementsUI(int32 LocalUserNum) 
{
	// Will always show the achievements UI for the current local signed-in user
	extern CORE_API void IOSShowAchievementsUI();
	IOSShowAchievementsUI();
	return true;
}

bool FOnlineExternalUIIOS::ShowLeaderboardUI( const FString& LeaderboardName )
{
	extern CORE_API void IOSShowLeaderboardUI(const FString& CategoryName);
	IOSShowLeaderboardUI(LeaderboardName);
	return true;
}

bool FOnlineExternalUIIOS::ShowWebURL(const FString& Url, const FShowWebUrlParams& ShowParams, const FOnShowWebUrlClosedDelegate& Delegate)
{
	return false;
}

bool FOnlineExternalUIIOS::CloseWebURL()
{
	return false;
}

bool FOnlineExternalUIIOS::ShowProfileUI(const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate)
{
	return false;
}

bool FOnlineExternalUIIOS::ShowAccountUpgradeUI(const FUniqueNetId& UniqueId)
{
	return false;
}

bool FOnlineExternalUIIOS::ShowStoreUI(int32 LocalUserNum, const FShowStoreParams& ShowParams, const FOnShowStoreUIClosedDelegate& Delegate)
{
	return false;
}

bool FOnlineExternalUIIOS::ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate)
{
	return false;
}

void FOnlineExternalUIIOS::OnLoginComplete(int ControllerIndex, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& ErrorString)
{
    FOnlineIdentityIOS* IdentityInterface = static_cast<FOnlineIdentityIOS*>(Subsystem->GetIdentityInterface().Get());
    TSharedPtr<FUniqueNetIdString> UniqueNetId;
    if (bWasSuccessful)
    {
        const FString PlayerId(IdentityInterface->GetLocalGameCenterUser().playerID);
        UniqueNetId = MakeShareable(new FUniqueNetIdString(PlayerId));
    }
    CopiedDelegate.ExecuteIfBound(UniqueNetId, ControllerIndex);

	check(IdentityInterface != nullptr);
	IdentityInterface->ClearOnLoginCompleteDelegate_Handle(ControllerIndex, CompleteDelegate);
}