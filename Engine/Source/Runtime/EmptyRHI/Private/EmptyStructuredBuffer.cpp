// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "EmptyRHIPrivate.h"


FEmptyStructuredBuffer::FEmptyStructuredBuffer(uint32 Stride, uint32 Size, FResourceArrayInterface* ResourceArray, uint32 Usage)
	: FRHIStructuredBuffer(Stride, Size, Usage)
{
	check((Size % Stride) == 0);

	// copy any resources to the CPU address
	if (ResourceArray)
	{
// 		FMemory::Memcpy( , ResourceArray->GetResourceData(), Size);
		ResourceArray->Discard();
	}
}

FEmptyStructuredBuffer::~FEmptyStructuredBuffer()
{

}




FStructuredBufferRHIRef FEmptyDynamicRHI::RHICreateStructuredBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	return nullptr;//new FEmptyStructuredBuffer(Stride, Size, ResourceArray, InUsage);
}

void* FEmptyDynamicRHI::RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI,uint32 Offset,uint32 Size,EResourceLockMode LockMode)
{
	FEmptyStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	return nullptr;
}

void FEmptyDynamicRHI::RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI)
{

}
