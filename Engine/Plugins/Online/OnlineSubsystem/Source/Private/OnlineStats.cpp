// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineStats.h"

/**
 *	Get a key value pair by key name
 * @param StatName key name to search for
 * @return KeyValuePair if found, NULL otherwise
 */
FVariantData* FOnlineStats::FindStatByName(const FName& StatName)
{
	return Properties.Find(StatName);
}

/**
 * Sets a stat of type float to the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatName the stat to change the value of
 * @param Value the new value to assign to the stat
 */
void FOnlineStats::SetFloatStat(const FName& StatName, float Value)
{
	FVariantData* Stat = FindStatByName(StatName);
	if (Stat != NULL)
	{
		if (Stat->GetType() == EOnlineKeyValuePairDataType::Float)
		{
			// Set the value
			Stat->SetValue(Value);
		}
	}
	else
	{
		FVariantData NewValue(Value);
		Properties.Add(StatName, NewValue);
	}
}

/**
 * Sets a stat of type int32 to the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatName the stat to change the value of
 * @param Value the new value to assign to the stat
 */
void FOnlineStats::SetIntStat(const FName& StatName, int32 Value)
{
	FVariantData* Stat = FindStatByName(StatName);
	if (Stat != NULL && Stat->GetType() == EOnlineKeyValuePairDataType::Int32)
	{
		// Set the value
		Stat->SetValue(Value);
	}
	else
	{
		FVariantData NewValue(Value);
		Properties.Add(StatName, NewValue);
	}
}

/**
 * Increments a stat of type float by the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatName the stat to increment
 * @param IncBy the value to increment by
 */
void FOnlineStats::IncrementFloatStat(const FName& StatName, float IncBy)
{
	FVariantData* Stat = FindStatByName(StatName);
	if (Stat != NULL && Stat->GetType() == EOnlineKeyValuePairDataType::Float)
	{
		// Set the value
		Stat->Increment<float, EOnlineKeyValuePairDataType::Float>(IncBy);
	}
	else
	{
		FVariantData NewValue(IncBy);
		Properties.Add(StatName, NewValue);
	}
}

/**
 * Increments a stat of type int32 by the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatName the stat to increment
 * @param IncBy the value to increment by
 */
void FOnlineStats::IncrementIntStat(const FName& StatName, int32 IncBy)
{
	FVariantData* Stat = FindStatByName(StatName);
	if (Stat != NULL && Stat->GetType() == EOnlineKeyValuePairDataType::Int32)
	{
		// Set the value
		Stat->Increment<int32, EOnlineKeyValuePairDataType::Int32>(IncBy);
	}
	else
	{
		FVariantData NewValue(IncBy);
		Properties.Add(StatName, NewValue);
	}
}

/**
 * Decrements a stat of type float by the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatName the stat to decrement
 * @param DecBy the value to decrement by
 */
void FOnlineStats::DecrementFloatStat(const FName& StatName, float DecBy)
{
	FVariantData* Stat = FindStatByName(StatName);
	if (Stat != NULL && Stat->GetType() == EOnlineKeyValuePairDataType::Float)
	{
		// Set the value
		Stat->Decrement<float, EOnlineKeyValuePairDataType::Float>(DecBy);
	}
	else
	{
		FVariantData NewValue(-DecBy);
		Properties.Add(StatName, NewValue);
	}
}

/**
 * Decrements a stat of type int32 by the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatName the stat to decrement
 * @param DecBy the value to decrement by
 */
void FOnlineStats::DecrementIntStat(const FName& StatName, int32 DecBy)
{
	FVariantData* Stat = FindStatByName(StatName);
	if (Stat != NULL && Stat->GetType() == EOnlineKeyValuePairDataType::Int32)
	{
		// Set the value
		Stat->Decrement<int32, EOnlineKeyValuePairDataType::Int32>(DecBy);
	}
	else
	{
		FVariantData NewValue(-DecBy);
		Properties.Add(StatName, NewValue);
	}
}
