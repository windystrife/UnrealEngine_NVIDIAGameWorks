// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLDrv.h: Public OpenGL RHI definitions.
=============================================================================*/

#pragma once

// Dependencies.

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "GPUProfiler.h"
#include "RenderResource.h"

//TODO: Move these to OpenGLDrvPrivate.h
#if PLATFORM_WINDOWS
#include "Runtime/OpenGLDrv/Private/Windows/OpenGLWindows.h"
#elif PLATFORM_LINUX
#include "Runtime/OpenGLDrv/Private/Linux/OpenGLLinux.h"
#elif PLATFORM_IOS
#include "IOS/IOSOpenGL.h"
#elif PLATFORM_ANDROIDESDEFERRED
#include "Android/AndroidESDeferredOpenGL.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidOpenGL.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5OpenGL.h"
#elif PLATFORM_LINUX
#include "Linux/OpenGLLinux.h"
#endif

#define OPENGL_USE_BINDABLE_UNIFORMS 0
#define OPENGL_USE_BLIT_FOR_BACK_BUFFER 1

// OpenGL RHI public headers.
#include "OpenGLUtil.h"
#include "OpenGLState.h"

#define FOpenGLCachedUniformBuffer_Invalid 0xFFFFFFFF

class FOpenGLDynamicRHI;
class FResourceBulkDataInterface;
struct Rect;

template<class T> struct TOpenGLResourceTraits;

// This class has multiple inheritance but really FGPUTiming is a static class
class FOpenGLBufferedGPUTiming : public FRenderResource, public FGPUTiming
{
public:

	/**
	 * Constructor.
	 *
	 * @param InOpenGLRHI			RHI interface
	 * @param InBufferSize		Number of buffered measurements
	 */
	FOpenGLBufferedGPUTiming(class FOpenGLDynamicRHI* InOpenGLRHI, int32 BufferSize);

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
	 * Initializes all OpenGL resources.
	 */
	virtual void InitDynamicRHI() override;

	/**
	 * Releases all OpenGL resources.
	 */
	virtual void ReleaseDynamicRHI() override;


private:

	/**
	 * Initializes the static variables, if necessary.
	 */
	static void PlatformStaticInitialize(void* UserData);

	/** RHI interface */
	FOpenGLDynamicRHI*					OpenGLRHI;
	/** Number of timestamps created in 'StartTimestamps' and 'EndTimestamps'. */
	int32								BufferSize;
	/** Current timing being measured on the CPU. */
	int32								CurrentTimestamp;
	/** Number of measurements in the buffers (0 - BufferSize). */
	int32								NumIssuedTimestamps;
	/** Timestamps for all StartTimings. */
	TArray<FOpenGLRenderQuery *>		StartTimestamps;
	/** Timestamps for all EndTimings. */
	TArray<FOpenGLRenderQuery *>		EndTimestamps;
	/** Whether we are currently timing the GPU: between StartTiming() and EndTiming(). */
	bool								bIsTiming;
};

/**
  * Used to track whether a period was disjoint on the GPU, which means GPU timings are invalid.
  * OpenGL lacks this concept at present, so the class is just a placeholder
  * Timings are all assumed to be non-disjoint
  */
class FOpenGLDisjointTimeStampQuery : public FRenderResource
{
public:
	FOpenGLDisjointTimeStampQuery(class FOpenGLDynamicRHI* InOpenGLRHI=NULL);

	void Init(class FOpenGLDynamicRHI* InOpenGLRHI)
	{
		OpenGLRHI = InOpenGLRHI;
		InitResource();
	}

	void StartTracking();
	void EndTracking();
	bool IsResultValid();
	bool GetResult(uint64* OutResult=NULL);
	static uint64 GetTimingFrequency()
	{
		return 1000000000ull;
	}
	static bool IsSupported()
	{
		return FOpenGL::SupportsDisjointTimeQueries();
	}

	/**
	 * Initializes all resources.
	 */
	virtual void InitDynamicRHI();

	/**
	 * Releases all resources.
	 */
	virtual void ReleaseDynamicRHI();


private:
	bool	bIsResultValid;
	GLuint	DisjointQuery;
	uint64	Context;

	FOpenGLDynamicRHI* OpenGLRHI;
};

/** A single perf event node, which tracks information about a appBeginDrawEvent/appEndDrawEvent range. */
class FOpenGLEventNode : public FGPUProfilerEventNode
{
public:

	FOpenGLEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent, class FOpenGLDynamicRHI* InRHI)
	:	FGPUProfilerEventNode(InName, InParent)
	,	Timing(InRHI, 1)
	{
		// Initialize Buffered timestamp queries 
		Timing.InitResource();
	}

	virtual ~FOpenGLEventNode()
	{
		Timing.ReleaseResource();
	}

	/** 
	 * Returns the time in ms that the GPU spent in this draw event.  
	 * This blocks the CPU if necessary, so can cause hitching.
	 */
	float GetTiming() override;

	virtual void StartTiming() override
	{
		Timing.StartTiming();
	}

	virtual void StopTiming() override
	{
		Timing.EndTiming();
	}

	FOpenGLBufferedGPUTiming Timing;
};

/** An entire frame of perf event nodes, including ancillary timers. */
class FOpenGLEventNodeFrame : public FGPUProfilerEventNodeFrame
{
public:
	FOpenGLEventNodeFrame(class FOpenGLDynamicRHI* InRHI) :
		FGPUProfilerEventNodeFrame(),
		RootEventTiming(InRHI, 1),
		DisjointQuery(InRHI)
	{
	  RootEventTiming.InitResource();
	  DisjointQuery.InitResource();
	}

	~FOpenGLEventNodeFrame()
	{

		RootEventTiming.ReleaseResource();
		DisjointQuery.ReleaseResource();
	}

	/** Start this frame of per tracking */
	void StartFrame() override;

	/** End this frame of per tracking, but do not block yet */
	void EndFrame() override;

	/** Calculates root timing base frequency (if needed by this RHI) */
	virtual float GetRootTimingResults() override;

	virtual void LogDisjointQuery() override;

	/** Timer tracking inclusive time spent in the root nodes. */
	FOpenGLBufferedGPUTiming RootEventTiming;

	/** Disjoint query tracking whether the times reported by DumpEventTree are reliable. */
	FOpenGLDisjointTimeStampQuery DisjointQuery;
};

/** 
 * Encapsulates GPU profiling logic and data. 
 * There's only one global instance of this struct so it should only contain global data, nothing specific to a frame.
 */
struct FOpenGLGPUProfiler : public FGPUProfiler
{
	/** Used to measure GPU time per frame. */
	FOpenGLBufferedGPUTiming FrameTiming;

	/** Measuring GPU frame time with a disjoint query. */
	static const int MAX_GPUFRAMEQUERIES = 4;
	FOpenGLDisjointTimeStampQuery DisjointGPUFrameTimeQuery[MAX_GPUFRAMEQUERIES];
	int CurrentGPUFrameQueryIndex;

	class FOpenGLDynamicRHI* OpenGLRHI;
	// count the number of beginframe calls without matching endframe calls.
	int32 NestedFrameCount;

	/** GPU hitch profile histories */
	TIndirectArray<FOpenGLEventNodeFrame> GPUHitchEventNodeFrames;

	FOpenGLGPUProfiler(class FOpenGLDynamicRHI* InOpenGLRHI)
	:	FGPUProfiler()
	,	FrameTiming(InOpenGLRHI, 4)
	,	CurrentGPUFrameQueryIndex(0)
	,	OpenGLRHI(InOpenGLRHI)
	,	NestedFrameCount(0)
	{
		FrameTiming.InitResource();
		for ( int32 Index=0; Index < MAX_GPUFRAMEQUERIES; ++Index )
		{
			DisjointGPUFrameTimeQuery[Index].Init(InOpenGLRHI);
		}
	}

	virtual FGPUProfilerEventNode* CreateEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent) override
	{
		FOpenGLEventNode* EventNode = new FOpenGLEventNode(InName, InParent, OpenGLRHI);
		return EventNode;
	}

	void Cleanup();

	virtual void PushEvent(const TCHAR* Name, FColor Color) override;
	virtual void PopEvent() override;

	void BeginFrame(class FOpenGLDynamicRHI* InRHI);
	void EndFrame();
};


/** The interface which is implemented by the dynamically bound RHI. */
class OPENGLDRV_API FOpenGLDynamicRHI : public FDynamicRHI, public IRHICommandContext
{
public:

	friend class FOpenGLViewport;
#if PLATFORM_ANDROIDESDEFERRED // Flithy hack to workaround radr://16011763
	friend class FOpenGLTextureBase;
#endif

	/** Initialization constructor. */
	FOpenGLDynamicRHI();

	/** Destructor */
	~FOpenGLDynamicRHI() {}

	// FDynamicRHI interface.
	virtual void Init();
	virtual void Shutdown();
	virtual const TCHAR* GetName() override { return TEXT("OpenGL"); }

	template<typename TRHIType>
	static FORCEINLINE typename TOpenGLResourceTraits<TRHIType>::TConcreteType* ResourceCast(TRHIType* Resource)
	{
		return static_cast<typename TOpenGLResourceTraits<TRHIType>::TConcreteType*>(Resource);
	}

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
	virtual FTexture2DRHIRef RHICreateTextureExternal2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo) final override;
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
	virtual void RHITick(float DeltaTime) final override;
	virtual void RHISetStreamOutTargets(uint32 NumTargets,const FVertexBufferRHIParamRef* VertexBuffers,const uint32* Offsets) final override;
	virtual void RHIDiscardRenderTargets(bool Depth,bool Stencil,uint32 ColorBitMask) final override;
	virtual void RHIBlockUntilGPUIdle() final override;
	virtual void RHISubmitCommandsAndFlushGPU() final override;
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
	virtual void RHISetBlendFactor(const FLinearColor& BlendFactor) final override
	{
		// Currently ignored as well as on RHISetBlendState()...
	}
	virtual void RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs) final override;
	virtual void RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo) final override;
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
	virtual void RHIInvalidateCachedState() final override;

	// FOpenGLDynamicRHI interface.
	virtual FTexture2DRHIRef RHICreateTexture2DFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, const FClearValueBinding& ClearValueBinding, GLuint Resource, uint32 Flags);
	virtual FTexture2DRHIRef RHICreateTexture2DArrayFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, const FClearValueBinding& ClearValueBinding, GLuint Resource, uint32 Flags);
	virtual FTextureCubeRHIRef RHICreateTextureCubeFromResource(EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, const FClearValueBinding& ClearValueBinding, GLuint Resource, uint32 Flags);
	virtual void RHIAliasTextureResources(FTextureRHIParamRef DestTexture, FTextureRHIParamRef SrcTexture);


	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
	// OpenGL TODO
#if WITH_NVVOLUMETRICLIGHTING
	virtual void ClearStateCache() final override {}
	virtual bool GetPlatformDesc(NvVl::PlatformDesc& PlatformDesc) final override { return false; }
	virtual void GetPlatformRenderCtx(NvVl::PlatformRenderCtx& PlatformRenderCtx) final override {}
	virtual void GetPlatformShaderResource(FTextureRHIParamRef TextureRHI, NvVl::PlatformShaderResource& PlatformShaderResource) final override {}
	virtual void GetPlatformRenderTarget(FTextureRHIParamRef TextureRHI, NvVl::PlatformRenderTarget& PlatformRenderTarget) final override {}
#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting


	void Cleanup();

	void PurgeFramebufferFromCaches(GLuint Framebuffer);
	void OnVertexBufferDeletion(GLuint VertexBufferResource);
	void OnIndexBufferDeletion(GLuint IndexBufferResource);
	void OnPixelBufferDeletion(GLuint PixelBufferResource);
	void OnUniformBufferDeletion(GLuint UniformBufferResource,uint32 AllocatedSize,bool bStreamDraw);
	void OnProgramDeletion(GLint ProgramResource);
	void InvalidateTextureResourceInCache(GLuint Resource);
	void InvalidateUAVResourceInCache(GLuint Resource);
	/** Set a resource on texture target of a specific real OpenGL stage. Goes through cache to eliminate redundant calls. */
	void CachedSetupTextureStage(FOpenGLContextState& ContextState, GLint TextureIndex, GLenum Target, GLuint Resource, GLint BaseMip, GLint NumMips);
	void CachedSetupUAVStage(FOpenGLContextState& ContextState, GLint UAVIndex, GLenum Format, GLuint Resource);
	void UpdateSRV(FOpenGLShaderResourceView* SRV);
	FOpenGLContextState& GetContextStateForCurrentContext(bool bAssertIfInvalid = true);

	void CachedBindArrayBuffer( FOpenGLContextState& ContextState, GLuint Buffer )
	{
		VERIFY_GL_SCOPE();
		if( ContextState.ArrayBufferBound != Buffer )
		{
			glBindBuffer( GL_ARRAY_BUFFER, Buffer );
			ContextState.ArrayBufferBound = Buffer;
		}
	}

	void CachedBindElementArrayBuffer( FOpenGLContextState& ContextState, GLuint Buffer )
	{
		VERIFY_GL_SCOPE();
		if( ContextState.ElementArrayBufferBound != Buffer )
		{
			glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, Buffer );
			ContextState.ElementArrayBufferBound = Buffer;
		}
	}

	void CachedBindPixelUnpackBuffer( FOpenGLContextState& ContextState, GLuint Buffer )
	{
		VERIFY_GL_SCOPE();

		if( ContextState.PixelUnpackBufferBound != Buffer )
		{
			glBindBuffer( GL_PIXEL_UNPACK_BUFFER, Buffer );
			ContextState.PixelUnpackBufferBound = Buffer;
		}
	}

	void CachedBindUniformBuffer( FOpenGLContextState& ContextState, GLuint Buffer )
	{
		VERIFY_GL_SCOPE();
		check(IsInRenderingThread());
		if( ContextState.UniformBufferBound != Buffer )
		{
			glBindBuffer( GL_UNIFORM_BUFFER, Buffer );
			ContextState.UniformBufferBound = Buffer;
		}
	}

	bool IsUniformBufferBound( FOpenGLContextState& ContextState, GLuint Buffer ) const
	{
		return ( ContextState.UniformBufferBound == Buffer );
	}

	/** Add query to Queries list upon its creation. */
	void RegisterQuery( FOpenGLRenderQuery* Query );

	/** Remove query from Queries list upon its deletion. */
	void UnregisterQuery( FOpenGLRenderQuery* Query );

	/** Inform all queries about the need to recreate themselves after OpenGL context they're in gets deleted. */
	void InvalidateQueries();

	FOpenGLSamplerState* GetPointSamplerState() const { return (FOpenGLSamplerState*)PointSamplerState.GetReference(); }

	FRHITexture* CreateOpenGLTexture(uint32 SizeX, uint32 SizeY, bool CubeTexture, bool ArrayTexture, bool bIsExternal, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 ArraySize, uint32 Flags, const FClearValueBinding& InClearValue, FResourceBulkDataInterface* BulkData = NULL);

	void SetCustomPresent(class FRHICustomPresent* InCustomPresent);
private:

	/** Counter incremented each time RHIBeginScene is called. */
	uint32 SceneFrameCounter;

	/** Value used to detect when resource tables need to be recached. INDEX_NONE means always recache. */
	uint32 ResourceTableFrameCounter;

	/** RHI device state, independent of underlying OpenGL context used */
	FOpenGLRHIState						PendingState;
	FOpenGLStreamedVertexBufferArray	DynamicVertexBuffers;
	FOpenGLStreamedIndexBufferArray		DynamicIndexBuffers;
	FSamplerStateRHIRef					PointSamplerState;

	/** A list of all viewport RHIs that have been created. */
	TArray<FOpenGLViewport*> Viewports;
	TRefCountPtr<FOpenGLViewport>		DrawingViewport;
	bool								bRevertToSharedContextAfterDrawingViewport;

	bool								bIsRenderingContextAcquired;

	/** A history of the most recently used bound shader states, used to keep transient bound shader states from being recreated for each use. */
	TGlobalResource< TBoundShaderStateHistory<10000> > BoundShaderStateHistory;

	/** Per-context state caching */
	FOpenGLContextState InvalidContextState;
	FOpenGLContextState	SharedContextState;
	FOpenGLContextState	RenderingContextState;
	
	/** Cached mip-limits for textures when ARB_texture_view is unavailable */
	TMap<GLuint, TPair<GLenum, GLenum>> TextureMipLimits;

	/** Underlying platform-specific data */
	struct FPlatformOpenGLDevice* PlatformDevice;

	/** Query list. This is used to inform queries they're no longer valid when OpenGL context they're in gets released from another thread. */
	TArray<FOpenGLRenderQuery*> Queries;

	/** A critical section to protect modifications and iteration over Queries list */
	FCriticalSection QueriesListCriticalSection;

	/** Query list. This is used to inform queries they're no longer valid when OpenGL context they're in gets released from another thread. */
	TArray<FOpenGLRenderQuery*> TimerQueries;

	/** A critical section to protect modifications and iteration over Queries list */
	FCriticalSection TimerQueriesListCriticalSection;

	FOpenGLGPUProfiler GPUProfilingData;
	friend FOpenGLGPUProfiler;
//	FOpenGLEventQuery FrameSyncEvent;

	FCriticalSection CustomPresentSection;
	TRefCountPtr<class FRHICustomPresent> CustomPresent;

	GLuint GetOpenGLFramebuffer(uint32 NumSimultaneousRenderTargets, FOpenGLTextureBase** RenderTargets, uint32* ArrayIndices, uint32* MipmapLevels, FOpenGLTextureBase* DepthStencilTarget);

	void InitializeStateResources();

	/** needs to be called before each dispatch call */
	

	void EnableVertexElementCached(FOpenGLContextState& ContextCache, const FOpenGLVertexElement &VertexElement, GLsizei Stride, void *Pointer, GLuint Buffer);
	void EnableVertexElementCachedZeroStride(FOpenGLContextState& ContextCache, const FOpenGLVertexElement &VertexElement, uint32 NumVertices, FOpenGLVertexBuffer* VertexBuffer);
	void SetupVertexArrays(FOpenGLContextState& ContextCache, uint32 BaseVertexIndex, FOpenGLStream* Streams, uint32 NumStreams, uint32 MaxVertices);
	void SetupVertexArraysVAB(FOpenGLContextState& ContextCache, uint32 BaseVertexIndex, FOpenGLStream* Streams, uint32 NumStreams, uint32 MaxVertices);
	void SetupVertexArraysUP(FOpenGLContextState& ContextState, void* Buffer, uint32 Stride);

	void SetupBindlessTextures( FOpenGLContextState& ContextState, const TArray<FOpenGLBindlessSamplerInfo> &Samplers );

	/** needs to be called before each draw call */
	void BindPendingFramebuffer( FOpenGLContextState& ContextState );
	void BindPendingShaderState( FOpenGLContextState& ContextState );
	void BindPendingComputeShaderState( FOpenGLContextState& ContextState, FComputeShaderRHIParamRef ComputeShaderRHI);
	void UpdateRasterizerStateInOpenGLContext( FOpenGLContextState& ContextState );
	void UpdateDepthStencilStateInOpenGLContext( FOpenGLContextState& ContextState );
	void UpdateScissorRectInOpenGLContext( FOpenGLContextState& ContextState );
	void UpdateViewportInOpenGLContext( FOpenGLContextState& ContextState );
	
	template <class ShaderType> void SetResourcesFromTables(const ShaderType* RESTRICT);
	void CommitGraphicsResourceTables();
	void CommitComputeResourceTables(class FOpenGLComputeShader* ComputeShader);
	void CommitNonComputeShaderConstants();
	void CommitComputeShaderConstants(FComputeShaderRHIParamRef ComputeShaderRHI);
	void SetPendingBlendStateForActiveRenderTargets( FOpenGLContextState& ContextState );
	
	void SetupTexturesForDraw( FOpenGLContextState& ContextState);
	template <typename StateType>
	void SetupTexturesForDraw( FOpenGLContextState& ContextState, const StateType ShaderState, int32 MaxTexturesNeeded);

	void SetupUAVsForDraw(FOpenGLContextState& ContextState, const TRefCountPtr<FOpenGLComputeShader> &ComputeShader, int32 MaxUAVsNeeded);

	void RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil);

public:
	/** Remember what RHI user wants set on a specific OpenGL texture stage, translating from Stage and TextureIndex for stage pair. */
	void InternalSetShaderTexture(FOpenGLTextureBase* Texture, FOpenGLShaderResourceView* SRV, GLint TextureIndex, GLenum Target, GLuint Resource, int NumMips, int LimitMip);
	void InternalSetShaderUAV(GLint UAVIndex, GLenum Format, GLuint Resource);
	void InternalSetSamplerStates(GLint TextureIndex, FOpenGLSamplerState* SamplerState);

private:
	void ApplyTextureStage(FOpenGLContextState& ContextState, GLint TextureIndex, const FTextureStage& TextureStage, FOpenGLSamplerState* SamplerState);

	void ReadSurfaceDataRaw(FOpenGLContextState& ContextState, FTextureRHIParamRef TextureRHI,FIntRect Rect,TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags);

	void BindUniformBufferBase(FOpenGLContextState& ContextState, int32 NumUniformBuffers, FUniformBufferRHIRef* BoundUniformBuffers, uint32 FirstUniformBuffer, bool ForceUpdate);

	void ClearCurrentFramebufferWithCurrentScissor(FOpenGLContextState& ContextState, int8 ClearType, int32 NumClearColors, const FLinearColor* ClearColorArray, float Depth, uint32 Stencil);

	void FreeZeroStrideBuffers();

	/** Remaps vertex attributes on devices where GL_MAX_VERTEX_ATTRIBS < 16 */
	uint32 RemapVertexAttrib(uint32 VertexAttributeIndex)
	{
		if (FOpenGL::NeedsVertexAttribRemapTable())
		{
			check(VertexAttributeIndex < ARRAY_COUNT(PendingState.BoundShaderState->VertexShader->Bindings.VertexAttributeRemap));
			VertexAttributeIndex = PendingState.BoundShaderState->VertexShader->Bindings.VertexAttributeRemap[VertexAttributeIndex];
			check(VertexAttributeIndex < NUM_OPENGL_VERTEX_STREAMS); // check that this attribute has remaped correctly.
		}
		return VertexAttributeIndex;
	}
};

/** Implements the OpenGLDrv module as a dynamic RHI providing module. */
class FOpenGLDynamicRHIModule : public IDynamicRHIModule
{
public:
	
	// IModuleInterface
	virtual bool SupportsDynamicReloading() override { return false; }

	// IDynamicRHIModule
	virtual bool IsSupported() override;

	virtual FDynamicRHI* CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel = ERHIFeatureLevel::Num) override;
};

extern ERHIFeatureLevel::Type GRequestedFeatureLevel;
