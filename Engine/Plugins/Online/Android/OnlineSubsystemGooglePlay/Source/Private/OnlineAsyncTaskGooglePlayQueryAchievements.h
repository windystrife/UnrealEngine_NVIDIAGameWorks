// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemGooglePlayPackage.h"
#include "OnlineAchievementsInterface.h"

THIRD_PARTY_INCLUDES_START
#include "gpg/achievement_manager.h"
THIRD_PARTY_INCLUDES_END

class FOnlineSubsystemGooglePlay;

class FOnlineAsyncTaskGooglePlayQueryAchievements : public FOnlineAsyncTaskBasic<FOnlineSubsystemGooglePlay>
{
public:
	FOnlineAsyncTaskGooglePlayQueryAchievements(
		FOnlineSubsystemGooglePlay* InSubsystem,
		const FUniqueNetIdString& InUserId,
		const FOnQueryAchievementsCompleteDelegate& InDelegate);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("QueryAchievements"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

	// FOnlineAsyncTask
	virtual void Tick() override;

private:
	FUniqueNetIdString UserId;
	FOnQueryAchievementsCompleteDelegate Delegate;
	gpg::AchievementManager::FetchAllResponse Response;
};
