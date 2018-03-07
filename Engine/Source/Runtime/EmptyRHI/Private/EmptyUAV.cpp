// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "EmptyRHIPrivate.h"

FEmptyShaderResourceView::~FEmptyShaderResourceView()
{
	SourceVertexBuffer = NULL;
	SourceTexture = NULL;
}



FUnorderedAccessViewRHIRef FEmptyDynamicRHI::RHICreateUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBufferRHI, bool bUseUAVCounter, bool bAppendBuffer)
{
	FEmptyStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	// create the UAV buffer to point to the structured buffer's memory
	FEmptyUnorderedAccessView* UAV = new FEmptyUnorderedAccessView;
	UAV->SourceStructuredBuffer = StructuredBuffer;

	return UAV;
}

FUnorderedAccessViewRHIRef FEmptyDynamicRHI::RHICreateUnorderedAccessView(FTextureRHIParamRef TextureRHI, uint32 MipLevel)
{
	FEmptySurface& Surface = GetEmptySurfaceFromRHITexture(TextureRHI);

	// create the UAV buffer to point to the structured buffer's memory
	FEmptyUnorderedAccessView* UAV = new FEmptyUnorderedAccessView;
	UAV->SourceTexture = (FRHITexture*)TextureRHI;

	return UAV;
}

FUnorderedAccessViewRHIRef FEmptyDynamicRHI::RHICreateUnorderedAccessView(FVertexBufferRHIParamRef VertexBufferRHI, uint8 Format)
{
	FEmptyVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

	// create the UAV buffer to point to the structured buffer's memory
	FEmptyUnorderedAccessView* UAV = new FEmptyUnorderedAccessView;
	UAV->SourceVertexBuffer = VertexBuffer;

	return UAV;
}

FShaderResourceViewRHIRef FEmptyDynamicRHI::RHICreateShaderResourceView(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	FEmptyStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	FEmptyShaderResourceView* SRV = new FEmptyShaderResourceView;
	return SRV;
}

FShaderResourceViewRHIRef FEmptyDynamicRHI::RHICreateShaderResourceView(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Stride, uint8 Format)
{
	FEmptyVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

	FEmptyShaderResourceView* SRV = new FEmptyShaderResourceView;
	SRV->SourceVertexBuffer = VertexBuffer;
	return SRV;
}

FShaderResourceViewRHIRef FEmptyDynamicRHI::RHICreateShaderResourceView(FIndexBufferRHIParamRef BufferRHI)
{
	// there should be no need to create an object
	return FShaderResourceViewRHIRef();
}

FShaderResourceViewRHIRef FEmptyDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	FEmptyShaderResourceView* SRV = new FEmptyShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)Texture2DRHI;
	return SRV;
}

FShaderResourceViewRHIRef FEmptyDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	FEmptyShaderResourceView* SRV = new FEmptyShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)Texture2DRHI;
	return SRV;
}

FShaderResourceViewRHIRef FEmptyDynamicRHI::RHICreateShaderResourceView(FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel)
{
	FEmptyShaderResourceView* SRV = new FEmptyShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)Texture3DRHI;
	return SRV;
}

FShaderResourceViewRHIRef FEmptyDynamicRHI::RHICreateShaderResourceView(FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel)
{
	FEmptyShaderResourceView* SRV = new FEmptyShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)Texture2DArrayRHI;
	return SRV;
}

FShaderResourceViewRHIRef FEmptyDynamicRHI::RHICreateShaderResourceView(FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel)
{
	FEmptyShaderResourceView* SRV = new FEmptyShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)TextureCubeRHI;
	return SRV;
}

void FEmptyDynamicRHI::RHIClearTinyUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values)
{
	FEmptyUnorderedAccessView* UnorderedAccessView = ResourceCast(UnorderedAccessViewRHI);

}


