// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHIContext.h: Interface for RHI Contexts
=============================================================================*/

#pragma once

#include "Misc/AssertionMacros.h"
#include "Math/Color.h"
#include "Math/IntPoint.h"
#include "Math/IntRect.h"
#include "Math/Box2D.h"
#include "Math/PerspectiveMatrix.h"
#include "Math/TranslationMatrix.h"
#include "Math/ScaleMatrix.h"
#include "Math/Float16Color.h"
#include "Modules/ModuleInterface.h"
// NvFlow begin
#include "GameWorks/RHINvFlow.h"
// NvFlow end

class FRHIDepthRenderTargetView;
class FRHIRenderTargetView;
class FRHISetRenderTargetsInfo;
struct FResolveParams;
struct FViewportBounds;
enum class EAsyncComputeBudget;
enum class EResourceTransitionAccess;
enum class EResourceTransitionPipeline;


FORCEINLINE FBoundShaderStateRHIRef RHICreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclaration,
	FVertexShaderRHIParamRef VertexShader,
	FHullShaderRHIParamRef HullShader,
	FDomainShaderRHIParamRef DomainShader,
	FPixelShaderRHIParamRef PixelShader,
	FGeometryShaderRHIParamRef GeometryShader
);

/** Context that is capable of doing Compute work.  Can be async or compute on the gfx pipe. */
class IRHIComputeContext
{
public:
	/**
	* Compute queue will wait for the fence to be written before continuing.
	*/
	virtual void RHIWaitComputeFence(FComputeFenceRHIParamRef InFence) = 0;

	/**
	*Sets the current compute shader.
	*/
	virtual void RHISetComputeShader(FComputeShaderRHIParamRef ComputeShader) = 0;

	virtual void RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState)
	{
		if (ComputePipelineState)
		{
			FRHIComputePipelineStateFallback* FallbackState = static_cast<FRHIComputePipelineStateFallback*>(ComputePipelineState);
			RHISetComputeShader(FallbackState->GetComputeShader());
		}
	}

	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) = 0;

	virtual void RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) = 0;

	virtual void RHISetAsyncComputeBudget(EAsyncComputeBudget Budget) = 0;

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
	virtual void RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFence) = 0;

	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FComputeShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) = 0;

	/**
	* Sets sampler state.
	* @param GeometryShader	The geometry shader to set the sampler for.
	* @param SamplerIndex		The index of the sampler.
	* @param NewState			The new sampler state.
	*/
	virtual void RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) = 0;

	/**
	* Sets a compute shader UAV parameter.
	* @param ComputeShader	The compute shader to set the UAV for.
	* @param UAVIndex		The index of the UAVIndex.
	* @param UAV			The new UAV.
	*/
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV) = 0;

	/**
	* Sets a compute shader counted UAV parameter and initial count
	* @param ComputeShader	The compute shader to set the UAV for.
	* @param UAVIndex		The index of the UAVIndex.
	* @param UAV			The new UAV.
	* @param InitialCount	The initial number of items in the UAV.
	*/
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount) = 0;

	virtual void RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) = 0;

	virtual void RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) = 0;

	virtual void RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) = 0;

	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) = 0;

	virtual void RHIPopEvent() = 0;

	/**
	* Submit the current command buffer to the GPU if possible.
	*/
	virtual void RHISubmitCommandsHint() = 0;

	/**
	 * Some RHI implementations (OpenGL) cache render state internally
	 * Signal to RHI that cached state is no longer valid
	 */
	virtual void RHIInvalidateCachedState() {};
};

// These states are now set by the Pipeline State Object and are now deprecated
class IRHIDeprecatedContext
{
public:
	/**
	* Set bound shader state. This will set the vertex decl/shader, and pixel shader
	* @param BoundShaderState - state resource
	*/
	virtual void RHISetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderState) = 0;

	virtual void RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewState, uint32 StencilRef) = 0;

	virtual void RHISetRasterizerState(FRasterizerStateRHIParamRef NewState) = 0;

	// Allows to set the blend state, parameter can be created with RHICreateBlendState()
	virtual void RHISetBlendState(FBlendStateRHIParamRef NewState, const FLinearColor& BlendFactor) = 0;
};

/** The interface RHI command context. Sometimes the RHI handles these. On platforms that can processes command lists in parallel, it is a separate object. */
class IRHICommandContext : public IRHIComputeContext, public IRHIDeprecatedContext
{
public:
	virtual ~IRHICommandContext()
	{
	}

	/**
	* Compute queue will wait for the fence to be written before continuing.
	*/
	virtual void RHIWaitComputeFence(FComputeFenceRHIParamRef InFence) override
	{
		if (InFence)
		{
			checkf(InFence->GetWriteEnqueued(), TEXT("ComputeFence: %s waited on before being written. This will hang the GPU."), *InFence->GetName().ToString());
		}
	}

	/**
	*Sets the current compute shader.  Mostly for compliance with platforms
	*that require shader setting before resource binding.
	*/
	virtual void RHISetComputeShader(FComputeShaderRHIParamRef ComputeShader) = 0;

	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) = 0;

	virtual void RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) = 0;

	virtual void RHISetAsyncComputeBudget(EAsyncComputeBudget Budget) override
	{
	}

	virtual void RHIAutomaticCacheFlushAfterComputeShader(bool bEnable) = 0;

	virtual void RHIFlushComputeShaderCache() = 0;

	// Useful when used with geometry shader (emit polygons to different viewports), otherwise SetViewPort() is simpler
	// @param Count >0
	// @param Data must not be 0
	virtual void RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) = 0;

	/** Clears a UAV to the multi-component value provided. */
	virtual void RHIClearTinyUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values) = 0;

	/**
	* Resolves from one texture to another.
	* @param SourceTexture - texture to resolve from, 0 is silenty ignored
	* @param DestTexture - texture to resolve to, 0 is silenty ignored
	* @param bKeepOriginalSurface - true if the original surface will still be used after this function so must remain valid
	* @param ResolveParams - optional resolve params
	*/
	virtual void RHICopyToResolveTarget(FTextureRHIParamRef SourceTexture, FTextureRHIParamRef DestTexture, bool bKeepOriginalSurface, const FResolveParams& ResolveParams) = 0;

	/**
	* Explicitly transition a texture resource from readable -> writable by the GPU or vice versa.
	* We know rendertargets are only used as rendered targets on the Gfx pipeline, so these transitions are assumed to be implemented such
	* Gfx->Gfx and Gfx->Compute pipeline transitions are both handled by this call by the RHI implementation.  Hence, no pipeline parameter on this call.
	*
	* @param TransitionType - direction of the transition
	* @param InTextures - array of texture objects to transition
	* @param NumTextures - number of textures to transition
	*/
	virtual void RHITransitionResources(EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InTextures, int32 NumTextures)
	{
		if (TransitionType == EResourceTransitionAccess::EReadable)
		{
			const FResolveParams ResolveParams;
			for (int32 i = 0; i < NumTextures; ++i)
			{
				RHICopyToResolveTarget(InTextures[i], InTextures[i], true, ResolveParams);
			}
		}
	}

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
	virtual void RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFence)
	{
		if (WriteComputeFence)
		{
			WriteComputeFence->WriteFence();
		}
	}

	void RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs)
	{
		RHITransitionResources(TransitionType, TransitionPipeline, InUAVs, NumUAVs, nullptr);
	}


	virtual void RHIBeginRenderQuery(FRenderQueryRHIParamRef RenderQuery) = 0;

	virtual void RHIEndRenderQuery(FRenderQueryRHIParamRef RenderQuery) = 0;

	virtual void RHIBeginOcclusionQueryBatch() = 0;

	virtual void RHIEndOcclusionQueryBatch() = 0;

	virtual void RHISubmitCommandsHint() = 0;

	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIBeginDrawingViewport(FViewportRHIParamRef Viewport, FTextureRHIParamRef RenderTargetRHI) = 0;

	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIEndDrawingViewport(FViewportRHIParamRef Viewport, bool bPresent, bool bLockToVsync) = 0;

	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIBeginFrame() = 0;

	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIEndFrame() = 0;

	/**
	* Signals the beginning of scene rendering. The RHI makes certain caching assumptions between
	* calls to BeginScene/EndScene. Currently the only restriction is that you can't update texture
	* references.
	*/
	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIBeginScene() = 0;

	/**
	* Signals the end of scene rendering. See RHIBeginScene.
	*/
	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIEndScene() = 0;

	/**
	* Signals the beginning and ending of rendering to a resource to be used in the next frame on a multiGPU system
	*/
	virtual void RHIBeginUpdateMultiFrameResource(FTextureRHIParamRef Texture)
	{
		/* empty default implementation */
	}

	virtual void RHIEndUpdateMultiFrameResource(FTextureRHIParamRef Texture)
	{
		/* empty default implementation */
	}

	virtual void RHIBeginUpdateMultiFrameResource(FUnorderedAccessViewRHIParamRef UAV)
	{
		/* empty default implementation */
	}

	virtual void RHIEndUpdateMultiFrameResource(FUnorderedAccessViewRHIParamRef UAV)
	{
		/* empty default implementation */
	}

	virtual void RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint32 Offset) = 0;
	
	virtual void RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Offset) = 0;

	// @param MinX including like Win32 RECT
	// @param MinY including like Win32 RECT
	// @param MaxX excluding like Win32 RECT
	// @param MaxY excluding like Win32 RECT
	virtual void RHISetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ) = 0;

	virtual void RHISetStereoViewport(uint32 LeftMinX, uint32 RightMinX, uint32 LeftMinY, uint32 RightMinY, float MinZ, uint32 LeftMaxX, uint32 RightMaxX, uint32 LeftMaxY, uint32 RightMaxY, float MaxZ)
	{
		/* empty default implementation */
	}

	// @param MinX including like Win32 RECT
	// @param MinY including like Win32 RECT
	// @param MaxX excluding like Win32 RECT
	// @param MaxY excluding like Win32 RECT
	virtual void RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY) = 0;

	/**
	* This will set most relevant pipeline state. Legacy APIs are expected to set corresponding disjoint state as well.
	* @param GraphicsShaderState - the graphics pipeline state
	* This implementation is only in place while we transition/refactor.
	*/
	virtual void RHISetGraphicsPipelineState(FGraphicsPipelineStateRHIParamRef GraphicsState)
	{
		FRHIGraphicsPipelineStateFallBack* FallbackGraphicsState = static_cast<FRHIGraphicsPipelineStateFallBack*>(GraphicsState);

		auto& PsoInit = FallbackGraphicsState->Initializer;

		RHISetBoundShaderState(
			RHICreateBoundShaderState(
				PsoInit.BoundShaderState.VertexDeclarationRHI,
				PsoInit.BoundShaderState.VertexShaderRHI,
				PsoInit.BoundShaderState.HullShaderRHI,
				PsoInit.BoundShaderState.DomainShaderRHI,
				PsoInit.BoundShaderState.PixelShaderRHI,
				PsoInit.BoundShaderState.GeometryShaderRHI
			).GetReference()
		);

		RHISetDepthStencilState(FallbackGraphicsState->Initializer.DepthStencilState, 0);
		RHISetRasterizerState(FallbackGraphicsState->Initializer.RasterizerState);
		RHISetBlendState(FallbackGraphicsState->Initializer.BlendState, FLinearColor(1.0f, 1.0f, 1.0f));
	}

	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FVertexShaderRHIParamRef VertexShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) = 0;

	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FHullShaderRHIParamRef HullShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) = 0;

	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FDomainShaderRHIParamRef DomainShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) = 0;

	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) = 0;

	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FPixelShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) = 0;

	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FComputeShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) = 0;

	/**
	* Sets sampler state.
	* @param GeometryShader	The geometry shader to set the sampler for.
	* @param SamplerIndex		The index of the sampler.
	* @param NewState			The new sampler state.
	*/
	virtual void RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) = 0;

	/**
	* Sets sampler state.
	* @param GeometryShader	The geometry shader to set the sampler for.
	* @param SamplerIndex		The index of the sampler.
	* @param NewState			The new sampler state.
	*/
	virtual void RHISetShaderSampler(FVertexShaderRHIParamRef VertexShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) = 0;

	/**
	* Sets sampler state.
	* @param GeometryShader	The geometry shader to set the sampler for.
	* @param SamplerIndex		The index of the sampler.
	* @param NewState			The new sampler state.
	*/
	virtual void RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) = 0;

	/**
	* Sets sampler state.
	* @param GeometryShader	The geometry shader to set the sampler for.
	* @param SamplerIndex		The index of the sampler.
	* @param NewState			The new sampler state.
	*/
	virtual void RHISetShaderSampler(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) = 0;

	/**
	* Sets sampler state.
	* @param GeometryShader	The geometry shader to set the sampler for.
	* @param SamplerIndex		The index of the sampler.
	* @param NewState			The new sampler state.
	*/
	virtual void RHISetShaderSampler(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) = 0;

	/**
	* Sets sampler state.
	* @param GeometryShader	The geometry shader to set the sampler for.
	* @param SamplerIndex		The index of the sampler.
	* @param NewState			The new sampler state.
	*/
	virtual void RHISetShaderSampler(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) = 0;

	/**
	* Sets a compute shader UAV parameter.
	* @param ComputeShader	The compute shader to set the UAV for.
	* @param UAVIndex		The index of the UAVIndex.
	* @param UAV			The new UAV.
	*/
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV) = 0;

	/**
	* Sets a compute shader counted UAV parameter and initial count
	* @param ComputeShader	The compute shader to set the UAV for.
	* @param UAVIndex		The index of the UAVIndex.
	* @param UAV			The new UAV.
	* @param InitialCount	The initial number of items in the UAV.
	*/
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount) = 0;

	virtual void RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) = 0;

	virtual void RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) = 0;

	virtual void RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) = 0;

	virtual void RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) = 0;

	virtual void RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) = 0;

	virtual void RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) = 0;

	virtual void RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) = 0;

	virtual void RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) = 0;

	virtual void RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) = 0;

	virtual void RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) = 0;

	virtual void RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) = 0;

	virtual void RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) = 0;

	virtual void RHISetShaderParameter(FVertexShaderRHIParamRef VertexShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) = 0;

	virtual void RHISetShaderParameter(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) = 0;

	virtual void RHISetShaderParameter(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) = 0;

	virtual void RHISetShaderParameter(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) = 0;

	virtual void RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) = 0;

	virtual void RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) = 0;

	virtual void RHISetStencilRef(uint32 StencilRef) {}

	virtual void RHISetBlendFactor(const FLinearColor& BlendFactor) {}

	virtual void RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs) = 0;

	virtual void RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo) = 0;

	// Bind the clear state of the currently set rendertargets.  This is used by platforms which
	// need the state of the target when finalizing a hardware clear or a resource transition to SRV
	// The explicit bind is needed to support parallel rendering (propagate state between contexts).
	virtual void RHIBindClearMRTValues(bool bClearColor, bool bClearDepth, bool bClearStencil) {}

	virtual void RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances) = 0;

	virtual void RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) = 0;

	virtual void RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances) = 0;

	// @param NumPrimitives need to be >0 
	virtual void RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBuffer, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) = 0;

	virtual void RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType, FIndexBufferRHIParamRef IndexBuffer, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) = 0;

	/**
	* Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawPrimitiveUP
	* @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
	* @param NumPrimitives The number of primitives in the VertexData buffer
	* @param NumVertices The number of vertices to be written
	* @param VertexDataStride Size of each vertex
	* @param OutVertexData Reference to the allocated vertex memory
	*/
	virtual void RHIBeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData) = 0;

	/**
	* Draw a primitive using the vertex data populated since RHIBeginDrawPrimitiveUP and clean up any memory as needed
	*/
	virtual void RHIEndDrawPrimitiveUP() = 0;

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
	virtual void RHIBeginDrawIndexedPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData) = 0;

	/**
	* Draw a primitive using the vertex and index data populated since RHIBeginDrawIndexedPrimitiveUP and clean up any memory as needed
	*/
	virtual void RHIEndDrawIndexedPrimitiveUP() = 0;

	/**
	* Enabled/Disables Depth Bounds Testing with the given min/max depth.
	* @param bEnable	Enable(non-zero)/disable(zero) the depth bounds test
	* @param MinDepth	The minimum depth for depth bounds test
	* @param MaxDepth	The maximum depth for depth bounds test.
	*					The valid values for fMinDepth and fMaxDepth are such that 0 <= fMinDepth <= fMaxDepth <= 1
	*/
	virtual void RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth) = 0;

	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) = 0;

	virtual void RHIPopEvent() = 0;

	virtual void RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture) = 0;

	// NvFlow begin
	virtual void NvFlowGetDeviceDesc(FRHINvFlowDeviceDesc* desc) {}
	virtual void NvFlowGetDepthStencilViewDesc(FTexture2DRHIParamRef depthSurface, FTexture2DRHIParamRef depthTexture, FRHINvFlowDepthStencilViewDesc* desc) {}
	virtual void NvFlowGetRenderTargetViewDesc(FRHINvFlowRenderTargetViewDesc* desc) {}
	virtual FShaderResourceViewRHIRef NvFlowCreateSRV(const FRHINvFlowResourceViewDesc* desc) { return FShaderResourceViewRHIRef(); }
	virtual FRHINvFlowResourceRW* NvFlowCreateResourceRW(const FRHINvFlowResourceRWViewDesc* desc, FShaderResourceViewRHIRef* pRHIRefSRV, FUnorderedAccessViewRHIRef* pRHIRefUAV) { return nullptr; }
	virtual void NvFlowReleaseResourceRW(FRHINvFlowResourceRW*) {}
	virtual void NvFlowReserveDescriptors(FRHINvFlowDescriptorReserveHandle* dstHandle, uint32 numDescriptors, uint64 lastFenceCompleted, uint64 nextFenceValue) {}
	
	virtual void NvFlowRestoreState() {}
	FRHINvFlowCleanup NvFlowCleanup;
	virtual void NvFlowWork(void(*workFunc)(void*,SIZE_T,IRHICommandContext*), void* paramData, SIZE_T numBytes)
	{
		if (workFunc)
		{
			workFunc(paramData, numBytes, this);
		}
	}
	// NvFlow end	
	
	virtual TRefCountPtr<FRHIRenderPass> RHIBeginRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* InName)
	{
		// Fallback...
		InInfo.Validate();

		FRHISetRenderTargetsInfo RTInfo;
		InInfo.ConvertToRenderTargetsInfo(RTInfo);
		FRHIRenderPassFallback* RenderPass = new FRHIRenderPassFallback(InInfo, InName);
		RHISetRenderTargetsAndClear(RTInfo);
		return RenderPass;
	}

	virtual void RHIEndRenderPass(FRHIRenderPass* RenderPass)
	{
		FRHIRenderPassFallback* Fallback = (FRHIRenderPassFallback*)RenderPass;
		Fallback->SetEnded();
	}

	virtual TRefCountPtr<FRHIParallelRenderPass> RHIBeginParallelRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* InName)
	{
		// Fallback...
		InInfo.Validate();

		FRHISetRenderTargetsInfo RTInfo;
		InInfo.ConvertToRenderTargetsInfo(RTInfo);
		RHISetRenderTargetsAndClear(RTInfo);
		return new FRHIParallelRenderPassFallback(InInfo, InName);
	}

	virtual void RHIEndParallelRenderPass(FRHIParallelRenderPass* RenderPass)
	{
		FRHIParallelRenderPassFallback* PassFallback = (FRHIParallelRenderPassFallback*)RenderPass;
		PassFallback->SetEnded();
	}

	virtual TRefCountPtr<FRHIRenderSubPass> RHIBeginRenderSubPass(FRHIParallelRenderPass* RenderPass)
	{
		FRHIParallelRenderPassFallback* Fallback = (FRHIParallelRenderPassFallback*)RenderPass;
		return new FRHIRenderSubPassFallback(Fallback);
	}

	virtual void RHIEndRenderSubPass(FRHIParallelRenderPass* RenderPass, FRHIRenderSubPass* RenderSubPass)
	{
		FRHIParallelRenderPassFallback* PassFallback = (FRHIParallelRenderPassFallback*)RenderPass;
		FRHIRenderSubPassFallback* SubPassFallback = (FRHIRenderSubPassFallback*)RenderSubPass;
		check(SubPassFallback->GetParent() == RenderPass);
		SubPassFallback->SetEnded();
	}

	virtual void RHICopyTexture(FTextureRHIParamRef SourceTexture, FTextureRHIParamRef DestTexture, const FResolveParams& ResolveParams)
	{
		RHICopyToResolveTarget(SourceTexture, DestTexture, true, ResolveParams);
	}
	
	// WaveWorks Start
	virtual const TArray<WaveWorksShaderInput>* RHIGetWaveWorksShaderInput() { return nullptr; }
	virtual const TArray<WaveWorksShaderInput>* RHIGetWaveWorksQuadTreeShaderInput() { return nullptr; }
	virtual FWaveWorksRHIRef RHICreateWaveWorks(const struct GFSDK_WaveWorks_Simulation_Settings& Settings, const struct GFSDK_WaveWorks_Simulation_Params& Params) { return nullptr; }
	virtual void RHISetWaveWorksState(FWaveWorksRHIParamRef State, const FMatrix& ViewMatrix, const TArray<uint32>& ShaderInputMappings) {}
	// WaveWorks End

	// NVCHANGE_BEGIN: Add HBAO+
#if WITH_GFSDK_SSAO
	virtual void RHIRenderHBAO(
		const FTextureRHIParamRef SceneDepthTextureRHI,
		const FMatrix& ProjectionMatrix,
		const FTextureRHIParamRef SceneNormalTextureRHI,
		const FMatrix& ViewMatrix,
		const FTextureRHIParamRef SceneColorTextureRHI,
		const GFSDK_SSAO_Parameters& AOParams)
	{
		checkNoEntry();
	}
#endif
	// NVCHANGE_END: Add HBAO+

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	virtual void RHIVXGICleanupAfterVoxelization() { checkNoEntry(); }

	virtual void RHISetViewportsAndScissorRects(uint32 Count, const FViewportBounds* Viewports, const FScissorRect* ScissorRects) { checkNoEntry(); }
	virtual void RHIDispatchIndirectComputeShaderStructured(FStructuredBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) { checkNoEntry(); }
	virtual void RHICopyStructuredBufferData(FStructuredBufferRHIParamRef DestBuffer, uint32 DestOffset, FStructuredBufferRHIParamRef SrcBuffer, uint32 SrcOffset, uint32 DataSize) { checkNoEntry(); }
	virtual void RHIExecuteVxgiRenderingCommand(NVRHI::IRenderThreadCommand* pCommand) { checkNoEntry(); }
#endif
	// NVCHANGE_END: Add VXGI
};
