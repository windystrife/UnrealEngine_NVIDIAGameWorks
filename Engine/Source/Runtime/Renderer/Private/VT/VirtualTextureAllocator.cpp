// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureAllocator.h"

#include "VirtualTexture.h"

FVirtualTextureAllocator::FVirtualTextureAllocator( uint32 Size, uint32 Dimensions )
	: vDimensions( Dimensions )
{
	uint8 LogSize = FMath::CeilLogTwo( Size );
	
	// Start with one empty block
	AddressBlocks.Add( FAddressBlock( LogSize ) );
	SortedBlocks.AddUninitialized();
	SortedBlocks[0].vAddress = 0;
	SortedBlocks[0].Index = 0;
	
	// Init free list
	FreeList.AddUninitialized( LogSize + 1 );
	for( uint8 i = 0; i < LogSize; i++ )
	{
		FreeList[i] = 0xffff;
	}
	FreeList[ LogSize ] = 0;
}

// returns SortedIndex
uint32 FVirtualTextureAllocator::Find( uint64 vAddress ) const
{
	uint32 Min = 0;
	uint32 Max = SortedBlocks.Num();
	
	// Binary search for upper bound
	while( Min != Max )
	{
		uint32 Mid = Min + (Max - Min) / 2;
		uint32 Key = SortedBlocks[ Mid ].vAddress;

		if( vAddress < Key )
			Max = Mid;
		else
			Min = Mid + 1;
	}

	return Min - 1;
}

IVirtualTexture* FVirtualTextureAllocator::Find( uint64 vAddress, uint64& Local_vAddress ) const
{
	uint32 SortedIndex = Find( vAddress );

	const FSortedBlock&	SortedBlock = SortedBlocks[ SortedIndex ];
	const FAddressBlock& AddressBlock = AddressBlocks[ SortedBlock.Index ];
	checkSlow( SortedBlock.vAddress == AddressBlock.vAddress );

	uint32 BlockSize = 1 << ( vDimensions * AddressBlock.vLogSize );
	if( vAddress >= AddressBlock.vAddress &&
		vAddress <  AddressBlock.vAddress + BlockSize )
	{
		Local_vAddress = vAddress - AddressBlock.vAddress;
		// TODO mip bias
		return AddressBlock.VT;
	}

	return nullptr;
}

uint64 FVirtualTextureAllocator::Alloc( IVirtualTexture* VT )
{
	uint32 BlockSize = FMath::Max( VT->SizeX, VT->SizeY );
	uint8 vLogSize = FMath::CeilLogTwo( BlockSize );

	// Find smallest free that fits
	for( int i = vLogSize; i < FreeList.Num(); i++ )
	{
		uint16 FreeIndex = FreeList[i];
		if( FreeIndex != 0xffff )
		{
			// Found free
			FAddressBlock& AllocBlock = AddressBlocks[ FreeIndex ];
			checkSlow( AllocBlock.VT == nullptr );
			checkSlow( AllocBlock.PrevFree == 0xffff );

			// Remove from free list
			FreeList[i] = AllocBlock.NextFree;
			if( AllocBlock.NextFree != 0xffff )
			{
				AddressBlocks[ AllocBlock.NextFree ].PrevFree = 0xffff;
				AllocBlock.NextFree = 0xffff;
			}

			AllocBlock.VT = VT;

			// Add to hash table
			uint16 Key = reinterpret_cast< UPTRINT >( VT ) / 16;
			HashTable.Add( Key, FreeIndex );

			// Recursive subdivide until the right size
			int NumNewBlocks = 0;
			while( AllocBlock.vLogSize > vLogSize )
			{
				AllocBlock.vLogSize--;
				const uint32 NumSiblings = (1 << vDimensions) - 1;
				for( uint32 Sibling = NumSiblings; Sibling > 0; Sibling-- )
				{
					AddressBlocks.Add( FAddressBlock( AllocBlock, Sibling, vDimensions ) );
				}
				NumNewBlocks += NumSiblings;
			}

			uint32 SortedIndex = Find( AllocBlock.vAddress ) + 1;
			checkSlow( AllocBlock.vAddress == SortedBlocks[ SortedIndex ].vAddress );

			// Make room for newly added
			SortedBlocks.InsertUninitialized( SortedIndex, NumNewBlocks );

			for( int Block = 0; Block < NumNewBlocks; Block++ )
			{
				uint32 Index = AddressBlocks.Num() - NumNewBlocks + Block;
				FAddressBlock AddressBlock = AddressBlocks[ Index ];

				// Place on free list
				AddressBlock.NextFree = FreeList[ AddressBlock.vLogSize ];
				AddressBlocks[ AddressBlock.NextFree ].PrevFree = Index;
				FreeList[ AddressBlock.vLogSize ] = Index;

				// Add to sorted list
				SortedBlocks[ SortedIndex + Block ].vAddress = AddressBlock.vAddress;
				SortedBlocks[ SortedIndex + Block ].Index = Index;
			}

			return AllocBlock.vAddress;
		}
	}

	return ~0u;
}

void FVirtualTextureAllocator::Free( IVirtualTexture* VT )
{
	// Find block index
	uint16 Key = reinterpret_cast< UPTRINT >( VT ) / 16;
	uint32 Index;
	for( Index = HashTable.First( Key ); HashTable.IsValid( Index ); Index = HashTable.Next( Index ) )
	{
		if( AddressBlocks[ Index ].VT == VT )
		{
			break;
		}
	}
	if( HashTable.IsValid( Index ) )
	{
		FAddressBlock& AddressBlock = AddressBlocks[ Index ];
		checkSlow( AddressBlock.VT == VT );
		checkSlow( AddressBlock.NextFree == 0xffff );
		checkSlow( AddressBlock.PrevFree == 0xffff );

		AddressBlock.VT = nullptr;

		// TODO merge with sibling free blocks

		// Place on free list
		AddressBlock.NextFree = FreeList[ AddressBlock.vLogSize ];
		AddressBlocks[ AddressBlock.NextFree ].PrevFree = Index;
		FreeList[ AddressBlock.vLogSize ] = Index;
	}
}