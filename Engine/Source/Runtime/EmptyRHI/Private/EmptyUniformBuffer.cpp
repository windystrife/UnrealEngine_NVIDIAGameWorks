// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyConstantBuffer.cpp: Empty Constant buffer implementation.
=============================================================================*/

#include "EmptyRHIPrivate.h"


FEmptyGlobalUniformBuffer::FEmptyGlobalUniformBuffer(uint32 InSize) 
	: MaxSize(InSize)
	, bIsDirty(false)
	, ShadowData(NULL)
	, CurrentUpdateSize(0)
	, TotalUpdateSize(0)
{
	InitResource();
}

FEmptyGlobalUniformBuffer::~FEmptyGlobalUniformBuffer()
{
	ReleaseResource();
}

void FEmptyGlobalUniformBuffer::InitDynamicRHI()
{
	// allocate the local shadow copy of the data
	ShadowData = (uint8*)FMemory::Malloc(MaxSize);
	FMemory::Memzero(ShadowData, MaxSize);
}

void FEmptyGlobalUniformBuffer::ReleaseDynamicRHI()
{
	// free local shadow copy
	FMemory::Free(ShadowData);
}

void FEmptyGlobalUniformBuffer::UpdateConstant(const uint8* Data, uint16 Offset, uint16 Size)
{
	// copy the constant into the buffer
	FMemory::Memcpy(ShadowData + Offset, Data, Size);
	
	// mark the highest point used in the buffer
	CurrentUpdateSize = FPlatformMath::Max<uint32>(Offset + Size, CurrentUpdateSize);
	
	// this buffer is now dirty
	bIsDirty = true;
}




/*-----------------------------------------------------------------------------
	Uniform buffer RHI object
-----------------------------------------------------------------------------*/

FEmptyUniformBuffer::FEmptyUniformBuffer(const FRHIUniformBufferLayout& InLayout, const void* Contents, uint32 NumBytes, EUniformBufferUsage Usage)
	: FRHIUniformBuffer(InLayout)
{
}

FEmptyUniformBuffer::~FEmptyUniformBuffer()
{
}

FUniformBufferRHIRef FEmptyDynamicRHI::RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage)
{
	check(IsInRenderingThread());
	return nullptr;//new FEmptyUniformBuffer(Contents, NumBytes, Usage);
}
