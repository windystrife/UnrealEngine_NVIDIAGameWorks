// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Core/RingBuffer.h"

/*=============================================================================
	BuildPatchHash.h: Declares and implements template classes for use with the 
	Build and Patching hash functionality.
=============================================================================*/

/**
 * A macro for barrel rolling a 64 bit value n times to the left.
 * @param Value		The value to shift
 * @param Shifts	The number of times to shift
 */
#define ROTLEFT_64B( Value, Shifts ) Value = ( ( ( Value ) << ( ( Shifts ) % 64 ) ) | ( ( Value ) >> ( ( 64 - ( ( Shifts ) % 64 ) ) % 64 ) ) )

/**
 * A static struct containing 64bit polynomial constant and hash table lookup for use with FRollingHash.
 */
struct FRollingHashConst
{
	// The lookup hash table
	static uint64 HashTable[ 256 ];
	
	/**
	 * Builds the hash table for use when hashing. Must be called before using FRollingHash.
	 */
	static void Init();
};

/**
 * A class that performs a rolling hash
 */
template< uint32 WindowSize >
class FRollingHash
{
	// A typedef for our data ring buffer
	typedef TRingBuffer< uint8, WindowSize > HashRingBuffer;

public:
	/**
	 * Constructor
	 */
	FRollingHash();

	/**
	 * Pass this function the initial data set to start the Rolling Hash with.
	 * @param NewByte		The byte to add to the hash state
	 */
	void ConsumeByte( const uint8& NewByte );

	/**
	 * Helper to consume from a byte array
	 * @param NewBytes		The byte array
	 * @param NumBytes		The number of bytes to consume
	 */
	void ConsumeBytes( const uint8* NewBytes, const uint32& NumBytes );

	/**
	 * Rolls the window by one byte forwards.
	 * @param NewByte		The byte that will be added to the front of the window
	 */
	void RollForward( const uint8& NewByte );

	/**
	 * Clears all data ready for a new entire data set
	 */
	void Clear();

	/**
	 * Get the HashState for the current window
	 * @return		The hash state
	 */
	const uint64 GetWindowHash() const;

	/**
	 * Get the Ring Buffer for the current window
	 * @return		The ring buffer
	 */
	const HashRingBuffer& GetWindowData() const;

	/**
	 * Get how many DataValueType values we still need to consume until our window is full 
	 * @return		The number of DataValueType we still need
	 */
	const uint32 GetNumDataNeeded() const;

	/**
	 * Get the size of our window 
	 * @return		Template arg WindowSize
	 */
	const uint32 GetWindowSize() const;

	/**
	 * Static function to simply return the hash for a given data range.
	 * @param DataSet	The buffer to the data. This must be a buffer of length == WindowSize
	 * @return			The hash state for the provided data
	 */
	static uint64 GetHashForDataSet( const uint8* DataSet );

private:
	// The current hash value
	uint64 HashState;
	// The number of bytes we have consumed so far, used in hash function and to check validity of calls
	uint32 NumBytesConsumed;
	// Store the data to make access and rolling easier
	HashRingBuffer WindowData;
};

namespace FCycPoly64Hash
{
	/**
	 * Calculate the hash for a given data range.
	 * @param DataSet	The buffer to the data. This must be a buffer of length == DataSize
	 * @param DataSize	The size of the buffer
	 * @param State		The current hash state, if continuing hash from previous buffer. 0 for new data.
	 * @return			The hash state for the provided data
	 */
	uint64 GetHashForDataSet(const uint8* DataSet, const uint32 DataSize, const uint64 State = 0);
}

/* FRollingHash implementation
*****************************************************************************/
template< uint32 WindowSize >
FRollingHash< WindowSize >::FRollingHash()
	: HashState( 0 )
	, NumBytesConsumed( 0 )
{
}

template< uint32 WindowSize >
void FRollingHash< WindowSize >::ConsumeByte( const uint8& NewByte )
{
	// We must be setup by consuming the correct amount of bytes
	check( NumBytesConsumed < WindowSize );
	++NumBytesConsumed;

	// Add the byte to our buffer
	WindowData.Enqueue( NewByte );
	// Add to our HashState
	ROTLEFT_64B( HashState, 1 );
	HashState ^= FRollingHashConst::HashTable[ NewByte ];
}

template< uint32 WindowSize >
void FRollingHash< WindowSize >::ConsumeBytes( const uint8* NewBytes, const uint32& NumBytes )
{
	for ( uint32 i = 0; i < NumBytes; ++i )
	{
		ConsumeByte( NewBytes[i] );
	}
}

template< uint32 WindowSize >
const uint32 FRollingHash< WindowSize >::GetNumDataNeeded() const
{
	return WindowSize - NumBytesConsumed;
}

template< uint32 WindowSize >
const uint32 FRollingHash< WindowSize >::GetWindowSize() const
{
	return WindowSize;
}

template< uint32 WindowSize >
void FRollingHash< WindowSize >::RollForward( const uint8& NewByte )
{
	// We must have consumed enough bytes to function correctly
	check( NumBytesConsumed == WindowSize );
	uint8 OldByte;
	WindowData.Dequeue( OldByte );
	WindowData.Enqueue( NewByte );
	// Update our HashState
	uint64 OldTerm = FRollingHashConst::HashTable[ OldByte ];
	ROTLEFT_64B( OldTerm, WindowSize );
	ROTLEFT_64B( HashState, 1 );
	HashState ^= OldTerm;
	HashState ^= FRollingHashConst::HashTable[ NewByte ];
}

template< uint32 WindowSize >
void FRollingHash< WindowSize >::Clear()
{
	HashState = 0;
	NumBytesConsumed = 0;
	WindowData.Empty();
}

template< uint32 WindowSize >
const uint64 FRollingHash< WindowSize >::GetWindowHash() const
{
	// We must have consumed enough bytes to function correctly
	check( NumBytesConsumed == WindowSize );
	return HashState;
}

template< uint32 WindowSize >
const TRingBuffer< uint8, WindowSize >& FRollingHash< WindowSize >::GetWindowData() const
{
	return WindowData;
}

template< uint32 WindowSize >
uint64 FRollingHash< WindowSize >::GetHashForDataSet( const uint8* DataSet )
{
	uint64 HashState = 0;
	for ( uint32 i = 0; i < WindowSize; ++i )
	{
		ROTLEFT_64B( HashState, 1 );
		HashState ^= FRollingHashConst::HashTable[ DataSet[i] ];
	}
	return HashState;
}

/**
 * A static function to perform sanity checks on the rolling hash class.
 */
static bool CheckRollingHashAlgorithm()
{
	bool bCheckOk = true;
	// Sanity Check the RollingHash code!!
	FString IndivWords[6];
	IndivWords[0] = "123456";	IndivWords[1] = "7890-=";	IndivWords[2] = "qwerty";	IndivWords[3] = "uiop[]";	IndivWords[4] = "asdfgh";	IndivWords[5] = "jkl;'#";
	FString DataToRollOver = FString::Printf( TEXT( "%s%s%s%s%s%s" ), *IndivWords[0], *IndivWords[1], *IndivWords[2], *IndivWords[3], *IndivWords[4], *IndivWords[5] );
	uint64 IndivHashes[6];
	for (uint32 idx = 0; idx < 6; ++idx )
	{
		uint8 Converted[6];
		for (uint32 iChar = 0; iChar < 6; ++iChar )
			Converted[iChar] = IndivWords[idx][iChar];
		IndivHashes[idx] = FRollingHash< 6 >::GetHashForDataSet( Converted );
	}

	FRollingHash< 6 > RollingHash;
	uint32 StrIdx = 0;
	for (uint32 k=0; k<6; ++k)
		RollingHash.ConsumeByte( DataToRollOver[ StrIdx++ ] );
	bCheckOk = bCheckOk && IndivHashes[0] == RollingHash.GetWindowHash();
	for (uint32 k=0; k<6; ++k)
		RollingHash.RollForward( DataToRollOver[ StrIdx++ ] );
	bCheckOk = bCheckOk && IndivHashes[1] == RollingHash.GetWindowHash();
	for (uint32 k=0; k<6; ++k)
		RollingHash.RollForward( DataToRollOver[ StrIdx++ ] );
	bCheckOk = bCheckOk && IndivHashes[2] == RollingHash.GetWindowHash();
	for (uint32 k=0; k<6; ++k)
		RollingHash.RollForward( DataToRollOver[ StrIdx++ ] );
	bCheckOk = bCheckOk && IndivHashes[3] == RollingHash.GetWindowHash();
	for (uint32 k=0; k<6; ++k)
		RollingHash.RollForward( DataToRollOver[ StrIdx++ ] );
	bCheckOk = bCheckOk && IndivHashes[4] == RollingHash.GetWindowHash();
	for (uint32 k=0; k<6; ++k)
		RollingHash.RollForward( DataToRollOver[ StrIdx++ ] );
	bCheckOk = bCheckOk && IndivHashes[5] == RollingHash.GetWindowHash();

	return bCheckOk;
}
