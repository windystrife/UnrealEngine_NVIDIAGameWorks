// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemFacebookCommon.h"
#include "OnlineSubsystemFacebookPrivate.h"
#include "OnlineIdentityFacebookCommon.h"
#include "OnlineFriendsFacebookCommon.h"
#include "OnlineSharingFacebookCommon.h"
#include "OnlineUserFacebookCommon.h"
#include "OnlineExternalUIFacebookCommon.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"

FOnlineSubsystemFacebookCommon::FOnlineSubsystemFacebookCommon()
	: FacebookIdentity(nullptr)
	, FacebookFriends(nullptr)
	, FacebookSharing(nullptr)
	, FacebookUser(nullptr)
	, FacebookExternalUI(nullptr)
{
	if (!GConfig->GetString(TEXT("OnlineSubsystemFacebook"), TEXT("ClientId"), ClientId, GEngineIni))
	{
		UE_LOG(LogOnline, Warning, TEXT("Missing ClientId= in [OnlineSubsystemFacebook] of DefaultEngine.ini"));
	}
}

FOnlineSubsystemFacebookCommon::FOnlineSubsystemFacebookCommon(FName InInstanceName)
	: FOnlineSubsystemImpl(FACEBOOK_SUBSYSTEM, InInstanceName)
	, FacebookIdentity(nullptr)
	, FacebookFriends(nullptr)
	, FacebookSharing(nullptr)
	, FacebookUser(nullptr)
	, FacebookExternalUI(nullptr)
{
	if (!GConfig->GetString(TEXT("OnlineSubsystemFacebook"), TEXT("ClientId"), ClientId, GEngineIni))
	{
		UE_LOG(LogOnline, Warning, TEXT("Missing ClientId= in [OnlineSubsystemFacebook] of DefaultEngine.ini"));
	}
}

FOnlineSubsystemFacebookCommon::~FOnlineSubsystemFacebookCommon()
{
}

bool FOnlineSubsystemFacebookCommon::Init()
{
	return true;
}

bool FOnlineSubsystemFacebookCommon::Shutdown()
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineSubsystemFacebookCommon::Shutdown()"));

	FOnlineSubsystemImpl::Shutdown();

#define DESTRUCT_INTERFACE(Interface) \
	if (Interface.IsValid()) \
	{ \
		ensure(Interface.IsUnique()); \
		Interface = nullptr; \
	}

	DESTRUCT_INTERFACE(FacebookSharing);
	DESTRUCT_INTERFACE(FacebookExternalUI);
	DESTRUCT_INTERFACE(FacebookFriends);
	DESTRUCT_INTERFACE(FacebookUser);
	DESTRUCT_INTERFACE(FacebookIdentity);

#undef DESTRUCT_INTERFACE

	return true;
}

bool FOnlineSubsystemFacebookCommon::Tick(float DeltaTime)
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

	return true;
}

FString FOnlineSubsystemFacebookCommon::GetAppId() const
{
	return ClientId;
}

bool FOnlineSubsystemFacebookCommon::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FOnlineSubsystemImpl::Exec(InWorld, Cmd, Ar))
	{
		return true;
	}
	return false;
}

bool FOnlineSubsystemFacebookCommon::IsEnabled() const
{
	// Check the ini for disabling Facebook
	bool bEnableFacebook = false;
	if (!GConfig->GetBool(TEXT("OnlineSubsystemFacebook"), TEXT("bEnabled"), bEnableFacebook, GEngineIni))
	{
		UE_LOG(LogOnline, Warning, TEXT("The [OnlineSubsystemFacebook]:bEnabled flag has not been set."));
	}

	return bEnableFacebook;
}

IOnlineSessionPtr FOnlineSubsystemFacebookCommon::GetSessionInterface() const
{
	return nullptr;
}

IOnlineFriendsPtr FOnlineSubsystemFacebookCommon::GetFriendsInterface() const
{
	return FacebookFriends;
}

IOnlinePartyPtr FOnlineSubsystemFacebookCommon::GetPartyInterface() const
{
	return nullptr;
}

IOnlineGroupsPtr FOnlineSubsystemFacebookCommon::GetGroupsInterface() const
{
	return nullptr;
}

IOnlineSharedCloudPtr FOnlineSubsystemFacebookCommon::GetSharedCloudInterface() const
{
	return nullptr;
}

IOnlineUserCloudPtr FOnlineSubsystemFacebookCommon::GetUserCloudInterface() const
{
	return nullptr;
}

IOnlineLeaderboardsPtr FOnlineSubsystemFacebookCommon::GetLeaderboardsInterface() const
{
	return nullptr;
}

IOnlineVoicePtr FOnlineSubsystemFacebookCommon::GetVoiceInterface() const
{
	return nullptr;
}

IOnlineExternalUIPtr FOnlineSubsystemFacebookCommon::GetExternalUIInterface() const	
{
	return FacebookExternalUI;
}

IOnlineTimePtr FOnlineSubsystemFacebookCommon::GetTimeInterface() const
{
	return nullptr;
}

IOnlineIdentityPtr FOnlineSubsystemFacebookCommon::GetIdentityInterface() const
{
	return FacebookIdentity;
}

IOnlineTitleFilePtr FOnlineSubsystemFacebookCommon::GetTitleFileInterface() const
{
	return nullptr;
}

IOnlineEntitlementsPtr FOnlineSubsystemFacebookCommon::GetEntitlementsInterface() const
{
	return nullptr;
}

IOnlineStorePtr FOnlineSubsystemFacebookCommon::GetStoreInterface() const
{
	return nullptr;
}

IOnlineEventsPtr FOnlineSubsystemFacebookCommon::GetEventsInterface() const
{
	return nullptr;
}

IOnlineAchievementsPtr FOnlineSubsystemFacebookCommon::GetAchievementsInterface() const
{
	return nullptr;
}

IOnlineSharingPtr FOnlineSubsystemFacebookCommon::GetSharingInterface() const
{
	return FacebookSharing;
}

IOnlineUserPtr FOnlineSubsystemFacebookCommon::GetUserInterface() const
{
	return FacebookUser;
}

IOnlineMessagePtr FOnlineSubsystemFacebookCommon::GetMessageInterface() const
{
	return nullptr;
}

IOnlinePresencePtr FOnlineSubsystemFacebookCommon::GetPresenceInterface() const
{
	return nullptr;
}

IOnlineChatPtr FOnlineSubsystemFacebookCommon::GetChatInterface() const
{
	return nullptr;
}

IOnlineTurnBasedPtr FOnlineSubsystemFacebookCommon::GetTurnBasedInterface() const
{
	return nullptr;
}

FText FOnlineSubsystemFacebookCommon::GetOnlineServiceName() const
{
	return NSLOCTEXT("OnlineSubsystemFacebook", "OnlineServiceName", "Facebook");
}
