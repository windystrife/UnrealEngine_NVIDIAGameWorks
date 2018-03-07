// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyIndexBuffer.cpp: Empty Index buffer RHI implementation.
=============================================================================*/

#include "EmptyRHIPrivate.h"


/** Constructor */
FEmptyIndexBuffer::FEmptyIndexBuffer(uint32 InStride, uint32 InSize, uint32 InUsage)
	: FRHIIndexBuffer(InStride, InSize, InUsage)
{
}

void* FEmptyIndexBuffer::Lock(EResourceLockMode LockMode, uint32 Size)
{
	return NULL;
}

void FEmptyIndexBuffer::Unlock()
{

}

FIndexBufferRHIRef FEmptyDynamicRHI::RHICreateIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	// make the RHI object, which will allocate memory
	FEmptyIndexBuffer* IndexBuffer = new FEmptyIndexBuffer(Stride, Size, InUsage);
	
	if(CreateInfo.ResourceArray)
	{
		check(Size == CreateInfo.ResourceArray->GetResourceDataSize());

		// make a buffer usable by CPU
		void* Buffer = RHILockIndexBuffer(IndexBuffer, 0, Size, RLM_WriteOnly);

		// copy the contents of the given data into the buffer
		FMemory::Memcpy(Buffer, CreateInfo.ResourceArray->GetResourceData(), Size);

		RHIUnlockIndexBuffer(IndexBuffer);

		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	return IndexBuffer;
}

void* FEmptyDynamicRHI::RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	FEmptyIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	return (uint8*)IndexBuffer->Lock(LockMode, Size) + Offset;
}

void FEmptyDynamicRHI::RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI)
{
	FEmptyIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	IndexBuffer->Unlock();
}
