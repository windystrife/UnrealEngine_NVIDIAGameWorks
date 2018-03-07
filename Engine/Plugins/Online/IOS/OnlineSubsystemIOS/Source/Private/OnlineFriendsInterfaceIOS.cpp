// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "OnlineSubsystemIOSPrivatePCH.h"

// FOnlineFriendIOS

TSharedRef<const FUniqueNetId> FOnlineFriendIOS::GetUserId() const
{
	return UserId;
}

FString FOnlineFriendIOS::GetRealName() const
{
	FString Result;
	GetAccountData(TEXT("nickname"), Result);
	return Result;
}

FString FOnlineFriendIOS::GetDisplayName(const FString& Platform) const
{
	FString Result;
	GetAccountData(TEXT("nickname"), Result);
	return Result;
}

bool FOnlineFriendIOS::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	return GetAccountData(AttrName, OutAttrValue);
}

EInviteStatus::Type FOnlineFriendIOS::GetInviteStatus() const
{
	return EInviteStatus::Accepted;
}

const FOnlineUserPresence& FOnlineFriendIOS::GetPresence() const
{
	return Presence;
}

// FOnlineFriendsIOS

FOnlineFriendsIOS::FOnlineFriendsIOS(FOnlineSubsystemIOS* InSubsystem)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsIOS::FOnlineFriendsIOS()"));

	IdentityInterface = (FOnlineIdentityIOS*)InSubsystem->GetIdentityInterface().Get();
}

bool FOnlineFriendsIOS::ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate /*= FOnReadFriendsListComplete()*/)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsIOS::ReadFriendsList()"));
	bool bSuccessfullyBeganReadFriends = false;

	if( IdentityInterface->GetLocalGameCenterUser() != NULL && IdentityInterface->GetLocalGameCenterUser().isAuthenticated )
	{
		bSuccessfullyBeganReadFriends = true;
		dispatch_async(dispatch_get_main_queue(), ^
		{
		// Get the friends list for the local player from the server
#ifdef __IPHONE_8_0
            if ([[GKLocalPlayer localPlayer] respondsToSelector:@selector(loadFriendPlayersWithCompletionHandler)] == YES)
            {
                [[GKLocalPlayer localPlayer] loadFriendPlayersWithCompletionHandler:
                 ^(NSArray* Friends, NSError* Error)
                 {
                    if( Error )
                    {
                        FString ErrorStr(FString::Printf(TEXT("FOnlineFriendsIOS::ReadFriendsList() - Failed to read friends list with error: [%i]"), [Error code]));
                        UE_LOG(LogOnline, Verbose, TEXT("%s"), *ErrorStr);
                 
                        // Report back to the game thread whether this succeeded.
                        [FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
                         {
                            Delegate.ExecuteIfBound(0, false, ListName, ErrorStr);
                            return true;
                         }];
                    }
                    else
                    {
                        [GKPlayer loadPlayersForIdentifiers:Friends withCompletionHandler:
                         ^(NSArray* Players, NSError* Error2)
                         {
                            FString ErrorStr;
                            bool bWasSuccessful = false;
                            if( Error2 )
                            {
                                ErrorStr = FString::Printf(TEXT("FOnlineFriendsIOS::ReadFriendsList() - Failed to loadPlayersForIdentifiers with error: [%i]"), [Error2 code]);
                            }
                            else
                            {
                                // Clear our previosly cached friends before we repopulate the cache.
                                CachedFriends.Empty();
                                for( int32 FriendIdx = 0; FriendIdx < [Players count]; FriendIdx++ )
                                {
                                    GKPlayer* Friend = Players[ FriendIdx ];
                  
                                    // Add new friend entry to list
                                    TSharedRef<FOnlineFriendIOS> FriendEntry(
                                            new FOnlineFriendIOS(*FString(Friend.playerID))
                                    );
                                    FriendEntry->AccountData.Add(
                                            TEXT("nickname"), *FString(Friend.alias)
                                    );
                                    CachedFriends.Add(FriendEntry);
                  
                                    UE_LOG(LogOnline, Verbose, TEXT("GCFriend - Id:%s Alias:%s"), *FString(Friend.playerID),
                                        *FriendEntry->GetDisplayName() );
                                }
                  
                                bWasSuccessful = true;
                            }
                  
                            // Report back to the game thread whether this succeeded.
                            [FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
                             {
                                Delegate.ExecuteIfBound(0, bWasSuccessful, ListName, ErrorStr);
                                return true;
                             }];
                         }];
                    }
                 }];
            }
            else
#endif
            {
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_8_0
                [[GKLocalPlayer localPlayer] loadFriendsWithCompletionHandler:
                 ^(NSArray* Friends, NSError* Error)
                 {
                    if( Error )
                    {
                        FString ErrorStr(FString::Printf(TEXT("FOnlineFriendsIOS::ReadFriendsList() - Failed to read friends list with error: [%i]"), [Error code]));
                        UE_LOG(LogOnline, Verbose, TEXT("%s"), *ErrorStr);

						// Report back to the game thread whether this succeeded.
						[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
						{
							Delegate.ExecuteIfBound(0, false, ListName, ErrorStr);
							return true;
						}];
                    }
                    else
                    {
                        [GKPlayer loadPlayersForIdentifiers:Friends withCompletionHandler:
                         ^(NSArray* Players, NSError* Error2)
                         {
								FString ErrorStr;
								bool bWasSuccessful = false;
							if( Error2 )
							{
									ErrorStr = FString::Printf(TEXT("FOnlineFriendsIOS::ReadFriendsList() - Failed to loadPlayersForIdentifiers with error: [%i]"), [Error2 code]);
							}
							else
							{
								// Clear our previosly cached friends before we repopulate the cache.
								CachedFriends.Empty();
								for( int32 FriendIdx = 0; FriendIdx < [Players count]; FriendIdx++ )
								{
									GKPlayer* Friend = Players[ FriendIdx ];

									// Add new friend entry to list
									TSharedRef<FOnlineFriendIOS> FriendEntry(
										new FOnlineFriendIOS(*FString(Friend.playerID))
										);
									FriendEntry->AccountData.Add(
										TEXT("nickname"), *FString(Friend.alias)
										);
									CachedFriends.Add(FriendEntry);

									UE_LOG(LogOnline, Verbose, TEXT("GCFriend - Id:%s Alias:%s"), *FString(Friend.playerID), 
										*FriendEntry->GetDisplayName() );
								}

                                bWasSuccessful = true;
                            }
								
							// Report back to the game thread whether this succeeded.
							[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
							{
								Delegate.ExecuteIfBound(0, bWasSuccessful, ListName, ErrorStr);
								return true;
							}];
						}];
                    }
                }];
#endif
			}
		});
	}
	else
	{
		// no gamecenter login means we cannot read the friends.
		Delegate.ExecuteIfBound( 0 , false, ListName, TEXT("not logged in") );
	}

	return bSuccessfullyBeganReadFriends;
}

bool FOnlineFriendsIOS::DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate /*= FOnDeleteFriendsListComplete()*/)
{
	Delegate.ExecuteIfBound(LocalUserNum, false, ListName, FString(TEXT("DeleteFriendsList() is not supported")));
	return false;
}

bool FOnlineFriendsIOS::SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnSendInviteComplete& Delegate /*= FOnSendInviteComplete()*/)
{
	Delegate.ExecuteIfBound(LocalUserNum, false, FriendId, ListName, FString(TEXT("SendInvite() is not supported")));
	return false;
}

bool FOnlineFriendsIOS::AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate /*= FOnAcceptInviteComplete()*/)
{
	Delegate.ExecuteIfBound(LocalUserNum, false, FriendId, ListName, FString(TEXT("AcceptInvite() is not supported")));
	return false;
}

bool FOnlineFriendsIOS::RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TriggerOnRejectInviteCompleteDelegates(LocalUserNum, false, FriendId, ListName, FString(TEXT("RejectInvite() is not supported")));
	return false;
}

bool FOnlineFriendsIOS::DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TriggerOnDeleteFriendCompleteDelegates(LocalUserNum, false, FriendId, ListName, FString(TEXT("DeleteFriend() is not supported")));
	return false;
}

bool FOnlineFriendsIOS::GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray< TSharedRef<FOnlineFriend> >& OutFriends)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsIOS::GetFriendsList()"));

	for (int32 Idx=0; Idx < CachedFriends.Num(); Idx++)
	{
		OutFriends.Add(CachedFriends[Idx]);
	}

	return true;
}

TSharedPtr<FOnlineFriend> FOnlineFriendsIOS::GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TSharedPtr<FOnlineFriend> Result;

	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsIOS::GetFriend()"));

	for (int32 Idx=0; Idx < CachedFriends.Num(); Idx++)
	{
		if (*(CachedFriends[Idx]->GetUserId()) == FriendId)
		{
			Result = CachedFriends[Idx];
			break;
		}
	}

	return Result;
}

bool FOnlineFriendsIOS::IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsIOS::IsFriend()"));

	TSharedPtr<FOnlineFriend> Friend = GetFriend(LocalUserNum,FriendId,ListName);
	if (Friend.IsValid() &&
		Friend->GetInviteStatus() == EInviteStatus::Accepted)
	{
		return true;
	}
	return false;
}

bool FOnlineFriendsIOS::QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineFriendsIOS::QueryRecentPlayers()"));

	TriggerOnQueryRecentPlayersCompleteDelegates(UserId, Namespace, false, TEXT("not implemented"));

	return false;
}

bool FOnlineFriendsIOS::GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray< TSharedRef<FOnlineRecentPlayer> >& OutRecentPlayers)
{
	return false;
}

bool FOnlineFriendsIOS::BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	return false;
}

bool FOnlineFriendsIOS::UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	return false;
}

bool FOnlineFriendsIOS::QueryBlockedPlayers(const FUniqueNetId& UserId)
{
	return false;
}

bool FOnlineFriendsIOS::GetBlockedPlayers(const FUniqueNetId& UserId, TArray< TSharedRef<FOnlineBlockedPlayer> >& OutBlockedPlayers)
{
	return false;
}

void FOnlineFriendsIOS::DumpBlockedPlayers() const
{
}