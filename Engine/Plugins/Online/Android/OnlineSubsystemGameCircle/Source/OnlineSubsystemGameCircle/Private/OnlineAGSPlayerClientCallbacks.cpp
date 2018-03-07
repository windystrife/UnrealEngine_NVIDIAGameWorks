// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineAGSPlayerClientCallbacks.h"
#include "OnlineSubsystemGameCircle.h"


// This callback is a listener and will be called multiple times. Do not use the callback manager
FOnlineSignedInStateChangedListener::FOnlineSignedInStateChangedListener(FOnlineSubsystemGameCircle *const InSubsystem)
	: GameCircleSubsystem(InSubsystem)
{
	check(GameCircleSubsystem);
}

void FOnlineSignedInStateChangedListener::onSignedInStateChanged(bool isSignedIn)
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOnlineSignedInStateChangedListener::onSignedInStateChanged  isSignedIn = %s"), isSignedIn ? TEXT("TRUE") : TEXT("FALSE"));
	GameCircleSubsystem->GetIdentityGameCircle()->SetSignedInState(isSignedIn);
}


FOnlineGetLocalPlayerCallback::FOnlineGetLocalPlayerCallback(FOnlineSubsystemGameCircle *const InSubsystem)
	: GameCircleSubsystem(InSubsystem)
{
	check(GameCircleSubsystem && GameCircleSubsystem->GetCallbackManager());
	GameCircleSubsystem->GetCallbackManager()->AddActiveCallback(this);
}

void FOnlineGetLocalPlayerCallback::onGetLocalPlayerCb(AmazonGames::ErrorCode errorCode, const AmazonGames::PlayerInfo* responseStruct, int developerTag)
{
	GameCircleSubsystem->GetIdentityGameCircle()->OnGetLocalPlayerPlayerCallback(errorCode, responseStruct);
	GameCircleSubsystem->GetCallbackManager()->CallbackCompleted(this);
}


FOnlineGetFriendIdsCallback::FOnlineGetFriendIdsCallback(FOnlineSubsystemGameCircle *const InSubsystem)
	: GameCircleSubsystem(InSubsystem)
{
	check(GameCircleSubsystem && GameCircleSubsystem->GetCallbackManager());
	GameCircleSubsystem->GetCallbackManager()->AddActiveCallback(this);
}

void FOnlineGetFriendIdsCallback::onGetFriendIdsCb(AmazonGames::ErrorCode errorCode, const AmazonGames::FriendIdList* responseStruct, int developerTag)
{
	GameCircleSubsystem->GetFriendsGameCircle()->OnGetFriendIdsCallback(errorCode, responseStruct);
	GameCircleSubsystem->GetCallbackManager()->CallbackCompleted(this);
}


FOnlineGetBatchFriendsCallback::FOnlineGetBatchFriendsCallback(FOnlineSubsystemGameCircle *const InSubsystem)
	: GameCircleSubsystem(InSubsystem)
{
	check(GameCircleSubsystem && GameCircleSubsystem->GetCallbackManager());
	GameCircleSubsystem->GetCallbackManager()->AddActiveCallback(this);
}

void FOnlineGetBatchFriendsCallback::onGetBatchFriendsCb(AmazonGames::ErrorCode errorCode, const AmazonGames::FriendList* responseStruct, int developerTag)
{
	GameCircleSubsystem->GetFriendsGameCircle()->OnGetBatchFriendsCallback(errorCode, responseStruct);
	GameCircleSubsystem->GetCallbackManager()->CallbackCompleted(this);
}
