// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineFriendsInterface.h"
#include "OnlinePresenceInterface.h"
#include "OnlineSubsystemFacebookTypes.h"
#include "OnlineJsonSerializer.h"
#include "Http.h"
#include "OnlineSubsystemFacebookPackage.h"

class FOnlineSubsystemFacebook;

/** Details about a friend */
#define FRIEND_FIELD_ID "id"
#define FRIEND_FIELD_NAME "name"
#define FRIEND_FIELD_FIRSTNAME "first_name"
#define FRIEND_FIELD_LASTNAME "last_name"
#define FRIEND_FIELD_PICTURE "picture"

/**
 * Info associated with an online friend on the Facebook service
 */
class FOnlineFriendFacebook : 
	public FOnlineFriend
{
public:

	// FOnlineUser

	virtual TSharedRef<const FUniqueNetId> GetUserId() const override;
	virtual FString GetRealName() const override;
	virtual FString GetDisplayName(const FString& Platform = FString()) const override;
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;

	// FOnlineFriend
	
	virtual EInviteStatus::Type GetInviteStatus() const override;
	virtual const FOnlineUserPresence& GetPresence() const override;

	// FOnlineFriendFacebook

	/**
	 * Init/default constructor
	 */
	FOnlineFriendFacebook(const FString& InUserId=TEXT("")) 
		: UserIdPtr(new FUniqueNetIdString(InUserId))
	{
	}

	/**
	 * Destructor
	 */
	virtual ~FOnlineFriendFacebook()
	{
	}

	/**
	 * Parse Json friend request data
	 *
	 * @param JsonObject a json payload from a friend request
	 */
	bool Parse(const TSharedPtr<FJsonObject>& JsonObject);

private:

	/**
	 * Get account data attribute
	 *
	 * @param Key account data entry key
	 * @param OutVal [out] value that was found
	 *
	 * @return true if entry was found
	 */
	inline bool GetAccountData(const FString& Key, FString& OutVal) const
	{
		const FString* FoundVal = AccountData.Find(Key);
		if (FoundVal != NULL)
		{
			OutVal = *FoundVal;
			return true;
		}
		return false;
	}

	void AddUserAttributes(const TSharedPtr<FJsonObject>& JsonUser);

	/** User Id represented as a FUniqueNetId */
	TSharedRef<const FUniqueNetId> UserIdPtr;
	/** Profile picture */
	FUserOnlineFacebookPicture Picture;
	/** Any addition account data associated with the friend */
	FJsonSerializableKeyValueMap AccountData;
	/** @temp presence info */
	FOnlineUserPresence Presence;

	/** Allow the FB friends to fill in our private members from it's callbacks */
	friend class FOnlineFriendsFacebookCommon;
};

/**
 * Facebook service implementation of the online friends interface
 */
class FOnlineFriendsFacebookCommon :
	public IOnlineFriends
{	

public:

	// IOnlineFriends

	virtual bool ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate = FOnReadFriendsListComplete()) override;
	virtual bool DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate = FOnDeleteFriendsListComplete()) override;
	virtual bool SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName,  const FOnSendInviteComplete& Delegate = FOnSendInviteComplete()) override;
	virtual bool AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate = FOnAcceptInviteComplete()) override;
 	virtual bool RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
 	virtual bool DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray< TSharedRef<FOnlineFriend> >& OutFriends) override;
	virtual TSharedPtr<FOnlineFriend> GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace) override;
	virtual bool GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray< TSharedRef<FOnlineRecentPlayer> >& OutRecentPlayers) override;
	virtual bool BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId) override;
	virtual bool UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId) override;
	virtual bool QueryBlockedPlayers(const FUniqueNetId& UserId) override;
	virtual bool GetBlockedPlayers(const FUniqueNetId& UserId, TArray< TSharedRef<FOnlineBlockedPlayer> >& OutBlockedPlayers) override;
	virtual void DumpBlockedPlayers() const override;

	// FOnlineFriendsFacebook

	/**
	 * Constructor
	 *
	 * @param InSubsystem Facebook subsystem being used
	 */
	FOnlineFriendsFacebookCommon(FOnlineSubsystemFacebook* InSubsystem);
	
	/**
	 * Destructor
	 */
	virtual ~FOnlineFriendsFacebookCommon();

private:

	/**
	 * Should use the initialization constructor instead
	 */
	FOnlineFriendsFacebookCommon();

	/**
	 * Delegate called when a user /me request from Facebook is complete
	 */
	void QueryFriendsList_HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, FOnReadFriendsListComplete Delegate);

	/** For accessing identity/token info of user logged in */
	FOnlineSubsystemFacebook* FacebookSubsystem;
	/** Config based url for querying friends list */
	FString FriendsUrl;
	/** Config based list of fields to use when querying friends list */
	TArray<FString> FriendsFields;

	/** List of online friends */
	struct FOnlineFriendsList
	{
		TArray< TSharedRef<FOnlineFriendFacebook> > Friends;
	};
	/** Cached friends list from last call to ReadFriendsList for each local user */
	TMap<int, FOnlineFriendsList> FriendsMap;

	/** Info used to send request to register a user */
	struct FPendingFriendsQuery
	{
		FPendingFriendsQuery(int32 InLocalUserNum=0) 
			: LocalUserNum(InLocalUserNum)
		{
		}
		/** local index of user making the request */
		int32 LocalUserNum;
	};
	/** List of pending Http requests for user registration */
	TMap<class IHttpRequest*, FPendingFriendsQuery> FriendsQueryRequests;
};

typedef TSharedPtr<FOnlineFriendsFacebookCommon, ESPMode::ThreadSafe> FOnlineFriendsFacebookCommonPtr;
