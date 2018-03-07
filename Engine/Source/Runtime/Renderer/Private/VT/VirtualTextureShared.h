// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

FORCEINLINE uint16 HashPage( uint32 vLevel, uint64 vAddress, uint8 vDimensions )
{
	// Mix level into top 4 bits 
	return (uint16)( vLevel << 6 ) ^ (uint16)( vAddress >> ( vDimensions * vLevel ) );
}


struct FPageUpdate
{
	uint64	vAddress;
	uint16	pAddress;
	uint8	vLevel;
	uint8	vLogSize;

	FPageUpdate()
	{}

	FPageUpdate( const FPageUpdate& Update, uint64 Offset, uint8 vDimensions )
		: vAddress( Update.vAddress + ( Offset << ( vDimensions * Update.vLogSize ) ) )
		, pAddress( Update.pAddress )
		, vLevel( Update.vLevel )
		, vLogSize( Update.vLogSize )
	{}

	inline void Check( uint8 vDimensions )
	{
		uint64 LowBitMask = ( 1ull << ( vDimensions * vLogSize ) ) - 1;
		checkSlow( (vAddress & LowBitMask) == 0 );

		checkSlow( vLogSize <= vLevel );
	}
};

// Single page table can't possibly be bigger than 32 bit addressing
struct FPageTableUpdate
{
	uint32	vAddress;
	uint16	pAddress;
	uint8	vLevel;
	uint8	vLogSize;

	FPageTableUpdate( const FPageUpdate& Other )
		: vAddress(	Other.vAddress )
		, pAddress(	Other.pAddress )
		, vLevel(	Other.vLevel )
		, vLogSize(	Other.vLogSize )
	{}

	FPageTableUpdate& operator=( const FPageUpdate& Other )
	{
		vAddress	= Other.vAddress;
		pAddress	= Other.pAddress;
		vLevel		= Other.vLevel;
		vLogSize	= Other.vLogSize;
		
		return *this;
	}
};