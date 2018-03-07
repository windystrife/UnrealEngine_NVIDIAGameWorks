// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

/** Helpers for converting various common types to strings that analytics providers can consume. */
namespace AnalyticsConversion
{
	/** Lexical conversion. Allow any type that we have a Lex for. Can't use universal references here because it then eats all non-perfect matches for the array and TMap conversions below, which we want to use a custom, analytics specific implementation for. */
	template <typename T>
	inline auto ToString(const T& Value) -> decltype(Lex::ToString(Value)) 
	{
		return Lex::ToString(Value);
	}
	inline FString ToString(float Value)
	{
		return Lex::ToSanitizedString(Value);
	}
	inline FString ToString(double Value)
	{
		return Lex::ToSanitizedString(Value);
	}

	/** Array conversion. Creates comma-separated list. */
	template <typename T, typename AllocatorType>
	FString ToString(const TArray<T, AllocatorType>& ValueArray)
	{
		FString Result;
		// Serialize the array into "value1,value2,..." format
		for (const T& Value : ValueArray)
		{
			Result += AnalyticsConversion::ToString(Value);
			Result += TEXT(",");
		}
		// Remove the trailing comma (LeftChop will ensure an empty container won't crash here).
		Result = Result.LeftChop(1);
		return Result;
	}

	/** Map conversion. Creates comma-separated list. Creates comma-separated list with colon-separated key:value pairs. */
	template<typename KeyType, typename ValueType, typename Allocator, typename KeyFuncs>
	FString ToString(const TMap<KeyType, ValueType, Allocator, KeyFuncs>& ValueMap)
	{
		FString Result;
		// Serialize the map into "key:value,..." format
		for (auto& KVP : ValueMap)
		{
			Result += AnalyticsConversion::ToString(KVP.Key);
			Result += TEXT(":");
			Result += AnalyticsConversion::ToString(KVP.Value);
			Result += TEXT(",");
		}
		// Remove the trailing comma (LeftChop will ensure an empty container won't crash here).
		Result = Result.LeftChop(1);
		return Result;
	}
}

