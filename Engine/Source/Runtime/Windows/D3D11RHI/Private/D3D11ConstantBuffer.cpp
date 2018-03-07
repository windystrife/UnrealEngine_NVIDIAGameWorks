// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11ConstantBuffer.cpp: D3D Constant buffer RHI implementation.
=============================================================================*/

#include "D3D11RHIPrivate.h"

// SHANOND: Looks like UpdateSubresource is going to be the way to update these CBs.
//			The driver writers are trying to optimize for UpdateSubresource and this
//			will also avoid any driver renaming issues we may hit with map_discard.

#define MAX_POOL_BUFFERS 1 // use update subresource and plop it into the command stream

/** Sizes of constant buffers defined in ED3D11ShaderOffsetBuffer. */
const uint32 GConstantBufferSizes[MAX_CONSTANT_BUFFER_SLOTS] = 
{
	// CBs must be a multiple of 16
	(uint32)Align(MAX_GLOBAL_CONSTANT_BUFFER_SIZE, 16),
};

// New circular buffer system for faster constant uploads.  Avoids CopyResource and speeds things up considerably
FD3D11ConstantBuffer::FD3D11ConstantBuffer(FD3D11DynamicRHI* InD3DRHI, uint32 InSize, uint32 SubBuffers) : 
	D3DRHI(InD3DRHI),
	MaxSize(InSize),
	ShadowData(NULL),
	CurrentUpdateSize(0),
	TotalUpdateSize(0)
{
	InitResource();
}

FD3D11ConstantBuffer::~FD3D11ConstantBuffer()
{
	ReleaseResource();
}

/**
* Creates a constant buffer on the device
*/
void FD3D11ConstantBuffer::InitDynamicRHI()
{
// New circular buffer system for faster constant uploads.  Avoids CopyResource and speeds things up considerably
	// aligned for best performance
	ShadowData = (uint8*)FMemory::Malloc(MaxSize, 16);
	FMemory::Memzero(ShadowData,MaxSize);
}

void FD3D11ConstantBuffer::ReleaseDynamicRHI()
{
	if(ShadowData)
	{
		FMemory::Free(ShadowData);
	}
}
