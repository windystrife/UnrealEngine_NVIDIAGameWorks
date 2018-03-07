// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineFriendsInterfaceGameCircle.h"
#include "OnlineFriendGameCircle.h"
#include "OnlineAGSPlayerClientCallbacks.h"
#include "OnlineSubsystemGameCircle.h"
#include "OnlineIdentityInterfaceGameCircle.h"


FOnlineFriendsInterfaceGameCircle::FOnlineFriendsInterfaceGameCircle(FOnlineSubsystemGameCircle *const InSubsystem)
	: GameCircleSubsystem(InSubsystem)
	, FriendsListReadDelegate(nullptr)
	, bHasLocalFriendsList(false)
{
	check(GameCircleSubsystem);
}

bool FOnlineFriendsInterfaceGameCircle::ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate)
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("FOnlineFriendsInterfaceGameCircle::ReadFriendsList"));
	if(FriendsListReadDelegate)
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("FOnlineFriendsInterfaceGameCircle::ReadFriendsList returning false"));
		return false;
	}

	// Request list of friends' Ids from Amazon
	// Ignore list name, just get the friends
	FriendsListReadDelegate = &Delegate;
	FriendsListReadUserNum = LocalUserNum;
	FriendsListName = ListName;
	FPlatformMisc::LowLevelOutputDebugString(TEXT("FOnlineFriendsInterfaceGameCircle::ReadFriendsList getFriendIds"));
	AmazonGames::PlayerClientInterface::getFriendIds(new FOnlineGetFriendIdsCallback(GameCircleSubsystem));

	return true;
}

bool FOnlineFriendsInterfaceGameCircle::DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate)
{
	return false;
}

bool FOnlineFriendsInterfaceGameCircle::SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnSendInviteComplete& Delegate)
{
	return false;
}

bool FOnlineFriendsInterfaceGameCircle::AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate)
{
	return false;
}

bool FOnlineFriendsInterfaceGameCircle::RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	return false;
}
bool FOnlineFriendsInterfaceGameCircle::DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	return false;
}

bool FOnlineFriendsInterfaceGameCircle::GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray< TSharedRef<FOnlineFriend> >& OutFriends)
{
	OutFriends.Empty();
	for(TSharedPtr<FOnlineFriend> FriendPtr : FriendsList)
	{
		OutFriends.Add(FriendPtr.ToSharedRef());
	}
	return true;
}
TSharedPtr<FOnlineFriend> FOnlineFriendsInterfaceGameCircle::GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	for(TSharedPtr<FOnlineFriend> FriendPtr : FriendsList)
	{
		if(FriendId == FriendPtr->GetUserId().Get())
		{
			return FriendPtr;
		}
	}
	return nullptr;
}

bool FOnlineFriendsInterfaceGameCircle::IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	return GetFriend(LocalUserNum, FriendId, ListName).IsValid();
}

bool FOnlineFriendsInterfaceGameCircle::QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace)
{
	return false;
}

bool FOnlineFriendsInterfaceGameCircle::GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray< TSharedRef<FOnlineRecentPlayer> >& OutRecentPlayers)
{
	return false;
}

bool FOnlineFriendsInterfaceGameCircle::BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	return false;
}

bool FOnlineFriendsInterfaceGameCircle::UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	return false;
}

bool FOnlineFriendsInterfaceGameCircle::QueryBlockedPlayers(const FUniqueNetId& UserId)
{
	return false;
}

bool FOnlineFriendsInterfaceGameCircle::GetBlockedPlayers(const FUniqueNetId& UserId, TArray< TSharedRef<FOnlineBlockedPlayer> >& OutBlockedPlayers)
{
	return false;
}

void FOnlineFriendsInterfaceGameCircle::DumpBlockedPlayers() const
{
}

void FOnlineFriendsInterfaceGameCircle::OnGetFriendIdsCallback(const AmazonGames::ErrorCode InErrorCode, const AmazonGames::FriendIdList* InFriendIdList)
{
	if(InErrorCode != AmazonGames::ErrorCode::NO_ERROR)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OnGetFriendIdsCallback Error %d"), InErrorCode);
		FriendsListReadDelegate->ExecuteIfBound(FriendsListReadUserNum, false, FriendsListName, FString::Printf(TEXT("AmazonGames::PlayerClientInterface::getFriendIds returned ErrorCode %d"), InErrorCode));
		FriendsListReadDelegate = nullptr;
		return;
	}
	else if(InFriendIdList->numFriendIds == 0)
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("OnGetFriendIdsCallback No Friends"));
		FriendsList.Empty();
		FriendsListReadDelegate->ExecuteIfBound(FriendsListReadUserNum, true, FriendsListName, FString(TEXT("NO_ERROR")));
		FriendsListReadDelegate = nullptr;
		bHasLocalFriendsList = true;
		return;
	}

	TSharedPtr<AmazonGames::FriendIdList> BatchFriendIDs = MakeShareable(new AmazonGames::FriendIdList);
	BatchFriendIDs->numFriendIds = InFriendIdList->numFriendIds;
	BatchFriendIDs->friendIds = InFriendIdList->friendIds;
	AmazonGames::PlayerClientInterface::getBatchFriends(BatchFriendIDs.Get(), new FOnlineGetBatchFriendsCallback(GameCircleSubsystem));
}

void FOnlineFriendsInterfaceGameCircle::OnGetBatchFriendsCallback(const AmazonGames::ErrorCode InErrorCode, const AmazonGames::FriendList* InFriendList)
{
	if(InErrorCode != AmazonGames::ErrorCode::NO_ERROR)
	{
		FriendsListReadDelegate->ExecuteIfBound(FriendsListReadUserNum, false, FriendsListName, FString::Printf(TEXT("AmazonGames::PlayerClientInterface::getBatchFriends returned ErrorCode %d"), InErrorCode));
		FriendsListReadDelegate = nullptr;
		return;
	}
	
	FriendsList.Empty();
	FString PlayerIdStr;
	FString Alias;
	FString AvatarURL;
	FOnlineIdentityGameCirclePtr IdentityInterface = GameCircleSubsystem->GetIdentityGameCircle();

	for(int Idx = 0; Idx < InFriendList->numFriends; ++Idx)
	{
		Alias = InFriendList->friends[Idx].alias;
		AvatarURL = InFriendList->friends[Idx].avatarUrl;
		PlayerIdStr = InFriendList->friends[Idx].playerId;
		
		UE_LOG_ONLINE(Log, TEXT("Friends[%d] - ID: %s  Alias: %s  AvatarURL: %s"), Idx, *PlayerIdStr, *Alias, *AvatarURL);
		FriendsList.Add( MakeShareable( new FOnlineFriendGameCircle(IdentityInterface->CreateUniquePlayerId(PlayerIdStr), Alias, AvatarURL) ) );
	}

	bHasLocalFriendsList = true;
	FriendsListReadDelegate->ExecuteIfBound(FriendsListReadUserNum, true, FriendsListName, FString(TEXT("NO_ERROR")));
	FriendsListReadDelegate = nullptr;
}
