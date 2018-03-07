// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemTypes.h"
#include "OnlinePresenceInterface.h"


class FOnlineFriendGameCircle : public FOnlineFriend
{
public:

	// Use this constructor
	FOnlineFriendGameCircle(TSharedPtr<const FUniqueNetId> InUniqueId, const FString& InPlayerAlias, const FString& InAvatarURL);

	//~ Begin FOnlineUser
	virtual TSharedRef<const FUniqueNetId> GetUserId() const override;
	virtual FString GetRealName() const override;
	virtual FString GetDisplayName(const FString& Platform = FString()) const override;
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;
	//~ End FOnlineUser

	//~ Begin FOnlineFriend
	virtual EInviteStatus::Type GetInviteStatus() const override;
	virtual const FOnlineUserPresence& GetPresence() const override;
	//~ End FOnlineFriend

private:

	// Hide the default constructor
	FOnlineFriendGameCircle() {};

	TSharedPtr<const FUniqueNetId> PlayerId;
	FString PlayerAlias;
	FString AvatarURL;

	FOnlineUserPresence Presence;
};
