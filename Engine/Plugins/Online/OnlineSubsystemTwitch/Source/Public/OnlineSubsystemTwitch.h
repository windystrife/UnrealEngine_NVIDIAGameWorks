// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystem.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineSubsystemTwitchModule.h"
#include "OnlineSubsystemTwitchPackage.h"

/** Forward declarations of all interface classes */
typedef TSharedPtr<class FOnlineIdentityTwitch, ESPMode::ThreadSafe> FOnlineIdentityTwitchPtr;
typedef TSharedPtr<class FOnlineExternalUITwitch, ESPMode::ThreadSafe> FOnlineExternalUITwitchPtr;

class FOnlineAsyncTaskManagerTwitch;
class FRunnableThread;

/**
 * Twitch backend services
 */
class ONLINESUBSYSTEMTWITCH_API FOnlineSubsystemTwitch :
	public FOnlineSubsystemImpl
{
public:

	// IOnlineSubsystem

	virtual IOnlineSessionPtr GetSessionInterface() const override;
	virtual IOnlineFriendsPtr GetFriendsInterface() const override;
	virtual IOnlinePartyPtr GetPartyInterface() const override;
	virtual IOnlineGroupsPtr GetGroupsInterface() const override;
	virtual IOnlineSharedCloudPtr GetSharedCloudInterface() const override;
	virtual IOnlineUserCloudPtr GetUserCloudInterface() const override;
	virtual IOnlineEntitlementsPtr GetEntitlementsInterface() const override;
	virtual IOnlineLeaderboardsPtr GetLeaderboardsInterface() const override;
	virtual IOnlineVoicePtr GetVoiceInterface() const override;
	virtual IOnlineExternalUIPtr GetExternalUIInterface() const override;	
	virtual IOnlineTimePtr GetTimeInterface() const override;
	virtual IOnlineIdentityPtr GetIdentityInterface() const override;
	virtual IOnlineTitleFilePtr GetTitleFileInterface() const override;
	virtual IOnlineStorePtr GetStoreInterface() const override;
	virtual IOnlineStoreV2Ptr GetStoreV2Interface() const override;
	virtual IOnlinePurchasePtr GetPurchaseInterface() const override;
	virtual IOnlineEventsPtr GetEventsInterface() const override;
	virtual IOnlineAchievementsPtr GetAchievementsInterface() const override;
	virtual IOnlineSharingPtr GetSharingInterface() const override;
	virtual IOnlineUserPtr GetUserInterface() const override;
	virtual IOnlineMessagePtr GetMessageInterface() const override;
	virtual IOnlinePresencePtr GetPresenceInterface() const override;
	virtual IOnlineChatPtr GetChatInterface() const override;
	virtual IOnlineTurnBasedPtr GetTurnBasedInterface() const override;
	virtual bool Init() override;
	virtual void PreUnload() override;
	virtual bool Shutdown() override;
	virtual FString GetAppId() const override;
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	virtual FText GetOnlineServiceName() const override;

	// FOnlineSubsystemTwitch

	/**
	 * Destructor
	 */
	virtual ~FOnlineSubsystemTwitch() = default;

	/**
	 * Is Twitch available for use
	 *
	 * @return true if Twitch functionality is available, false otherwise
	 */
	static bool IsEnabled();

	/**
	 * Get the twitch login service configuration
	 *
	 * @return login service instance associated with the subsystem
	 */
	inline FOnlineIdentityTwitchPtr GetTwitchIdentityService() const
	{
		return TwitchIdentity;
	}


PACKAGE_SCOPE:

 	/** Only the factory makes instances */
	FOnlineSubsystemTwitch(FName InInstanceName);
	
	/** Default constructor unavailable */
	FOnlineSubsystemTwitch() = delete;

	/** @return Twich API version */
	const FString& GetTwitchApiVersion() const { return TwitchApiVersion; }
	
private:

	bool HandleAuthExecCommands(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar);

	/** Interface to the identity registration/auth services */
	FOnlineIdentityTwitchPtr TwitchIdentity;

	/** Interface for external UI services on Twitch */
	FOnlineExternalUITwitchPtr TwitchExternalUIInterface;

	/** Twitch API version */
	FString TwitchApiVersion;
};

typedef TSharedPtr<FOnlineSubsystemTwitch, ESPMode::ThreadSafe> FOnlineSubsystemTwitchPtr;

