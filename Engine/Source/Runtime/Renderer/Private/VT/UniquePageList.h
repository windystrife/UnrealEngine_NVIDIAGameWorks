// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VirtualTextureShared.h"
#include "HashTable.h"

// 4k x 4k virtual pages
// 16 IDs
FORCEINLINE uint32 EncodePage( uint32 ID, uint32 vLevel, uint32 vPosition )
{
	uint32 Page;
	Page  = vPosition	<<  0;
	Page |= vLevel		<< 24;
	Page |= ID			<< 28;
	return Page;
}

FORCEINLINE void DecodePage( uint32 Page, uint32& ID, uint32& vLevel, uint32& vPosition )
{
	vPosition	= ( Page >>  0 ) & 0xffffff;
	vLevel		= ( Page >> 24 ) & 0xf;
	ID			= ( Page >> 28 );
}


class FUniquePageList
{
public:
			FUniquePageList();

	void	Add( uint32 Page, uint32 Count );
	void	ExpandByMips( uint32 NumMips );

	uint32	GetNum() const					{ return NumPages; }
	uint32	GetPage( uint32 Index ) const	{ return Pages[ Index ]; }
	uint32	GetCount( uint32 Index ) const	{ return Counts[ Index ]; }
	
	uint8	NumLevels[16];
	uint8	Dimensions[16];

private:
	enum
	{
		HashSize		= 1024,
		MaxUniquePages	= 4096,
	};

	uint32	NumPages;
	uint32	Pages[ MaxUniquePages ];
	uint16	Counts[ MaxUniquePages ];
	
	TStaticHashTable< HashSize, MaxUniquePages > HashTable;
};

FUniquePageList::FUniquePageList()
	: NumPages( 0 )
{}

void FUniquePageList::Add( uint32 Page, uint32 Count )
{
	uint32 ID, vLevel, vPosition;
	DecodePage( Page, ID, vLevel, vPosition );
	uint32 vDimensions = Dimensions[ ID ];

	// Search hash table
	uint16 Hash = HashPage( vLevel, vPosition, vDimensions );
	uint16 Index;
	for( Index = HashTable.First( Hash ); HashTable.IsValid( Index ); Index = HashTable.Next( Index ) )
	{
		if( Page == Pages[ Index ] )
		{
			break;
		}
	}
	if( !HashTable.IsValid( Index ) )
	{
		if( NumPages == MaxUniquePages )
		{
			// Ran out of space
			return;
		}

		// Add page
		Index = NumPages++;
		HashTable.Add( Hash, Index );

		Pages[ Index ] = Page;
		Counts[ Index ] = 0;
	}
	
	// Add count
	Counts[ Index ] += Count;
}

// Expanding the list by mips gives look ahead and faster time to first data when many high res pages are requested.
void FUniquePageList::ExpandByMips( uint32 NumMips )
{
	// TODO cache parent page chains

	for( uint32 i = 0, Num = NumPages; i < Num; i++ )
	{
		uint64 Page = Pages[i];

		uint32 ID, vLevel, vPosition;
		DecodePage( Page, ID, vLevel, vPosition );
		uint32 vDimensions = Dimensions[ ID ];
		uint32 Count = Counts[i];

		for( uint32 Mip = 0; Mip < NumMips; Mip++ )
		{
			vLevel++;
			
			if( vLevel >= NumLevels[ ID ] )
			{
				break;
			}

			// Mask out low bits
			vPosition &= 0xffffffff << ( vDimensions * vLevel );

			Add( EncodePage( ID, vLevel, vPosition ), Count );
		}
	}
}