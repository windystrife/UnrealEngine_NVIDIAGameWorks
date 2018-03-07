// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineAGSLeaderboardsClientCallbacks.h"
#include "Async.h"
#include "OnlineSubsystemGameCircle.h"


FOnlineGetPlayerScoreCallback::FOnlineGetPlayerScoreCallback(FOnlineSubsystemGameCircle *const InSubsystem)
	: GameCircleSubsystem(InSubsystem)
{
	check(GameCircleSubsystem && GameCircleSubsystem->GetCallbackManager());
	GameCircleSubsystem->GetCallbackManager()->AddActiveCallback(this);
}

void FOnlineGetPlayerScoreCallback::onGetPlayerScoreCb(AmazonGames::ErrorCode errorCode, const AmazonGames::PlayerScoreInfo* response, int developerTag)
{
	GameCircleSubsystem->GetLeaderboardsGameCircle()->OnGetPlayerScoreCallback(errorCode, response);
	GameCircleSubsystem->GetCallbackManager()->CallbackCompleted(this);
}


FOnlineGetFriendsScoresCallback::FOnlineGetFriendsScoresCallback(FOnlineSubsystemGameCircle *const InSubsystem)
	: GameCircleSubsystem(InSubsystem)
{
	check(GameCircleSubsystem && GameCircleSubsystem->GetCallbackManager());
	GameCircleSubsystem->GetCallbackManager()->AddActiveCallback(this);
}

void FOnlineGetFriendsScoresCallback::onGetScoresCb(AmazonGames::ErrorCode errorCode, const AmazonGames::LeaderboardScores* scoresResponse, int developerTag)
{
	GameCircleSubsystem->GetLeaderboardsGameCircle()->OnGetFriendsScoresCallback(errorCode, scoresResponse);
	GameCircleSubsystem->GetCallbackManager()->CallbackCompleted(this);
}


FOnlineSubmitScoreCallback::FOnlineSubmitScoreCallback(FOnlineSubsystemGameCircle *const InSubsystem)
	: GameCircleSubsystem(InSubsystem)
{
	check(GameCircleSubsystem && GameCircleSubsystem->GetCallbackManager());
	GameCircleSubsystem->GetCallbackManager()->AddActiveCallback(this);
}

void FOnlineSubmitScoreCallback::onSubmitScoreCb(AmazonGames::ErrorCode errorCode, const AmazonGames::SubmitScoreResponse* submitScoreResponse, int developerTag)
{
	GameCircleSubsystem->GetLeaderboardsGameCircle()->OnSubmitScoreCallback(errorCode, submitScoreResponse);
	GameCircleSubsystem->GetCallbackManager()->CallbackCompleted(this);
}
