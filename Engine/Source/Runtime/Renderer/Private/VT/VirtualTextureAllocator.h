// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VirtualTextureShared.h"
#include "HashTable.h"

class IVirtualTexture;

// Allocates virtual memory address space
class FVirtualTextureAllocator
{
public:
						FVirtualTextureAllocator( uint32 Size, uint32 Dimensions );
						~FVirtualTextureAllocator() {}

	IVirtualTexture*	Find( uint64 vAddress, uint64& Local_vAddress ) const;
	uint64				Alloc( IVirtualTexture* VT );
	void				Free( IVirtualTexture* VT );
	// TODO				Realloc

private:
	uint32				Find( uint64 vAddress ) const;

	struct FAddressBlock
	{
		IVirtualTexture*	VT;
		uint64				vAddress;
		uint16				NextFree;
		uint16				PrevFree;
		uint8				vLogSize;
		uint8				MipBias;

		FAddressBlock()
		{}

		FAddressBlock( uint8 LogSize )
			: VT( nullptr )
			, vAddress( 0 )
			, NextFree( 0xffff )
			, PrevFree( 0xffff )
			, vLogSize( LogSize )
			, MipBias( 0 )
		{}

		FAddressBlock( const FAddressBlock& Block, uint64 Offset, uint32 Dimensions )
			: VT( nullptr )
			, vAddress( Block.vAddress + ( Offset << ( Dimensions * Block.vLogSize ) ) )
			, NextFree( 0xffff )
			, PrevFree( 0xffff )
			, vLogSize( Block.vLogSize )
			, MipBias( 0 )
		{}
	};

	struct FSortedBlock
	{
		uint64	vAddress;
		uint16	Index;
	};

	uint32	vDimensions;

	TArray< FAddressBlock >	AddressBlocks;
	TArray< uint16 >		FreeList;
	TArray< FSortedBlock >	SortedBlocks;
	FHashTable				HashTable;
};