// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11RHIPrivate.h: Private D3D RHI definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "D3D11RHI.h"
// Dependencies.
#include "RHI.h"
#include "GPUProfiler.h"
#include "ShaderCore.h"
#include "Containers/ResourceArray.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"

DECLARE_LOG_CATEGORY_EXTERN(LogD3D11RHI, Log, All);

#include "Windows/D3D11RHIBasePrivate.h"
#include "StaticArray.h"

// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
#include "GFSDK_VXGI.h"
#include "D3D11NvRHI.h"
#endif
// NVCHANGE_END: Add VXGI

// NVCHANGE_BEGIN: Add HBAO+
#if WITH_GFSDK_SSAO
#include "GFSDK_SSAO.h"
#endif
// NVCHANGE_END: Add HBAO+

// D3D RHI public headers.
#include "D3D11Util.h"
#include "D3D11State.h"
#include "D3D11Resources.h"
#include "D3D11Viewport.h"
#include "D3D11ConstantBuffer.h"
#include "D3D11StateCache.h"

#ifndef WITH_DX_PERF
#define WITH_DX_PERF	1
#endif

#ifndef NV_AFTERMATH
#define NV_AFTERMATH	0
#endif

#if NV_AFTERMATH
#define GFSDK_Aftermath_WITH_DX11 1
#include "GFSDK_Aftermath.h"
#undef GFSDK_Aftermath_WITH_DX11
extern int32 GDX11NVAfterMathEnabled;
#endif

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
#define CHECK_SRV_TRANSITIONS 0
#else
//Feature is broken, and also will leak memory when the program is alt-tabbed.  disable for now.
#define CHECK_SRV_TRANSITIONS 0
#endif

// DX11 doesn't support higher MSAA count
#define DX_MAX_MSAA_COUNT 8


/**
 * The D3D RHI stats.
 */
DECLARE_CYCLE_STAT_EXTERN(TEXT("Present time"),STAT_D3D11PresentTime,STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("CreateTexture time"),STAT_D3D11CreateTextureTime,STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("LockTexture time"),STAT_D3D11LockTextureTime,STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("UnlockTexture time"),STAT_D3D11UnlockTextureTime,STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("CopyTexture time"),STAT_D3D11CopyTextureTime,STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("CreateBoundShaderState time"),STAT_D3D11CreateBoundShaderStateTime,STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("New bound shader state time"),STAT_D3D11NewBoundShaderStateTime,STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Clean uniform buffer pool"),STAT_D3D11CleanUniformBufferTime,STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Clear shader resources"),STAT_D3D11ClearShaderResourceTime,STATGROUP_D3D11RHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Uniform buffer pool num free"),STAT_D3D11NumFreeUniformBuffers,STATGROUP_D3D11RHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Bound Shader State"), STAT_D3D11NumBoundShaderState, STATGROUP_D3D11RHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Uniform buffer pool memory"), STAT_D3D11FreeUniformBufferMemory, STATGROUP_D3D11RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update uniform buffer"),STAT_D3D11UpdateUniformBufferTime,STATGROUP_D3D11RHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Textures Allocated"),STAT_D3D11TexturesAllocated,STATGROUP_D3D11RHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Textures Released"),STAT_D3D11TexturesReleased,STATGROUP_D3D11RHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Texture object pool memory"),STAT_D3D11TexturePoolMemory,STATGROUP_D3D11RHI, );

struct FD3D11GlobalStats
{
	// in bytes, never change after RHI, needed to scale game features
	static int64 GDedicatedVideoMemory;
	
	// in bytes, never change after RHI, needed to scale game features
	static int64 GDedicatedSystemMemory;
	
	// in bytes, never change after RHI, needed to scale game features
	static int64 GSharedSystemMemory;
	
	// In bytes. Never changed after RHI init. Our estimate of the amount of memory that we can use for graphics resources in total.
	static int64 GTotalGraphicsMemory;
};


// This class has multiple inheritance but really FGPUTiming is a static class
class FD3D11BufferedGPUTiming : public FRenderResource, public FGPUTiming
{
public:
	/**
	 * Constructor.
	 *
	 * @param InD3DRHI			RHI interface
	 * @param InBufferSize		Number of buffered measurements
	 */
	FD3D11BufferedGPUTiming(class FD3D11DynamicRHI* InD3DRHI, int32 BufferSize);

	/**
	 * Start a GPU timing measurement.
	 */
	void	StartTiming();

	/**
	 * End a GPU timing measurement.
	 * The timing for this particular measurement will be resolved at a later time by the GPU.
	 */
	void	EndTiming();

	/**
	 * Retrieves the most recently resolved timing measurement.
	 * The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
	 *
	 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
	 */
	uint64	GetTiming(bool bGetCurrentResultsAndBlock = false);

	/**
	 * Initializes all D3D resources.
	 */
	virtual void InitDynamicRHI() override;

	/**
	 * Releases all D3D resources.
	 */
	virtual void ReleaseDynamicRHI() override;

private:
	/**
	 * Initializes the static variables, if necessary.
	 */
	static void PlatformStaticInitialize(void* UserData);

	/** RHI interface */
	FD3D11DynamicRHI*			D3DRHI;
	/** Number of timestamps created in 'StartTimestamps' and 'EndTimestamps'. */
	int32						BufferSize;
	/** Current timing being measured on the CPU. */
	int32						CurrentTimestamp;
	/** Number of measurements in the buffers (0 - BufferSize). */
	int32						NumIssuedTimestamps;
	/** Timestamps for all StartTimings. */
	TRefCountPtr<ID3D11Query>*	StartTimestamps;
	/** Timestamps for all EndTimings. */
	TRefCountPtr<ID3D11Query>*	EndTimestamps;
	/** Whether we are currently timing the GPU: between StartTiming() and EndTiming(). */
	bool						bIsTiming;
};

/** Used to track whether a period was disjoint on the GPU, which means GPU timings are invalid. */
class FD3D11DisjointTimeStampQuery : public FRenderResource
{
public:
	FD3D11DisjointTimeStampQuery(class FD3D11DynamicRHI* InD3DRHI);

	void StartTracking();
	void EndTracking();
	bool IsResultValid();
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT GetResult();

	/**
	 * Initializes all D3D resources.
	 */
	virtual void InitDynamicRHI() override;

	/**
	 * Releases all D3D resources.
	 */
	virtual void ReleaseDynamicRHI() override;


private:

	TRefCountPtr<ID3D11Query> DisjointQuery;

	FD3D11DynamicRHI* D3DRHI;
};

/** A single perf event node, which tracks information about a appBeginDrawEvent/appEndDrawEvent range. */
class FD3D11EventNode : public FGPUProfilerEventNode
{
public:
	FD3D11EventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent, class FD3D11DynamicRHI* InRHI) :
		FGPUProfilerEventNode(InName, InParent),
		Timing(InRHI, 1)
	{
		// Initialize Buffered timestamp queries 
		Timing.InitResource(); // can't do this from the RHI thread
	}

	virtual ~FD3D11EventNode()
	{
		Timing.ReleaseResource();  // can't do this from the RHI thread
	}

	/** 
	 * Returns the time in ms that the GPU spent in this draw event.  
	 * This blocks the CPU if necessary, so can cause hitching.
	 */
	virtual float GetTiming() override;


	virtual void StartTiming() override
	{
		Timing.StartTiming();
	}

	virtual void StopTiming() override
	{
		Timing.EndTiming();
	}

	FD3D11BufferedGPUTiming Timing;
};

/** An entire frame of perf event nodes, including ancillary timers. */
class FD3D11EventNodeFrame : public FGPUProfilerEventNodeFrame
{
public:

	FD3D11EventNodeFrame(class FD3D11DynamicRHI* InRHI) :
		FGPUProfilerEventNodeFrame(),
		RootEventTiming(InRHI, 1),
		DisjointQuery(InRHI)
	{
		RootEventTiming.InitResource();
		DisjointQuery.InitResource();
	}

	~FD3D11EventNodeFrame()
	{
		RootEventTiming.ReleaseResource();
		DisjointQuery.ReleaseResource();
	}

	/** Start this frame of per tracking */
	virtual void StartFrame() override;

	/** End this frame of per tracking, but do not block yet */
	virtual void EndFrame() override;

	/** Calculates root timing base frequency (if needed by this RHI) */
	virtual float GetRootTimingResults() override;

	virtual void LogDisjointQuery() override;

	/** Timer tracking inclusive time spent in the root nodes. */
	FD3D11BufferedGPUTiming RootEventTiming;

	/** Disjoint query tracking whether the times reported by DumpEventTree are reliable. */
	FD3D11DisjointTimeStampQuery DisjointQuery;
};

/** 
 * Encapsulates GPU profiling logic and data. 
 * There's only one global instance of this struct so it should only contain global data, nothing specific to a frame.
 */
struct FD3DGPUProfiler : public FGPUProfiler
{
	/** Used to measure GPU time per frame. */
	FD3D11BufferedGPUTiming FrameTiming;

	class FD3D11DynamicRHI* D3D11RHI;

	/** GPU hitch profile histories */
	TIndirectArray<FD3D11EventNodeFrame> GPUHitchEventNodeFrames;

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI 
	bool bRequestProfileForStatUnitVxgi;
	bool bLatchedRequestProfileForStatUnitVxgi;
	float VxgiWorldSpaceTime;
	float VxgiScreenSpaceTime;
#endif
	// NVCHANGE_END: Add VXGI

	FD3DGPUProfiler(class FD3D11DynamicRHI* InD3DRHI) :
		FGPUProfiler(),
		FrameTiming(InD3DRHI, 4),
		D3D11RHI(InD3DRHI)
		// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI 
		, bRequestProfileForStatUnitVxgi(false)
		, bLatchedRequestProfileForStatUnitVxgi(false)
		, VxgiWorldSpaceTime(0.f)
		, VxgiScreenSpaceTime(0.f)
#endif
		// NVCHANGE_END: Add VXGI
	{
		// Initialize Buffered timestamp queries 
		FrameTiming.InitResource();
	}

	virtual FGPUProfilerEventNode* CreateEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent) override
	{
		FD3D11EventNode* EventNode = new FD3D11EventNode(InName, InParent, D3D11RHI);
		return EventNode;
	}

	virtual void PushEvent(const TCHAR* Name, FColor Color) override;
	virtual void PopEvent() override;

	void BeginFrame(class FD3D11DynamicRHI* InRHI);

	void EndFrame();

	bool CheckGpuHeartbeat() const;

private:
	TMap<uint32, FString> CachedStrings;
	TArray<uint32> PushPopStack;
};

/** Forward declare the context for the AMD AGS utility library. */
struct AGSContext;

/** The interface which is implemented by the dynamically bound RHI. */
class D3D11RHI_API FD3D11DynamicRHI : public FDynamicRHI, public IRHICommandContext
{
public:

	friend class FD3D11Viewport;

	/** Global D3D11 lock list */
	TMap<FD3D11LockedKey,FD3D11LockedData> OutstandingLocks;

	/** Initialization constructor. */
	FD3D11DynamicRHI(IDXGIFactory1* InDXGIFactory1,D3D_FEATURE_LEVEL InFeatureLevel,int32 InChosenAdapter, const DXGI_ADAPTER_DESC& ChosenDescription);

	/** Destructor */
	virtual ~FD3D11DynamicRHI();

	/** If it hasn't been initialized yet, initializes the D3D device. */
	virtual void InitD3DDevice();

	// FDynamicRHI interface.
	virtual void Init() override;
	virtual void Shutdown() override;
	virtual const TCHAR* GetName() override { return TEXT("D3D11"); }

	// HDR display output
	virtual void EnableHDR();
	virtual void ShutdownHDR();

	virtual void FlushPendingLogs() override;

	template<typename TRHIType>
	static FORCEINLINE typename TD3D11ResourceTraits<TRHIType>::TConcreteType* ResourceCast(TRHIType* Resource)
	{
		return static_cast<typename TD3D11ResourceTraits<TRHIType>::TConcreteType*>(Resource);
	}

	/**
	 * Reads a D3D query's data into the provided buffer.
	 * @param Query - The D3D query to read data from.
	 * @param Data - The buffer to read the data into.
	 * @param DataSize - The size of the buffer.
	 * @param bWait - If true, it will wait for the query to finish.
	 * @param QueryType e.g. RQT_Occlusion or RQT_AbsoluteTime
	 * @return true if the query finished.
	 */
	bool GetQueryData(ID3D11Query* Query,void* Data,SIZE_T DataSize,bool bWait, ERenderQueryType QueryType);

	virtual void RHIBeginUpdateMultiFrameResource(FTextureRHIParamRef Texture) override;
	virtual void RHIEndUpdateMultiFrameResource(FTextureRHIParamRef Texture) override;

	virtual void RHIBeginUpdateMultiFrameResource(FUnorderedAccessViewRHIParamRef UAV) override;
	virtual void RHIEndUpdateMultiFrameResource(FUnorderedAccessViewRHIParamRef UAV) override;

	virtual FSamplerStateRHIRef RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer) final override;
	virtual FRasterizerStateRHIRef RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer) final override;
	virtual FDepthStencilStateRHIRef RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer) final override;
	virtual FBlendStateRHIRef RHICreateBlendState(const FBlendStateInitializerRHI& Initializer) final override;
	virtual FVertexDeclarationRHIRef RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements) final override;
	virtual FPixelShaderRHIRef RHICreatePixelShader(const TArray<uint8>& Code) final override;
	virtual FVertexShaderRHIRef RHICreateVertexShader(const TArray<uint8>& Code) final override;
	virtual FHullShaderRHIRef RHICreateHullShader(const TArray<uint8>& Code) final override;
	virtual FDomainShaderRHIRef RHICreateDomainShader(const TArray<uint8>& Code) final override;
	virtual FGeometryShaderRHIRef RHICreateGeometryShader(const TArray<uint8>& Code) final override;
	virtual FGeometryShaderRHIRef RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream) final override;
	virtual FComputeShaderRHIRef RHICreateComputeShader(const TArray<uint8>& Code) final override;
	virtual FBoundShaderStateRHIRef RHICreateBoundShaderState(FVertexDeclarationRHIParamRef VertexDeclaration, FVertexShaderRHIParamRef VertexShader, FHullShaderRHIParamRef HullShader, FDomainShaderRHIParamRef DomainShader, FPixelShaderRHIParamRef PixelShader, FGeometryShaderRHIParamRef GeometryShader) final override;
	virtual FUniformBufferRHIRef RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage) final override;
	virtual FIndexBufferRHIRef RHICreateIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual void* RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBuffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode) final override;
	virtual void RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBuffer) final override;
	virtual FVertexBufferRHIRef RHICreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual void* RHILockVertexBuffer(FVertexBufferRHIParamRef VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode) final override;
	virtual void RHIUnlockVertexBuffer(FVertexBufferRHIParamRef VertexBuffer) final override;
	virtual void RHICopyVertexBuffer(FVertexBufferRHIParamRef SourceBuffer,FVertexBufferRHIParamRef DestBuffer) final override;
	virtual FStructuredBufferRHIRef RHICreateStructuredBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual void* RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode) final override;
	virtual void RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBuffer) final override;
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBuffer, bool bUseUAVCounter, bool bAppendBuffer) final override;
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FTextureRHIParamRef Texture, uint32 MipLevel) final override;
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FVertexBufferRHIParamRef VertexBuffer, uint8 Format) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FStructuredBufferRHIParamRef StructuredBuffer) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FIndexBufferRHIParamRef Buffer) final override;
	virtual uint64 RHICalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign) final override;
	virtual uint64 RHICalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign) final override;
	virtual uint64 RHICalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign) final override;
	virtual void RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats) final override;
	virtual bool RHIGetTextureMemoryVisualizeData(FColor* TextureData,int32 SizeX,int32 SizeY,int32 Pitch,int32 PixelSize) final override;
	virtual FTextureReferenceRHIRef RHICreateTextureReference(FLastRenderTimeContainer* LastRenderTime) final override;
	virtual void RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture) final override;
	virtual FTexture2DRHIRef RHICreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual FTexture2DRHIRef RHIAsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, void** InitialMipData, uint32 NumInitialMips) final override;
	virtual void RHICopySharedMips(FTexture2DRHIParamRef DestTexture2D, FTexture2DRHIParamRef SrcTexture2D) final override;
	virtual FTexture2DArrayRHIRef RHICreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual FTexture3DRHIRef RHICreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual void RHIGetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel) final override;
	virtual void RHIGenerateMips(FTextureRHIParamRef Texture) final override;
	virtual uint32 RHIComputeMemorySize(FTextureRHIParamRef TextureRHI) final override;
	virtual FTexture2DRHIRef RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus) final override;
	virtual ETextureReallocationStatus RHIFinalizeAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted) final override;
	virtual ETextureReallocationStatus RHICancelAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted) final override;
	virtual void* RHILockTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void* RHILockTexture2DArray(FTexture2DArrayRHIParamRef Texture, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef Texture, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void RHIUpdateTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData) final override;
	virtual void RHIUpdateTexture3D(FTexture3DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData) final override;
	virtual FTextureCubeRHIRef RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual FTextureCubeRHIRef RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual void* RHILockTextureCubeFace(FTextureCubeRHIParamRef Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void RHIBindDebugLabelName(FTextureRHIParamRef Texture, const TCHAR* Name) final override;
	virtual void RHIReadSurfaceData(FTextureRHIParamRef Texture,FIntRect Rect,TArray<FColor>& OutData,FReadSurfaceDataFlags InFlags) final override;
	virtual void RHIReadSurfaceData(FTextureRHIParamRef TextureRHI, FIntRect InRect, TArray<FLinearColor>& OutData, FReadSurfaceDataFlags InFlags) final override;
	virtual void RHIMapStagingSurface(FTextureRHIParamRef Texture,void*& OutData,int32& OutWidth,int32& OutHeight) final override;
	virtual void RHIUnmapStagingSurface(FTextureRHIParamRef Texture) final override;
	virtual void RHIReadSurfaceFloatData(FTextureRHIParamRef Texture,FIntRect Rect,TArray<FFloat16Color>& OutData,ECubeFace CubeFace,int32 ArrayIndex,int32 MipIndex) final override;
	virtual void RHIRead3DSurfaceFloatData(FTextureRHIParamRef Texture,FIntRect Rect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData) final override;
	virtual FRenderQueryRHIRef RHICreateRenderQuery(ERenderQueryType QueryType) final override;
	virtual bool RHIGetRenderQueryResult(FRenderQueryRHIParamRef RenderQuery, uint64& OutResult, bool bWait) final override;
	virtual FTexture2DRHIRef RHIGetViewportBackBuffer(FViewportRHIParamRef Viewport) final override;
	virtual void RHIAdvanceFrameForGetViewportBackBuffer(FViewportRHIParamRef Viewport) final override;
	virtual void RHIAcquireThreadOwnership() final override;
	virtual void RHIReleaseThreadOwnership() final override;
	virtual void RHIFlushResources() final override;
	virtual uint32 RHIGetGPUFrameCycles() final override;
	virtual FViewportRHIRef RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) final override;
	virtual void RHIResizeViewport(FViewportRHIParamRef Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen) final override;
	virtual void RHIResizeViewport(FViewportRHIParamRef Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) final override;
	virtual void RHITick(float DeltaTime) final override;
	virtual void RHISetStreamOutTargets(uint32 NumTargets,const FVertexBufferRHIParamRef* VertexBuffers,const uint32* Offsets) final override;
	virtual void RHIDiscardRenderTargets(bool Depth,bool Stencil,uint32 ColorBitMask) final override;
	virtual void RHIBlockUntilGPUIdle() final override;
	virtual bool RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate) final override;
	virtual void RHIGetSupportedResolution(uint32& Width, uint32& Height) final override;
	virtual void RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef Texture, uint32 FirstMip) final override;
	virtual void RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef Texture, uint32 FirstMip) final override;
	virtual void RHIExecuteCommandList(FRHICommandList* CmdList) final override;
	virtual void* RHIGetNativeDevice() final override;
	virtual class IRHICommandContext* RHIGetDefaultContext() final override;
	virtual class IRHICommandContextContainer* RHIGetCommandContextContainer(int32 Index, int32 Num) final override;



	virtual void RHISetComputeShader(FComputeShaderRHIParamRef ComputeShader) final override;
	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) final override;
	virtual void RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIAutomaticCacheFlushAfterComputeShader(bool bEnable) final override;
	virtual void RHIFlushComputeShaderCache() final override;
	virtual void RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) final override;
	virtual void RHIClearTinyUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values) final override;
	virtual void RHICopyToResolveTarget(FTextureRHIParamRef SourceTexture, FTextureRHIParamRef DestTexture, bool bKeepOriginalSurface, const FResolveParams& ResolveParams) final override;
	virtual void RHITransitionResources(EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InTextures, int32 NumTextures) final override;
	virtual void RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteFence) final override;
	virtual void RHIBeginRenderQuery(FRenderQueryRHIParamRef RenderQuery) final override;
	virtual void RHIEndRenderQuery(FRenderQueryRHIParamRef RenderQuery) final override;
	virtual void RHIBeginOcclusionQueryBatch() final override;
	virtual void RHIEndOcclusionQueryBatch() final override;
	virtual void RHISubmitCommandsHint() final override;
	virtual void RHIBeginDrawingViewport(FViewportRHIParamRef Viewport, FTextureRHIParamRef RenderTargetRHI) final override;
	virtual void RHIEndDrawingViewport(FViewportRHIParamRef Viewport, bool bPresent, bool bLockToVsync) final override;
	virtual void RHIBeginFrame() final override;
	virtual void RHIEndFrame() final override;
	virtual void RHIBeginScene() final override;
	virtual void RHIEndScene() final override;
	virtual void RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint32 Offset) final override;
	virtual void RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Offset) final override;
	virtual void RHISetRasterizerState(FRasterizerStateRHIParamRef NewState) final override;
	virtual void RHISetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ) final override;
	virtual void RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY) final override;
	virtual void RHISetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderState) final override;
	virtual void RHISetGraphicsPipelineState(FGraphicsPipelineStateRHIParamRef GraphicsState) final override
	{
		FRHIGraphicsPipelineStateFallBack* FallbackGraphicsState = static_cast<FRHIGraphicsPipelineStateFallBack*>(GraphicsState);
		IRHICommandContext::RHISetGraphicsPipelineState(GraphicsState);
		// Store the PSO's primitive (after since IRHICommandContext::RHISetGraphicsPipelineState sets the BSS)
		PSOPrimitiveType = FallbackGraphicsState->Initializer.PrimitiveType;
	}

	virtual void RHISetShaderTexture(FVertexShaderRHIParamRef VertexShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FHullShaderRHIParamRef HullShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FDomainShaderRHIParamRef DomainShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FPixelShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FComputeShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FVertexShaderRHIParamRef VertexShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV) final override;
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
	virtual void RHISetBlendState(FBlendStateRHIParamRef NewState, const FLinearColor& BlendFactor) final override;
	virtual void RHISetBlendFactor(const FLinearColor& BlendFactor) final override;
	virtual void RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs) final override;
	virtual void RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo) final override;
	virtual void RHIBindClearMRTValues(bool bClearColor, bool bClearDepth, bool bClearStencil) final override;
	virtual void RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	virtual void RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances) final override;
	virtual void RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBuffer, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	virtual void RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType, FIndexBufferRHIParamRef IndexBuffer, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIBeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData) final override;
	virtual void RHIEndDrawPrimitiveUP() final override;
	virtual void RHIBeginDrawIndexedPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData) final override;
	virtual void RHIEndDrawIndexedPrimitiveUP() final override;
	virtual void RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth) final override;
	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) final override;
	virtual void RHIPopEvent() final override;

	virtual void RHICopySubTextureRegion(FTexture2DRHIParamRef SourceTexture, FTexture2DRHIParamRef DestinationTexture, FBox2D SourceBox, FBox2D DestinationBox) final override;

	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	virtual void ClearStateCache() final override;
	virtual bool GetPlatformDesc(NvVl::PlatformDesc& PlatformDesc) final override;
	virtual void GetPlatformRenderCtx(NvVl::PlatformRenderCtx& PlatformRenderCtx) final override;
	virtual void GetPlatformShaderResource(FTextureRHIParamRef TextureRHI, NvVl::PlatformShaderResource& PlatformShaderResource) final override;
	virtual void GetPlatformRenderTarget(FTextureRHIParamRef TextureRHI, NvVl::PlatformRenderTarget& PlatformRenderTarget) final override;
#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting

	virtual FTexture2DRHIRef RHICreateTexture2DFromResource(EPixelFormat Format, uint32 TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D11Texture2D* Resource);
	virtual FTextureCubeRHIRef RHICreateTextureCubeFromResource(EPixelFormat Format, uint32 TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D11Texture2D* Resource);
	virtual void RHIAliasTextureResources(FTextureRHIParamRef DestTexture, FTextureRHIParamRef SrcTexture);

	// NvFlow begin
	virtual void NvFlowGetDeviceDesc(FRHINvFlowDeviceDesc* desc) override;
	virtual void NvFlowGetDepthStencilViewDesc(FTexture2DRHIParamRef depthSurface, FTexture2DRHIParamRef depthTexture, FRHINvFlowDepthStencilViewDesc* desc) override;
	virtual void NvFlowGetRenderTargetViewDesc(FRHINvFlowRenderTargetViewDesc* desc) override;
	virtual FShaderResourceViewRHIRef NvFlowCreateSRV(const FRHINvFlowResourceViewDesc* desc) override;
	virtual FRHINvFlowResourceRW* NvFlowCreateResourceRW(const FRHINvFlowResourceRWViewDesc* desc, FShaderResourceViewRHIRef* pRHIRefSRV, FUnorderedAccessViewRHIRef* pRHIRefUAV) override;
	// NvFlow end
	
	// WaveWorks Start
	virtual const TArray<WaveWorksShaderInput>* RHIGetWaveWorksShaderInput() final override;
	virtual const TArray<WaveWorksShaderInput>* RHIGetWaveWorksQuadTreeShaderInput() final override;
	virtual FWaveWorksRHIRef RHICreateWaveWorks(const struct GFSDK_WaveWorks_Simulation_Settings& Settings, const struct GFSDK_WaveWorks_Simulation_Params& Params) final override;
	virtual void RHISetWaveWorksState(FWaveWorksRHIParamRef State, const FMatrix& ViewMatrix, const TArray<uint32>& ShaderInputMappings) final override;

	void CommitResources();
	void CacheWaveWorksQuadTreeState(const TArray<uint32>& ShaderInputMappings);
	// WaveWorks End
	
	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	NVRHI::FRendererInterfaceD3D11* VxgiRendererD3D11;
	virtual VXGI::IGlobalIllumination* RHIVXGIGetInterface() final override;
	virtual void RHIVXGISetVoxelizationParameters(const VXGI::VoxelizationParameters& Parameters) final override;
	virtual void RHIVXGISetPixelShaderResourceAttributes(NVRHI::ShaderHandle PixelShader, const TArray<uint8>& ShaderResourceTable, bool bUsesGlobalCB) final override;
	virtual void RHIVXGIApplyDrawStateOverrideShaders(const NVRHI::DrawCallState& DrawCallState, const FBoundShaderStateInput* BoundShaderStateInput, EPrimitiveType PrimitiveTypeOverride) final override;
	virtual void RHIVXGIApplyShaderResources(const NVRHI::DrawCallState& DrawCallState) final override;
	virtual void RHIVXGISetCommandList(FRHICommandList* RHICommandList) final override;
	virtual FRHITexture* GetRHITextureFromVXGI(NVRHI::TextureHandle texture) final override;
	virtual NVRHI::TextureHandle GetVXGITextureFromRHI(FRHITexture* texture) final override;
	virtual void RHIVXGIGetGPUTime(float& OutWorldSpaceTime, float& OutScreenSpaceTime) final override;
	virtual void RHIVXGICleanupAfterVoxelization() final override;
	virtual void RHISetViewportsAndScissorRects(uint32 Count, const FViewportBounds* Viewports, const FScissorRect* ScissorRects) final override;
	virtual void RHIDispatchIndirectComputeShaderStructured(FStructuredBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHICopyStructuredBufferData(FStructuredBufferRHIParamRef DestBuffer, uint32 DestOffset, FStructuredBufferRHIParamRef SrcBuffer, uint32 SrcOffset, uint32 DataSize) final override;
	virtual void RHIExecuteVxgiRenderingCommand(NVRHI::IRenderThreadCommand* Command) final override;
private:
	VXGI::IGlobalIllumination* VxgiInterface;
	VXGI::VoxelizationParameters VxgiVoxelizationParameters;
	bool bVxgiVoxelizationParametersSet;
	void CreateVxgiInterface();
	void ReleaseVxgiInterface();
public:
#endif
	// NVCHANGE_END: Add VXGI

	// NVCHANGE_BEGIN: Add HBAO+
#if WITH_GFSDK_SSAO
	virtual void RHIRenderHBAO(
		const FTextureRHIParamRef SceneDepthTextureRHI,
		const FMatrix& ProjectionMatrix,
		const FTextureRHIParamRef SceneNormalTextureRHI,
		const FMatrix& ViewMatrix,
		const FTextureRHIParamRef SceneColorTextureRHI,
		const GFSDK_SSAO_Parameters& AOParams) final override;
#endif
	// NVCHANGE_END: Add HBAO+

	// Accessors.	
	ID3D11Device* GetDevice() const
	{
		return Direct3DDevice;
	}
	FD3D11DeviceContext* GetDeviceContext() const
	{
		return Direct3DDeviceIMContext;
	}

	IDXGIFactory1* GetFactory() const
	{
		return DXGIFactory1;
	}

	bool CheckGpuHeartbeat() const override
	{
		return GPUProfilingData.CheckGpuHeartbeat();
	}

private:
	void RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil);

	enum class EForceFullScreenClear
	{
		EDoNotForce,
		EForce
	};

	virtual void RHIClearMRTImpl(bool bClearColor, int32 NumClearColors, const FLinearColor* ColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil);

	template <EShaderFrequency ShaderFrequency>
	void ClearShaderResourceViews(FD3D11BaseShaderResource* Resource);

	template <EShaderFrequency ShaderFrequency>
	void ClearAllShaderResourcesForFrequency();

	void CheckIfSRVIsResolved(ID3D11ShaderResourceView* SRV);

	template <EShaderFrequency ShaderFrequency>
	void InternalSetShaderResourceView(FD3D11BaseShaderResource* Resource, ID3D11ShaderResourceView* SRV, int32 ResourceIndex, FName SRVName, FD3D11StateCache::ESRV_Type SrvType = FD3D11StateCache::SRV_Unknown);

	void SetCurrentComputeShader(FComputeShaderRHIParamRef ComputeShader)
	{
		CurrentComputeShader = ComputeShader;
	}
	
	const FComputeShaderRHIRef& GetCurrentComputeShader() const
	{
		return CurrentComputeShader;
	}

public:

	template <EShaderFrequency ShaderFrequency>
	void SetShaderResourceView(FD3D11BaseShaderResource* Resource, ID3D11ShaderResourceView* SRV, int32 ResourceIndex, FName SRVName, FD3D11StateCache::ESRV_Type SrvType = FD3D11StateCache::SRV_Unknown)
	{
		InternalSetShaderResourceView<ShaderFrequency>(Resource, SRV, ResourceIndex, SRVName, SrvType);
	}

	void ClearState();
	void ConditionalClearShaderResource(FD3D11BaseShaderResource* Resource);
	void ClearAllShaderResources();


	static DXGI_FORMAT GetPlatformTextureResourceFormat(DXGI_FORMAT InFormat, uint32 InFlags);

#if	PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	virtual void* CreateVirtualTexture(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, bool bCubeTexture, uint32 Flags, void* D3DTextureDesc, void* D3DTextureResource) = 0;
	virtual void DestroyVirtualTexture(uint32 Flags, void* RawTextureMemory) = 0;
	virtual bool HandleSpecialLock(FD3D11LockedData& LockedData, uint32 MipIndex, uint32 ArrayIndex, uint32 Flags, EResourceLockMode LockMode,
		void* D3DTextureResource, void* RawTextureMemory, uint32 NumMips, uint32& DestStride) = 0;
	virtual bool HandleSpecialUnlock(uint32 MipIndex, uint32 Flags, void* D3DTextureResource, void* RawTextureMemory) = 0;
#endif

	uint32 GetHDRDetectedDisplayIndex() const
	{
		return HDRDetectedDisplayIndex;
	}

	void SetHDRDetectedDisplayIndices(const uint32 DisplayIndex, const uint32 IHVIndex)
	{
		HDRDetectedDisplayIndex = DisplayIndex;
		HDRDetectedDisplayIHVIndex = IHVIndex;
	}

protected:
	/** The global D3D interface. */
	TRefCountPtr<IDXGIFactory1> DXGIFactory1;

	/** The global D3D device's immediate context */
	TRefCountPtr<FD3D11DeviceContext> Direct3DDeviceIMContext;

	/** The global D3D device's immediate context */
	TRefCountPtr<FD3D11Device> Direct3DDevice;

	// NVCHANGE_BEGIN: Add HBAO+
#if WITH_GFSDK_SSAO
	GFSDK_SSAO_Context_D3D11* HBAOContext;
	HMODULE HBAOModuleHandle;
#endif
	// NVCHANGE_END: Add HBAO+

	FD3D11StateCache StateCache;

	/** A list of all viewport RHIs that have been created. */
	TArray<FD3D11Viewport*> Viewports;

	/** The viewport which is currently being drawn. */
	TRefCountPtr<FD3D11Viewport> DrawingViewport;

	/** The feature level of the device. */
	D3D_FEATURE_LEVEL FeatureLevel;

	/**
	 * The context for the AMD AGS utility library.
	 * AGSContext does not implement AddRef/Release.
	 * Just use a bare pointer.
	 */
	AGSContext* AmdAgsContext;

	// set by UpdateMSAASettings(), get by GetMSAAQuality()
	// [SampleCount] = Quality, 0xffffffff if not supported
	uint32 AvailableMSAAQualities[DX_MAX_MSAA_COUNT + 1];

	/** A buffer in system memory containing all zeroes of the specified size. */
	void* ZeroBuffer;
	uint32 ZeroBufferSize;

	// Tracks the currently set state blocks.
	bool bCurrentDepthStencilStateIsReadOnly;

	// Current PSO Primitive Type
	EPrimitiveType PSOPrimitiveType;

	TRefCountPtr<ID3D11RenderTargetView> CurrentRenderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	TRefCountPtr<ID3D11UnorderedAccessView> CurrentUAVs[D3D11_PS_CS_UAV_REGISTER_COUNT];
	TRefCountPtr<ID3D11DepthStencilView> CurrentDepthStencilTarget;
	TRefCountPtr<FD3D11TextureBase> CurrentDepthTexture;
	FD3D11BaseShaderResource* CurrentResourcesBoundAsSRVs[SF_NumFrequencies][D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	int32 MaxBoundShaderResourcesIndex[SF_NumFrequencies];
	uint32 NumSimultaneousRenderTargets;
	uint32 NumUAVs;

	/** Internal frame counter, incremented on each call to RHIBeginScene. */
	uint32 SceneFrameCounter;

	/** Internal frame counter that just counts calls to Present */
	uint32 PresentCounter;

	/**
	 * Internal counter used for resource table caching.
	 * INDEX_NONE means caching is not allowed.
	 */
	uint32 ResourceTableFrameCounter;

	/** D3D11 defines a maximum of 14 constant buffers per shader stage. */
	enum { MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE = 14 };

	/** Track the currently bound uniform buffers. */
	FUniformBufferRHIRef BoundUniformBuffers[SF_NumFrequencies][MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE];

	/** Bit array to track which uniform buffers have changed since the last draw call. */
	uint16 DirtyUniformBuffers[SF_NumFrequencies];

	/** Tracks the current depth stencil access type. */
	FExclusiveDepthStencil CurrentDSVAccessType;

	/** When a new shader is set, we discard all old constants set for the previous shader. */
	bool bDiscardSharedConstants;

	/** Set to true when the current shading setup uses tessellation */
	bool bUsingTessellation;

	/** Dynamic vertex and index buffers. */
	TRefCountPtr<FD3D11DynamicBuffer> DynamicVB;
	TRefCountPtr<FD3D11DynamicBuffer> DynamicIB;

	// State for begin/end draw primitive UP interface.
	uint32 PendingNumVertices;
	uint32 PendingVertexDataStride;
	uint32 PendingPrimitiveType;
	uint32 PendingNumPrimitives;
	uint32 PendingMinVertexIndex;
	uint32 PendingNumIndices;
	uint32 PendingIndexDataStride;

	/** A list of all D3D constant buffers RHIs that have been created. */
	TArray<TRefCountPtr<FD3D11ConstantBuffer> > VSConstantBuffers;
	TArray<TRefCountPtr<FD3D11ConstantBuffer> > HSConstantBuffers;
	TArray<TRefCountPtr<FD3D11ConstantBuffer> > DSConstantBuffers;
	TArray<TRefCountPtr<FD3D11ConstantBuffer> > PSConstantBuffers;
	TArray<TRefCountPtr<FD3D11ConstantBuffer> > GSConstantBuffers;
	TArray<TRefCountPtr<FD3D11ConstantBuffer> > CSConstantBuffers;

	/** A history of the most recently used bound shader states, used to keep transient bound shader states from being recreated for each use. */
	TGlobalResource< TBoundShaderStateHistory<10000> > BoundShaderStateHistory;
	FComputeShaderRHIRef CurrentComputeShader;

	/** If HDR display detected, we store the output device. */
	uint32 HDRDetectedDisplayIndex;
	uint32 HDRDetectedDisplayIHVIndex;

#if CHECK_SRV_TRANSITIONS
	/*
	 * Rendertargets must be explicitly 'resolved' to manage their transition to an SRV on some platforms and DX12
	 * We keep track of targets that need 'resolving' to provide safety asserts at SRV binding time.
	 */
	struct FUnresolvedRTInfo
	{
		FUnresolvedRTInfo(FName InResourceName, int32 InMipLevel, int32 InNumMips, int32 InArraySlice, int32 InArraySize)
		: ResourceName(InResourceName)
		, MipLevel(InMipLevel)
		, NumMips(InNumMips)
		, ArraySlice(InArraySlice)  
		, ArraySize(InArraySize)
		{
		}

		bool operator==(const FUnresolvedRTInfo& Other) const
		{
			return MipLevel == Other.MipLevel &&
				NumMips == Other.NumMips &&
				ArraySlice == Other.ArraySlice &&
				ArraySize == Other.ArraySize;
		}

		FName ResourceName;
		int32 MipLevel;
		int32 NumMips;
		int32 ArraySlice;
		int32 ArraySize;
	};
	FThreadSafeCounter UnresolvedTargetsConcurrencyGuard;
	TMultiMap<ID3D11Resource*, FUnresolvedRTInfo> UnresolvedTargets;
#endif

	FD3DGPUProfiler GPUProfilingData;
	// >= 0, was computed before, unless hardware was changed during engine init it should be the same
	int32 ChosenAdapter;
	// we don't use AdapterDesc.Description as there is a bug with Optimus where it can report the wrong name
	DXGI_ADAPTER_DESC ChosenDescription;

	template<typename BaseResourceType>
	TD3D11Texture2D<BaseResourceType>* CreateD3D11Texture2D(uint32 SizeX,uint32 SizeY,uint32 SizeZ,bool bTextureArray,bool CubeTexture,uint8 Format,
		uint32 NumMips,uint32 NumSamples,uint32 Flags,FRHIResourceCreateInfo& CreateInfo);

	FD3D11Texture3D* CreateD3D11Texture3D(uint32 SizeX,uint32 SizeY,uint32 SizeZ,uint8 Format,uint32 NumMips,uint32 Flags,FRHIResourceCreateInfo& CreateInfo);

	template<typename BaseResourceType>
	TD3D11Texture2D<BaseResourceType>* CreateTextureFromResource(bool bTextureArray, bool bCubeTexture, EPixelFormat Format, uint32 TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D11Texture2D* TextureResource);

	/** Initializes the constant buffers.  Called once at RHI initialization time. */
	void InitConstantBuffers();

	/** needs to be called before each draw call */
	virtual void CommitNonComputeShaderConstants();

	/** needs to be called before each dispatch call */
	virtual void CommitComputeShaderConstants();

	template <class ShaderType> void SetResourcesFromTables(const ShaderType* RESTRICT);
	void CommitGraphicsResourceTables();
	void CommitComputeResourceTables(FD3D11ComputeShader* ComputeShader);

	void ValidateExclusiveDepthStencilAccess(FExclusiveDepthStencil Src) const;

	/** 
	 * Gets the best supported MSAA settings from the provided MSAA count to check against. 
	 * 
	 * @param PlatformFormat		The format of the texture being created 
	 * @param MSAACount				The MSAA count to check against. 
	 * @param OutBestMSAACount		The best MSAA count that is suppored.  Could be smaller than MSAACount if it is not supported 
	 * @param OutMSAAQualityLevels	The number MSAA quality levels for the best msaa count supported
	 */
	void GetBestSupportedMSAASetting( DXGI_FORMAT PlatformFormat, uint32 MSAACount, uint32& OutBestMSAACount, uint32& OutMSAAQualityLevels );

	// shared code for different D3D11 devices (e.g. PC DirectX11 and XboxOne) called
	// after device creation and GRHISupportsAsyncTextureCreation was set and before resource init
	void SetupAfterDeviceCreation();

	// called by SetupAfterDeviceCreation() when the device gets initialized
	void UpdateMSAASettings();

	// @return 0xffffffff if not not supported
	uint32 GetMaxMSAAQuality(uint32 SampleCount);

	void CommitRenderTargetsAndUAVs();

	/**
	 * Cleanup the D3D device.
	 * This function must be called from the main game thread.
	 */
	virtual void CleanupD3DDevice();

	void ReleasePooledUniformBuffers();

	template<typename TPixelShader>
	void ResolveTextureUsingShader(
		FRHICommandList_RecursiveHazardous& RHICmdList,
		FD3D11Texture2D* SourceTexture,
		FD3D11Texture2D* DestTexture,
		ID3D11RenderTargetView* DestSurfaceRTV,
		ID3D11DepthStencilView* DestSurfaceDSV,
		const D3D11_TEXTURE2D_DESC& ResolveTargetDesc,
		const FResolveRect& SourceRect,
		const FResolveRect& DestRect,
			FD3D11DeviceContext* Direct3DDeviceContext, 
			typename TPixelShader::FParameter PixelShaderParameter
		);

	// Some platforms might want to override this
	virtual void SetScissorRectIfRequiredWhenSettingViewport(uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
	{
		RHISetScissorRect(true, MinX, MinY, MaxX, MaxY);
	}

	/**
	* Returns a pointer to a texture resource that can be used for CPU reads.
	* Note: the returned resource could be the original texture or a new temporary texture.
	* @param TextureRHI - Source texture to create a staging texture from.
	* @param InRect - rectangle to 'stage'.
	* @param StagingRectOUT - parameter is filled with the rectangle to read from the returned texture.
	* @return The CPU readable Texture object.
	*/
	TRefCountPtr<ID3D11Texture2D> GetStagingTexture(FTextureRHIParamRef TextureRHI,FIntRect InRect, FIntRect& OutRect, FReadSurfaceDataFlags InFlags);

	void ReadSurfaceDataNoMSAARaw(FTextureRHIParamRef TextureRHI,FIntRect Rect,TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags);

	void ReadSurfaceDataMSAARaw(FRHICommandList_RecursiveHazardous& RHICmdList, FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags);

	friend struct FD3DGPUProfiler;

};

struct FD3D11Adapter
{
	/** -1 if not supported or FindAdpater() wasn't called. Ideally we would store a pointer to IDXGIAdapter but it's unlikely the adpaters change during engine init. */
	int32 AdapterIndex;
	/** The maximum D3D11 feature level supported. 0 if not supported or FindAdpater() wasn't called */
	D3D_FEATURE_LEVEL MaxSupportedFeatureLevel;

	// constructor
	FD3D11Adapter(int32 InAdapterIndex = -1, D3D_FEATURE_LEVEL InMaxSupportedFeatureLevel = (D3D_FEATURE_LEVEL)0)
		: AdapterIndex(InAdapterIndex)
		, MaxSupportedFeatureLevel(InMaxSupportedFeatureLevel)
	{
	}

	bool IsValid() const
	{
		return MaxSupportedFeatureLevel != (D3D_FEATURE_LEVEL)0 && AdapterIndex >= 0;
	}
};

/** Implements the D3D11RHI module as a dynamic RHI providing module. */
class FD3D11DynamicRHIModule : public IDynamicRHIModule
{
public:
	// IModuleInterface	
	virtual bool SupportsDynamicReloading() override { return false; }
	virtual void StartupModule() override;

	// IDynamicRHIModule
	virtual bool IsSupported() override;
	virtual FDynamicRHI* CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel = ERHIFeatureLevel::Num) override;

private:
	FD3D11Adapter ChosenAdapter;
	// we don't use GetDesc().Description as there is a bug with Optimus where it can report the wrong name
	DXGI_ADAPTER_DESC ChosenDescription;

	// set MaxSupportedFeatureLevel and ChosenAdapter
	void FindAdapter();
};

/** Find an appropriate DXGI format for the input format and SRGB setting. */
inline DXGI_FORMAT FindShaderResourceDXGIFormat(DXGI_FORMAT InFormat,bool bSRGB)
{
	if(bSRGB)
	{
		switch(InFormat)
		{
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:    return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		case DXGI_FORMAT_BC1_TYPELESS:         return DXGI_FORMAT_BC1_UNORM_SRGB;
		case DXGI_FORMAT_BC2_TYPELESS:         return DXGI_FORMAT_BC2_UNORM_SRGB;
		case DXGI_FORMAT_BC3_TYPELESS:         return DXGI_FORMAT_BC3_UNORM_SRGB;
		case DXGI_FORMAT_BC7_TYPELESS:         return DXGI_FORMAT_BC7_UNORM_SRGB;
		};
	}
	else
	{
		switch(InFormat)
		{
		case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
		case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
		case DXGI_FORMAT_BC1_TYPELESS:      return DXGI_FORMAT_BC1_UNORM;
		case DXGI_FORMAT_BC2_TYPELESS:      return DXGI_FORMAT_BC2_UNORM;
		case DXGI_FORMAT_BC3_TYPELESS:      return DXGI_FORMAT_BC3_UNORM;
		case DXGI_FORMAT_BC7_TYPELESS:      return DXGI_FORMAT_BC7_UNORM;
		};
	}
	switch(InFormat)
	{
		case DXGI_FORMAT_R24G8_TYPELESS: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_R32_FLOAT;
		case DXGI_FORMAT_R16_TYPELESS: return DXGI_FORMAT_R16_UNORM;
			// NVCHANGE_BEGIN: Add VXGI
		case DXGI_FORMAT_R8_TYPELESS: return DXGI_FORMAT_R8_UNORM;
			// NVCHANGE_END: Add VXGI
#if DEPTH_32_BIT_CONVERSION
		// Changing Depth Buffers to 32 bit on Dingo as D24S8 is actually implemented as a 32 bit buffer in the hardware
		case DXGI_FORMAT_R32G8X24_TYPELESS: return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS; 
#endif
	}
	return InFormat;
}

/** Find an appropriate DXGI format unordered access of the raw format. */
inline DXGI_FORMAT FindUnorderedAccessDXGIFormat(DXGI_FORMAT InFormat)
{
	switch(InFormat)
	{
		case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
		case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	return InFormat;
}

/** Find the appropriate depth-stencil targetable DXGI format for the given format. */
inline DXGI_FORMAT FindDepthStencilDXGIFormat(DXGI_FORMAT InFormat)
{
	switch(InFormat)
	{
		case DXGI_FORMAT_R24G8_TYPELESS:
			return DXGI_FORMAT_D24_UNORM_S8_UINT;
#if DEPTH_32_BIT_CONVERSION
		// Changing Depth Buffers to 32 bit on Dingo as D24S8 is actually implemented as a 32 bit buffer in the hardware
		case DXGI_FORMAT_R32G8X24_TYPELESS:
			return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
#endif
		case DXGI_FORMAT_R32_TYPELESS:
			return DXGI_FORMAT_D32_FLOAT;
		case DXGI_FORMAT_R16_TYPELESS:
			return DXGI_FORMAT_D16_UNORM;
	};
	return InFormat;
}

/** 
 * Returns whether the given format contains stencil information.  
 * Must be passed a format returned by FindDepthStencilDXGIFormat, so that typeless versions are converted to their corresponding depth stencil view format.
 */
inline bool HasStencilBits(DXGI_FORMAT InFormat)
{
	switch(InFormat)
	{
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return true;
#if  DEPTH_32_BIT_CONVERSION
	// Changing Depth Buffers to 32 bit on Dingo as D24S8 is actually implemented as a 32 bit buffer in the hardware
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return true;
#endif
	};
	return false;
}

/**
 *	Default 'Fast VRAM' allocator...
 */
class FFastVRAMAllocator
{
public:
	FFastVRAMAllocator() {}

	virtual ~FFastVRAMAllocator() {}

	/**
	 *	IMPORTANT: This function CAN modify the TextureDesc!
	 */
	virtual FVRamAllocation AllocTexture2D(D3D11_TEXTURE2D_DESC& TextureDesc)
	{
		return FVRamAllocation();
	}

	/**
	 *	IMPORTANT: This function CAN modify the TextureDesc!
	 */
	virtual FVRamAllocation AllocTexture3D(D3D11_TEXTURE3D_DESC& TextureDesc)
	{
		return FVRamAllocation();
	}

	/**
	 *	IMPORTANT: This function CAN modify the BufferDesc!
	 */
	virtual FVRamAllocation AllocUAVBuffer(D3D11_BUFFER_DESC& BufferDesc)
	{
		return FVRamAllocation();
	}

	template< typename t_A, typename t_B >
	static t_A RoundUpToNextMultiple( const t_A& a, const t_B& b )
	{
		return ( ( a - 1 ) / b + 1 ) * b;
	}

	static FFastVRAMAllocator* GetFastVRAMAllocator();
};





// 1d, 31 bit (uses the sign bit for internal use), O(n) where n is the amount of elements stored
// does not enforce any alignment
// unoccupied regions get compacted but occupied don't get compacted
class FRangeAllocator
{
public:

	struct FRange
	{
		// not valid
		FRange()
			: Start(0)
			, Size(0)
		{
			check(!IsValid());
		}

		void SetOccupied(int32 InStart, int32 InSize)
		{
			check(InStart >= 0);
			check(InSize > 0);

			Start = InStart;
			Size = InSize;
			check(IsOccupied());
		}

		void SetUnOccupied(int32 InStart, int32 InSize)
		{
			check(InStart >= 0);
			check(InSize > 0);

			Start = InStart;
			Size = -InSize;
			check(!IsOccupied());
		}

		bool IsValid() { return Size != 0; }

		bool IsOccupied() const { return Size > 0; }
		uint32 ComputeSize() const { return (Size > 0) ? Size : -Size; }

		// @apram InSize can be <0 to remove from the size
		void ExtendUnoccupied(int32 InSize) { check(!IsOccupied()); Size -= InSize; }

		void MakeOccupied(int32 InSize) { check(InSize > 0); check(!IsOccupied()); Size = InSize; }
		void MakeUnOccupied() { check(IsOccupied()); Size = -Size; }

		bool operator==(const FRange& rhs) const { return Start == rhs.Start && Size == rhs.Size; }

		int32 GetStart() { return Start; }
		int32 GetEnd() { return Start + ComputeSize(); }

	private:
		// in bytes
		int32 Start;
		// in bytes, 0:not valid, <0:unoccupied, >0:occupied
		int32 Size;
	};
public:

	// constructor
	FRangeAllocator(uint32 TotalSize)
	{
		FRange NewRange;

		NewRange.SetUnOccupied(0, TotalSize);

		Entries.Add(NewRange);
	}

	// specified range must be non occupied
	void OccupyRange(FRange InRange)
	{
		check(InRange.IsValid());
		check(InRange.IsOccupied());

		for(uint32 i = 0, Num = Entries.Num(); i < Num; ++i)
		{
			FRange& ref = Entries[i];

			if(!ref.IsOccupied())
			{
				int32 OverlapSize = ref.GetEnd() - InRange.GetStart();

				if(OverlapSize > 0)
				{
					int32 FrontCutSize = InRange.GetStart() - ref.GetStart();

					// there is some front part we cut off
					if(FrontCutSize > 0)
					{
						FRange NewFrontRange;

						NewFrontRange.SetUnOccupied(InRange.GetStart(), ref.ComputeSize() - FrontCutSize);

						ref.SetUnOccupied(ref.GetStart(), FrontCutSize);

						++i;

						// remaining is added behind the found element
						Entries.Insert(NewFrontRange, i);

						// don't access ref or Num any more - Entries[] might be reallocated
					}

					check(Entries[i].GetStart() == InRange.GetStart());

					int32 BackCutSize = Entries[i].ComputeSize() - InRange.ComputeSize();

					// otherwise the range was already occupied or not enough space was left (internal error)
					check(BackCutSize >= 0);

					// there is some back part we cut off
					if(BackCutSize > 0)
					{
						FRange NewBackRange;

						NewBackRange.SetUnOccupied(Entries[i].GetStart() + InRange.ComputeSize(), BackCutSize);

						Entries.Insert(NewBackRange, i + 1);
					}

					Entries[i] = InRange;
					return;
				}
			}
		}
	}


	// All resources in ESRAM must be 64KiB aligned
//	uint32 AlignedByteOffset = FFastVRAMAllocator::RoundUpToNextMultiple(ESRAMByteOffset, ESRAMMinumumAlignment );


	// @param InSize >0
	FRange AllocRange(uint32 InSize)//, uint32 Alignment)
	{
		check(InSize > 0);

		for(uint32 i = 0, Num = Entries.Num(); i < Num; ++i)
		{
			FRange& ref = Entries[i];

			if(!ref.IsOccupied())
			{
				uint32 RefSize = ref.ComputeSize();

				// take the first fitting one - later we could optimize for minimal fragmentation
				if(RefSize >= InSize)
				{
					ref.MakeOccupied(InSize);

					FRange Ret = ref;

					if(RefSize > InSize)
					{
						FRange NewRange;

						NewRange.SetUnOccupied(ref.GetEnd(), RefSize - InSize);

						// remaining is added behind the found element
						Entries.Insert(NewRange, i + 1);
					}
					return Ret;
				}
			}
		}

		// nothing found
		return FRange();
	}

	// @param In needs to be what was returned by AllocRange()
	void ReleaseRange(FRange In)
	{
		int32 Index = Entries.Find(In);

		check(Index != INDEX_NONE);

		FRange& refIndex = Entries[Index];

		refIndex.MakeUnOccupied();

		Compacten(Index);
	}

	// for debugging
	uint32 GetNumEntries() const { return Entries.Num(); }

	// for debugging
	uint32 ComputeUnoccupiedSize() const
	{
		uint32 Ret = 0;

		for(uint32 i = 0, Num = Entries.Num(); i < Num; ++i)
		{
			const FRange& ref = Entries[i];

			if(!ref.IsOccupied())
			{
				uint32 RefSize = ref.ComputeSize();

				Ret += RefSize;
			}
		}

		return Ret;
	}

private:
	// compact unoccupied ranges
	void Compacten(uint32 StartIndex)
	{
		check(!Entries[StartIndex].IsOccupied());

		if(StartIndex && !Entries[StartIndex-1].IsOccupied())
		{
			// Seems we can combine with the element before,
			// searching further is not needed as we assume the buffer was compact before the last change.
			--StartIndex;
		}

		uint32 ElementsToRemove = 0;
		uint32 SizeGained = 0;

		for(uint32 i = StartIndex + 1, Num = Entries.Num(); i < Num; ++i)
		{
			FRange& ref = Entries[i];

			if(!ref.IsOccupied())
			{
				++ElementsToRemove;
				SizeGained += ref.ComputeSize();
			}
			else
			{
				break;
			}
		}

		if(ElementsToRemove)
		{
			Entries.RemoveAt(StartIndex + 1, ElementsToRemove, false);
			Entries[StartIndex].ExtendUnoccupied(SizeGained);
		}
	}

public:
	static void Test()
	{
		// testing code
#if !UE_BUILD_SHIPPING
		{
			// create
			FRangeAllocator A(10);
			check(A.GetNumEntries() == 1);
			check(A.ComputeUnoccupiedSize() == 10);

			// successfully alloc
			FRangeAllocator::FRange a = A.AllocRange(3);
			check(a.GetStart() == 0);
			check(a.GetEnd() == 3);
			check(a.IsOccupied());
			check(A.GetNumEntries() == 2);
			check(A.ComputeUnoccupiedSize() == 7);

			// successfully alloc
			FRangeAllocator::FRange b = A.AllocRange(4);
			check(b.GetStart() == 3);
			check(b.GetEnd() == 7);
			check(b.IsOccupied());
			check(A.GetNumEntries() == 3);
			check(A.ComputeUnoccupiedSize() == 3);

			// failed alloc
			FRangeAllocator::FRange c = A.AllocRange(4);
			check(!c.IsValid());
			check(!c.IsOccupied());
			check(A.GetNumEntries() == 3);
			check(A.ComputeUnoccupiedSize() == 3);

			// successfully alloc
			FRangeAllocator::FRange d = A.AllocRange(3);
			check(d.GetStart() == 7);
			check(d.GetEnd() == 10);
			check(d.IsOccupied());
			check(A.GetNumEntries() == 3);
			check(A.ComputeUnoccupiedSize() == 0);

			A.ReleaseRange(b);
			check(A.GetNumEntries() == 3);
			check(A.ComputeUnoccupiedSize() == 4);

			A.ReleaseRange(a);
			check(A.GetNumEntries() == 2);
			check(A.ComputeUnoccupiedSize() == 7);

			A.ReleaseRange(d);
			check(A.GetNumEntries() == 1);
			check(A.ComputeUnoccupiedSize() == 10);

			// we are back to a clean start

			FRangeAllocator::FRange e = A.AllocRange(10);
			check(e.GetStart() == 0);
			check(e.GetEnd() == 10);
			check(e.IsOccupied());
			check(A.GetNumEntries() == 1);
			check(A.ComputeUnoccupiedSize() == 0);

			A.ReleaseRange(e);
			check(A.GetNumEntries() == 1);
			check(A.ComputeUnoccupiedSize() == 10);

			// we are back to a clean start

			// create define range we want to block out
			FRangeAllocator::FRange f;
			f.SetOccupied(2, 4);
			A.OccupyRange(f);
			check(A.GetNumEntries() == 3);
			check(A.ComputeUnoccupiedSize() == 6);

			FRangeAllocator::FRange g = A.AllocRange(2);
			check(g.GetStart() == 0);
			check(g.GetEnd() == 2);
			check(g.IsOccupied());
			check(A.GetNumEntries() == 3);
			check(A.ComputeUnoccupiedSize() == 4);

			FRangeAllocator::FRange h = A.AllocRange(4);
			check(h.GetStart() == 6);
			check(h.GetEnd() == 10);
			check(h.IsOccupied());
			check(A.GetNumEntries() == 3);
			check(A.ComputeUnoccupiedSize() == 0);
		}
#endif // !UE_BUILD_SHIPPING
	}

private:

	// ordered from small to large (for efficient compactening)
	TArray<FRange> Entries;
};
