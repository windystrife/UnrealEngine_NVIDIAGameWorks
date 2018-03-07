// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalRHIPrivate.h"
#include "MetalCommandEncoder.h"
#include "MetalState.h"
#include "MetalFence.h"

class FMetalCommandList;
class FMetalCommandQueue;

class FMetalRenderPass
{
public:
#pragma mark - Public C++ Boilerplate -

	/** Default constructor */
	FMetalRenderPass(FMetalCommandList& CmdList, FMetalStateCache& StateCache);
	
	/** Destructor */
	~FMetalRenderPass(void);
	
#pragma mark -
    void Begin(id<MTLFence> Fence);
	
	void Wait(id<MTLFence> Fence);

	void Update(id<MTLFence> Fence);
	
    void BeginRenderPass(MTLRenderPassDescriptor* const RenderPass);

    void RestartRenderPass(MTLRenderPassDescriptor* const RenderPass);
    
    void DrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances);
    
    void DrawPrimitiveIndirect(uint32 PrimitiveType, FMetalVertexBuffer* VertexBuffer, uint32 ArgumentOffset);
    
    void DrawIndexedPrimitive(id<MTLBuffer> IndexBuffer, uint32 IndexStride, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance,
                         uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances);
    
    void DrawIndexedIndirect(FMetalIndexBuffer* IndexBufferRHI, uint32 PrimitiveType, FMetalStructuredBuffer* VertexBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances);
    
    void DrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FMetalIndexBuffer* IndexBufferRHI,FMetalVertexBuffer* VertexBufferRHI,uint32 ArgumentOffset);
    
    void DrawPatches(uint32 PrimitiveType, id<MTLBuffer> IndexBuffer, uint32 IndexBufferStride, int32 BaseVertexIndex, uint32 FirstInstance, uint32 StartIndex,
                     uint32 NumPrimitives, uint32 NumInstances);
    
    void Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ);
    
    void DispatchIndirect(FMetalVertexBuffer* ArgumentBufferRHI, uint32 ArgumentOffset);
    
    id<MTLFence> EndRenderPass(void);
    
    void CopyFromTextureToBuffer(id<MTLTexture> Texture, uint32 sourceSlice, uint32 sourceLevel, MTLOrigin sourceOrigin, MTLSize sourceSize, id<MTLBuffer> toBuffer, uint32 destinationOffset, uint32 destinationBytesPerRow, uint32 destinationBytesPerImage, MTLBlitOption options);
    
    void CopyFromBufferToTexture(id<MTLBuffer> Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, MTLSize sourceSize, id<MTLTexture> toTexture, uint32 destinationSlice, uint32 destinationLevel, MTLOrigin destinationOrigin);
    
    void CopyFromTextureToTexture(id<MTLTexture> Texture, uint32 sourceSlice, uint32 sourceLevel, MTLOrigin sourceOrigin, MTLSize sourceSize, id<MTLTexture> toTexture, uint32 destinationSlice, uint32 destinationLevel, MTLOrigin destinationOrigin);
	
	void CopyFromBufferToBuffer(id<MTLBuffer> SourceBuffer, NSUInteger SourceOffset, id<MTLBuffer> DestinationBuffer, NSUInteger DestinationOffset, NSUInteger Size);
	
	void PresentTexture(id<MTLTexture> Texture, uint32 sourceSlice, uint32 sourceLevel, MTLOrigin sourceOrigin, MTLSize sourceSize, id<MTLTexture> toTexture, uint32 destinationSlice, uint32 destinationLevel, MTLOrigin destinationOrigin);
    
    void SynchronizeTexture(id<MTLTexture> Texture, uint32 Slice, uint32 Level);
    
    void SynchroniseResource(id<MTLResource> Resource);
    
    void FillBuffer(id<MTLBuffer> Buffer, NSRange Range, uint8 Value);
	
	void AsyncCopyFromBufferToTexture(id<MTLBuffer> Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, MTLSize sourceSize, id<MTLTexture> toTexture, uint32 destinationSlice, uint32 destinationLevel, MTLOrigin destinationOrigin);
	
	void AsyncCopyFromTextureToTexture(id<MTLTexture> Texture, uint32 sourceSlice, uint32 sourceLevel, MTLOrigin sourceOrigin, MTLSize sourceSize, id<MTLTexture> toTexture, uint32 destinationSlice, uint32 destinationLevel, MTLOrigin destinationOrigin);
	
	void AsyncGenerateMipmapsForTexture(id<MTLTexture> Texture);
	
    id<MTLFence> Submit(EMetalSubmitFlags SubmissionFlags);
    
    id<MTLFence> End(void);
	
	void InsertCommandBufferFence(FMetalCommandBufferFence& Fence, MTLCommandBufferHandler Handler);
	
	void AddCompletionHandler(MTLCommandBufferHandler Handler);
	
	void AddAsyncCommandBufferHandlers(MTLCommandBufferHandler Scheduled, MTLCommandBufferHandler Completion);

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
    
#pragma mark - Public Accessors -
	
	/*
	 * Get the current internal command buffer.
	 * @returns The current command buffer.
	 */
	id<MTLCommandBuffer> GetCurrentCommandBuffer(void) const;
	
	/*
	 * Get the internal ring-buffer used for temporary allocations.
	 * @returns The temporary allocation buffer for the command-pass.
	 */
	FRingBuffer& GetRingBuffer(void);
	
private:
#pragma mark -
    void ConditionalSwitchToRender(void);
    void ConditionalSwitchToTessellation(void);
    void ConditionalSwitchToCompute(void);
	void ConditionalSwitchToBlit(void);
	void ConditionalSwitchToAsyncBlit(void);
	
    void PrepareToRender(uint32 PrimType);
    void PrepareToTessellate(uint32 PrimType);
    void PrepareToDispatch(void);

    void CommitRenderResourceTables(void);
    void CommitTessellationResourceTables(void);
    void CommitDispatchResourceTables(void);
    
    void ConditionalSubmit();
private:
#pragma mark -
	FMetalCommandList& CmdList;
    FMetalStateCache& State;
    
    // Which of the buffers/textures/sampler slots are bound
    // The state cache is responsible for ensuring we bind the correct 
    FMetalTextureMask BoundTextures[SF_NumFrequencies];
    uint32 BoundBuffers[SF_NumFrequencies];
    uint16 BoundSamplers[SF_NumFrequencies];
    
    FMetalCommandEncoder CurrentEncoder;
    FMetalCommandEncoder PrologueEncoder;
    
    FMetalFence PassStartFence;
    FMetalFence CurrentEncoderFence;
    FMetalFence PrologueEncoderFence;
    
    typedef TMetalPtr<MTLRenderPassDescriptor*> MTLRenderPassDescriptorRef;
    MTLRenderPassDescriptorRef RenderPassDesc;
    
    uint32 NumOutstandingOps;
    bool bWithinRenderPass;
};
