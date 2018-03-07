// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

struct SLATE_API FTextRange
{
	FTextRange()
		: BeginIndex(INDEX_NONE)
		, EndIndex(INDEX_NONE)
	{

	}

	FTextRange( int32 InBeginIndex, int32 InEndIndex )
		: BeginIndex( InBeginIndex )
		, EndIndex( InEndIndex )
	{

	}

	FORCEINLINE bool operator==(const FTextRange& Other) const
	{
		return BeginIndex == Other.BeginIndex 
			&& EndIndex == Other.EndIndex;
	}

	FORCEINLINE bool operator!=(const FTextRange& Other) const
	{
		return !(*this == Other);
	}

	friend inline uint32 GetTypeHash(const FTextRange& Key)
	{
		uint32 KeyHash = 0;
		KeyHash = HashCombine(KeyHash, GetTypeHash(Key.BeginIndex));
		KeyHash = HashCombine(KeyHash, GetTypeHash(Key.EndIndex));
		return KeyHash;
	}

	int32 Len() const { return EndIndex - BeginIndex; }
	bool IsEmpty() const { return (EndIndex - BeginIndex) <= 0; }
	void Offset(int32 Amount) { BeginIndex += Amount; BeginIndex = FMath::Max(0, BeginIndex);  EndIndex += Amount; EndIndex = FMath::Max(0, EndIndex); }
	bool Contains(int32 Index) const { return Index >= BeginIndex && Index < EndIndex; }
	bool InclusiveContains(int32 Index) const { return Index >= BeginIndex && Index <= EndIndex; }

	FTextRange Intersect(const FTextRange& Other) const
	{
		FTextRange Intersected(FMath::Max(BeginIndex, Other.BeginIndex), FMath::Min(EndIndex, Other.EndIndex));
		if (Intersected.EndIndex <= Intersected.BeginIndex)
		{
			return FTextRange(0, 0);
		}

		return Intersected;
	}

	/**
	 * Produce an array of line ranges from the given text, breaking at any new-line characters
	 */
	static void CalculateLineRangesFromString(const FString& Input, TArray<FTextRange>& LineRanges);

	int32 BeginIndex;
	int32 EndIndex;
};
