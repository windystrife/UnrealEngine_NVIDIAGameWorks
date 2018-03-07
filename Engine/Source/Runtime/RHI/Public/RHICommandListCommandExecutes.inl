// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHICommandListCommandExecutes.inl: RHI Command List execute functions.
=============================================================================*/

#if !defined(INTERNAL_DECORATOR)
	#define INTERNAL_DECORATOR(Method) CmdList.GetContext().Method
#endif

//for functions where the signatures do not match between gfx and compute commandlists
#if !defined(INTERNAL_DECORATOR_COMPUTE)
#define INTERNAL_DECORATOR_COMPUTE(Method) CmdList.GetComputeContext().Method
#endif

//for functions where the signatures match between gfx and compute commandlists
#if !defined(INTERNAL_DECORATOR_CONTEXT_PARAM1)
#define INTERNAL_DECORATOR_CONTEXT(Method) IRHIComputeContext& Context = (CmdListType == ECmdList::EGfx) ? CmdList.GetContext() : CmdList.GetComputeContext(); Context.Method
#endif

class FRHICommandListBase;
class IRHIComputeContext;
struct FComputedBSS;
struct FComputedGraphicsPipelineState;
struct FComputedUniformBuffer;
struct FMemory;
struct FRHICommandAutomaticCacheFlushAfterComputeShader;
struct FRHICommandBeginDrawingViewport;
struct FRHICommandBeginFrame;
struct FRHICommandBeginOcclusionQueryBatch;
struct FRHICommandBeginRenderQuery;
struct FRHICommandBeginScene;
struct FRHICommandBindClearMRTValues;
struct FRHICommandBuildLocalBoundShaderState;
struct FRHICommandBuildLocalGraphicsPipelineState;
struct FRHICommandBuildLocalUniformBuffer;
struct FRHICommandClearUAV;
struct FRHICommandCopyToResolveTarget;
struct FRHICommandDrawIndexedIndirect;
struct FRHICommandDrawIndexedPrimitive;
struct FRHICommandDrawIndexedPrimitiveIndirect;
struct FRHICommandDrawPrimitive;
struct FRHICommandDrawPrimitiveIndirect;
struct FRHICommandEnableDepthBoundsTest;
struct FRHICommandEndDrawIndexedPrimitiveUP;
struct FRHICommandEndDrawingViewport;
struct FRHICommandEndDrawPrimitiveUP;
struct FRHICommandEndFrame;
struct FRHICommandEndOcclusionQueryBatch;
struct FRHICommandEndRenderQuery;
struct FRHICommandEndScene;
struct FRHICommandFlushComputeShaderCache;
struct FRHICommandSetBlendFactor;
struct FRHICommandSetBlendState;
struct FRHICommandSetBoundShaderState;
struct FRHICommandSetDepthStencilState;
struct FRHICommandSetLocalBoundShaderState;
struct FRHICommandSetLocalGraphicsPipelineState;
struct FRHICommandSetRasterizerState;
struct FRHICommandSetRenderTargets;
struct FRHICommandSetRenderTargetsAndClear;
struct FRHICommandSetScissorRect;
struct FRHICommandSetStencilRef;
struct FRHICommandSetStereoViewport;
struct FRHICommandSetStreamSource;
struct FRHICommandSetViewport;
struct FRHICommandTransitionTextures;
struct FRHICommandTransitionTexturesArray;
struct FRHICommandUpdateTextureReference;
enum class ECmdList;
template <typename TShaderRHIParamRef> struct FRHICommandSetLocalUniformBuffer;

void FRHICommandBeginUpdateMultiFrameResource::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginUpdateMultiFrameResource);
	INTERNAL_DECORATOR(RHIBeginUpdateMultiFrameResource)(Texture);
}

void FRHICommandEndUpdateMultiFrameResource::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndUpdateMultiFrameResource);
	INTERNAL_DECORATOR(RHIEndUpdateMultiFrameResource)(Texture);
}

void FRHICommandBeginUpdateMultiFrameUAV::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginUpdateMultiFrameUAV);
	INTERNAL_DECORATOR(RHIBeginUpdateMultiFrameResource)(UAV);
}

void FRHICommandEndUpdateMultiFrameUAV::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndUpdateMultiFrameUAV);
	INTERNAL_DECORATOR(RHIEndUpdateMultiFrameResource)(UAV);
}

void FRHICommandSetRasterizerState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetRasterizerState);
	INTERNAL_DECORATOR(RHISetRasterizerState)(State);
}

void FRHICommandSetDepthStencilState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetDepthStencilState);
	INTERNAL_DECORATOR(RHISetDepthStencilState)(State, StencilRef);
}

void FRHICommandSetStencilRef::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetStencilRef);
	INTERNAL_DECORATOR(RHISetStencilRef)(StencilRef);
}

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetShaderParameter<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderParameter);
	INTERNAL_DECORATOR(RHISetShaderParameter)(Shader, BufferIndex, BaseIndex, NumBytes, NewValue); 
}
template struct FRHICommandSetShaderParameter<FVertexShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderParameter<FHullShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderParameter<FDomainShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderParameter<FGeometryShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderParameter<FPixelShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderParameter<FComputeShaderRHIParamRef, ECmdList::EGfx>;
template<> void FRHICommandSetShaderParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderParameter);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderParameter)(Shader, BufferIndex, BaseIndex, NumBytes, NewValue);
}

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetShaderUniformBuffer<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderUniformBuffer);
	INTERNAL_DECORATOR(RHISetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer);
}
template struct FRHICommandSetShaderUniformBuffer<FVertexShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderUniformBuffer<FHullShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderUniformBuffer<FDomainShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderUniformBuffer<FGeometryShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderUniformBuffer<FPixelShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderUniformBuffer<FComputeShaderRHIParamRef, ECmdList::EGfx>;
template<> void FRHICommandSetShaderUniformBuffer<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderUniformBuffer);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer);
}

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetShaderTexture<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderTexture);
	INTERNAL_DECORATOR(RHISetShaderTexture)(Shader, TextureIndex, Texture);
}
template struct FRHICommandSetShaderTexture<FVertexShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderTexture<FHullShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderTexture<FDomainShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderTexture<FGeometryShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderTexture<FPixelShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderTexture<FComputeShaderRHIParamRef, ECmdList::EGfx>;
template<> void FRHICommandSetShaderTexture<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderTexture);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderTexture)(Shader, TextureIndex, Texture);
}

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetShaderResourceViewParameter<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderResourceViewParameter);
	INTERNAL_DECORATOR(RHISetShaderResourceViewParameter)(Shader, SamplerIndex, SRV);
}
template struct FRHICommandSetShaderResourceViewParameter<FVertexShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderResourceViewParameter<FHullShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderResourceViewParameter<FDomainShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderResourceViewParameter<FGeometryShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderResourceViewParameter<FPixelShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderResourceViewParameter<FComputeShaderRHIParamRef, ECmdList::EGfx>;
template<> void FRHICommandSetShaderResourceViewParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderResourceViewParameter);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderResourceViewParameter)(Shader, SamplerIndex, SRV);
}

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetUAVParameter<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetUAVParameter);
	INTERNAL_DECORATOR_CONTEXT(RHISetUAVParameter)(Shader, UAVIndex, UAV);
}
template struct FRHICommandSetUAVParameter<FComputeShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetUAVParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>;

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetUAVParameter_IntialCount<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetUAVParameter);
	INTERNAL_DECORATOR_CONTEXT(RHISetUAVParameter)(Shader, UAVIndex, UAV, InitialCount);
}
template struct FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef, ECmdList::ECompute>;

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetShaderSampler<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderSampler);
	INTERNAL_DECORATOR(RHISetShaderSampler)(Shader, SamplerIndex, Sampler);
}
template struct FRHICommandSetShaderSampler<FVertexShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderSampler<FHullShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderSampler<FDomainShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderSampler<FGeometryShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderSampler<FPixelShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderSampler<FComputeShaderRHIParamRef, ECmdList::EGfx>;
template<> void FRHICommandSetShaderSampler<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderSampler);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderSampler)(Shader, SamplerIndex, Sampler);
}

// WaveWorks Start
void FRHICommandSetWaveWorksState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetWaveWorksState);
	INTERNAL_DECORATOR(RHISetWaveWorksState)(State, ViewMatrix, ShaderInputMappings);
}
// WaveWorks End

void FRHICommandDrawPrimitive::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawPrimitive);
	INTERNAL_DECORATOR(RHIDrawPrimitive)(PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances);
}

void FRHICommandDrawIndexedPrimitive::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawIndexedPrimitive);
	INTERNAL_DECORATOR(RHIDrawIndexedPrimitive)(IndexBuffer, PrimitiveType, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
}

void FRHICommandSetBoundShaderState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetBoundShaderState);
	INTERNAL_DECORATOR(RHISetBoundShaderState)(BoundShaderState);
}

void FRHICommandSetBlendState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetBlendState);
	INTERNAL_DECORATOR(RHISetBlendState)(State, BlendFactor);
}

void FRHICommandSetBlendFactor::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetBlendFactor);
	INTERNAL_DECORATOR(RHISetBlendFactor)(BlendFactor);
}

void FRHICommandSetStreamSourceDEPRECATED::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetStreamSource);
	INTERNAL_DECORATOR(RHISetStreamSource)(StreamIndex, VertexBuffer, Stride, Offset);
}

void FRHICommandSetStreamSource::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetStreamSource);
	INTERNAL_DECORATOR(RHISetStreamSource)(StreamIndex, VertexBuffer, Offset);
}

void FRHICommandSetViewport::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetViewport);
	INTERNAL_DECORATOR(RHISetViewport)(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
}

void FRHICommandSetStereoViewport::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetStereoViewport);
	INTERNAL_DECORATOR(RHISetStereoViewport)(LeftMinX, RightMinX, LeftMinY, RightMinY, MinZ, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, MaxZ);
}

void FRHICommandSetScissorRect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetScissorRect);
	INTERNAL_DECORATOR(RHISetScissorRect)(bEnable, MinX, MinY, MaxX, MaxY);
}

void FRHICommandBeginRenderPass::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginRenderPass);
	check(!LocalRenderPass->RenderPass.GetReference());
	LocalRenderPass->RenderPass = INTERNAL_DECORATOR(RHIBeginRenderPass)(Info, Name);
}

void FRHICommandEndRenderPass::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndRenderPass);
	check(LocalRenderPass->RenderPass.GetReference());
	INTERNAL_DECORATOR(RHIEndRenderPass)(LocalRenderPass->RenderPass);
}

void FRHICommandBeginParallelRenderPass::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginParallelRenderPass);
	LocalRenderPass->RenderPass = INTERNAL_DECORATOR(RHIBeginParallelRenderPass)(Info, Name);
}

void FRHICommandEndParallelRenderPass::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndParallelRenderPass);
	INTERNAL_DECORATOR(RHIEndParallelRenderPass)(LocalRenderPass->RenderPass);
}

void FRHICommandBeginRenderSubPass::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginRenderSubPass);
	LocalRenderSubPass->RenderSubPass = INTERNAL_DECORATOR(RHIBeginRenderSubPass)(LocalRenderPass->RenderPass);
}

void FRHICommandEndRenderSubPass::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndRenderSubPass);
	INTERNAL_DECORATOR(RHIEndRenderSubPass)(LocalRenderPass->RenderPass, LocalRenderSubPass->RenderSubPass);
}

void FRHICommandSetRenderTargets::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetRenderTargets);
	INTERNAL_DECORATOR(RHISetRenderTargets)(
		NewNumSimultaneousRenderTargets,
		NewRenderTargetsRHI,
		&NewDepthStencilTarget,
		NewNumUAVs,
		UAVs);
}

void FRHICommandSetRenderTargetsAndClear::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetRenderTargetsAndClear);
	INTERNAL_DECORATOR(RHISetRenderTargetsAndClear)(RenderTargetsInfo);
}

void FRHICommandBindClearMRTValues::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BindClearMRTValues);
	INTERNAL_DECORATOR(RHIBindClearMRTValues)(
		bClearColor,
		bClearDepth,
		bClearStencil		
		);
}

void FRHICommandEndDrawPrimitiveUP::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndDrawPrimitiveUP);
	void* Buffer = NULL;
	INTERNAL_DECORATOR(RHIBeginDrawPrimitiveUP)(PrimitiveType, NumPrimitives, NumVertices, VertexDataStride, Buffer);
	FMemory::Memcpy(Buffer, OutVertexData, NumVertices * VertexDataStride);
	INTERNAL_DECORATOR(RHIEndDrawPrimitiveUP)();
}

void FRHICommandEndDrawIndexedPrimitiveUP::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndDrawIndexedPrimitiveUP);
	void* VertexBuffer = nullptr;
	void* IndexBuffer = nullptr;
	INTERNAL_DECORATOR(RHIBeginDrawIndexedPrimitiveUP)(
		PrimitiveType,
		NumPrimitives,
		NumVertices,
		VertexDataStride,
		VertexBuffer,
		MinVertexIndex,
		NumIndices,
		IndexDataStride,
		IndexBuffer);
	FMemory::Memcpy(VertexBuffer, OutVertexData, NumVertices * VertexDataStride);
	FMemory::Memcpy(IndexBuffer, OutIndexData, NumIndices * IndexDataStride);
	INTERNAL_DECORATOR(RHIEndDrawIndexedPrimitiveUP)();
}

template<ECmdList CmdListType>
void FRHICommandSetComputeShader<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetComputeShader);
	INTERNAL_DECORATOR_CONTEXT(RHISetComputeShader)(ComputeShader);
}
template struct FRHICommandSetComputeShader<ECmdList::EGfx>;
template struct FRHICommandSetComputeShader<ECmdList::ECompute>;

template<ECmdList CmdListType>
void FRHICommandSetComputePipelineState<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetComputePipelineState);
	extern FRHIComputePipelineState* ExecuteSetComputePipelineState(FComputePipelineState* ComputePipelineState);
	FRHIComputePipelineState* RHIComputePipelineState = ExecuteSetComputePipelineState(ComputePipelineState);
	INTERNAL_DECORATOR_CONTEXT(RHISetComputePipelineState)(RHIComputePipelineState);
}
template struct FRHICommandSetComputePipelineState<ECmdList::EGfx>;
template struct FRHICommandSetComputePipelineState<ECmdList::ECompute>;

void FRHICommandSetGraphicsPipelineState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetGraphicsPipelineState);
	extern FRHIGraphicsPipelineState* ExecuteSetGraphicsPipelineState(FGraphicsPipelineState* GraphicsPipelineState);
	FRHIGraphicsPipelineState* RHIGraphicsPipelineState = ExecuteSetGraphicsPipelineState(GraphicsPipelineState);
	INTERNAL_DECORATOR(RHISetGraphicsPipelineState)(RHIGraphicsPipelineState);
}

template<ECmdList CmdListType>
void FRHICommandDispatchComputeShader<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DispatchComputeShader);
	INTERNAL_DECORATOR_CONTEXT(RHIDispatchComputeShader)(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}
template struct FRHICommandDispatchComputeShader<ECmdList::EGfx>;
template struct FRHICommandDispatchComputeShader<ECmdList::ECompute>;

template<ECmdList CmdListType>
void FRHICommandDispatchIndirectComputeShader<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DispatchIndirectComputeShader);
	INTERNAL_DECORATOR_CONTEXT(RHIDispatchIndirectComputeShader)(ArgumentBuffer, ArgumentOffset);
}
template struct FRHICommandDispatchIndirectComputeShader<ECmdList::EGfx>;
template struct FRHICommandDispatchIndirectComputeShader<ECmdList::ECompute>;

void FRHICommandAutomaticCacheFlushAfterComputeShader::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(AutomaticCacheFlushAfterComputeShader);
	INTERNAL_DECORATOR(RHIAutomaticCacheFlushAfterComputeShader)(bEnable);
}

void FRHICommandFlushComputeShaderCache::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(FlushComputeShaderCache);
	INTERNAL_DECORATOR(RHIFlushComputeShaderCache)();
}

void FRHICommandDrawPrimitiveIndirect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawPrimitiveIndirect);
	INTERNAL_DECORATOR(RHIDrawPrimitiveIndirect)(PrimitiveType, ArgumentBuffer, ArgumentOffset);
}

void FRHICommandDrawIndexedIndirect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawIndexedIndirect);
	INTERNAL_DECORATOR(RHIDrawIndexedIndirect)(IndexBufferRHI, PrimitiveType, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances);
}

void FRHICommandDrawIndexedPrimitiveIndirect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawIndexedPrimitiveIndirect);
	INTERNAL_DECORATOR(RHIDrawIndexedPrimitiveIndirect)(PrimitiveType, IndexBuffer, ArgumentsBuffer, ArgumentOffset);
}

void FRHICommandEnableDepthBoundsTest::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EnableDepthBoundsTest);
	INTERNAL_DECORATOR(RHIEnableDepthBoundsTest)(bEnable, MinDepth, MaxDepth);
}

void FRHICommandClearTinyUAV::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(ClearTinyUAV);
	INTERNAL_DECORATOR(RHIClearTinyUAV)(UnorderedAccessViewRHI, Values);
}

void FRHICommandCopyToResolveTarget::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(CopyToResolveTarget);
	INTERNAL_DECORATOR(RHICopyToResolveTarget)(SourceTexture, DestTexture, bKeepOriginalSurface, ResolveParams);
}

void FRHICommandCopyTexture::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(CopyToResolveTarget);
	INTERNAL_DECORATOR(RHICopyTexture)(SourceTexture, DestTexture, ResolveParams);
}

void FRHICommandTransitionTextures::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(TransitionTextures);
	INTERNAL_DECORATOR(RHITransitionResources)(TransitionType, &Textures[0], NumTextures);
}

void FRHICommandTransitionTexturesArray::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(TransitionTextures);
	INTERNAL_DECORATOR(RHITransitionResources)(TransitionType, &Textures[0], Textures.Num());
}

template<ECmdList CmdListType>
void FRHICommandTransitionUAVs<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(TransitionUAVs);
	INTERNAL_DECORATOR_CONTEXT(RHITransitionResources)(TransitionType, TransitionPipeline, UAVs, NumUAVs, WriteFence);
}
template struct FRHICommandTransitionUAVs<ECmdList::EGfx>;
template struct FRHICommandTransitionUAVs<ECmdList::ECompute>;

template<ECmdList CmdListType>
void FRHICommandSetAsyncComputeBudget<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetAsyncComputeBudget);
	INTERNAL_DECORATOR_CONTEXT(RHISetAsyncComputeBudget)(Budget);
}
template struct FRHICommandSetAsyncComputeBudget<ECmdList::EGfx>;
template struct FRHICommandSetAsyncComputeBudget<ECmdList::ECompute>;

template<ECmdList CmdListType>
void FRHICommandWaitComputeFence<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(WaitComputeFence);
	INTERNAL_DECORATOR_CONTEXT(RHIWaitComputeFence)(WaitFence);
}
template struct FRHICommandWaitComputeFence<ECmdList::EGfx>;
template struct FRHICommandWaitComputeFence<ECmdList::ECompute>;

// NVCHANGE_BEGIN: Add HBAO+
#if WITH_GFSDK_SSAO

void FRHICommandRenderHBAO::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(RenderHBAO);
	INTERNAL_DECORATOR(RHIRenderHBAO)(
		SceneDepthTextureRHI,
		ProjectionMatrix,
		SceneNormalTextureRHI,
		ViewMatrix,
		SceneColorTextureRHI,
		AOParams);
}

#endif
// NVCHANGE_END: Add HBAO+

// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI

void FRHIVXGICleanupAfterVoxelization::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(VXGICleanupAfterVoxelization);
	INTERNAL_DECORATOR(RHIVXGICleanupAfterVoxelization)();
}

void FRHISetViewportsAndScissorRects::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetViewportsAndScissorRects);
	INTERNAL_DECORATOR(RHISetViewportsAndScissorRects)(Count, Viewports.GetData(), ScissorRects.GetData());
}

void FRHIDispatchIndirectComputeShaderStructured::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DispatchIndirectComputeShaderStructured);
	INTERNAL_DECORATOR(RHIDispatchIndirectComputeShaderStructured)(ArgumentBuffer, ArgumentOffset);
}

void FRHICopyStructuredBufferData::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(CopyStructuredBufferData);
	INTERNAL_DECORATOR(RHICopyStructuredBufferData)(DestBuffer, DestOffset, SrcBuffer, SrcOffset, DataSize);
}

void FRHIExecuteVxgiRenderingCommand::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(ExecuteVxgiRenderingCommand);
	INTERNAL_DECORATOR(RHIExecuteVxgiRenderingCommand)(Command);
}
#endif
// NVCHANGE_END: Add VXGI

void FRHICommandBuildLocalGraphicsPipelineState::Execute(FRHICommandListBase& CmdList)
{
	LLM_SCOPE(ELLMTag::Shaders);
	RHISTAT(BuildLocalGraphicsPipelineState);
	check(!IsValidRef(WorkArea.ComputedGraphicsPipelineState->GraphicsPipelineState));
	if (WorkArea.ComputedGraphicsPipelineState->UseCount)
	{
		WorkArea.ComputedGraphicsPipelineState->GraphicsPipelineState =
			RHICreateGraphicsPipelineState(WorkArea.Args);
	}
}

void FRHICommandSetLocalGraphicsPipelineState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetLocalGraphicsPipelineState);
	check(LocalGraphicsPipelineState.WorkArea->ComputedGraphicsPipelineState->UseCount > 0 && IsValidRef(LocalGraphicsPipelineState.WorkArea->ComputedGraphicsPipelineState->GraphicsPipelineState)); // this should have been created and should have uses outstanding

	INTERNAL_DECORATOR(RHISetGraphicsPipelineState)(LocalGraphicsPipelineState.WorkArea->ComputedGraphicsPipelineState->GraphicsPipelineState);

	if (--LocalGraphicsPipelineState.WorkArea->ComputedGraphicsPipelineState->UseCount == 0)
	{
		LocalGraphicsPipelineState.WorkArea->ComputedGraphicsPipelineState->~FComputedGraphicsPipelineState();
	}
}

// WaveWorks Begin
void FRHICommandBuildDrawQuadTreeWaveWorks::Execute(FRHICommandListBase& CmdList)
{
	WorkArea.WaveWorks->DrawQuadTree(
		WorkArea.QuadTreeHandle,
		WorkArea.ViewMatrix,
		WorkArea.ProjMatrix,
		WorkArea.ShaderInputMappings
		);	
}
// WaveWorks End

void FRHICommandBuildLocalUniformBuffer::Execute(FRHICommandListBase& CmdList)
{
	LLM_SCOPE(ELLMTag::Shaders);
	RHISTAT(BuildLocalUniformBuffer);
	check(!IsValidRef(WorkArea.ComputedUniformBuffer->UniformBuffer)); // should not already have been created
	check(WorkArea.Layout);
	check(WorkArea.Contents); 
	if (WorkArea.ComputedUniformBuffer->UseCount)
	{
		WorkArea.ComputedUniformBuffer->UniformBuffer = RHICreateUniformBuffer(WorkArea.Contents, *WorkArea.Layout, UniformBuffer_SingleFrame); 
	}
	WorkArea.Layout = nullptr;
	WorkArea.Contents = nullptr;
}

template <typename TShaderRHIParamRef>
void FRHICommandSetLocalUniformBuffer<TShaderRHIParamRef>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetLocalUniformBuffer);
	check(LocalUniformBuffer.WorkArea->ComputedUniformBuffer->UseCount > 0 && IsValidRef(LocalUniformBuffer.WorkArea->ComputedUniformBuffer->UniformBuffer)); // this should have been created and should have uses outstanding
	INTERNAL_DECORATOR(RHISetShaderUniformBuffer)(Shader, BaseIndex, LocalUniformBuffer.WorkArea->ComputedUniformBuffer->UniformBuffer);
	if (--LocalUniformBuffer.WorkArea->ComputedUniformBuffer->UseCount == 0)
	{
		LocalUniformBuffer.WorkArea->ComputedUniformBuffer->~FComputedUniformBuffer();
	}
}
template struct FRHICommandSetLocalUniformBuffer<FVertexShaderRHIParamRef>;
template struct FRHICommandSetLocalUniformBuffer<FHullShaderRHIParamRef>;
template struct FRHICommandSetLocalUniformBuffer<FDomainShaderRHIParamRef>;
template struct FRHICommandSetLocalUniformBuffer<FGeometryShaderRHIParamRef>;
template struct FRHICommandSetLocalUniformBuffer<FPixelShaderRHIParamRef>;
template struct FRHICommandSetLocalUniformBuffer<FComputeShaderRHIParamRef>;

void FRHICommandBeginRenderQuery::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginRenderQuery);
	INTERNAL_DECORATOR(RHIBeginRenderQuery)(RenderQuery);
}

void FRHICommandEndRenderQuery::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndRenderQuery);
	INTERNAL_DECORATOR(RHIEndRenderQuery)(RenderQuery);
}

void FRHICommandBeginOcclusionQueryBatch::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginOcclusionQueryBatch);
	INTERNAL_DECORATOR(RHIBeginOcclusionQueryBatch)();
}

void FRHICommandEndOcclusionQueryBatch::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndOcclusionQueryBatch);
	INTERNAL_DECORATOR(RHIEndOcclusionQueryBatch)();
}

template<ECmdList CmdListType>
void FRHICommandSubmitCommandsHint<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SubmitCommandsHint);
	INTERNAL_DECORATOR_CONTEXT(RHISubmitCommandsHint)();
}
template struct FRHICommandSubmitCommandsHint<ECmdList::EGfx>;
template struct FRHICommandSubmitCommandsHint<ECmdList::ECompute>;

void FRHICommandUpdateTextureReference::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(UpdateTextureReference);
	INTERNAL_DECORATOR(RHIUpdateTextureReference)(TextureRef, NewTexture);
}

void FRHICommandBeginScene::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginScene);
	INTERNAL_DECORATOR(RHIBeginScene)();
}

void FRHICommandEndScene::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndScene);
	INTERNAL_DECORATOR(RHIEndScene)();
}

void FRHICommandBeginFrame::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginFrame);
	INTERNAL_DECORATOR(RHIBeginFrame)();
}

void FRHICommandEndFrame::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndFrame);
	INTERNAL_DECORATOR(RHIEndFrame)();
}

void FRHICommandBeginDrawingViewport::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginDrawingViewport);
	INTERNAL_DECORATOR(RHIBeginDrawingViewport)(Viewport, RenderTargetRHI);
}

void FRHICommandEndDrawingViewport::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndDrawingViewport);
	INTERNAL_DECORATOR(RHIEndDrawingViewport)(Viewport, bPresent, bLockToVsync);
}

template<ECmdList CmdListType>
void FRHICommandPushEvent<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(PushEvent);
	INTERNAL_DECORATOR_CONTEXT(RHIPushEvent)(Name, Color);
}
template struct FRHICommandPushEvent<ECmdList::EGfx>;
template struct FRHICommandPushEvent<ECmdList::ECompute>;

template<ECmdList CmdListType>
void FRHICommandPopEvent<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(PopEvent);
	INTERNAL_DECORATOR_CONTEXT(RHIPopEvent)();
}
template struct FRHICommandPopEvent<ECmdList::EGfx>;
template struct FRHICommandPopEvent<ECmdList::ECompute>;

void FRHICommandInvalidateCachedState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(RHIInvalidateCachedState);
	INTERNAL_DECORATOR(RHIInvalidateCachedState)();
}

// NvFlow begin
void FRHICommandNvFlowWork::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(NvFlowWork);
	INTERNAL_DECORATOR(NvFlowWork)(WorkFunc, ParamData, NumBytes);
}
// NvFlow end

// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
void FRHICommandBeginAccumulation::Execute(FRHICommandListBase& CmdList)
{
	if (GNVVolumetricLightingRHI)
	{
		GNVVolumetricLightingRHI->BeginAccumulation(SceneDepthTextureRHI, ViewerDescs, MediumDesc, DebugFlags);
	}
}

void FRHICommandRenderVolume::Execute(FRHICommandListBase& CmdList)
{
	if (GNVVolumetricLightingRHI)
	{
		GNVVolumetricLightingRHI->RenderVolume(ShadowMapTextures, ShadowMapDesc, LightDesc, VolumeDesc);
	}
}

void FRHICommandEndAccumulation::Execute(FRHICommandListBase& CmdList)
{
	if (GNVVolumetricLightingRHI)
	{
		GNVVolumetricLightingRHI->EndAccumulation();
	}
}

void FRHICommandApplyLighting::Execute(FRHICommandListBase& CmdList)
{
	if (GNVVolumetricLightingRHI)
	{
		GNVVolumetricLightingRHI->ApplyLighting(SceneColorSurfaceRHI, PostprocessDesc);
	}
}

#endif
// NVCHANGE_END: Nvidia Volumetric Lighting