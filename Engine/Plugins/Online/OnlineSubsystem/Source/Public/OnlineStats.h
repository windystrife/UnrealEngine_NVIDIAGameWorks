// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/CoreOnline.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineKeyValuePair.h"

/** Representation of a single column and its data */ 
typedef FOnlineKeyValuePairs<FName, FVariantData> FStatsColumnArray;
/** Representation of a single stat value to post to the backend */
typedef FOnlineKeyValuePairs<FName, FVariantData> FStatPropertyArray;

/**
 * An interface used to collect and manage online stats
 */
class ONLINESUBSYSTEM_API FOnlineStats
{
public:

	/** Array of stats we are gathering */
	FStatPropertyArray Properties;

	/**
	 *	Get a key value pair by key name
	 * @param StatName key name to search for
	 * @return KeyValuePair if found, NULL otherwise
	 */
	class FVariantData* FindStatByName(const FName& StatName);

	/**
	 * Sets a stat of type SDT_Float to the value specified. Does nothing
	 * if the stat is not of the right type.
	 *
	 * @param StatName the stat to change the value of
	 * @param Value the new value to assign to the stat
	 */
	virtual void SetFloatStat(const FName& StatName, float Value);

	/**
	 * Sets a stat of type SDT_Int to the value specified. Does nothing
	 * if the stat is not of the right type.
	 *
	 * @param StatName the stat to change the value of
	 * @param Value the new value to assign to the stat
	 */
	virtual void SetIntStat(const FName& StatName, int32 Value);

	/**
	 * Increments a stat of type float by the value specified. Does nothing
	 * if the stat is not of the right type.
	 *
	 * @param StatName the stat to increment
	 * @param IncBy the value to increment by
	 */
	virtual void IncrementFloatStat(const FName& StatName, float IncBy = 1.0f);

	/**
	 * Increments a stat of type int32 by the value specified. Does nothing
	 * if the stat is not of the right type.
	 *
	 * @param StatName the stat to increment
	 * @param IncBy the value to increment by
	 */
	virtual void IncrementIntStat(const FName& StatName, int32 IncBy = 1);

	/**
	 * Decrements a stat of type float by the value specified. Does nothing
	 * if the stat is not of the right type.
	 *
	 * @param StatName the stat to decrement
	 * @param DecBy the value to decrement by
	 */
	virtual void DecrementFloatStat(const FName& StatName, float DecBy = 1.0f);

	/**
	 * Decrements a stat of type int32 by the value specified. Does nothing
	 * if the stat is not of the right type.
	 *
	 * @param StatName the stat to decrement
	 * @param DecBy the value to decrement by
	 */
	virtual void DecrementIntStat(const FName& StatName, int32 DecBy = 1);

	/**
	 * Destructor
	 */
	virtual ~FOnlineStats()
	{
		/** no-op */
	}
};

/**
 *	Interface for storing/writing data to a leaderboard
 */
class ONLINESUBSYSTEM_API FOnlineLeaderboardWrite : public FOnlineStats
{
public:

	/** Sort Method */
	ELeaderboardSort::Type SortMethod;
	/** Display Type */
	ELeaderboardFormat::Type DisplayFormat;
	/** Update Method */
	ELeaderboardUpdateMethod::Type UpdateMethod;

	/** Names of the leaderboards to write to */
	TArray<FName> LeaderboardNames;

	/** Name of the stat that the leaderboard is rated by */
	FName RatedStat;

	FOnlineLeaderboardWrite() :
		SortMethod(ELeaderboardSort::None),
		DisplayFormat(ELeaderboardFormat::Number),
		UpdateMethod(ELeaderboardUpdateMethod::KeepBest),
		RatedStat(NAME_None)
	{
	}
};

/**
 *	Representation of a single row in a retrieved leaderboard
 */
struct FOnlineStatsRow
{
private:
    /** Hidden on purpose */
    FOnlineStatsRow() : NickName() {}

public:
	/** Name of player in this row */
	const FString NickName;
	/** Unique Id for the player in this row */
    const TSharedPtr<const FUniqueNetId> PlayerId;
	/** Player's rank in this leaderboard */
    int32 Rank;
	/** All requested data on the leaderboard for this player */
	FStatsColumnArray Columns;

	FOnlineStatsRow(const FString& InNickname, const TSharedRef<const FUniqueNetId>& InPlayerId) :
		NickName(InNickname),
		PlayerId(InPlayerId)
	{
	}
};

/**
 *	Representation of a single column of data in a leaderboard
 */
struct FColumnMetaData
{
private:
	FColumnMetaData() :
		ColumnName(NAME_None),
		DataType(EOnlineKeyValuePairDataType::Empty)
	{}

public:

	/** Name of the column to retrieve */
	const FName ColumnName;
	/** Type of data this column represents */
	const EOnlineKeyValuePairDataType::Type DataType;

	FColumnMetaData(const FName InColumnName, EOnlineKeyValuePairDataType::Type InDataType) :
		ColumnName(InColumnName),
		DataType(InDataType)
	{
	}
};

/**
 *	Interface for reading data from a leaderboard service
 */
class ONLINESUBSYSTEM_API FOnlineLeaderboardRead
{
public:
	/** Name of the leaderboard read */
	FName LeaderboardName;
	/** Column this leaderboard is sorted by */
	FName SortedColumn;
	/** Column metadata for this leaderboard */
	TArray<FColumnMetaData> ColumnMetadata;
	/** Array of ranked users retrieved (not necessarily sorted yet) */
	TArray<FOnlineStatsRow> Rows;
	/** Indicates an error reading data occurred while processing */
	EOnlineAsyncTaskState::Type ReadState;

	FOnlineLeaderboardRead() :
		LeaderboardName(NAME_None),
		SortedColumn(NAME_None),
		ReadState(EOnlineAsyncTaskState::NotStarted)
	{
	}

	/**
	 *	Retrieve a single record from the leaderboard for a given user
	 *
	 * @param UserId user id to retrieve a record for
	 * @return the requested user row or NULL if not found
	 */
	FOnlineStatsRow* FindPlayerRecord(const FUniqueNetId& UserId)
	{
		for (int32 UserIdx=0; UserIdx<Rows.Num(); UserIdx++)
		{
			if (*Rows[UserIdx].PlayerId == UserId)
			{
				return &Rows[UserIdx];
			}
		}

		return NULL;
	}
};

typedef TSharedRef<FOnlineLeaderboardRead, ESPMode::ThreadSafe> FOnlineLeaderboardReadRef;
typedef TSharedPtr<FOnlineLeaderboardRead, ESPMode::ThreadSafe> FOnlineLeaderboardReadPtr;

// TODO ONLINE
class FOnlinePlayerScore
{

};

/**
 * The interface for writing achievement stats to the server.
 */
class ONLINESUBSYSTEM_API FOnlineAchievementsWrite : public FOnlineStats
{
public:
	/**
	 * Constructor
	 */
	FOnlineAchievementsWrite() :
		WriteState(EOnlineAsyncTaskState::NotStarted)
	{

	}

	/** Indicates an error reading data occurred while processing */
	EOnlineAsyncTaskState::Type WriteState;
};

typedef TSharedRef<FOnlineAchievementsWrite, ESPMode::ThreadSafe> FOnlineAchievementsWriteRef;
typedef TSharedPtr<FOnlineAchievementsWrite, ESPMode::ThreadSafe> FOnlineAchievementsWritePtr;
