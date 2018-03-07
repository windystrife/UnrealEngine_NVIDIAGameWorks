// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineFriendsInterfaceOculus.h"
#include "OnlineSubsystemOculusPrivate.h"
#include "OnlineSubsystemOculusPackage.h"

FOnlineFriendsOculus::FOnlineFriendsOculus(FOnlineSubsystemOculus& InSubsystem)
: OculusSubsystem(InSubsystem)
{
}

const FString FOnlineFriendsOculus::FriendsListInviteableUsers = TEXT("invitableUsers");

bool FOnlineFriendsOculus::ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate)
{
	if (ListName == EFriendsLists::ToString(EFriendsLists::Default) || ListName == EFriendsLists::ToString(EFriendsLists::OnlinePlayers))
	{
		OculusSubsystem.AddRequestDelegate(
			ovr_User_GetLoggedInUserFriends(),
			FOculusMessageOnCompleteDelegate::CreateLambda([this, LocalUserNum, ListName, Delegate](ovrMessageHandle Message, bool bIsError)
		{
			OnQueryFriendsComplete(Message, bIsError, LocalUserNum, ListName, PlayerFriends, /* bAppendToExistingMap */ false, Delegate);
		}));
		return true;
	}

	if (ListName == FriendsListInviteableUsers)
	{
		OculusSubsystem.AddRequestDelegate(
			ovr_Room_GetInvitableUsers(),
			FOculusMessageOnCompleteDelegate::CreateLambda([this, LocalUserNum, ListName, Delegate](ovrMessageHandle Message, bool bIsError)
		{
			OnQueryFriendsComplete(Message, bIsError, LocalUserNum, ListName, InvitableUsers, /* bAppendToExistingMap */ false, Delegate);
		}));
		return true;
	}

	Delegate.ExecuteIfBound(LocalUserNum, false, ListName, TEXT("Invalid friends list"));
	return false;
}

void FOnlineFriendsOculus::OnQueryFriendsComplete(ovrMessageHandle Message, bool bIsError, int32 LocalUserNum, const FString& ListName, TMap<uint64, TSharedRef<FOnlineFriend>>& OutList, bool bAppendToExistingMap, const FOnReadFriendsListComplete& Delegate)
{
	FString ErrorStr;
	if (bIsError)
	{
		auto Error = ovr_Message_GetError(Message);
		auto ErrorMessage = ovr_Error_GetMessage(Error);
		ErrorStr = FString(ErrorMessage);
		Delegate.ExecuteIfBound(LocalUserNum, false, ListName, ErrorStr);
		return;
	}

	auto UserArray = ovr_Message_GetUserArray(Message);
	auto UserNum = ovr_UserArray_GetSize(UserArray);

	if (!bAppendToExistingMap)
	{
		OutList.Empty(UserNum);
	}

	for (size_t FriendIndex = 0; FriendIndex < UserNum; ++FriendIndex)
	{
		auto Friend = ovr_UserArray_GetElement(UserArray, FriendIndex);
		auto FriendId = ovr_User_GetID(Friend);
		auto FriendDisplayName = ovr_User_GetOculusID(Friend);
		auto FriendPresenceStatus = ovr_User_GetPresenceStatus(Friend);
		auto FriendInviteToken = ovr_User_GetInviteToken(Friend);
		FString FriendInviteTokenString(UTF8_TO_TCHAR((FriendInviteToken != nullptr) ? FriendInviteToken : ""));

		TSharedRef<FOnlineOculusFriend> OnlineFriend(new FOnlineOculusFriend(FriendId, FriendDisplayName, FriendPresenceStatus, FriendInviteTokenString));

		OutList.Add(FriendId, OnlineFriend);
	}

	bool bHasPaging = ovr_UserArray_HasNextPage(UserArray);
	if (bHasPaging)
	{
		OculusSubsystem.AddRequestDelegate(
			ovr_User_GetNextUserArrayPage(UserArray),
			FOculusMessageOnCompleteDelegate::CreateLambda([this, LocalUserNum, ListName, &OutList, Delegate](ovrMessageHandle InMessage, bool bInIsError)
		{
			OnQueryFriendsComplete(InMessage, bInIsError, LocalUserNum, ListName, OutList, /* bAppendToExistingMap */ true, Delegate);
		}));
	}
	else
	{
		Delegate.ExecuteIfBound(LocalUserNum, true, ListName, ErrorStr);
	}

}

bool FOnlineFriendsOculus::DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate)
{
	/** Does not exist in LibOVRPlatform */
	Delegate.ExecuteIfBound(LocalUserNum, false, ListName, TEXT("Not implemented"));
	return false;
}

bool FOnlineFriendsOculus::SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnSendInviteComplete& Delegate)
{
	/** Does not exist in LibOVRPlatform */
	Delegate.ExecuteIfBound(LocalUserNum, false, FriendId, ListName, TEXT("Not implemented"));
	return false;
}

bool FOnlineFriendsOculus::AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate)
{
	/** Does not exist in LibOVRPlatform */
	Delegate.ExecuteIfBound(LocalUserNum, false, FriendId, ListName, TEXT("Not implemented"));
	return false;
}

bool FOnlineFriendsOculus::RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	/** Does not exist in LibOVRPlatform */	
	return false;
}

bool FOnlineFriendsOculus::DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	/** Does not exist in LibOVRPlatform */
	return false;
}

bool FOnlineFriendsOculus::GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray<TSharedRef<FOnlineFriend>>& OutFriends)
{
	if (ListName == EFriendsLists::ToString(EFriendsLists::Default))
	{
		PlayerFriends.GenerateValueArray(OutFriends);
		return true;
	}
	if (ListName == EFriendsLists::ToString(EFriendsLists::OnlinePlayers))
	{
		for (auto Friend : PlayerFriends)
		{
			if (Friend.Value->GetPresence().bIsOnline)
			{
				OutFriends.Add(Friend.Value);
			}
		}
		return true;
	}
	if (ListName == FriendsListInviteableUsers)
	{
		InvitableUsers.GenerateValueArray(OutFriends);
		return true;
	}

	return false;
}

TSharedPtr<FOnlineFriend> FOnlineFriendsOculus::GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	auto OculusFriendId = static_cast<const FUniqueNetIdOculus&>(FriendId);

	if (ListName == EFriendsLists::ToString(EFriendsLists::Default))
	{
		if (!PlayerFriends.Contains(OculusFriendId.GetID()))
		{
			return nullptr;
		}
		return PlayerFriends[OculusFriendId.GetID()];
	}

	if (ListName == EFriendsLists::ToString(EFriendsLists::OnlinePlayers))
	{
		if (!PlayerFriends.Contains(OculusFriendId.GetID()))
		{
			return nullptr;
		}

		auto Friend = PlayerFriends[OculusFriendId.GetID()];
		if (!Friend->GetPresence().bIsOnline)
		{
			return nullptr;
		}

		return Friend;
	}

	if (ListName == FriendsListInviteableUsers)
	{
		if (!InvitableUsers.Contains(OculusFriendId.GetID()))
		{
			return nullptr;
		}
		return InvitableUsers[OculusFriendId.GetID()];
	}

	return nullptr;
}

bool FOnlineFriendsOculus::IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	auto Friend = GetFriend(0, FriendId, ListName);
	return Friend.IsValid();
}

bool FOnlineFriendsOculus::QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace)
{
	/** Does not exist in LibOVRPlatform */
	return false;
}

bool FOnlineFriendsOculus::GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray< TSharedRef<FOnlineRecentPlayer> >& OutRecentPlayers)
{
	/** Does not exist in LibOVRPlatform */
	return false;
}

bool FOnlineFriendsOculus::BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	/** Does not exist in LibOVRPlatform */
	return false;
}

bool FOnlineFriendsOculus::UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	/** Does not exist in LibOVRPlatform */
	return false;
}

bool FOnlineFriendsOculus::QueryBlockedPlayers(const FUniqueNetId& UserId)
{
	/** Does not exist in LibOVRPlatform */
	return false;
}

bool FOnlineFriendsOculus::GetBlockedPlayers(const FUniqueNetId& UserId, TArray< TSharedRef<FOnlineBlockedPlayer> >& OutBlockedPlayers)
{
	/** Does not exist in LibOVRPlatform */
	return false;
}

void FOnlineFriendsOculus::DumpBlockedPlayers() const
{
	/** Does not exist in LibOVRPlatform */
}
