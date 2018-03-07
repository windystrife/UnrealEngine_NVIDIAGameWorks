// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"

template<typename ResourceType>
inline FD3D12UnorderedAccessView* CreateUAV(D3D12_UNORDERED_ACCESS_VIEW_DESC& Desc, ResourceType* Resource, bool bNeedsCounterResource)
{
	if (Resource == nullptr)
	{
		return nullptr;
	}

	FD3D12Adapter* Adapter = Resource->GetParentDevice()->GetParentAdapter();

	return Adapter->CreateLinkedViews<ResourceType, FD3D12UnorderedAccessView>(Resource, [bNeedsCounterResource, &Desc](ResourceType* Resource)
	{
		FD3D12Device* Device = Resource->GetParentDevice();
		FD3D12Resource* CounterResource = nullptr;

		if (bNeedsCounterResource)
		{
			const GPUNodeMask Node = Device->GetNodeMask();
			Device->GetParentAdapter()->CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, Node, Node, 4, &CounterResource, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		}

		return new FD3D12UnorderedAccessView(Device, &Desc, &Resource->ResourceLocation, CounterResource);
	});
}

FUnorderedAccessViewRHIRef FD3D12DynamicRHI::RHICreateUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBufferRHI, bool bUseUAVCounter, bool bAppendBuffer)
{
	FD3D12StructuredBuffer*  StructuredBuffer = FD3D12DynamicRHI::ResourceCast(StructuredBufferRHI);

	FD3D12ResourceLocation& Location = StructuredBuffer->ResourceLocation;

	const D3D12_RESOURCE_DESC& BufferDesc = Location.GetResource()->GetDesc();

	const uint32 BufferUsage = StructuredBuffer->GetUsage();
	const bool bByteAccessBuffer = (BufferUsage & BUF_ByteAddressBuffer) != 0;
	const bool bStructuredBuffer = !bByteAccessBuffer;
	check(bByteAccessBuffer != bStructuredBuffer); // You can't have a structured buffer that allows raw views

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	UAVDesc.Format = DXGI_FORMAT_UNKNOWN;

	uint32 EffectiveStride = StructuredBuffer->GetStride();

	if (bByteAccessBuffer)
	{
		UAVDesc.Format  = DXGI_FORMAT_R32_TYPELESS;
		EffectiveStride = 4;
	}

	else if (BufferUsage & BUF_DrawIndirect)
	{
		UAVDesc.Format  = DXGI_FORMAT_R32_UINT;
		EffectiveStride = 4;
	}
	UAVDesc.Buffer.FirstElement = Location.GetOffsetFromBaseOfResource() / EffectiveStride;
	UAVDesc.Buffer.NumElements  = Location.GetSize() / EffectiveStride;
	UAVDesc.Buffer.StructureByteStride = bStructuredBuffer ? EffectiveStride : 0;
	UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	UAVDesc.Buffer.CounterOffsetInBytes = 0;

	const bool bNeedsCounterResource = bAppendBuffer | bUseUAVCounter;

	if (bByteAccessBuffer)
	{
		UAVDesc.Buffer.Flags |= D3D12_BUFFER_UAV_FLAG_RAW;
	}

	return CreateUAV(UAVDesc, StructuredBuffer, bNeedsCounterResource);
}

FUnorderedAccessViewRHIRef FD3D12DynamicRHI::RHICreateUnorderedAccessView(FTextureRHIParamRef TextureRHI, uint32 MipLevel)
{
	FD3D12TextureBase* Texture = GetD3D12TextureFromRHITexture(TextureRHI);

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};

	const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[TextureRHI->GetFormat()].PlatformFormat;
	UAVDesc.Format = FindShaderResourceDXGIFormat(PlatformResourceFormat, false);

	if (TextureRHI->GetTexture3D() != NULL)
	{
		FD3D12Texture3D* Texture3D = (FD3D12Texture3D*)Texture;
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		UAVDesc.Texture3D.MipSlice = MipLevel;
		UAVDesc.Texture3D.FirstWSlice = 0;
		UAVDesc.Texture3D.WSize = Texture3D->GetSizeZ() >> MipLevel;

		return CreateUAV(UAVDesc, Texture3D, false);
	}
	else if (TextureRHI->GetTexture2DArray() != NULL)
	{
		FD3D12Texture2DArray* Texture2DArray = (FD3D12Texture2DArray*)Texture;
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		UAVDesc.Texture2DArray.MipSlice = MipLevel;
		UAVDesc.Texture2DArray.FirstArraySlice = 0;
		UAVDesc.Texture2DArray.ArraySize = Texture2DArray->GetSizeZ();
		UAVDesc.Texture2DArray.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, UAVDesc.Format);

		return CreateUAV(UAVDesc, Texture2DArray, false);
	}
	else if (TextureRHI->GetTextureCube() != NULL)
	{
		FD3D12TextureCube* TextureCube = (FD3D12TextureCube*)Texture;
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		UAVDesc.Texture2DArray.MipSlice = MipLevel;
		UAVDesc.Texture2DArray.FirstArraySlice = 0;
		UAVDesc.Texture2DArray.ArraySize = 6;
		UAVDesc.Texture2DArray.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, UAVDesc.Format);

		return CreateUAV(UAVDesc, TextureCube, false);
	}
	else
	{
		FD3D12Texture2D* Texture2D = (FD3D12Texture2D*)Texture;
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		UAVDesc.Texture2D.MipSlice = MipLevel;
		UAVDesc.Texture2D.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, UAVDesc.Format);

		return CreateUAV(UAVDesc, Texture2D, false);
	}
}

FUnorderedAccessViewRHIRef FD3D12DynamicRHI::RHICreateUnorderedAccessView(FVertexBufferRHIParamRef VertexBufferRHI, uint8 Format)
{
	FD3D12VertexBuffer*  VertexBuffer = FD3D12DynamicRHI::ResourceCast(VertexBufferRHI);
	FD3D12ResourceLocation& Location = VertexBuffer->ResourceLocation;

	const D3D12_RESOURCE_DESC& BufferDesc = Location.GetResource()->GetDesc();
	const uint64 effectiveBufferSize = Location.GetSize();

	const uint32 BufferUsage = VertexBuffer->GetUsage();
	const bool bByteAccessBuffer = (BufferUsage & BUF_ByteAddressBuffer) != 0;

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	UAVDesc.Format = FindUnorderedAccessDXGIFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat);
	UAVDesc.Buffer.FirstElement = Location.GetOffsetFromBaseOfResource();

	UAVDesc.Buffer.NumElements = effectiveBufferSize / GPixelFormats[Format].BlockBytes;
	UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	UAVDesc.Buffer.CounterOffsetInBytes = 0;
	UAVDesc.Buffer.StructureByteStride = 0;

	if (bByteAccessBuffer)
	{
		UAVDesc.Buffer.Flags |= D3D12_BUFFER_UAV_FLAG_RAW;
		UAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		UAVDesc.Buffer.NumElements = effectiveBufferSize / 4;
		UAVDesc.Buffer.FirstElement /= 4;
	}

	else
	{
		UAVDesc.Buffer.NumElements = effectiveBufferSize / GPixelFormats[Format].BlockBytes;
		UAVDesc.Buffer.FirstElement /= GPixelFormats[Format].BlockBytes;
	}

	return CreateUAV(UAVDesc, VertexBuffer, false);
}

void FD3D12CommandContext::RHIClearTinyUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values)
{
	FD3D12UnorderedAccessView*  UnorderedAccessView = FD3D12DynamicRHI::ResourceCast(UnorderedAccessViewRHI);

	// Check if the view heap is full and needs to rollover.
	if (!StateCache.GetDescriptorCache()->GetCurrentViewHeap()->CanReserveSlots(1))
	{
		StateCache.GetDescriptorCache()->GetCurrentViewHeap()->RollOver();
	}
	uint32 ReservedSlot = StateCache.GetDescriptorCache()->GetCurrentViewHeap()->ReserveSlots(1);
	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = UnorderedAccessView->GetView();
	D3D12_CPU_DESCRIPTOR_HANDLE DestSlot = StateCache.GetDescriptorCache()->GetCurrentViewHeap()->GetCPUSlotHandle(ReservedSlot);
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle = StateCache.GetDescriptorCache()->GetCurrentViewHeap()->GetGPUSlotHandle(ReservedSlot);
	GetParentDevice()->GetDevice()->CopyDescriptorsSimple(1, DestSlot, CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	FD3D12DynamicRHI::TransitionResource(CommandListHandle, UnorderedAccessView, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	numClears++;

	CommandListHandle.FlushResourceBarriers();
	CommandListHandle->ClearUnorderedAccessViewUint(GPUHandle, CPUHandle, UnorderedAccessView->GetResource()->GetResource(), Values, 0, nullptr);
	CommandListHandle.UpdateResidency(UnorderedAccessView->GetResource());

	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUWork(1);
	}
}

FUnorderedAccessViewRHIRef FD3D12DynamicRHI::RHICreateUnorderedAccessView_RenderThread(FRHICommandListImmediate& RHICmdList, FStructuredBufferRHIParamRef StructuredBufferRHI, bool bUseUAVCounter, bool bAppendBuffer)
{
	FD3D12StructuredBuffer* StructuredBuffer = FD3D12DynamicRHI::ResourceCast(StructuredBufferRHI);
	// TODO: we have to stall the RHI thread when creating SRVs of dynamic buffers because they get renamed.
	// perhaps we could do a deferred operation?
	if (StructuredBuffer->GetUsage() & BUF_AnyDynamic)
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return RHICreateUnorderedAccessView(StructuredBufferRHI, bUseUAVCounter, bAppendBuffer);
	}
	return RHICreateUnorderedAccessView(StructuredBufferRHI, bUseUAVCounter, bAppendBuffer);
}

FUnorderedAccessViewRHIRef FD3D12DynamicRHI::RHICreateUnorderedAccessView_RenderThread(FRHICommandListImmediate& RHICmdList, FTextureRHIParamRef Texture, uint32 MipLevel)
{
	return FD3D12DynamicRHI::RHICreateUnorderedAccessView(Texture, MipLevel);
}

FUnorderedAccessViewRHIRef FD3D12DynamicRHI::RHICreateUnorderedAccessView_RenderThread(FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBufferRHI, uint8 Format)
{
	FD3D12VertexBuffer* VertexBuffer = FD3D12DynamicRHI::ResourceCast(VertexBufferRHI);

	// TODO: we have to stall the RHI thread when creating SRVs of dynamic buffers because they get renamed.
	// perhaps we could do a deferred operation?
	if (VertexBuffer->GetUsage() & BUF_AnyDynamic)
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return RHICreateUnorderedAccessView(VertexBufferRHI, Format);
	}
	return RHICreateUnorderedAccessView(VertexBufferRHI, Format);
}