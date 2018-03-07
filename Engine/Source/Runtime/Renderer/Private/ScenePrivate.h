// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScenePrivate.h: Private scene manager definitions.
=============================================================================*/

#pragma once

// Dependencies.

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Math/RandomStream.h"
#include "Engine/EngineTypes.h"
#include "RHI.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "SceneTypes.h"
#include "UniformBuffer.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "RendererInterface.h"
#include "SceneUtils.h"
#include "SceneManagement.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TextureLayout3d.h"
#include "ScenePrivateBase.h"
#include "PostProcess/RenderTargetPool.h"
#include "SceneCore.h"
#include "PrimitiveSceneInfo.h"
#include "LightSceneInfo.h"
#include "DepthRendering.h"
#include "SceneHitProxyRendering.h"
#include "ShadowRendering.h"
#include "TextureLayout.h"
#include "SceneRendering.h"
#include "StaticMeshDrawList.h"
#include "LightMapRendering.h"
#include "VelocityRendering.h"
#include "BasePassRendering.h"
#include "MobileBasePassRendering.h"
#include "VolumeRendering.h"

#if WITH_FLEX
#include "FlexFluidSurfaceRendering.h"
#endif

// NVCHANGE_BEGIN: Add VXGI
#include "VxgiRendering.h"
// NVCHANGE_END: Add VXGI
/** Factor by which to grow occlusion tests **/
#define OCCLUSION_SLOP (1.0f)

class AWorldSettings;
class FAtmosphericFogSceneInfo;
class FLightPropagationVolume;
class FMaterialParameterCollectionInstanceResource;
class FPrecomputedLightVolume;
class FScene;
class UAtmosphericFogComponent;
class UDecalComponent;
class UExponentialHeightFogComponent;
class ULightComponent;
class UPlanarReflectionComponent;
class UPrimitiveComponent;
class UReflectionCaptureComponent;
class USkyLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextureCube;
class UWindDirectionalSourceComponent;

/** Holds information about a single primitive's occlusion. */
class FPrimitiveOcclusionHistory
{
public:
	/** The primitive the occlusion information is about. */
	FPrimitiveComponentId PrimitiveId;

	/** The occlusion query which contains the primitive's pending occlusion results. */
	TArray<FRenderQueryRHIRef, TInlineAllocator<FOcclusionQueryHelpers::MaxBufferedOcclusionFrames> > PendingOcclusionQuery;

	uint32 HZBTestIndex;
	uint32 HZBTestFrameNumber;

	/** The last time the primitive was visible. */
	float LastVisibleTime;

	/** The last time the primitive was in the view frustum. */
	float LastConsideredTime;

	/** 
	 *	The pixels that were rendered the last time the primitive was drawn.
	 *	It is the ratio of pixels unoccluded to the resolution of the scene.
	 */
	float LastPixelsPercentage;

	/** whether or not this primitive was grouped the last time it was queried */
	bool bGroupedQuery;

	/** 
	 * For things that have subqueries (folaige), this is the non-zero
	 */
	int32 CustomIndex;

	/** Initialization constructor. */
	FORCEINLINE FPrimitiveOcclusionHistory(FPrimitiveComponentId InPrimitiveId, int32 SubQuery)
		: PrimitiveId(InPrimitiveId)
		, HZBTestIndex(0)
		, HZBTestFrameNumber(~0u)
		, LastVisibleTime(0.0f)
		, LastConsideredTime(0.0f)
		, LastPixelsPercentage(0.0f)
		, bGroupedQuery(false)
		, CustomIndex(SubQuery)
	{
		PendingOcclusionQuery.Empty(FOcclusionQueryHelpers::MaxBufferedOcclusionFrames);
		PendingOcclusionQuery.AddZeroed(FOcclusionQueryHelpers::MaxBufferedOcclusionFrames);
	}

	FORCEINLINE FPrimitiveOcclusionHistory()
		: HZBTestIndex(0)
		, HZBTestFrameNumber(~0u)
		, LastVisibleTime(0.0f)
		, LastConsideredTime(0.0f)
		, LastPixelsPercentage(0.0f)
		, bGroupedQuery(false)
		, CustomIndex(0)
	{
		PendingOcclusionQuery.Empty(FOcclusionQueryHelpers::MaxBufferedOcclusionFrames);
		PendingOcclusionQuery.AddZeroed(FOcclusionQueryHelpers::MaxBufferedOcclusionFrames);
	}


	/** Destructor. Note that the query should have been released already. */
	~FPrimitiveOcclusionHistory()
	{
//		check( !IsValidRef(PendingOcclusionQuery) );
	}

	template<class TOcclusionQueryPool> // here we use a template just to allow this to be inlined without sorting out the header order
	FORCEINLINE void ReleaseQueries(FRHICommandListImmediate& RHICmdList, TOcclusionQueryPool& Pool, int32 NumBufferedFrames)
	{
		for (int32 QueryIndex = 0; QueryIndex < NumBufferedFrames; QueryIndex++)
		{
			Pool.ReleaseQuery(PendingOcclusionQuery[QueryIndex]);
		}
	}

	FORCEINLINE FRenderQueryRHIRef& GetPastQuery(uint32 FrameNumber, int32 NumBufferedFrames)
	{
		// Get the oldest occlusion query
		const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryLookupIndex(FrameNumber, NumBufferedFrames);
		return PendingOcclusionQuery[QueryIndex];
	}

	FORCEINLINE void SetCurrentQuery(uint32 FrameNumber, FRenderQueryRHIParamRef NewQuery, int32 NumBufferedFrames)
	{
		// Get the current occlusion query
		const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryIssueIndex(FrameNumber, NumBufferedFrames);
		PendingOcclusionQuery[QueryIndex] = NewQuery;
	}
};

struct FPrimitiveOcclusionHistoryKey
{
	FPrimitiveComponentId PrimitiveId;
	int32 CustomIndex;

	FPrimitiveOcclusionHistoryKey(const FPrimitiveOcclusionHistory& Element)
		: PrimitiveId(Element.PrimitiveId)
		, CustomIndex(Element.CustomIndex)
	{
	}
	FPrimitiveOcclusionHistoryKey(FPrimitiveComponentId InPrimitiveId, int32 InCustomIndex)
		: PrimitiveId(InPrimitiveId)
		, CustomIndex(InCustomIndex)
	{
	}
};

/** Defines how the hash set indexes the FPrimitiveOcclusionHistory objects. */
struct FPrimitiveOcclusionHistoryKeyFuncs : BaseKeyFuncs<FPrimitiveOcclusionHistory,FPrimitiveOcclusionHistoryKey>
{
	typedef FPrimitiveOcclusionHistoryKey KeyInitType;

	static KeyInitType GetSetKey(const FPrimitiveOcclusionHistory& Element)
	{
		return FPrimitiveOcclusionHistoryKey(Element);
	}

	static bool Matches(KeyInitType A,KeyInitType B)
	{
		return A.PrimitiveId == B.PrimitiveId && A.CustomIndex == B.CustomIndex;
	}

	static uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key.PrimitiveId.PrimIDValue) ^ (GetTypeHash(Key.CustomIndex) >> 20);
	}
};


class FIndividualOcclusionHistory
{
	TArray<FRenderQueryRHIRef, TInlineAllocator<FOcclusionQueryHelpers::MaxBufferedOcclusionFrames> > PendingOcclusionQuery;

public:

	FORCEINLINE FIndividualOcclusionHistory()
	{
		PendingOcclusionQuery.Empty(FOcclusionQueryHelpers::MaxBufferedOcclusionFrames);
		PendingOcclusionQuery.AddZeroed(FOcclusionQueryHelpers::MaxBufferedOcclusionFrames);
	}


	template<class TOcclusionQueryPool> // here we use a template just to allow this to be inlined without sorting out the header order
	FORCEINLINE void ReleaseQueries(FRHICommandListImmediate& RHICmdList, TOcclusionQueryPool& Pool, int32 NumBufferedFrames)
	{
		for (int32 QueryIndex = 0; QueryIndex < NumBufferedFrames; QueryIndex++)
		{
			Pool.ReleaseQuery(PendingOcclusionQuery[QueryIndex]);
		}
	}

	FORCEINLINE FRenderQueryRHIRef& GetPastQuery(uint32 FrameNumber, int32 NumBufferedFrames)
	{
		// Get the oldest occlusion query
		const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryLookupIndex(FrameNumber, NumBufferedFrames);
		return PendingOcclusionQuery[QueryIndex];
	}

	FORCEINLINE void SetCurrentQuery(uint32 FrameNumber, FRenderQueryRHIParamRef NewQuery, int32 NumBufferedFrames)
	{
		// Get the current occlusion query
		const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryIssueIndex(FrameNumber, NumBufferedFrames);
		PendingOcclusionQuery[QueryIndex] = NewQuery;
	}
};

/**
 * Distance cull fading uniform buffer containing faded in
 */
class FGlobalDistanceCullFadeUniformBuffer : public TUniformBuffer< FDistanceCullFadeUniformShaderParameters >
{
public:
	/** Default constructor. */
	FGlobalDistanceCullFadeUniformBuffer()
	{
		FDistanceCullFadeUniformShaderParameters Uniforms;
		Uniforms.FadeTimeScaleBias.X = 0.0f;
		Uniforms.FadeTimeScaleBias.Y = 1.0f;
		SetContents( Uniforms );
	}
};

/** Global primitive uniform buffer resource containing faded in */
extern TGlobalResource< FGlobalDistanceCullFadeUniformBuffer > GDistanceCullFadedInUniformBuffer;

/**
 * Stores fading state for a single primitive in a single view
 */
class FPrimitiveFadingState
{
public:
	FPrimitiveFadingState()
		: FadeTimeScaleBias(ForceInitToZero)
		, FrameNumber(0)
		, EndTime(0.0f)
		, bIsVisible(false)
		, bValid(false)
	{
	}

	/** Scale and bias to use on time to calculate fade opacity */
	FVector2D FadeTimeScaleBias;

	/** The uniform buffer for the fade parameters */
	FDistanceCullFadeUniformBufferRef UniformBuffer;

	/** Frame number when last updated */
	uint32 FrameNumber;
	
	/** Time when fade will be finished. */
	float EndTime;
	
	/** Currently visible? */
	bool bIsVisible;

	/** Valid? */
	bool bValid;
};

enum FGlobalDFCacheType
{
	GDF_MostlyStatic,
	GDF_Full,
	GDF_Num
};

class FGlobalDistanceFieldCacheTypeState
{
public:

	FGlobalDistanceFieldCacheTypeState()
	{
	}

	TArray<FVector4> PrimitiveModifiedBounds;
	TRefCountPtr<IPooledRenderTarget> VolumeTexture;
};

class FGlobalDistanceFieldClipmapState
{
public:

	FGlobalDistanceFieldClipmapState()
	{
		FullUpdateOrigin = FIntVector::ZeroValue;
		LastPartialUpdateOrigin = FIntVector::ZeroValue;
		CachedMaxOcclusionDistance = 0;
		CachedGlobalDistanceFieldViewDistance = 0;
		CacheMostlyStaticSeparately = 1;
	}

	FIntVector FullUpdateOrigin;
	FIntVector LastPartialUpdateOrigin;
	float CachedMaxOcclusionDistance;
	float CachedGlobalDistanceFieldViewDistance;
	uint32 CacheMostlyStaticSeparately;
	
	FGlobalDistanceFieldCacheTypeState Cache[GDF_Num];
};

/** Maps a single primitive to it's per-view fading state data */
typedef TMap<FPrimitiveComponentId, FPrimitiveFadingState> FPrimitiveFadingStateMap;

class FOcclusionRandomStream
{
	enum {NumSamples = 3571};
public:

	/** Default constructor - should set seed prior to use. */
	FOcclusionRandomStream()
		: CurrentSample(0)
	{
		FRandomStream RandomStream(0x83246);
		for (int32 Index = 0; Index < NumSamples; Index++)
		{
			Samples[Index] = RandomStream.GetFraction();
		}
		Samples[0] = 0.0f; // we want to make sure we have at least a few zeros
		Samples[NumSamples/3] = 0.0f; // we want to make sure we have at least a few zeros
		Samples[(NumSamples*2)/3] = 0.0f; // we want to make sure we have at least a few zeros
	}

	/** @return A random number between 0 and 1. */
	FORCEINLINE float GetFraction()
	{
		if (CurrentSample >= NumSamples)
		{
			CurrentSample = 0;
		}
		return Samples[CurrentSample++];
	}
private:

	/** Index of the last sample we produced **/
	uint32 CurrentSample;
	/** A list of float random samples **/
	float Samples[NumSamples];
};

/** Random table for occlusion **/
extern FOcclusionRandomStream GOcclusionRandomStream;


/**
Helper class to time sections of the GPU work.
Buffers multiple frames to avoid waiting on the GPU so times are a little lagged.
*/
class FLatentGPUTimer
{
	static const int32 NumBufferedFrames = FOcclusionQueryHelpers::MaxBufferedOcclusionFrames + 1;

public:

	FLatentGPUTimer(int32 InAvgSamples = 30);

	void Release();

	/** Retrieves the most recently ready query results. */
	bool Tick(FRHICommandListImmediate& RHICmdList);
	/** Kicks off the query for the start of the rendering you're timing. */
	void Begin(FRHICommandListImmediate& RHICmdList);
	/** Kicks off the query for the end of the rendering you're timing. */
	void End(FRHICommandListImmediate& RHICmdList);

	/** Returns the most recent time in ms. */
	float GetTimeMS();
	/** Gets the average time in ms. Average is tracked over AvgSamples. */
	float GetAverageTimeMS();

private:

	int32 GetQueryIndex();

	//Average Tracking;
	int32 AvgSamples;
	TArray<float> TimeSamples;
	float TotalTime;
	int32 SampleIndex;

	int32 QueryIndex;
	FRenderQueryRHIRef StartQueries[NumBufferedFrames];
	FRenderQueryRHIRef EndQueries[NumBufferedFrames];
	FGraphEventRef QuerySubmittedFences[NumBufferedFrames];
};

/** HLOD tree persistent fading and visibility state */
class FHLODVisibilityState
{
public:
	FHLODVisibilityState()
		: TemporalLODSyncTime(0.0f)
		, UpdateCount(0)
	{}

	bool IsNodeFading(const int32 PrimIndex) const
	{
		checkSlow(PrimitiveFadingLODMap.IsValidIndex(PrimIndex));
		return PrimitiveFadingLODMap[PrimIndex];
	}

	bool IsNodeFadingOut(const int32 PrimIndex) const
	{
		checkSlow(PrimitiveFadingOutLODMap.IsValidIndex(PrimIndex));
		return PrimitiveFadingOutLODMap[PrimIndex];
	}

	bool IsNodeHidden(const int32 PrimIndex) const
	{
		return HiddenChildPrimitiveMap.IsValidIndex(PrimIndex) && HiddenChildPrimitiveMap[PrimIndex];
	}

	TBitArray<>	PrimitiveFadingLODMap;
	TBitArray<>	PrimitiveFadingOutLODMap;
	TBitArray<>	HiddenChildPrimitiveMap;
	float		TemporalLODSyncTime;
	uint16		UpdateCount;
};

/** HLOD scene node persistent fading and visibility state */
struct FHLODSceneNodeVisibilityState
{
	FHLODSceneNodeVisibilityState()
		: UpdateCount(0)
		, bWasVisible(0)
		, bIsVisible(0)
		, bIsFading(0)
	{}

	/** Last updated FrameCount */
	uint16 UpdateCount;

	/** Persistent visibility states */
	uint16 bWasVisible	: 1;
	uint16 bIsVisible	: 1;
	uint16 bIsFading	: 1;
};

/**
 * The scene manager's private implementation of persistent view state.
 * This class is associated with a particular camera across multiple frames by the game thread.
 * The game thread calls FRendererModule::AllocateViewState to create an instance of this private implementation.
 */
class FSceneViewState : public FSceneViewStateInterface, public FDeferredCleanupInterface, public FRenderResource
{
public:

	class FProjectedShadowKey
	{
	public:

		FORCEINLINE bool operator == (const FProjectedShadowKey &Other) const
		{
			return (PrimitiveId == Other.PrimitiveId && Light == Other.Light && ShadowSplitIndex == Other.ShadowSplitIndex && bTranslucentShadow == Other.bTranslucentShadow);
		}

		FProjectedShadowKey(const FProjectedShadowInfo& ProjectedShadowInfo)
			: PrimitiveId(ProjectedShadowInfo.GetParentSceneInfo() ? ProjectedShadowInfo.GetParentSceneInfo()->PrimitiveComponentId : FPrimitiveComponentId())
			, Light(ProjectedShadowInfo.GetLightSceneInfo().Proxy->GetLightComponent())
			, ShadowSplitIndex(ProjectedShadowInfo.CascadeSettings.ShadowSplitIndex)
			, bTranslucentShadow(ProjectedShadowInfo.bTranslucentShadow)
		{
		}

		FProjectedShadowKey(FPrimitiveComponentId InPrimitiveId, const ULightComponent* InLight, int32 InSplitIndex, bool bInTranslucentShadow)
			: PrimitiveId(InPrimitiveId)
			, Light(InLight)
			, ShadowSplitIndex(InSplitIndex)
			, bTranslucentShadow(bInTranslucentShadow)
		{
		}

		friend FORCEINLINE uint32 GetTypeHash(const FSceneViewState::FProjectedShadowKey& Key)
		{
			return PointerHash(Key.Light,GetTypeHash(Key.PrimitiveId));
		}

	private:
		FPrimitiveComponentId PrimitiveId;
		const ULightComponent* Light;
		int32 ShadowSplitIndex;
		bool bTranslucentShadow;
	};

	uint32 UniqueID;
	typedef TMap<FSceneViewState::FProjectedShadowKey, FRenderQueryRHIRef> ShadowKeyOcclusionQueryMap;
	TArray<ShadowKeyOcclusionQueryMap, TInlineAllocator<FOcclusionQueryHelpers::MaxBufferedOcclusionFrames> > ShadowOcclusionQueryMaps;

	/** The view's occlusion query pool. */
	FRenderQueryPool OcclusionQueryPool;

	FHZBOcclusionTester HZBOcclusionTests;

	/** Storage to which compressed visibility chunks are uncompressed at runtime. */
	TArray<uint8> DecompressedVisibilityChunk;

	/** Cached visibility data from the last call to GetPrecomputedVisibilityData. */
	const TArray<uint8>* CachedVisibilityChunk;
	int32 CachedVisibilityHandlerId;
	int32 CachedVisibilityBucketIndex;
	int32 CachedVisibilityChunkIndex;

	/** Parameter to keep track of previous frame. Managed by the rendering thread. */
	FViewMatrices PrevViewMatrices;
	FViewMatrices PendingPrevViewMatrices;

	uint32		PendingPrevFrameNumber;
	uint32		PrevFrameNumber;
	float		LastRenderTime;
	float		LastRenderTimeDelta;
	float		MotionBlurTimeScale;
	FMatrix		PrevViewMatrixForOcclusionQuery;
	FVector		PrevViewOriginForOcclusionQuery;

	// A counter incremented once each time this view is rendered.
	uint32 OcclusionFrameCounter;

	/** Used by states that have IsViewParent() == true to store primitives for child states. */
	TSet<FPrimitiveComponentId> ParentPrimitives;

	/** For this view, the set of primitives that are currently fading, either in or out. */
	FPrimitiveFadingStateMap PrimitiveFadingStates;

	FIndirectLightingCacheAllocation* TranslucencyLightingCacheAllocations[TVC_MAX];

	TMap<int32, FIndividualOcclusionHistory> PlanarReflectionOcclusionHistories;

	// Array of ClipmapIndex
	TArray<int32> DeferredGlobalDistanceFieldUpdates[GDF_Num];

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Are we currently in the state of freezing rendering? (1 frame where we gather what was rendered) */
	uint32 bIsFreezing : 1;

	/** Is rendering currently frozen? */
	uint32 bIsFrozen : 1;

	/** True if the CachedViewMatrices is holding frozen view matrices, otherwise false */
	uint32 bIsFrozenViewMatricesCached : 1;

	/** The set of primitives that were rendered the frame that we froze rendering */
	TSet<FPrimitiveComponentId> FrozenPrimitives;

	/** The cache view matrices at the time of freezing or the cached debug fly cam's view matrices. */
	FViewMatrices CachedViewMatrices;
#endif

	/** HLOD persistent fading and visibility state */
	FHLODVisibilityState HLODVisibilityState;
	TMap<FPrimitiveComponentId, FHLODSceneNodeVisibilityState> HLODSceneNodeVisibilityStates;

private:

	// to implement eye adaptation / auto exposure changes over time
	class FEyeAdaptationRTManager
	{
	public:

		FEyeAdaptationRTManager() :
			CurrentBuffer(0) {};

		void SafeRelease()
		{
			PooledRenderTarget[0].SafeRelease();
			PooledRenderTarget[1].SafeRelease();
		}

		/** Return current Render Target */
		TRefCountPtr<IPooledRenderTarget>& GetCurrentRT(FRHICommandList& RHICmdList)
		{
			return GetRTRef(RHICmdList, CurrentBuffer);
		}

		/** Return old Render Target*/
		TRefCountPtr<IPooledRenderTarget>& GetLastRT(FRHICommandList& RHICmdList)
		{
			return GetRTRef(RHICmdList, 1 - CurrentBuffer);
		}

		/** Reverse the current/last order of the targets */
		void SwapRTs()
		{
			CurrentBuffer = 1 - CurrentBuffer;
		}

	private:

		/** Return one of two two render targets */
		TRefCountPtr<IPooledRenderTarget>&  GetRTRef(FRHICommandList& RHICmdList, const int BufferNumber)
		{
			check(BufferNumber == 0 || BufferNumber == 1);

			// Create textures if needed.
			if (!PooledRenderTarget[BufferNumber].IsValid())
			{
				// Create the texture needed for EyeAdaptation
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(FIntPoint(1, 1), PF_G32R32F /*PF_R32_FLOAT*/, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
				if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5)
				{
					Desc.TargetableFlags |= TexCreate_UAV;
				}
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, PooledRenderTarget[BufferNumber], TEXT("EyeAdaptation"), true, ERenderTargetTransience::NonTransient);
			}

			return PooledRenderTarget[BufferNumber];
		}

	private:

		int CurrentBuffer;
		TRefCountPtr<IPooledRenderTarget> PooledRenderTarget[2];
	} EyeAdaptationRTManager;

	// eye adaptation is only valid after it has been computed, not on allocation of the RT
	bool bValidEyeAdaptation;

	// The LUT used by tonemapping.  In stereo this is only computed and stored by the Left Eye.
	TRefCountPtr<IPooledRenderTarget> CombinedLUTRenderTarget;

	// LUT is only valid after it has been computed, not on allocation of the RT
	bool bValidTonemappingLUT;


	// used by the Postprocess Material Blending system to avoid recreation and garbage collection of MIDs
	TArray<UMaterialInstanceDynamic*> MIDPool;
	uint32 MIDUsedCount;

	// if TemporalAA is on this cycles through 0..TemporalAASampleCount-1, ResetViewState() puts it back to 0
	uint8 TemporalAASampleIndex;
	// >= 1, 1 means there is no TemporalAA
	uint8 TemporalAASampleCount;

	// counts up by one each frame, warped in 0..7 range, ResetViewState() puts it back to 0
	uint32 FrameIndexMod8;
	
	// counts up by one each frame, warped in 0..3 range, ResetViewState() puts it back to 0
	int32 DistanceFieldTemporalSampleIndex;

	// light propagation volume used in this view
	TRefCountPtr<FLightPropagationVolume> LightPropagationVolume;

	// whether this view is a stereo counterpart to a primary view
	bool bIsStereoView;

public:

	FHeightfieldLightingAtlas* HeightfieldLightingAtlas;

	// Temporal AA result of last frame
	TRefCountPtr<IPooledRenderTarget> TemporalAAHistoryRT;
	TRefCountPtr<IPooledRenderTarget> PendingTemporalAAHistoryRT;
	// Temporal AA result for DOF of last frame
	TRefCountPtr<IPooledRenderTarget> DOFHistoryRT;
	TRefCountPtr<IPooledRenderTarget> DOFHistoryRT2;
	// Temporal AA result for SSR
	TRefCountPtr<IPooledRenderTarget> SSRHistoryRT;
	// Temporal AA result for light shafts of last frame
	TRefCountPtr<IPooledRenderTarget> LightShaftOcclusionHistoryRT;
	// Temporal AA result for light shafts of last frame
	TMap<const ULightComponent*, TRefCountPtr<IPooledRenderTarget> > LightShaftBloomHistoryRTs;
	TRefCountPtr<IPooledRenderTarget> DistanceFieldAOHistoryRT;
	TRefCountPtr<IPooledRenderTarget> DistanceFieldAOConfidenceHistoryRT;
	TRefCountPtr<IPooledRenderTarget> DistanceFieldIrradianceHistoryRT;
	// Mobile temporal AA surfaces.
	TRefCountPtr<IPooledRenderTarget> MobileAaBloomSunVignette0;
	TRefCountPtr<IPooledRenderTarget> MobileAaBloomSunVignette1;
	TRefCountPtr<IPooledRenderTarget> MobileAaColor0;
	TRefCountPtr<IPooledRenderTarget> MobileAaColor1;

	// Pre-computed filter in spectral (i.e. FFT) domain along with data to determine if we need to up date it
	struct {
		/// @cond DOXYGEN_WARNINGS
		void SafeRelease() { Spectral.SafeRelease(); CenterWeight.SafeRelease(); }
		/// @endcond

		// The 2d fourier transform of the physical space texture.
		TRefCountPtr<IPooledRenderTarget> Spectral;
		TRefCountPtr<IPooledRenderTarget> CenterWeight; // a 1-pixel buffer that holds blend weights for half-resolution fft.

														// The physical space source texture
		UTexture2D* Physical = NULL;

		// The Scale * 100 = percentage of the image space that the physical kernel represents.
		// e.g. Scale = 1 indicates that the physical kernel image occupies the same size 
		// as the image to be processed with the FFT convolution.
		float Scale = 0.f;

		// The size of the viewport for which the spectral kernel was calculated. 
		FIntPoint ImageSize;

		FVector2D CenterUV;

		// Mip level of the physical space source texture used when caching the spectral space texture.
		uint32 PhysicalMipLevel;
	} BloomFFTKernel;

	// cache for stencil reads to a avoid reallocations of the SRV, Key is to detect if the object has changed
	FTextureRHIRef SelectionOutlineCacheKey;
	TRefCountPtr<FRHIShaderResourceView> SelectionOutlineCacheValue;

	FForwardLightingViewResources ForwardLightingResources;

	FForwardLightingCullingResources ForwardLightingCullingResources;

	TRefCountPtr<IPooledRenderTarget> LightScatteringHistory;

	/** Distance field AO tile intersection GPU resources.  Last frame's state is not used, but they must be sized exactly to the view so stored here. */
	class FTileIntersectionResources* AOTileIntersectionResources;

	class FAOScreenGridResources* AOScreenGridResources;

	bool bInitializedGlobalDistanceFieldOrigins;
	FGlobalDistanceFieldClipmapState GlobalDistanceFieldClipmapState[GMaxGlobalDistanceFieldClipmaps];
	int32 GlobalDistanceFieldUpdateIndex;

	FVertexBufferRHIRef IndirectShadowCapsuleShapesVertexBuffer;
	FShaderResourceViewRHIRef IndirectShadowCapsuleShapesSRV;
	FVertexBufferRHIRef IndirectShadowMeshDistanceFieldCasterIndicesVertexBuffer;
	FShaderResourceViewRHIRef IndirectShadowMeshDistanceFieldCasterIndicesSRV;
	FVertexBufferRHIRef IndirectShadowLightDirectionVertexBuffer;
	FShaderResourceViewRHIRef IndirectShadowLightDirectionSRV;
	FRWBuffer IndirectShadowVolumetricLightmapDerivedLightDirection;
	FRWBuffer CapsuleTileIntersectionCountsBuffer;

	/** Timestamp queries around separate translucency, used for auto-downsampling. */
	FLatentGPUTimer TranslucencyTimer;
	FLatentGPUTimer SeparateTranslucencyTimer;

	/** This is float since it is derived off of UWorld::RealTimeSeconds, which is relative to BeginPlay time. */
	float LastAutoDownsampleChangeTime;
	float SmoothedHalfResTranslucencyGPUDuration;
	float SmoothedFullResTranslucencyGPUDuration;

	/** Current desired state of auto-downsampled separate translucency for this view. */
	bool bShouldAutoDownsampleTranslucency;

	// Is DOFHistoryRT set from DepthOfField?
	bool bDOFHistory;
	// Is DOFHistoryRT2 set from DepthOfField?
	bool bDOFHistory2;

	// True when Sequencer has paused
	bool bSequencerIsPaused;

	FTemporalLODState TemporalLODState;

	// call after OnFrameRenderingSetup()
	virtual uint32 GetCurrentTemporalAASampleIndex() const
	{
		return TemporalAASampleIndex;
	}

	// call after OnFrameRenderingSetup()
	uint32 GetCurrentTemporalAASampleCount() const
	{
		return TemporalAASampleCount;
	}

	virtual uint32 GetFrameIndexMod8() const
	{
		return FrameIndexMod8;
	}

	// to make rendering more deterministic
	virtual void ResetViewState()
	{
		TemporalAASampleIndex = 0;
		FrameIndexMod8 = 0;
		DistanceFieldTemporalSampleIndex = 0;

		ReleaseDynamicRHI();
	}

	// @param SampleCount 0 or 1 for no TemporalAA 
	void OnFrameRenderingSetup(uint32 SampleCount, const FSceneViewFamily& Family)
	{
		if(!SampleCount)
		{
			SampleCount = 1;
		}

		TemporalAASampleCount = FMath::Min(SampleCount, (uint32)255);
		
		if (!Family.bWorldIsPaused)
		{
			TemporalAASampleIndex++;

			FrameIndexMod8 = (FrameIndexMod8 + 1) % 8;
		}

		if(TemporalAASampleIndex >= TemporalAASampleCount)
		{
			TemporalAASampleIndex = 0;
		}
	}

	void SetupDistanceFieldTemporalOffset(const FSceneViewFamily& Family)
	{
		if (!Family.bWorldIsPaused)
		{
			DistanceFieldTemporalSampleIndex++;
		}

		if(DistanceFieldTemporalSampleIndex >= 4)
		{
			DistanceFieldTemporalSampleIndex = 0;
		}
	}

	int32 GetDistanceFieldTemporalSampleIndex() const
	{
		return DistanceFieldTemporalSampleIndex;
	}


	// call only if not yet created
	void SetupLightPropagationVolume(FSceneView& View, FSceneViewFamily& ViewFamily);

	/**
	 * @return can return 0
	 * @param bIncludeStereo - specifies whether the getter should include stereo views in its returned value
	 */
	FLightPropagationVolume* GetLightPropagationVolume(ERHIFeatureLevel::Type InFeatureLevel, bool bIncludeStereo = false) const;

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
private:
	// We should keep on in here instead of global so that we don't trash texture rendering previews
	mutable VXGI::IViewTracer* ViewTracer;
public:
	VXGI::IViewTracer* GetVXGITracer() const
	{
		check(IsInRenderingThread());
		//Create this on demand since many views don't need one
		if (!ViewTracer)
		{
			auto Status = GDynamicRHI->RHIVXGIGetInterface()->createNewTracer(&ViewTracer);
			check(VXGI_SUCCEEDED(Status));
		}
		return ViewTracer;
	}

	TRefCountPtr<IPooledRenderTarget> PrevSceneDepthZ;
	TRefCountPtr<IPooledRenderTarget> PrevVxgiNormalAndRoughness;

	FTexture2DRHIParamRef GetPreviousVxgiSceneDepthTexture() const { return (IsValidRef(PrevSceneDepthZ) ? ((const FTexture2DRHIRef&)PrevSceneDepthZ->GetRenderTargetItem().ShaderResourceTexture) : (nullptr)); }
	FTexture2DRHIParamRef GetPreviousVxgiNormalAndRoughnessTexture() const { return (IsValidRef(PrevSceneDepthZ) ? ((const FTexture2DRHIRef&)PrevVxgiNormalAndRoughness->GetRenderTargetItem().ShaderResourceTexture) : (nullptr)); }

	const NVRHI::TextureHandle GetPreviousVxgiSceneDepthTextureHandle() const { return GDynamicRHI->GetVXGITextureFromRHI(GetPreviousVxgiSceneDepthTexture()); }
	const NVRHI::TextureHandle GetPreviousVxgiNormalAndRoughnessTextureHandle() const { return GDynamicRHI->GetVXGITextureFromRHI(GetPreviousVxgiNormalAndRoughnessTexture()); }
#endif
	// NVCHANGE_END: Add VXGI

	/** Default constructor. */
	FSceneViewState();

	void DestroyLightPropagationVolume();

	virtual ~FSceneViewState();

	// called every frame after the view state was updated
	void UpdateLastRenderTime(const FSceneViewFamily& Family)
	{
		// The editor can trigger multiple update calls within a frame
		if(Family.CurrentRealTime != LastRenderTime)
		{
			LastRenderTimeDelta = Family.CurrentRealTime - LastRenderTime;
			LastRenderTime = Family.CurrentRealTime;
		}
	}

	void TrimHistoryRenderTargets(const FScene* Scene);

	/** 
	 * Called every frame after UpdateLastRenderTime, sets up the information for the lagged temporal LOD transition
	 */
	void UpdateTemporalLODTransition(const FViewInfo& View)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (bIsFrozen)
		{
			return;
		}
#endif

		TemporalLODState.UpdateTemporalLODTransition(View, LastRenderTime);
	}

	/** 
	 * Returns an array of visibility data for the given view, or NULL if none exists. 
	 * The data bits are indexed by VisibilityId of each primitive in the scene.
	 * This method decompresses data if necessary and caches it based on the bucket and chunk index in the view state.
	 */
	const uint8* GetPrecomputedVisibilityData(FViewInfo& View, const FScene* Scene);

	/**
	 * Cleans out old entries from the primitive occlusion history, and resets unused pending occlusion queries.
	 * @param MinHistoryTime - The occlusion history for any primitives which have been visible and unoccluded since
	 *							this time will be kept.  The occlusion history for any primitives which haven't been
	 *							visible and unoccluded since this time will be discarded.
	 * @param MinQueryTime - The pending occlusion queries older than this will be discarded.
	 */
	void TrimOcclusionHistory(FRHICommandListImmediate& RHICmdList, float CurrentTime, float MinHistoryTime, float MinQueryTime, int32 FrameNumber);

	/**
	 * Checks whether a shadow is occluded this frame.
	 * @param Primitive - The shadow subject.
	 * @param Light - The shadow source.
	 */
	bool IsShadowOccluded(FRHICommandListImmediate& RHICmdList, FSceneViewState::FProjectedShadowKey ShadowKey, int32 NumBufferedFrames) const;

	/**
	* Retrieve a single-pixel render targets with intra-frame state for use in eye adaptation post processing.
	*/
	TRefCountPtr<IPooledRenderTarget>& GetEyeAdaptation(FRHICommandList& RHICmdList)
	{
		return EyeAdaptationRTManager.GetCurrentRT(RHICmdList);
	}

	/**
	* Retrieve a single-pixel render targets with intra-frame state for use in eye adaptation post processing.
	*/
	IPooledRenderTarget* GetCurrentEyeAdaptationRT(FRHICommandList& RHICmdList)
	{
		return EyeAdaptationRTManager.GetCurrentRT(RHICmdList).GetReference();
	}
	IPooledRenderTarget* GetLastEyeAdaptationRT(FRHICommandList& RHICmdList)
	{
		return EyeAdaptationRTManager.GetLastRT(RHICmdList).GetReference();
	}

	/** Swaps the double-buffer targets used in eye adaptation */
	void SwapEyeAdaptationRTs()
	{
		EyeAdaptationRTManager.SwapRTs();
	}

	bool HasValidEyeAdaptation() const
	{
		return bValidEyeAdaptation;
	}

	void SetValidEyeAdaptation()
	{
		bValidEyeAdaptation = true;
	}

	bool HasValidTonemappingLUT() const
	{
		return bValidTonemappingLUT;
	}

	void SetValidTonemappingLUT(bool bValid = true)
	{
		bValidTonemappingLUT = bValid;
	}
	

	// Returns a reference to the render target used for the LUT.  Allocated on the first request.
	FSceneRenderTargetItem& GetTonemappingLUTRenderTarget(FRHICommandList& RHICmdList, const int32 LUTSize, const bool bUseVolumeLUT, const bool bNeedUAV)
	{
		if (CombinedLUTRenderTarget.IsValid() == false || 
			CombinedLUTRenderTarget->GetDesc().Extent.Y != LUTSize ||
			((CombinedLUTRenderTarget->GetDesc().Depth != 0) != bUseVolumeLUT) ||
			!!(CombinedLUTRenderTarget->GetDesc().TargetableFlags & TexCreate_UAV) != bNeedUAV)
		{
			// Create the texture needed for the tonemapping LUT
			EPixelFormat LUTPixelFormat = PF_A2B10G10R10;
			if (!GPixelFormats[LUTPixelFormat].Supported)
			{
				LUTPixelFormat = PF_R8G8B8A8;
			}

			FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(FIntPoint(LUTSize * LUTSize, LUTSize), LUTPixelFormat, FClearValueBinding::Transparent, TexCreate_None, TexCreate_ShaderResource, false);
			Desc.TargetableFlags |= bNeedUAV ? TexCreate_UAV : TexCreate_RenderTargetable;

			if (bUseVolumeLUT)
			{
				Desc.Extent = FIntPoint(LUTSize, LUTSize);
				Desc.Depth = LUTSize;
			}

			Desc.DebugName = TEXT("CombineLUTs");
			
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, CombinedLUTRenderTarget, Desc.DebugName, true, ERenderTargetTransience::NonTransient);
		}

		FSceneRenderTargetItem& RenderTarget = CombinedLUTRenderTarget.GetReference()->GetRenderTargetItem();
		return RenderTarget;
	}

	const FTextureRHIRef* GetTonemappingLUTTexture() const {
		const FTextureRHIRef* ShaderResourceTexture = NULL;
	
		if (CombinedLUTRenderTarget.IsValid()) {
			ShaderResourceTexture = &CombinedLUTRenderTarget->GetRenderTargetItem().ShaderResourceTexture;
		}
		return ShaderResourceTexture;
	}


	// FRenderResource interface.
	virtual void InitDynamicRHI() override
	{
		HZBOcclusionTests.InitDynamicRHI();
	}

	virtual void ReleaseDynamicRHI() override
	{
		for (int i = 0; i < ShadowOcclusionQueryMaps.Num(); ++i)
		{
			ShadowOcclusionQueryMaps[i].Reset();
		}
		PrimitiveOcclusionHistorySet.Empty();
		PrimitiveFadingStates.Empty();
		OcclusionQueryPool.Release();
		HZBOcclusionTests.ReleaseDynamicRHI();
		EyeAdaptationRTManager.SafeRelease();
		CombinedLUTRenderTarget.SafeRelease();
		TemporalAAHistoryRT.SafeRelease();
		PendingTemporalAAHistoryRT.SafeRelease();
		DOFHistoryRT.SafeRelease();
		DOFHistoryRT2.SafeRelease();
		SSRHistoryRT.SafeRelease();
		LightShaftOcclusionHistoryRT.SafeRelease();
		LightShaftBloomHistoryRTs.Empty();
		DistanceFieldAOHistoryRT.SafeRelease();
		DistanceFieldAOConfidenceHistoryRT.SafeRelease();
		DistanceFieldAOConfidenceHistoryRT.SafeRelease();
		DistanceFieldIrradianceHistoryRT.SafeRelease();
		MobileAaBloomSunVignette0.SafeRelease();
		MobileAaBloomSunVignette1.SafeRelease();
		MobileAaColor0.SafeRelease();
		MobileAaColor1.SafeRelease();
		BloomFFTKernel.SafeRelease();
		SelectionOutlineCacheKey.SafeRelease();
		SelectionOutlineCacheValue.SafeRelease();

		for (int32 CascadeIndex = 0; CascadeIndex < ARRAY_COUNT(GlobalDistanceFieldClipmapState); CascadeIndex++)
		{
			for (int32 CacheType = 0; CacheType < ARRAY_COUNT(GlobalDistanceFieldClipmapState[CascadeIndex].Cache); CacheType++)
			{
				GlobalDistanceFieldClipmapState[CascadeIndex].Cache[CacheType].VolumeTexture.SafeRelease();
			}
		}

		IndirectShadowCapsuleShapesVertexBuffer.SafeRelease();
		IndirectShadowCapsuleShapesSRV.SafeRelease();
		IndirectShadowMeshDistanceFieldCasterIndicesVertexBuffer.SafeRelease();
		IndirectShadowMeshDistanceFieldCasterIndicesSRV.SafeRelease();
		IndirectShadowLightDirectionVertexBuffer.SafeRelease();
		IndirectShadowLightDirectionSRV.SafeRelease();
		CapsuleTileIntersectionCountsBuffer.Release();
		TranslucencyTimer.Release();
		SeparateTranslucencyTimer.Release();
		ForwardLightingResources.Release();
		ForwardLightingCullingResources.Release();
		LightScatteringHistory.SafeRelease();

		// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
		PrevSceneDepthZ.SafeRelease();
		PrevVxgiNormalAndRoughness.SafeRelease();
		if (ViewTracer)
		{
			GDynamicRHI->RHIVXGIGetInterface()->destroyTracer(ViewTracer);
			ViewTracer = NULL;
		}
#endif
		// NVCHANGE_END: Add VXGI
	}

	// FSceneViewStateInterface
	RENDERER_API virtual void Destroy() override;
	
	virtual FSceneViewState* GetConcreteViewState() override
	{
		return this;
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{

		Collector.AddReferencedObjects(MIDPool);
	
		if (BloomFFTKernel.Physical)
		{
			Collector.AddReferencedObject(BloomFFTKernel.Physical);
		}
	}

	/** called in InitViews() */
	virtual void OnStartFrame(FSceneView& View, FSceneViewFamily& ViewFamily) override
	{
		check(IsInRenderingThread());

		if(!(View.FinalPostProcessSettings.IndirectLightingColor * View.FinalPostProcessSettings.IndirectLightingIntensity).IsAlmostBlack())
		{
			SetupLightPropagationVolume(View, ViewFamily);
		}
	}

	// needed for GetReusableMID()
	virtual void OnStartPostProcessing(FSceneView& CurrentView) override
	{
		check(IsInGameThread());
		
		// Needs to be done once for all viewstates.  If multiple FSceneViews are sharing the same ViewState, this will cause problems.
		// Sharing should be illegal right now though.
		MIDUsedCount = 0;
	}

	// Note: OnStartPostProcessing() needs to be called each frame for each view
	virtual UMaterialInstanceDynamic* GetReusableMID(class UMaterialInterface* InSource) override
	{		
		check(IsInGameThread());
		check(InSource);

		// 0 or MID (MaterialInstanceDynamic) pointer
		auto InputAsMID = Cast<UMaterialInstanceDynamic>(InSource);

		// fixup MID parents as this is not allowed, take the next MIC or Material.
		UMaterialInterface* ParentOfTheNewMID = InputAsMID ? InputAsMID->Parent : InSource;

		// this is not allowed and would cause an error later in the code
		check(!ParentOfTheNewMID->IsA(UMaterialInstanceDynamic::StaticClass()));

		UMaterialInstanceDynamic* NewMID = 0;

		if(MIDUsedCount < (uint32)MIDPool.Num())
		{
			NewMID = MIDPool[MIDUsedCount];

			if(NewMID->Parent != ParentOfTheNewMID)
			{
				// create a new one
				// garbage collector will remove the old one
				// this should not happen too often
				NewMID = UMaterialInstanceDynamic::Create(ParentOfTheNewMID, 0);
				MIDPool[MIDUsedCount] = NewMID;
			}

			// reusing an existing object means we need to clear out the Vector and Scalar parameters
			NewMID->ClearParameterValues();
		}
		else
		{
			NewMID = UMaterialInstanceDynamic::Create(ParentOfTheNewMID, 0);
			check(NewMID);

			MIDPool.Add(NewMID);
		}

		if(InputAsMID)
		{
			// parent is an MID so we need to copy the MID Vector and Scalar parameters over
			NewMID->CopyInterpParameters(InputAsMID);
		}

		check(NewMID->GetRenderProxy(false));
		MIDUsedCount++;
		return NewMID;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	virtual void ActivateFrozenViewMatrices(FSceneView& SceneView) override
	{
		auto* ViewState = static_cast<FSceneViewState*>(SceneView.State);
		if (ViewState->bIsFrozen)
		{
			check(ViewState->bIsFrozenViewMatricesCached);

			Swap(SceneView.ViewMatrices, ViewState->CachedViewMatrices);
			ViewState->bIsFrozenViewMatricesCached = false;
		}
	}

	virtual void RestoreUnfrozenViewMatrices(FSceneView& SceneView) override
	{
		auto* ViewState = static_cast<FSceneViewState*>(SceneView.State);
		if (ViewState->bIsFrozen)
		{
			check(!ViewState->bIsFrozenViewMatricesCached);

			Swap(SceneView.ViewMatrices, ViewState->CachedViewMatrices);
			ViewState->bIsFrozenViewMatricesCached = true;
		}
	}
#endif

	virtual FTemporalLODState& GetTemporalLODState() override
	{
		return TemporalLODState;
	}

	virtual const FTemporalLODState& GetTemporalLODState() const override
	{
		return TemporalLODState;
	}

	float GetTemporalLODTransition() const override
	{
		return TemporalLODState.GetTemporalLODTransition(LastRenderTime);
	}

	uint32 GetViewKey() const override
	{
		return UniqueID;
	}

	uint32 GetOcclusionFrameCounter() const
	{
		return OcclusionFrameCounter;
	}

	// FDeferredCleanupInterface
	virtual void FinishCleanup() override
	{
		delete this;
	}

	virtual SIZE_T GetSizeBytes() const override;

	virtual void SetSequencerState(const bool bIsPaused) override
	{
		bSequencerIsPaused = bIsPaused;
	}

	virtual bool GetSequencerState() override
	{
		return bSequencerIsPaused;
	}

	/** Information about visibility/occlusion states in past frames for individual primitives. */
	TSet<FPrimitiveOcclusionHistory,FPrimitiveOcclusionHistoryKeyFuncs> PrimitiveOcclusionHistorySet;
};

/** Rendering resource class that manages a cubemap array for reflections. */
class FReflectionEnvironmentCubemapArray : public FRenderResource
{
public:

	FReflectionEnvironmentCubemapArray(ERHIFeatureLevel::Type InFeatureLevel)
		: FRenderResource(InFeatureLevel)
		, MaxCubemaps(0)
		, CubemapSize(0)
	{}

	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

	/** 
	 * Updates the maximum number of cubemaps that this array is allocated for.
	 * This reallocates the resource but does not copy over the old contents. 
	 */
	void UpdateMaxCubemaps(uint32 InMaxCubemaps, int32 CubemapSize);

	/**
	* Updates the maximum number of cubemaps that this array is allocated for.
	* This reallocates the resource and copies over the old contents, preserving indices
	*/
	void ResizeCubemapArrayGPU(uint32 InMaxCubemaps, int32 CubemapSize, const TArray<int32>& IndexRemapping);

	int32 GetMaxCubemaps() const { return MaxCubemaps; }
	int32 GetCubemapSize() const { return CubemapSize; }
	bool IsValid() const { return IsValidRef(ReflectionEnvs); }
	FSceneRenderTargetItem& GetRenderTarget() const { return ReflectionEnvs->GetRenderTargetItem(); }

protected:
	uint32 MaxCubemaps;
	int32 CubemapSize;
	TRefCountPtr<IPooledRenderTarget> ReflectionEnvs;

	void ReleaseCubeArray();
};

/** Per-component reflection capture state that needs to persist through a re-register. */
class FCaptureComponentSceneState
{
public:
	/** Index of the cubemap in the array for this capture component. */
	int32 CaptureIndex;

	FCaptureComponentSceneState(int32 InCaptureIndex) :
		CaptureIndex(InCaptureIndex)
	{}

	bool operator==(const FCaptureComponentSceneState& Other) const 
	{
		return CaptureIndex == Other.CaptureIndex;
	}
};

/** Scene state used to manage the reflection environment feature. */
class FReflectionEnvironmentSceneData
{
public:
	
	/** 
	 * Set to true for one frame whenever RegisteredReflectionCaptures or the transforms of any registered reflection proxy has changed,
	 * Which allows one frame to update cached proxy associations.
	 */
	bool bRegisteredReflectionCapturesHasChanged;

	/** The rendering thread's list of visible reflection captures in the scene. */
	TArray<FReflectionCaptureProxy*> RegisteredReflectionCaptures;
	TArray<FVector> RegisteredReflectionCapturePositions;

	/** 
	 * Cubemap array resource which contains the captured scene for each reflection capture.
	 * This is indexed by the value of AllocatedReflectionCaptureState.CaptureIndex.
	 */
	FReflectionEnvironmentCubemapArray CubemapArray;

	/** Rendering thread map from component to scene state.  This allows storage of RT state that needs to persist through a component re-register. */
	TMap<const UReflectionCaptureComponent*, FCaptureComponentSceneState> AllocatedReflectionCaptureState;

	/** Rendering bitfield to track cubemap slots used. Needs to kept in sync with AllocatedReflectionCaptureState */
	TBitArray<> CubemapArraySlotsUsed;


	/** 
	 * Game thread list of reflection components that have been allocated in the cubemap array. 
	 * These are not necessarily all visible or being rendered, but their scene state is stored in the cubemap array.
	 */
	TSparseArray<UReflectionCaptureComponent*> AllocatedReflectionCapturesGameThread;

	/** Game thread tracking of what size this scene has allocated for the cubemap array. */
	int32 MaxAllocatedReflectionCubemapsGameThread;

	FReflectionEnvironmentSceneData(ERHIFeatureLevel::Type InFeatureLevel) :
		bRegisteredReflectionCapturesHasChanged(true),
		CubemapArray(InFeatureLevel),
		MaxAllocatedReflectionCubemapsGameThread(0)
	{}


	void ResizeCubemapArrayGPU(uint32 InMaxCubemaps, int32 InCubemapSize);
};

class FVolumetricLightmapInterpolation
{
public:
	FVector4 IndirectLightingSHCoefficients0[3];
	FVector4 IndirectLightingSHCoefficients1[3];
	FVector4 IndirectLightingSHCoefficients2;
	FVector4 IndirectLightingSHSingleCoefficient;
	FVector4 PointSkyBentNormal;
	float DirectionalLightShadowing;
	uint32 LastUsedSceneFrameNumber;
};

class FVolumetricLightmapSceneData
{
public:

	bool HasData() const { return LevelVolumetricLightmaps.Num() > 0; }
	void AddLevelVolume(const class FPrecomputedVolumetricLightmap* InVolume, EShadingPath ShadingPath);
	void RemoveLevelVolume(const class FPrecomputedVolumetricLightmap* InVolume);
	const FPrecomputedVolumetricLightmap* GetLevelVolumetricLightmap() const 
	{ 
		return LevelVolumetricLightmaps.Num() > 0 ? LevelVolumetricLightmaps.Last() : NULL; 
	}

	TMap<FVector, FVolumetricLightmapInterpolation> CPUInterpolationCache;

private:
	TArray<const FPrecomputedVolumetricLightmap*> LevelVolumetricLightmaps;
};

class FPrimitiveAndInstance
{
public:

	FPrimitiveAndInstance(const FVector4& InBoundingSphere, FPrimitiveSceneInfo* InPrimitive, int32 InInstanceIndex) :
		BoundingSphere(InBoundingSphere),
		Primitive(InPrimitive),
		InstanceIndex(InInstanceIndex)
	{}

	FVector4 BoundingSphere;
	FPrimitiveSceneInfo* Primitive;
	int32 InstanceIndex;
};

class FPrimitiveSurfelFreeEntry
{
public:
	FPrimitiveSurfelFreeEntry(int32 InOffset, int32 InNumSurfels) :
		Offset(InOffset),
		NumSurfels(InNumSurfels)
	{}

	FPrimitiveSurfelFreeEntry() :
		Offset(0),
		NumSurfels(0)
	{}

	int32 Offset;
	int32 NumSurfels;
};

class FPrimitiveSurfelAllocation
{
public:
	FPrimitiveSurfelAllocation(int32 InOffset, int32 InNumLOD0, int32 InNumSurfels, int32 InNumInstances) :
		Offset(InOffset),
		NumLOD0(InNumLOD0),
		NumSurfels(InNumSurfels),
		NumInstances(InNumInstances)
	{}

	FPrimitiveSurfelAllocation() :
		Offset(0),
		NumLOD0(0),
		NumSurfels(0),
		NumInstances(1)
	{}

	int32 GetTotalNumSurfels() const
	{
		return NumSurfels * NumInstances;
	}

	int32 Offset;
	int32 NumLOD0;
	int32 NumSurfels;
	int32 NumInstances;
};

class FPrimitiveRemoveInfo
{
public:
	FPrimitiveRemoveInfo(const FPrimitiveSceneInfo* InPrimitive) :
		Primitive(InPrimitive),
		bOftenMoving(InPrimitive->Proxy->IsOftenMoving()),
		DistanceFieldInstanceIndices(Primitive->DistanceFieldInstanceIndices)
	{}

	/** 
	 * Must not be dereferenced after creation, the primitive was removed from the scene and deleted
	 * Value of the pointer is still useful for map lookups
	 */
	const FPrimitiveSceneInfo* Primitive;

	bool bOftenMoving;

	TArray<int32, TInlineAllocator<1>> DistanceFieldInstanceIndices;
};

class FSurfelBufferAllocator
{
public:

	FSurfelBufferAllocator() : NumSurfelsInBuffer(0) {}

	int32 GetNumSurfelsInBuffer() const { return NumSurfelsInBuffer; }
	void AddPrimitive(const FPrimitiveSceneInfo* PrimitiveSceneInfo, int32 PrimitiveLOD0Surfels, int32 PrimitiveNumSurfels, int32 NumInstances);
	void RemovePrimitive(const FPrimitiveSceneInfo* Primitive);

	const FPrimitiveSurfelAllocation* FindAllocation(const FPrimitiveSceneInfo* Primitive)
	{
		return Allocations.Find(Primitive);
	}

private:

	int32 NumSurfelsInBuffer;
	TMap<const FPrimitiveSceneInfo*, FPrimitiveSurfelAllocation> Allocations;
	TArray<FPrimitiveSurfelFreeEntry> FreeList;
};

/** Scene data used to manage distance field object buffers on the GPU. */
class FDistanceFieldSceneData
{
public:

	FDistanceFieldSceneData(EShaderPlatform ShaderPlatform);
	~FDistanceFieldSceneData();

	void AddPrimitive(FPrimitiveSceneInfo* InPrimitive);
	void UpdatePrimitive(FPrimitiveSceneInfo* InPrimitive);
	void RemovePrimitive(FPrimitiveSceneInfo* InPrimitive);
	void Release();
	void VerifyIntegrity();

	bool HasPendingOperations() const
	{
		return PendingAddOperations.Num() > 0 || PendingUpdateOperations.Num() > 0 || PendingRemoveOperations.Num() > 0;
	}

	bool HasPendingRemovePrimitive(const FPrimitiveSceneInfo* Primitive) const
	{
		for (int32 RemoveIndex = 0; RemoveIndex < PendingRemoveOperations.Num(); RemoveIndex++)
		{
			if (PendingRemoveOperations[RemoveIndex].Primitive == Primitive)
			{
				return true;
			}
		}

		return false;
	}

	inline bool CanUse16BitObjectIndices() const
	{
		return bCanUse16BitObjectIndices && (NumObjectsInBuffer < (1 << 16));
	}

	int32 NumObjectsInBuffer;
	class FDistanceFieldObjectBuffers* ObjectBuffers;

	/** Stores the primitive and instance index of every entry in the object buffer. */
	TArray<FPrimitiveAndInstance> PrimitiveInstanceMapping;
	TArray<const FPrimitiveSceneInfo*> HeightfieldPrimitives;

	class FSurfelBuffers* SurfelBuffers;
	FSurfelBufferAllocator SurfelAllocations;

	class FInstancedSurfelBuffers* InstancedSurfelBuffers;
	FSurfelBufferAllocator InstancedSurfelAllocations;

	/** Pending operations on the object buffers to be processed next frame. */
	TArray<FPrimitiveSceneInfo*> PendingAddOperations;
	TSet<FPrimitiveSceneInfo*> PendingUpdateOperations;
	TArray<FPrimitiveRemoveInfo> PendingRemoveOperations;
	TArray<FVector4> PrimitiveModifiedBounds[GDF_Num];

	/** Used to detect atlas reallocations, since objects store UVs into the atlas and need to be updated when it changes. */
	int32 AtlasGeneration;

	bool bTrackAllPrimitives;
	bool bCanUse16BitObjectIndices;
};

/** Stores data for an allocation in the FIndirectLightingCache. */
class FIndirectLightingCacheBlock
{
public:

	FIndirectLightingCacheBlock() :
		MinTexel(FIntVector(0, 0, 0)),
		TexelSize(0),
		Min(FVector(0, 0, 0)),
		Size(FVector(0, 0, 0)),
		bHasEverBeenUpdated(false)
	{}

	FIntVector MinTexel;
	int32 TexelSize;
	FVector Min;
	FVector Size;
	bool bHasEverBeenUpdated;
};

/** Stores information about an indirect lighting cache block to be updated. */
class FBlockUpdateInfo
{
public:

	FBlockUpdateInfo(const FIndirectLightingCacheBlock& InBlock, FIndirectLightingCacheAllocation* InAllocation) :
		Block(InBlock),
		Allocation(InAllocation)
	{}

	FIndirectLightingCacheBlock Block;
	FIndirectLightingCacheAllocation* Allocation;
};

/** Information about the primitives that are attached together. */
class FAttachmentGroupSceneInfo
{
public:

	/** The parent primitive, which is the root of the attachment tree. */
	FPrimitiveSceneInfo* ParentSceneInfo;

	/** The primitives in the attachment group. */
	TArray<FPrimitiveSceneInfo*> Primitives;

	FAttachmentGroupSceneInfo() :
		ParentSceneInfo(nullptr)
	{}
};

struct FILCUpdatePrimTaskData
{
	FGraphEventRef TaskRef;
	TMap<FIntVector, FBlockUpdateInfo> OutBlocksToUpdate;
	TArray<FIndirectLightingCacheAllocation*> OutTransitionsOverTimeToUpdate;
};

/** 
 * Implements a volume texture atlas for caching indirect lighting on a per-object basis.
 * The indirect lighting is interpolated from Lightmass SH volume lighting samples.
 */
class FIndirectLightingCache : public FRenderResource
{
public:	

	/** true for the editor case where we want a better preview for object that have no valid lightmaps */
	FIndirectLightingCache(ERHIFeatureLevel::Type InFeatureLevel);

	// FRenderResource interface
	virtual void InitDynamicRHI();
	virtual void ReleaseDynamicRHI();

	/** Allocates a block in the volume texture atlas for a primitive. */
	FIndirectLightingCacheAllocation* AllocatePrimitive(const FPrimitiveSceneInfo* PrimitiveSceneInfo, bool bUnbuiltPreview);

	/** Releases the indirect lighting allocation for the given primitive. */
	void ReleasePrimitive(FPrimitiveComponentId PrimitiveId);

	FIndirectLightingCacheAllocation* FindPrimitiveAllocation(FPrimitiveComponentId PrimitiveId);	

	/** Updates indirect lighting in the cache based on visibility synchronously. */
	void UpdateCache(FScene* Scene, FSceneRenderer& Renderer, bool bAllowUnbuiltPreview);

	/** Starts a task to update the cache primitives.  Results and task ref returned in the FILCUpdatePrimTaskData structure */
	void StartUpdateCachePrimitivesTask(FScene* Scene, FSceneRenderer& Renderer, bool bAllowUnbuiltPreview, FILCUpdatePrimTaskData& OutTaskData);

	/** Wait on a previously started task and complete any block updates and debug draw */
	void FinalizeCacheUpdates(FScene* Scene, FSceneRenderer& Renderer, FILCUpdatePrimTaskData& TaskData);

	/** Force all primitive allocations to be re-interpolated. */
	void SetLightingCacheDirty(FScene* Scene, const FPrecomputedLightVolume* Volume);

	// Accessors
	FSceneRenderTargetItem& GetTexture0() { return Texture0->GetRenderTargetItem(); }
	FSceneRenderTargetItem& GetTexture1() { return Texture1->GetRenderTargetItem(); }
	FSceneRenderTargetItem& GetTexture2() { return Texture2->GetRenderTargetItem(); }

private:
	/** Internal helper to determine if indirect lighting is enabled at all */
	bool IndirectLightingAllowed(FScene* Scene, FSceneRenderer& Renderer) const;

	void ProcessPrimitiveUpdate(FScene* Scene, FViewInfo& View, int32 PrimitiveIndex, bool bAllowUnbuiltPreview, bool bAllowVolumeSample, TMap<FIntVector, FBlockUpdateInfo>& OutBlocksToUpdate, TArray<FIndirectLightingCacheAllocation*>& OutTransitionsOverTimeToUpdate);

	/** Internal helper to perform the work of updating the cache primitives.  Can be done on any thread as a task */
	void UpdateCachePrimitivesInternal(FScene* Scene, FSceneRenderer& Renderer, bool bAllowUnbuiltPreview, TMap<FIntVector, FBlockUpdateInfo>& OutBlocksToUpdate, TArray<FIndirectLightingCacheAllocation*>& OutTransitionsOverTimeToUpdate);

	/** Internal helper to perform blockupdates and transition updates on the results of UpdateCachePrimitivesInternal.  Must be on render thread. */
	void FinalizeUpdateInternal_RenderThread(FScene* Scene, FSceneRenderer& Renderer, TMap<FIntVector, FBlockUpdateInfo>& BlocksToUpdate, const TArray<FIndirectLightingCacheAllocation*>& TransitionsOverTimeToUpdate);

	/** Internal helper which adds an entry to the update lists for this allocation, if needed (due to movement, etc). Returns true if the allocation was updated or will be udpated */
	bool UpdateCacheAllocation(
		const FBoxSphereBounds& Bounds, 
		int32 BlockSize,
		bool bPointSample,
		bool bUnbuiltPreview,
		FIndirectLightingCacheAllocation*& Allocation, 
		TMap<FIntVector, FBlockUpdateInfo>& BlocksToUpdate,
		TArray<FIndirectLightingCacheAllocation*>& TransitionsOverTimeToUpdate);	

	/** 
	 * Creates a new allocation if needed, caches the result in PrimitiveSceneInfo->IndirectLightingCacheAllocation, 
	 * And adds an entry to the update lists when an update is needed. 
	 */
	void UpdateCachePrimitive(
		const TMap<FPrimitiveComponentId, FAttachmentGroupSceneInfo>& AttachmentGroups,
		FPrimitiveSceneInfo* PrimitiveSceneInfo,
		bool bAllowUnbuiltPreview, 
		bool bAllowVolumeSample, 
		TMap<FIntVector, FBlockUpdateInfo>& BlocksToUpdate, 
		TArray<FIndirectLightingCacheAllocation*>& TransitionsOverTimeToUpdate);	

	/** Updates the contents of the volume texture blocks in BlocksToUpdate. */
	void UpdateBlocks(FScene* Scene, FViewInfo* DebugDrawingView, TMap<FIntVector, FBlockUpdateInfo>& BlocksToUpdate);

	/** Updates any outstanding transitions with a new delta time. */
	void UpdateTransitionsOverTime(const TArray<FIndirectLightingCacheAllocation*>& TransitionsOverTimeToUpdate, float DeltaWorldTime) const;

	/** Creates an allocation to be used outside the indirect lighting cache and a block to be used internally. */
	FIndirectLightingCacheAllocation* CreateAllocation(int32 BlockSize, const FBoxSphereBounds& Bounds, bool bPointSample, bool bUnbuiltPreview);	

	/** Block accessors. */
	FIndirectLightingCacheBlock& FindBlock(FIntVector TexelMin);
	const FIndirectLightingCacheBlock& FindBlock(FIntVector TexelMin) const;

	/** Block operations. */
	void DeallocateBlock(FIntVector Min, int32 Size);
	bool AllocateBlock(int32 Size, FIntVector& OutMin);

	/**
	 * Updates an allocation block in the cache, by re-interpolating values and uploading to the cache volume texture.
	 * @param DebugDrawingView can be 0
	 */
	void UpdateBlock(FScene* Scene, FViewInfo* DebugDrawingView, FBlockUpdateInfo& Block);

	/** Interpolates a single SH sample from all levels. */
	void InterpolatePoint(
		FScene* Scene, 
		const FIndirectLightingCacheBlock& Block,
		float& OutDirectionalShadowing, 
		FSHVectorRGB3& OutIncidentRadiance,
		FVector& OutSkyBentNormal);

	/** Interpolates SH samples for a block from all levels. */
	void InterpolateBlock(
		FScene* Scene, 
		const FIndirectLightingCacheBlock& Block, 
		TArray<float>& AccumulatedWeight, 
		TArray<FSHVectorRGB2>& AccumulatedIncidentRadiance);

	/** 
	 * Normalizes, adjusts for SH ringing, and encodes SH samples into a texture format.
	 * @param DebugDrawingView can be 0
	 */
	void EncodeBlock(
		FViewInfo* DebugDrawingView,
		const FIndirectLightingCacheBlock& Block, 
		const TArray<float>& AccumulatedWeight, 
		const TArray<FSHVectorRGB2>& AccumulatedIncidentRadiance,
		TArray<FFloat16Color>& Texture0Data,
		TArray<FFloat16Color>& Texture1Data,
		TArray<FFloat16Color>& Texture2Data		
		);

	/** Helper that calculates an effective world position min and size given a bounds. */
	void CalculateBlockPositionAndSize(const FBoxSphereBounds& Bounds, int32 TexelSize, FVector& OutMin, FVector& OutSize) const;

	/** Helper that calculates a scale and add to convert world space position into volume texture UVs for a given block. */
	void CalculateBlockScaleAndAdd(FIntVector InTexelMin, int32 AllocationTexelSize, FVector InMin, FVector InSize, FVector& OutScale, FVector& OutAdd, FVector& OutMinUV, FVector& OutMaxUV) const;

	/** true: next rendering we update all entries no matter if they are visible to avoid further hitches */
	bool bUpdateAllCacheEntries;

	/** Size of the volume texture cache. */
	int32 CacheSize;

	/** Volume textures that store SH indirect lighting, interpolated from Lightmass volume samples. */
	TRefCountPtr<IPooledRenderTarget> Texture0;
	TRefCountPtr<IPooledRenderTarget> Texture1;
	TRefCountPtr<IPooledRenderTarget> Texture2;

	/** Tracks the allocation state of the atlas. */
	TMap<FIntVector, FIndirectLightingCacheBlock> VolumeBlocks;

	/** Tracks used sections of the volume texture atlas. */
	FTextureLayout3d BlockAllocator;

	int32 NextPointId;

	/** Tracks primitive allocations by component, so that they persist across re-registers. */
	TMap<FPrimitiveComponentId, FIndirectLightingCacheAllocation*> PrimitiveAllocations;

	friend class FUpdateCachePrimitivesTask;
};

/**
 * Bounding information used to cull primitives in the scene.
 */
struct FPrimitiveBounds
{
	FBoxSphereBounds BoxSphereBounds;
	/** Square of the minimum draw distance for the primitive. */
	float MinDrawDistanceSq;
	/** Maximum draw distance for the primitive. */
	float MaxDrawDistance;
};

/**
 * Precomputed primitive visibility ID.
 */
struct FPrimitiveVisibilityId
{
	/** Index in to the byte where precomputed occlusion data is stored. */
	int32 ByteIndex;
	/** Mast of the bit where precomputed occlusion data is stored. */
	uint8 BitMask;
};

/**
 * Flags that affect how primitives are occlusion culled.
 */
namespace EOcclusionFlags
{
	enum Type
	{
		/** No flags. */
		None = 0x0,
		/** Indicates the primitive can be occluded. */
		CanBeOccluded = 0x1,
		/** Allow the primitive to be batched with others to determine occlusion. */
		AllowApproximateOcclusion = 0x4,
		/** Indicates the primitive has a valid ID for precomputed visibility. */
		HasPrecomputedVisibility = 0x8,
		/** Indicates the primitive has a valid ID for precomputed visibility. */
		HasSubprimitiveQueries = 0x10,
	};
};

class FLODSceneTree
{
public:
	FLODSceneTree(FScene* InScene)
		: Scene(InScene)
		, LastHLODDistanceScale(-1.0f)
	{
	}

	/** Information about the primitives that are attached together. */
	struct FLODSceneNode
	{
		/** Children scene infos. */
		TArray<FPrimitiveSceneInfo*> ChildrenSceneInfos;

		/** The primitive. */
		FPrimitiveSceneInfo* SceneInfo;

		FLODSceneNode()
			: SceneInfo(nullptr)
		{
		}

		void AddChild(FPrimitiveSceneInfo* NewChild)
		{
			if(NewChild && !ChildrenSceneInfos.Contains(NewChild))
			{
				ChildrenSceneInfos.Add(NewChild);
			}
		}

		void RemoveChild(FPrimitiveSceneInfo* ChildToDelete)
		{
			if(ChildToDelete && ChildrenSceneInfos.Contains(ChildToDelete))
			{
				ChildrenSceneInfos.Remove(ChildToDelete);
			}
		}
	};

	void AddChildNode(FPrimitiveComponentId NodeId, FPrimitiveSceneInfo* ChildSceneInfo);
	void RemoveChildNode(FPrimitiveComponentId NodeId, FPrimitiveSceneInfo* ChildSceneInfo);

	void UpdateNodeSceneInfo(FPrimitiveComponentId NodeId, FPrimitiveSceneInfo* SceneInfo);
	void UpdateAndApplyVisibilityStates(FViewInfo& View);

	bool IsActive() const { return (SceneNodes.Num() > 0); }

private:

	void ResetHLODDistanceScaleApplication()
	{
		LastHLODDistanceScale = -1.0f;
	}

	/** Scene this Tree belong to */
	FScene* Scene;

	/** The LOD groups in the scene.  The map key is the current primitive who has children. */
	TMap<FPrimitiveComponentId, FLODSceneNode> SceneNodes;

	/** Transition distance scaling */
	float LastHLODDistanceScale;

	/** Recursive state updates */
	void ApplyNodeFadingToChildren(FSceneViewState* ViewState, FLODSceneNode& Node, FSceneBitArray& VisibilityFlags, const bool bIsFading, const bool bIsFadingOut);
	void HideNodeChildren(FSceneViewState* ViewState, FLODSceneNode& Node, FSceneBitArray& VisibilityFlags);
};

typedef TMap<FMaterial*, FMaterialShaderMap*> FMaterialsToUpdateMap;

class FCachedShadowMapData
{
public:
	FWholeSceneProjectedShadowInitializer Initializer;
	FShadowMapRenderTargetsRefCounted ShadowMap;
	float LastUsedTime;
	bool bCachedShadowMapHasPrimitives;

	FCachedShadowMapData(const FWholeSceneProjectedShadowInitializer& InInitializer, float InLastUsedTime) :
		Initializer(InInitializer),
		LastUsedTime(InLastUsedTime),
		bCachedShadowMapHasPrimitives(true)
	{}
};

#if WITH_EDITOR
	class FPixelInspectorData
	{
	public:
		FPixelInspectorData();

		void InitializeBuffers(FRenderTarget* BufferFinalColor, FRenderTarget* BufferSceneColor, FRenderTarget* BufferDepth, FRenderTarget* BufferHDR, FRenderTarget* BufferA, FRenderTarget* BufferBCDE, int32 bufferIndex);

		bool AddPixelInspectorRequest(FPixelInspectorRequest *PixelInspectorRequest);

		//Hold the buffer array
		TMap<FIntPoint, FPixelInspectorRequest *> Requests;

		FRenderTarget* RenderTargetBufferDepth[2];
		FRenderTarget* RenderTargetBufferFinalColor[2];
		FRenderTarget* RenderTargetBufferHDR[2];
		FRenderTarget* RenderTargetBufferSceneColor[2];
		FRenderTarget* RenderTargetBufferA[2];
		FRenderTarget* RenderTargetBufferBCDE[2];
	};
#endif //WITH_EDITOR

/** 
 * Renderer scene which is private to the renderer module.
 * Ordinarily this is the renderer version of a UWorld, but an FScene can be created for previewing in editors which don't have a UWorld as well.
 * The scene stores renderer state that is independent of any view or frame, with the primary actions being adding and removing of primitives and lights.
 */
class FScene : public FSceneInterface
{
public:

	/** An optional world associated with the scene. */
	UWorld* World;

	/** An optional FX system associated with the scene. */
	class FFXSystemInterface* FXSystem;

	// various static draw lists for this DPG

	/** position-only opaque depth draw list */
	TStaticMeshDrawList<FPositionOnlyDepthDrawingPolicy> PositionOnlyDepthDrawList;
	/** opaque depth draw list */
	TStaticMeshDrawList<FDepthDrawingPolicy> DepthDrawList;
	/** masked depth draw list */
	TStaticMeshDrawList<FDepthDrawingPolicy> MaskedDepthDrawList;
	/** Base pass draw list - no light map */
	TStaticMeshDrawList<TBasePassDrawingPolicy<FUniformLightMapPolicy> > BasePassUniformLightMapPolicyDrawList[EBasePass_MAX];
	/** Base pass draw list - self shadowed translucency*/
	TStaticMeshDrawList<TBasePassDrawingPolicy<FSelfShadowedTranslucencyPolicy> > BasePassSelfShadowedTranslucencyDrawList[EBasePass_MAX];
	TStaticMeshDrawList<TBasePassDrawingPolicy<FSelfShadowedCachedPointIndirectLightingPolicy> > BasePassSelfShadowedCachedPointIndirectTranslucencyDrawList[EBasePass_MAX];
	TStaticMeshDrawList<TBasePassDrawingPolicy<FSelfShadowedVolumetricLightmapPolicy> > BasePassSelfShadowedVolumetricLightmapTranslucencyDrawList[EBasePass_MAX];

	/** hit proxy draw list (includes both opaque and translucent objects) */
	TStaticMeshDrawList<FHitProxyDrawingPolicy> HitProxyDrawList;

	/** hit proxy draw list, with only opaque objects */
	TStaticMeshDrawList<FHitProxyDrawingPolicy> HitProxyDrawList_OpaqueOnly;

	/** draw list for motion blur velocities */
	TStaticMeshDrawList<FVelocityDrawingPolicy> VelocityDrawList;

	/** Draw list used for rendering whole scene shadow depths. */
	TStaticMeshDrawList<FShadowDepthDrawingPolicy<false> > WholeSceneShadowDepthDrawList;

	/** Draw list used for rendering whole scene reflective shadow maps.  */
	TStaticMeshDrawList<FShadowDepthDrawingPolicy<true> > WholeSceneReflectiveShadowMapDrawList;

	/** Maps a light-map type to the appropriate base pass draw list. */
	template<typename LightMapPolicyType>
	TStaticMeshDrawList<TBasePassDrawingPolicy<LightMapPolicyType> >& GetBasePassDrawList(EBasePassDrawListType DrawType);

	/** Mobile base pass draw lists */
	TStaticMeshDrawList<TMobileBasePassDrawingPolicy<FUniformLightMapPolicy, 0> > MobileBasePassUniformLightMapPolicyDrawList[EBasePass_MAX];
	TStaticMeshDrawList<TMobileBasePassDrawingPolicy<FUniformLightMapPolicy, 0> > MobileBasePassUniformLightMapPolicyDrawListWithCSM[EBasePass_MAX];


	/** Maps a light-map type to the appropriate base pass draw list. */
	template<typename LightMapPolicyType>
	TStaticMeshDrawList<TMobileBasePassDrawingPolicy<LightMapPolicyType,0> >& GetMobileBasePassDrawList(EBasePassDrawListType DrawType);

	template<typename LightMapPolicyType>
	TStaticMeshDrawList<TMobileBasePassDrawingPolicy<LightMapPolicyType, 0> >& GetMobileBasePassCSMDrawList(EBasePassDrawListType DrawType);

#if WITH_EDITOR
	/** Draw list to use for selected static meshes in the editor only */
	TStaticMeshDrawList<FEditorSelectionDrawingPolicy> EditorSelectionDrawList;
#endif
	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	/** Voxelization draw list */
	TStaticMeshDrawList<TVXGIVoxelizationDrawingPolicy< FVXGIVoxelizationNoLightMapPolicy> > VxgiVoxelizationDrawList;
#endif
	// NVCHANGE_END: Add VXGI

	/**
	 * The following arrays are densely packed primitive data needed by various
	 * rendering passes. PrimitiveSceneInfo->PackedIndex maintains the index
	 * where data is stored in these arrays for a given primitive.
	 */

	/** Packed array of primitives in the scene. */
	TArray<FPrimitiveSceneInfo*> Primitives;
	/** Packed array of primitive scene proxies in the scene. */
	TArray<FPrimitiveSceneProxy*> PrimitiveSceneProxies;
	/** Packed array of primitive bounds. */
	TArray<FPrimitiveBounds> PrimitiveBounds;
	/** Packed array of primitive flags. */
	TArray<FPrimitiveFlagsCompact> PrimitiveFlagsCompact;
	/** Packed array of precomputed primitive visibility IDs. */
	TArray<FPrimitiveVisibilityId> PrimitiveVisibilityIds;
	/** Packed array of primitive occlusion flags. See EOcclusionFlags. */
	TArray<uint8> PrimitiveOcclusionFlags;
	/** Packed array of primitive occlusion bounds. */
	TArray<FBoxSphereBounds> PrimitiveOcclusionBounds;
	/** Packed array of primitive components associated with the primitive. */
	TArray<FPrimitiveComponentId> PrimitiveComponentIds;

	/** The lights in the scene. */
	TSparseArray<FLightSceneInfoCompact> Lights;

	/** 
	 * Lights in the scene which are invisible, but still needed by the editor for previewing. 
	 * Lights in this array cannot be in the Lights array.  They also are not fully set up, as AddLightSceneInfo_RenderThread is not called for them.
	 */
	TSparseArray<FLightSceneInfoCompact> InvisibleLights;

	/** Shadow casting lights that couldn't get a shadowmap channel assigned and therefore won't have valid dynamic shadows, forward renderer only. */
	TArray<FName> OverflowingDynamicShadowedLights;

	/** The mobile quality level for which static draw lists have been built. */
	bool bStaticDrawListsMobileHDR;
	bool bStaticDrawListsMobileHDR32bpp;

	/** Whether the early Z pass was force enabled when static draw lists were built. */
	int32 StaticDrawListsEarlyZPassMode;

	/** Whether the ShaderPipelines were enabled when the static draw lists were built. */
	int32 StaticDrawShaderPipelines;

	/** True if a change to SkyLight / Lighting has occurred that requires static draw lists to be updated. */
	bool bScenesPrimitivesNeedStaticMeshElementUpdate;	

	/** The scene's sky light, if any. */
	FSkyLightSceneProxy* SkyLight;

	/** Used to track the order that skylights were enabled in. */
	TArray<FSkyLightSceneProxy*> SkyLightStack;

	/** The directional light to use for simple dynamic lighting, if any. */
	FLightSceneInfo* SimpleDirectionalLight;

	/** For the mobile renderer, the first directional light in each lighting channel. */
	FLightSceneInfo* MobileDirectionalLights[NUM_LIGHTING_CHANNELS];

	/** The sun light for atmospheric effect, if any. */
	FLightSceneInfo* SunLight;

	/** The decals in the scene. */
	TSparseArray<FDeferredDecalProxy*> Decals;

	/** Potential capsule shadow casters registered to the scene. */
	TArray<FPrimitiveSceneInfo*> DynamicIndirectCasterPrimitives; 

	TArray<class FPlanarReflectionSceneProxy*> PlanarReflections;
	TArray<class UPlanarReflectionComponent*> PlanarReflections_GameThread;

	/** State needed for the reflection environment feature. */
	FReflectionEnvironmentSceneData ReflectionSceneData;

	/** 
	 * Precomputed lighting volumes in the scene, used for interpolating dynamic object lighting.
	 * These are typically one per streaming level and they store volume lighting samples computed by Lightmass. 
	 */
	TArray<const FPrecomputedLightVolume*> PrecomputedLightVolumes;

	/** Interpolates and caches indirect lighting for dynamic objects. */
	FIndirectLightingCache IndirectLightingCache;

	FVolumetricLightmapSceneData VolumetricLightmapSceneData;

	/** Distance field object scene data. */
	FDistanceFieldSceneData DistanceFieldSceneData;
	
	/** Map from light id to the cached shadowmap data for that light. */
	TMap<int32, FCachedShadowMapData> CachedShadowMaps;

	TRefCountPtr<IPooledRenderTarget> PreShadowCacheDepthZ;

	/** Preshadows that are currently cached in the PreshadowCache render target. */
	TArray<TRefCountPtr<FProjectedShadowInfo> > CachedPreshadows;

	/** Texture layout that tracks current allocations in the PreshadowCache render target. */
	FTextureLayout PreshadowCacheLayout;

	/** The static meshes in the scene. */
	TSparseArray<FStaticMesh*> StaticMeshes;

	/** This sparse array is used just to track free indices for FStaticMesh::BatchVisibilityId. */
	TSparseArray<bool> StaticMeshBatchVisibility;

	/** The exponential fog components in the scene. */
	TArray<FExponentialHeightFogSceneInfo> ExponentialFogs;

	/** The atmospheric fog components in the scene. */
	FAtmosphericFogSceneInfo* AtmosphericFog;

	/** The wind sources in the scene. */
	TArray<class FWindSourceSceneProxy*> WindSources;

	/** Wind source components, tracked so the game thread can also access wind parameters */
	TArray<UWindDirectionalSourceComponent*> WindComponents_GameThread;

	/** SpeedTree wind objects in the scene. FLocalVertexFactoryShaderParameters needs to lookup by FVertexFactory, but wind objects are per tree (i.e. per UStaticMesh)*/
	TMap<const UStaticMesh*, struct FSpeedTreeWindComputation*> SpeedTreeWindComputationMap;
	TMap<FVertexFactory*, const UStaticMesh*> SpeedTreeVertexFactoryMap;

	/** The attachment groups in the scene.  The map key is the attachment group's root primitive. */
	TMap<FPrimitiveComponentId,FAttachmentGroupSceneInfo> AttachmentGroups;

	/** Precomputed visibility data for the scene. */
	const FPrecomputedVisibilityHandler* PrecomputedVisibilityHandler;

	/** An octree containing the shadow casting lights in the scene. */
	FSceneLightOctree LightOctree;

	/** An octree containing the primitives in the scene. */
	FScenePrimitiveOctree PrimitiveOctree;

	/** Indicates whether this scene requires hit proxy rendering. */
	bool bRequiresHitProxies;

	/** Whether this is an editor scene. */
	bool bIsEditorScene;

	/** Set by the rendering thread to signal to the game thread that the scene needs a static lighting build. */
	volatile mutable int32 NumUncachedStaticLightingInteractions;

	/** Track numbers of various lights types on mobile, used to show warnings for disabled shader permutations. */
	int32 NumMobileStaticAndCSMLights_RenderThread;
	int32 NumMobileMovableDirectionalLights_RenderThread;

	FMotionBlurInfoData MotionBlurInfoData;

	/** GPU Skinning cache, if enabled */
	class FGPUSkinCache* GPUSkinCache;

	/** Uniform buffers for parameter collections with the corresponding Ids. */
	TMap<FGuid, FUniformBufferRHIRef> ParameterCollections;

	/** LOD Tree Holder for massive LOD system */
	FLODSceneTree SceneLODHierarchy;

	float DefaultMaxDistanceFieldOcclusionDistance;

	float GlobalDistanceFieldViewDistance;

	float DynamicIndirectShadowsSelfShadowingIntensity;

	const FReadOnlyCVARCache& ReadOnlyCVARCache;

	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	struct FNVVolumetricLightingProperties*	VolumetricLightingProperties;
#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting

#if WITH_EDITOR
	/** Editor Pixel inspector */
	FPixelInspectorData PixelInspectorData;
#endif //WITH_EDITOR

	/** Initialization constructor. */
	FScene(UWorld* InWorld, bool bInRequiresHitProxies,bool bInIsEditorScene, bool bCreateFXSystem, ERHIFeatureLevel::Type InFeatureLevel);

	virtual ~FScene();

	// FSceneInterface interface.
	virtual void AddPrimitive(UPrimitiveComponent* Primitive) override;
	virtual void RemovePrimitive(UPrimitiveComponent* Primitive) override;
	virtual void ReleasePrimitive(UPrimitiveComponent* Primitive) override;
	virtual void UpdatePrimitiveTransform(UPrimitiveComponent* Primitive) override;
	virtual void UpdatePrimitiveAttachment(UPrimitiveComponent* Primitive) override;
	virtual FPrimitiveSceneInfo* GetPrimitiveSceneInfo(int32 PrimitiveIndex) override;
	virtual void AddLight(ULightComponent* Light) override;
	virtual void RemoveLight(ULightComponent* Light) override;
	virtual void AddInvisibleLight(ULightComponent* Light) override;
	virtual void SetSkyLight(FSkyLightSceneProxy* Light) override;
	virtual void DisableSkyLight(FSkyLightSceneProxy* Light) override;
	virtual void AddDecal(UDecalComponent* Component) override;
	virtual void RemoveDecal(UDecalComponent* Component) override;
	virtual void UpdateDecalTransform(UDecalComponent* Decal) override;
	virtual void AddReflectionCapture(UReflectionCaptureComponent* Component) override;
	virtual void RemoveReflectionCapture(UReflectionCaptureComponent* Component) override;
	virtual void GetReflectionCaptureData(UReflectionCaptureComponent* Component, class FReflectionCaptureFullHDR& OutDerivedData) override;
	virtual void UpdateReflectionCaptureTransform(UReflectionCaptureComponent* Component) override;
	virtual void ReleaseReflectionCubemap(UReflectionCaptureComponent* CaptureComponent) override;
	virtual void AddPlanarReflection(class UPlanarReflectionComponent* Component) override;
	virtual void RemovePlanarReflection(UPlanarReflectionComponent* Component) override;
	virtual void UpdatePlanarReflectionTransform(UPlanarReflectionComponent* Component) override;
	virtual void UpdateSceneCaptureContents(class USceneCaptureComponent2D* CaptureComponent) override;
	virtual void UpdateSceneCaptureContents(class USceneCaptureComponentCube* CaptureComponent) override;
// WaveWorks Start
	virtual void UpdateSceneCaptureContents(class UWaveWorksShorelineCaptureComponent* CaptureComponent) override;
// WaveWorks End
	virtual void UpdatePlanarReflectionContents(UPlanarReflectionComponent* CaptureComponent, FSceneRenderer& MainSceneRenderer) override;
	virtual void AllocateReflectionCaptures(const TArray<UReflectionCaptureComponent*>& NewCaptures) override;
	virtual void UpdateSkyCaptureContents(const USkyLightComponent* CaptureComponent, bool bCaptureEmissiveOnly, UTextureCube* SourceCubemap, FTexture* OutProcessedTexture, float& OutAverageBrightness, FSHVectorRGB3& OutIrradianceEnvironmentMap, TArray<FFloat16Color>* OutRadianceMap) override; 
	virtual void AddPrecomputedLightVolume(const class FPrecomputedLightVolume* Volume) override;
	virtual void RemovePrecomputedLightVolume(const class FPrecomputedLightVolume* Volume) override;
	virtual bool HasPrecomputedVolumetricLightmap_RenderThread() const override;
	virtual void AddPrecomputedVolumetricLightmap(const class FPrecomputedVolumetricLightmap* Volume) override;
	virtual void RemovePrecomputedVolumetricLightmap(const class FPrecomputedVolumetricLightmap* Volume) override;
	virtual void UpdateLightTransform(ULightComponent* Light) override;
	virtual void UpdateLightColorAndBrightness(ULightComponent* Light) override;
	virtual void AddExponentialHeightFog(UExponentialHeightFogComponent* FogComponent) override;
	virtual void RemoveExponentialHeightFog(UExponentialHeightFogComponent* FogComponent) override;
	virtual void AddAtmosphericFog(UAtmosphericFogComponent* FogComponent) override;
	virtual void RemoveAtmosphericFog(UAtmosphericFogComponent* FogComponent) override;
	virtual void RemoveAtmosphericFogResource_RenderThread(FRenderResource* FogResource) override;
	virtual FAtmosphericFogSceneInfo* GetAtmosphericFogSceneInfo() override { return AtmosphericFog; }
	virtual void AddWindSource(UWindDirectionalSourceComponent* WindComponent) override;
	virtual void RemoveWindSource(UWindDirectionalSourceComponent* WindComponent) override;
	virtual const TArray<FWindSourceSceneProxy*>& GetWindSources_RenderThread() const override;
	virtual void GetWindParameters(const FVector& Position, FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const override;
	virtual void GetWindParameters_GameThread(const FVector& Position, FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const override;
	virtual void GetDirectionalWindParameters(FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const override;
	virtual void AddSpeedTreeWind(FVertexFactory* VertexFactory, const UStaticMesh* StaticMesh) override;
	virtual void RemoveSpeedTreeWind(FVertexFactory* VertexFactory, const UStaticMesh* StaticMesh) override;
	virtual void RemoveSpeedTreeWind_RenderThread(FVertexFactory* VertexFactory, const UStaticMesh* StaticMesh) override;
	virtual void UpdateSpeedTreeWind(double CurrentTime) override;
	virtual FUniformBufferRHIParamRef GetSpeedTreeUniformBuffer(const FVertexFactory* VertexFactory) override;
	virtual void DumpUnbuiltLightInteractions( FOutputDevice& Ar ) const override;
	virtual void DumpStaticMeshDrawListStats() const override;
	virtual void SetClearMotionBlurInfoGameThread() override;
	virtual void UpdateParameterCollections(const TArray<FMaterialParameterCollectionInstanceResource*>& InParameterCollections) override;

	/** Determines whether the scene has atmospheric fog and sun light. */
	bool HasAtmosphericFog() const
	{
		return (AtmosphericFog != NULL); // Use default value when Sun Light is not existing
	}

	/**
	 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
	 *
	 * @param	Primitive				Primitive to retrieve interacting lights for
	 * @param	RelevantLights	[out]	Array of lights interacting with primitive
	 */
	virtual void GetRelevantLights( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const override;

	/** Sets the precomputed visibility handler for the scene, or NULL to clear the current one. */
	virtual void SetPrecomputedVisibility(const FPrecomputedVisibilityHandler* InPrecomputedVisibilityHandler) override;

	/** Sets shader maps on the specified materials without blocking. */
	virtual void SetShaderMapsOnMaterialResources(const TMap<FMaterial*, class FMaterialShaderMap*>& MaterialsToUpdate) override;

	/** Updates static draw lists for the given set of materials. */
	virtual void UpdateStaticDrawListsForMaterials(const TArray<const FMaterial*>& Materials) override;

	virtual void Release() override;
	virtual UWorld* GetWorld() const override { return World; }

	/** Finds the closest reflection capture to a point in space. */
	const FReflectionCaptureProxy* FindClosestReflectionCapture(FVector Position) const;

	const class FPlanarReflectionSceneProxy* FindClosestPlanarReflection(const FBoxSphereBounds& Bounds) const;

	void FindClosestReflectionCaptures(FVector Position, const FReflectionCaptureProxy* (&SortedByDistanceOUT)[FPrimitiveSceneInfo::MaxCachedReflectionCaptureProxies]) const;
	
	/** 
	 * Gets the scene's cubemap array and index into that array for the given reflection proxy. 
	 * If the proxy was not found in the scene's reflection state, the outputs are not written to.
	 */
	void GetCaptureParameters(const FReflectionCaptureProxy* ReflectionProxy, FTextureRHIParamRef& ReflectionCubemapArray, int32& ArrayIndex) const;

	int64 GetCachedWholeSceneShadowMapsSize() const;

	/**
	 * Marks static mesh elements as needing an update if necessary.
	 */
	void ConditionalMarkStaticMeshElementsForUpdate();

	/**
	 * @return		true if hit proxies should be rendered in this scene.
	 */
	virtual bool RequiresHitProxies() const override;

	SIZE_T GetSizeBytes() const;

	/**
	* Return the scene to be used for rendering
	*/
	virtual class FScene* GetRenderScene() override
	{
		return this;
	}

	virtual void UpdateSceneSettings(AWorldSettings* WorldSettings) override;

	virtual class FGPUSkinCache* GetGPUSkinCache() override
	{
		return GPUSkinCache;
	}

	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	virtual void UpdateVolumetricLightingSettings(AWorldSettings* WorldSettings) override;
#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting

	/**
	 * Sets the FX system associated with the scene.
	 */
	virtual void SetFXSystem( class FFXSystemInterface* InFXSystem ) override;

	/**
	 * Get the FX system associated with the scene.
	 */
	virtual class FFXSystemInterface* GetFXSystem() override;

	/**
	 * Exports the scene.
	 *
	 * @param	Ar		The Archive used for exporting.
	 **/
	virtual void Export( FArchive& Ar ) const override;

	FUniformBufferRHIParamRef GetParameterCollectionBuffer(const FGuid& InId) const
	{
		const FUniformBufferRHIRef* ExistingUniformBuffer = ParameterCollections.Find(InId);

		if (ExistingUniformBuffer)
		{
			return *ExistingUniformBuffer;
		}

		return FUniformBufferRHIParamRef();
	}

	virtual void ApplyWorldOffset(FVector InOffset) override;

	virtual void OnLevelAddedToWorld(FName InLevelName, UWorld* InWorld, bool bIsLightingScenario) override;
	virtual void OnLevelRemovedFromWorld(UWorld* InWorld, bool bIsLightingScenario) override;

	virtual bool HasAnyLights() const override 
	{ 
		check(IsInGameThread());
		return NumVisibleLights_GameThread > 0 || NumEnabledSkylights_GameThread > 0; 
	}

	virtual bool IsEditorScene() const override { return bIsEditorScene; }

	virtual ERHIFeatureLevel::Type GetFeatureLevel() const override { return FeatureLevel; }

	bool ShouldRenderSkylightInBasePass(EBlendMode BlendMode) const
	{
		return ShouldRenderSkylightInBasePass_Internal(BlendMode) && (ReadOnlyCVARCache.bEnableStationarySkylight || IsSimpleForwardShadingEnabled(GetShaderPlatform()));
	}

	bool ShouldRenderSkylightInBasePass_Internal(EBlendMode BlendMode) const
	{
		if (IsTranslucentBlendMode(BlendMode))
		{
			//both stationary and movable skylights are applied during actual translucency render
			return SkyLight && !SkyLight->bHasStaticLighting;
		}
		else
	{
		const bool bRenderSkylight = SkyLight
			&& !SkyLight->bHasStaticLighting
			// The deferred shading renderer does movable skylight diffuse in a later deferred pass, not in the base pass
				// bWantsStaticShadowing means 'stationary skylight'
			&& (SkyLight->bWantsStaticShadowing || IsAnyForwardShadingEnabled(GetShaderPlatform()));

		return bRenderSkylight;
	}
	}

	virtual TArray<FPrimitiveComponentId> GetScenePrimitiveComponentIds() const override
	{
		return PrimitiveComponentIds;
	}

#if WITH_EDITOR
	virtual bool InitializePixelInspector(FRenderTarget* BufferFinalColor, FRenderTarget* BufferSceneColor, FRenderTarget* BufferDepth, FRenderTarget* BufferHDR, FRenderTarget* BufferA, FRenderTarget* BufferBCDE, int32 BufferIndex) override;

	virtual bool AddPixelInspectorRequest(FPixelInspectorRequest *PixelInspectorRequest) override;
#endif //WITH_EDITOR

	virtual uint32 GetFrameNumber() const override
	{
		return SceneFrameNumber;
	}

	virtual void IncrementFrameNumber() override
	{
		++SceneFrameNumber;
	}

	void EnsureMotionBlurCacheIsUpToDate(bool bWorldIsPaused);

	void ResetMotionBlurCacheTracking()
	{
		CurrentFrameUpdatedMotionBlurCache = false;
	}

private:

	/**
	 * Ensures the packed primitive arrays contain the same number of elements.
	 */
	void CheckPrimitiveArrays();

	/**
	 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
	 * Render thread version of function.
	 * @param	Primitive				Primitive to retrieve interacting lights for
	 * @param	RelevantLights	[out]	Array of lights interacting with primitive
	 */
	void GetRelevantLights_RenderThread( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const;

	/**
	 * Adds a primitive to the scene.  Called in the rendering thread by AddPrimitive.
	 * @param PrimitiveSceneInfo - The primitive being added.
	 */
	void AddPrimitiveSceneInfo_RenderThread(FRHICommandListImmediate& RHICmdList, FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/**
	 * Removes a primitive from the scene.  Called in the rendering thread by RemovePrimitive.
	 * @param PrimitiveSceneInfo - The primitive being removed.
	 */
	void RemovePrimitiveSceneInfo_RenderThread(FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/** Updates a primitive's transform, called on the rendering thread. */
	void UpdatePrimitiveTransform_RenderThread(FRHICommandListImmediate& RHICmdList, FPrimitiveSceneProxy* PrimitiveSceneProxy, const FBoxSphereBounds& WorldBounds, const FBoxSphereBounds& LocalBounds, const FMatrix& LocalToWorld, const FVector& OwnerPosition);

	/** Updates a single primitive's lighting attachment root. */
	void UpdatePrimitiveLightingAttachmentRoot(UPrimitiveComponent* Primitive);

	void AssignAvailableShadowMapChannelForLight(FLightSceneInfo* LightSceneInfo);

	/**
	 * Adds a light to the scene.  Called in the rendering thread by AddLight.
	 * @param LightSceneInfo - The light being added.
	 */
	void AddLightSceneInfo_RenderThread(FLightSceneInfo* LightSceneInfo);

	/**
	 * Adds a decal to the scene.  Called in the rendering thread by AddDecal or RemoveDecal.
	 * @param Component - The object that should being added or removed.
	 * @param bAdd true:add, FASLE:remove
	 */
	void AddOrRemoveDecal_RenderThread(FDeferredDecalProxy* Component, bool bAdd);

	/**
	 * Removes a light from the scene.  Called in the rendering thread by RemoveLight.
	 * @param LightSceneInfo - The light being removed.
	 */
	void RemoveLightSceneInfo_RenderThread(FLightSceneInfo* LightSceneInfo);

	void UpdateLightTransform_RenderThread(FLightSceneInfo* LightSceneInfo, const struct FUpdateLightTransformParameters& Parameters);

	/** 
	* Updates the contents of the given reflection capture by rendering the scene. 
	* This must be called on the game thread.
	*/
	void UpdateReflectionCaptureContents(UReflectionCaptureComponent* CaptureComponent);

	/** Updates the contents of all reflection captures in the scene.  Must be called from the game thread. */
	void UpdateAllReflectionCaptures();

	/** Sets shader maps on the specified materials without blocking. */
	void SetShaderMapsOnMaterialResources_RenderThread(FRHICommandListImmediate& RHICmdList, const FMaterialsToUpdateMap& MaterialsToUpdate);

	/** Updates static draw lists for the given materials. */
	void UpdateStaticDrawListsForMaterials_RenderThread(FRHICommandListImmediate& RHICmdList, const TArray<const FMaterial*>& Materials);

	/**
	 * Shifts scene data by provided delta
	 * Called on world origin changes
	 * 
	 * @param	InOffset	Delta to shift scene by
	 */
	void ApplyWorldOffset_RenderThread(FVector InOffset);

	/**
	 * Notification from game thread that level was added to a world
	 *
	 * @param	InLevelName		Level name
	 */
	void OnLevelAddedToWorld_RenderThread(FName InLevelName);

private:
	/** 
	 * The number of visible lights in the scene
	 * Note: This is tracked on the game thread!
	 */
	int32 NumVisibleLights_GameThread;

	/** 
	 * Whether the scene has a valid sky light.
	 * Note: This is tracked on the game thread!
	 */
	int32 NumEnabledSkylights_GameThread;

	/** This scene's feature level */
	ERHIFeatureLevel::Type FeatureLevel;

	/** Frame number incremented per-family viewing this scene. */
	uint32 SceneFrameNumber;

	/** Whether the motion blur cache has been updated already for this frame. */
	bool CurrentFrameUpdatedMotionBlurCache;
};

inline bool ShouldIncludeDomainInMeshPass(EMaterialDomain Domain)
{
	// Non-Surface domains can be applied to static meshes for thumbnails or material editor preview
	// Volume domain materials however must only be rendered in the voxelization pass
	return Domain != MD_Volume;
}

#include "BasePassRendering.inl"

