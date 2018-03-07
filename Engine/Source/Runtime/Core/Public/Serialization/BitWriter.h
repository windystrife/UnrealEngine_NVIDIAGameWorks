// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Serialization/Archive.h"
#include "Containers/Array.h"
#include "Math/UnrealMathUtility.h"

/*-----------------------------------------------------------------------------
	FBitWriter.
-----------------------------------------------------------------------------*/

//
// Writes bitstreams.
//
struct CORE_API FBitWriter : public FArchive
{
	friend struct FBitWriterMark;

public:
	/** Default constructor. Zeros everything. */
	FBitWriter(void);

	/**
	 * Constructor using known size the buffer needs to be
	 */
	FBitWriter( int64 InMaxBits, bool AllowResize = false );

	void SerializeBits( void* Src, int64 LengthBits );

	/**
	 * Serializes a compressed integer - Value must be < Max
	 *
	 * @param Value		The value to serialize, must be < Max
	 * @param Max		The maximum allowed value - good to aim for power-of-two
	 */
	void SerializeInt(uint32& Value, uint32 Max);

	/**
	 * Serializes the specified Value, but does not bounds check against ValueMax;
	 * instead, it will wrap around if the value exceeds ValueMax (this differs from SerializeInt, which clamps)
	 *
	 * @param Value		The value to serialize
	 * @param ValueMax	The maximum value to write; wraps Value if it exceeds this
	 */
	void WriteIntWrapped(uint32 Value, uint32 ValueMax);

	void WriteBit( uint8 In );
	void Serialize( void* Src, int64 LengthBytes );

	/**
	 * Returns a pointer to our internal buffer.
	 */
	FORCEINLINE uint8* GetData(void)
	{
#if !UE_BUILD_SHIPPING
		// If this happens, your code has insufficient IsError() checks.
		check(!IsError());
#endif

		return Buffer.GetData();
	}

	FORCEINLINE const uint8* GetData(void) const
	{
#if !UE_BUILD_SHIPPING
		// If this happens, your code has insufficient IsError() checks.
		check(!IsError());
#endif

		return Buffer.GetData();
	}

	FORCEINLINE const TArray<uint8>* GetBuffer(void) const
	{
#if !UE_BUILD_SHIPPING
		// If this happens, your code has insufficient IsError() checks.
		check(!IsError());
#endif

		return &Buffer;
	}

	/**
	 * Returns the number of bytes written.
	 */
	FORCEINLINE int64 GetNumBytes(void) const
	{
		return (Num+7)>>3;
	}

	/**
	 * Returns the number of bits written.
	 */
	FORCEINLINE int64 GetNumBits(void) const
	{
		return Num;
	}

	/**
	 * Returns the number of bits the buffer supports.
	 */
	FORCEINLINE int64 GetMaxBits(void) const
	{
		return Max;
	}

	/**
	 * Marks this bit writer as overflowed.
	 *
	 * @param LengthBits	The number of bits being written at the time of overflow
	 */
	void SetOverflowed(int32 LengthBits);

	/**
	 * Sets whether or not this writer intentionally allows overflows (to avoid logspam)
	 */
	FORCEINLINE void SetAllowOverflow(bool bInAllow)
	{
		bAllowOverflow = bInAllow;
	}

	FORCEINLINE bool AllowAppend(int64 LengthBits)
	{
		if (Num+LengthBits > Max)
		{
			if (AllowResize)
			{
				// Resize our buffer. The common case for resizing bitwriters is hitting the max and continuing to add a lot of small segments of data
				// Though we could just allow the TArray buffer to handle the slack and resizing, we would still constantly hit the FBitWriter's max
				// and cause this block to be executed, as well as constantly zeroing out memory inside AddZeroes (though the memory would be allocated
				// in chunks).

				Max = FMath::Max<int64>(Max<<1,Num+LengthBits);
				int32 ByteMax = (Max+7)>>3;
				Buffer.AddZeroed(ByteMax - Buffer.Num());
				check((Max+7)>>3== Buffer.Num());
				return true;
			}
			else
			{
				return false;
			}
		}
		return true;
	}

	FORCEINLINE void SetAllowResize(bool NewResize)
	{
		AllowResize = NewResize;
	}

	/**
	 * Resets the bit writer back to its initial state
	 */
	void Reset(void);

	FORCEINLINE void WriteAlign()
	{
		Num = ( Num + 7 ) & ( ~0x07 );
	}

private:

	TArray<uint8> Buffer;
	int64   Num;
	int64   Max;
	bool	AllowResize;

	/** Whether or not overflowing is allowed (overflows silently) */
	bool bAllowOverflow;
};


//
// For pushing and popping FBitWriter positions.
//
struct CORE_API FBitWriterMark
{
public:

	FBitWriterMark()
		: Overflowed(false)
		, Num(0)
	{ }

	FBitWriterMark( FBitWriter& Writer )
	{
		Init(Writer);
	}

	int64 GetNumBits()
	{
		return Num;
	}

	void Init( FBitWriter& Writer)
	{
		Num = Writer.Num;
		Overflowed = Writer.ArIsError;
	}

	void Pop( FBitWriter& Writer );
	void Copy( FBitWriter& Writer, TArray<uint8> &Buffer );
	void PopWithoutClear( FBitWriter& Writer );

private:

	bool			Overflowed;
	int64			Num;
};
