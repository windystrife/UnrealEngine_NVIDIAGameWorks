// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemTwitchPrivate.h"
#include "OnlineIdentityTwitch.h"
#include "OnlineExternalUITwitch.h"
#include "Engine/Console.h"
#include "Misc/ConfigCacheIni.h"

IOnlineSessionPtr FOnlineSubsystemTwitch::GetSessionInterface() const
{
	return nullptr;
}

IOnlineFriendsPtr FOnlineSubsystemTwitch::GetFriendsInterface() const
{
	return nullptr;
}

IOnlinePartyPtr FOnlineSubsystemTwitch::GetPartyInterface() const
{
	return nullptr;
}

IOnlineGroupsPtr FOnlineSubsystemTwitch::GetGroupsInterface() const
{
	return nullptr;
}

IOnlineSharedCloudPtr FOnlineSubsystemTwitch::GetSharedCloudInterface() const
{
	return nullptr;
}

IOnlineUserCloudPtr FOnlineSubsystemTwitch::GetUserCloudInterface() const
{
	return nullptr;
}

IOnlineEntitlementsPtr FOnlineSubsystemTwitch::GetEntitlementsInterface() const
{
	return nullptr;
}

IOnlineLeaderboardsPtr FOnlineSubsystemTwitch::GetLeaderboardsInterface() const
{
	return nullptr;
}

IOnlineVoicePtr FOnlineSubsystemTwitch::GetVoiceInterface() const
{
	return nullptr;
}

IOnlineExternalUIPtr FOnlineSubsystemTwitch::GetExternalUIInterface() const
{
	return TwitchExternalUIInterface;
}

IOnlineTimePtr FOnlineSubsystemTwitch::GetTimeInterface() const
{
	return nullptr;
}

IOnlineIdentityPtr FOnlineSubsystemTwitch::GetIdentityInterface() const
{
	return TwitchIdentity;
}

IOnlineTitleFilePtr FOnlineSubsystemTwitch:: GetTitleFileInterface() const
{
	return nullptr;
}

IOnlineStorePtr FOnlineSubsystemTwitch::GetStoreInterface() const
{
	return nullptr;
}

IOnlineStoreV2Ptr FOnlineSubsystemTwitch::GetStoreV2Interface() const
{
	return nullptr;
}

IOnlinePurchasePtr FOnlineSubsystemTwitch::GetPurchaseInterface() const
{
	return nullptr;
}

IOnlineEventsPtr FOnlineSubsystemTwitch::GetEventsInterface() const
{
	return nullptr;
}

IOnlineAchievementsPtr FOnlineSubsystemTwitch::GetAchievementsInterface() const
{
	return nullptr;
}

IOnlineSharingPtr FOnlineSubsystemTwitch::GetSharingInterface() const
{
	return nullptr;
}

IOnlineUserPtr FOnlineSubsystemTwitch::GetUserInterface() const
{
	return nullptr;
}

IOnlineMessagePtr FOnlineSubsystemTwitch::GetMessageInterface() const
{
	return nullptr;
}

IOnlinePresencePtr FOnlineSubsystemTwitch::GetPresenceInterface() const
{
	return nullptr;
}

IOnlineChatPtr FOnlineSubsystemTwitch::GetChatInterface() const
{
	return nullptr;
}

IOnlineTurnBasedPtr FOnlineSubsystemTwitch::GetTurnBasedInterface() const
{
	return nullptr;
}

bool FOnlineSubsystemTwitch::Init()
{
	UE_LOG_ONLINE(Verbose, TEXT("FOnlineSubsystemTwitch::Init() Name: %s"), *InstanceName.ToString());

	TwitchIdentity = MakeShared<FOnlineIdentityTwitch, ESPMode::ThreadSafe>(this);
	TwitchExternalUIInterface = MakeShared<FOnlineExternalUITwitch, ESPMode::ThreadSafe>(this);

	return true;
}

void FOnlineSubsystemTwitch::PreUnload()
{
	UE_LOG_ONLINE(Verbose, TEXT("FOnlineSubsystemTwitch::Preunload() Name: %s"), *InstanceName.ToString());
}

bool FOnlineSubsystemTwitch::Shutdown()
{
	UE_LOG_ONLINE(Verbose, TEXT("FOnlineSubsystemTwitch::Shutdown() Name: %s"), *InstanceName.ToString());

	FOnlineSubsystemImpl::Shutdown();

#define DESTRUCT_INTERFACE(Interface) \
	if (Interface.IsValid()) \
	{ \
		ensure(Interface.IsUnique()); \
		Interface = nullptr; \
	}

	DESTRUCT_INTERFACE(TwitchIdentity);
	DESTRUCT_INTERFACE(TwitchExternalUIInterface);

#undef DESTRUCT_INTERFACE
	return true;
}

FString FOnlineSubsystemTwitch::GetAppId() const
{
	FString ClientId;
	if (!GConfig->GetString(TEXT("OnlineSubsystemTwitch"), TEXT("ClientId"), ClientId, GEngineIni) ||
		ClientId.IsEmpty())
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG(LogOnline, Warning, TEXT("Missing ClientId= in [OnlineSubsystemTwitch] of DefaultEngine.ini"));
		}
	}
	return ClientId;
}

bool FOnlineSubsystemTwitch::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FOnlineSubsystemImpl::Exec(InWorld, Cmd, Ar))
	{
		return true;
	}

	bool bWasHandled = false;
	if (FParse::Command(&Cmd, TEXT("TEST")))
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (FParse::Command(&Cmd, TEXT("AUTH")))
		{
			bWasHandled = HandleAuthExecCommands(InWorld, Cmd, Ar);
		}
#endif // WITH_DEV_AUTOMATION_TESTS
	}
	
	return bWasHandled;
}

FText FOnlineSubsystemTwitch::GetOnlineServiceName() const
{
	return NSLOCTEXT("OnlineSubsystemTwitch", "OnlineServiceName", "Twitch");
}

bool FOnlineSubsystemTwitch::HandleAuthExecCommands(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bWasHandled = false;

#if WITH_DEV_AUTOMATION_TESTS
	if (FParse::Command(&Cmd, TEXT("INFO")))
	{
		FString AuthType = FParse::Token(Cmd, false);
		bWasHandled = true;
	}
	else if (FParse::Command(&Cmd, TEXT("LOGIN")))
	{
		FOnlineIdentityTwitchPtr Identity = GetTwitchIdentityService();
		bWasHandled = true;

		int32 LocalUserNum = 0;
		FString Id = FParse::Token(Cmd, false);
		FString Auth = FParse::Token(Cmd, false);

		static FDelegateHandle LoginCompleteDelegateHandle;
		if (LoginCompleteDelegateHandle.IsValid())
		{
			UE_LOG_ONLINE(Error, TEXT("Another login attempt is already in progress"));
		}
		auto Delegate = FOnLoginCompleteDelegate::CreateLambda([Identity, Id, Auth](int32 InLocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error) {
			ensure(LoginCompleteDelegateHandle.IsValid());
			if (bWasSuccessful)
			{
				UE_LOG_ONLINE(Display, TEXT("Twitch login completed successfully.  UserId=%s"), *UserId.ToString());
			}
			else
			{
				if (Error.StartsWith(TWITCH_LOGIN_ERROR_MISSING_PERMISSIONS))
				{
					TArray<FString> MissingPermissions;
					Error.ParseIntoArray(MissingPermissions, TEXT(" "));
					// First index will be TWITCH_LOGIN_ERROR_MISSING_PERMISSIONS, so skip
					for (int32 MissingPermissionIndex = 1; MissingPermissionIndex < MissingPermissions.Num(); ++MissingPermissionIndex)
					{
						UE_LOG_ONLINE(Display, TEXT("Twitch log in failed:  Missing permission %s"), *MissingPermissions[MissingPermissionIndex]);
					}
				}
			}
			Identity->ClearOnLoginCompleteDelegate_Handle(InLocalUserNum, LoginCompleteDelegateHandle);
			LoginCompleteDelegateHandle.Reset();
		});
		LoginCompleteDelegateHandle = Identity->AddOnLoginCompleteDelegate_Handle(LocalUserNum, Delegate);
		Identity->Login(LocalUserNum, FOnlineAccountCredentials(Identity->GetAuthType(), Id, Auth));
	}
#endif // WITH_DEV_AUTOMATION_TESTS

	return bWasHandled;
}


bool FOnlineSubsystemTwitch::IsEnabled()
{
	// Check the ini for disabling Twitch
	bool bEnableTwitch = true;
	GConfig->GetBool(TEXT("OnlineSubsystemTwitch"), TEXT("bEnabled"), bEnableTwitch, GEngineIni);
	return bEnableTwitch;
}

FOnlineSubsystemTwitch::FOnlineSubsystemTwitch(FName InInstanceName)
	: FOnlineSubsystemImpl(TWITCH_SUBSYSTEM, InInstanceName)
	, TwitchApiVersion(TEXT("application/vnd.twitchtv.v5+json"))
{
}
