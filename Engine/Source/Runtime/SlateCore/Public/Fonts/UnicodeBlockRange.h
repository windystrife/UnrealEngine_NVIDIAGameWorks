// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArrayView.h"

/** Enumeration of pre-defined Unicode block ranges that can be used to access entries from FUnicodeBlockRange */
enum class EUnicodeBlockRange : uint16
{
#define REGISTER_UNICODE_BLOCK_RANGE(LOWERBOUND, UPPERBOUND, SYMBOLNAME, DISPLAYNAME) SYMBOLNAME,
#include "UnicodeBlockRange.inl"
#undef REGISTER_UNICODE_BLOCK_RANGE
};

/** Pre-defined Unicode block ranges that can be used with the character ranges in sub-fonts */
struct SLATECORE_API FUnicodeBlockRange
{
	/** Index enum of this block */
	EUnicodeBlockRange Index;

	/** Display name of this block */
	FText DisplayName;

	/** Range of this block */
	FInt32Range Range;

	/** Returns an array containing all of the pre-defined block ranges */
	static TArrayView<const FUnicodeBlockRange> GetUnicodeBlockRanges();

	/** Returns the block corresponding to the given enum */
	static FUnicodeBlockRange GetUnicodeBlockRange(const EUnicodeBlockRange InBlockIndex);
};
