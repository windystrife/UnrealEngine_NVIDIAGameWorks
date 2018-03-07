// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HashTable.h"

CORE_API uint32 FHashTable::EmptyHash[1] = { ~0u };

CORE_API void FHashTable::Resize( uint32 NewIndexSize )
{
	if( NewIndexSize == IndexSize )
	{
		return;
	}

	if( NewIndexSize == 0 )
	{
		Free();
		return;
	}

	if( IndexSize == 0 )
	{
		HashMask = HashSize - 1;
		Hash = new uint32[ HashSize ];
		FMemory::Memset( Hash, 0xff, HashSize * 4 );
	}

	uint32* NewNextIndex = new uint32[ NewIndexSize ];

	if( NextIndex )
	{
		FMemory::Memcpy( NewNextIndex, NextIndex, IndexSize * 4 );
		delete[] NextIndex;
	}
	
	IndexSize = NewIndexSize;
	NextIndex = NewNextIndex;
}

CORE_API float FHashTable::AverageSearch() const
{
	uint32 SumAvgSearch = 0;
	uint32 NumElements = 0;
	for( uint32 Key = 0; Key < HashSize; Key++ )
	{
		uint32 NumInBucket = 0;
		for( uint32 i = First( Key ); IsValid( i ); i = Next( i ) )
		{
			NumInBucket++;
		}

		SumAvgSearch += NumInBucket * ( NumInBucket + 1 );
		NumElements  += NumInBucket;
	}
	return ( 0.5f * SumAvgSearch ) / NumElements;
}