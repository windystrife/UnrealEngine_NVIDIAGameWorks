// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineStats.h"

THIRD_PARTY_INCLUDES_START
#include "gpg/leaderboard_manager.h"
THIRD_PARTY_INCLUDES_END

class FOnlineSubsystemGooglePlay;

class FOnlineAsyncTaskGooglePlayReadLeaderboard : public FOnlineAsyncTaskBasic<FOnlineSubsystemGooglePlay>
{
public:
	FOnlineAsyncTaskGooglePlayReadLeaderboard(
		FOnlineSubsystemGooglePlay* InSubsystem,
		FOnlineLeaderboardReadRef& InReadObject,
		const FString& InLeaderboardId);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("ReadLeaderboard"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

	// FOnlineAsyncTask
	virtual void Tick() override;

private:
	/** Leaderboard read data */
	FOnlineLeaderboardReadRef ReadObject;

	/** Google Play leaderboard id */
	FString LeaderboardId;

	/** API query result */
	gpg::LeaderboardManager::FetchScoreSummaryResponse Response;
};
