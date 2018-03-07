// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncTaskGooglePlayReadLeaderboard.h"
#include "OnlineSubsystemGooglePlay.h"
#include "OnlineIdentityInterfaceGooglePlay.h"
#include "OnlineLeaderboardInterfaceGooglePlay.h"

THIRD_PARTY_INCLUDES_START
#include "gpg/leaderboard_manager.h"
THIRD_PARTY_INCLUDES_END

using namespace gpg;

FOnlineAsyncTaskGooglePlayReadLeaderboard::FOnlineAsyncTaskGooglePlayReadLeaderboard(
	FOnlineSubsystemGooglePlay* InSubsystem,
	FOnlineLeaderboardReadRef& InReadObject,
	const FString& InLeaderboardId)
	: FOnlineAsyncTaskBasic(InSubsystem)
	, ReadObject(InReadObject)
	, LeaderboardId(InLeaderboardId)
{
	Response.status = ResponseStatus::ERROR_TIMEOUT;
}

void FOnlineAsyncTaskGooglePlayReadLeaderboard::Finalize()
{
	ReadObject->ReadState = bWasSuccessful ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;
}

void FOnlineAsyncTaskGooglePlayReadLeaderboard::TriggerDelegates()
{
	Subsystem->GetLeaderboardsInterface()->TriggerOnLeaderboardReadCompleteDelegates(bWasSuccessful);
}

void FOnlineAsyncTaskGooglePlayReadLeaderboard::Tick()
{
	// Convert our FString leaderboard ID to ASCII for the API's std::string.
	auto ConvertedId = FOnlineSubsystemGooglePlay::ConvertFStringToStdString(LeaderboardId);
	Response = Subsystem->GetGameServices()->Leaderboards().FetchScoreSummaryBlocking(
		DataSource::CACHE_OR_NETWORK,
		Timeout(10000),
		ConvertedId,
		LeaderboardTimeSpan::ALL_TIME,
		LeaderboardCollection::PUBLIC);

	bWasSuccessful = false;

	if (Response.status != ResponseStatus::VALID)
	{
		bIsComplete = true;
		return;
	}

	// We can only get the current user's leaderboard score from Google Play, so just add one row with it.
	//TSharedRef<const FUniqueNetId> UserId = MakeShareable(new FUniqueNetIdString(FString(TEXT("0"))));
	auto UserId = Subsystem->GetIdentityGooglePlay()->GetCurrentUserId();
	if (!UserId.IsValid())
	{
		// If there's no user signed in, can't get leaderboard.
		bIsComplete = true;
		return;
	}

	FOnlineStatsRow* UserRow = ReadObject.Get().FindPlayerRecord(*UserId);
	if (UserRow == NULL)
	{
		UserRow = new (ReadObject->Rows) FOnlineStatsRow(UserId->ToString(), UserId.ToSharedRef());
	}

	for (int32 StatIdx = 0; StatIdx < ReadObject->ColumnMetadata.Num(); StatIdx++)
	{
		const FColumnMetaData& ColumnMeta = ReadObject->ColumnMetadata[StatIdx];

		FVariantData* LastColumn = NULL;
		switch (ColumnMeta.DataType)
		{
			case EOnlineKeyValuePairDataType::Int32:
			{
				int32 Value = Response.data.CurrentPlayerScore().Value();
				LastColumn = &(UserRow->Columns.Add(ColumnMeta.ColumnName, FVariantData(Value)));
				bWasSuccessful = true;
				break;
			}

			default:
			{
				UE_LOG_ONLINE(Warning, TEXT("Unsupported key value pair during retrieval from Google Play %s"), ColumnMeta.ColumnName);
				break;
			}
		}
	}
	
	bIsComplete = true;
}

