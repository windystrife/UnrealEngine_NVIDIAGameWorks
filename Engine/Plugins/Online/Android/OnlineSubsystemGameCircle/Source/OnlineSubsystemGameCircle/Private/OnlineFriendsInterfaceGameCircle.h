// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineFriendsInterface.h"
#include "AGS/PlayerClientInterface.h"


class FOnlineSubsystemGameCircle;

class FOnlineFriendsInterfaceGameCircle : public IOnlineFriends
{
public:

	//~ Begin IOnlineFriends Interface
	virtual bool ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate = FOnReadFriendsListComplete()) override;
	virtual bool DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate = FOnDeleteFriendsListComplete()) override;
	virtual bool SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnSendInviteComplete& Delegate = FOnSendInviteComplete()) override;
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
	//~ End IOnlineFriends Interface

PACKAGE_SCOPE:

	/**
	 * Constructor used by subsystem
	 */
	FOnlineFriendsInterfaceGameCircle(FOnlineSubsystemGameCircle *const InSubsystem);

	void OnGetFriendIdsCallback(const AmazonGames::ErrorCode InErrorCode, const AmazonGames::FriendIdList* InFriendIdList);

	void OnGetBatchFriendsCallback(const AmazonGames::ErrorCode InErrorCode, const AmazonGames::FriendList* InFriendList);

	bool HasLocalFriendsList() const { return bHasLocalFriendsList; }

private:

	/**
	 * Private default constructor
	 */
	FOnlineFriendsInterfaceGameCircle() : GameCircleSubsystem(nullptr) {}

	FOnlineSubsystemGameCircle * GameCircleSubsystem;

	const FOnReadFriendsListComplete * FriendsListReadDelegate;
	int32 FriendsListReadUserNum;
	FString FriendsListName;

	TArray< TSharedPtr<FOnlineFriend> > FriendsList;
	bool bHasLocalFriendsList;
};

typedef TSharedPtr<FOnlineFriendsInterfaceGameCircle, ESPMode::ThreadSafe> FOnlineFriendsGameCirclePtr;
