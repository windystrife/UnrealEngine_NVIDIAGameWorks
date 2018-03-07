// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalRHIPrivate.h"
#include "MetalCommandQueue.h"
#include "GPUProfiler.h"

#if METAL_STATISTICS
#include "MetalStatistics.h"
#endif

// Stats
DECLARE_CYCLE_STAT_EXTERN(TEXT("MakeDrawable time"),STAT_MetalMakeDrawableTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Draw call time"),STAT_MetalDrawCallTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("PrepareDraw time"),STAT_MetalPrepareDrawTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("PipelineState time"),STAT_MetalPipelineStateTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("BoundShaderState time"),STAT_MetalBoundShaderStateTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("VertexDeclaration time"),STAT_MetalVertexDeclarationTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Uniform buffer pool cleanup time"), STAT_MetalUniformBufferCleanupTime, STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Buffer Page-Off time"), STAT_MetalBufferPageOffTime, STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Texture Page-Off time"), STAT_MetalTexturePageOffTime, STATGROUP_MetalRHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Uniform buffer pool memory"), STAT_MetalTotalUniformBufferMemory, STATGROUP_MetalRHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Free Uniform pool memory"), STAT_MetalFreeUniformBufferMemory, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Uniform buffer pool num free"), STAT_MetalNumFreeUniformBuffers, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Buffer Count"), STAT_MetalBufferCount, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Texture Count"), STAT_MetalTextureCount, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Uncommitted Command Buffer Count"), STAT_MetalCommandBufferCount, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Sampler State Count"), STAT_MetalSamplerStateCount, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Depth Stencil State Count"), STAT_MetalDepthStencilStateCount, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Render Pipeline State Count"), STAT_MetalRenderPipelineStateCount, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Compute Pipeline State Count"), STAT_MetalComputePipelineStateCount, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Render Pipeline Color Desc Count"), STAT_MetalRenderPipelineColorAttachmentDescriptor, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Render Pass Desc Count"), STAT_MetalRenderPassDescriptorCount, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Render Pass Color Desc Count"), STAT_MetalRenderPassColorAttachmentDescriptorCount, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Render Pass Depth Desc Count"), STAT_MetalRenderPassDepthAttachmentDescriptorCount, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Render Pass Stencil Desc Count"), STAT_MetalRenderPassStencilAttachmentDescriptorCount, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Vertex Desc Count"), STAT_MetalVertexDescriptorCount, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Function Count"), STAT_MetalFunctionCount, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Free Pooled Buffer Count"), STAT_MetalFreePooledBufferCount, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Total Pooled Buffer Count"), STAT_MetalPooledBufferCount, STATGROUP_MetalRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Metal Buffers Allocated Per-Frame"), STAT_MetalBufferNativeAlloctations, STATGROUP_MetalRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Metal Buffers Freed Per-Frame"), STAT_MetalBufferNativeFreed, STATGROUP_MetalRHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Total Pooled Buffer Memory"), STAT_MetalPooledBufferMem, STATGROUP_MetalRHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Used Pooled Buffer Memory"), STAT_MetalUsedPooledBufferMem, STATGROUP_MetalRHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Free Pooled Buffer Memory"), STAT_MetalFreePooledBufferMem, STATGROUP_MetalRHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Wasted Pooled Buffer Memory"), STAT_MetalWastedPooledBufferMem, STATGROUP_MetalRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Buffers Allocated Per-Frame"), STAT_MetalBufferAlloctations, STATGROUP_MetalRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Buffers Freed Per-Frame"), STAT_MetalBufferFreed, STATGROUP_MetalRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Buffer Memory Allocated Per-Frame"), STAT_MetalBufferMemAlloc, STATGROUP_MetalRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Buffer Memory Freed Per-Frame"), STAT_MetalBufferMemFreed, STATGROUP_MetalRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Metal Buffer Memory Allocated Per-Frame"), STAT_MetalBufferNativeMemAlloc, STATGROUP_MetalRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Metal Buffer Memory Freed Per-Frame"), STAT_MetalBufferNativeMemFreed, STATGROUP_MetalRHI, );

DECLARE_CYCLE_STAT_EXTERN(TEXT("PrepareVertexDesc time"),STAT_MetalPrepareVertexDescTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("BoundShaderPrepareDraw time"),STAT_MetalBoundShaderPrepareDrawTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("BoundShaderLock time"),STAT_MetalBoundShaderLockTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("PipelineLock time"),STAT_MetalPipelineLockTime,STATGROUP_MetalRHI, );

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Uniform Memory Allocated Per-Frame"), STAT_MetalUniformMemAlloc, STATGROUP_MetalRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Uniform Memory Freed Per-Frame"), STAT_MetalUniformMemFreed, STATGROUP_MetalRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Vertex Memory Allocated Per-Frame"), STAT_MetalVertexMemAlloc, STATGROUP_MetalRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Vertex Memory Freed Per-Frame"), STAT_MetalVertexMemFreed, STATGROUP_MetalRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Index Memory Allocated Per-Frame"), STAT_MetalIndexMemAlloc, STATGROUP_MetalRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Index Memory Freed Per-Frame"), STAT_MetalIndexMemFreed, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Private Texture Count"), STAT_MetalPrivateTextureCount, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Managed Texture Count"), STAT_MetalManagedTextureCount, STATGROUP_MetalRHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Private Texture Memory"), STAT_MetalPrivateTextureMem, STATGROUP_MetalRHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Managed Texture Memory"), STAT_MetalManagedTextureMem, STATGROUP_MetalRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Texture Memory Updated Per-Frame"), STAT_MetalTextureMemUpdate, STATGROUP_MetalRHI, );

DECLARE_CYCLE_STAT_EXTERN(TEXT("Texture Page-On time"), STAT_MetalTexturePageOnTime, STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("GPU Work time"), STAT_MetalGPUWorkTime, STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("GPU Idle time"), STAT_MetalGPUIdleTime, STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Present time"), STAT_MetalPresentTime, STATGROUP_MetalRHI, );
#if STATS
extern int64 volatile GMetalTexturePageOnTime;
extern int64 volatile GMetalGPUWorkTime;
extern int64 volatile GMetalGPUIdleTime;
extern int64 volatile GMetalPresentTime;
#endif

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Number Command Buffers Created Per-Frame"), STAT_MetalCommandBufferCreatedPerFrame, STATGROUP_MetalRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Number Command Buffers Committed Per-Frame"), STAT_MetalCommandBufferCommittedPerFrame, STATGROUP_MetalRHI, );

#if METAL_STATISTICS
#define RHI_PROFILE_DRAW_CALL_STATS(StartPoint, EndPoint, NumPrims, NumVerts) FMetalDrawProfiler GPUWork(Profiler, (uint32)StartPoint, (uint32)EndPoint, NumPrims, NumVerts)
#else
#define RHI_PROFILE_DRAW_CALL_STATS(StartPoint, EndPoint, NumPrims, NumVerts) FMetalDrawProfiler GPUWork(Profiler, (uint32)0, (uint32)0, NumPrims, NumVerts)
#endif

class FMetalDrawProfiler
{
public:
	FMetalDrawProfiler(struct FMetalGPUProfiler* InProfiler, uint32 StartPoint, uint32 EndPoint, uint32 NumPrimitives = 0, uint32 NumVertices = 0);
	~FMetalDrawProfiler();
	
private:
	struct FMetalGPUProfiler* Profiler;
	uint32 StartPoint;
	uint32 EndPoint;
};

/** A single perf event node, which tracks information about a appBeginDrawEvent/appEndDrawEvent range. */
class FMetalEventNode : public FGPUProfilerEventNode
{
public:
	
	FMetalEventNode(FMetalContext* InContext, const TCHAR* InName, FGPUProfilerEventNode* InParent, bool bIsRoot, bool bInFullProfiling)
	: FGPUProfilerEventNode(InName, InParent)
	, Context(InContext)
	, StartTime(0)
	, EndTime(0)
	, bRoot(bIsRoot)
    , bFullProfiling(bInFullProfiling)
	{
	}
	
	virtual ~FMetalEventNode();
	
	/**
	 * Returns the time in ms that the GPU spent in this draw event.
	 * This blocks the CPU if necessary, so can cause hitching.
	 */
	virtual float GetTiming() override;
	
	virtual void StartTiming() override;
	
	virtual void StopTiming() override;
	
	MTLCommandBufferHandler Start(void);
	MTLCommandBufferHandler Stop(void);

	bool Wait() const { return bRoot && bFullProfiling; }
	bool IsRoot() const { return bRoot; }
	
	uint64 GetCycles() { return EndTime - StartTime; }
	
#if METAL_STATISTICS
	void StartDraw(bool bActiveStats, uint32 StartPoint, uint32 EndPoint, uint32 NumPrimitives, uint32 NumVertices);
	void StopDraw(void);
	
	void GetStats(FMetalPipelineStats& OutStats);
#endif
	
private:
	FMetalContext* Context;
#if METAL_STATISTICS
	TArray<IMetalDrawStats*> DrawStats;
#endif
	uint64 StartTime;
	uint64 EndTime;
	bool bRoot;
    bool bFullProfiling;
};

/** An entire frame of perf event nodes, including ancillary timers. */
class FMetalEventNodeFrame : public FGPUProfilerEventNodeFrame
{
public:
	FMetalEventNodeFrame(FMetalContext* InContext, bool bInFullProfiling)
	: RootNode(new FMetalEventNode(InContext, TEXT("Frame"), nullptr, true, bInFullProfiling))
    , bFullProfiling(bInFullProfiling)
	{
	}
	
	virtual ~FMetalEventNodeFrame()
	{
        if(bFullProfiling)
        {
            delete RootNode;
        }
	}
	
	/** Start this frame of per tracking */
	virtual void StartFrame() override;
	
	/** End this frame of per tracking, but do not block yet */
	virtual void EndFrame() override;
	
	/** Calculates root timing base frequency (if needed by this RHI) */
	virtual float GetRootTimingResults() override;
	
	virtual void LogDisjointQuery() override;
	
	FMetalEventNode* RootNode;
    bool bFullProfiling;
};

// This class has multiple inheritance but really FGPUTiming is a static class
class FMetalGPUTiming : public FGPUTiming
{
public:
	
	/**
	 * Constructor.
	 */
	FMetalGPUTiming()
	{
		StaticInitialize(nullptr, PlatformStaticInitialize);
	}
	
	
private:
	
	/**
	 * Initializes the static variables, if necessary.
	 */
	static void PlatformStaticInitialize(void* UserData)
	{
		// Are the static variables initialized?
		if ( !GAreGlobalsInitialized )
		{
			GIsSupported = true;
			GTimingFrequency = 1000 * 1000 * 1000;
			GAreGlobalsInitialized = true;
		}
	}
};

/**
 * Encapsulates GPU profiling logic and data.
 * There's only one global instance of this struct so it should only contain global data, nothing specific to a frame.
 */
struct FMetalGPUProfiler : public FGPUProfiler
{
	/** GPU hitch profile histories */
	TIndirectArray<FMetalEventNodeFrame> GPUHitchEventNodeFrames;
	
	FMetalGPUProfiler(FMetalContext* InContext)
	:	FGPUProfiler()
	,	Context(InContext)
    ,   NumNestedFrames(0)
    ,	bActiveStats(false)
	{
		FMemory::Memzero((void*)&FrameStartGPU[0], sizeof(FrameStartGPU));
		FMemory::Memzero((void*)&FrameEndGPU[0], sizeof(FrameEndGPU));
		FMemory::Memzero((void*)&FrameGPUTime[0], sizeof(FrameGPUTime));
		FMemory::Memzero((void*)&FrameIdleTime[0], sizeof(FrameIdleTime));
		FMemory::Memzero((void*)&FramePresentTime[0], sizeof(FramePresentTime));
	}
	
	virtual ~FMetalGPUProfiler() {}
	
	virtual FGPUProfilerEventNode* CreateEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent) override;
	
	void Cleanup();
	
	virtual void PushEvent(const TCHAR* Name, FColor Color) override;
	virtual void PopEvent() override;
	
	void BeginFrame();
	void EndFrame();
	
	void StartGPUWork(uint32 StartPoint, uint32 EndPoint, uint32 NumPrimitives = 0, uint32 NumVertices = 0);
	void FinishGPUWork(void);
	
	static void IncrementFrameIndex();
	static void RecordFrame(id<MTLCommandBuffer> Buffer);
	static void RecordPresent(id<MTLCommandBuffer> Buffer);
	static void RecordCommandBuffer(id<MTLCommandBuffer> Buffer);
	
	#define MAX_FRAME_HISTORY 3
	static volatile int32 FrameTimeGPUIndex;
	static volatile int64 FrameStartGPU[MAX_FRAME_HISTORY];
	static volatile int64 FrameEndGPU[MAX_FRAME_HISTORY];
	static volatile int64 FrameGPUTime[MAX_FRAME_HISTORY];
	static volatile int64 FrameIdleTime[MAX_FRAME_HISTORY];
	static volatile int64 FramePresentTime[MAX_FRAME_HISTORY];
	
	FMetalGPUTiming TimingSupport;
	FMetalContext* Context;
    int32 NumNestedFrames;
    bool bActiveStats;
};

