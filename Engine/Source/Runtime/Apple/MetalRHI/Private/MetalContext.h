// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "MetalViewport.h"
#include "MetalBufferPools.h"
#include "MetalCommandEncoder.h"
#include "MetalCommandQueue.h"
#include "MetalCommandList.h"
#include "MetalRenderPass.h"
#include "MetalHeap.h"
#include "MetalCaptureManager.h"
#if PLATFORM_IOS
#include "IOSView.h"
#endif
#include "LockFreeList.h"

#define NUM_SAFE_FRAMES 4

class FMetalRHICommandContext;
class FShaderCacheState;

class FMetalContext
{
	friend class FMetalCommandContextContainer;
public:
	FMetalContext(FMetalCommandQueue& Queue, bool const bIsImmediate);
	virtual ~FMetalContext();
	
	id<MTLDevice> GetDevice();
	FMetalCommandQueue& GetCommandQueue();
	FMetalCommandList& GetCommandList();
	id<MTLCommandBuffer> GetCurrentCommandBuffer();
	FMetalStateCache& GetCurrentState() { return StateCache; }
	FMetalRenderPass& GetCurrentRenderPass() { return RenderPass; }
	
	void InsertCommandBufferFence(FMetalCommandBufferFence& Fence, MTLCommandBufferHandler Handler = nil);

	/**
	 * Do anything necessary to prepare for any kind of draw call 
	 * @param PrimitiveType The UE4 primitive type for the draw call, needed to compile the correct render pipeline.
	 * @param IndexType The index buffer type (none, uint16, uint32), needed to compile the correct tessellation compute pipeline.
	 * @returns True if the preparation completed and the draw call can be encoded, false to skip.
	 */
	bool PrepareToDraw(uint32 PrimitiveType, EMetalIndexType IndexType = EMetalIndexType_None);
	
	/**
	 * Set the color, depth and stencil render targets, and then make the new command buffer/encoder
	 */
	void SetRenderTargetsInfo(const FRHISetRenderTargetsInfo& RenderTargetsInfo, bool const bRestart = false);
	
	/**
	 * Allocate from a dynamic ring buffer - by default align to the allowed alignment for offset field when setting buffers
	 */
	uint32 AllocateFromRingBuffer(uint32 Size, uint32 Alignment=0);
	id<MTLBuffer> GetRingBuffer();

	TSharedRef<FMetalQueryBufferPool, ESPMode::ThreadSafe> GetQueryBufferPool()
	{
		return QueryBuffer.ToSharedRef();
	}

    void SubmitCommandsHint(uint32 const bFlags = EMetalSubmitFlagsCreateCommandBuffer);
	void SubmitCommandBufferAndWait();
	void ResetRenderCommandEncoder();
	
	void DrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances);
	
	void DrawPrimitiveIndirect(uint32 PrimitiveType, FMetalVertexBuffer* VertexBuffer, uint32 ArgumentOffset);
	
	void DrawIndexedPrimitive(id<MTLBuffer> IndexBuffer, uint32 IndexStride, MTLIndexType IndexType, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance,
							  uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances);
	
	void DrawIndexedIndirect(FMetalIndexBuffer* IndexBufferRHI, uint32 PrimitiveType, FMetalStructuredBuffer* VertexBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances);
	
	void DrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FMetalIndexBuffer* IndexBufferRHI,FMetalVertexBuffer* VertexBufferRHI,uint32 ArgumentOffset);
	
	void DrawPatches(uint32 PrimitiveType, id<MTLBuffer> IndexBuffer, uint32 IndexBufferStride, int32 BaseVertexIndex, uint32 FirstInstance, uint32 StartIndex,
					 uint32 NumPrimitives, uint32 NumInstances);
	
	void CopyFromTextureToBuffer(id<MTLTexture> Texture, uint32 sourceSlice, uint32 sourceLevel, MTLOrigin sourceOrigin, MTLSize sourceSize, id<MTLBuffer> toBuffer, uint32 destinationOffset, uint32 destinationBytesPerRow, uint32 destinationBytesPerImage, MTLBlitOption options);
	
	void CopyFromBufferToTexture(id<MTLBuffer> Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, MTLSize sourceSize, id<MTLTexture> toTexture, uint32 destinationSlice, uint32 destinationLevel, MTLOrigin destinationOrigin);
	
	void CopyFromTextureToTexture(id<MTLTexture> Texture, uint32 sourceSlice, uint32 sourceLevel, MTLOrigin sourceOrigin, MTLSize sourceSize, id<MTLTexture> toTexture, uint32 destinationSlice, uint32 destinationLevel, MTLOrigin destinationOrigin);
	
	void CopyFromBufferToBuffer(id<MTLBuffer> SourceBuffer, NSUInteger SourceOffset, id<MTLBuffer> DestinationBuffer, NSUInteger DestinationOffset, NSUInteger Size);
	
    void AsyncCopyFromBufferToTexture(id<MTLBuffer> Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, MTLSize sourceSize, id<MTLTexture> toTexture, uint32 destinationSlice, uint32 destinationLevel, MTLOrigin destinationOrigin);
    
    void AsyncCopyFromTextureToTexture(id<MTLTexture> Texture, uint32 sourceSlice, uint32 sourceLevel, MTLOrigin sourceOrigin, MTLSize sourceSize, id<MTLTexture> toTexture, uint32 destinationSlice, uint32 destinationLevel, MTLOrigin destinationOrigin);
    
    void AsyncGenerateMipmapsForTexture(id<MTLTexture> Texture);
    
	void SubmitAsyncCommands(MTLCommandBufferHandler ScheduledHandler, MTLCommandBufferHandler CompletionHandler, bool const bWait);
	
	void SynchronizeTexture(id<MTLTexture> Texture, uint32 Slice, uint32 Level);
	
	void SynchroniseResource(id<MTLResource> Resource);
	
	void FillBuffer(id<MTLBuffer> Buffer, NSRange Range, uint8 Value);

	void Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ);
	void DispatchIndirect(FMetalVertexBuffer* ArgumentBuffer, uint32 ArgumentOffset);

	void StartTiming(class FMetalEventNode* EventNode);
	void EndTiming(class FMetalEventNode* EventNode);

#if ENABLE_METAL_GPUPROFILE
	static void MakeCurrent(FMetalContext* Context);
	static FMetalContext* GetCurrentContext();
#endif
	
	void SetParallelPassFences(id<MTLFence> Start, id<MTLFence> End);
	
	void InitFrame(bool const bImmediateContext);
	void FinishFrame();

protected:
	/** The underlying Metal device */
	id<MTLDevice> Device;
	
	/** The wrapper around the device command-queue for creating & committing command buffers to */
	FMetalCommandQueue& CommandQueue;
	
	/** The wrapper around commabd buffers for ensuring correct parallel execution order */
	FMetalCommandList CommandList;
	
	/** The cache of all tracked & accessible state. */
	FMetalStateCache StateCache;
	
	/** The render pass handler that actually encodes our commands. */
	FMetalRenderPass RenderPass;
	
	/** A sempahore used to ensure that wait for previous frames to complete if more are in flight than we permit */
	dispatch_semaphore_t CommandBufferSemaphore;
	
	/** A pool of buffers for writing visibility query results. */
	TSharedPtr<FMetalQueryBufferPool, ESPMode::ThreadSafe> QueryBuffer;
	
	/** Initial fence to wait on for parallel contexts */
	FMetalFence StartFence;
	
	/** Fence to update at the end for parallel contexts */
	FMetalFence EndFence;
	
#if ENABLE_METAL_GPUPROFILE
	/** the slot to store a per-thread context ref */
	static uint32 CurrentContextTLSSlot;
#endif
	
	/** Whether the validation layer is enabled */
	bool bValidationEnabled;
};


class FMetalDeviceContext : public FMetalContext
{
public:
	static FMetalDeviceContext* CreateDeviceContext();
	virtual ~FMetalDeviceContext();
	
	void Init(void);
	
	inline bool SupportsFeature(EMetalFeatures InFeature) { return CommandQueue.SupportsFeature(InFeature); }
	
	id<MTLTexture> CreateTexture(FMetalSurface* Surface, MTLTextureDescriptor* Descriptor);
	id<MTLBuffer> CreatePooledBuffer(FMetalPooledBufferArgs const& Args);
	void ReleasePooledBuffer(id<MTLBuffer> Buf);
	void ReleaseObject(id Object);
	void ReleaseResource(id<MTLResource> Object);
	void ReleaseTexture(FMetalSurface* Surface, id<MTLTexture> Texture);
	void ReleaseFence(id<MTLFence> Fence);
	void ReleaseHeap(id<MTLHeap> Heap);
	
	void BeginFrame();
	void FlushFreeList();
	void ClearFreeList();
	void DrainHeap();
	void EndFrame();
	
	/** RHIBeginScene helper */
	void BeginScene();
	/** RHIEndScene helper */
	void EndScene();
	
	void BeginDrawingViewport(FMetalViewport* Viewport);
	void EndDrawingViewport(FMetalViewport* Viewport, bool bPresent, bool bLockToVsync);
	
	/** Take a parallel FMetalContext from the free-list or allocate a new one if required */
	FMetalRHICommandContext* AcquireContext(int32 NewIndex, int32 NewNum);
	
	/** Release a parallel FMetalContext back into the free-list */
	void ReleaseContext(FMetalRHICommandContext* Context);
	
	/** Returns the number of concurrent contexts encoding commands, including the device context. */
	uint32 GetNumActiveContexts(void) const;
	
	/** Get the index of the bound Metal device in the global list of rendering devices. */
	uint32 GetDeviceIndex(void) const;
	
private:
	FMetalDeviceContext(id<MTLDevice> MetalDevice, uint32 DeviceIndex, FMetalCommandQueue* Queue);
	
private:
	/** The chosen Metal device */
	id<MTLDevice> Device;
	
	/** The index into the GPU device list for the selected Metal device */
	uint32 DeviceIndex;
	
	/** Dynamic memory heap */
	FMetalHeap Heap;
	
	/** GPU Frame Capture Manager */
	FMetalCaptureManager CaptureManager;
	
	/** Free lists for releasing objects only once it is safe to do so */
	TSet<id> ObjectFreeList;
	TSet<id<MTLResource>> ResourceFreeList;
	TSet<id<MTLHeap>> HeapFreeList;
	struct FMetalDelayedFreeList
	{
		dispatch_semaphore_t Signal;
		TSet<id> ObjectFreeList;
		TSet<id<MTLResource>> ResourceFreeList;
		TSet<id<MTLHeap>> HeapFreeList;
#if METAL_DEBUG_OPTIONS
		int32 DeferCount;
#endif
	};
	TArray<FMetalDelayedFreeList*> DelayedFreeLists;
	
#if METAL_DEBUG_OPTIONS
	/** The list of fences for the current frame */
	NSMutableArray<id<MTLFence>>* FrameFences;
#endif
	
	/** Free-list of contexts for parallel encoding */
	TLockFreePointerListLIFO<FMetalRHICommandContext> ParallelContexts;
	
	/** Fences for parallel execution */
	TArray<id<MTLFence>> ParallelFences;
	
	/** Critical section for FreeList */
	FCriticalSection FreeListMutex;
	
	/** Event for coordinating pausing of render thread to keep inline with the ios display link. */
	FEvent* FrameReadyEvent;
	
	/** Internal frame counter, incremented on each call to RHIBeginScene. */
	uint32 SceneFrameCounter;
	
	/** Internal frame counter, used to ensure that we only drain the buffer pool one after each frame within RHIEndFrame. */
	uint32 FrameCounter;
	
	/** Bitfield of supported Metal features with varying availability depending on OS/device */
	uint32 Features;
	
	/** Count of concurrent contexts encoding commands. */
	int32 ActiveContexts;
	
	/** Whether we presented this frame - only used to track when to introduce debug markers */
	bool bPresented;
};
