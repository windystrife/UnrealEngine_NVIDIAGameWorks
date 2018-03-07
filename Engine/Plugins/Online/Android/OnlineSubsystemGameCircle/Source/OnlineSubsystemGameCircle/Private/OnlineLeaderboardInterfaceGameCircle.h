// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineLeaderboardInterface.h"
#include "OnlineFriendsInterface.h"
#include "AGS/LeaderboardsClientInterface.h"

struct FOnlinePendingLeaderboardWrite
{
	FString LeaderboardName;
	uint64 Score;
};

/**
 * Interface definition for the online services leaderboard services 
 */
class FOnlineLeaderboardsGameCircle : public IOnlineLeaderboards
{
public:
	FOnlineLeaderboardsGameCircle(class FOnlineSubsystemGameCircle* InSubsystem);

	virtual bool ReadLeaderboards(const TArray< TSharedRef<const FUniqueNetId> >& Players, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsForFriends(int32 LocalUserNum, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsAroundRank(int32 Rank, uint32 Range, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsAroundUser(TSharedRef<const FUniqueNetId> Player, uint32 Range, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual void FreeStats(FOnlineLeaderboardRead& ReadObject) override;
	virtual bool WriteLeaderboards(const FName& SessionName, const FUniqueNetId& Player, FOnlineLeaderboardWrite& WriteObject) override;
	virtual bool FlushLeaderboards(const FName& SessionName) override;
	virtual bool WriteOnlinePlayerRatings(const FName& SessionName, int32 LeaderboardId, const TArray<FOnlinePlayerScore>& PlayerScores) override;

PACKAGE_SCOPE:

	void OnGetPlayerScoreCallback(AmazonGames::ErrorCode InErrorCode, const AmazonGames::PlayerScoreInfo* InPlayerScore);

	void OnGetFriendsScoresCallback(AmazonGames::ErrorCode InErrorCode,
									const AmazonGames::LeaderboardScores *const InScoreResponse);

	void OnSubmitScoreCallback(AmazonGames::ErrorCode InErrorCode,
								const AmazonGames::SubmitScoreResponse *const InSubmitScoreResponse);

	void OnReadFriendsListComplete(int32 LocalPlayer, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr);

private:

	void GetScoreForNextPlayer(const char *const LeaderboardID);

	void SubmitNextUnreportedScore();

	void WriteRowForUIDToReadObject(TSharedRef<const FUniqueNetId> UniqueID, const char *const InLeaderboardId, int InRank, int InValue, FOnlineLeaderboardReadRef ReadObject);

	void TriggerReadDelegatesOnGameThread(bool bWasSuccessful);

	void TriggerFlushDelegatesOnGameThread(const FName& SessionName, bool bWasSuccessful);

	/** Pointer to owning subsystem */
	FOnlineSubsystemGameCircle* Subsystem;

	/* Save the player Ids passed in to ReadLeaderboards so we can get their scores one at a time */
	TArray< TSharedRef<const FUniqueNetId> > PlayersToQuery;
	FOnlineLeaderboardReadPtr PlayerReadObject;

	FOnlineLeaderboardReadPtr FriendReadObject;
	FOnReadFriendsListComplete ReadFriendsListDelegate;

	/** Scores are cached here in WriteLeaderboards until FlushLeaderboards is called */
	TArray<FOnlinePendingLeaderboardWrite> UnreportedScores;
	FName FlushSession;
	bool bFlushInProgress;
};

typedef TSharedPtr<FOnlineLeaderboardsGameCircle, ESPMode::ThreadSafe> FOnlineLeaderboardsGameCirclePtr;
