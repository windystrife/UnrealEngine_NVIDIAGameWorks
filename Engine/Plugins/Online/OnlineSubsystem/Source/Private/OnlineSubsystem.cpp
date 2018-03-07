// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "NboSerializer.h"
#include "Online.h"
#include "Misc/NetworkVersion.h"

DEFINE_LOG_CATEGORY(LogOnline);
DEFINE_LOG_CATEGORY(LogOnlineGame);
DEFINE_LOG_CATEGORY(LogOnlineChat);
DEFINE_LOG_CATEGORY(LogVoiceEngine);

#if STATS
ONLINESUBSYSTEM_API DEFINE_STAT(STAT_Online_Async);
ONLINESUBSYSTEM_API DEFINE_STAT(STAT_Online_AsyncTasks);
ONLINESUBSYSTEM_API DEFINE_STAT(STAT_Session_Interface);
ONLINESUBSYSTEM_API DEFINE_STAT(STAT_Voice_Interface);
#endif

/** Workaround, please avoid using this */
TSharedPtr<const FUniqueNetId> GetFirstSignedInUser(IOnlineIdentityPtr IdentityInt)
{
	TSharedPtr<const FUniqueNetId> UserId = nullptr;
	if (IdentityInt.IsValid())
	{
		for (int32 i = 0; i < MAX_LOCAL_PLAYERS; i++)
		{
			UserId = IdentityInt->GetUniquePlayerId(i);
			if (UserId.IsValid() && UserId->IsValid())
			{
				break;
			}
		}
	}

	return UserId;
}

int32 GetBuildUniqueId()
{
	static bool bStaticCheck = false;
	static bool bUseBuildIdOverride = false;
	static int32 BuildIdOverride = 0;
	if (!bStaticCheck)
	{
		if (FParse::Value(FCommandLine::Get(), TEXT("BuildIdOverride="), BuildIdOverride) && BuildIdOverride != 0)
		{
			bUseBuildIdOverride = true;
		}
		else
		{
			if (!GConfig->GetBool(TEXT("OnlineSubsystem"), TEXT("bUseBuildIdOverride"), bUseBuildIdOverride, GEngineIni))
			{
				UE_LOG_ONLINE(Warning, TEXT("Missing bUseBuildIdOverride= in [OnlineSubsystem] of DefaultEngine.ini"));
			}

			if (!GConfig->GetInt(TEXT("OnlineSubsystem"), TEXT("BuildIdOverride"), BuildIdOverride, GEngineIni))
			{
				UE_LOG_ONLINE(Warning, TEXT("Missing BuildIdOverride= in [OnlineSubsystem] of DefaultEngine.ini"));
			}
		}

		bStaticCheck = true;
	}

	const uint32 NetworkVersion = FNetworkVersion::GetLocalNetworkVersion();

	int32 BuildId = 0;
	if (bUseBuildIdOverride == false)
	{
		/** Engine package CRC doesn't change, can't be used as the version - BZ */
		FNboSerializeToBuffer Buffer(64);
		// Serialize to a NBO buffer for consistent CRCs across platforms
		Buffer << NetworkVersion;
		// Now calculate the CRC
		uint32 Crc = FCrc::MemCrc32((uint8*)Buffer, Buffer.GetByteCount());

		// make sure it's positive when it's cast back to an int
		BuildId = static_cast<int32>(Crc & 0x7fffffff);
	}
	else
	{
		BuildId = BuildIdOverride;
	}

	UE_LOG_ONLINE(VeryVerbose, TEXT("GetBuildUniqueId: Network CL %u LocalNetworkVersion %u bUseBuildIdOverride %d BuildIdOverride %d BuildId %d"),
		FNetworkVersion::GetNetworkCompatibleChangelist(),
		NetworkVersion,
		bUseBuildIdOverride,
		BuildIdOverride,
		BuildId);

	return BuildId;
}

bool IsPlayerInSessionImpl(IOnlineSession* SessionInt, FName SessionName, const FUniqueNetId& UniqueId)
{
	bool bFound = false;
	FNamedOnlineSession* Session = SessionInt->GetNamedSession(SessionName);
	if (Session != NULL)
	{
		const bool bIsSessionOwner = *Session->OwningUserId == UniqueId;

		FUniqueNetIdMatcher PlayerMatch(UniqueId);
		if (bIsSessionOwner || 
			Session->RegisteredPlayers.IndexOfByPredicate(PlayerMatch) != INDEX_NONE)
		{
			bFound = true;
		}
	}
	return bFound;
}

int32 GetBeaconPortFromSessionSettings(const FOnlineSessionSettings& SessionSettings)
{
	int32 BeaconListenPort = DEFAULT_BEACON_PORT;
	if (!SessionSettings.Get(SETTING_BEACONPORT, BeaconListenPort) || BeaconListenPort <= 0)
	{
		// Reset the default BeaconListenPort back to DEFAULT_BEACON_PORT because the SessionSettings value does not exist or was not valid
		BeaconListenPort = DEFAULT_BEACON_PORT;
	}
	return BeaconListenPort;
}

#if !UE_BUILD_SHIPPING

static void ResetAchievements()
{
	auto IdentityInterface = Online::GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("ResetAchievements command: couldn't get the identity interface"));
		return;
	}
	
	TSharedPtr<const FUniqueNetId> UserId = IdentityInterface->GetUniquePlayerId(0);
	if(!UserId.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("ResetAchievements command: invalid UserId"));
		return;
	}

	auto AchievementsInterface = Online::GetAchievementsInterface();
	if (!AchievementsInterface.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("ResetAchievements command: couldn't get the achievements interface"));
		return;
	}

	AchievementsInterface->ResetAchievements(*UserId);
}

FAutoConsoleCommand CmdResetAchievements(
	TEXT("online.ResetAchievements"),
	TEXT("Reset achievements for the currently logged in user."),
	FConsoleCommandDelegate::CreateStatic(ResetAchievements)
	);

#endif
