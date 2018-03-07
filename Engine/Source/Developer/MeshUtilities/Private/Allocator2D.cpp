// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Allocator2D.h"

FAllocator2D::FAllocator2D( uint32 InWidth, uint32 InHeight )
	: Width( InWidth )
	, Height( InHeight )
	, LastRowFail( -1 )
{
	Pitch = ( Width + 63 ) / 64;
	// alloc +1 to avoid buffer overrun
	Bits = (uint64*)FMemory::Malloc( (Pitch * Height + 1) * sizeof(uint64) );

	Rows.Reserve( Height );

	for ( uint32 Y = 0; Y < Height; ++Y )
	{
		FRow Row;
		Row.Index = Y;

		Rows.Add( Row );
	}

	Clear();
}

FAllocator2D::~FAllocator2D()
{
	FMemory::Free( Bits );
}

FAllocator2D::FAllocator2D( const FAllocator2D& Other )
	: Width( Other.Width )
	, Height( Other.Height )
	, Pitch( Other.Pitch )
	, Rows( Other.Rows )
	, LastRowFail( -1 )
{
	Bits = (uint64*)FMemory::Malloc( (Pitch * Height + 1) * sizeof(uint64) );
	FMemory::Memcpy( Bits, Other.Bits, (Pitch * Height + 1) * sizeof(uint64) );
}

FAllocator2D& FAllocator2D::operator=( const FAllocator2D& Other )
{
	if ( Width != Other.Width || Height != Other.Height || Pitch != Other.Pitch )
	{
		Width = Other.Width;
		Height = Other.Height;
		Pitch = Other.Pitch;

		FMemory::Free( Bits );
		Bits = (uint64*)FMemory::Malloc( (Pitch * Height + 1) * sizeof(uint64) );
	}

	FMemory::Memcpy( Bits, Other.Bits, (Pitch * Height + 1) * sizeof(uint64) );

	Rows = Other.Rows;

	return *this;
}

void FAllocator2D::Clear()
{
	InitSegments();

	FMemory::Memzero( Bits, (Pitch * Height + 1) * sizeof(uint64) );
}

bool FAllocator2D::Find( FRect& Rect )
{
	FRect TestRect = Rect;
	for( TestRect.X = 0; TestRect.X <= Width - TestRect.W; TestRect.X++ )
	{
		for( TestRect.Y = 0; TestRect.Y <= Height - TestRect.H; TestRect.Y++ )
		{
			if( Test( TestRect ) )
			{
				Rect = TestRect;
				return true;
			}
		}
	}

	return false;
}

bool FAllocator2D::FindBitByBit( FRect& Rect, const FAllocator2D& Other )
{
	FRect TestRect = Rect;
	for( TestRect.X = 0; TestRect.X <= Width - TestRect.W; TestRect.X++ )
	{
		for( TestRect.Y = 0; TestRect.Y <= Height - TestRect.H; TestRect.Y++ )
		{
			if( Test( TestRect, Other ) )
			{
				Rect = TestRect;
				return true;
			}
		}
	}
	
	return false;
}

bool FAllocator2D::FindWithSegments( FRect& Rect, FRect BestRect, const FAllocator2D& Other )
{
	LastRowFail = -1;
	FRect TestRect = Rect;

	for( TestRect.Y = 0; TestRect.Y <= Height - TestRect.H; ++TestRect.Y )
	{
		uint32 FailedLength = 1;

		for( TestRect.X = 0; TestRect.X <= Width - TestRect.W; TestRect.X += FailedLength )
		{
			if( TestRect.X + TestRect.Y * Height >= BestRect.X + BestRect.Y * Height )
			{
				// We've moved past the BestRect
				return false;
			}

			if( TestAllRows( TestRect, Other, FailedLength ) )
			{
				Rect = TestRect;
				return true;
			}
		}
	}
	
	return false;
}

void FAllocator2D::Alloc( FRect Rect )
{
	for( uint32 y = Rect.Y; y < Rect.Y + Rect.H; y++ )
	{
		for( uint32 x = Rect.X; x < Rect.X + Rect.W; x++ )
		{
			SetBit( x, y );
		}
	}
}

void FAllocator2D::Alloc( FRect Rect, const FAllocator2D& Other )
{
	for( uint32 y = 0; y < Rect.H; y++ )
	{
		for( uint32 x = 0; x < Rect.W; x++ )
		{
			if( Other.GetBit( x, y ) )
			{
				SetBit( x + Rect.X, y + Rect.Y );
			}
		}
	}

	MergeSegments( Rect, Other );
}

bool FAllocator2D::TestAllRows( FRect Rect, const FAllocator2D& Other, uint32& FailedLength )
{
	FailedLength = 0.f;
	bool bSuccess = true;

	if ( LastRowFail > 0 )
	{
		const FRow& ThisRow = Rows[ Rect.Y + LastRowFail ];
		const FRow& OtherRow = Other.Rows[ LastRowFail ];

		if ( !TestRow( ThisRow, OtherRow, Rect, FailedLength ) )
		{
			return false;
		}
	}

	LastRowFail = -1;

	for ( uint32 y = 0; y < Rect.H; ++y )
	{
		const FRow& ThisRow = Rows[ Rect.Y + y ];
		const FRow& OtherRow = Other.Rows[ y ];

		uint32 CurrentFailedLength = 0;
		bool bThisRowSuccess = TestRow( ThisRow, OtherRow, Rect, CurrentFailedLength );
		bSuccess = bThisRowSuccess && bSuccess;

		if ( !bThisRowSuccess && CurrentFailedLength > FailedLength )
		{
			LastRowFail = y;
			FailedLength = CurrentFailedLength;
		}

		if ( FailedLength >= Width )
		{
			return false;
		}
	}

	return bSuccess;
}

bool FAllocator2D::TestRow( const FRow& ThisRow, const FRow& OtherRow, FRect Rect, uint32& FailedLength )
{
	if ( ThisRow.LongestSegment < OtherRow.LongestSegment )
	{
		FailedLength = Width;
		return false;
	}

	uint32 StartFreeSegmentIndex = 0;

	for ( const FSegment& OtherUsedSegment : OtherRow.UsedSegments )
	{
		if ( OtherUsedSegment.StartPos >= Rect.W )
		{
			break;
		}

		bool bFoundSpaceForSegment = false;
		bool bFoundFutureSpaceForSegment = false;

		const uint32 StartPos = Rect.X + OtherUsedSegment.StartPos;
		const uint32 EndPos = Rect.X + FMath::Min( OtherUsedSegment.StartPos + OtherUsedSegment.Length, Rect.W );

		for ( int32 i = StartFreeSegmentIndex; i < ThisRow.FreeSegments.Num(); ++i )
		{
			const FSegment& ThisFreeSegment = ThisRow.FreeSegments[i];

			if ( StartPos >= ThisFreeSegment.StartPos &&
				EndPos <= ThisFreeSegment.StartPos + ThisFreeSegment.Length )
			{
				StartFreeSegmentIndex = i;
				bFoundSpaceForSegment = true;
				break;
			}
			else if ( !bFoundFutureSpaceForSegment &&
				StartPos < ThisFreeSegment.StartPos &&
				OtherUsedSegment.Length <= ThisFreeSegment.Length )
			{
				bFoundFutureSpaceForSegment = true;
				FailedLength = ThisFreeSegment.StartPos - StartPos;
				break;
			}
		}

		if ( !bFoundSpaceForSegment )
		{
			if ( !bFoundFutureSpaceForSegment )
			{
				FailedLength = Width;
			}

			return false;
		}
	}

	return true;
}

void FAllocator2D::FlipX( FRect Rect )
{
	uint32 MinY = 0;

	uint32 MaxY = Rect.H - 1;
	for ( int32 Y = Rect.H - 1; Y > 0; --Y )
	{
		if ( Rows[ Y ].UsedSegments.Num() > 0 )
			break;

		--MaxY;
	}

	for ( uint32 Y = 0; Y <= MaxY; ++Y )
	{
		for ( uint32 LowX = 0; LowX < Rect.W / 2; ++LowX )
		{
			uint32 HighX = Rect.W - 1 - LowX;
		
			const uint64 BitLow = GetBit( LowX, Y );
			const uint64 BitHigh = GetBit( HighX, Y );

			if ( BitLow != 0ull )
			{
				SetBit( HighX, Y );
			}
			else
			{
				ClearBit( HighX, Y );
			}

			if ( BitHigh != 0ull )
			{
				SetBit( LowX, Y );
			}
			else
			{
				ClearBit( LowX, Y );
			}
		}
	}

	CreateUsedSegments();
}

void FAllocator2D::FlipY( FRect Rect )
{
	uint32 MinY = 0;

	uint32 MaxY = Rect.H - 1;
	for ( int32 Y = Rect.H - 1; Y > 0; --Y )
	{
		if ( Rows[ Y ].UsedSegments.Num() > 0 )
			break;

		--MaxY;
	}

	for ( uint32 LowY = 0; LowY < ( MaxY + 1 ) / 2; ++LowY )
	{
		for ( uint32 X = 0; X < Rect.W; ++X )
		{
			uint32 HighY = MaxY - LowY;

			const uint64 BitLow = GetBit( X, LowY );
			const uint64 BitHigh = GetBit( X, HighY );

			if ( BitLow != 0ull )
			{
				SetBit( X, HighY );
			}
			else
			{
				ClearBit( X, HighY );
			}

			if ( BitHigh != 0ull )
			{
				SetBit( X, LowY );
			}
			else
			{
				ClearBit( X, LowY );
			}
		}
	}

	for ( uint32 LowY = MinY; LowY < ( MaxY + 1 ) / 2; ++LowY )
	{
		const int32 HighY = MaxY - LowY;

		Rows.Swap( LowY, HighY );
		Rows[ LowY ].Index = LowY;
		Rows[ HighY ].Index = HighY;
	}
}

void FAllocator2D::InitSegments()
{
	FSegment FreeSegment;
	FreeSegment.StartPos = 0;
	FreeSegment.Length = Width;

	for ( FRow& Row : Rows )
	{
		Row.FreeSegments.Empty( 1 );
		Row.FreeSegments.Add( FreeSegment );
		Row.LongestSegment = FreeSegment.Length;

		Row.UsedSegments.Empty();

	}
}

void FAllocator2D::CreateUsedSegments()
{
	for ( uint32 y = 0; y < Height; ++y )
	{
		FRow& CurrentRow = Rows[y];
		CurrentRow.LongestSegment = 0;
		CurrentRow.UsedSegments.Empty();

		int32 FirstUsedX = -1;
		for ( uint32 k = 0; k < Pitch; ++k )
		{
			uint32 x = k * 64;

			// If all bits are set
			if ( Bits[k + y * Pitch] == ~0ull )
			{
				if ( FirstUsedX < 0 )
				{
					FirstUsedX = x;
				}
					
				if ( k == Pitch - 1 )
				{
					AddUsedSegment( CurrentRow, FirstUsedX, x + 64 - FirstUsedX );
					FirstUsedX = -1;
				}
			}
			// No bits are set
			else if ( Bits[k + y * Pitch] == 0ull )
			{
				if ( FirstUsedX >= 0 )
				{
					AddUsedSegment( CurrentRow, FirstUsedX, x - FirstUsedX );
					FirstUsedX = -1;
				}
			}
			// Some bits are set
			else
			{
				for ( uint32 i = 0; i < 64; ++i )
				{
					const uint32 SubX = x + i;

					if ( GetBit(SubX, y) )
					{
						if ( FirstUsedX < 0 )
						{
							FirstUsedX = SubX;
						}
							
						if ( SubX == Width - 1 )
						{
							AddUsedSegment( CurrentRow, FirstUsedX, SubX + 1 - FirstUsedX );
							FirstUsedX = -1;
						}
					}
					else if ( FirstUsedX >= 0 )
					{
						AddUsedSegment( CurrentRow, FirstUsedX, SubX - FirstUsedX );
						FirstUsedX = -1;
					}
				}
			}
		}
	}
}

void FAllocator2D::AddUsedSegment( FRow& Row, uint32 StartPos, uint32 Length )
{
	FSegment UsedSegment;
	UsedSegment.StartPos = StartPos;
	UsedSegment.Length = Length;

	Row.UsedSegments.Add(UsedSegment);

	if ( UsedSegment.Length > Row.LongestSegment )
	{
		Row.LongestSegment = UsedSegment.Length;
	}
}

void FAllocator2D::MergeSegments( FRect Rect, const FAllocator2D& Other )
{
	for ( uint32 y = 0; y < Rect.H; ++y )
	{
		FRow& ThisRow = Rows[Rect.Y + y];
		const FRow& OtherRow = Other.Rows[y];

		for ( const FSegment& OtherUsedSegment : OtherRow.UsedSegments )
		{
			for ( int32 i = 0; i < ThisRow.FreeSegments.Num(); ++i )
			{
				FSegment& ThisFreeSegment = ThisRow.FreeSegments[i];

				uint32 StartPos = Rect.X + OtherUsedSegment.StartPos;

				if ( StartPos >= ThisFreeSegment.StartPos &&
					StartPos < ThisFreeSegment.StartPos + ThisFreeSegment.Length )
				{
					if ( ThisFreeSegment.Length == 1 )
					{
						ThisRow.FreeSegments.RemoveAtSwap(i);
						break;
					}

					FSegment FirstSegment;
					FirstSegment.StartPos = ThisFreeSegment.StartPos;
					FirstSegment.Length = StartPos - ThisFreeSegment.StartPos;

					uint32 EndPos = Rect.X + FMath::Min( OtherUsedSegment.StartPos + OtherUsedSegment.Length, Rect.W ) - 1;

					FSegment SecondSegment;
					SecondSegment.StartPos = EndPos + 1;
					SecondSegment.Length = ThisFreeSegment.StartPos + ThisFreeSegment.Length - SecondSegment.StartPos;

					ThisRow.FreeSegments.RemoveAtSwap(i);

					if ( FirstSegment.Length > 0 )
					{
						ThisRow.FreeSegments.Add( FirstSegment );
					}
					
					if ( SecondSegment.Length > 0 )
					{
						ThisRow.FreeSegments.Add( SecondSegment );
					}

					break;
				}
			}
		}

		ThisRow.FreeSegments.Sort();

		ThisRow.LongestSegment = 0;
		for ( const FSegment& Segment : ThisRow.FreeSegments )
		{
			ThisRow.LongestSegment = FMath::Max( Segment.Length, ThisRow.LongestSegment );
		}
	}
}
