// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Fonts/UnicodeBlockRange.h"

TArrayView<const FUnicodeBlockRange> FUnicodeBlockRange::GetUnicodeBlockRanges()
{
	static const FUnicodeBlockRange UnicodeBlockRanges[] = {
		#define REGISTER_UNICODE_BLOCK_RANGE(LOWERBOUND, UPPERBOUND, SYMBOLNAME, DISPLAYNAME) { EUnicodeBlockRange::SYMBOLNAME, DISPLAYNAME, FInt32Range(FInt32Range::BoundsType::Inclusive(LOWERBOUND), FInt32Range::BoundsType::Inclusive(UPPERBOUND)) },
		#include "UnicodeBlockRange.inl"
		#undef REGISTER_UNICODE_BLOCK_RANGE
	};

	return UnicodeBlockRanges;
}

FUnicodeBlockRange FUnicodeBlockRange::GetUnicodeBlockRange(const EUnicodeBlockRange InBlockIndex)
{
	TArrayView<const FUnicodeBlockRange> UnicodeBlockRanges = GetUnicodeBlockRanges();
	return UnicodeBlockRanges[(int32)InBlockIndex];
}
