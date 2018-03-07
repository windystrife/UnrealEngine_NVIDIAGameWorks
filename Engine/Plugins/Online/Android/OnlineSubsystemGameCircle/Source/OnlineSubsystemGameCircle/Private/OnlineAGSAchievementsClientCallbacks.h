// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AGS/AchievementsClientInterface.h"
#include "OnlineAchievementsInterface.h"
#include "OnlineSubsystemTypes.h"

class FOnlineSubsystemGameCircle;

class FOnlineGetAchievementsCallback : public AmazonGames::IGetAchievementsCb
{
public:

	FOnlineGetAchievementsCallback(FOnlineSubsystemGameCircle *const InSubsystem, 
									const FUniqueNetIdString& InUserId,
									const FOnQueryAchievementsCompleteDelegate& InDelegate);

	virtual void onGetAchievementsCb(AmazonGames::ErrorCode errorCode, 
										const AmazonGames::AchievementsData* responseStruct,
										int developerTag) final;

private:

	FOnlineGetAchievementsCallback();

	FOnlineSubsystemGameCircle *const GameCircleSubsystem;
	FUniqueNetIdString UserID;
	FOnQueryAchievementsCompleteDelegate Delegate;
};


class FOnlineUpdateProgressCallback : public AmazonGames::IUpdateProgressCb
{
public:

	FOnlineUpdateProgressCallback(FOnlineSubsystemGameCircle *const InSubsystem);

	virtual void onUpdateProgressCb(
		AmazonGames::ErrorCode errorCode,
		const AmazonGames::UpdateProgressResponse* responseStruct,
		int developerTag) final;

private:

	FOnlineUpdateProgressCallback();

	FOnlineSubsystemGameCircle *const GameCircleSubsystem;
};
