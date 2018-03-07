// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineLeaderboardInterfaceGameCircle.h"
#include "Online.h"
#include "OnlineSubsystemGameCircle.h"
#include "OnlineAGSLeaderboardsClientCallbacks.h"
#include "Async/TaskGraphInterfaces.h"

FOnlineLeaderboardsGameCircle::FOnlineLeaderboardsGameCircle(FOnlineSubsystemGameCircle* InSubsystem)
	: Subsystem(InSubsystem)
	, bFlushInProgress(false)
	, FriendReadObject(nullptr)
{
	check(Subsystem);
	ReadFriendsListDelegate.BindRaw(this, &FOnlineLeaderboardsGameCircle::OnReadFriendsListComplete);
}

bool FOnlineLeaderboardsGameCircle::ReadLeaderboards(const TArray< TSharedRef<const FUniqueNetId> >& Players, FOnlineLeaderboardReadRef& ReadObject)
{
	if(PlayersToQuery.Num() > 0 || PlayerReadObject.IsValid() || Subsystem->GetIdentityGameCircle()->GetLoginStatus(0) != ELoginStatus::LoggedIn)
	{
		UE_LOG_ONLINE(Warning, TEXT("Leaderboards Query Already In Progress"));
		return false;
	}
	else if(Players.Num() == 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("No players passed in for ReadLeaderboards"));
		return false;
	}

	PlayersToQuery = Players;
	PlayerReadObject = MakeShareable(&ReadObject.Get());
	PlayerReadObject->ReadState = EOnlineAsyncTaskState::InProgress;
	
	GetScoreForNextPlayer( FOnlineSubsystemGameCircle::ConvertFStringToStdString( ReadObject->LeaderboardName.ToString() ).c_str() );

	return true;
}

bool FOnlineLeaderboardsGameCircle::ReadLeaderboardsForFriends(int32 LocalUserNum, FOnlineLeaderboardReadRef& ReadObject)
{
	ReadObject->ReadState = EOnlineAsyncTaskState::InProgress;

	UE_LOG_ONLINE(Display, TEXT("Requesting Friend Ids From AmazonGames Interface"));
	FriendReadObject = MakeShareable(&ReadObject.Get());
	FString EmptyString;

	if(Subsystem->GetFriendsGameCircle()->HasLocalFriendsList())
	{
		OnReadFriendsListComplete(LocalUserNum, true, EmptyString, EmptyString);
	}
	else
	{
		Subsystem->GetFriendsGameCircle()->ReadFriendsList(LocalUserNum, EmptyString, ReadFriendsListDelegate);
	}
	
	return true;
}

bool FOnlineLeaderboardsGameCircle::ReadLeaderboardsAroundRank(int32 Rank, uint32 Range, FOnlineLeaderboardReadRef& ReadObject)
{
	UE_LOG_ONLINE(Warning, TEXT("FOnlineLeaderboardsGameCircle::ReadLeaderboardsAroundRank is currently not supported."));
	return false;
}
bool FOnlineLeaderboardsGameCircle::ReadLeaderboardsAroundUser(TSharedRef<const FUniqueNetId> Player, uint32 Range, FOnlineLeaderboardReadRef& ReadObject)
{
	UE_LOG_ONLINE(Warning, TEXT("FOnlineLeaderboardsGameCircle::ReadLeaderboardsAroundUser is currently not supported."));
	return false;
}

void FOnlineLeaderboardsGameCircle::FreeStats(FOnlineLeaderboardRead& ReadObject)
{
	// Game Circle has no functionality supporting this
}

bool FOnlineLeaderboardsGameCircle::WriteLeaderboards(const FName& SessionName, const FUniqueNetId& Player, FOnlineLeaderboardWrite& WriteObject)
{
	bool bWroteAnyLeaderboard = false;

	for(int32 LeaderboardIdx = 0; LeaderboardIdx < WriteObject.LeaderboardNames.Num(); ++LeaderboardIdx)
	{
		FString LeaderboardName = WriteObject.LeaderboardNames[LeaderboardIdx].ToString();
		UE_LOG_ONLINE(Display, TEXT("Going through stats for leaderboard :  %s "), *LeaderboardName);
		
		for(FStatPropertyArray::TConstIterator It(WriteObject.Properties); It; ++It)
		{
			const FVariantData& Stat = It.Value();
			uint64 Score;

			UE_LOG_ONLINE(Display, TEXT("Here's a stat"));

			//Google leaderboard stats are always a long/int64
			if(Stat.GetType() == EOnlineKeyValuePairDataType::Int64)
			{
				Stat.GetValue(Score);
				
				FOnlinePendingLeaderboardWrite* UnreportedScore = new (UnreportedScores) FOnlinePendingLeaderboardWrite();
				UnreportedScore->LeaderboardName = LeaderboardName;
				UnreportedScore->Score = Score;
				UE_LOG_ONLINE(Display, TEXT("FOnlineLeaderboardsAndroid::WriteLeaderboards() Int64 value Score: %d"), UnreportedScore->Score);

				bWroteAnyLeaderboard = true;
			}
			else if (Stat.GetType() == EOnlineKeyValuePairDataType::Int32)
			{
				// cast from 32 to 64
				int32 Score32 = 0;
				Stat.GetValue(Score32);
				Score = static_cast<uint64>(Score32);

				FOnlinePendingLeaderboardWrite* UnreportedScore = new (UnreportedScores) FOnlinePendingLeaderboardWrite();
				UnreportedScore->LeaderboardName = LeaderboardName;
				UnreportedScore->Score = Score;
				UE_LOG_ONLINE(Display, TEXT("FOnlineLeaderboardsAndroid::WriteLeaderboards() Int32 value Score: %d "), UnreportedScore->Score);

				bWroteAnyLeaderboard = true;
			}
		}
	}
	
	//Return whether any stat was cached
	return bWroteAnyLeaderboard;
}

bool FOnlineLeaderboardsGameCircle::FlushLeaderboards(const FName& SessionName)
{
	if(bFlushInProgress || Subsystem->GetIdentityGameCircle()->GetLoginStatus(0) != ELoginStatus::LoggedIn)
	{
		return false;
	}

	if(UnreportedScores.Num() > 0)
	{
		FlushSession = SessionName;
		bFlushInProgress = true;
		SubmitNextUnreportedScore();
	}
	else
	{
		TriggerFlushDelegatesOnGameThread(FlushSession, true);
	}

	return true;
}

bool FOnlineLeaderboardsGameCircle::WriteOnlinePlayerRatings(const FName& SessionName, int32 LeaderboardId, const TArray<FOnlinePlayerScore>& PlayerScores)
{
	//iOS doesn't support this, and there is no Google Play functionality for this either
	return false;
}

void FOnlineLeaderboardsGameCircle::OnGetPlayerScoreCallback(AmazonGames::ErrorCode InErrorCode, const AmazonGames::PlayerScoreInfo* InPlayerScore)
{
	if(InErrorCode != AmazonGames::ErrorCode::NO_ERROR)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(/*UE_LOG_ONLINE(Error, */TEXT("AmazonGames::LeaderboardClientInterface::getScoreForPlayer returned ErrorCode %d"), InErrorCode);
		PlayerReadObject->ReadState = EOnlineAsyncTaskState::Failed;
		TriggerReadDelegatesOnGameThread(false);
	}

	WriteRowForUIDToReadObject(PlayersToQuery[0], InPlayerScore->leaderboardId, InPlayerScore->rank, InPlayerScore->scoreValue, PlayerReadObject.ToSharedRef());

	PlayersToQuery.RemoveAt(0);
	if(PlayersToQuery.Num() > 0)
	{
		GetScoreForNextPlayer(InPlayerScore->leaderboardId);
	}
	else
	{
		PlayerReadObject->ReadState = EOnlineAsyncTaskState::Done;
		TriggerReadDelegatesOnGameThread(true);
	}
}

void FOnlineLeaderboardsGameCircle::OnGetFriendsScoresCallback(AmazonGames::ErrorCode InErrorCode, 
																const AmazonGames::LeaderboardScores *const InScoreResponse)
{
	if(InErrorCode != AmazonGames::ErrorCode::NO_ERROR)
	{
		UE_LOG_ONLINE(Error, TEXT("getScores FRIENDS_ALL_TIME returned error code %d"), InErrorCode);
		FriendReadObject->ReadState = EOnlineAsyncTaskState::Failed;
		TriggerReadDelegatesOnGameThread(false);
		FriendReadObject = nullptr;
		return;
	}

	TArray< TSharedRef<FOnlineFriend> > FriendsList;
	Subsystem->GetFriendsGameCircle()->GetFriendsList(0, FString(), FriendsList);
	FString ScoreAlias;
	int scoreIndex;
	int32 friendIndex;
	bool bMatchedAlias;

	for(scoreIndex = 0; scoreIndex < InScoreResponse->numScores; ++scoreIndex)
	{
		bMatchedAlias = false;
		ScoreAlias = InScoreResponse->scores[scoreIndex].playerAlias;
		for(friendIndex = 0; friendIndex < FriendsList.Num(); ++friendIndex)
		{
			if(ScoreAlias.Equals(FriendsList[friendIndex]->GetDisplayName()))
			{
				bMatchedAlias = true;
				break;
			}
		}

		if(bMatchedAlias)
		{
			WriteRowForUIDToReadObject(FriendsList[friendIndex]->GetUserId(), 
										InScoreResponse->scores[scoreIndex].leaderboardString, 
										InScoreResponse->scores[scoreIndex].rank, 
										InScoreResponse->scores[scoreIndex].scoreValue, 
										FriendReadObject.ToSharedRef());
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("Did not find matching alias in AmazonFriendList - %s"), *ScoreAlias);
		}
	}

	FriendReadObject->ReadState = EOnlineAsyncTaskState::Done;
	TriggerReadDelegatesOnGameThread(true);
	FriendReadObject = nullptr;
}

void FOnlineLeaderboardsGameCircle::OnSubmitScoreCallback(AmazonGames::ErrorCode InErrorCode, const AmazonGames::SubmitScoreResponse *const InSubmitScoreResponse)
{
	if(InErrorCode != AmazonGames::ErrorCode::NO_ERROR)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(/* UE_LOG_ONLINE(Warning, */TEXT("Submit Score Callback Received Error Code %d"), InErrorCode);
		bFlushInProgress = false;
		TriggerFlushDelegatesOnGameThread(FlushSession, false);
		return;
	}

	UnreportedScores.RemoveAt(0);
	if(UnreportedScores.Num() > 0)
	{
		SubmitNextUnreportedScore();
	}
	else
	{
		bFlushInProgress = false;
		TriggerFlushDelegatesOnGameThread(FlushSession, true);
	}
}

void FOnlineLeaderboardsGameCircle::OnReadFriendsListComplete(int32 LocalPlayer, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr)
{
	if(!bWasSuccessful)
	{
		FriendReadObject->ReadState = EOnlineAsyncTaskState::Failed;
		TriggerReadDelegatesOnGameThread(false);
		FriendReadObject = nullptr;
		UE_LOG_ONLINE(Warning, TEXT("ReadFriendsList was unsuccessful - %s"), *ErrorStr);
		return;
	}

	AmazonGames::LeaderboardsClientInterface::getScores(FOnlineSubsystemGameCircle::ConvertFStringToStdString(FriendReadObject->LeaderboardName.ToString()).c_str(),
														AmazonGames::LeaderboardFilter::FRIENDS_ALL_TIME, 
														new FOnlineGetFriendsScoresCallback(Subsystem));
}

void FOnlineLeaderboardsGameCircle::GetScoreForNextPlayer(const char *const LeaderboardID)
{
	check(PlayersToQuery.Num() > 0);
	const FUniqueNetIdString NetIdString(PlayersToQuery[0]->ToString());

	UE_LOG_ONLINE(Display, TEXT("Getting Score for Player Id - %s . %s"), *PlayersToQuery[0]->ToString(), *NetIdString.UniqueNetIdStr);

	AmazonGames::LeaderboardsClientInterface::getScoreForPlayer(LeaderboardID, 
																FOnlineSubsystemGameCircle::ConvertFStringToStdString(NetIdString.UniqueNetIdStr).c_str(),
																AmazonGames::GLOBAL_ALL_TIME,
																new FOnlineGetPlayerScoreCallback(Subsystem));
}

void FOnlineLeaderboardsGameCircle::SubmitNextUnreportedScore()
{
	check(UnreportedScores.Num() > 0);
	std::string ConvertedId;

	UE_LOG_ONLINE(Display, TEXT("Submitting an unreported score to \"%s\" . Value: %d "), *UnreportedScores[0].LeaderboardName, UnreportedScores[0].Score);

	ConvertedId = FOnlineSubsystemGameCircle::ConvertFStringToStdString(UnreportedScores[0].LeaderboardName);
	AmazonGames::LeaderboardsClientInterface::submitScore(ConvertedId.c_str(), UnreportedScores[0].Score, new FOnlineSubmitScoreCallback(Subsystem));
}

void FOnlineLeaderboardsGameCircle::WriteRowForUIDToReadObject(TSharedRef<const FUniqueNetId> UniqueID, 
															   const char *const InLeaderboardId, int InRank, int InValue, 
															   FOnlineLeaderboardReadRef ReadObject)
{
	FString LBId;
	LBId = InLeaderboardId;

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("WriteRowForUIDToReadObject %s %d %d"), *LBId, InRank, InValue);
	FOnlineStatsRow* UserRow = ReadObject.Get().FindPlayerRecord(*UniqueID);
	if (UserRow == NULL)
	{
		UserRow = new (ReadObject->Rows) FOnlineStatsRow(UniqueID->ToString(), UniqueID);
	}
	
	for (int32 statIndex = 0; statIndex < ReadObject->ColumnMetadata.Num(); statIndex++)
	{
		const FColumnMetaData& ColumnMeta = ReadObject->ColumnMetadata[statIndex];

		FVariantData* LastColumn = NULL;
		switch (ColumnMeta.DataType)
		{
		case EOnlineKeyValuePairDataType::Int32:
			{
				int32 Value = InValue;
				LastColumn = &(UserRow->Columns.Add(ColumnMeta.ColumnName, FVariantData(Value)));
				break;
			}

		default:
			{
				UE_LOG_ONLINE(Warning, TEXT("Unsupported key value pair during retrieval from Game Circle %s"), ColumnMeta.ColumnName);
				break;
			}
		}
	}
}

void FOnlineLeaderboardsGameCircle::TriggerReadDelegatesOnGameThread(bool bWasSuccessful)
{
	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.TriggerReadDelegatesOnGameThread"), STAT_FSimpleDelegateGraphTask_TriggerReadDelegatesOnGameThread, STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=](){
			TriggerOnLeaderboardReadCompleteDelegates(bWasSuccessful);
		}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_TriggerReadDelegatesOnGameThread), 
		nullptr, 
		ENamedThreads::GameThread
	);
}

void FOnlineLeaderboardsGameCircle::TriggerFlushDelegatesOnGameThread(const FName& SessionName, bool bWasSuccessful)
{
	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.TriggerFlushDelegatesOnGameThread"), STAT_FSimpleDelegateGraphTask_TriggerFlushDelegatesOnGameThread, STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=](){
			TriggerOnLeaderboardFlushCompleteDelegates(SessionName, bWasSuccessful);
		}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_TriggerFlushDelegatesOnGameThread), 
		nullptr, 
		ENamedThreads::GameThread
	);
}
