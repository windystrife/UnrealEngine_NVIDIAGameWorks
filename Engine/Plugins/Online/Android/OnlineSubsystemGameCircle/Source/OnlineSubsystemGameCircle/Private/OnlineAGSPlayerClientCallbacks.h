// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AGS/PlayerClientInterface.h"

class FOnlineSubsystemGameCircle;


class FOnlineSignedInStateChangedListener : public AmazonGames::ISignedInStateChangedListener
{
public:

	FOnlineSignedInStateChangedListener(FOnlineSubsystemGameCircle *const InSubsystem);

	virtual void onSignedInStateChanged(bool isSignedIn) final;

private:

	FOnlineSignedInStateChangedListener();

	FOnlineSubsystemGameCircle *const GameCircleSubsystem;
};


class FOnlineGetLocalPlayerCallback : public AmazonGames::IGetLocalPlayerCb
{
public:

	FOnlineGetLocalPlayerCallback(FOnlineSubsystemGameCircle *const InSubsystem);

	virtual void onGetLocalPlayerCb(AmazonGames::ErrorCode errorCode, const AmazonGames::PlayerInfo* responseStruct, int developerTag) final;

private:

	FOnlineGetLocalPlayerCallback();

	FOnlineSubsystemGameCircle *const GameCircleSubsystem;
};


class FOnlineGetFriendIdsCallback : public AmazonGames::IGetFriendIdsCb
{
public:

	FOnlineGetFriendIdsCallback(FOnlineSubsystemGameCircle *const InSubsystem);

	virtual void onGetFriendIdsCb(AmazonGames::ErrorCode errorCode, const AmazonGames::FriendIdList* responseStruct, int developerTag) final;

private:

	FOnlineGetFriendIdsCallback();

	FOnlineSubsystemGameCircle *const GameCircleSubsystem;
};


class FOnlineGetBatchFriendsCallback : public AmazonGames::IGetBatchFriendsCb
{
public:

	FOnlineGetBatchFriendsCallback(FOnlineSubsystemGameCircle *const InSubsystem);

	virtual void onGetBatchFriendsCb(AmazonGames::ErrorCode errorCode, const AmazonGames::FriendList* responseStruct, int developerTag) final;

private:

	FOnlineGetBatchFriendsCallback();

	FOnlineSubsystemGameCircle *const GameCircleSubsystem;
};
