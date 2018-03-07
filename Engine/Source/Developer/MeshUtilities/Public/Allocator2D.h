// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FRect;
struct Rect;

class MESHUTILITIES_API FAllocator2D
{
public:
	struct FRect
	{
		uint32 X;
		uint32 Y;
		uint32 W;
		uint32 H;
	};

	struct FSegment
	{
		uint32 StartPos;
		uint32 Length;

		bool operator<( const FSegment& Other ) const { return StartPos < Other.StartPos; }
	};

	struct FRow
	{
		uint32 Index;
		uint32 LongestSegment; // Represents either the longest free segment or the longest used segment, depending on how we're using this row

		TArray< FSegment > FreeSegments;
		TArray< FSegment > UsedSegments;
	};

public:
				FAllocator2D( uint32 Width, uint32 Height );
				FAllocator2D( const FAllocator2D& Other );
				~FAllocator2D();
	
	FAllocator2D& operator=( const FAllocator2D& Other );

	// Must clear before using
	void		Clear();

	bool		Find( FRect& Rect );
	bool		Test( FRect Rect );
	void		Alloc( FRect Rect );

	bool		FindBitByBit( FRect& Rect, const FAllocator2D& Other );
	bool		FindWithSegments( FRect& Rect, FRect BestRect, const FAllocator2D& Other );
	bool		Test( FRect Rect, const FAllocator2D& Other );
	void		Alloc( FRect Rect, const FAllocator2D& Other );
	
	uint64		GetBit( uint32 x, uint32 y ) const;
	void		SetBit( uint32 x, uint32 y );
	void		ClearBit( uint32 x, uint32 y );
	
	void		CreateUsedSegments();
	void		MergeSegments( FRect Rect, const FAllocator2D& Other );

	void		FlipX( FRect Rect );
	void		FlipY( FRect Rect );

protected:
	bool		TestAllRows( FRect Rect, const FAllocator2D& Other, uint32& FailedLength );
	bool		TestRow( const FRow& ThisRow, const FRow& OtherRow, FRect Rect, uint32& FailedLength );

	void		InitSegments();
	void		AddUsedSegment( FRow& Row, uint32 StartPos, uint32 Length );

private:
	uint64*		Bits;

	uint32		Width;
	uint32		Height;
	uint32		Pitch;

	TArray< FRow > Rows;
	int32		LastRowFail;
};

// Returns non-zero if set
FORCEINLINE uint64 FAllocator2D::GetBit( uint32 x, uint32 y ) const
{
	return Bits[ (x >> 6) + y * Pitch ] & ( 1ull << ( x & 63 ) );
}

FORCEINLINE void FAllocator2D::SetBit( uint32 x, uint32 y )
{
	Bits[ (x >> 6) + y * Pitch ] |= ( 1ull << ( x & 63 ) );
}

FORCEINLINE void FAllocator2D::ClearBit( uint32 x, uint32 y )
{
	Bits[ (x >> 6) + y * Pitch ] &= ~( 1ull << ( x & 63 ) );
}

inline bool FAllocator2D::Test( FRect Rect )
{
	for( uint32 y = Rect.Y; y < Rect.Y + Rect.H; y++ )
	{
		for( uint32 x = Rect.X; x < Rect.X + Rect.W; x++ )
		{
			if( GetBit( x, y ) )
			{
				return false;
			}
		}
	}
	
	return true;
}

inline bool FAllocator2D::Test( FRect Rect, const FAllocator2D& Other )
{
	const uint32 LowShift = Rect.X & 63;
	const uint32 HighShift = 64 - LowShift;

	for( uint32 y = 0; y < Rect.H; y++ )
	{
#if 1
		uint32 ThisIndex = (Rect.X >> 6) + (y + Rect.Y) * Pitch;
		uint32 OtherIndex = y * Pitch;

		// Test a uint64 at a time
		for( uint32 x = 0; x < Rect.W; x += 64 )
		{
			// no need to zero out HighInt on wrap around because Other will always be zero outside Rect.
			uint64 LowInt  = Bits[ ThisIndex ];
			uint64 HighInt = Bits[ ThisIndex + 1 ];

			uint64 ThisInt = (HighInt << HighShift) | (LowInt >> LowShift);
			uint64 OtherInt = Other.Bits[ OtherIndex ];

			if( ThisInt & OtherInt )
			{
				return false;
			}

			ThisIndex++;
			OtherIndex++;
		}
#else
		for( uint32 x = 0; x < Rect.W; x++ )
		{
			if( Other.GetBit( x, y ) && GetBit( x + Rect.X, y + Rect.Y ) )
			{
				return false;
			}
		}
#endif
	}
	
	return true;
}
