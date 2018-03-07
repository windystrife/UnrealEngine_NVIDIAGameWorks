// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineIdentityInterfaceGameCircle.h"
#include "OnlineAchievementsInterfaceGameCircle.h"
#include "OnlineLeaderboardInterfaceGameCircle.h"
#include "OnlineExternalUIInterfaceGameCircle.h"
#include "OnlineFriendsInterfaceGameCircle.h"
#include "OnlineAGSCallbackManager.h"
#include "OnlineStoreInterfaceGameCircle.h"
#include "UniquePtr.h"

#include <string>

/**
 * OnlineSubsystemGameCircle - Implementation of the online subsystem for Amazon Game Circle services
 */
class ONLINESUBSYSTEMGAMECIRCLE_API FOnlineSubsystemGameCircle : 
	public FOnlineSubsystemImpl
{
public:
	
	virtual ~FOnlineSubsystemGameCircle() {}

	//~ Begin IOnlineSubsystem Interface
	virtual IOnlineSessionPtr GetSessionInterface() const override;
	virtual IOnlineFriendsPtr GetFriendsInterface() const override;
	virtual IOnlinePartyPtr GetPartyInterface() const override;
	virtual IOnlineGroupsPtr GetGroupsInterface() const override;
	virtual IOnlineSharedCloudPtr GetSharedCloudInterface() const override;
	virtual IOnlineUserCloudPtr GetUserCloudInterface() const override;
	virtual IOnlineLeaderboardsPtr GetLeaderboardsInterface() const override;
	virtual IOnlineVoicePtr GetVoiceInterface() const  override;
	virtual IOnlineExternalUIPtr GetExternalUIInterface() const override;
	virtual IOnlineTimePtr GetTimeInterface() const override;
	virtual IOnlineIdentityPtr GetIdentityInterface() const override;
	virtual IOnlineTitleFilePtr GetTitleFileInterface() const override;
	virtual IOnlineEntitlementsPtr GetEntitlementsInterface() const override;
	virtual IOnlineStorePtr GetStoreInterface() const override;
	virtual IOnlineStoreV2Ptr GetStoreV2Interface() const override { return nullptr; }
	virtual IOnlinePurchasePtr GetPurchaseInterface() const override { return nullptr; }
	virtual IOnlineEventsPtr GetEventsInterface() const override { return nullptr; }
	virtual IOnlineMessagePtr GetMessageInterface() const override { return nullptr; }
	virtual IOnlineSharingPtr GetSharingInterface() const override { return nullptr; }
	virtual IOnlineUserPtr GetUserInterface() const override { return nullptr; }
	virtual IOnlineAchievementsPtr GetAchievementsInterface() const override;
	virtual IOnlinePresencePtr GetPresenceInterface() const override { return nullptr; }
	virtual IOnlineChatPtr GetChatInterface() const override { return nullptr; }
	virtual IOnlineTurnBasedPtr GetTurnBasedInterface() const override { return nullptr; }

	virtual class UObject* GetNamedInterface(FName InterfaceName) override { return nullptr; }
	virtual void SetNamedInterface(FName InterfaceName, class UObject* NewInterface) override {}
	virtual bool IsDedicated() const override { return false; }
	virtual bool IsServer() const override { return true; }
	virtual void SetForceDedicated(bool bForce) override {}
	virtual bool IsLocalPlayer(const FUniqueNetId& UniqueId) const override { return true; }

	virtual bool Init() override;
	virtual bool Shutdown() override;
	virtual FString GetAppId() const override;
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	virtual FText GetOnlineServiceName() const override;
	//~ End IOnlineSubsystem Interface

	virtual bool Tick(float DeltaTime) override;
	
PACKAGE_SCOPE:

	FOnlineSubsystemGameCircle();
	FOnlineSubsystemGameCircle(FName InInstanceName);

	/**
	 * Is Online Subsystem Android available for use
	 * @return true if Android Online Subsystem functionality is available, false otherwise
	 */
	bool IsEnabled();

	FOnlineAGSCallbackManager *const GetCallbackManager() const { return AGSCallbackManager.Get(); }

	/** Utility function, useful for Google APIs that take a std::string but we only have an FString */
	static std::string ConvertFStringToStdString(const FString& InString);

	/** Returns the Game Circle-specific version of Identity, useful to avoid unnecessary casting */
	FOnlineIdentityGameCirclePtr GetIdentityGameCircle() const { return IdentityInterface; }

	/** Returns the Game Circle-specific version of Achievements, useful to avoid unnecessary casting */
	FOnlineAchievementsGameCirclePtr GetAchievementsGameCircle() const { return AchievementsInterface; }

	/** Returns the Game Circle-specific version of Leaderboards, useful to avoid unnecessary casting */
	FOnlineLeaderboardsGameCirclePtr GetLeaderboardsGameCircle() const { return LeaderboardsInterface; }

	/** Returns the Game Circle-specific version of Leaderboards, useful to avoid unnecessary casting */
	FOnlineFriendsGameCirclePtr GetFriendsGameCircle() const { return FriendsInterface; }

	/** Return the GameCircle-specific version of the ExternalUIInterface */
	FOnlineExternalUIGameCirclePtr GetExternalUIGameCircle() const { return ExternalUIInterface; }

	/**
	* Is IAP available for use
	* @return true if IAP should be available, false otherwise
	*/
	bool IsInAppPurchasingEnabled();

	///** Start a ShowLoginUI async task. Creates the GameServices object first if necessary. */
	//void StartShowLoginUITask(int PlayerId, const FOnLoginUIClosedDelegate& Delegate = FOnLoginUIClosedDelegate());

	///** Start a logout task if one isn't already in progress. */
	//void StartLogoutTask(int32 LocalUserNum);

private:

	///** Android callback when an activity is finished */
	//void OnActivityResult(JNIEnv *env, jobject thiz, jobject activity, jint requestCode, jint resultCode, jobject data);

	/** Manage all the different AmazonGames::ICallback classes */
	FOnlineAGSCallbackManagerPtr AGSCallbackManager;

	/** Interface to the online identity system */
	FOnlineIdentityGameCirclePtr IdentityInterface;

	FOnlineStoreGameCirclePtr StoreInterface;

	/** Interface to the online leaderboards */
	FOnlineLeaderboardsGameCirclePtr LeaderboardsInterface;

	/** Interface to the online achievements */
	FOnlineAchievementsGameCirclePtr AchievementsInterface;

	/** Interface to the external UI services */
	FOnlineExternalUIGameCirclePtr ExternalUIInterface;

	/** Interface to the friends services */
	FOnlineFriendsGameCirclePtr FriendsInterface;

	///** Handle of registered delegate for onActivityResult */
	//FDelegateHandle OnActivityResultDelegateHandle;

	///** This task needs to be able to set the GameServicesPtr and clear CurrentLoginTask*/
	//friend class FOnlineAsyncTaskGameCircleLogin;

	///** This task needs to be able to clear CurrentShowLoginUITask */
	//friend class FOnlineAsyncTaskGameCircleShowLoginUI;

	///** This task needs to be able to clear CurrentLogoutTask */
	//friend class FOnlineAsyncTaskGameCircleLogout;
};

typedef TSharedPtr<FOnlineSubsystemGameCircle, ESPMode::ThreadSafe> FOnlineSubsystemGameCirclePtr;
