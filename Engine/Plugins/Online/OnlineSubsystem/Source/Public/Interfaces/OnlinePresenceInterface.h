// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/CoreOnline.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineDelegateMacros.h"
#include "OnlineKeyValuePair.h"

/** Type of presence keys */
typedef FString FPresenceKey;

/** Type of presence properties - a key/value map */
typedef FOnlineKeyValuePairs<FPresenceKey, FVariantData> FPresenceProperties;

/** The default key that will update presence text in the platform's UI */
const FString DefaultPresenceKey = TEXT("RichPresence");

/** Custom presence data that is not seen by users but can be polled */
const FString CustomPresenceDataKey = TEXT("CustomData");

/** Name of the client that sent the presence update */
const FString DefaultAppIdKey = TEXT("AppId");

/** Name of the platform the the presence update */
const FString DefaultPlatformKey = TEXT("Platform");

/** Override Id of the client to set the presence state to */
const FString OverrideAppIdKey = TEXT("OverrideAppId");

/** Id of the session for the presence update. @todo samz - SessionId on presence data should be FUniqueNetId not uint64 */
const FString DefaultSessionIdKey = TEXT("SessionId");

/** Resource the client is logged in with */
const FString PresenceResourceKey = TEXT("ResourceKey");

namespace EOnlinePresenceState
{
	enum Type : uint8
	{
		Online,
		Offline,
		Away,
		ExtendedAway,
		DoNotDisturb,
		Chat
	};

	/** 
	 * @return the stringified version of the enum passed in 
	 */
	inline const TCHAR* ToString(EOnlinePresenceState::Type EnumVal)
	{
		switch (EnumVal)
		{
		case Online:
			return TEXT("Online");
		case Offline:
			return TEXT("Offline");
		case Away:
			return TEXT("Away");
		case ExtendedAway:
			return TEXT("ExtendedAway");
		case DoNotDisturb:
			return TEXT("DoNotDisturb");
		case Chat:
			return TEXT("Chat");
		}
		return TEXT("");
	}

	static FText OnlineText =  NSLOCTEXT("OnlinePresence", "Online", "Online");
	static FText OfflineText =  NSLOCTEXT("OnlinePresence", "Offline", "Offline");
	static FText AwayText =  NSLOCTEXT("OnlinePresence", "Away", "Away");
	static FText ExtendedAwayText =  NSLOCTEXT("OnlinePresence", "ExtendedAway", "ExtendedAway");
	static FText DoNotDisturbText =  NSLOCTEXT("OnlinePresence", "DoNotDisturb", "DoNotDisturb");
	static FText ChatText =  NSLOCTEXT("OnlinePresence", "Chat", "Chat");
	/** 
	 * @return the loc text version of the enum passed in 
	 */
	inline const FText ToLocText(EOnlinePresenceState::Type EnumVal)
	{
		switch (EnumVal)
		{
		case Online:
			return OnlineText;
		case Offline:
			return OfflineText;
		case Away:
			return AwayText;
		case ExtendedAway:
			return ExtendedAwayText;
		case DoNotDisturb:
			return DoNotDisturbText;
		case Chat:
			return ChatText;
		}
		return FText::GetEmpty();
	}

}

class FOnlineUserPresenceStatus
{
public:
	FString StatusStr;
	EOnlinePresenceState::Type State;
	FPresenceProperties Properties;

	FOnlineUserPresenceStatus()
		: State(EOnlinePresenceState::Offline)
	{

	}
};

/**
 * Presence info for an online user returned via IOnlinePresence interface
 */
class FOnlineUserPresence
{
public:
	TSharedPtr<const FUniqueNetId> SessionId;
	uint32 bIsOnline:1;
	uint32 bIsPlaying:1;
	uint32 bIsPlayingThisGame:1;
	uint32 bIsJoinable:1;
	uint32 bHasVoiceSupport:1;
	FOnlineUserPresenceStatus Status;

	/** Constructor */
	FOnlineUserPresence()
	{
		Reset();
	}

	void Reset()
	{
		SessionId = nullptr;
		bIsOnline = 0;
		bIsPlaying = 0;
		bIsPlayingThisGame = 0;
		bIsJoinable = 0;
		bHasVoiceSupport = 0;
		Status = FOnlineUserPresenceStatus();
	}
};

/**
 * Delegate executed when new presence data is available for a user.
 *
 * @param UserId The unique id of the user whose presence was received.
 * @param Presence The unique id of the user whose presence was received.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPresenceReceived, const class FUniqueNetId& /*UserId*/, const TSharedRef<FOnlineUserPresence>& /*Presence*/);
typedef FOnPresenceReceived::FDelegate FOnPresenceReceivedDelegate;

/**
 * Delegate executed when the array of presence data for a user changes.
 *
 * @param UserId The unique id of the user whose presence was received.
 * @param PresenceArray The updated presence array
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPresenceArrayUpdated, const class FUniqueNetId& /*UserId*/, const TArray<TSharedRef<FOnlineUserPresence> >& /*PresenceArray*/);
typedef FOnPresenceArrayUpdated::FDelegate FOnPresenceArrayUpdatedDelegate;

/**
 *	Interface class for getting and setting rich presence information.
 */
class IOnlinePresence
{
public:
	/** Virtual destructor to force proper child cleanup */
	virtual ~IOnlinePresence() {}

	/**
	 * Delegate executed when setting or querying presence for a user has completed.
	 *
	 * @param UserId The unique id of the user whose presence was set.
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error.
	 */
	DECLARE_DELEGATE_TwoParams(FOnPresenceTaskCompleteDelegate, const class FUniqueNetId& /*UserId*/, const bool /*bWasSuccessful*/);

	/**
	 * Starts an async task that sets presence information for the user.
	 *
	 * @param User The unique id of the user whose presence is being set.
	 * @param Status The collection of state and key/value pairs to set as the user's presence data.
	 * @param Delegate The delegate to be executed when the potentially asynchronous set operation completes.
	 */
	virtual void SetPresence(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, const FOnPresenceTaskCompleteDelegate& Delegate = FOnPresenceTaskCompleteDelegate()) = 0;

	/**
	 * Starts an async operation that will update the cache with presence data from all users in the Users array.
	 * On platforms that support multiple keys, this function will query all keys.
	 *
	 * @param Users The list of unique ids of the users to query for presence information.
	 * @param Delegate The delegate to be executed when the potentially asynchronous query operation completes.
	 */
	//@todo samz - interface should be QueryPresence(const FUniqueNetId& User,  const TArray<TSharedRef<const FUniqueNetId> >& UserIds, const FOnPresenceTaskCompleteDelegate& Delegate)
	virtual void QueryPresence(const FUniqueNetId& User, const FOnPresenceTaskCompleteDelegate& Delegate = FOnPresenceTaskCompleteDelegate()) = 0;

	/**
	 * Delegate executed when new presence data is available for a user.
	 *
	 * @param UserId The unique id of the user whose presence was received.
	 * @param Presence The unique id of the user whose presence was received.
	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnPresenceReceived, const class FUniqueNetId& /*UserId*/, const TSharedRef<FOnlineUserPresence>& /*Presence*/);

	/**
 	 * Delegate executed when the array of presence data for a user changes.
 	 *
	 * @param UserId The unique id of the user whose presence was received.
 	 * @param PresenceArray The updated presence array
 	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnPresenceArrayUpdated, const class FUniqueNetId& /*UserId*/, const TArray<TSharedRef<FOnlineUserPresence> >& /*NewPresenceArray*/);

	/**
	 * Gets the cached presence information for a user.
	 *
	 * @param User The unique id of the user from which to get presence information.
	 * @param OutPresence If found, a shared pointer to the cached presence data for User will be stored here.
	 * @return Whether the data was found or not.
	 */
	virtual EOnlineCachedResult::Type GetCachedPresence(const FUniqueNetId& User, TSharedPtr<FOnlineUserPresence>& OutPresence) = 0;

	/**
	* Gets the cached presence information for a user.
	*
	* @param LocalUserId The unique id of the local user
	* @param User The unique id of the user from which to get presence information.
	* @param AppId The id of the application you want to get the presence of
	* @param OutPresence If found, a shared pointer to the cached presence data for User will be stored here.
	* @return Whether the data was found or not.
	*/
	virtual EOnlineCachedResult::Type GetCachedPresenceForApp(const FUniqueNetId& LocalUserId, const FUniqueNetId& User, const FString& AppId, TSharedPtr<FOnlineUserPresence>& OutPresence) = 0;
};
