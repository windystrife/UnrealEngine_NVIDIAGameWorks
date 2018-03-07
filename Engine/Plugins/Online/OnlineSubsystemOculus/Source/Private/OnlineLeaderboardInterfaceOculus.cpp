// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineLeaderboardInterfaceOculus.h"
#include "OnlineSubsystemOculusPrivate.h"
#include "OnlineIdentityOculus.h"
#include "OnlineSubsystemOculusPackage.h"

FOnlineLeaderboardOculus::FOnlineLeaderboardOculus(class FOnlineSubsystemOculus& InSubsystem)
: OculusSubsystem(InSubsystem)
{
}

bool FOnlineLeaderboardOculus::ReadLeaderboards(const TArray< TSharedRef<const FUniqueNetId> >& Players, FOnlineLeaderboardReadRef& ReadObject)
{
	bool bOnlyLoggedInUser = false;
	if (Players.Num() > 0) 
	{
		auto LoggedInPlayerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(0);
		if (Players.Num() == 1 && LoggedInPlayerId.IsValid() && *Players[0] == *LoggedInPlayerId)
		{
			bOnlyLoggedInUser = true;
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("Filtering by player ids other than the logged in player is not supported.  Ignoring the 'Players' parameter"));
		}
	}
	return ReadOculusLeaderboards(/* Only Friends */ false, bOnlyLoggedInUser, ReadObject);
};

bool FOnlineLeaderboardOculus::ReadLeaderboardsForFriends(int32 LocalUserNum, FOnlineLeaderboardReadRef& ReadObject)
{
	return ReadOculusLeaderboards(/* Only Friends */ true, /* Only Logged In User */ false, ReadObject);
}

bool FOnlineLeaderboardOculus::ReadLeaderboardsAroundRank(int32 Rank, uint32 Range, FOnlineLeaderboardReadRef& ReadObject)
{
	// UNDONE
	return false;
}

bool FOnlineLeaderboardOculus::ReadLeaderboardsAroundUser(TSharedRef<const FUniqueNetId> Player, uint32 Range, FOnlineLeaderboardReadRef& ReadObject)
{
	// UNDONE
	return false;
}

bool FOnlineLeaderboardOculus::ReadOculusLeaderboards(bool bOnlyFriends, bool bOnlyLoggedInUser, FOnlineLeaderboardReadRef& ReadObject)
{
	auto FilterType = (bOnlyFriends) ? ovrLeaderboard_FilterFriends : ovrLeaderboard_FilterNone;

	auto Limit = 100;
	auto StartAt = ovrLeaderboard_StartAtTop;

	// If only getting the logged in user, then only return back one result
	if (bOnlyLoggedInUser)
	{
		Limit = 1;
		StartAt = ovrLeaderboard_StartAtCenteredOnViewer;
	}

	ReadObject->ReadState = EOnlineAsyncTaskState::InProgress;
	OculusSubsystem.AddRequestDelegate(
		ovr_Leaderboard_GetEntries(TCHAR_TO_ANSI(*ReadObject->LeaderboardName.ToString()), Limit, FilterType, StartAt),
		FOculusMessageOnCompleteDelegate::CreateLambda([this, ReadObject](ovrMessageHandle Message, bool bIsError)
	{
		OnReadLeaderboardsComplete(Message, bIsError, ReadObject);
	}));
	return true;
}

void FOnlineLeaderboardOculus::OnReadLeaderboardsComplete(ovrMessageHandle Message, bool bIsError, const FOnlineLeaderboardReadRef& ReadObject)
{
	if (bIsError)
	{
		ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
		TriggerOnLeaderboardReadCompleteDelegates(false);
		return;
	}

	auto LeaderboardArray = ovr_Message_GetLeaderboardEntryArray(Message);
	auto LeaderboardArraySize = ovr_LeaderboardEntryArray_GetSize(LeaderboardArray);

	EOnlineKeyValuePairDataType::Type ScoreType = EOnlineKeyValuePairDataType::Int64;

	for (auto Metadata : ReadObject->ColumnMetadata)
	{
		if (Metadata.ColumnName == ReadObject->SortedColumn)
		{
			ScoreType = Metadata.DataType;
			break;
		}
	}

	for (size_t i = 0; i < LeaderboardArraySize; i++)
	{
		auto LeaderboardEntry = ovr_LeaderboardEntryArray_GetElement(LeaderboardArray, i);
		auto User = ovr_LeaderboardEntry_GetUser(LeaderboardEntry);
		auto NickName = FString(ovr_User_GetOculusID(User));
		auto UserID = ovr_User_GetID(User);
		auto Rank = ovr_LeaderboardEntry_GetRank(LeaderboardEntry);
		auto Score = ovr_LeaderboardEntry_GetScore(LeaderboardEntry);

		auto Row = FOnlineStatsRow(NickName, MakeShareable(new FUniqueNetIdOculus(UserID)));
		Row.Rank = Rank;
		FVariantData ScoreData = [ScoreType, Score] {
			switch (ScoreType)
			{
				case EOnlineKeyValuePairDataType::Int32:
					// prevent overflowing by capping to the max rather than truncate to preserve
					// order of the score
					if (Score > INT32_MAX)
					{
						return FVariantData(INT32_MAX);
					}
					else if (Score < INT32_MIN)
					{
						return FVariantData(INT32_MIN);
					}
					else
					{
						return FVariantData(static_cast<int32>(Score));
					}
					break;
				case EOnlineKeyValuePairDataType::UInt32:
					// prevent overflowing by capping to the max rather than truncate to preserve
					// order of the score
					if (Score > UINT32_MAX)
					{
						return FVariantData(UINT32_MAX);
					}
					else if (Score < 0)
					{
						return FVariantData(static_cast<uint32>(0));
					}
					else
					{
						return FVariantData(static_cast<uint32>(Score));
					}
					break;
				default:
					return FVariantData(Score);
					break;
			}
		} ();

		Row.Columns.Add(ReadObject->SortedColumn, MoveTemp(ScoreData));
		ReadObject->Rows.Add(Row);
	}

	bool bHasPaging = ovr_LeaderboardEntryArray_HasNextPage(LeaderboardArray);
	if (bHasPaging)
	{
		OculusSubsystem.AddRequestDelegate(
			ovr_Leaderboard_GetNextEntries(LeaderboardArray),
			FOculusMessageOnCompleteDelegate::CreateLambda([this, ReadObject](ovrMessageHandle InMessage, bool bInIsError)
		{
			OnReadLeaderboardsComplete(InMessage, bInIsError, ReadObject);
		}));
		return;
	}

	ReadObject->ReadState = EOnlineAsyncTaskState::Done;
	TriggerOnLeaderboardReadCompleteDelegates(true);
}

void FOnlineLeaderboardOculus::FreeStats(FOnlineLeaderboardRead& ReadObject)
{
	// no-op
}

bool FOnlineLeaderboardOculus::WriteLeaderboards(const FName& SessionName, const FUniqueNetId& Player, FOnlineLeaderboardWrite& WriteObject)
{
	auto LoggedInPlayerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(0);
	if (!(LoggedInPlayerId.IsValid() && Player == *LoggedInPlayerId))
	{
		UE_LOG_ONLINE(Error, TEXT("Can only write to leaderboards for logged in player id"));
		return false;
	}

	auto StatData = WriteObject.FindStatByName(WriteObject.RatedStat);

	if (StatData == nullptr)
	{
		UE_LOG_ONLINE(Error, TEXT("Could not find RatedStat: %s"), *WriteObject.RatedStat.ToString());
		return false;
	}

	long long Score;

	switch (StatData->GetType())
	{
		case EOnlineKeyValuePairDataType::Int32:
		{
			int32 Value;
			StatData->GetValue(Value);
			Score = Value;
			break;
		}
		case EOnlineKeyValuePairDataType::UInt32:
		{
			uint32 Value;
			StatData->GetValue(Value);
			Score = Value;
			break;
		}
		case EOnlineKeyValuePairDataType::Int64:
		{
			int64 Value;
			StatData->GetValue(Value);
			Score = Value;
			break;
		}
		case EOnlineKeyValuePairDataType::UInt64:
		case EOnlineKeyValuePairDataType::Double:
		case EOnlineKeyValuePairDataType::Float:
		default:
		{
			UE_LOG_ONLINE(Error, TEXT("Invalid Stat type to save to the leaderboard: %s"), EOnlineKeyValuePairDataType::ToString(StatData->GetType()));
			return false;
		}
	}

	for (const auto& LeaderboardName : WriteObject.LeaderboardNames)
	{
		OculusSubsystem.AddRequestDelegate(
			ovr_Leaderboard_WriteEntry(TCHAR_TO_ANSI(*LeaderboardName.ToString()), Score, /* extra_data */ nullptr, 0, (WriteObject.UpdateMethod == ELeaderboardUpdateMethod::Force)),
			FOculusMessageOnCompleteDelegate::CreateLambda([this](ovrMessageHandle Message, bool bIsError)
		{
			if (bIsError) 
			{
				auto Error = ovr_Message_GetError(Message);
				auto ErrorMessage = ovr_Error_GetMessage(Error);
				UE_LOG_ONLINE(Error, TEXT("%s"), *FString(ErrorMessage));
			}
		}));
	}

	return true;
};

bool FOnlineLeaderboardOculus::FlushLeaderboards(const FName& SessionName)
{
	TriggerOnLeaderboardFlushCompleteDelegates(SessionName, true);
	return true;
};

bool FOnlineLeaderboardOculus::WriteOnlinePlayerRatings(const FName& SessionName, int32 LeaderboardId, const TArray<FOnlinePlayerScore>& PlayerScores)
{
	// Not supported
	return false;
};
