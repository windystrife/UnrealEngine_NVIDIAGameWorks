// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12View.cpp: 
=============================================================================*/

#include "D3D12RHIPrivate.h"

template<typename TextureType>
FD3D12ShaderResourceView* CreateSRV(TextureType* Texture, D3D12_SHADER_RESOURCE_VIEW_DESC& Desc)
{
	if (Texture == nullptr)
	{
		return nullptr;
	}

	FD3D12Adapter* Adapter = Texture->GetParentDevice()->GetParentAdapter();

	return Adapter->CreateLinkedViews<TextureType, FD3D12ShaderResourceView>(Texture, [&Desc](TextureType* Texture)
	{
		return FD3D12ShaderResourceView::CreateShaderResourceView(Texture->GetParentDevice(), &Texture->ResourceLocation, Desc);
	});
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	FD3D12Texture2D*  Texture2D = FD3D12DynamicRHI::ResourceCast(Texture2DRHI);

	uint64 Offset = 0;
	const D3D12_RESOURCE_DESC& TextureDesc = Texture2D->GetResource()->GetDesc();

	const bool bSRGB = (Texture2D->GetFlags() & TexCreate_SRGB) != 0;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(TextureDesc.Format, bSRGB);

	// Create a Shader Resource View
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;
	SRVDesc.Texture2D.MostDetailedMip = MipLevel;

	SRVDesc.Format = PlatformShaderResourceFormat;

	SRVDesc.Texture2D.PlaneSlice = GetPlaneSliceFromViewFormat(TextureDesc.Format, SRVDesc.Format);

	return CreateSRV(Texture2D, SRVDesc);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView(FTexture2DArrayRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	FD3D12Texture2DArray*  Texture2DArray = FD3D12DynamicRHI::ResourceCast(Texture2DRHI);

	uint64 Offset = 0;
	const D3D12_RESOURCE_DESC& TextureDesc = Texture2DArray->GetResource()->GetDesc();

	const bool bSRGB = (Texture2DArray->GetFlags() & TexCreate_SRGB) != 0;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(TextureDesc.Format, bSRGB);

	// Create a Shader Resource View
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	SRVDesc.Texture2DArray.ArraySize = TextureDesc.DepthOrArraySize;
	SRVDesc.Texture2DArray.FirstArraySlice = 0;
	SRVDesc.Texture2DArray.MipLevels = 1;
	SRVDesc.Texture2DArray.MostDetailedMip = MipLevel;

	SRVDesc.Format = PlatformShaderResourceFormat;

	SRVDesc.Texture2DArray.PlaneSlice = GetPlaneSliceFromViewFormat(TextureDesc.Format, SRVDesc.Format);

	return CreateSRV(Texture2DArray, SRVDesc);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView(FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel)
{
	FD3D12TextureCube*  TextureCube = FD3D12DynamicRHI::ResourceCast(TextureCubeRHI);

	uint64 Offset = 0;
	const D3D12_RESOURCE_DESC& TextureDesc = TextureCube->GetResource()->GetDesc();

	const bool bSRGB = (TextureCube->GetFlags() & TexCreate_SRGB) != 0;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(TextureDesc.Format, bSRGB);

	// Create a Shader Resource View
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	SRVDesc.TextureCube.MipLevels = 1;
	SRVDesc.TextureCube.MostDetailedMip = MipLevel;

	SRVDesc.Format = PlatformShaderResourceFormat;

	return CreateSRV(TextureCube, SRVDesc);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	FD3D12Texture2D*  Texture2D = FD3D12DynamicRHI::ResourceCast(Texture2DRHI);
	uint64 Offset = 0;
	const D3D12_RESOURCE_DESC& TextureDesc = Texture2D->GetResource()->GetDesc();

	const DXGI_FORMAT PlatformResourceFormat = GetPlatformTextureResourceFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat, Texture2D->GetFlags());

	const bool bSRGB = (Texture2D->GetFlags() & TexCreate_SRGB) != 0;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);

	// Create a Shader Resource View
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (TextureDesc.SampleDesc.Count > 1)
	{
		// MS textures can't have mips apparently, so nothing else to set.
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
	}
	else
	{
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MostDetailedMip = MipLevel;
		SRVDesc.Texture2D.MipLevels = NumMipLevels;
		SRVDesc.Texture2D.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, PlatformShaderResourceFormat);
	}

	SRVDesc.Format = PlatformShaderResourceFormat;

	return CreateSRV(Texture2D, SRVDesc);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView(FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel)
{
	FD3D12Texture3D*  Texture3D = FD3D12DynamicRHI::ResourceCast(Texture3DRHI);

	uint64 Offset = 0;
	const D3D12_RESOURCE_DESC& TextureDesc = Texture3D->GetResource()->GetDesc();

	const bool bSRGB = (Texture3D->GetFlags() & TexCreate_SRGB) != 0;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(TextureDesc.Format, bSRGB);

	// Create a Shader Resource View
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
	SRVDesc.Texture3D.MipLevels = 1;
	SRVDesc.Texture3D.MostDetailedMip = MipLevel;

	SRVDesc.Format = PlatformShaderResourceFormat;

	return CreateSRV(Texture3D, SRVDesc);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	FD3D12StructuredBuffer*  StructuredBuffer = FD3D12DynamicRHI::ResourceCast(StructuredBufferRHI);

	return GetAdapter().CreateLinkedViews<FD3D12StructuredBuffer,
		FD3D12ShaderResourceView>(StructuredBuffer, [](FD3D12StructuredBuffer* StructuredBuffer)
	{
		check(StructuredBuffer);

		FD3D12ResourceLocation& Location = StructuredBuffer->ResourceLocation;

		const uint64 Offset = Location.GetOffsetFromBaseOfResource();
		const D3D12_RESOURCE_DESC& BufferDesc = Location.GetResource()->GetDesc();

		const uint32 BufferUsage = StructuredBuffer->GetUsage();
		const bool bByteAccessBuffer = (BufferUsage & BUF_ByteAddressBuffer) != 0;
		const bool bUINT8Access = (BufferUsage & BUF_UINT8) != 0;
		// Create a Shader Resource View
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		// BufferDesc.StructureByteStride  is not getting patched through the D3D resource DESC structs, so use the RHI version as a hack
		uint32 Stride = StructuredBuffer->GetStride();

		if (bByteAccessBuffer)
		{
			SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
			SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			Stride = 4;
		}
		else if (bUINT8Access)
		{
			SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			SRVDesc.Format = DXGI_FORMAT_R8_UINT;
			Stride = 1;
			SRVDesc.Buffer.StructureByteStride = Stride;
		}
		else
		{
			SRVDesc.Buffer.StructureByteStride = Stride;
			SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		}

		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		SRVDesc.Buffer.NumElements = Location.GetSize() / Stride;
		SRVDesc.Buffer.FirstElement = Offset / Stride;

		return new FD3D12ShaderResourceView(StructuredBuffer->GetParentDevice(), &SRVDesc, &Location, Stride);
	});
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Stride, uint8 Format)
{
	FD3D12VertexBuffer*  VertexBuffer = FD3D12DynamicRHI::ResourceCast(VertexBufferRHI);
	return GetAdapter().CreateLinkedViews<FD3D12VertexBuffer, FD3D12ShaderResourceView>(VertexBuffer,
	[Stride, Format](FD3D12VertexBuffer* VertexBuffer)
	{
		check(VertexBuffer);

		FD3D12ResourceLocation& Location = VertexBuffer->ResourceLocation;

		const uint32 Width = VertexBuffer->GetSize();

		FD3D12Resource* pResource = Location.GetResource();

		uint32 CreationStride = Stride;

		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		if (VertexBuffer->GetUsage() & BUF_ByteAddressBuffer)
		{
			SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			SRVDesc.Buffer.NumElements = Width / 4;
			SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
			CreationStride = 4;
		}
		else
		{
			SRVDesc.Format = FindShaderResourceDXGIFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat, false);
			SRVDesc.Buffer.NumElements = Width / Stride;
		}
		SRVDesc.Buffer.StructureByteStride = 0;

		if (pResource)
		{
			// Create a Shader Resource View
			SRVDesc.Buffer.FirstElement = Location.GetOffsetFromBaseOfResource() / CreationStride;
		}
		else
		{
			// Null underlying D3D12 resource should only be the case for dynamic resources
			check(VertexBuffer->GetUsage() & BUF_AnyDynamic);
		}

		FD3D12ShaderResourceView* ShaderResourceView = new FD3D12ShaderResourceView(VertexBuffer->GetParentDevice(), &SRVDesc, &Location, CreationStride);
		VertexBuffer->SetDynamicSRV(ShaderResourceView);
		return ShaderResourceView;
	});
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView(FIndexBufferRHIParamRef BufferRHI)
{
	FD3D12IndexBuffer* IndexBuffer = FD3D12DynamicRHI::ResourceCast(BufferRHI);
	return GetAdapter().CreateLinkedViews<FD3D12IndexBuffer, FD3D12ShaderResourceView>(IndexBuffer,
		[](FD3D12IndexBuffer* IndexBuffer)
	{
		check(IndexBuffer);

		FD3D12ResourceLocation& Location = IndexBuffer->ResourceLocation;

		const uint32 Width = IndexBuffer->GetSize();

		FD3D12Resource* pResource = Location.GetResource();

		uint32 CreationStride = IndexBuffer->GetStride();

		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		if (IndexBuffer->GetUsage() & BUF_ByteAddressBuffer)
		{
			check(CreationStride == 4u);
			SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			SRVDesc.Buffer.NumElements = Width / 4;
			SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		}
		else
		{
			check(CreationStride == 2u || CreationStride == 4u);
			SRVDesc.Format = CreationStride == 2u ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
			SRVDesc.Buffer.NumElements = Width / CreationStride;
		}
		SRVDesc.Buffer.StructureByteStride = 0;

		if (pResource)
		{
			// Create a Shader Resource View
			SRVDesc.Buffer.FirstElement = Location.GetOffsetFromBaseOfResource() / CreationStride;
		}
		else
		{
			// Null underlying D3D12 resource should only be the case for dynamic resources
			check(IndexBuffer->GetUsage() & BUF_AnyDynamic);
		}

		FD3D12ShaderResourceView* ShaderResourceView = new FD3D12ShaderResourceView(IndexBuffer->GetParentDevice(), &SRVDesc, &Location, CreationStride);
		return ShaderResourceView;
	});
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	return RHICreateShaderResourceView(Texture2DRHI, MipLevel);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	return RHICreateShaderResourceView(Texture2DRHI, MipLevel, NumMipLevels, Format);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel)
{
	return RHICreateShaderResourceView(Texture3DRHI, MipLevel);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel)
{
	return RHICreateShaderResourceView(Texture2DArrayRHI, MipLevel);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView_RenderThread(FRHICommandListImmediate& RHICmdList, FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel)
{
	return RHICreateShaderResourceView(TextureCubeRHI, MipLevel);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView_RenderThread(FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBufferRHI, uint32 Stride, uint8 Format)
{
	FD3D12VertexBuffer*  VertexBuffer = FD3D12DynamicRHI::ResourceCast(VertexBufferRHI);

	// TODO: we have to stall the RHI thread when creating SRVs of dynamic buffers because they get renamed.
	// perhaps we could do a deferred operation?
	if (VertexBuffer->GetUsage() & BUF_AnyDynamic)
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return RHICreateShaderResourceView(VertexBufferRHI, Stride, Format);
	}
	return RHICreateShaderResourceView(VertexBufferRHI, Stride, Format);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::CreateShaderResourceView_RenderThread(FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBufferRHI, uint32 Stride, uint8 Format)
{
	return RHICreateShaderResourceView_RenderThread(RHICmdList, VertexBufferRHI, Stride, Format);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView_RenderThread(FRHICommandListImmediate& RHICmdList, FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	FD3D12StructuredBuffer*  StructuredBuffer = FD3D12DynamicRHI::ResourceCast(StructuredBufferRHI);

	// TODO: we have to stall the RHI thread when creating SRVs of dynamic buffers because they get renamed.
	// perhaps we could do a deferred operation?
	if (StructuredBuffer->GetUsage() & BUF_AnyDynamic)
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return RHICreateShaderResourceView(StructuredBufferRHI);
	}
	return RHICreateShaderResourceView(StructuredBufferRHI);
}

FD3D12ShaderResourceView* FD3D12ShaderResourceView::CreateShaderResourceView(FD3D12Device* Parent,  FD3D12ResourceLocation* ResourceLocation, D3D12_SHADER_RESOURCE_VIEW_DESC& Desc)
{
	return new FD3D12ShaderResourceView(Parent, &Desc, ResourceLocation);
}

FD3D12RenderTargetView* FD3D12RenderTargetView::CreateRenderTargetView(FD3D12Device* Parent, FD3D12ResourceLocation* ResourceLocation, D3D12_RENDER_TARGET_VIEW_DESC& Desc)
{
	return new FD3D12RenderTargetView(Parent, &Desc, ResourceLocation);
}

FD3D12DepthStencilView* FD3D12DepthStencilView::CreateDepthStencilView(FD3D12Device* Parent, FD3D12ResourceLocation* ResourceLocation, D3D12_DEPTH_STENCIL_VIEW_DESC& Desc, bool HasStencil)
{
	return new FD3D12DepthStencilView(Parent, &Desc, ResourceLocation, HasStencil);
}

#if USE_STATIC_ROOT_SIGNATURE
void FD3D12ConstantBufferView::AllocateHeapSlot()
{
	if (!OfflineDescriptorHandle.ptr)
	{
	    FD3D12OfflineDescriptorManager& DescriptorAllocator = GetParentDevice()->GetViewDescriptorAllocator<D3D12_CONSTANT_BUFFER_VIEW_DESC>();
	    OfflineDescriptorHandle = DescriptorAllocator.AllocateHeapSlot(OfflineHeapIndex);
	    check(OfflineDescriptorHandle.ptr != 0);
	}
}

void FD3D12ConstantBufferView::FreeHeapSlot()
{
	if (OfflineDescriptorHandle.ptr)
	{
		FD3D12OfflineDescriptorManager& DescriptorAllocator = GetParentDevice()->GetViewDescriptorAllocator<D3D12_CONSTANT_BUFFER_VIEW_DESC>();
		DescriptorAllocator.FreeHeapSlot(OfflineDescriptorHandle, OfflineHeapIndex);
		OfflineDescriptorHandle.ptr = 0;
	}
}

void FD3D12ConstantBufferView::Create(D3D12_GPU_VIRTUAL_ADDRESS GPUAddress, const uint32 AlignedSize)
{
	Desc.BufferLocation = GPUAddress;
	Desc.SizeInBytes = AlignedSize;
	GetParentDevice()->GetDevice()->CreateConstantBufferView(&Desc, OfflineDescriptorHandle);
}
#endif
