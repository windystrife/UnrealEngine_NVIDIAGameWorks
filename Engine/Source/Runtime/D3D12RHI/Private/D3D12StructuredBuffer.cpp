// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"

D3D12_RESOURCE_DESC CreateStructuredBufferResourceDesc(uint32 Size, uint32 InUsage)
{
	// Describe the structured buffer.
	D3D12_RESOURCE_DESC Desc = CD3DX12_RESOURCE_DESC::Buffer(Size);

	if ((InUsage & BUF_ShaderResource) == 0)
	{
		Desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	if (InUsage & BUF_UnorderedAccess)
	{
		Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	if (InUsage & BUF_DrawIndirect)
	{
		Desc.Flags |= D3D12RHI_RESOURCE_FLAG_ALLOW_INDIRECT_BUFFER;
	}


	return Desc;
}

FStructuredBufferRHIRef FD3D12DynamicRHI::CreateStructuredBuffer_RenderThread(FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	// Check for values that will cause D3D calls to fail
	check(Size / Stride > 0 && Size % Stride == 0);

	const D3D12_RESOURCE_DESC Desc = CreateStructuredBufferResourceDesc(Size, InUsage);

	// Structured buffers, non-byte address buffers, need to be aligned to their stride to ensure that they
	// can be addressed correctly with element based offsets.
	const uint32 Alignment = ((InUsage & (BUF_ByteAddressBuffer | BUF_DrawIndirect)) == 0) ? Stride : 4;

	FD3D12StructuredBuffer* NewBuffer = GetAdapter().CreateRHIBuffer<FD3D12StructuredBuffer>(&RHICmdList, Desc, Alignment, Stride, Size, InUsage, CreateInfo, false);
	if (NewBuffer->ResourceLocation.IsTransient())
	{
		// TODO: this should ideally be set in platform-independent code, since this tracking is for the high level
		NewBuffer->SetCommitted(false);
	}

	UpdateBufferStats(&NewBuffer->ResourceLocation, true, D3D12_BUFFER_TYPE_STRUCTURED);

	return NewBuffer;
}

FStructuredBufferRHIRef FD3D12DynamicRHI::RHICreateStructuredBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	// Check for values that will cause D3D calls to fail
	check(Size / Stride > 0 && Size % Stride == 0);

	const D3D12_RESOURCE_DESC Desc = CreateStructuredBufferResourceDesc(Size, InUsage);

	// Structured buffers, non-byte address buffers, need to be aligned to their stride to ensure that they
	// can be addressed correctly with element based offsets.
	const uint32 Alignment = ((InUsage & (BUF_ByteAddressBuffer | BUF_DrawIndirect)) == 0) ? Stride : 4;

	FD3D12StructuredBuffer* NewBuffer = GetAdapter().CreateRHIBuffer<FD3D12StructuredBuffer>(nullptr, Desc, Alignment, Stride, Size, InUsage, CreateInfo, false);
	if (NewBuffer->ResourceLocation.IsTransient())
	{
		// TODO: this should ideally be set in platform-independent code, since this tracking is for the high level
		NewBuffer->SetCommitted(false);
	}

	UpdateBufferStats(&NewBuffer->ResourceLocation, true, D3D12_BUFFER_TYPE_STRUCTURED);

	return NewBuffer;
}

FD3D12StructuredBuffer::~FD3D12StructuredBuffer()
{
	UpdateBufferStats(&ResourceLocation, false, D3D12_BUFFER_TYPE_STRUCTURED);
}

void FD3D12StructuredBuffer::Rename(FD3D12ResourceLocation& NewResource)
{
	FD3D12ResourceLocation::TransferOwnership(ResourceLocation, NewResource);
}

void* FD3D12DynamicRHI::RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	return LockBuffer(nullptr, FD3D12DynamicRHI::ResourceCast(StructuredBufferRHI), Offset, Size, LockMode);
}

void FD3D12DynamicRHI::RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	UnlockBuffer(nullptr, FD3D12DynamicRHI::ResourceCast(StructuredBufferRHI));
}
