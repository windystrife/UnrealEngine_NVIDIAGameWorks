// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemGoogleCommon.h"
#include "OnlineSubsystemGooglePrivate.h"
#include "OnlineIdentityGoogleCommon.h"
#include "OnlineExternalUIGoogleCommon.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"

FOnlineSubsystemGoogleCommon::FOnlineSubsystemGoogleCommon()
	: GoogleIdentity(nullptr)
	, GoogleExternalUI(nullptr)
{
	if (!GConfig->GetString(TEXT("OnlineSubsystemGoogle"), TEXT("ClientId"), ClientId, GEngineIni))
	{
		UE_LOG(LogOnline, Warning, TEXT("Missing ClientId= in [OnlineSubsystemGoogle] of DefaultEngine.ini"));
	}

	if (!GConfig->GetString(TEXT("OnlineSubsystemGoogle"), TEXT("ServerClientId"), ServerClientId, GEngineIni))
	{
		UE_LOG(LogOnline, Warning, TEXT("Missing ServerClientId= in [OnlineSubsystemGoogle] of DefaultEngine.ini"));
	}
}

FOnlineSubsystemGoogleCommon::FOnlineSubsystemGoogleCommon(FName InInstanceName)
	: FOnlineSubsystemImpl(GOOGLE_SUBSYSTEM, InInstanceName)
{
	if (!GConfig->GetString(TEXT("OnlineSubsystemGoogle"), TEXT("ClientId"), ClientId, GEngineIni))
	{
		UE_LOG(LogOnline, Warning, TEXT("Missing ClientId= in [OnlineSubsystemGoogle] of DefaultEngine.ini"));
	}

	if (!GConfig->GetString(TEXT("OnlineSubsystemGoogle"), TEXT("ServerClientId"), ServerClientId, GEngineIni))
	{
		UE_LOG(LogOnline, Warning, TEXT("Missing ServerClientId= in [OnlineSubsystemGoogle] of DefaultEngine.ini"));
	}
}

FOnlineSubsystemGoogleCommon::~FOnlineSubsystemGoogleCommon()
{
}

bool FOnlineSubsystemGoogleCommon::Init()
{
	return true;
}

bool FOnlineSubsystemGoogleCommon::Shutdown()
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineSubsystemGoogleCommon::Shutdown()"));

	FOnlineSubsystemImpl::Shutdown();

#define DESTRUCT_INTERFACE(Interface) \
	if (Interface.IsValid()) \
	{ \
		ensure(Interface.IsUnique()); \
		Interface = nullptr; \
	}

	DESTRUCT_INTERFACE(GoogleExternalUI);
	DESTRUCT_INTERFACE(GoogleIdentity);

#undef DESTRUCT_INTERFACE

	return true;
}

bool FOnlineSubsystemGoogleCommon::Tick(float DeltaTime)
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

	return true;
}

FString FOnlineSubsystemGoogleCommon::GetAppId() const
{
	return ClientId;
}

bool FOnlineSubsystemGoogleCommon::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FOnlineSubsystemImpl::Exec(InWorld, Cmd, Ar))
	{
		return true;
	}
	return false;
}

bool FOnlineSubsystemGoogleCommon::IsEnabled() const
{
	// Check the ini for disabling Google
	bool bEnableGoogle = false;
	if (!GConfig->GetBool(TEXT("OnlineSubsystemGoogle"), TEXT("bEnabled"), bEnableGoogle, GEngineIni))
	{
		UE_LOG(LogOnline, Warning, TEXT("The [OnlineSubsystemGoogle]:bEnabled flag has not been set."));
	}

	return bEnableGoogle;
}

IOnlineSessionPtr FOnlineSubsystemGoogleCommon::GetSessionInterface() const
{
	return nullptr;
}

IOnlineFriendsPtr FOnlineSubsystemGoogleCommon::GetFriendsInterface() const
{
	return nullptr;
}

IOnlinePartyPtr FOnlineSubsystemGoogleCommon::GetPartyInterface() const
{
	return nullptr;
}

IOnlineGroupsPtr FOnlineSubsystemGoogleCommon::GetGroupsInterface() const
{
	return nullptr;
}

IOnlineSharedCloudPtr FOnlineSubsystemGoogleCommon::GetSharedCloudInterface() const
{
	return nullptr;
}

IOnlineUserCloudPtr FOnlineSubsystemGoogleCommon::GetUserCloudInterface() const
{
	return nullptr;
}

IOnlineLeaderboardsPtr FOnlineSubsystemGoogleCommon::GetLeaderboardsInterface() const
{
	return nullptr;
}

IOnlineVoicePtr FOnlineSubsystemGoogleCommon::GetVoiceInterface() const
{
	return nullptr;
}

IOnlineExternalUIPtr FOnlineSubsystemGoogleCommon::GetExternalUIInterface() const	
{
	return GoogleExternalUI;
}

IOnlineTimePtr FOnlineSubsystemGoogleCommon::GetTimeInterface() const
{
	return nullptr;
}

IOnlineIdentityPtr FOnlineSubsystemGoogleCommon::GetIdentityInterface() const
{
	return GoogleIdentity;
}

IOnlineTitleFilePtr FOnlineSubsystemGoogleCommon::GetTitleFileInterface() const
{
	return nullptr;
}

IOnlineEntitlementsPtr FOnlineSubsystemGoogleCommon::GetEntitlementsInterface() const
{
	return nullptr;
}

IOnlineStorePtr FOnlineSubsystemGoogleCommon::GetStoreInterface() const
{
	return nullptr;
}

IOnlineEventsPtr FOnlineSubsystemGoogleCommon::GetEventsInterface() const
{
	return nullptr;
}

IOnlineAchievementsPtr FOnlineSubsystemGoogleCommon::GetAchievementsInterface() const
{
	return nullptr;
}

IOnlineSharingPtr FOnlineSubsystemGoogleCommon::GetSharingInterface() const
{
	return nullptr;
}

IOnlineUserPtr FOnlineSubsystemGoogleCommon::GetUserInterface() const
{
	return nullptr;
}

IOnlineMessagePtr FOnlineSubsystemGoogleCommon::GetMessageInterface() const
{
	return nullptr;
}

IOnlinePresencePtr FOnlineSubsystemGoogleCommon::GetPresenceInterface() const
{
	return nullptr;
}

IOnlineChatPtr FOnlineSubsystemGoogleCommon::GetChatInterface() const
{
	return nullptr;
}

IOnlineTurnBasedPtr FOnlineSubsystemGoogleCommon::GetTurnBasedInterface() const
{
	return nullptr;
}

FText FOnlineSubsystemGoogleCommon::GetOnlineServiceName() const
{
	return NSLOCTEXT("OnlineSubsystemGoogleCommon", "OnlineServiceName", "Google");
}

