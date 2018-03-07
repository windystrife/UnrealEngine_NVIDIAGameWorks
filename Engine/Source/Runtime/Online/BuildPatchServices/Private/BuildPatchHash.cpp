// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchHash.cpp: Implements the static struct FRollingHashConst
=============================================================================*/

#include "BuildPatchHash.h"

// We'll use the commonly used in CRC64, ECMA polynomial defined in ECMA 182.
static const uint64 HashPoly64 = 0xC96C5795D7870F42;

uint64 FRollingHashConst::HashTable[ 256 ] = { 0 };

void FRollingHashConst::Init()
{
	for( uint32 TableIdx = 0; TableIdx < 256; ++TableIdx )
	{
		uint64 val = TableIdx;
		for( uint32 ShiftCount = 0; ShiftCount < 8; ++ShiftCount )
		{
			if( ( val & 1 ) == 1 )
			{
				val >>= 1;
				val ^= HashPoly64;
			}
			else
			{
				val >>= 1;
			}
		}
		HashTable[ TableIdx ] = val;
	}
}

uint64 FCycPoly64Hash::GetHashForDataSet(const uint8* DataSet, const uint32 DataSize, const uint64 State/* = 0*/)
{
	uint64 HashState = State;
	for (uint32 i = 0; i < DataSize; ++i)
	{
		ROTLEFT_64B(HashState, 1);
		HashState ^= FRollingHashConst::HashTable[DataSet[i]];
	}
	return HashState;
}
