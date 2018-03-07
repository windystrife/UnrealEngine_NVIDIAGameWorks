// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>
#include "MetalBufferPools.h"
#include "MetalDebugCommandEncoder.h"
#include "MetalFence.h"

class FMetalCommandList;
class FMetalCommandQueue;
struct MTLCommandBufferRef;
struct FMetalCommandBufferFence;

/**
 * Enumeration for submission hints to avoid unclear bool values.
 */
enum EMetalSubmitFlags
{
	/** No submission flags. */
	EMetalSubmitFlagsNone = 0,
	/** Create the next command buffer. */
	EMetalSubmitFlagsCreateCommandBuffer = 1 << 0,
	/** Wait on the submitted command buffer. */
	EMetalSubmitFlagsWaitOnCommandBuffer = 1 << 1,
	/** Break a single logical command-buffer into parts to keep the GPU active. */
	EMetalSubmitFlagsBreakCommandBuffer = 1 << 2,
	/** Submit the prologue command-buffer only, leave the current command-buffer active.  */
	EMetalSubmitFlagsAsyncCommandBuffer = 1 << 3,
};

/**
 * FMetalCommandEncoder:
 *	Wraps the details of switching between different command encoders on the command-buffer, allowing for restoration of the render encoder if needed.
 * 	UE4 expects the API to serialise commands in-order, but Metal expects applications to work with command-buffers directly so we need to implement 
 *	the RHI semantics by switching between encoder types. This class hides the ugly details. Eventually it might be possible to move some of the operations
 *	into pre- & post- command-buffers so that we avoid encoder switches but that will take changes to the RHI and high-level code too, so it won't happen soon.
 */
class FMetalCommandEncoder
{
public:
#pragma mark - Public C++ Boilerplate -

	/** Default constructor */
	FMetalCommandEncoder(FMetalCommandList& CmdList);
	
	/** Destructor */
	~FMetalCommandEncoder(void);
	
	/** Reset cached state for reuse */
	void Reset(void);
	
#pragma mark - Public Command Buffer Mutators -

	/**
	 * Start encoding to CommandBuffer. It is an error to call this with any outstanding command encoders or current command buffer.
	 * Instead call EndEncoding & CommitCommandBuffer before calling this.
	 */
	void StartCommandBuffer(void);
	
	/**
	 * Commit the existing command buffer if there is one & optionally waiting for completion, if there isn't a current command buffer this is a no-op.
	 * @param Flags Flags to control commit behaviour.
 	 */
	void CommitCommandBuffer(uint32 const Flags);

#pragma mark - Public Command Buffer Accessors -
	
	/** @returns the current command buffer */
	id<MTLCommandBuffer> GetCommandBuffer() const { return CommandBuffer; }

#pragma mark - Public Command Encoder Accessors -
	
	/** @returns True if and only if there is an active render command encoder, otherwise false. */
	bool IsRenderCommandEncoderActive(void) const;
	
	/** @returns True if and only if there is an active compute command encoder, otherwise false. */
	bool IsComputeCommandEncoderActive(void) const;
	
	/** @returns True if and only if there is an active blit command encoder, otherwise false. */
	bool IsBlitCommandEncoderActive(void) const;
	
	/**
	 * True iff the command-encoder submits immediately to the command-queue, false if it performs any buffering.
	 * @returns True iff the command-list submits immediately to the command-queue, false if it performs any buffering.
	 */
	bool IsImmediate(void) const;

	/** @returns True if and only if there is valid render pass descriptor set on the encoder, otherwise false. */
	bool IsRenderPassDescriptorValid(void) const;
	
	/** @returns The active render command encoder or nil if there isn't one. */
	id<MTLRenderCommandEncoder> GetRenderCommandEncoder(void) const;
	
	/** @returns The active compute command encoder or nil if there isn't one. */
	id<MTLComputeCommandEncoder> GetComputeCommandEncoder(void) const;
	
	/** @returns The active blit command encoder or nil if there isn't one. */
	id<MTLBlitCommandEncoder> GetBlitCommandEncoder(void) const;
	
	/** @returns The MTLFence for the current encoder or nil if there isn't one. */
	id<MTLFence> GetEncoderFence(void) const;
	
#pragma mark - Public Command Encoder Mutators -

	/**
 	 * Begins encoding rendering commands into the current command buffer. No other encoder may be active & the MTLRenderPassDescriptor must previously have been set.
	 */
	void BeginRenderCommandEncoding(void);
	
	/** Begins encoding compute commands into the current command buffer. No other encoder may be active. */
	void BeginComputeCommandEncoding(void);
	
	/** Begins encoding blit commands into the current command buffer. No other encoder may be active. */
	void BeginBlitCommandEncoding(void);
	
	/** Declare that all command generation from this encoder is complete, and detach from the MTLCommandBuffer if there is an encoder active or does nothing if there isn't. */
	id<MTLFence> EndEncoding(void);
	
	/** Initialises a fence for the current command-buffer optionally adding a command-buffer completion handler to the command-buffer */
	void InsertCommandBufferFence(FMetalCommandBufferFence& Fence, MTLCommandBufferHandler Handler);
	
	/** Adds a command-buffer completion handler to the command-buffer */
	void AddCompletionHandler(MTLCommandBufferHandler Handler);
	
	/** Update the event to capture all GPU work so far enqueued by this encoder. */
	void UpdateFence(id<MTLFence> Fence);
	
	/** Prevent further GPU work until the event is reached. */
	void WaitForFence(id<MTLFence> Fence);

#pragma mark - Public Debug Support -
	
	/*
	 * Inserts a debug string into the command buffer.  This does not change any API behavior, but can be useful when debugging.
	 * @param string The name of the signpost. 
	 */
	void InsertDebugSignpost(NSString* const String);
	
	/*
	 * Push a new named string onto a stack of string labels.
	 * @param string The name of the debug group. 
	 */
	void PushDebugGroup(NSString* const String);
	
	/* Pop the latest named string off of the stack. */
	void PopDebugGroup(void);
	
#pragma mark - Public Render State Mutators -

	/**
	 * Set the render pass descriptor - no encoder may be active when this function is called.
	 * @param RenderPass The render pass descriptor to set. May be nil.
	 */
	void SetRenderPassDescriptor(MTLRenderPassDescriptor* const RenderPass);
	
	/**
	 * Set the render pass store actions, call after SetRenderPassDescriptor but before EndEncoding.
	 * @param ColorStore The store actions for color targets.
	 * @param DepthStore The store actions for the depth buffer - use MTLStoreActionUnknown if no depth-buffer bound.
	 * @param StencilStore The store actions for the stencil buffer - use MTLStoreActionUnknown if no stencil-buffer bound.
	 */
	void SetRenderPassStoreActions(MTLStoreAction const* const ColorStore, MTLStoreAction const DepthStore, MTLStoreAction const StencilStore);
	
	/*
	 * Sets the current render pipeline state object.
	 * @param PipelineState The pipeline state to set. Must not be nil.
	 */
	void SetRenderPipelineState(FMetalShaderPipeline* const PipelineState);
	
	/*
	 * Set the viewport, which is used to transform vertexes from normalized device coordinates to window coordinates.  Fragments that lie outside of the viewport are clipped, and optionally clamped for fragments outside of znear/zfar.
	 * @param Viewport The array of viewport dimensions to use.
	 * @param NumActive The number of active viewport dimensions to use.
	 */
	void SetViewport(MTLViewport const Viewport[], uint32 NumActive);
	
	/*
	 * The winding order of front-facing primitives.
	 * @param FrontFacingWinding The front face winding.
	 */
	void SetFrontFacingWinding(MTLWinding const FrontFacingWinding);
	
	/*
	 * Controls if primitives are culled when front facing, back facing, or not culled at all.
	 * @param CullMode The cull mode.
	 */
	void SetCullMode(MTLCullMode const CullMode);
	
	/*
	 * Depth Bias.
	 * @param DepthBias The depth-bias value.
	 * @param SlopeScale The slope-scale to apply.
	 * @param Clamp The value to clamp to.
	 */
	void SetDepthBias(float const DepthBias, float const SlopeScale, float const Clamp);
	
	/*
	 * Specifies a rectangle for a fragment scissor test.  All fragments outside of this rectangle are discarded.
	 * @param Rect The array of scissor rect dimensions.
	 * @param NumActive The number of active scissor rect dimensions.
	 */
	void SetScissorRect(MTLScissorRect const Rect[], uint32 NumActive);
	
	/*
	 * Set how to rasterize triangle and triangle strip primitives.
	 * @param FillMode The fill mode.
	 */
	void SetTriangleFillMode(MTLTriangleFillMode const FillMode);
	
	/*
	 * Set the constant blend color used across all blending on all render targets
	 * @param Red The value for the red channel in 0-1.
	 * @param Green The value for the green channel in 0-1.
	 * @param Blue The value for the blue channel in 0-1.
	 * @param Alpha The value for the alpha channel in 0-1.
	 */
	void SetBlendColor(float const Red, float const Green, float const Blue, float const Alpha);
	
	/*
	 * Set the DepthStencil state object.
	 * @param DepthStencilState The depth-stencil state, must not be nil.
	 */
	void SetDepthStencilState(id<MTLDepthStencilState> const DepthStencilState);
	
	/*
	 * Set the stencil reference value for both the back and front stencil buffers.
	 * @param ReferenceValue The stencil ref value to use.
	 */
	void SetStencilReferenceValue(uint32 const ReferenceValue);
	
	/*
	 * Monitor if samples pass the depth and stencil tests.
	 * @param Mode Controls if the counter is disabled or moniters passing samples.
	 * @param Offset The offset relative to the occlusion query buffer provided when the command encoder was created.  offset must be a multiple of 8.
	 */
	void SetVisibilityResultMode(MTLVisibilityResultMode const Mode, NSUInteger const Offset);
	
#pragma mark - Public Shader Resource Mutators -
	
	/*
	 * Set a global buffer for the specified shader frequency at the given bind point index.
	 * @param FunctionType The shader function to modify.
	 * @param Buffer The buffer to bind or nil to clear.
	 * @param Offset The offset in the buffer or 0 when Buffer is nil.
	 * @param Length The data length - caller is responsible for accounting for non-zero Offset.
	 * @param Index The index to modify.
	 */
	void SetShaderBuffer(MTLFunctionType const FunctionType, id<MTLBuffer> const Buffer, NSUInteger const Offset, NSUInteger const Length, NSUInteger const Index, EPixelFormat const Format = PF_Unknown);
	
	/*
	 * Set an FMetalBufferData to the specified shader frequency at the given bind point index.
	 * @param FunctionType The shader function to modify.
	 * @param Data The data to bind or nullptr to clear.
	 * @param Offset The offset in the buffer or 0 when Buffer is nil.
	 * @param Index The index to modify.
	 */
	void SetShaderData(MTLFunctionType const FunctionType, FMetalBufferData* Data, NSUInteger const Offset, NSUInteger const Index);
	
	/*
	 * Set bytes to the specified shader frequency at the given bind point index.
	 * @param FunctionType The shader function to modify.
	 * @param Bytes The data to bind or nullptr to clear.
	 * @param Length The length of the buffer or 0 when Bytes is nil.
	 * @param Index The index to modify.
	 */
	void SetShaderBytes(MTLFunctionType const FunctionType, uint8 const* Bytes, NSUInteger const Length, NSUInteger const Index);
	
	/*
	 * Set a global texture for the specified shader frequency at the given bind point index.
	 * @param FunctionType The shader function to modify.
	 * @param Texture The texture to bind or nil to clear.
	 * @param Index The index to modify.
	 */
	void SetShaderTexture(MTLFunctionType const FunctionType, id<MTLTexture> const Texture, NSUInteger const Index);
	
	/*
	 * Set a global sampler for the specified shader frequency at the given bind point index.
	 * @param FunctionType The shader function to modify.
	 * @param Sampler The sampler state to bind or nil to clear.
	 * @param Index The index to modify.
	 */
	void SetShaderSamplerState(MTLFunctionType const FunctionType, id<MTLSamplerState> const Sampler, NSUInteger const Index);
	
	/*
	 * Set the shader side-table data for FunctionType at Index.
	 * @param FunctionType The shader function to modify.
	 * @param Index The index to bind data to.
	 */
	void SetShaderSideTable(MTLFunctionType const FunctionType, NSUInteger const Index);
	
#pragma mark - Public Compute State Mutators -
	
	/*
	 * Set the compute pipeline state that will be used.
	 * @param State The state to set - must not be nil.
	 */
	void SetComputePipelineState(FMetalShaderPipeline* State);

#pragma mark - Public Ring-Buffer Accessor -
	
	/*
	 * Get the internal ring-buffer used for temporary allocations.
	 * @returns The temporary allocation buffer for this command-encoder.
	 */
	FRingBuffer& GetRingBuffer(void);
	
private:
#pragma mark - Private Functions -
	/*
	 * Set the offset for the buffer bound on the specified shader frequency at the given bind point index.
	 * @param FunctionType The shader function to modify.
	 * @param Offset The offset in the buffer or 0 when Buffer is nil.
	 * @param Length The data length - caller is responsible for accounting for non-zero Offset.
	 * @param Index The index to modify.
	 */
	void SetShaderBufferOffset(MTLFunctionType const FunctionType, NSUInteger const Offset, NSUInteger const Length, NSUInteger const Index);
	
	void SetShaderBufferInternal(MTLFunctionType Function, uint32 Index);
	
#pragma mark - Private Type Declarations -
private:
    /** A structure of arrays for the current buffer binding settings. */
    struct FMetalBufferBindings
    {
        /** The bound buffers or nil. */
        id<MTLBuffer> Buffers[ML_MaxBuffers];
        /** The bound buffers or nil. */
        FMetalBufferData* Bytes[ML_MaxBuffers];
        /** The bound buffer offsets or 0. */
        NSUInteger Offsets[ML_MaxBuffers];
		/** The bound buffer lengths */
		uint32 Lengths[ML_MaxBuffers*2];
        /** A bitmask for which buffers were bound by the application where a bit value of 1 is bound and 0 is unbound. */
        uint32 Bound;
	};
	
#pragma mark - Private Member Variables -
	FMetalCommandList& CommandList;

    // Cache Queue feature
    bool bSupportsMetalFeaturesSetBytes;
    
	FMetalBufferBindings ShaderBuffers[MTLFunctionTypeKernel+1];
	
	MTLStoreAction ColorStoreActions[MaxSimultaneousRenderTargets];
	MTLStoreAction DepthStoreAction;
	MTLStoreAction StencilStoreAction;
	
	FRingBuffer RingBuffer;
	
	MTLRenderPassDescriptor* RenderPassDesc;
	NSUInteger RenderPassDescApplied;
	
	TSharedPtr<MTLCommandBufferRef, ESPMode::ThreadSafe> CommandBufferPtr;
	id<MTLCommandBuffer> CommandBuffer;
	id<MTLRenderCommandEncoder> RenderCommandEncoder;
	id<MTLComputeCommandEncoder> ComputeCommandEncoder;
	id<MTLBlitCommandEncoder> BlitCommandEncoder;
	FMetalFence EncoderFence;
	
	NSMutableArray<MTLCommandBufferHandler>* CompletionHandlers;
	NSMutableArray* DebugGroups;
};
