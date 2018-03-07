// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

static FORCEINLINE uint32 Murmur32( std::initializer_list< uint32 > InitList )
{
	uint32 Hash = 0;

	for( auto Element : InitList )
	{
		Element *= 0xcc9e2d51;
		Element = ( Element << 15 ) | ( Element >> (32 - 15) );
		Element *= 0x1b873593;
    
		Hash ^= Element;
		Hash = ( Hash << 13 ) | ( Hash >> (32 - 13) );
		Hash = Hash * 5 + 0xe6546b64;
	}

	Hash ^= Hash >> 16;
	Hash *= 0x85ebca6b;
	Hash ^= Hash >> 13;
	Hash *= 0xc2b2ae35;
	Hash ^= Hash >> 16;

	return Hash;
}

/*-----------------------------------------------------------------------------
	Statically sized hash table, used to index another data structure.
	Vastly simpler and faster than TMap.

	Example find:

	uint16 Key = HashFunction( ID );
	for( uint16 i = HashTable.First( Key ); HashTable.IsValid( i ); i = HashTable.Next( i ) )
	{
		if( Array[i].ID == ID )
		{
			return Array[i];
		}
	}
-----------------------------------------------------------------------------*/
template< uint16 HashSize, uint16 IndexSize >
class TStaticHashTable
{
public:
				TStaticHashTable();
	
	void		Clear();

	// Functions used to search
	uint16		First( uint16 Key ) const;
	uint16		Next( uint16 Index ) const;
	bool		IsValid( uint16 Index ) const;
	
	void		Add( uint16 Key, uint16 Index );
	void		Remove( uint16 Key, uint16 Index );

protected:
	uint16		Hash[ HashSize ];
	uint16		NextIndex[ IndexSize ];
};

template< uint16 HashSize, uint16 IndexSize >
FORCEINLINE TStaticHashTable< HashSize, IndexSize >::TStaticHashTable()
{
	static_assert( ( HashSize & (HashSize - 1) ) == 0, "Hash size must be power of 2" );
	static_assert( IndexSize - 1 < 0xffff, "Index 0xffff is reserved" );
	Clear();
}

template< uint16 HashSize, uint16 IndexSize >
FORCEINLINE void TStaticHashTable< HashSize, IndexSize >::Clear()
{
	FMemory::Memset( Hash, 0xff, HashSize * 2 );
}

// First in hash chain
template< uint16 HashSize, uint16 IndexSize >
FORCEINLINE uint16 TStaticHashTable< HashSize, IndexSize >::First( uint16 Key ) const
{
	Key &= HashSize - 1;
	return Hash[ Key ];
}

// Next in hash chain
template< uint16 HashSize, uint16 IndexSize >
FORCEINLINE uint16 TStaticHashTable< HashSize, IndexSize >::Next( uint16 Index ) const
{
	checkSlow( Index < IndexSize );
	return NextIndex[ Index ];
}

template< uint16 HashSize, uint16 IndexSize >
FORCEINLINE bool TStaticHashTable< HashSize, IndexSize >::IsValid( uint16 Index ) const
{
	return Index != 0xffff;
}

template< uint16 HashSize, uint16 IndexSize >
FORCEINLINE void TStaticHashTable< HashSize, IndexSize >::Add( uint16 Key, uint16 Index )
{
	checkSlow( Index < IndexSize );

	Key &= HashSize - 1;
	NextIndex[ Index ] = Hash[ Key ];
	Hash[ Key ] = Index;
}

template< uint16 HashSize, uint16 IndexSize >
inline void TStaticHashTable< HashSize, IndexSize >::Remove( uint16 Key, uint16 Index )
{
	checkSlow( Index < IndexSize );

	Key &= HashSize - 1;

	if( Hash[Key] == Index )
	{
		// Head of chain
		Hash[Key] = NextIndex[ Index ];
	}
	else
	{
		for( uint16 i = Hash[Key]; IsValid(i); i = NextIndex[i] )
		{
			if( NextIndex[i] == Index )
			{
				// Next = Next->Next
				NextIndex[i] = NextIndex[ Index ];
				break;
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	Dynamically sized hash table, used to index another data structure.
	Vastly simpler and faster than TMap.

	Example find:

	uint32 Key = HashFunction( ID );
	for( uint32 i = HashTable.First( Key ); HashTable.IsValid( i ); i = HashTable.Next( i ) )
	{
		if( Array[i].ID == ID )
		{
			return Array[i];
		}
	}
-----------------------------------------------------------------------------*/
class FHashTable
{
public:
					FHashTable( uint32 InHashSize = 1024, uint32 InIndexSize = 0 );
					~FHashTable();

	void			Clear();
	void			Free();
	CORE_API void	Resize( uint32 NewIndexSize );

	// Functions used to search
	uint32			First( uint16 Key ) const;
	uint32			Next( uint32 Index ) const;
	bool			IsValid( uint32 Index ) const;
	
	void			Add( uint16 Key, uint32 Index );
	void			Remove( uint16 Key, uint32 Index );

	// Average # of compares per search
	CORE_API float	AverageSearch() const;

protected:
	// Avoids allocating hash until first add
	CORE_API static uint32	EmptyHash[1];

	uint32			HashSize;
	uint16			HashMask;
	uint32			IndexSize;

	uint32*			Hash;
	uint32*			NextIndex;
};


FORCEINLINE FHashTable::FHashTable( uint32 InHashSize, uint32 InIndexSize )
	: HashSize( InHashSize )
	, HashMask( 0 )
	, IndexSize( InIndexSize )
	, Hash( EmptyHash )
	, NextIndex( NULL )
{
	check( HashSize <= 0x10000 );
	check( FMath::IsPowerOfTwo( HashSize ) );
	
	if( IndexSize )
	{
		HashMask = HashSize - 1;
		
		Hash = new uint32[ HashSize ];
		NextIndex = new uint32[ IndexSize ];

		FMemory::Memset( Hash, 0xff, HashSize * 4 );
	}
}

FORCEINLINE FHashTable::~FHashTable()
{
	Free();
}

FORCEINLINE void FHashTable::Clear()
{
	if( IndexSize )
	{
		FMemory::Memset( Hash, 0xff, HashSize * 4 );
	}
}

FORCEINLINE void FHashTable::Free()
{
	if( IndexSize )
	{
		HashMask = 0;
		IndexSize = 0;
		
		delete[] Hash;
		Hash = EmptyHash;
		
		delete[] NextIndex;
		NextIndex = NULL;
	}
} 

// First in hash chain
FORCEINLINE uint32 FHashTable::First( uint16 Key ) const
{
	Key &= HashMask;
	return Hash[ Key ];
}

// Next in hash chain
FORCEINLINE uint32 FHashTable::Next( uint32 Index ) const
{
	checkSlow( Index < IndexSize );
	return NextIndex[ Index ];
}

FORCEINLINE bool FHashTable::IsValid( uint32 Index ) const
{
	return Index != ~0u;
}

FORCEINLINE void FHashTable::Add( uint16 Key, uint32 Index )
{
	if( Index >= IndexSize )
	{
		Resize(FMath::Max<uint32>(32u, FMath::RoundUpToPowerOfTwo(Index + 1)));
	}

	Key &= HashMask;
	NextIndex[ Index ] = Hash[ Key ];
	Hash[ Key ] = Index;
}

inline void FHashTable::Remove( uint16 Key, uint32 Index )
{
	if( Index >= IndexSize )
	{
		return;
	}

	Key &= HashMask;

	if( Hash[Key] == Index )
	{
		// Head of chain
		Hash[Key] = NextIndex[ Index ];
	}
	else
	{
		for( uint32 i = Hash[Key]; IsValid(i); i = NextIndex[i] )
		{
			if( NextIndex[i] == Index )
			{
				// Next = Next->Next
				NextIndex[i] = NextIndex[ Index ];
				break;
			}
		}
	}
}
