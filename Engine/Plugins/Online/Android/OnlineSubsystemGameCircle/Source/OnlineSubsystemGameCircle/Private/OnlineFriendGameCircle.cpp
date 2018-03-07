// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineFriendGameCircle.h"
#include "OnlinePresenceInterface.h"
#include "OnlineSubsystem.h"

FOnlineFriendGameCircle::FOnlineFriendGameCircle(TSharedPtr<const FUniqueNetId> InUniqueId, const FString& InPlayerAlias, const FString& InAvatarURL)
	: PlayerId(InUniqueId)
	, PlayerAlias(InPlayerAlias)
	, AvatarURL(InAvatarURL)
{
}

TSharedRef<const FUniqueNetId> FOnlineFriendGameCircle::GetUserId() const
{
	return PlayerId.ToSharedRef();
}

FString FOnlineFriendGameCircle::GetRealName() const
{
	UE_LOG_ONLINE(Warning, TEXT("FOnlineFriendGameCircle::GetRealName - No real name for player. Returning PlayerAlias"));
	return PlayerAlias;
}

FString FOnlineFriendGameCircle::GetDisplayName(const FString& Platform) const
{
	return PlayerAlias;
}

bool FOnlineFriendGameCircle::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	if(AttrName.Equals(TEXT("AvatarURL"), ESearchCase::IgnoreCase))
	{
		OutAttrValue = AvatarURL;
		return true;
	}

	return false;
}

EInviteStatus::Type FOnlineFriendGameCircle::GetInviteStatus() const
{
	UE_LOG_ONLINE(Warning, TEXT("FOnlineFriendGameCircle::GetInviteStatus - No Invite Implementation"));
	return EInviteStatus::Unknown;
}

const FOnlineUserPresence& FOnlineFriendGameCircle::GetPresence() const
{
	UE_LOG_ONLINE(Warning, TEXT("FOnlineFriendGameCircle::GetPresence - No UserPresence Implementation"));
	return Presence;
}
