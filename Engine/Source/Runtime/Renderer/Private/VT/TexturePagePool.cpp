// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TexturePagePool.h"

#include "VirtualTextureSpace.h"
#include "VirtualTextureSystem.h"

template< typename IndexType, typename CountType >
void RadixSort32( IndexType* RESTRICT Dst, IndexType* RESTRICT Src, CountType Num )
{
	CountType Histograms[ 1024 + 2048 + 2048 ];
	CountType* RESTRICT Histogram0 = Histograms + 0;
	CountType* RESTRICT Histogram1 = Histogram0 + 1024;
	CountType* RESTRICT Histogram2 = Histogram1 + 2048;

	FMemory::Memzero( Histograms, sizeof( Histograms ) );

	{
		// Parallel histogram generation pass
		const IndexType* RESTRICT s = (const IndexType* RESTRICT)Src;
		for( CountType i = 0; i < Num; i++ )
		{
			IndexType j = s[i];
			Histogram0[ ( j >>  0 ) & 1023 ]++;
			Histogram1[ ( j >> 10 ) & 2047 ]++;
			Histogram2[ ( j >> 21 ) & 2047 ]++;
		}
	}
	{
		// Set each histogram entry to the sum of entries preceding it
		CountType Sum0 = 0;
		CountType Sum1 = 0;
		CountType Sum2 = 0;
		for( CountType i = 0; i < 1024; i++ )
		{
			CountType t;
			t = Histogram0[i] + Sum0; Histogram0[i] = Sum0 - 1; Sum0 = t;
			t = Histogram1[i] + Sum1; Histogram1[i] = Sum1 - 1; Sum1 = t;
			t = Histogram2[i] + Sum2; Histogram2[i] = Sum2 - 1; Sum2 = t;
		}
		for( CountType i = 1024; i < 2048; i++ )
		{
			CountType t;
			t = Histogram1[i] + Sum1; Histogram1[i] = Sum1 - 1; Sum1 = t;
			t = Histogram2[i] + Sum2; Histogram2[i] = Sum2 - 1; Sum2 = t;
		}
	}
	{
		// Sort pass 1
		const IndexType* RESTRICT s = (const IndexType* RESTRICT)Src;
		IndexType* RESTRICT d = Dst;
		for( CountType i = 0; i < Num; i++ )
		{
			IndexType k = s[i];
			d[ ++Histogram0[ ( (k >> 0) & 1023 ) ] ] = k;
		}
	}
	{
		// Sort pass 2
		const IndexType* RESTRICT s = (const IndexType* RESTRICT)Dst;
		IndexType* RESTRICT d = Src;
		for( CountType i = 0; i < Num; i++ )
		{
			IndexType k = s[i];
			d[ ++Histogram1[ ( (k >> 10) & 2047 ) ] ] = k;
		}
	}
	{
		// Sort pass 3
		const IndexType* RESTRICT s = (const IndexType* RESTRICT)Src;
		IndexType* RESTRICT d = Dst;
		for( CountType i = 0; i < Num; i++ )
		{
			IndexType k = s[i];
			d[ ++Histogram2[ ( (k >> 21) & 2047 ) ] ] = k;
		}
	}
}


FORCEINLINE uint64 EncodeSortKey( uint8 ID, uint8 vLevel, uint64 vAddress )
{
	uint64 Key;
	Key  = (uint64)vAddress	<<  0;
	Key |= (uint64)vLevel	<< 48;
	Key |= (uint64)ID		<< 56;
	return Key;
}

FORCEINLINE void DecodeSortKey( uint64 Key, uint8& ID, uint8& vLevel, uint64& vAddress )
{
	vAddress	= ( Key >>  0 ) & 0xffffffffffffull;
	vLevel		= ( Key >> 48 ) & 0xf;
	ID			= ( Key >> 56 );
}


FTexturePagePool::FTexturePagePool( uint32 Size, uint32 Dimensions )
	: vDimensions( Dimensions )
	, HashTable( 2048, Size )
	, FreeHeap( Size, Size )
{
	Pages.SetNum( Size );
	SortedKeys.Reserve( Size );

	for( int32 i = 0; i < Pages.Num(); i++ )
	{
		FTexturePage& Page = Pages[i];

		Page.pAddress = i;
		Page.vAddress = 0;
		Page.vLevel = 0;
		Page.ID = 0xff;

		FreeHeap.Add( 0, i );
	}
}

FTexturePagePool::~FTexturePagePool()
{}

bool FTexturePagePool::AnyFreeAvailable( uint32 Frame ) const
{
	if( FreeHeap.Num() > 0 )
	{
		// Keys include vLevel to help prevent parent before child ordering
		uint32 PageIndex = FreeHeap.Top();
		uint32 PageFrame = FreeHeap.GetKey( PageIndex ) >> 4;
		return PageFrame != Frame;
	}

	return false;
}

uint32 FTexturePagePool::Alloc( uint32 Frame )
{
	check( AnyFreeAvailable( Frame ) );

	uint32 PageIndex = FreeHeap.Top();
	FreeHeap.Pop();
	return PageIndex;
}

void FTexturePagePool::Free( uint32 Frame, uint32 PageIndex )
{
	FreeHeap.Add( (Frame << 4) + (Pages[ PageIndex ].vLevel & 0xf), PageIndex );
}

void FTexturePagePool::UpdateUsage( uint32 Frame, uint32 PageIndex )
{
	FreeHeap.Update( (Frame << 4) + (Pages[ PageIndex ].vLevel & 0xf), PageIndex );
}


uint32 FTexturePagePool::FindPage( uint8 ID, uint8 vLevel, uint64 vAddress ) const
{
	uint16 Hash = HashPage( vLevel, vAddress, vDimensions );
	for( uint32 PageIndex = HashTable.First( Hash ); HashTable.IsValid( PageIndex ); PageIndex = HashTable.Next( PageIndex ) )
	{
		if( ID			== Pages[ PageIndex ].ID &&
			vLevel		== Pages[ PageIndex ].vLevel &&
			vAddress	== Pages[ PageIndex ].vAddress )
		{
			return PageIndex;
		}
	}

	return ~0u;
}

uint32 FTexturePagePool::FindNearestPage( uint8 ID, uint8 vLevel, uint64 vAddress ) const
{
	while( vLevel < 16 )
	{
		uint16 Hash = HashPage( vLevel, vAddress, vDimensions );
		for( uint32 PageIndex = HashTable.First( Hash ); HashTable.IsValid( PageIndex ); PageIndex = HashTable.Next( PageIndex ) )
		{
			if( ID			== Pages[ PageIndex ].ID &&
				vLevel		== Pages[ PageIndex ].vLevel &&
				vAddress	== Pages[ PageIndex ].vAddress )
			{
				return PageIndex;
			}
		}

		vLevel++;
		vAddress &= ~0ull << ( vDimensions * vLevel );
	}

	return ~0u;
}

void FTexturePagePool::UnmapPage( uint16 pAddress )
{
	FTexturePage& Page = Pages[ pAddress ];

	if( Page.ID != 0xff )
	{
		// Unmap old page
		HashTable.Remove( HashPage( Page.vLevel, Page.vAddress, vDimensions ), Page.pAddress );

		uint32 Ancestor_pAddress = FindNearestPage( Page.ID, Page.vLevel, Page.vAddress );
		uint8  Ancestor_vLevel = Ancestor_pAddress == ~0u ? 0xff : Pages[ Ancestor_pAddress ].vLevel;
		GVirtualTextureSystem.GetSpace( Page.ID )->QueueUpdate( Page.vLevel, Page.vAddress, Ancestor_vLevel, Ancestor_pAddress );

		uint64 OldKey = EncodeSortKey( Page.ID, Page.vLevel, Page.vAddress );
		uint32 OldIndex = LowerBound( 0, SortedKeys.Num(), OldKey, ~0ull );
		SortedSubIndexes.Add( ( OldIndex << 16 ) | Page.pAddress );
	}

	Page.vLevel = 0;
	Page.vAddress = 0;
	Page.ID = 0xff;

	SortedKeysDirty = true;
}

void FTexturePagePool::MapPage( uint8 ID, uint8 vLevel, uint64 vAddress, uint16 pAddress )
{
	FTexturePage& Page = Pages[ pAddress ];

	Page.vLevel = vLevel;
	Page.vAddress = vAddress;
	Page.ID = ID;

	{
		uint64 NewKey = EncodeSortKey( Page.ID, Page.vLevel, Page.vAddress );
		uint32 NewIndex = UpperBound( 0, SortedKeys.Num(), NewKey, ~0ull );
		SortedAddIndexes.Add( ( NewIndex << 16 ) | Page.pAddress );

		// Map new page
		HashTable.Add( HashPage( Page.vLevel, Page.vAddress, vDimensions ), Page.pAddress );
		GVirtualTextureSystem.GetSpace( Page.ID )->QueueUpdate( Page.vLevel, Page.vAddress, Page.vLevel, Page.pAddress );
	}

	SortedKeysDirty = true;
}

// Must call this before the below functions so that SortedKeys is up to date.
inline void FTexturePagePool::BuildSortedKeys()
{
	checkSlow( SortedSubIndexes.Num() || SortedAddIndexes.Num() );

	SortedSubIndexes.Sort();
	SortedAddIndexes.Sort(
		[this]( const uint32& A, const uint32& B )
		{
			const FTexturePage& PageA = Pages[ A & 0xffff ];
			const FTexturePage& PageB = Pages[ B & 0xffff ];

			uint64 KeyA = EncodeSortKey( PageA.ID, PageA.vLevel, PageA.vAddress );
			uint64 KeyB = EncodeSortKey( PageB.ID, PageB.vLevel, PageB.vAddress );

			return KeyA < KeyB;
		} );
	
	// Copy version
	Exchange( SortedKeys,	UnsortedKeys );
	Exchange( SortedIndexes,UnsortedIndexes );

	uint32 NumUnsorted = UnsortedKeys.Num();
	SortedKeys.SetNum(		NumUnsorted + SortedAddIndexes.Num() - SortedSubIndexes.Num(), false );
	SortedIndexes.SetNum(	NumUnsorted + SortedAddIndexes.Num() - SortedSubIndexes.Num(), false );
	
	int32 SubI = 0;
	int32 AddI = 0;
	int32 UnsortedI = 0;
	int32 SortedI = 0;

	while( SortedI < SortedKeys.Num() )
	{
		const uint32 SubIndex = SubI < SortedSubIndexes.Num() ? ( SortedSubIndexes[ SubI ] >> 16 ) : NumUnsorted;
		const uint32 AddIndex = AddI < SortedAddIndexes.Num() ? ( SortedAddIndexes[ AddI ] >> 16 ) : NumUnsorted;

		const uint32 Interval = FMath::Min( SubIndex, AddIndex ) - UnsortedI;
		if( Interval )
		{
			FMemory::Memcpy( &SortedKeys[ SortedI ], &UnsortedKeys[ UnsortedI ], Interval * sizeof( uint64 ) );
			FMemory::Memcpy( &SortedIndexes[ SortedI ], &UnsortedIndexes[ UnsortedI ], Interval * sizeof( uint16 ) );

			UnsortedI += Interval;
			SortedI += Interval;

			if( SortedI >= SortedKeys.Num() )
				break;
		}

		if( SubIndex < AddIndex )
		{
			checkSlow( SubI < SortedSubIndexes.Num() );

			// Skip hole
			UnsortedI++;
			SubI++;
		}
		else
		{
			checkSlow( AddI < SortedAddIndexes.Num() );

			// Add new updated page
			uint16 pAddress = SortedAddIndexes[ AddI ] & 0xffff;

			FTexturePage& Page = Pages[ pAddress ];
			SortedKeys[ SortedI ] = EncodeSortKey( Page.ID, Page.vLevel, Page.vAddress );
			SortedIndexes[ SortedI ] = Page.pAddress;

			SortedI++;
			AddI++;
		}
	}

	SortedSubIndexes.Reset();
	SortedAddIndexes.Reset();

	SortedKeysDirty = false;
}

// Binary search lower bound
// Similar to std::lower_bound
// Range [Min,Max)
uint32 FTexturePagePool::LowerBound( uint32 Min, uint32 Max, uint64 SearchKey, uint64 Mask ) const
{
	while( Min != Max )
	{
		uint32 Mid = Min + (Max - Min) / 2;
		uint64 Key = SortedKeys[ Mid ] & Mask;

		if( SearchKey <= Key )
			Max = Mid;
		else
			Min = Mid + 1;
	}

	return Min;
}

// Binary search upper bound
// Similar to std::upper_bound
// Range [Min,Max)
uint32 FTexturePagePool::UpperBound( uint32 Min, uint32 Max, uint64 SearchKey, uint64 Mask ) const
{
	while( Min != Max )
	{
		uint32 Mid = Min + (Max - Min) / 2;
		uint64 Key = SortedKeys[ Mid ] & Mask;

		if( SearchKey < Key )
			Max = Mid;
		else
			Min = Mid + 1;
	}

	return Min;
}

// Binary search equal range
// Similar to std::equal_range
// Range [Min,Max)
uint32 FTexturePagePool::EqualRange( uint32 Min, uint32 Max, uint64 SearchKey, uint64 Mask ) const
{
	while( Min != Max )
	{
		uint32 Mid = Min + (Max - Min) / 2;
		uint64 Key = SortedKeys[ Mid ] & Mask;

		if( SearchKey < Key )
		{
			Max = Mid;
		}
		else if( SearchKey > Key )
		{
			Min = Mid + 1;
		}
		else
		{	// Range straddles Mid. Search both sides and return.
			Min = LowerBound( Min,     Mid, SearchKey, Mask );
			Max = UpperBound( Mid + 1, Max, SearchKey, Mask );
			return Min | ( Max << 16 );
		}
	}

	return 0;
}

void FTexturePagePool::RefreshEntirePageTable( uint8 ID, TArray< FPageTableUpdate >* Output )
{
	if( SortedKeysDirty )
	{
		BuildSortedKeys();
	}

	// TODO match ID

	for( int i = SortedKeys.Num() - 1; i >= 0; i-- )
	{
		FPageUpdate Update;
		uint8 Update_ID;
		DecodeSortKey( SortedKeys[i], Update_ID, Update.vLevel, Update.vAddress );
		Update.pAddress = SortedIndexes[i];
		Update.vLogSize = Update.vLevel;

		for( int Mip = Update.vLevel; Mip >= 0; Mip-- )
		{
			Output[ Mip ].Add( Update );
		}
	}
}

/*
======================
Update entry in page table for this page and entries for all of its unmapped descendants.

If no mapped descendants then this is a single square per mip.
If there are mapped descendants then draw those on top using painters algorithm.
Outputs list of FPageTableUpdate which will be drawn on the GPU to the page table.
======================
*/
void FTexturePagePool::ExpandPageTableUpdatePainters( uint8 ID, FPageUpdate Update, TArray< FPageTableUpdate >* Output )
{
	if( SortedKeysDirty )
	{
		BuildSortedKeys();
	}

	static TArray< FPageUpdate > LoopOutput;

	LoopOutput.Reset();

	uint8  vLogSize = Update.vLogSize;
	uint32 vAddress = Update.vAddress;
	
	Output[ vLogSize ].Add( Update );

	// Start with input quad
	LoopOutput.Add( Update );

	uint32 SearchRange = SortedKeys.Num();
	
	for( uint32 Mip = vLogSize; Mip > 0; )
	{
		Mip--;
		uint64 SearchKey = EncodeSortKey( ID, Mip, vAddress );
		uint64 Mask = ~0ull << ( vDimensions * vLogSize );
		
		uint32 DescendantRange = EqualRange( 0, SearchRange, SearchKey, Mask );
		if( DescendantRange != 0 )
		{
			uint32 DescendantMin = DescendantRange & 0xffff;
			uint32 DescendantMax = DescendantRange >> 16;

			// List is sorted by level so lower levels must be earlier in the list than what we found.
			SearchRange = DescendantMin;

			for( uint32 DescendantIndex = DescendantMin; DescendantIndex < DescendantMax; DescendantIndex++ )
			{
				checkSlow( SearchKey == ( SortedKeys[ DescendantIndex ] & Mask ) );

				FPageUpdate Descendant;
				uint8 Descendant_ID, Descendant_Level;
				DecodeSortKey( SortedKeys[ DescendantIndex ], Descendant_ID, Descendant_Level, Descendant.vAddress );
				Descendant.pAddress = SortedIndexes[ DescendantIndex ];

				Descendant.vLevel	= Mip;
				Descendant.vLogSize	= Mip;

				checkSlow( Descendant_ID == ID );
				checkSlow( Descendant_Level == Mip );

				// Mask out low bits
				uint64 Ancestor_vAddress = Descendant.vAddress & ( ~0ull << ( vDimensions * vLogSize ) );
				checkSlow( Ancestor_vAddress == vAddress );

				LoopOutput.Add( Descendant );
			}
		}

		Output[ Mip ].Append( LoopOutput );
	}
}

/*
======================
Update entry in page table for this page and entries for all of its unmapped descendants.

If no mapped descendants then this is a single square per mip.
If there are mapped descendants then break it up into many squares in quadtree order with holes for any already mapped pages.
Outputs list of FPageTableUpdate which will be drawn on the GPU to the page table.
======================
*/
void FTexturePagePool::ExpandPageTableUpdateMasked( uint8 ID, FPageUpdate Update, TArray< FPageTableUpdate >* Output )
{
	if( SortedKeysDirty )
	{
		BuildSortedKeys();
	}

	static TArray< FPageUpdate > LoopInput;
	static TArray< FPageUpdate > LoopOutput;
	static TArray< FPageUpdate > Stack;

	LoopInput.Reset();
	LoopOutput.Reset();
	checkSlow( Stack.Num() == 0 );

	uint8  vLogSize = Update.vLogSize;
	uint64 vAddress = Update.vAddress;
	
	Output[ vLogSize ].Add( FPageTableUpdate( Update ) );

	// Start with input quad
	LoopOutput.Add( Update );

	uint32 SearchRange = SortedKeys.Num();
	
	for( uint32 Mip = vLogSize; Mip > 0; )
	{
		Mip--;
		uint64 SearchKey = EncodeSortKey( ID, Mip, vAddress );
		uint64 Mask = ~0ull << ( vDimensions * vLogSize );
		
		uint32 DescendantRange = EqualRange( 0, SearchRange, SearchKey, Mask );
		if( DescendantRange != 0 )
		{
			uint32 DescendantMin = DescendantRange & 0xffff;
			uint32 DescendantMax = DescendantRange >> 16;

			// List is sorted by level so lower levels must be earlier in the list than what we found.
			SearchRange = DescendantMin;

			// Ping pong input and output
			Exchange( LoopInput, LoopOutput );
			LoopOutput.Reset();
			int32 InputIndex = 0;

			Update = LoopInput[ InputIndex++ ];

			for( uint32 DescendantIndex = DescendantMin; DescendantIndex < DescendantMax; )
			{
				checkSlow( SearchKey == ( SortedKeys[ DescendantIndex ] & Mask ) );

				FPageUpdate Descendant;
				uint8 Descendant_ID, Descendant_Level;
				DecodeSortKey( SortedKeys[ DescendantIndex ], Descendant_ID, Descendant_Level, Descendant.vAddress );
				Descendant.pAddress = SortedIndexes[ DescendantIndex ];

				Descendant.vLevel	= Mip;
				Descendant.vLogSize	= Mip;

				checkSlow( Descendant_ID == ID );
				checkSlow( Descendant_Level == Mip );

				// Mask out low bits
				uint64 Ancestor_vAddress = Descendant.vAddress & ( ~0ull << ( vDimensions * vLogSize ) );
				checkSlow( Ancestor_vAddress == vAddress );

				uint32 UpdateSize		= 1 << ( vDimensions * Update.vLogSize );
				uint32 DescendantSize	= 1 << ( vDimensions * Descendant.vLogSize );

				checkSlow( Update.vLogSize >= Mip );

				Update.Check( vDimensions );
				Descendant.Check( vDimensions );

				// Find if Update intersects with Descendant

				// Is update past descendant?
				if( Update.vAddress > Descendant.vAddress )
				{
					checkSlow( Update.vAddress >= Descendant.vAddress + DescendantSize );
					// Move to next descendant
					DescendantIndex++;
					continue;
				}
				// Is update quad before descendant quad and doesn't intersect?
				else if( Update.vAddress + UpdateSize <= Descendant.vAddress )
				{
					// Output this update and fetch next
					LoopOutput.Add( Update );
				}
				// Does update quad equal descendant quad?
				else if(	Update.vAddress	== Descendant.vAddress &&
							Update.vLogSize	== Descendant.vLogSize )
				{
					// Move to next descendant
					DescendantIndex++;
					// Toss this update and fetch next
				}
				else
				{
					checkSlow( Update.vLogSize > Mip );

					// Update intersects with Descendant but isn't the same size
					// Split update into 4 for 2D, 8 for 3D
					Update.vLogSize--;
					for( uint32 Sibling = (1 << vDimensions) - 1; Sibling > 0; Sibling-- )
					{
						Stack.Push( FPageUpdate( Update, Sibling, vDimensions ) );
					}
					continue;
				}

				// Fetch next update
				if( Stack.Num() )
				{
					Update = Stack.Pop( false );
				}
				else if( InputIndex < LoopInput.Num() )
				{
					Update = LoopInput[ InputIndex++ ];
				}
				else
				{
					// No more input
					Update.vLogSize = 0xff;
					break;
				}
			}

			// If update was still being worked with add it
			if( Update.vLogSize != 0xff )
			{
				LoopOutput.Add( Update );
			}
			// Add remaining stack to output
			while( Stack.Num() )
			{
				LoopOutput.Add( Stack.Pop( false ) );
			}
			// Add remaining input to output
			LoopOutput.Append( LoopInput.GetData() + InputIndex, LoopInput.Num() - InputIndex );
		}
		
		if( LoopOutput.Num() == 0 )
		{
			// Completely masked out by descendants
			break;
		}
		else
		{
			Output[ Mip ].Append( LoopOutput );
		}
	}
}