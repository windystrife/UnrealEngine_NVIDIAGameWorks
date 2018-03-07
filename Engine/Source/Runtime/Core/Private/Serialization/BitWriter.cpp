// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnBits.h: Unreal bitstream manipulation classes.
=============================================================================*/

#include "Serialization/BitWriter.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"

extern const uint8 GShift[8];
extern const uint8 GMask[8];

CORE_API void appBitsCpy( uint8* Dest, int32 DestBit, uint8* Src, int32 SrcBit, int32 BitCount );

/*-----------------------------------------------------------------------------
	FBitWriter.
-----------------------------------------------------------------------------*/

/**
 * Constructor using known size the buffer needs to be
 */
FBitWriter::FBitWriter( int64 InMaxBits, bool InAllowResize /*=false*/ )
	: Num(0)
	, Max(InMaxBits)
	, bAllowOverflow(false)
{
	Buffer.AddUninitialized( (InMaxBits+7)>>3 );

	AllowResize = InAllowResize;
	FMemory::Memzero(Buffer.GetData(), Buffer.Num());
	ArIsPersistent = ArIsSaving = 1;
}

/**
 * Default constructor. Zeros everything.
 */
FBitWriter::FBitWriter(void)
	: Num(0)
	, Max(0)
	, AllowResize(false)
	, bAllowOverflow(false)
{
}

/**
 * Resets the bit writer back to its initial state
 */
void FBitWriter::Reset(void)
{
	FArchive::Reset();
	Num = 0;
	FMemory::Memzero(Buffer.GetData(), Buffer.Num());
	ArIsPersistent = ArIsSaving = 1;
}

void FBitWriter::SerializeBits( void* Src, int64 LengthBits )
{
	if( AllowAppend(LengthBits) )
	{
		//for( int32 i=0; i<LengthBits; i++,Num++ )
		//	if( ((uint8*)Src)[i>>3] & GShift[i&7] )
		//		Buffer(Num>>3) |= GShift[Num&7];
		if( LengthBits == 1 )
		{
			if( ((uint8*)Src)[0] & 0x01 )
				Buffer[Num>>3] |= GShift[Num&7];
			Num++;
		}
		else
		{
			appBitsCpy(Buffer.GetData(), Num, (uint8*)Src, 0, LengthBits);
			Num += LengthBits;
		}
	}
	else
	{
		SetOverflowed(LengthBits);
	}
}
void FBitWriter::Serialize( void* Src, int64 LengthBytes )
{
	//warning: Copied and pasted from FBitWriter::SerializeBits
	int64 LengthBits = LengthBytes*8;
	if( AllowAppend(LengthBits) )
	{
		appBitsCpy(Buffer.GetData(), Num, (uint8*)Src, 0, LengthBits);
		Num += LengthBits;
	}
	else
	{
		SetOverflowed(LengthBits);
	}
}

void FBitWriter::SerializeInt(uint32& Value, uint32 ValueMax)
{
	check(ValueMax >= 2);

	const int32 LengthBits = FMath::CeilLogTwo(ValueMax);
	uint32 WriteValue = Value;

	if (WriteValue >= ValueMax)
	{
		const TCHAR Msg[] = TEXT("FBitWriter::SerializeInt(): Value out of bounds (Value: %u, ValueMax: %u)");

		UE_LOG(LogSerialization, Error, Msg, WriteValue, ValueMax);
		ensureMsgf(false, Msg, WriteValue, ValueMax);

		WriteValue = ValueMax - 1;
	}

	if (AllowAppend(LengthBits))
	{
		uint32 NewValue = 0;
		int64 LocalNum = Num;	// Use local var to avoid LHS

		for (uint32 Mask=1; (NewValue + Mask) < ValueMax && Mask; Mask*=2, LocalNum++)
		{
			if (WriteValue & Mask)
			{
				Buffer[LocalNum >> 3] += GShift[LocalNum & 7];
				NewValue += Mask;
			}
		}

		Num = LocalNum;
	}
	else
	{
		SetOverflowed(LengthBits);
	}
}

void FBitWriter::WriteIntWrapped(uint32 Value, uint32 ValueMax)
{
	check(ValueMax >= 2);

	const int32 LengthBits = FMath::CeilLogTwo(ValueMax);

	if (AllowAppend(LengthBits))
	{
		uint32 NewValue = 0;

		for (uint32 Mask=1; NewValue+Mask < ValueMax && Mask; Mask*=2, Num++)
		{
			if (Value & Mask)
			{
				Buffer[Num>>3] += GShift[Num&7];
				NewValue += Mask;
			}
		}
	}
	else
	{
		SetOverflowed(LengthBits);
	}
}
void FBitWriter::WriteBit( uint8 In )
{
	if( AllowAppend(1) )
	{
		if( In )
			Buffer[Num>>3] |= GShift[Num&7];
		Num++;
	}
	else
	{
		SetOverflowed(1);
	}
}

void FBitWriter::SetOverflowed(int32 LengthBits)
{
	if (!bAllowOverflow)
	{
		UE_LOG(LogNetSerialization, Error, TEXT("FBitWriter overflowed! (WriteLen: %i, Remaining: %i, Max: %i)"),
				LengthBits, (Max-Num), Max);
	}

	ArIsError = 1;
}

/*-----------------------------------------------------------------------------
	FBitWriterMark.
-----------------------------------------------------------------------------*/

void FBitWriterMark::Pop( FBitWriter& Writer )
{
	checkSlow(Num<=Writer.Num);
	checkSlow(Num<=Writer.Max);

	if( Num&7 )
	{
		Writer.Buffer[Num>>3] &= GMask[Num&7];
	}
	int32 Start = (Num       +7)>>3;
	int32 End   = (Writer.Num+7)>>3;
	if( End != Start )
	{
		checkSlow(Start<Writer.Buffer.Num());
		checkSlow(End<=Writer.Buffer.Num());
		FMemory::Memzero( &Writer.Buffer[Start], End-Start );
	}
	Writer.ArIsError = Overflowed;
	Writer.Num       = Num;
}
/** Copies the last section into a buffer. Does not clear the FBitWriter like ::Pop does */
void FBitWriterMark::Copy( FBitWriter& Writer, TArray<uint8> &Buffer )
{
	checkSlow(Num<=Writer.Num);
	checkSlow(Num<=Writer.Max);

	int32 Bytes = (Writer.Num - Num + 7) >> 3;
	if( Bytes > 0 )
	{
		Buffer.SetNumUninitialized(Bytes);		// This makes room but doesnt zero
		Buffer[Bytes-1] = 0;	// Make sure the last byte is 0 out, because appBitsCpy wont touch the last bits
		appBitsCpy(Buffer.GetData(), 0, Writer.Buffer.GetData(), Num, Writer.Num - Num);
	}
}

/** Pops the BitWriter back to the start but doesn't clear what was written. */
void FBitWriterMark::PopWithoutClear( FBitWriter& Writer )
{
	Writer.Num = Num;
}

