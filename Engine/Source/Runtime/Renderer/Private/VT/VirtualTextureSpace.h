// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VirtualTextureShared.h"
#include "TexturePagePool.h"
#include "VirtualTextureAllocator.h"
#include "RenderTargetPool.h"

// Virtual memory address space mapped by a page table texture
class FVirtualTextureSpace : public FRenderResource
{
public:
						FVirtualTextureSpace( uint32 InSize, uint8 InDimensions, EPixelFormat InFormat, FTexturePagePool* InPool );
						~FVirtualTextureSpace();

	// FRenderResource interface
	virtual void		InitDynamicRHI() override;
	virtual void		ReleaseDynamicRHI() override;
	
	FRHITexture*		GetPageTableTexture() const		{ return PageTable->GetRenderTargetItem().ShaderResourceTexture; }
	
	void				QueueUpdate( uint8 vLogSize, uint64 vAddress, uint8 vLevel, uint16 pAddress );
	void				ApplyUpdates( FRHICommandList& RHICmdList );

	uint32				ID;
	uint32				PageTableSize;
	uint32				PageTableLevels;
	EPixelFormat		PageTableFormat;
	uint8				Dimensions;
	
	FTexturePagePool*	Pool;

	FVirtualTextureAllocator	Allocator;

private:
	TRefCountPtr< IPooledRenderTarget >	PageTable;
	
	TArray< FPageUpdate >		PageTableUpdates;

	FStructuredBufferRHIRef		UpdateBuffer;
	FShaderResourceViewRHIRef	UpdateBufferSRV;
};
