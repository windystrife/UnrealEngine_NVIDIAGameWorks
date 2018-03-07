// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

// Metal RHI public headers.
#include <Metal/Metal.h>
#include "MetalState.h"
#include "MetalResources.h"
#include "MetalViewport.h"

class FMetalContext;

/** The interface RHI command context. */
class FMetalRHICommandContext : public IRHICommandContext
{
public:
	FMetalRHICommandContext(struct FMetalGPUProfiler* InProfiler, FMetalContext* WrapContext);
	virtual ~FMetalRHICommandContext();

	/** Get the internal context */
	FORCEINLINE FMetalContext& GetInternalContext() const { return *Context; }
	
	/** Get the profiler pointer */
	FORCEINLINE struct FMetalGPUProfiler* GetProfiler() const { return Profiler; }
	
	/**
	 *Sets the current compute shader.  Mostly for compliance with platforms
	 *that require shader setting before resource binding.
	 */
	virtual void RHISetComputeShader(FComputeShaderRHIParamRef ComputeShader) override;
	
	virtual void RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState) override;
	
	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) final override;
	
	virtual void RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) final override;
	
	virtual void RHIAutomaticCacheFlushAfterComputeShader(bool bEnable) final override;
	
	virtual void RHIFlushComputeShaderCache() final override;
	
	// Useful when used with geometry shader (emit polygons to different viewports), otherwise SetViewPort() is simpler
	// @param Count >0
	// @param Data must not be 0
	virtual void RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) final override;
	
	/** Clears a UAV to the multi-component value provided. */
	virtual void RHIClearTinyUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values) final override;
	
	/**
	 * Resolves from one texture to another.
	 * @param SourceTexture - texture to resolve from, 0 is silenty ignored
	 * @param DestTexture - texture to resolve to, 0 is silenty ignored
	 * @param bKeepOriginalSurface - true if the original surface will still be used after this function so must remain valid
	 * @param ResolveParams - optional resolve params
	 */
	virtual void RHICopyToResolveTarget(FTextureRHIParamRef SourceTexture, FTextureRHIParamRef DestTexture, bool bKeepOriginalSurface, const FResolveParams& ResolveParams) final override;
	
	virtual void RHIBeginRenderQuery(FRenderQueryRHIParamRef RenderQuery) final override;
	
	virtual void RHIEndRenderQuery(FRenderQueryRHIParamRef RenderQuery) final override;
	
	virtual void RHIBeginOcclusionQueryBatch() final override;
	
	virtual void RHIEndOcclusionQueryBatch() final override;
	
	virtual void RHISubmitCommandsHint() override;
	
	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIBeginDrawingViewport(FViewportRHIParamRef Viewport, FTextureRHIParamRef RenderTargetRHI) override;
	
	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIEndDrawingViewport(FViewportRHIParamRef Viewport, bool bPresent, bool bLockToVsync) override;
	
	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIBeginFrame() override;
	
	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIEndFrame() override;
	
	/**
		* Signals the beginning of scene rendering. The RHI makes certain caching assumptions between
		* calls to BeginScene/EndScene. Currently the only restriction is that you can't update texture
		* references.
		*/
	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIBeginScene() override;
	
	/**
		* Signals the end of scene rendering. See RHIBeginScene.
		*/
	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIEndScene() override;
	
	virtual void RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint32 Offset) final override;
	virtual void RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Offset) final override;

	virtual void RHISetRasterizerState(FRasterizerStateRHIParamRef NewState) final override;
	
	// @param MinX including like Win32 RECT
	// @param MinY including like Win32 RECT
	// @param MaxX excluding like Win32 RECT
	// @param MaxY excluding like Win32 RECT
	virtual void RHISetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ) final override;

	virtual void RHISetStereoViewport(uint32 LeftMinX, uint32 RightMinX, uint32 LeftMinY, uint32 RightMinY, float MinZ, uint32 LeftMaxX, uint32 RightMaxX, uint32 LeftMaxY, uint32 RightMaxY, float MaxZ) final override;
	
	// @param MinX including like Win32 RECT
	// @param MinY including like Win32 RECT
	// @param MaxX excluding like Win32 RECT
	// @param MaxY excluding like Win32 RECT
	virtual void RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY) final override;
	
	/**
	 * Set bound shader state. This will set the vertex decl/shader, and pixel shader
	 * @param BoundShaderState - state resource
	 */
	virtual void RHISetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderState) final override;
	
	virtual void RHISetGraphicsPipelineState(FGraphicsPipelineStateRHIParamRef GraphicsState) final override;
	
	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FVertexShaderRHIParamRef VertexShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	
	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FHullShaderRHIParamRef HullShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	
	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FDomainShaderRHIParamRef DomainShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	
	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	
	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FPixelShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	
	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FComputeShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	
	/**
	 * Sets sampler state.
	 * @param GeometryShader	The geometry shader to set the sampler for.
	 * @param SamplerIndex		The index of the sampler.
	 * @param NewState			The new sampler state.
	 */
	virtual void RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	
	/**
	 * Sets sampler state.
	 * @param GeometryShader	The geometry shader to set the sampler for.
	 * @param SamplerIndex		The index of the sampler.
	 * @param NewState			The new sampler state.
	 */
	virtual void RHISetShaderSampler(FVertexShaderRHIParamRef VertexShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	
	/**
	 * Sets sampler state.
	 * @param GeometryShader	The geometry shader to set the sampler for.
	 * @param SamplerIndex		The index of the sampler.
	 * @param NewState			The new sampler state.
	 */
	virtual void RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	
	/**
	 * Sets sampler state.
	 * @param GeometryShader	The geometry shader to set the sampler for.
	 * @param SamplerIndex		The index of the sampler.
	 * @param NewState			The new sampler state.
	 */
	virtual void RHISetShaderSampler(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	
	/**
	 * Sets sampler state.
	 * @param GeometryShader	The geometry shader to set the sampler for.
	 * @param SamplerIndex		The index of the sampler.
	 * @param NewState			The new sampler state.
	 */
	virtual void RHISetShaderSampler(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	
	/**
	 * Sets sampler state.
	 * @param GeometryShader	The geometry shader to set the sampler for.
	 * @param SamplerIndex		The index of the sampler.
	 * @param NewState			The new sampler state.
	 */
	virtual void RHISetShaderSampler(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	
	/** Sets a compute shader UAV parameter. */
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV) final override;
	
	/** Sets a compute shader UAV parameter and initial count */
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount) final override;
	
	virtual void RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	
	virtual void RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	
	virtual void RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	
	virtual void RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	
	virtual void RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	
	virtual void RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	
	virtual void RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	
	virtual void RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	
	virtual void RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	
	virtual void RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	
	virtual void RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	
	virtual void RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	
	virtual void RHISetShaderParameter(FVertexShaderRHIParamRef VertexShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	
	virtual void RHISetShaderParameter(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	
	virtual void RHISetShaderParameter(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	
	virtual void RHISetShaderParameter(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	
	virtual void RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	
	virtual void RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	
	virtual void RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewState, uint32 StencilRef) final override;

	virtual void RHISetStencilRef(uint32 StencilRef) final override;

	// Allows to set the blend state, parameter can be created with RHICreateBlendState()
	virtual void RHISetBlendState(FBlendStateRHIParamRef NewState, const FLinearColor& BlendFactor) final override;
	
	virtual void RHISetBlendFactor(const FLinearColor& BlendFactor) final override;

	virtual void RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs) final override;
	
	virtual void RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo) final override;
	
	virtual void RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	
	virtual void RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) final override;
	
	virtual void RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances) final override;
	
	// @param NumPrimitives need to be >0
	virtual void RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBuffer, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	
	virtual void RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType, FIndexBufferRHIParamRef IndexBuffer, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) final override;
	
	/**
	 * Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawPrimitiveUP
	 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
	 * @param NumPrimitives The number of primitives in the VertexData buffer
	 * @param NumVertices The number of vertices to be written
	 * @param VertexDataStride Size of each vertex
	 * @param OutVertexData Reference to the allocated vertex memory
	 */
	virtual void RHIBeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData) final override;
	
	/**
	 * Draw a primitive using the vertex data populated since RHIBeginDrawPrimitiveUP and clean up any memory as needed
	 */
	virtual void RHIEndDrawPrimitiveUP() final override;
	
	/**
	 * Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawIndexedPrimitiveUP
	 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
	 * @param NumPrimitives The number of primitives in the VertexData buffer
	 * @param NumVertices The number of vertices to be written
	 * @param VertexDataStride Size of each vertex
	 * @param OutVertexData Reference to the allocated vertex memory
	 * @param MinVertexIndex The lowest vertex index used by the index buffer
	 * @param NumIndices Number of indices to be written
	 * @param IndexDataStride Size of each index (either 2 or 4 bytes)
	 * @param OutIndexData Reference to the allocated index memory
	 */
	virtual void RHIBeginDrawIndexedPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData) final override;
	
	/**
	 * Draw a primitive using the vertex and index data populated since RHIBeginDrawIndexedPrimitiveUP and clean up any memory as needed
	 */
	virtual void RHIEndDrawIndexedPrimitiveUP() final override;
	
	/**
	 * Enabled/Disables Depth Bounds Testing with the given min/max depth.
	 * @param bEnable	Enable(non-zero)/disable(zero) the depth bounds test
	 * @param MinDepth	The minimum depth for depth bounds test
	 * @param MaxDepth	The maximum depth for depth bounds test.
	 *					The valid values for fMinDepth and fMaxDepth are such that 0 <= fMinDepth <= fMaxDepth <= 1
	 */
	virtual void RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth) final override;
	
	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) final override;
	
	virtual void RHIPopEvent() final override;
	
	virtual void RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture) final override;
	
	/**
	 * Explicitly transition a UAV from readable -> writable by the GPU or vice versa.
	 * Also explicitly states which pipeline the UAV can be used on next.  For example, if a Compute job just wrote this UAV for a Pixel shader to read
	 * you would do EResourceTransitionAccess::Readable and EResourceTransitionPipeline::EComputeToGfx
	 *
	 * @param TransitionType - direction of the transition
	 * @param EResourceTransitionPipeline - How this UAV is transitioning between Gfx and Compute, if at all.
	 * @param InUAVs - array of UAV objects to transition
	 * @param NumUAVs - number of UAVs to transition
	 * @param WriteComputeFence - Optional ComputeFence to write as part of this transition
	 */
	virtual void RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFence) final override;
	
	/**
	 * Compute queue will wait for the fence to be written before continuing.
	 */
	virtual void RHIWaitComputeFence(FComputeFenceRHIParamRef InFence) final override;

	
protected:
	static TGlobalResource<TBoundShaderStateHistory<10000>> BoundShaderStateHistory;
	
	/** Context implementation details. */
	FMetalContext* Context;
	
	/** Occlusion query batch fence */
	TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe> CommandBufferFence;
	
	/** Profiling implementation details. */
	struct FMetalGPUProfiler* Profiler;
	
	/** Some local variables to track the pending primitive information used in RHIEnd*UP functions */
	uint32 PendingVertexBufferOffset;
	uint32 PendingVertexDataStride;
	
	uint32 PendingIndexBufferOffset;
	uint32 PendingIndexDataStride;
	
	uint32 PendingPrimitiveType;
	uint32 PendingNumPrimitives;

private:
	void RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil);
};

class FMetalRHIComputeContext : public FMetalRHICommandContext
{
public:
	FMetalRHIComputeContext(struct FMetalGPUProfiler* InProfiler, FMetalContext* WrapContext);
	virtual ~FMetalRHIComputeContext();
	
	virtual void RHISetAsyncComputeBudget(EAsyncComputeBudget Budget) final override;
	virtual void RHISetComputeShader(FComputeShaderRHIParamRef ComputeShader) final override;
	virtual void RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState) final override;
	virtual void RHISubmitCommandsHint() final override;
};

class FMetalRHIImmediateCommandContext : public FMetalRHICommandContext
{
public:
	FMetalRHIImmediateCommandContext(struct FMetalGPUProfiler* InProfiler, FMetalContext* WrapContext);

	// FRHICommandContext API accessible only on the immediate device context
	virtual void RHIBeginDrawingViewport(FViewportRHIParamRef Viewport, FTextureRHIParamRef RenderTargetRHI) final override;
	virtual void RHIEndDrawingViewport(FViewportRHIParamRef Viewport, bool bPresent, bool bLockToVsync) final override;
	virtual void RHIBeginFrame() final override;
	virtual void RHIEndFrame() final override;
	virtual void RHIBeginScene() final override;
	virtual void RHIEndScene() final override;
	
protected:
	friend class FMetalDynamicRHI;
};
