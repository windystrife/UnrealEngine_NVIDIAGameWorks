// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

class IVirtualTexture
{
public:
					IVirtualTexture( uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ )
						: SizeX( InSizeX )
						, SizeY( InSizeY )
						, SizeZ( InSizeZ )
					{}

	virtual			~IVirtualTexture() = 0;

	// Locates page and returns if page data can be provided at this moment.
	virtual bool	LocatePageData( uint8 vLevel, uint64 vAddress, void* RESTRICT& Location ) const = 0;

	// Produces and fills in texture data for the page in the physical texture(s).
	virtual void	ProducePageData( FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, uint8 vLevel, uint64 vAddress, uint16 pAddress, void* RESTRICT Location ) const = 0;
	
	// Size in pages
	uint32	SizeX;
	uint32	SizeY;
	uint32	SizeZ;
};
