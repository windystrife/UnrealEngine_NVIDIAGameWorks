// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "MetalRHIPrivate.h"
#include "MetalCommandBuffer.h"
#include "RenderUtils.h"

FMetalShaderResourceView::FMetalShaderResourceView()
: TextureView(nullptr)
, MipLevel(0)
, NumMips(0)
, Format(0)
, Stride(0)
{
	
}

FMetalShaderResourceView::~FMetalShaderResourceView()
{
	if(TextureView)
	{
		FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(SourceTexture);
		if (Surface)
		{
			Surface->SRVs.Remove(this);
		}
		
		if(TextureView->Texture)
		{
			[TextureView->Texture release];
			TextureView->Texture = nil;
			
			[TextureView->StencilTexture release];
			TextureView->StencilTexture = nil;
			
			[TextureView->MSAATexture release];
			TextureView->MSAATexture = nil;
		}
		delete TextureView;
		TextureView = nullptr;
	}
	
	SourceVertexBuffer = NULL;
	SourceTexture = NULL;
}

id<MTLTexture> FMetalShaderResourceView::GetLinearTexture(bool const bUAV)
{
	id<MTLTexture> NewLinearTexture = nil;
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesLinearTextures) && (!bUAV || FMetalCommandQueue::SupportsFeature(EMetalFeaturesLinearTextureUAVs)))
	{
		if (IsValidRef(SourceVertexBuffer))
		{
			NewLinearTexture = SourceVertexBuffer->GetLinearTexture((EPixelFormat)Format);
			check(NewLinearTexture);
		}
		else if (IsValidRef(SourceIndexBuffer))
		{
			NewLinearTexture = SourceIndexBuffer->LinearTexture;
			check(NewLinearTexture);
		}
	}
	return NewLinearTexture;
}

FUnorderedAccessViewRHIRef FMetalDynamicRHI::RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FStructuredBufferRHIParamRef StructuredBuffer, bool bUseUAVCounter, bool bAppendBuffer)
{
	return GDynamicRHI->RHICreateUnorderedAccessView(StructuredBuffer, bUseUAVCounter, bAppendBuffer);
}

FUnorderedAccessViewRHIRef FMetalDynamicRHI::RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTextureRHIParamRef Texture, uint32 MipLevel)
{
	FMetalSurface* Surface = (FMetalSurface*)Texture->GetTextureBaseRHI();
	id<MTLTexture> Tex = Surface->Texture;
	if (ImmediateContext.Context->GetCommandQueue().SupportsFeature(EMetalFeaturesResourceOptions) && !(Tex.usage & MTLTextureUsagePixelFormatView))
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return GDynamicRHI->RHICreateUnorderedAccessView(Texture, MipLevel);
	}
	else
	{
		return GDynamicRHI->RHICreateUnorderedAccessView(Texture, MipLevel);
	}
}

FUnorderedAccessViewRHIRef FMetalDynamicRHI::RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint8 Format)
{
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesLinearTextureUAVs))
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return GDynamicRHI->RHICreateUnorderedAccessView(VertexBuffer, Format);
	}
	return GDynamicRHI->RHICreateUnorderedAccessView(VertexBuffer, Format);
}

FUnorderedAccessViewRHIRef FMetalDynamicRHI::RHICreateUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBufferRHI, bool bUseUAVCounter, bool bAppendBuffer)
{
	@autoreleasepool {
	FMetalStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);
	
	FMetalShaderResourceView* SRV = new FMetalShaderResourceView;
	SRV->SourceVertexBuffer = nullptr;
	SRV->SourceIndexBuffer = nullptr;
	SRV->TextureView = nullptr;
	SRV->SourceStructuredBuffer = StructuredBuffer;

	// create the UAV buffer to point to the structured buffer's memory
	FMetalUnorderedAccessView* UAV = new FMetalUnorderedAccessView;
	UAV->SourceView = SRV;
	return UAV;
	}
}

FUnorderedAccessViewRHIRef FMetalDynamicRHI::RHICreateUnorderedAccessView(FTextureRHIParamRef TextureRHI, uint32 MipLevel)
{
	@autoreleasepool {
	FMetalShaderResourceView* SRV = new FMetalShaderResourceView;
	SRV->SourceTexture = TextureRHI;
	
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);
	SRV->TextureView = Surface ? new FMetalSurface(*Surface, NSMakeRange(MipLevel, 1)) : nullptr;
	
	SRV->SourceVertexBuffer = nullptr;
	SRV->SourceIndexBuffer = nullptr;
	SRV->SourceStructuredBuffer = nullptr;
	
	SRV->MipLevel = MipLevel;
	SRV->NumMips = 1;
	SRV->Format = PF_Unknown;
		
	if (Surface)
	{
		Surface->SRVs.Add(SRV);
	}
		
	// create the UAV buffer to point to the structured buffer's memory
	FMetalUnorderedAccessView* UAV = new FMetalUnorderedAccessView;
	UAV->SourceView = SRV;

	return UAV;
	}
}

FUnorderedAccessViewRHIRef FMetalDynamicRHI::RHICreateUnorderedAccessView(FVertexBufferRHIParamRef VertexBufferRHI, uint8 Format)
{
	@autoreleasepool {
	FMetalVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	
	FMetalShaderResourceView* SRV = new FMetalShaderResourceView;
	SRV->SourceVertexBuffer = VertexBuffer;
	SRV->TextureView = nullptr;
	SRV->SourceIndexBuffer = nullptr;
	SRV->SourceStructuredBuffer = nullptr;
	SRV->Format = Format;
		
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesLinearTextureUAVs))
	{
		check(VertexBuffer->GetUsage() & BUF_UnorderedAccess);
		
		id<MTLTexture> Texture = VertexBuffer->GetLinearTexture((EPixelFormat)Format);
		check(Texture);
	}
		
	// create the UAV buffer to point to the structured buffer's memory
	FMetalUnorderedAccessView* UAV = new FMetalUnorderedAccessView;
	UAV->SourceView = SRV;

	return UAV;
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	FMetalTexture2D* Texture = ResourceCast(Texture2DRHI);
	id<MTLTexture> Tex = Texture->Surface.Texture;
	if (ImmediateContext.Context->GetCommandQueue().SupportsFeature(EMetalFeaturesResourceOptions) && !(Tex.usage & MTLTextureUsagePixelFormatView))
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return GDynamicRHI->RHICreateShaderResourceView(Texture2DRHI, MipLevel);
	}
	else
	{
		return GDynamicRHI->RHICreateShaderResourceView(Texture2DRHI, MipLevel);
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	FMetalTexture2D* Texture = ResourceCast(Texture2DRHI);
	id<MTLTexture> Tex = Texture->Surface.Texture;
	if (ImmediateContext.Context->GetCommandQueue().SupportsFeature(EMetalFeaturesResourceOptions) && !(Tex.usage & MTLTextureUsagePixelFormatView))
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return GDynamicRHI->RHICreateShaderResourceView(Texture2DRHI, MipLevel, NumMipLevels, Format);
	}
	else
	{
		return GDynamicRHI->RHICreateShaderResourceView(Texture2DRHI, MipLevel, NumMipLevels, Format);
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel)
{
	FMetalTexture3D* Texture = ResourceCast(Texture3DRHI);
	id<MTLTexture> Tex = Texture->Surface.Texture;
	if (ImmediateContext.Context->GetCommandQueue().SupportsFeature(EMetalFeaturesResourceOptions) && !(Tex.usage & MTLTextureUsagePixelFormatView))
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return GDynamicRHI->RHICreateShaderResourceView(Texture3DRHI, MipLevel);
	}
	else
	{
		return GDynamicRHI->RHICreateShaderResourceView(Texture3DRHI, MipLevel);
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel)
{
	FMetalTexture2DArray* Texture = ResourceCast(Texture2DArrayRHI);
	id<MTLTexture> Tex = Texture->Surface.Texture;
	if (ImmediateContext.Context->GetCommandQueue().SupportsFeature(EMetalFeaturesResourceOptions) && !(Tex.usage & MTLTextureUsagePixelFormatView))
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return GDynamicRHI->RHICreateShaderResourceView(Texture2DArrayRHI, MipLevel);
	}
	else
	{
		return GDynamicRHI->RHICreateShaderResourceView(Texture2DArrayRHI, MipLevel);
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel)
{
	FMetalTextureCube* Texture = ResourceCast(TextureCubeRHI);
	id<MTLTexture> Tex = Texture->Surface.Texture;
	if (ImmediateContext.Context->GetCommandQueue().SupportsFeature(EMetalFeaturesResourceOptions) && !(Tex.usage & MTLTextureUsagePixelFormatView))
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return GDynamicRHI->RHICreateShaderResourceView(TextureCubeRHI, MipLevel);
	}
	else
	{
		return GDynamicRHI->RHICreateShaderResourceView(TextureCubeRHI, MipLevel);
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::CreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format)
{
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesLinearTextures))
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return GDynamicRHI->RHICreateShaderResourceView(VertexBuffer, Stride, Format);
	}
	return GDynamicRHI->RHICreateShaderResourceView(VertexBuffer, Stride, Format);
}

FShaderResourceViewRHIRef FMetalDynamicRHI::CreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef Buffer)
{
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesLinearTextures))
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return GDynamicRHI->RHICreateShaderResourceView(Buffer);
	}
	return GDynamicRHI->RHICreateShaderResourceView(Buffer);
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format)
{
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesLinearTextures))
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return GDynamicRHI->RHICreateShaderResourceView(VertexBuffer, Stride, Format);
	}
	return GDynamicRHI->RHICreateShaderResourceView(VertexBuffer, Stride, Format);
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef Buffer)
{
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesLinearTextures))
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return GDynamicRHI->RHICreateShaderResourceView(Buffer);
	}
	return GDynamicRHI->RHICreateShaderResourceView(Buffer);
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FStructuredBufferRHIParamRef StructuredBuffer)
{
	return GDynamicRHI->RHICreateShaderResourceView(StructuredBuffer);
}


FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	FMetalStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	FMetalShaderResourceView* SRV = new FMetalShaderResourceView;
	SRV->SourceVertexBuffer = nullptr;
	SRV->SourceIndexBuffer = nullptr;
	SRV->TextureView = nullptr;
	SRV->SourceStructuredBuffer = StructuredBuffer;
	
	return SRV;
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Stride, uint8 Format)
{
	@autoreleasepool {
	FMetalVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
		
	FMetalShaderResourceView* SRV = new FMetalShaderResourceView;
	SRV->SourceVertexBuffer = VertexBuffer;
	SRV->TextureView = nullptr;
	SRV->SourceIndexBuffer = nullptr;
	SRV->SourceStructuredBuffer = nullptr;
	SRV->Format = Format;
	SRV->Stride = Stride;
	
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesLinearTextures))
	{
		check(Stride == GPixelFormats[Format].BlockBytes);
		check(VertexBuffer->GetUsage() & BUF_ShaderResource);
		
		id<MTLTexture> Texture = VertexBuffer->GetLinearTexture((EPixelFormat)Format);
		check(Texture);
	}
	
	return SRV;
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView(FIndexBufferRHIParamRef BufferRHI)
{
	@autoreleasepool {
	FMetalIndexBuffer* Buffer = ResourceCast(BufferRHI);
		
	FMetalShaderResourceView* SRV = new FMetalShaderResourceView;
	SRV->SourceVertexBuffer = nullptr;
	SRV->SourceIndexBuffer = Buffer;
	SRV->TextureView = nullptr;
	SRV->SourceStructuredBuffer = nullptr;
	SRV->Format = (Buffer->IndexType == MTLIndexTypeUInt16) ? PF_R16_UINT : PF_R32_UINT;
	
	check(!FMetalCommandQueue::SupportsFeature(EMetalFeaturesLinearTextures) || Buffer->LinearTexture);
	
	return SRV;
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	@autoreleasepool {
    FMetalShaderResourceView* SRV = new FMetalShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)Texture2DRHI;
	
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(Texture2DRHI);
	SRV->TextureView = Surface ? new FMetalSurface(*Surface, NSMakeRange(MipLevel, 1)) : nullptr;
	
	SRV->SourceVertexBuffer = nullptr;
	SRV->SourceIndexBuffer = nullptr;
	SRV->SourceStructuredBuffer = nullptr;
	
	SRV->MipLevel = MipLevel;
	SRV->NumMips = 1;
	SRV->Format = PF_Unknown;
		
	if (Surface)
	{
		Surface->SRVs.Add(SRV);
	}
	
	return SRV;
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	@autoreleasepool {
	FMetalShaderResourceView* SRV = new FMetalShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)Texture2DRHI;
	
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(Texture2DRHI);
	SRV->TextureView = Surface ? new FMetalSurface(*Surface, NSMakeRange(MipLevel, NumMipLevels), (EPixelFormat)Format) : nullptr;
		
	SRV->SourceVertexBuffer = nullptr;
	SRV->SourceIndexBuffer = nullptr;
	SRV->SourceStructuredBuffer = nullptr;
	
	SRV->MipLevel = MipLevel;
	SRV->NumMips = NumMipLevels;
	SRV->Format = Format;
		
	if (Surface)
	{
		Surface->SRVs.Add(SRV);
	}
	
	return SRV;
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView(FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel)
{
	@autoreleasepool {
	FMetalShaderResourceView* SRV = new FMetalShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)Texture3DRHI;
	
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(Texture3DRHI);
	SRV->TextureView = Surface ? new FMetalSurface(*Surface, NSMakeRange(MipLevel, 1)) : nullptr;
	
	SRV->SourceVertexBuffer = nullptr;
	SRV->SourceIndexBuffer = nullptr;
	SRV->SourceStructuredBuffer = nullptr;
	
	SRV->MipLevel = MipLevel;
	SRV->NumMips = 1;
	SRV->Format = PF_Unknown;
		
	if (Surface)
	{
		Surface->SRVs.Add(SRV);
	}
	
	return SRV;
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView(FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel)
{
	@autoreleasepool {
	FMetalShaderResourceView* SRV = new FMetalShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)Texture2DArrayRHI;
	
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(Texture2DArrayRHI);
	SRV->TextureView = Surface ? new FMetalSurface(*Surface, NSMakeRange(MipLevel, 1)) : nullptr;
	
	SRV->SourceVertexBuffer = nullptr;
	SRV->SourceIndexBuffer = nullptr;
	SRV->SourceStructuredBuffer = nullptr;
	
	SRV->MipLevel = MipLevel;
	SRV->NumMips = 1;
	SRV->Format = PF_Unknown;
		
	if (Surface)
	{
		Surface->SRVs.Add(SRV);
	}
	
	return SRV;
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView(FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel)
{
	@autoreleasepool {
	FMetalShaderResourceView* SRV = new FMetalShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)TextureCubeRHI;
	
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureCubeRHI);
	SRV->TextureView = Surface ? new FMetalSurface(*Surface, NSMakeRange(MipLevel, 1)) : nullptr;
	
	SRV->SourceVertexBuffer = nullptr;
	SRV->SourceIndexBuffer = nullptr;
	SRV->SourceStructuredBuffer = nullptr;
	
	SRV->MipLevel = MipLevel;
	SRV->NumMips = 1;
	SRV->Format = PF_Unknown;
		
	if (Surface)
	{
		Surface->SRVs.Add(SRV);
	}
	
	return SRV;
	}
}

void FMetalRHICommandContext::RHIClearTinyUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values)
{
	@autoreleasepool {
	FMetalUnorderedAccessView* UnorderedAccessView = ResourceCast(UnorderedAccessViewRHI);
	if (UnorderedAccessView->SourceView->SourceStructuredBuffer || UnorderedAccessView->SourceView->SourceVertexBuffer)
	{
		check(UnorderedAccessView->SourceView->SourceStructuredBuffer || UnorderedAccessView->SourceView->SourceVertexBuffer);
		
		id<MTLBuffer> Buffer = UnorderedAccessView->SourceView->SourceVertexBuffer ? UnorderedAccessView->SourceView->SourceVertexBuffer->Buffer : UnorderedAccessView->SourceView->SourceStructuredBuffer->Buffer;
		uint32 Size = UnorderedAccessView->SourceView->SourceVertexBuffer ? UnorderedAccessView->SourceView->SourceVertexBuffer->GetSize() : UnorderedAccessView->SourceView->SourceStructuredBuffer->GetSize();
		
		uint32 NumComponents = 1;
		uint32 NumBytes = 1;
		EPixelFormat Format = (EPixelFormat)UnorderedAccessView->SourceView->Format;
		if (Format != 0)
		{
			NumComponents = GPixelFormats[Format].NumComponents;
			NumBytes = GPixelFormats[Format].BlockBytes;
		}
		
		// If all the values are the same then we can treat it as one component.
		NumComponents = (Values[0] == Values[1] == Values[2] == Values[3]) ? 1 : NumComponents;
		
		
		if (NumComponents > 1 || NumBytes > 1)
		{
			// get the pointer to send back for writing
			uint32 AlignedSize = Align(Size, BufferOffsetAlignment);
			uint32 Offset = 0;
			id<MTLBuffer> Temp = nil;
			bool bBufferPooled = false;
			
			if (AlignedSize > (1024 * 1024))
			{
				FMetalPooledBufferArgs Args(GetMetalDeviceContext().GetDevice(), AlignedSize, MTLStorageModeShared);
				Temp = GetMetalDeviceContext().CreatePooledBuffer(Args);
				bBufferPooled = true;
			}
			else
			{
				Offset = Context->AllocateFromRingBuffer(AlignedSize);
				Temp = Context->GetRingBuffer();
			}
			
			// Construct a pattern that can be encoded into the temporary buffer (handles packing & 2-byte formats).
			uint32 Pattern[4];
			switch(Format)
			{
				case PF_Unknown:
				case PF_R8_UINT:
				case PF_G8:
				case PF_A8:
				{
					Pattern[0] = Values[0];
					break;
				}
				case PF_G16:
				case PF_R16F:
				case PF_R16F_FILTER:
				case PF_R16_UINT:
				case PF_R16_SINT:
				{
					Pattern[0] = Values[0];
					break;
				}
				case PF_R32_FLOAT:
				case PF_R32_UINT:
				case PF_R32_SINT:
				{
					Pattern[0] = Values[0];
					break;
				}
				case PF_R8G8:
				case PF_V8U8:
				{
					UE_LOG(LogMetal, Warning, TEXT("UAV pattern fill for format: %d is untested"), Format);
					Pattern[0] = Values[0];
					Pattern[1] = Values[1];
					break;
				}
				case PF_G16R16:
				case PF_G16R16F:
				case PF_R16G16_UINT:
				case PF_G16R16F_FILTER:
				{
					UE_LOG(LogMetal, Warning, TEXT("UAV pattern fill for format: %d is untested"), Format);
					Pattern[0] = Values[0];
					Pattern[0] |= ((Values[1] & 0xffff) << 16);
					break;
				}
				case PF_G32R32F:
				{
					UE_LOG(LogMetal, Warning, TEXT("UAV pattern fill for format: %d is untested"), Format);
					Pattern[0] = Values[0];
					Pattern[1] = Values[1];
					break;
				}
				case PF_R5G6B5_UNORM:
				{
					UE_LOG(LogMetal, Warning, TEXT("UAV pattern fill for format: %d is untested"), Format);
					Pattern[0] = Values[0] & 0x1f;
					Pattern[0] |= (Values[1] & 0x3f) << 5;
					Pattern[0] |= (Values[2] & 0x1f) << 11;
					break;
				}
				case PF_FloatR11G11B10:
				{
					UE_LOG(LogMetal, Warning, TEXT("UAV pattern fill for format: %d is untested"), Format);
					Pattern[0] = Values[0] & 0x7FF;
					Pattern[0] |= ((Values[1] & 0x7FF) << 11);
					Pattern[0] |= ((Values[2] & 0x3FF) << 22);
					break;
				}
				case PF_B8G8R8A8:
				case PF_R8G8B8A8:
				case PF_A8R8G8B8:
				{
					UE_LOG(LogMetal, Warning, TEXT("UAV pattern fill for format: %d is untested"), Format);
					Pattern[0] = Values[0];
					Pattern[0] |= ((Values[1] & 0xff) << 8);
					Pattern[0] |= ((Values[2] & 0xff) << 16);
					Pattern[0] |= ((Values[3] & 0xff) << 24);
					break;
				}
				case PF_A2B10G10R10:
				{
					UE_LOG(LogMetal, Warning, TEXT("UAV pattern fill for format: %d is untested"), Format);
					Pattern[0] = Values[0] & 0x3;
					Pattern[0] |= ((Values[1] & 0x3FF) << 2);
					Pattern[0] |= ((Values[2] & 0x3FF) << 12);
					Pattern[0] |= ((Values[3] & 0x3FF) << 22);
					break;
				}
				case PF_A16B16G16R16:
				case PF_R16G16B16A16_UINT:
				case PF_R16G16B16A16_SINT:
				{
					UE_LOG(LogMetal, Warning, TEXT("UAV pattern fill for format: %d is untested"), Format);
					Pattern[0] = Values[0];
					Pattern[0] |= ((Values[1] & 0xffff) << 16);
					Pattern[1] = Values[2];
					Pattern[1] |= ((Values[3] & 0xffff) << 16);
					break;
				}
				case PF_R32G32B32A32_UINT:
				case PF_A32B32G32R32F:
				{
					UE_LOG(LogMetal, Warning, TEXT("UAV pattern fill for format: %d is untested"), Format);
					Pattern[0] = Values[0];
					Pattern[1] = Values[1];
					Pattern[2] = Values[2];
					Pattern[3] = Values[3];
					break;
				}
				case PF_FloatRGB:
				case PF_FloatRGBA:
				{
					UE_LOG(LogMetal, Fatal, TEXT("No UAV pattern fill for format: %d"), Format);
					break;
				}
				case PF_DepthStencil:
				case PF_ShadowDepth:
				case PF_D24:
				case PF_X24_G8:
				case PF_A1:
				case PF_ASTC_4x4:
				case PF_ASTC_6x6:
				case PF_ASTC_8x8:
				case PF_ASTC_10x10:
				case PF_ASTC_12x12:
				case PF_BC6H:
				case PF_BC7:
				case PF_ETC1:
				case PF_ETC2_RGB:
				case PF_ETC2_RGBA:
				case PF_ATC_RGB:
				case PF_ATC_RGBA_E:
				case PF_ATC_RGBA_I:
				case PF_BC4:
				case PF_PVRTC2:
				case PF_PVRTC4:
				case PF_BC5:
				case PF_DXT1:
				case PF_DXT3:
				case PF_DXT5:
				case PF_UYVY:
				case PF_MAX:
				default:
				{
					UE_LOG(LogMetal, Fatal, TEXT("No UAV support for format: %d"), Format);
					break;
				}
			}
			
			// Pattern memset for varying blocksize (1/2/4/8/16 bytes)
			switch(NumBytes)
			{
				case 1:
				{
					memset(((uint8*)[Temp contents]) + Offset, Pattern[0], AlignedSize);
					break;
				}
				case 2:
				{
					uint16* Dst = (uint16*)(((uint8*)[Temp contents]) + Offset);
					for (uint32 i = 0; i < AlignedSize / 2; i++, Dst++)
					{
						*Dst = (uint16)Pattern[0];
					}
					break;
				}
				case 4:
				{
					memset_pattern4(((uint8*)[Temp contents]) + Offset, Values, AlignedSize);
					break;
				}
				case 8:
				{
					memset_pattern8(((uint8*)[Temp contents]) + Offset, Values, AlignedSize);
					break;
				}
				case 16:
				{
					memset_pattern16(((uint8*)[Temp contents]) + Offset, Values, AlignedSize);
					break;
				}
				default:
				{
					UE_LOG(LogMetal, Fatal, TEXT("Invalid UAV pattern fill size (%d) for: %d"), NumBytes, Format);
					break;
				}
			}
			
			Context->CopyFromBufferToBuffer(Temp, Offset, Buffer, 0, Size);
			
			if(bBufferPooled)
			{
				GetMetalDeviceContext().ReleasePooledBuffer(Temp);
			}
		}
		else
		{
			// Fill the buffer via a blit encoder - I hope that is sufficient.
			Context->FillBuffer(Buffer, NSMakeRange(0, Size), Values[0]);
		}
		
		// If there are problems you may need to add calls to restore the render command encoder at this point
		// but we don't generally want to do that.
	}
	else if (UnorderedAccessView->SourceView->SourceTexture)
	{
		UE_LOG(LogRHI, Fatal,TEXT("Metal RHI doesn't support RHIClearTinyUAV with FRHITexture yet!"));
	}
	else
	{
		UE_LOG(LogRHI, Fatal,TEXT("Metal RHI doesn't support RHIClearUAV with this type yet!"));
	}
	}
}

FComputeFenceRHIRef FMetalDynamicRHI::RHICreateComputeFence(const FName& Name)
{
	@autoreleasepool {
	return new FMetalComputeFence(Name);
	}
}

void FMetalComputeFence::Wait(FMetalContext& Context)
{
	if (Context.GetCurrentCommandBuffer())
	{
		Context.SubmitCommandsHint(EMetalSubmitFlagsNone);
	}
	Context.GetCurrentRenderPass().Begin(Fence);
}

void FMetalRHICommandContext::RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFence)
{
	@autoreleasepool
	{
		if (WriteComputeFence)
		{
			FMetalComputeFence* Fence = ResourceCast(WriteComputeFence);
			Fence->Write(Context->GetCurrentRenderPass().End());
			if (GSupportsEfficientAsyncCompute)
			{
				this->RHISubmitCommandsHint();
			}
		}
	}
}

void FMetalRHICommandContext::RHIWaitComputeFence(FComputeFenceRHIParamRef InFence)
{
	@autoreleasepool {
	if (InFence)
	{
		checkf(InFence->GetWriteEnqueued(), TEXT("ComputeFence: %s waited on before being written. This will hang the GPU."), *InFence->GetName().ToString());
		FMetalComputeFence* Fence = ResourceCast(InFence);
		Fence->Wait(*Context);
	}
	}
}
