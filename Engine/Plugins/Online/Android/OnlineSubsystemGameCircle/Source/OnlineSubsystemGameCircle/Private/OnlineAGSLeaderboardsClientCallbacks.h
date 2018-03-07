// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AGS/LeaderboardsClientInterface.h"

class FOnlineSubsystemGameCircle;


class FOnlineGetPlayerScoreCallback : public AmazonGames::ILeaderboardGetPlayerScoreCb
{
public:

	FOnlineGetPlayerScoreCallback(FOnlineSubsystemGameCircle *const InSubsystem);

	virtual void onGetPlayerScoreCb(AmazonGames::ErrorCode errorCode, const AmazonGames::PlayerScoreInfo* response, int developerTag) final;

private:

	FOnlineGetPlayerScoreCallback();

	FOnlineSubsystemGameCircle *const GameCircleSubsystem;
};


class FOnlineGetFriendsScoresCallback : public AmazonGames::ILeaderboardGetScoresCb
{
public:

	FOnlineGetFriendsScoresCallback(FOnlineSubsystemGameCircle *const InSubsystem);
	
	virtual void onGetScoresCb(AmazonGames::ErrorCode errorCode, const AmazonGames::LeaderboardScores* scoresResponse, int developerTag) final;

private:

	FOnlineGetFriendsScoresCallback();

	FOnlineSubsystemGameCircle *const GameCircleSubsystem;
};


class FOnlineSubmitScoreCallback : public AmazonGames::ILeaderboardSubmitScoreCb
{
public:

	FOnlineSubmitScoreCallback(FOnlineSubsystemGameCircle *const InSubsystem);

	virtual void onSubmitScoreCb(AmazonGames::ErrorCode errorCode, const AmazonGames::SubmitScoreResponse* submitScoreResponse, int developerTag) final;

private:

	FOnlineSubmitScoreCallback();

	FOnlineSubsystemGameCircle *const GameCircleSubsystem;
};
