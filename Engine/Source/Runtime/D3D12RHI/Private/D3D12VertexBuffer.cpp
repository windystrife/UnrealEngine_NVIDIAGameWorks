// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12VertexBuffer.cpp: D3D vertex buffer RHI implementation.
	=============================================================================*/

#include "D3D12RHIPrivate.h"

D3D12_RESOURCE_DESC CreateVertexBufferResourceDesc(uint32 Size, uint32 InUsage)
{
	// Describe the vertex buffer.
	D3D12_RESOURCE_DESC Desc = CD3DX12_RESOURCE_DESC::Buffer(Size);

	if (InUsage & BUF_UnorderedAccess)
	{
		Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		static bool bRequiresRawView = (GMaxRHIFeatureLevel < ERHIFeatureLevel::SM5);
		if (bRequiresRawView)
		{
			// Force the buffer to be a raw, byte address buffer
			InUsage |= BUF_ByteAddressBuffer;
		}
	}

	if ((InUsage & BUF_ShaderResource) == 0)
	{
		Desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	if (InUsage & BUF_DrawIndirect)
	{
		Desc.Flags |= D3D12RHI_RESOURCE_FLAG_ALLOW_INDIRECT_BUFFER;
	}

	return Desc;
}

FD3D12VertexBuffer::~FD3D12VertexBuffer()
{
	if (ResourceLocation.GetResource() != nullptr)
	{
		UpdateBufferStats(&ResourceLocation, false, D3D12_BUFFER_TYPE_VERTEX);
	}
}

void FD3D12VertexBuffer::Rename(FD3D12ResourceLocation& NewResource)
{
	FD3D12ResourceLocation::TransferOwnership(ResourceLocation, NewResource);

	if (DynamicSRV != nullptr)
	{
		// This will force a new descriptor to be created
		CD3DX12_CPU_DESCRIPTOR_HANDLE Desc;
		Desc.ptr = 0;
		DynamicSRV->Rename(ResourceLocation, Desc, 0);
	}
}

FVertexBufferRHIRef FD3D12DynamicRHI::RHICreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	const D3D12_RESOURCE_DESC Desc = CreateVertexBufferResourceDesc(Size, InUsage);
	const uint32 Alignment = 4;

	FD3D12VertexBuffer* Buffer = GetAdapter().CreateRHIBuffer<FD3D12VertexBuffer>(nullptr, Desc, Alignment, 0, Size, InUsage, CreateInfo, false);
	if (Buffer->ResourceLocation.IsTransient() )
	{
		// TODO: this should ideally be set in platform-independent code, since this tracking is for the high level
		Buffer->SetCommitted(false);
	}

	UpdateBufferStats(&Buffer->ResourceLocation, true, D3D12_BUFFER_TYPE_VERTEX);

	return Buffer;
}

void* FD3D12DynamicRHI::RHILockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	return LockBuffer(nullptr, FD3D12DynamicRHI::ResourceCast(VertexBufferRHI), Offset, Size, LockMode);
}

void FD3D12DynamicRHI::RHIUnlockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI)
{
	UnlockBuffer(nullptr, FD3D12DynamicRHI::ResourceCast(VertexBufferRHI));
}

FVertexBufferRHIRef FD3D12DynamicRHI::CreateVertexBuffer_RenderThread(FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{	
	const D3D12_RESOURCE_DESC Desc = CreateVertexBufferResourceDesc(Size, InUsage);
	const uint32 Alignment = 4;

	FD3D12VertexBuffer* Buffer = GetAdapter().CreateRHIBuffer<FD3D12VertexBuffer>(&RHICmdList, Desc, Alignment, 0, Size, InUsage, CreateInfo, false);
	if (Buffer->ResourceLocation.IsTransient())
	{
		// TODO: this should ideally be set in platform-independent code, since this tracking is for the high level
		Buffer->SetCommitted(false);
	}
	UpdateBufferStats(&Buffer->ResourceLocation, true, D3D12_BUFFER_TYPE_VERTEX);

	return Buffer;
}

void* FD3D12DynamicRHI::LockVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	// Pull down the above RHI implementation so that we can flush only when absolutely necessary
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicRHI_LockVertexBuffer_RenderThread);
	check(IsInRenderingThread());

	return LockBuffer(&RHICmdList, FD3D12DynamicRHI::ResourceCast(VertexBufferRHI), Offset, SizeRHI, LockMode);
}

void FD3D12DynamicRHI::UnlockVertexBuffer_RenderThread(FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBufferRHI)
{
	// Pull down the above RHI implementation so that we can flush only when absolutely necessary
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicRHI_UnlockVertexBuffer_RenderThread);
	check(IsInRenderingThread());

	UnlockBuffer(&RHICmdList, FD3D12DynamicRHI::ResourceCast(VertexBufferRHI));
}

void FD3D12DynamicRHI::RHICopyVertexBuffer(FVertexBufferRHIParamRef SourceBufferRHI, FVertexBufferRHIParamRef DestBufferRHI)
{
	FD3D12VertexBuffer*  SourceBuffer = FD3D12DynamicRHI::ResourceCast(SourceBufferRHI);
	FD3D12VertexBuffer*  DestBuffer = FD3D12DynamicRHI::ResourceCast(DestBufferRHI);

	while (SourceBuffer && DestBuffer)
	{
		FD3D12Device* Device = SourceBuffer->GetParentDevice();
		check(Device == DestBuffer->GetParentDevice());

		FD3D12Resource* pSourceResource = SourceBuffer->ResourceLocation.GetResource();
		D3D12_RESOURCE_DESC const& SourceBufferDesc = pSourceResource->GetDesc();

		FD3D12Resource* pDestResource = DestBuffer->ResourceLocation.GetResource();
		D3D12_RESOURCE_DESC const& DestBufferDesc = pDestResource->GetDesc();

		check(SourceBufferDesc.Width == DestBufferDesc.Width);
		check(SourceBuffer->GetSize() == DestBuffer->GetSize());

		FD3D12CommandContext& Context = Device->GetDefaultCommandContext();
		Context.numCopies++;
		Context.CommandListHandle->CopyResource(pDestResource->GetResource(), pSourceResource->GetResource());
		Context.CommandListHandle.UpdateResidency(pDestResource);
		Context.CommandListHandle.UpdateResidency(pSourceResource);

		DEBUG_RHI_EXECUTE_COMMAND_LIST(this);

		Device->RegisterGPUWork(1);

		SourceBuffer = SourceBuffer->GetNextObject();
		DestBuffer = DestBuffer->GetNextObject();
	}
}

FVertexBufferRHIRef FD3D12DynamicRHI::CreateAndLockVertexBuffer_RenderThread(FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
{
	const D3D12_RESOURCE_DESC Desc = CreateVertexBufferResourceDesc(Size, InUsage);
	const uint32 Alignment = 4;

	FD3D12VertexBuffer* Buffer = GetAdapter().CreateRHIBuffer<FD3D12VertexBuffer>(nullptr, Desc, Alignment, 0, Size, InUsage, CreateInfo, false);
	if (Buffer->ResourceLocation.IsTransient())
	{
		// TODO: this should ideally be set in platform-independent code, since this tracking is for the high level
		Buffer->SetCommitted(false);
	}
	OutDataBuffer = LockVertexBuffer_RenderThread(RHICmdList, Buffer, 0, Size, RLM_WriteOnly);

	UpdateBufferStats(&Buffer->ResourceLocation, true, D3D12_BUFFER_TYPE_VERTEX);

	return Buffer;
}
