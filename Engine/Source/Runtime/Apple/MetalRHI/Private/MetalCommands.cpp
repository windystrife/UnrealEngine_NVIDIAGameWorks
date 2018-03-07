// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommands.cpp: Metal RHI commands implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "GlobalShader.h"
#include "OneColorShader.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "ShaderParameterUtils.h"
#include "SceneUtils.h"
#include "ShaderCache.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"
#include "StaticBoundShaderState.h"
#include "EngineGlobals.h"
#include "PipelineStateCache.h"

static const bool GUsesInvertedZ = true;

/** Vertex declaration for just one FVector4 position. */
class FVector4VertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, 0, VET_Float4, 0, sizeof(FVector4)));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}
	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};
static TGlobalResource<FVector4VertexDeclaration> GVector4VertexDeclaration;

MTLPrimitiveType TranslatePrimitiveType(uint32 PrimitiveType)
{
	switch (PrimitiveType)
	{
		case PT_TriangleList:	return MTLPrimitiveTypeTriangle;
		case PT_TriangleStrip:	return MTLPrimitiveTypeTriangleStrip;
		case PT_LineList:		return MTLPrimitiveTypeLine;
		case PT_PointList:		return MTLPrimitiveTypePoint;
		// Metal doesn't actually actually draw in control-point patch-lists because of the way the compute shader stage works - it can handle any arbitrary patch size and will output triangles.
		case PT_1_ControlPointPatchList:
		case PT_2_ControlPointPatchList:
		case PT_3_ControlPointPatchList:
		case PT_4_ControlPointPatchList:
		case PT_5_ControlPointPatchList:
		case PT_6_ControlPointPatchList:
		case PT_7_ControlPointPatchList:
		case PT_8_ControlPointPatchList:
		case PT_9_ControlPointPatchList:
		case PT_10_ControlPointPatchList:
		case PT_11_ControlPointPatchList:
		case PT_12_ControlPointPatchList:
		case PT_13_ControlPointPatchList:
		case PT_14_ControlPointPatchList:
		case PT_15_ControlPointPatchList:
		case PT_16_ControlPointPatchList:
		case PT_17_ControlPointPatchList:
		case PT_18_ControlPointPatchList:
		case PT_19_ControlPointPatchList:
		case PT_20_ControlPointPatchList:
		case PT_21_ControlPointPatchList:
		case PT_22_ControlPointPatchList:
		case PT_23_ControlPointPatchList:
		case PT_24_ControlPointPatchList:
		case PT_25_ControlPointPatchList:
		case PT_26_ControlPointPatchList:
		case PT_27_ControlPointPatchList:
		case PT_28_ControlPointPatchList:
		case PT_29_ControlPointPatchList:
		case PT_30_ControlPointPatchList:
		case PT_31_ControlPointPatchList:
		case PT_32_ControlPointPatchList:
		{
			static uint32 Logged = 0;
			if (!Logged)
			{
				Logged = 1;
				UE_LOG(LogMetal, Warning, TEXT("Untested primitive type %d"), (int32)PrimitiveType);
			}
			return MTLPrimitiveTypeTriangle;
		}
		default:				UE_LOG(LogMetal, Fatal, TEXT("Unsupported primitive type %d"), (int32)PrimitiveType); return MTLPrimitiveTypeTriangle;
	}
}

void FMetalRHICommandContext::RHISetStreamSource(uint32 StreamIndex,FVertexBufferRHIParamRef VertexBufferRHI,uint32 Stride,uint32 Offset)
{
	
	NOT_SUPPORTED("RHISetStreamSource with Stride is invalid on MetalRHI - Stride must be correctly configured on the vertex-declaration.");
}

void FMetalRHICommandContext::RHISetStreamSource(uint32 StreamIndex,FVertexBufferRHIParamRef VertexBufferRHI,uint32 Offset)
{
	@autoreleasepool {
		FMetalVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
		Context->GetCurrentState().SetVertexStream(StreamIndex, VertexBuffer ? VertexBuffer->Buffer : nil, VertexBuffer ? VertexBuffer->Data : nil, Offset, VertexBuffer ? VertexBuffer->GetSize() : 0);
	}
}

void FMetalDynamicRHI::RHISetStreamOutTargets(uint32 NumTargets, const FVertexBufferRHIParamRef* VertexBuffers, const uint32* Offsets)
{
	NOT_SUPPORTED("RHISetStreamOutTargets");

}

void FMetalRHICommandContext::RHISetRasterizerState(FRasterizerStateRHIParamRef NewStateRHI)
{
	NOT_SUPPORTED("RHISetRasterizerState");
}

void FMetalRHICommandContext::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShaderRHI)
{
	@autoreleasepool {
	FMetalComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);

	// cache this for Dispatch
	// sets this compute shader pipeline as the current (this resets all state, so we need to set all resources after calling this)
	Context->GetCurrentState().SetComputeShader(ComputeShader);
	}
}

void FMetalRHICommandContext::RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState)
{
	@autoreleasepool {
	FMetalComputePipelineState* ComputePipeline = ResourceCast(ComputePipelineState);
	
	// cache this for Dispatch
	// sets this compute shader pipeline as the current (this resets all state, so we need to set all resources after calling this)
	Context->GetCurrentState().SetComputeShader(ComputePipeline->GetComputeShader());
	}
}

void FMetalRHICommandContext::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	@autoreleasepool {
	RHI_PROFILE_DRAW_CALL_STATS(EMTLSamplePointBeforeCompute, EMTLSamplePointAfterCompute, 1,1);
	ThreadGroupCountX = FMath::Max(ThreadGroupCountX, 1u);
	ThreadGroupCountY = FMath::Max(ThreadGroupCountY, 1u);
	ThreadGroupCountZ = FMath::Max(ThreadGroupCountZ, 1u);
	
	Context->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	}
}

void FMetalRHICommandContext::RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
	@autoreleasepool {
	if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
	{
		RHI_PROFILE_DRAW_CALL_STATS(EMTLSamplePointBeforeCompute, EMTLSamplePointAfterCompute, 1,1);
		FMetalVertexBuffer* VertexBuffer = ResourceCast(ArgumentBufferRHI);
		
		Context->DispatchIndirect(VertexBuffer, ArgumentOffset);
	}
	else
	{
		NOT_SUPPORTED("RHIDispatchIndirectComputeShader");
	}
	}
}

void FMetalRHICommandContext::RHISetViewport(uint32 MinX,uint32 MinY,float MinZ,uint32 MaxX,uint32 MaxY,float MaxZ)
{
	@autoreleasepool {
	MTLViewport Viewport;
	Viewport.originX = MinX;
	Viewport.originY = MinY;
	Viewport.width = MaxX - MinX;
	Viewport.height = MaxY - MinY;
	Viewport.znear = MinZ;
	Viewport.zfar = MaxZ;
	
	Context->GetCurrentState().SetViewport(Viewport);
	
	FShaderCache::SetViewport(Context->GetCurrentState().GetShaderCacheStateObject(), MinX,MinY, MinZ, MaxX, MaxY, MaxZ);
	}
}

void FMetalRHICommandContext::RHISetStereoViewport(uint32 LeftMinX, uint32 RightMinX, uint32 LeftMinY, uint32 RightMinY, float MinZ, uint32 LeftMaxX, uint32 RightMaxX, uint32 LeftMaxY, uint32 RightMaxY, float MaxZ)
{
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesMultipleViewports))
	{
		@autoreleasepool {
		MTLViewport Viewport[2];
		
		Viewport[0].originX = LeftMinX;
		Viewport[0].originY = LeftMinY;
		Viewport[0].width = LeftMaxX - LeftMinX;
		Viewport[0].height = LeftMaxY - LeftMinY;
		Viewport[0].znear = MinZ;
		Viewport[0].zfar = MaxZ;
		
		Viewport[1].originX = RightMinX;
		Viewport[1].originY = RightMinY;
		Viewport[1].width = RightMaxX - RightMinX;
		Viewport[1].height = RightMaxY - RightMinY;
		Viewport[1].znear = MinZ;
		Viewport[1].zfar = MaxZ;
		
		Context->GetCurrentState().SetViewports(Viewport, 2);
		FShaderCache::SetViewport(Context->GetCurrentState().GetShaderCacheStateObject(), LeftMinX, LeftMinY, MinZ, LeftMaxX, LeftMaxY, MaxZ);
		}
	}
	else
	{
		NOT_SUPPORTED("RHISetStereoViewport");
	}
}

void FMetalRHICommandContext::RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data)
{ 
	NOT_SUPPORTED("RHISetMultipleViewports");
}

void FMetalRHICommandContext::RHISetScissorRect(bool bEnable,uint32 MinX,uint32 MinY,uint32 MaxX,uint32 MaxY)
{
	@autoreleasepool {
	MTLScissorRect Scissor;
	Scissor.x = MinX;
	Scissor.y = MinY;
	Scissor.width = MaxX - MinX;
	Scissor.height = MaxY - MinY;

	// metal doesn't support 0 sized scissor rect
	if (bEnable == false || Scissor.width == 0 || Scissor.height == 0)
	{
		MTLViewport const& Viewport = Context->GetCurrentState().GetViewport(0);
		CGSize FBSize = Context->GetCurrentState().GetFrameBufferSize();
		
		Scissor.x = Viewport.originX;
		Scissor.y = Viewport.originY;
		Scissor.width = (Viewport.originX + Viewport.width <= FBSize.width) ? Viewport.width : FBSize.width - Viewport.originX;
		Scissor.height = (Viewport.originY + Viewport.height <= FBSize.height) ? Viewport.height : FBSize.height - Viewport.originY;
	}
	Context->GetCurrentState().SetScissorRect(bEnable, Scissor);
	}
}

void FMetalRHICommandContext::RHISetBoundShaderState( FBoundShaderStateRHIParamRef BoundShaderStateRHI)
{
	NOT_SUPPORTED("RHISetBoundShaderState");
}

void FMetalRHICommandContext::RHISetGraphicsPipelineState(FGraphicsPipelineStateRHIParamRef GraphicsState)
{
	@autoreleasepool {
		FMetalGraphicsPipelineState* PipelineState = ResourceCast(GraphicsState);
		Context->GetCurrentState().SetGraphicsPipelineState(PipelineState);
		// Bad Arne!
		RHISetStencilRef(0);
		RHISetBlendFactor(FLinearColor(1.0f, 1.0f, 1.0f));
		FShaderCache::SetGraphicsPipelineStateObject(GetInternalContext().GetCurrentState().GetShaderCacheStateObject(), GraphicsState);
	}
}

void FMetalRHICommandContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAVRHI)
{
	@autoreleasepool {
	FMetalUnorderedAccessView* UAV = ResourceCast(UAVRHI);
	Context->GetCurrentState().SetShaderUnorderedAccessView(SF_Compute, UAVIndex, UAV);
	}
}

void FMetalRHICommandContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 UAVIndex,FUnorderedAccessViewRHIParamRef UAVRHI, uint32 InitialCount)
{
	NOT_SUPPORTED("RHISetUAVParameter");
}


void FMetalRHICommandContext::RHISetShaderTexture(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	@autoreleasepool {
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(NewTextureRHI);
	if (Surface != nullptr)
	{
		Context->GetCurrentState().SetShaderTexture(SF_Vertex, Surface->Texture, TextureIndex);
	}
	else
	{
		Context->GetCurrentState().SetShaderTexture(SF_Vertex, nil, TextureIndex);
	}
	}
}

void FMetalRHICommandContext::RHISetShaderTexture(FHullShaderRHIParamRef HullShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	@autoreleasepool {
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(NewTextureRHI);
	if (Surface != nullptr)
	{
		Context->GetCurrentState().SetShaderTexture(SF_Hull, Surface->Texture, TextureIndex);
	}
	else
	{
		Context->GetCurrentState().SetShaderTexture(SF_Hull, nil, TextureIndex);
	}
	}
}

void FMetalRHICommandContext::RHISetShaderTexture(FDomainShaderRHIParamRef DomainShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	@autoreleasepool {
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(NewTextureRHI);
	if (Surface != nullptr)
	{
		Context->GetCurrentState().SetShaderTexture(SF_Domain, Surface->Texture, TextureIndex);
	}
	else
	{
		Context->GetCurrentState().SetShaderTexture(SF_Domain, nil, TextureIndex);
	}
	}
}

void FMetalRHICommandContext::RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	NOT_SUPPORTED("RHISetShaderTexture-Geometry");

}

void FMetalRHICommandContext::RHISetShaderTexture(FPixelShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	@autoreleasepool {
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(NewTextureRHI);
	if (Surface != nullptr)
	{
		Context->GetCurrentState().SetShaderTexture(SF_Pixel, Surface->Texture, TextureIndex);
	}
	else
	{
		Context->GetCurrentState().SetShaderTexture(SF_Pixel, nil, TextureIndex);
	}
	}
}

void FMetalRHICommandContext::RHISetShaderTexture(FComputeShaderRHIParamRef ComputeShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	@autoreleasepool {
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(NewTextureRHI);
	if (Surface != nullptr)
	{
		Context->GetCurrentState().SetShaderTexture(SF_Compute, Surface->Texture, TextureIndex);
	}
	else
	{
		Context->GetCurrentState().SetShaderTexture(SF_Compute, nil, TextureIndex);
	}
	}
}


void FMetalRHICommandContext::RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	@autoreleasepool {
	FMetalShaderResourceView* SRV = ResourceCast(SRVRHI);
	Context->GetCurrentState().SetShaderResourceView(Context, SF_Vertex, TextureIndex, SRV);
	}
}

void FMetalRHICommandContext::RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	@autoreleasepool {
	FMetalShaderResourceView* SRV = ResourceCast(SRVRHI);
	Context->GetCurrentState().SetShaderResourceView(Context, SF_Hull, TextureIndex, SRV);
	}
}

void FMetalRHICommandContext::RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	@autoreleasepool {
	FMetalShaderResourceView* SRV = ResourceCast(SRVRHI);
	Context->GetCurrentState().SetShaderResourceView(Context, SF_Domain, TextureIndex, SRV);
	}
}

void FMetalRHICommandContext::RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	NOT_SUPPORTED("RHISetShaderResourceViewParameter");
}

void FMetalRHICommandContext::RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	@autoreleasepool {
	FMetalShaderResourceView* SRV = ResourceCast(SRVRHI);
	Context->GetCurrentState().SetShaderResourceView(Context, SF_Pixel, TextureIndex, SRV);
	}
}

void FMetalRHICommandContext::RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	@autoreleasepool {
	FMetalShaderResourceView* SRV = ResourceCast(SRVRHI);
	Context->GetCurrentState().SetShaderResourceView(Context, SF_Compute, TextureIndex, SRV);
	}
}


void FMetalRHICommandContext::RHISetShaderSampler(FVertexShaderRHIParamRef VertexShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	@autoreleasepool {
	FMetalSamplerState* NewState = ResourceCast(NewStateRHI);

	Context->GetCurrentState().SetShaderSamplerState(SF_Vertex, NewState, SamplerIndex);
	}
}

void FMetalRHICommandContext::RHISetShaderSampler(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	@autoreleasepool {
	FMetalSamplerState* NewState = ResourceCast(NewStateRHI);

	Context->GetCurrentState().SetShaderSamplerState(SF_Hull, NewState, SamplerIndex);
	}
}

void FMetalRHICommandContext::RHISetShaderSampler(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	@autoreleasepool {
	FMetalSamplerState* NewState = ResourceCast(NewStateRHI);

	Context->GetCurrentState().SetShaderSamplerState(SF_Domain, NewState, SamplerIndex);
	}
}

void FMetalRHICommandContext::RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	NOT_SUPPORTED("RHISetSamplerState-Geometry");
}

void FMetalRHICommandContext::RHISetShaderSampler(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	@autoreleasepool {
	FMetalSamplerState* NewState = ResourceCast(NewStateRHI);

	Context->GetCurrentState().SetShaderSamplerState(SF_Pixel, NewState, SamplerIndex);
	}
}

void FMetalRHICommandContext::RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	@autoreleasepool {
	FMetalSamplerState* NewState = ResourceCast(NewStateRHI);

	Context->GetCurrentState().SetShaderSamplerState(SF_Compute, NewState, SamplerIndex);
	}
}

void FMetalRHICommandContext::RHISetShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	@autoreleasepool {
	Context->GetCurrentState().GetShaderParameters(CrossCompiler::SHADER_STAGE_VERTEX).Set(BufferIndex, BaseIndex, NumBytes, NewValue);
	}
}

void FMetalRHICommandContext::RHISetShaderParameter(FHullShaderRHIParamRef HullShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	@autoreleasepool {
	Context->GetCurrentState().GetShaderParameters(CrossCompiler::SHADER_STAGE_HULL).Set(BufferIndex, BaseIndex, NumBytes, NewValue);
	}
}

void FMetalRHICommandContext::RHISetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	@autoreleasepool {
	Context->GetCurrentState().GetShaderParameters(CrossCompiler::SHADER_STAGE_PIXEL).Set(BufferIndex, BaseIndex, NumBytes, NewValue);
	}
}

void FMetalRHICommandContext::RHISetShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	@autoreleasepool {
	Context->GetCurrentState().GetShaderParameters(CrossCompiler::SHADER_STAGE_DOMAIN).Set(BufferIndex, BaseIndex, NumBytes, NewValue);
	}
}

void FMetalRHICommandContext::RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	NOT_SUPPORTED("RHISetShaderParameter-Geometry");
}

void FMetalRHICommandContext::RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	@autoreleasepool {
	Context->GetCurrentState().GetShaderParameters(CrossCompiler::SHADER_STAGE_COMPUTE).Set(BufferIndex, BaseIndex, NumBytes, NewValue);
	}
}

void FMetalRHICommandContext::RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	@autoreleasepool {
	FMetalVertexShader* VertexShader = ResourceCast(VertexShaderRHI);
	Context->GetCurrentState().BindUniformBuffer(SF_Vertex, BufferIndex, BufferRHI);

	auto& Bindings = VertexShader->Bindings;
	check(BufferIndex < Bindings.NumUniformBuffers);
	if (Bindings.bHasRegularUniformBuffers)
	{
		auto* UB = (FMetalUniformBuffer*)BufferRHI;
		Context->GetCurrentState().SetShaderBuffer(SF_Vertex, UB->Buffer, UB->Data, UB->Offset, UB->GetSize(), BufferIndex);
	}
	}
}

void FMetalRHICommandContext::RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	@autoreleasepool {
	FMetalHullShader* HullShader = ResourceCast(HullShaderRHI);
	Context->GetCurrentState().BindUniformBuffer(SF_Hull, BufferIndex, BufferRHI);

	auto& Bindings = HullShader->Bindings;
	check(BufferIndex < Bindings.NumUniformBuffers);
	if (Bindings.bHasRegularUniformBuffers)
	{
		auto* UB = (FMetalUniformBuffer*)BufferRHI;
		Context->GetCurrentState().SetShaderBuffer(SF_Hull, UB->Buffer, UB->Data, UB->Offset, UB->GetSize(), BufferIndex);
	}
	}
}

void FMetalRHICommandContext::RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	@autoreleasepool {
	FMetalDomainShader* DomainShader = ResourceCast(DomainShaderRHI);
	Context->GetCurrentState().BindUniformBuffer(SF_Domain, BufferIndex, BufferRHI);

	auto& Bindings = DomainShader->Bindings;
	check(BufferIndex < Bindings.NumUniformBuffers);
	if (Bindings.bHasRegularUniformBuffers)
	{
		auto* UB = (FMetalUniformBuffer*)BufferRHI;
		Context->GetCurrentState().SetShaderBuffer(SF_Domain, UB->Buffer, UB->Data, UB->Offset, UB->GetSize(), BufferIndex);
	}
	}
}

void FMetalRHICommandContext::RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	NOT_SUPPORTED("RHISetShaderUniformBuffer-Geometry");
}

void FMetalRHICommandContext::RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	@autoreleasepool {
	FMetalPixelShader* PixelShader = ResourceCast(PixelShaderRHI);
	Context->GetCurrentState().BindUniformBuffer(SF_Pixel, BufferIndex, BufferRHI);

	auto& Bindings = PixelShader->Bindings;
	check(BufferIndex < Bindings.NumUniformBuffers);
	if (Bindings.bHasRegularUniformBuffers)
	{
		auto* UB = (FMetalUniformBuffer*)BufferRHI;
		Context->GetCurrentState().SetShaderBuffer(SF_Pixel, UB->Buffer, UB->Data, UB->Offset, UB->GetSize(), BufferIndex);
	}
	}
}

void FMetalRHICommandContext::RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	@autoreleasepool {
	FMetalComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	Context->GetCurrentState().BindUniformBuffer(SF_Compute, BufferIndex, BufferRHI);

	auto& Bindings = ComputeShader->Bindings;
	check(BufferIndex < Bindings.NumUniformBuffers);
	if (Bindings.bHasRegularUniformBuffers)
	{
		auto* UB = (FMetalUniformBuffer*)BufferRHI;
		Context->GetCurrentState().SetShaderBuffer(SF_Compute, UB->Buffer, UB->Data, UB->Offset, UB->GetSize(), BufferIndex);
	}
	}
}


void FMetalRHICommandContext::RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewStateRHI, uint32 StencilRef)
{
	NOT_SUPPORTED("RHISetDepthStencilState");
}

void FMetalRHICommandContext::RHISetStencilRef(uint32 StencilRef)
{
	Context->GetCurrentState().SetStencilRef(StencilRef);
}

void FMetalRHICommandContext::RHISetBlendState(FBlendStateRHIParamRef NewStateRHI, const FLinearColor& BlendFactor)
{
	NOT_SUPPORTED("RHISetBlendState");
}

void FMetalRHICommandContext::RHISetBlendFactor(const FLinearColor& BlendFactor)
{
	Context->GetCurrentState().SetBlendFactor(BlendFactor);
}

void FMetalRHICommandContext::RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets,
	const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs)
{
	@autoreleasepool {
	FMetalContext* Manager = Context;
	FRHIDepthRenderTargetView DepthView;
	if (NewDepthStencilTargetRHI)
	{
		DepthView = *NewDepthStencilTargetRHI;
	}
	else
	{
		DepthView = FRHIDepthRenderTargetView(FTextureRHIParamRef(), ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::ENoAction);
	}

	FRHISetRenderTargetsInfo Info(NumSimultaneousRenderTargets, NewRenderTargets, DepthView);
	Info.NumUAVs = NumUAVs;
	for (uint32 i = 0; i < NumUAVs; i++)
	{
		Info.UnorderedAccessView[i] = UAVs[i];
	}
	RHISetRenderTargetsAndClear(Info);
	}
}

void FMetalDynamicRHI::RHIDiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask)
{
	// Deliberate do nothing - Metal doesn't care about this.
}

void FMetalRHICommandContext::RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	@autoreleasepool {
	FMetalContext* Manager = Context;
	

	if (Context->GetCommandQueue().SupportsFeature(EMetalFeaturesGraphicsUAVs))
	{
		for (uint32 i = 0; i < RenderTargetsInfo.NumUAVs; i++)
		{
			if (IsValidRef(RenderTargetsInfo.UnorderedAccessView[i]))
			{
				FMetalUnorderedAccessView* UAV = ResourceCast(RenderTargetsInfo.UnorderedAccessView[i].GetReference());
				Context->GetCurrentState().SetShaderUnorderedAccessView(SF_Pixel, i, UAV);
			}
		}
	}
	else
	{
		checkf(RenderTargetsInfo.NumUAVs == 0, TEXT("Calling SetRenderTargets with UAVs is not supported in this Metal standard"));
	}

	Manager->SetRenderTargetsInfo(RenderTargetsInfo);

	// Set the viewport to the full size of render target 0.
	if (RenderTargetsInfo.ColorRenderTarget[0].Texture)
	{
		const FRHIRenderTargetView& RenderTargetView = RenderTargetsInfo.ColorRenderTarget[0];
		FMetalSurface* RenderTarget = GetMetalSurfaceFromRHITexture(RenderTargetView.Texture);

		uint32 Width = FMath::Max((uint32)(RenderTarget->Texture.width >> RenderTargetView.MipIndex), (uint32)1);
		uint32 Height = FMath::Max((uint32)(RenderTarget->Texture.height >> RenderTargetView.MipIndex), (uint32)1);

		RHISetViewport(0, 0, 0.0f, Width, Height, 1.0f);
    }
    
    FShaderCache::SetRenderTargets(Context->GetCurrentState().GetShaderCacheStateObject(), RenderTargetsInfo.NumColorRenderTargets, RenderTargetsInfo.ColorRenderTarget, &RenderTargetsInfo.DepthStencilRenderTarget);
	}
}


void FMetalRHICommandContext::RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	@autoreleasepool {
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
	//checkf(NumInstances == 1, TEXT("Currently only 1 instance is supported"));
	
	NumInstances = FMath::Max(NumInstances,1u);
	
	RHI_DRAW_CALL_STATS(PrimitiveType,NumInstances*NumPrimitives);

	// how many verts to render
	uint32 NumVertices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);
	uint32 VertexCount = GetVertexCountForPrimitiveCount(NumPrimitives,PrimitiveType);
	
	// draw!
	if(!FShaderCache::IsPredrawCall(Context->GetCurrentState().GetShaderCacheStateObject()))
	{
		RHI_PROFILE_DRAW_CALL_STATS(EMTLSamplePointBeforeDraw, EMTLSamplePointAfterDraw, NumPrimitives * NumInstances, VertexCount * NumInstances);
	}
	
	Context->DrawPrimitive(PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances);
	}
}

void FMetalRHICommandContext::RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef VertexBufferRHI, uint32 ArgumentOffset)
{
	@autoreleasepool {
#if PLATFORM_IOS
	NOT_SUPPORTED("RHIDrawPrimitiveIndirect");
#else
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
	RHI_DRAW_CALL_STATS(PrimitiveType,1);
	
	FMetalVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	
	if(!FShaderCache::IsPredrawCall(Context->GetCurrentState().GetShaderCacheStateObject()))
	{
		RHI_PROFILE_DRAW_CALL_STATS(EMTLSamplePointBeforeDraw, EMTLSamplePointAfterDraw, 1, 1);
	}
	
	Context->DrawPrimitiveIndirect(PrimitiveType, VertexBuffer, ArgumentOffset);
#endif
	}
}

void FMetalRHICommandContext::RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance,
	uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	@autoreleasepool {
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
	//checkf(NumInstances == 1, TEXT("Currently only 1 instance is supported"));
	checkf(GRHISupportsBaseVertexIndex || BaseVertexIndex == 0, TEXT("BaseVertexIndex must be 0, see GRHISupportsBaseVertexIndex"));
	checkf(GRHISupportsFirstInstance || FirstInstance == 0, TEXT("FirstInstance must be 0, see GRHISupportsFirstInstance"));
	
	RHI_DRAW_CALL_STATS(PrimitiveType,FMath::Max(NumInstances,1u)*NumPrimitives);

	if(!FShaderCache::IsPredrawCall(Context->GetCurrentState().GetShaderCacheStateObject()))
	{
		RHI_PROFILE_DRAW_CALL_STATS(EMTLSamplePointBeforeDraw, EMTLSamplePointAfterDraw, NumPrimitives * NumInstances, NumVertices * NumInstances);
	}
		
	FMetalIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	
	Context->DrawIndexedPrimitive(IndexBuffer->Buffer, IndexBuffer->GetStride(), IndexBuffer->IndexType, PrimitiveType, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
	}
}

void FMetalRHICommandContext::RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef VertexBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances)
{
	@autoreleasepool {
	if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
	{
		check(NumInstances > 1);
		
		SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
		RHI_DRAW_CALL_STATS(PrimitiveType,1);
		
		FMetalIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
		FMetalStructuredBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
		
		if(!FShaderCache::IsPredrawCall(Context->GetCurrentState().GetShaderCacheStateObject()))
		{
			RHI_PROFILE_DRAW_CALL_STATS(EMTLSamplePointBeforeDraw, EMTLSamplePointAfterDraw, 1, 1);
		}
		
		Context->DrawIndexedIndirect(IndexBuffer, PrimitiveType, VertexBuffer, DrawArgumentsIndex, NumInstances);
	}
	else
	{
		NOT_SUPPORTED("RHIDrawIndexedIndirect");
	}
	}
}

void FMetalRHICommandContext::RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FIndexBufferRHIParamRef IndexBufferRHI,FVertexBufferRHIParamRef VertexBufferRHI,uint32 ArgumentOffset)
{
	@autoreleasepool {
	if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
	{
		SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
		RHI_DRAW_CALL_STATS(PrimitiveType,1);
		
		FMetalIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
		FMetalVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
		
		if(!FShaderCache::IsPredrawCall(Context->GetCurrentState().GetShaderCacheStateObject()))
		{
			RHI_PROFILE_DRAW_CALL_STATS(EMTLSamplePointBeforeDraw, EMTLSamplePointAfterDraw, 1, 1);
		}
		
		Context->DrawIndexedPrimitiveIndirect(PrimitiveType, IndexBuffer, VertexBuffer, ArgumentOffset);
	}
	else
	{
		NOT_SUPPORTED("RHIDrawIndexedPrimitiveIndirect");
	}
	}
}


void FMetalRHICommandContext::RHIBeginDrawPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
{
	@autoreleasepool {
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
	checkSlow(PendingVertexBufferOffset == 0xFFFFFFFF);

	// allocate space
	PendingVertexBufferOffset = Context->AllocateFromRingBuffer(VertexDataStride * NumVertices);

	// get the pointer to send back for writing
	uint8* RingBufferBytes = (uint8*)[Context->GetRingBuffer() contents];
	OutVertexData = RingBufferBytes + PendingVertexBufferOffset;
	
	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingVertexDataStride = VertexDataStride;
	}
}


void FMetalRHICommandContext::RHIEndDrawPrimitiveUP()
{
	@autoreleasepool {
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);

	RHI_DRAW_CALL_STATS(PendingPrimitiveType,PendingNumPrimitives);

	// set the vertex buffer
	uint32 NumVertices = GetVertexCountForPrimitiveCount(PendingNumPrimitives, PendingPrimitiveType);
	Context->GetCurrentState().SetVertexStream(0, Context->GetRingBuffer(), nil, PendingVertexBufferOffset, PendingVertexDataStride * NumVertices);
	if(Context->GetCurrentState().GetUsingTessellation())
	{
		Context->GetCurrentState().SetShaderBuffer(SF_Hull, Context->GetRingBuffer(), nil, PendingVertexBufferOffset, PendingVertexDataStride * NumVertices, UNREAL_TO_METAL_BUFFER_INDEX(0));
		Context->GetCurrentState().SetShaderBuffer(SF_Domain, Context->GetRingBuffer(), nil, PendingVertexBufferOffset, PendingVertexDataStride * NumVertices, UNREAL_TO_METAL_BUFFER_INDEX(0));
	}
	
	// how many to draw
	if(!FShaderCache::IsPredrawCall(Context->GetCurrentState().GetShaderCacheStateObject()))
	{
		RHI_PROFILE_DRAW_CALL_STATS(EMTLSamplePointBeforeDraw, EMTLSamplePointAfterDraw, PendingNumPrimitives,NumVertices);
	}
	
	Context->DrawPrimitive(PendingPrimitiveType, 0, PendingNumPrimitives, Context->GetCurrentState().GetRenderTargetArraySize());
	
	// mark temp memory as usable
	PendingVertexBufferOffset = 0xFFFFFFFF;
	}
}

void FMetalRHICommandContext::RHIBeginDrawIndexedPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData)
{
	@autoreleasepool {
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
	checkSlow(PendingVertexBufferOffset == 0xFFFFFFFF);
	checkSlow(PendingIndexBufferOffset == 0xFFFFFFFF);

	// allocate space
	uint32 VertexSize = Align(VertexDataStride * NumVertices, BufferOffsetAlignment);
	uint32 IndexSize = Align(IndexDataStride * NumIndices, BufferOffsetAlignment);
	PendingVertexBufferOffset = Context->AllocateFromRingBuffer(VertexSize + IndexSize);
	PendingIndexBufferOffset = PendingVertexBufferOffset + VertexSize;
	
	// get the pointers to send back for writing
	uint8* RingBufferBytes = (uint8*)[Context->GetRingBuffer() contents];
	OutVertexData = RingBufferBytes + PendingVertexBufferOffset;
	OutIndexData = RingBufferBytes + PendingIndexBufferOffset;
	
	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingIndexDataStride = IndexDataStride;

	PendingVertexDataStride = VertexDataStride;
	}
}

void FMetalRHICommandContext::RHIEndDrawIndexedPrimitiveUP()
{
	@autoreleasepool {
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);

	RHI_DRAW_CALL_STATS(PendingPrimitiveType,PendingNumPrimitives);

	// set the vertex buffer
	Context->GetCurrentState().SetVertexStream(0, Context->GetRingBuffer(), nil, PendingVertexBufferOffset, PendingIndexBufferOffset - PendingVertexBufferOffset);

	// how many to draw
	uint32 NumIndices = GetVertexCountForPrimitiveCount(PendingNumPrimitives, PendingPrimitiveType);
	
	if(!FShaderCache::IsPredrawCall(Context->GetCurrentState().GetShaderCacheStateObject()))
	{
		RHI_PROFILE_DRAW_CALL_STATS(EMTLSamplePointBeforeDraw, EMTLSamplePointAfterDraw, PendingNumPrimitives,NumIndices);
	}
	
	Context->DrawIndexedPrimitive(Context->GetRingBuffer(), PendingIndexDataStride, (PendingIndexDataStride == 2) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32, PendingPrimitiveType, 0, 0, NumIndices, PendingIndexBufferOffset/PendingIndexDataStride, PendingNumPrimitives, 1);
	
	// mark temp memory as usable
	PendingVertexBufferOffset = 0xFFFFFFFF;
	PendingIndexBufferOffset = 0xFFFFFFFF;
	}
}

void FMetalDynamicRHI::SetupRecursiveResources()
{
    /*
	@autoreleasepool {
	static bool bSetupResources = false;
	if (GRHISupportsRHIThread && !bSetupResources)
	{
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		extern int32 GCreateShadersOnLoad;
		TGuardValue<int32> Guard(GCreateShadersOnLoad, 1);
		auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<TOneColorVS<true> > DefaultVertexShader(ShaderMap);
		TShaderMapRef<TOneColorVS<true, true> > LayeredVertexShader(ShaderMap);
		GVector4VertexDeclaration.InitRHI();
		
		for (uint32 Instanced = 0; Instanced < 2; Instanced++)
		{
			FShader* VertexShader = !Instanced ? (FShader*)*DefaultVertexShader : (FShader*)*LayeredVertexShader;
			
			for (int32 NumBuffers = 1; NumBuffers <= MaxSimultaneousRenderTargets; NumBuffers++)
			{
				FOneColorPS* PixelShader = NULL;
				
				// Set the shader to write to the appropriate number of render targets
				// On AMD PC hardware, outputting to a color index in the shader without a matching render target set has a significant performance hit
				if (NumBuffers <= 1)
				{
					TShaderMapRef<TOneColorPixelShaderMRT<1> > MRTPixelShader(ShaderMap);
					PixelShader = *MRTPixelShader;
				}
				else if (IsFeatureLevelSupported( GMaxRHIShaderPlatform, ERHIFeatureLevel::SM4 ))
				{
					if (NumBuffers == 2)
					{
						TShaderMapRef<TOneColorPixelShaderMRT<2> > MRTPixelShader(ShaderMap);
						PixelShader = *MRTPixelShader;
					}
					else if (NumBuffers== 3)
					{
						TShaderMapRef<TOneColorPixelShaderMRT<3> > MRTPixelShader(ShaderMap);
						PixelShader = *MRTPixelShader;
					}
					else if (NumBuffers == 4)
					{
						TShaderMapRef<TOneColorPixelShaderMRT<4> > MRTPixelShader(ShaderMap);
						PixelShader = *MRTPixelShader;
					}
					else if (NumBuffers == 5)
					{
						TShaderMapRef<TOneColorPixelShaderMRT<5> > MRTPixelShader(ShaderMap);
						PixelShader = *MRTPixelShader;
					}
					else if (NumBuffers == 6)
					{
						TShaderMapRef<TOneColorPixelShaderMRT<6> > MRTPixelShader(ShaderMap);
						PixelShader = *MRTPixelShader;
					}
					else if (NumBuffers == 7)
					{
						TShaderMapRef<TOneColorPixelShaderMRT<7> > MRTPixelShader(ShaderMap);
						PixelShader = *MRTPixelShader;
					}
					else if (NumBuffers == 8)
					{
						TShaderMapRef<TOneColorPixelShaderMRT<8> > MRTPixelShader(ShaderMap);
						PixelShader = *MRTPixelShader;
					}
				}
				
				// SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, GClearMRTBoundShaderState[NumBuffers - 1][Instanced], GVector4VertexDeclaration.VertexDeclarationRHI, VertexShader, PixelShader);
			}
		}
		
		bSetupResources = true;
	}
	}
    */
}

void FMetalRHICommandContext::RHIClearMRT(bool bClearColor,int32 NumClearColors,const FLinearColor* ClearColorArray,bool bClearDepth,float Depth,bool bClearStencil,uint32 Stencil)
{
	NOT_SUPPORTED("RHIClearMRT");
}

void FMetalDynamicRHI::RHIBlockUntilGPUIdle()
{
	@autoreleasepool {
	ImmediateContext.Context->SubmitCommandBufferAndWait();
	}
}

uint32 FMetalDynamicRHI::RHIGetGPUFrameCycles()
{
	return GGPUFrameTime;
}

void FMetalRHICommandContext::RHIAutomaticCacheFlushAfterComputeShader(bool bEnable)
{
	// Nothing required here
}

void FMetalRHICommandContext::RHIFlushComputeShaderCache()
{
	// Nothing required here
}

void FMetalDynamicRHI::RHIExecuteCommandList(FRHICommandList* RHICmdList)
{
	NOT_SUPPORTED("RHIExecuteCommandList");
}

void FMetalRHICommandContext::RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth)
{
    // not supported
	NOT_SUPPORTED("RHIEnableDepthBoundsTest");
}

void FMetalRHICommandContext::RHISubmitCommandsHint()
{
	@autoreleasepool {
    Context->SubmitCommandsHint();
	}
}

IRHICommandContext* FMetalDynamicRHI::RHIGetDefaultContext()
{
	return &ImmediateContext;
}

IRHIComputeContext* FMetalDynamicRHI::RHIGetDefaultAsyncComputeContext()
{
	@autoreleasepool {
	IRHIComputeContext* ComputeContext = GSupportsEfficientAsyncCompute && AsyncComputeContext ? AsyncComputeContext : RHIGetDefaultContext();
	// On platforms that support non-async compute we set this to the normal context.  It won't be async, but the high level
	// code can be agnostic if it wants to be.
	return ComputeContext;
	}
}
