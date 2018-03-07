// Copyright (C) 2009 Nine Realms, Inc
//

#pragma once

/*
===============================================================================
	Direct Mapped Cache
	size must be a power of 2
===============================================================================
*/

template< typename T, uint32 Size >
class TCacheDirect
{
public:
				TCacheDirect();
	
	// find element in cache, returns NULL if not found
	T*			Find( uint32 Key );
	// add new element not already in cache
	void		Add( uint32 Key, const T& Element );
	// invalidates element if currently in cache
	void		Remove( uint32 Key );

protected:
	// Use AOS instead?
	T			Cache[ Size ];
	uint32		Keys[ Size ];
};

template< typename T, uint32 Size >
FORCEINLINE TCacheDirect<T, Size>::TCacheDirect()
{
	static_assert((Size & (Size - 1)) == 0, "Size must be power of 2.");

	FMemory::Memset( Keys, 0xff );
}

template< typename T, uint32 Size >
FORCEINLINE T* TCacheDirect<T, Size>::Find( uint32 Key )
{
	uint32 Index = Key & ( Size - 1 );
	return ( Keys[Index] == Key ) ? &Cache[Index] : NULL;
}

template< typename T, uint32 Size >
FORCEINLINE void TCacheDirect<T, Size>::Add( uint32 Key, const T& Element )
{
	checkSlow( !Find( Key ) );

	uint32 Index = Key & ( Size - 1 );
	Keys[Index] = Key;
	Cache[Index] = Element;
}

template< typename T, uint32 Size >
FORCEINLINE void TCacheDirect<T, Size>::Remove( uint32 Key )
{
	uint32 Index = Key & ( Size - 1 );
	if( Keys[ Index ] == Key )
	{
		Keys[ Index ] = ~0u;
	}
}

/*
===============================================================================
	4-way Set Associative Cache
	LRU replacement
	size must be a power of 2 and >= 4
===============================================================================
*/

template< typename T, uint32 size >
class TCache4Way
{
public:
				TCache4Way();

	T*			Find( uint32 key );						// find element in cache, returns NULL if not found

	void		Add( uint32 key, const T& Element );	// add new element not already in cache
	void		Remove( uint32 key );					// invalidates element if currently in cache

protected:
	T			cache[size >> 2][4];
	uint32		keys[size >> 2][4];
	uint8		order[size >> 2];
};

template< typename T, uint32 size >
FORCEINLINE TCache4Way<T, size>::TCache4Way()
{
	static_assert((Size & (Size - 1)) == 0, "Size must be power of 2.");

	FMemory::Memset( Keys, 0xff );

	uint8 StartingOrder = (3 << 6) | (2 << 4) | (1 << 2) | 0;
	FMemory::Memset( order, StartingOrder );
}

template< typename T, uint32 size >
FORCEINLINE T* TCache4Way<T, size>::Find( uint32 key )
{
	uint32 index = key & ( (size >> 2) - 1 );
	uint32 SetBits = order[index];
	
	uint32 SetBitsShifted = SetBits;
	uint32 LowMask = 0;
	uint32 HighMask = 0xfc;
	for( uint32 i = 0; i < 4; i++ )
	{
		uint32 SetOffset = SetBitsShifted & 3;
		if( key == keys[index][ SetOffset ] )
		{
			// Update order. SetIndex to MRU
			uint32 LowerBits  = SetBits & LowMask;
			uint32 HigherBits = SetBits & HighMask;
			order[index] = HigherBits | (LowerBits << 2) | SetOffset;

			return &cache[index][ SetOffset ];
		}

		SetBitsShifted >>= 2;
		LowMask = (LowMask << 2) | 3;
		HighMask <<= 2;
	}

	return NULL;
}

template< typename T, uint32 size >
FORCEINLINE void TCache4Way<T, size>::Add( uint32 Key, const T& Element )
{
	checkSlow( !Find( key ) );

	uint32 index = key & ( (size >> 2) - 1 );

	uint32 SetBits = order[index];
	// Take LRU
	uint32 SetOffset = (SetBits >> 6) & 3;
	// Set as MRU
	order[index] = (SetBits << 2) | SetOffset;
	
	keys[index][ SetOffset ] = Key;
	cache[index][ SetOffset ] = Element;
}

template< typename T, uint32 size >
FORCEINLINE void TCache4Way<T, size>::Remove( uint32 key )
{
	uint32 index = key & ( (size >> 2) - 1 );
	uint32 SetBits = order[index];
	
	uint32 SetBitsShifted = SetBits;
	uint32 LowMask = 0;
	uint32 HighMask = 0xfc;
	for( uint32 i = 0; i < 4; i++ )
	{
		uint32 SetOffset = SetBitsShifted & 3;
		if( key == keys[index][ SetIndex ] )
		{
			// Update order. SetIndex to LRU.
			uint32 LowerBits  = SetBits & LowMask;
			uint32 HigherBits = SetBits & HighMask;
			order[index] = (SetOffset << 6) | (HigherBits >> 2) | LowerBits;

			keys[index][ SetOffset ] = ~0u;
			return;
		}

		SetBitsShifted >>= 2;
		LowMask = (LowMask << 2) | 3;
		HighMask <<= 2;
	}
}
