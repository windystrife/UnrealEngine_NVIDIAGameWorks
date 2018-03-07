// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneVisibility.cpp: Scene visibility determination.
=============================================================================*/

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "Stats/Stats.h"
#include "Misc/MemStack.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineDefines.h"
#include "EngineGlobals.h"
#include "RHIDefinitions.h"
#include "SceneTypes.h"
#include "SceneInterface.h"
#include "RendererInterface.h"
#include "PrimitiveViewRelevance.h"
#include "MaterialShared.h"
#include "SceneManagement.h"
#include "ScenePrivateBase.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneCore.h"
#include "LightSceneInfo.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "DynamicPrimitiveDrawing.h"
#include "ScenePrivate.h"
#include "FXSystem.h"
#include "PostProcess/PostProcessing.h"
#include "SceneView.h"

// @third party code - BEGIN HairWorks
#include "HairWorksRenderer.h"
// @third party code - END HairWorks
/*------------------------------------------------------------------------------
	Globals
------------------------------------------------------------------------------*/

static float GWireframeCullThreshold = 5.0f;
static FAutoConsoleVariableRef CVarWireframeCullThreshold(
	TEXT("r.WireframeCullThreshold"),
	GWireframeCullThreshold,
	TEXT("Threshold below which objects in ortho wireframe views will be culled."),
	ECVF_RenderThreadSafe
	);

float GMinScreenRadiusForLights = 0.03f;
static FAutoConsoleVariableRef CVarMinScreenRadiusForLights(
	TEXT("r.MinScreenRadiusForLights"),
	GMinScreenRadiusForLights,
	TEXT("Threshold below which lights will be culled."),
	ECVF_RenderThreadSafe
	);

float GMinScreenRadiusForDepthPrepass = 0.03f;
static FAutoConsoleVariableRef CVarMinScreenRadiusForDepthPrepass(
	TEXT("r.MinScreenRadiusForDepthPrepass"),
	GMinScreenRadiusForDepthPrepass,
	TEXT("Threshold below which meshes will be culled from depth only pass."),
	ECVF_RenderThreadSafe
	);

float GMinScreenRadiusForCSMDepth = 0.01f;
static FAutoConsoleVariableRef CVarMinScreenRadiusForCSMDepth(
	TEXT("r.MinScreenRadiusForCSMDepth"),
	GMinScreenRadiusForCSMDepth,
	TEXT("Threshold below which meshes will be culled from CSM depth pass."),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarTemporalAASamples(
	TEXT("r.TemporalAASamples"),
	8,
	TEXT("Number of jittered positions for temporal AA (4, 8=default, 16, 32, 64)."),
	ECVF_RenderThreadSafe);

static int32 GHZBOcclusion = 0;
static FAutoConsoleVariableRef CVarHZBOcclusion(
	TEXT("r.HZBOcclusion"),
	GHZBOcclusion,
	TEXT("Defines which occlusion system is used.\n")
	TEXT(" 0: Hardware occlusion queries\n")
	TEXT(" 1: Use HZB occlusion system (default, less GPU and CPU cost, more conservative results)")
	TEXT(" 2: Force HZB occlusion system (overrides rendering platform preferences)"),
	ECVF_RenderThreadSafe
	);

static int32 GVisualizeOccludedPrimitives = 0;
static FAutoConsoleVariableRef CVarVisualizeOccludedPrimitives(
	TEXT("r.VisualizeOccludedPrimitives"),
	GVisualizeOccludedPrimitives,
	TEXT("Draw boxes for all occluded primitives"),
	ECVF_RenderThreadSafe | ECVF_Cheat
	);

static int32 GAllowSubPrimitiveQueries = 1;
static FAutoConsoleVariableRef CVarAllowSubPrimitiveQueries(
	TEXT("r.AllowSubPrimitiveQueries"),
	GAllowSubPrimitiveQueries,
	TEXT("Enables sub primitive queries, currently only used by hierarchical instanced static meshes. 1: Enable, 0 Disabled. When disabled, one query is used for the entire proxy."),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarStaticMeshLODDistanceScale(
	TEXT("r.StaticMeshLODDistanceScale"),
	1.0f,
	TEXT("Scale factor for the distance used in computing discrete LOD for static meshes. (defaults to 1)\n")
	TEXT("(higher values make LODs transition earlier, e.g., 2 is twice as fast / half the distance)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarHLODDistanceScale(
	TEXT("r.HLOD.DistanceScale"),
	1.0f,
	TEXT("Scale factor for the distance used in computing discrete HLOD for transition for static meshes. (defaults to 1)\n")
	TEXT("(higher values make HLODs transition farther away, e.g., 2 is twice the distance)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static int32 GOcclusionCullParallelPrimFetch = 0;
static FAutoConsoleVariableRef CVarOcclusionCullParallelPrimFetch(
	TEXT("r.OcclusionCullParallelPrimFetch"),
	GOcclusionCullParallelPrimFetch,
	TEXT("Enables Parallel Occlusion Cull primitive fetch."),
	ECVF_RenderThreadSafe
	);

static int32 GILCUpdatePrimTaskEnabled = 0;
static FAutoConsoleVariableRef CVarILCUpdatePrimitivesTask(
	TEXT("r.Cache.UpdatePrimsTaskEnabled"),
	GILCUpdatePrimTaskEnabled,
	TEXT("Enable threading for ILC primitive update.  Will overlap with the rest the end of InitViews."),
	ECVF_RenderThreadSafe
	);

static int32 GDoInitViewsLightingAfterPrepass = 0;
static FAutoConsoleVariableRef CVarDoInitViewsLightingAfterPrepass(
	TEXT("r.DoInitViewsLightingAfterPrepass"),
	GDoInitViewsLightingAfterPrepass,
	TEXT("Delays the lighting part of InitViews until after the prepass. This improves the threading throughput and gets the prepass to the GPU ASAP. Experimental options; has an unknown race."),
	ECVF_RenderThreadSafe
	);

/** Distance fade cvars */
static int32 GDisableLODFade = false;
static FAutoConsoleVariableRef CVarDisableLODFade( TEXT("r.DisableLODFade"), GDisableLODFade, TEXT("Disable fading for distance culling"), ECVF_RenderThreadSafe );

static float GFadeTime = 0.25f;
static FAutoConsoleVariableRef CVarLODFadeTime( TEXT("r.LODFadeTime"), GFadeTime, TEXT("How long LOD takes to fade (in seconds)."), ECVF_RenderThreadSafe );

static float GDistanceFadeMaxTravel = 1000.0f;
static FAutoConsoleVariableRef CVarDistanceFadeMaxTravel( TEXT("r.DistanceFadeMaxTravel"), GDistanceFadeMaxTravel, TEXT("Max distance that the player can travel during the fade time."), ECVF_RenderThreadSafe );


static TAutoConsoleVariable<int32> CVarParallelInitViews(
	TEXT("r.ParallelInitViews"),
#if WITH_EDITOR
	0,  
#else
	1,  
#endif
	TEXT("Toggles parallel init views. 0 = off; 1 = on"),
	ECVF_RenderThreadSafe
	);

float GLightMaxDrawDistanceScale = 1.0f;
static FAutoConsoleVariableRef CVarLightMaxDrawDistanceScale(
	TEXT("r.LightMaxDrawDistanceScale"),
	GLightMaxDrawDistanceScale,
	TEXT("Scale applied to the MaxDrawDistance of lights.  Useful for fading out local lights more aggressively on some platforms."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

/*------------------------------------------------------------------------------
	Visibility determination.
------------------------------------------------------------------------------*/

/**
 * Update a primitive's fading state.
 * @param FadingState - State to update.
 * @param View - The view for which to update.
 * @param bVisible - Whether the primitive should be visible in the view.
 */
static void UpdatePrimitiveFadingState(FPrimitiveFadingState& FadingState, FViewInfo& View, bool bVisible)
{
	if (FadingState.bValid)
	{
		if (FadingState.bIsVisible != bVisible)
		{
			float CurrentRealTime = View.Family->CurrentRealTime;

			// Need to kick off a fade, so make sure that we have fading state for that
			if( !IsValidRef(FadingState.UniformBuffer) )
			{
				// Primitive is not currently fading.  Start a new fade!
				FadingState.EndTime = CurrentRealTime + GFadeTime;

				if( bVisible )
				{
					// Fading in
					// (Time - StartTime) / FadeTime
					FadingState.FadeTimeScaleBias.X = 1.0f / GFadeTime;
					FadingState.FadeTimeScaleBias.Y = -CurrentRealTime / GFadeTime;
				}
				else
				{
					// Fading out
					// 1 - (Time - StartTime) / FadeTime
					FadingState.FadeTimeScaleBias.X = -1.0f / GFadeTime;
					FadingState.FadeTimeScaleBias.Y = 1.0f + CurrentRealTime / GFadeTime;
				}

				FDistanceCullFadeUniformShaderParameters Uniforms;
				Uniforms.FadeTimeScaleBias = FadingState.FadeTimeScaleBias;
				FadingState.UniformBuffer = FDistanceCullFadeUniformBufferRef::CreateUniformBufferImmediate( Uniforms, UniformBuffer_MultiFrame );
			}
			else
			{
				// Reverse fading direction but maintain current opacity
				// Solve for d: a*x+b = -a*x+d
				FadingState.FadeTimeScaleBias.Y = 2.0f * CurrentRealTime * FadingState.FadeTimeScaleBias.X + FadingState.FadeTimeScaleBias.Y;
				FadingState.FadeTimeScaleBias.X = -FadingState.FadeTimeScaleBias.X;
				
				if( bVisible )
				{
					// Fading in
					// Solve for x: a*x+b = 1
					FadingState.EndTime = ( 1.0f - FadingState.FadeTimeScaleBias.Y ) / FadingState.FadeTimeScaleBias.X;
				}
				else
				{
					// Fading out
					// Solve for x: a*x+b = 0
					FadingState.EndTime = -FadingState.FadeTimeScaleBias.Y / FadingState.FadeTimeScaleBias.X;
				}

				FDistanceCullFadeUniformShaderParameters Uniforms;
				Uniforms.FadeTimeScaleBias = FadingState.FadeTimeScaleBias;
				FadingState.UniformBuffer = FDistanceCullFadeUniformBufferRef::CreateUniformBufferImmediate( Uniforms, UniformBuffer_MultiFrame );
			}
		}
	}

	FadingState.FrameNumber = View.Family->FrameNumber;
	FadingState.bIsVisible = bVisible;
	FadingState.bValid = true;
}

bool FViewInfo::IsDistanceCulled( float DistanceSquared, float MinDrawDistance, float InMaxDrawDistance, const FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	float MaxDrawDistanceScale = GetCachedScalabilityCVars().ViewDistanceScale;
	float FadeRadius = GDisableLODFade ? 0.0f : GDistanceFadeMaxTravel;
	float MaxDrawDistance = InMaxDrawDistance * MaxDrawDistanceScale;

	// If cull distance is disabled, always show (except foliage)
	if (Family->EngineShowFlags.DistanceCulledPrimitives
		&& !PrimitiveSceneInfo->Proxy->IsDetailMesh())
	{
		return false;
	}

	// The primitive is always culled if it exceeds the max fade distance.
	if (DistanceSquared > FMath::Square(MaxDrawDistance + FadeRadius) ||
		DistanceSquared < FMath::Square(MinDrawDistance))
	{
		return true;
	}

	const bool bDistanceCulled = (DistanceSquared > FMath::Square(MaxDrawDistance));
	const bool bMayBeFading = (DistanceSquared > FMath::Square(MaxDrawDistance - FadeRadius));

	bool bStillFading = false;
	if( !GDisableLODFade && bMayBeFading && State != NULL && !bDisableDistanceBasedFadeTransitions )
	{
		// Update distance-based visibility and fading state if it has not already been updated.
		int32 PrimitiveIndex = PrimitiveSceneInfo->GetIndex();
		FRelativeBitReference PrimitiveBit(PrimitiveIndex);
		if (PotentiallyFadingPrimitiveMap.AccessCorrespondingBit(PrimitiveBit) == false)
		{
			FPrimitiveFadingState& FadingState = ((FSceneViewState*)State)->PrimitiveFadingStates.FindOrAdd(PrimitiveSceneInfo->PrimitiveComponentId);
			UpdatePrimitiveFadingState(FadingState, *this, !bDistanceCulled);
			FUniformBufferRHIParamRef UniformBuffer = FadingState.UniformBuffer;
			bStillFading = (UniformBuffer != NULL);
			PrimitiveFadeUniformBuffers[PrimitiveIndex] = UniformBuffer;
			PotentiallyFadingPrimitiveMap.AccessCorrespondingBit(PrimitiveBit) = true;
		}
	}

	// If we're still fading then make sure the object is still drawn, even if it's beyond the max draw distance
	return ( bDistanceCulled && !bStillFading );
}

static int32 FrustumCullNumWordsPerTask = 128;
static FAutoConsoleVariableRef CVarFrustumCullNumWordsPerTask(
	TEXT("r.FrustumCullNumWordsPerTask"),
	FrustumCullNumWordsPerTask,
	TEXT("Performance tweak. Controls the granularity for the ParallelFor for frustum culling."),
	ECVF_Default
	);

template<bool UseCustomCulling, bool bAlsoUseSphereTest>
static int32 FrustumCull(const FScene* Scene, FViewInfo& View)
{
	SCOPE_CYCLE_COUNTER(STAT_FrustumCull);

	FThreadSafeCounter NumCulledPrimitives;
	float MaxDrawDistanceScale = GetCachedScalabilityCVars().ViewDistanceScale;

	//Primitives per ParallelFor task
	//Using async FrustumCull. Thanks Yager! See https://udn.unrealengine.com/questions/252385/performance-of-frustumcull.html
	//Performance varies on total primitive count and tasks scheduled. Check the mentioned link above for some measurements.
	//There have been some changes as compared to the code measured in the link

	const int32 BitArrayNum = View.PrimitiveVisibilityMap.Num();
	const int32 BitArrayWords = FMath::DivideAndRoundUp(View.PrimitiveVisibilityMap.Num(), (int32)NumBitsPerDWORD);
	const int32 NumTasks = FMath::DivideAndRoundUp(BitArrayWords, FrustumCullNumWordsPerTask);

	ParallelFor(NumTasks, 
		[&NumCulledPrimitives, Scene, &View, MaxDrawDistanceScale](int32 TaskIndex)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FrustumCull_Loop);
			const int32 BitArrayNumInner = View.PrimitiveVisibilityMap.Num();
			FVector ViewOriginForDistanceCulling = View.ViewMatrices.GetViewOrigin();
			float FadeRadius = GDisableLODFade ? 0.0f : GDistanceFadeMaxTravel;
			uint8 CustomVisibilityFlags = EOcclusionFlags::CanBeOccluded | EOcclusionFlags::HasPrecomputedVisibility;

			// Primitives may be explicitly removed from stereo views when using mono
			const bool UseMonoCulling = View.Family->IsMonoscopicFarFieldEnabled() && (View.StereoPass == eSSP_LEFT_EYE || View.StereoPass == eSSP_RIGHT_EYE);

			const int32 TaskWordOffset = TaskIndex * FrustumCullNumWordsPerTask;

			for (int32 WordIndex = TaskWordOffset; WordIndex < TaskWordOffset + FrustumCullNumWordsPerTask && WordIndex * NumBitsPerDWORD < BitArrayNumInner; WordIndex++)
			{
				uint32 Mask = 0x1;
				uint32 VisBits = 0;
				uint32 FadingBits = 0;
				for (int32 BitSubIndex = 0; BitSubIndex < NumBitsPerDWORD && WordIndex * NumBitsPerDWORD + BitSubIndex < BitArrayNumInner; BitSubIndex++, Mask <<= 1)
				{
					int32 Index = WordIndex * NumBitsPerDWORD + BitSubIndex;
					const FPrimitiveBounds& Bounds = Scene->PrimitiveBounds[Index];
					float DistanceSquared = (Bounds.BoxSphereBounds.Origin - ViewOriginForDistanceCulling).SizeSquared();
					float MaxDrawDistance = Bounds.MaxDrawDistance * MaxDrawDistanceScale;
					int32 VisibilityId = INDEX_NONE;

					if (UseCustomCulling &&
						((Scene->PrimitiveOcclusionFlags[Index] & CustomVisibilityFlags) == CustomVisibilityFlags))
					{
						VisibilityId = Scene->PrimitiveVisibilityIds[Index].ByteIndex;
					}

					// If cull distance is disabled, always show (except foliage)
					if (View.Family->EngineShowFlags.DistanceCulledPrimitives
						&& !Scene->Primitives[Index]->Proxy->IsDetailMesh())
					{
						MaxDrawDistance = FLT_MAX;
					}

					// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
					if (View.bIsVxgiVoxelization)
					{
						bool bIsVisible = false;

						if (View.VxgiClipmapBounds.GetBox().Intersect(Bounds.BoxSphereBounds.GetBox()))
						{
							bIsVisible = true;
						}

						if (!bIsVisible)
						{
							STAT(NumCulledPrimitives.Increment());
						}
						else
						{
							// The primitive is visible!
							VisBits |= Mask;
						}
					}
					else
					{
#endif
						// NVCHANGE_END: Add VXGI

						// The primitive is always culled if it exceeds the max fade distance or lay outside the view frustum.
					if (DistanceSquared > FMath::Square(MaxDrawDistance + FadeRadius) ||
						(DistanceSquared < Bounds.MinDrawDistanceSq) ||
						(UseCustomCulling && !View.CustomVisibilityQuery->IsVisible(VisibilityId, FBoxSphereBounds(Bounds.BoxSphereBounds.Origin, Bounds.BoxSphereBounds.BoxExtent, Bounds.BoxSphereBounds.SphereRadius))) ||
						(bAlsoUseSphereTest && View.ViewFrustum.IntersectSphere(Bounds.BoxSphereBounds.Origin, Bounds.BoxSphereBounds.SphereRadius) == false) ||
						View.ViewFrustum.IntersectBox(Bounds.BoxSphereBounds.Origin, Bounds.BoxSphereBounds.BoxExtent) == false ||
						(UseMonoCulling && Scene->Primitives[Index]->Proxy->RenderInMono()))
					{
						STAT(NumCulledPrimitives.Increment());
					}
					else
					{
						if (DistanceSquared > FMath::Square(MaxDrawDistance))
						{
							FadingBits |= Mask;
						}
						else
						{
							// The primitive is visible!
							VisBits |= Mask;
							if (DistanceSquared > FMath::Square(MaxDrawDistance - FadeRadius))
							{
								FadingBits |= Mask;
							}
						}
					}

					// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
					}
#endif
					// NVCHANGE_END: Add VXGI
				}
				if (FadingBits)
				{
					check(!View.PotentiallyFadingPrimitiveMap.GetData()[WordIndex]); // this should start at zero
					View.PotentiallyFadingPrimitiveMap.GetData()[WordIndex] = FadingBits;
				}
				if (VisBits)
				{
					check(!View.PrimitiveVisibilityMap.GetData()[WordIndex]); // this should start at zero
					View.PrimitiveVisibilityMap.GetData()[WordIndex] = VisBits;
				}
			}
		},
		!FApp::ShouldUseThreadingForPerformance() || (UseCustomCulling && !View.CustomVisibilityQuery->IsThreadsafe()) || CVarParallelInitViews.GetValueOnRenderThread() == 0
	);

	return NumCulledPrimitives.GetValue();
}

/**
 * Updated primitive fading states for the view.
 */
static void UpdatePrimitiveFading(const FScene* Scene, FViewInfo& View)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdatePrimitiveFading);

	FSceneViewState* ViewState = (FSceneViewState*)View.State;

	if (ViewState)
	{
		uint32 PrevFrameNumber = ViewState->PrevFrameNumber;
		float CurrentRealTime = View.Family->CurrentRealTime;

		// First clear any stale fading states.
		for (FPrimitiveFadingStateMap::TIterator It(ViewState->PrimitiveFadingStates); It; ++It)
		{
			FPrimitiveFadingState& FadingState = It.Value();
			if (FadingState.FrameNumber != PrevFrameNumber ||
				(IsValidRef(FadingState.UniformBuffer) && CurrentRealTime >= FadingState.EndTime))
			{
				It.RemoveCurrent();
			}
		}

		// Should we allow fading transitions at all this frame?  For frames where the camera moved
		// a large distance or where we haven't rendered a view in awhile, it's best to disable
		// fading so users don't see unexpected object transitions.
		if (!GDisableLODFade && !View.bDisableDistanceBasedFadeTransitions)
		{
			// Do a pass over potentially fading primitives and update their states.
			for (FSceneSetBitIterator BitIt(View.PotentiallyFadingPrimitiveMap); BitIt; ++BitIt)
			{
				bool bVisible = View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt);
				FPrimitiveFadingState& FadingState = ViewState->PrimitiveFadingStates.FindOrAdd(Scene->PrimitiveComponentIds[BitIt.GetIndex()]);
				UpdatePrimitiveFadingState(FadingState, View, bVisible);
				FUniformBufferRHIParamRef UniformBuffer = FadingState.UniformBuffer;
				if (UniformBuffer && !bVisible)
				{
					// If the primitive is fading out make sure it remains visible.
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = true;
				}
				View.PrimitiveFadeUniformBuffers[BitIt.GetIndex()] = UniformBuffer;
			}
		}
	}
}

struct FOcclusionBounds
{
	FOcclusionBounds(FPrimitiveOcclusionHistory* InPrimitiveOcclusionHistory, const FVector& InBoundsOrigin, const FVector& InBoundsExtent, bool bInGroupedQuery)
		: PrimitiveOcclusionHistory(InPrimitiveOcclusionHistory)
		, BoundsOrigin(InBoundsOrigin)
		, BoundsExtent(InBoundsExtent)
		, bGroupedQuery(bInGroupedQuery)
	{}
	FPrimitiveOcclusionHistory* PrimitiveOcclusionHistory;
	FVector BoundsOrigin;
	FVector BoundsExtent;	
	bool bGroupedQuery;
};

struct FHZBBound
{
	FHZBBound(FPrimitiveOcclusionHistory* InTargetHistory, const FVector& InBoundsOrigin, const FVector& InBoundsExtent)
	: TargetHistory(InTargetHistory)
	, BoundsOrigin(InBoundsOrigin)
	, BoundsExtent(InBoundsExtent)
	{}

	FPrimitiveOcclusionHistory* TargetHistory;
	FVector BoundsOrigin;
	FVector BoundsExtent;
};

#define BALANCE_LOAD 1
#define QUERY_SANITY_CHECK 0

struct FVisForPrimParams
{
	FVisForPrimParams(){}

	FVisForPrimParams(const FScene* InScene, 
						FViewInfo* InView, 
						FViewElementPDI* InOcclusionPDI, 						
						const int32 InStartIndex, 
						const int32 InNumToProcess, 
						const bool bInSubmitQueries, 
						const bool bInHZBOcclusion,						
						TArray<FPrimitiveOcclusionHistory>* OutOcclusionHistory,
						TArray<FPrimitiveOcclusionHistory*>* OutQueriesToRelease,
						TArray<FHZBBound>* OutHZBBounds,
						TArray<FOcclusionBounds>* OutQueriesToRun,
						TArray<bool>* OutSubIsOccluded)
		: Scene(InScene)
		, View(InView)
		, OcclusionPDI(InOcclusionPDI)		
		, StartIndex(InStartIndex)
		, NumToProcess(InNumToProcess)
		, bSubmitQueries(bInSubmitQueries)
		, bHZBOcclusion(bInHZBOcclusion)		
		, InsertPrimitiveOcclusionHistory(OutOcclusionHistory)
		, QueriesToRelease(OutQueriesToRelease)
		, HZBBoundsToAdd(OutHZBBounds)
		, QueriesToAdd(OutQueriesToRun)	
		, SubIsOccluded(OutSubIsOccluded)
	{

	}

	void Init(	const FScene* InScene,
				FViewInfo* InView,
				FViewElementPDI* InOcclusionPDI,
				const int32 InStartIndex,
				const int32 InNumToProcess,
				const bool bInSubmitQueries,
				const bool bInHZBOcclusion,				
				TArray<FPrimitiveOcclusionHistory>* OutOcclusionHistory,
				TArray<FPrimitiveOcclusionHistory*>* OutQueriesToRelease,
				TArray<FHZBBound>* OutHZBBounds,
				TArray<FOcclusionBounds>* OutQueriesToRun,
				TArray<bool>* OutSubIsOccluded)
			
	{
		Scene = InScene;
		View = InView;
		OcclusionPDI = InOcclusionPDI;
		StartIndex = InStartIndex;
		NumToProcess = InNumToProcess;
		bSubmitQueries = bInSubmitQueries;
		bHZBOcclusion = bInHZBOcclusion;
		InsertPrimitiveOcclusionHistory = OutOcclusionHistory;
		QueriesToRelease = OutQueriesToRelease;
		HZBBoundsToAdd = OutHZBBounds;
		QueriesToAdd = OutQueriesToRun;
		SubIsOccluded = OutSubIsOccluded;
	}

	const FScene* Scene;
	FViewInfo* View;
	FViewElementPDI* OcclusionPDI;
	int32 StartIndex;
	int32 NumToProcess;
	bool bSubmitQueries;
	bool bHZBOcclusion;	

	//occlusion history to insert into.  In parallel these will be all merged back into the view's history on the main thread.
	//use TChunkedArray so pointers to the new FPrimitiveOcclusionHistory's won't change if the array grows.	
	TArray<FPrimitiveOcclusionHistory>*		InsertPrimitiveOcclusionHistory;
	TArray<FPrimitiveOcclusionHistory*>*	QueriesToRelease;
	TArray<FHZBBound>*						HZBBoundsToAdd;
	TArray<FOcclusionBounds>*				QueriesToAdd;
	int32									NumOccludedPrims;
	TArray<bool>*							SubIsOccluded;
};

//This function is shared between the single and multi-threaded versions.  Modifications to any primitives indexed by BitIt should be ok
//since only one of the task threads will ever reference it.  However, any modifications to shared state like the ViewState must be buffered
//to be recombined later.
template<bool bSingleThreaded>
static void FetchVisibilityForPrimitives_Range(FVisForPrimParams& Params)
{	
	int32 NumOccludedPrimitives = 0;
	
	const FScene* Scene				= Params.Scene;
	FViewInfo& View					= *Params.View;
	FViewElementPDI* OcclusionPDI	= Params.OcclusionPDI;
	const int32 StartIndex			= Params.StartIndex;
	const int32 NumToProcess		= Params.NumToProcess;
	const bool bSubmitQueries		= Params.bSubmitQueries;
	const bool bHZBOcclusion		= Params.bHZBOcclusion;

	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	const int32 NumBufferedFrames = FOcclusionQueryHelpers::GetNumBufferedFrames();
	const bool bClearQueries = !View.Family->EngineShowFlags.HitProxies;
	const float CurrentRealTime = View.Family->CurrentRealTime;
	uint32 OcclusionFrameCounter = ViewState->OcclusionFrameCounter;
	FRenderQueryPool& OcclusionQueryPool = ViewState->OcclusionQueryPool;
	FHZBOcclusionTester& HZBOcclusionTests = ViewState->HZBOcclusionTests;
	

	TSet<FPrimitiveOcclusionHistory, FPrimitiveOcclusionHistoryKeyFuncs>& ViewPrimitiveOcclusionHistory = ViewState->PrimitiveOcclusionHistorySet;
	TArray<FPrimitiveOcclusionHistory>* InsertPrimitiveOcclusionHistory = Params.InsertPrimitiveOcclusionHistory;
	TArray<FPrimitiveOcclusionHistory*>* QueriesToRelease = Params.QueriesToRelease;
	TArray<FHZBBound>* HZBBoundsToAdd = Params.HZBBoundsToAdd;
	TArray<FOcclusionBounds>* QueriesToAdd = Params.QueriesToAdd;	

	const int32 ReserveAmount = NumToProcess;
	if (!bSingleThreaded)
	{		
		check(InsertPrimitiveOcclusionHistory);
		check(QueriesToRelease);
		check(HZBBoundsToAdd);
		check(QueriesToAdd);

		//avoid doing reallocs as much as possible.  Unlikely to make an entry per processed element.		
		InsertPrimitiveOcclusionHistory->Reserve(ReserveAmount);
		QueriesToRelease->Reserve(ReserveAmount);
		HZBBoundsToAdd->Reserve(ReserveAmount);
		QueriesToAdd->Reserve(ReserveAmount);
	}
	
	int32 NumProcessed = 0;
	int32 NumTotalPrims = View.PrimitiveVisibilityMap.Num();
	int32 NumTotalDefUnoccluded = View.PrimitiveDefinitelyUnoccludedMap.Num();

	//if we are load balanced then we iterate only the set bits, and the ranges have been pre-selected to evenly distribute set bits among the tasks with no overlaps.
	//if not, then the entire array is evenly divided by range.
#if BALANCE_LOAD
	for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap, StartIndex); BitIt && (NumProcessed < NumToProcess); ++BitIt, ++NumProcessed)
#else
	for (TBitArray<SceneRenderingBitArrayAllocator>::FIterator BitIt(View.PrimitiveVisibilityMap, StartIndex); BitIt && (NumProcessed < NumToProcess); ++BitIt, ++NumProcessed)
#endif
	{		
		uint8 OcclusionFlags = Scene->PrimitiveOcclusionFlags[BitIt.GetIndex()];
		bool bCanBeOccluded = (OcclusionFlags & EOcclusionFlags::CanBeOccluded) != 0;

#if !BALANCE_LOAD		
		if (!View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt))
		{
			continue;
		}
#endif

		//we can't allow the prim history insertion array to realloc or it will invalidate pointers in the other output arrays.
		const bool bCanAllocPrimHistory = bSingleThreaded || InsertPrimitiveOcclusionHistory->Num() < InsertPrimitiveOcclusionHistory->Max();		

		if (GIsEditor)
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->Primitives[BitIt.GetIndex()];

			if (PrimitiveSceneInfo->Proxy->IsSelected())
			{
				// to render occluded outline for selected objects
				bCanBeOccluded = false;
			}
		}
		int32 NumSubQueries = 1;
		bool bSubQueries = false;
		const TArray<FBoxSphereBounds>* SubBounds = nullptr;

		check(Params.SubIsOccluded);
		TArray<bool>& SubIsOccluded = *Params.SubIsOccluded;
		int32 SubIsOccludedStart = SubIsOccluded.Num();
		if ((OcclusionFlags & EOcclusionFlags::HasSubprimitiveQueries) && GAllowSubPrimitiveQueries && !View.bDisableQuerySubmissions)
		{
			FPrimitiveSceneProxy* Proxy = Scene->Primitives[BitIt.GetIndex()]->Proxy;
			SubBounds = Proxy->GetOcclusionQueries(&View);
			NumSubQueries = SubBounds->Num();
			bSubQueries = true;
			if (!NumSubQueries)
			{
				View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
				continue;
			}
			SubIsOccluded.Reserve(NumSubQueries);
		}

		bool bAllSubOcclusionStateIsDefinite = true;
		bool bAllSubOccluded = true;
		FPrimitiveComponentId PrimitiveId = Scene->PrimitiveComponentIds[BitIt.GetIndex()];

		for (int32 SubQuery = 0; SubQuery < NumSubQueries; SubQuery++)
		{
			FPrimitiveOcclusionHistory* PrimitiveOcclusionHistory = ViewPrimitiveOcclusionHistory.Find(FPrimitiveOcclusionHistoryKey(PrimitiveId, SubQuery));
			
			bool bIsOccluded = false;
			bool bOcclusionStateIsDefinite = false;			

			if (!PrimitiveOcclusionHistory)
			{
				// If the primitive doesn't have an occlusion history yet, create it.
				if (bSingleThreaded)
				{					
					// In singlethreaded mode we can safely modify the view's history directly.
					PrimitiveOcclusionHistory = &ViewPrimitiveOcclusionHistory[
						ViewPrimitiveOcclusionHistory.Add(FPrimitiveOcclusionHistory(PrimitiveId, SubQuery))
					];
				}
				else if (bCanAllocPrimHistory)
				{
					// In multithreaded mode we have to buffer the new histories and add them to the view during a post-combine
					PrimitiveOcclusionHistory = &(*InsertPrimitiveOcclusionHistory)[
						InsertPrimitiveOcclusionHistory->Add(FPrimitiveOcclusionHistory(PrimitiveId, SubQuery))
					];
				}				
				
				// If the primitive hasn't been visible recently enough to have a history, treat it as unoccluded this frame so it will be rendered as an occluder and its true occlusion state can be determined.
				// already set bIsOccluded = false;

				// Flag the primitive's occlusion state as indefinite, which will force it to be queried this frame.
				// The exception is if the primitive isn't occludable, in which case we know that it's definitely unoccluded.
				bOcclusionStateIsDefinite = bCanBeOccluded ? false : true;
			}
			else
			{
				if (View.bIgnoreExistingQueries)
				{
					// If the view is ignoring occlusion queries, the primitive is definitely unoccluded.
					// already set bIsOccluded = false;
					bOcclusionStateIsDefinite = View.bDisableQuerySubmissions;
				}
				else if (bCanBeOccluded)
				{
					if (bHZBOcclusion)
					{
						if (HZBOcclusionTests.IsValidFrame(PrimitiveOcclusionHistory->HZBTestFrameNumber))
						{
							bIsOccluded = !HZBOcclusionTests.IsVisible(PrimitiveOcclusionHistory->HZBTestIndex);
							bOcclusionStateIsDefinite = true;
						}
					}
					else
					{
						// Read the occlusion query results.
						uint64 NumSamples = 0;
						FRenderQueryRHIRef& PastQuery = PrimitiveOcclusionHistory->GetPastQuery(OcclusionFrameCounter, NumBufferedFrames);
						if (IsValidRef(PastQuery))
						{
							//int32 RefCount = PastQuery.GetReference()->GetRefCount();
							// NOTE: RHIGetOcclusionQueryResult should never fail when using a blocking call, rendering artifacts may show up.
							//if (RHICmdList.GetRenderQueryResult(PastQuery, NumSamples, true))
							if (GDynamicRHI->RHIGetRenderQueryResult(PastQuery.GetReference(), NumSamples, true))
							{
								// we render occlusion without MSAA
								uint32 NumPixels = (uint32)NumSamples;

								// The primitive is occluded if none of its bounding box's pixels were visible in the previous frame's occlusion query.
								bIsOccluded = (NumPixels == 0);

								
								if (!bIsOccluded)
								{
									checkSlow(View.OneOverNumPossiblePixels > 0.0f);
									PrimitiveOcclusionHistory->LastPixelsPercentage = NumPixels * View.OneOverNumPossiblePixels;
								}
								else
								{
									PrimitiveOcclusionHistory->LastPixelsPercentage = 0.0f;
								}								


								// Flag the primitive's occlusion state as definite if it wasn't grouped.
								bOcclusionStateIsDefinite = !PrimitiveOcclusionHistory->bGroupedQuery;
							}
							else
							{
								// If the occlusion query failed, treat the primitive as visible.  
								// already set bIsOccluded = false;
							}
							//checkf(RefCount == PastQuery.GetReference()->GetRefCount(), TEXT("Ref count on prim: %i, old: %i, new: %i"), PrimitiveOcclusionHistory->PrimitiveId.PrimIDValue, RefCount, PastQuery.GetReference()->GetRefCount());
						}
						else
						{
							// If there's no occlusion query for the primitive, set it's visibility state to whether it has been unoccluded recently.
							bIsOccluded = (PrimitiveOcclusionHistory->LastVisibleTime + GEngine->PrimitiveProbablyVisibleTime < CurrentRealTime);

							
							
							if (bIsOccluded)
							{
								PrimitiveOcclusionHistory->LastPixelsPercentage = 0.0f;
							}
							else
							{
								PrimitiveOcclusionHistory->LastPixelsPercentage = GEngine->MaxOcclusionPixelsFraction;
							}							

							// the state was definite last frame, otherwise we would have ran a query
							bOcclusionStateIsDefinite = true;							
						}
					}

					if (GVisualizeOccludedPrimitives && OcclusionPDI && bIsOccluded)
					{
						const FBoxSphereBounds& Bounds = bSubQueries ? (*SubBounds)[SubQuery] : Scene->PrimitiveOcclusionBounds[BitIt.GetIndex()];
						DrawWireBox(OcclusionPDI, Bounds.GetBox(), FColor(50, 255, 50), SDPG_Foreground);
					}
				}
				else
				{					
					// Primitives that aren't occludable are considered definitely unoccluded.
					// already set bIsOccluded = false;
					bOcclusionStateIsDefinite = true;
				}

				if (bClearQueries)
				{					
					if (bSingleThreaded)
					{						
						OcclusionQueryPool.ReleaseQuery(PrimitiveOcclusionHistory->GetPastQuery(OcclusionFrameCounter, NumBufferedFrames));
					}
					else
					{
						FRenderQueryRHIRef &Query = PrimitiveOcclusionHistory->GetPastQuery(OcclusionFrameCounter, NumBufferedFrames);
						if (IsValidRef(Query))
						{
							check(Query.GetRefCount() > 0);
							QueriesToRelease->Add(PrimitiveOcclusionHistory);							
						}
					}
				}
			}

			if (PrimitiveOcclusionHistory)
			{

				// Set the primitive's considered time to keep its occlusion history from being trimmed.
				PrimitiveOcclusionHistory->LastConsideredTime = CurrentRealTime;

				if (bSubmitQueries && bCanBeOccluded)
				{
					bool bAllowBoundsTest;
					const FBoxSphereBounds& OcclusionBounds = bSubQueries ? (*SubBounds)[SubQuery] : Scene->PrimitiveOcclusionBounds[BitIt.GetIndex()];
					if (View.bHasNearClippingPlane)
					{
						bAllowBoundsTest = View.NearClippingPlane.PlaneDot(OcclusionBounds.Origin) <
							-(FVector::BoxPushOut(View.NearClippingPlane, OcclusionBounds.BoxExtent));

					}
					else if (!View.IsPerspectiveProjection())
					{
						// Transform parallel near plane
						static_assert((int32)ERHIZBuffer::IsInverted != 0, "Check equation for culling!");
						bAllowBoundsTest = View.WorldToScreen(OcclusionBounds.Origin).Z - View.ViewMatrices.GetProjectionMatrix().M[2][2] * OcclusionBounds.SphereRadius < 1;
					}
					else
					{
						bAllowBoundsTest = OcclusionBounds.SphereRadius < HALF_WORLD_MAX;
					}

					if (bAllowBoundsTest)
					{
						if (bHZBOcclusion)
						{
							// Always run
							if (bSingleThreaded)
							{								
								PrimitiveOcclusionHistory->HZBTestIndex = HZBOcclusionTests.AddBounds(OcclusionBounds.Origin, OcclusionBounds.BoxExtent);
							}
							else
							{
								HZBBoundsToAdd->Emplace(PrimitiveOcclusionHistory, OcclusionBounds.Origin, OcclusionBounds.BoxExtent);
							}
							PrimitiveOcclusionHistory->HZBTestFrameNumber = OcclusionFrameCounter;
						}
						else
						{
							// decide if a query should be run this frame
							bool bRunQuery, bGroupedQuery;

							if (!bSubQueries && // sub queries are never grouped, we assume the custom code knows what it is doing and will group internally if it wants
								(OcclusionFlags & EOcclusionFlags::AllowApproximateOcclusion))
							{
								if (bIsOccluded)
								{
									// Primitives that were occluded the previous frame use grouped queries.
									bGroupedQuery = true;
									bRunQuery = true;
								}
								else if (bOcclusionStateIsDefinite)
								{
									// If the primitive's is definitely unoccluded, only requery it occasionally.
									float FractionMultiplier = FMath::Max(PrimitiveOcclusionHistory->LastPixelsPercentage / GEngine->MaxOcclusionPixelsFraction, 1.0f);
									bRunQuery = (FractionMultiplier * GOcclusionRandomStream.GetFraction()) < GEngine->MaxOcclusionPixelsFraction;
									bGroupedQuery = false;
								}
								else
								{
									bGroupedQuery = false;
									bRunQuery = true;
								}
							}
							else
							{
								// Primitives that need precise occlusion results use individual queries.
								bGroupedQuery = false;
								bRunQuery = true;
							}

							if (bRunQuery)
							{
								const FVector BoundOrigin = OcclusionBounds.Origin + View.ViewMatrices.GetPreViewTranslation();
								const FVector BoundExtent = OcclusionBounds.BoxExtent;

								if (bSingleThreaded)
								{
									PrimitiveOcclusionHistory->SetCurrentQuery(OcclusionFrameCounter,
										bGroupedQuery ?
										View.GroupedOcclusionQueries.BatchPrimitive(BoundOrigin, BoundExtent) :
										View.IndividualOcclusionQueries.BatchPrimitive(BoundOrigin, BoundExtent),
										NumBufferedFrames
										);
								}
								else
								{
									QueriesToAdd->Emplace(PrimitiveOcclusionHistory, BoundOrigin, BoundExtent, bGroupedQuery);
								}
							}

							PrimitiveOcclusionHistory->bGroupedQuery = bGroupedQuery;
						}
					}
					else
					{
						// If the primitive's bounding box intersects the near clipping plane, treat it as definitely unoccluded.
						bIsOccluded = false;
						bOcclusionStateIsDefinite = true;
					}
				}
			}


			if (bSubQueries)
			{
				if (!View.bIgnoreExistingQueries)
				{
					SubIsOccluded.Add(bIsOccluded);
					if (!bIsOccluded)
					{
						bAllSubOccluded = false;
						if (bOcclusionStateIsDefinite)
						{
							if (PrimitiveOcclusionHistory)
							{
								PrimitiveOcclusionHistory->LastVisibleTime = CurrentRealTime;
							}
						}
					}
				}

				if (bIsOccluded || !bOcclusionStateIsDefinite)
				{
					bAllSubOcclusionStateIsDefinite = false;
				}
			}
			else
			{
					
				if (bIsOccluded)
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
					STAT(NumOccludedPrimitives++);
				}
				else if (bOcclusionStateIsDefinite)
				{
					if (PrimitiveOcclusionHistory)
					{
						PrimitiveOcclusionHistory->LastVisibleTime = CurrentRealTime;
					}
					
					View.PrimitiveDefinitelyUnoccludedMap.AccessCorrespondingBit(BitIt) = true;
				}					
			}			
		}

		if (bSubQueries)
		{
			if (SubIsOccluded.Num() > 0)
			{
				FPrimitiveSceneProxy* Proxy = Scene->Primitives[BitIt.GetIndex()]->Proxy;
				Proxy->AcceptOcclusionResults(&View, &SubIsOccluded, SubIsOccludedStart, SubIsOccluded.Num() - SubIsOccludedStart);
			}

			if (bAllSubOccluded)
			{
				View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
				STAT(NumOccludedPrimitives++);
			}
			else if (bAllSubOcclusionStateIsDefinite)
			{
				View.PrimitiveDefinitelyUnoccludedMap.AccessCorrespondingBit(BitIt) = true;
			}
		}
	}

	check(NumTotalDefUnoccluded == View.PrimitiveDefinitelyUnoccludedMap.Num());
	check(NumTotalPrims == View.PrimitiveVisibilityMap.Num());
	check(!InsertPrimitiveOcclusionHistory || InsertPrimitiveOcclusionHistory->Num() <= ReserveAmount);
	Params.NumOccludedPrims = NumOccludedPrimitives;	
}

FAutoConsoleTaskPriority CPrio_FetchVisibilityForPrimitivesTask(
	TEXT("TaskGraph.TaskPriorities.FetchVisibilityForPrimitivesTask"),
	TEXT("Task and thread priority for FetchVisibilityForPrimitivesTask."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);

class FetchVisibilityForPrimitivesTask
{
	FVisForPrimParams& Params;

public:

	FetchVisibilityForPrimitivesTask(FVisForPrimParams& InParams)
		: Params(InParams)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FetchVisibilityForPrimitivesTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_FetchVisibilityForPrimitivesTask.Get();
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FetchVisibilityForPrimitives_Range<false>(Params);
	}
};

static int32 FetchVisibilityForPrimitives(const FScene* Scene, FViewInfo& View, const bool bSubmitQueries, const bool bHZBOcclusion)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FetchVisibilityForPrimitives);
	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	
	static int32 SubIsOccludedArrayIndex = 0;
	SubIsOccludedArrayIndex = 1 - SubIsOccludedArrayIndex;

	if (GOcclusionCullParallelPrimFetch && GSupportsParallelOcclusionQueries)
	{		
		static const int32 MaxNumCullTasks = 4;
		static const int32 ActualNumCullTasks = 4;
		static const int32 NumOutputArrays = MaxNumCullTasks;
		
		FGraphEventRef TaskRefArray[NumOutputArrays];

		//params for each task
		FVisForPrimParams Params[NumOutputArrays];

		//output arrays for each task
		TArray<FPrimitiveOcclusionHistory> OutputOcclusionHistory[NumOutputArrays];
		TArray<FPrimitiveOcclusionHistory*> OutQueriesToRelease[NumOutputArrays];
		TArray<FHZBBound> OutHZBBounds[NumOutputArrays];
		TArray<FOcclusionBounds> OutQueriesToRun[NumOutputArrays];	

		static TArray<bool> FrameSubIsOccluded[NumOutputArrays][FSceneView::NumBufferedSubIsOccludedArrays];

		//optionally balance the tasks by how the visible primitives are distributed in the array rather than just breaking up the array by range.
		//should make the tasks more equal length.
#if BALANCE_LOAD
		int32 StartIndices[NumOutputArrays] = { 0 };
		int32 ProcessRange[NumOutputArrays] = { 0 };
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FetchVisibilityForPrimitivesPreProcess);
			int32 NumBitsSet = 0;
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt, ++NumBitsSet)
			{
			}
			
			int32 BitsPerTask = NumBitsSet / ActualNumCullTasks;
			int32 NumBitsForRange = 0;
			int32 CurrentStartIndex = 0;
			int32 RangeToSet = 0;

			//accumulate set bits for each task until we reach the target, then set the start/end and move on.
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt && RangeToSet < (ActualNumCullTasks - 1); ++BitIt)
			{
				++NumBitsForRange;
				if (NumBitsForRange == BitsPerTask)
				{
					StartIndices[RangeToSet] = CurrentStartIndex;
					ProcessRange[RangeToSet] = NumBitsForRange;

					++RangeToSet;
					NumBitsForRange = 0;
					CurrentStartIndex = BitIt.GetIndex() + 1;
				}
			}

			//final range is the rest of the set bits, no matter how many there are.
			StartIndices[ActualNumCullTasks - 1] = CurrentStartIndex;
			ProcessRange[ActualNumCullTasks - 1] = NumBitsSet - (BitsPerTask * 3);
		}
#endif

		const int32 NumPrims = View.PrimitiveVisibilityMap.Num();
		const int32 NumPerTask = NumPrims / ActualNumCullTasks;
		int32 StartIndex = 0;
		int32 NumToProcess = NumPerTask;

		FGraphEventArray TaskWaitArray;
		int32 NumTasks = 0;
		for (int32 i = 0; i < ActualNumCullTasks && (StartIndex < NumPrims); ++i, ++NumTasks)
		{
			NumToProcess = (i == (ActualNumCullTasks - 1)) ? (NumPrims - StartIndex) : NumPerTask;
			TArray<bool>& SubIsOccluded = FrameSubIsOccluded[i][SubIsOccludedArrayIndex];
			SubIsOccluded.Reset();

			Params[i].Init(
				Scene,
				&View,
				nullptr,
#if BALANCE_LOAD
				StartIndices[i],
				ProcessRange[i],
#else
				StartIndex,
				NumToProcess,
#endif
				bSubmitQueries,
				bHZBOcclusion,				
				&OutputOcclusionHistory[i],
				&OutQueriesToRelease[i],
				&OutHZBBounds[i],
				&OutQueriesToRun[i],
				&SubIsOccluded
				);

			TaskRefArray[i] = TGraphTask<FetchVisibilityForPrimitivesTask>::CreateTask().ConstructAndDispatchWhenReady(Params[i]);			
			TaskWaitArray.Add(TaskRefArray[i]);

			StartIndex += NumToProcess;
		}

		const int32 NumBufferedFrames = FOcclusionQueryHelpers::GetNumBufferedFrames();
		uint32 OcclusionFrameCounter = ViewState->OcclusionFrameCounter;
		TSet<FPrimitiveOcclusionHistory, FPrimitiveOcclusionHistoryKeyFuncs>& ViewPrimitiveOcclusionHistory = ViewState->PrimitiveOcclusionHistorySet;
		FRenderQueryPool& OcclusionQueryPool = ViewState->OcclusionQueryPool;
		FHZBOcclusionTester& HZBOcclusionTests = ViewState->HZBOcclusionTests;		

		int32 NumOccludedPrims = 0;
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FetchVisibilityForPrimitivesCombine);

			//wait for them all so we don't start modifying the prim histories while the gather is running
			FTaskGraphInterface::Get().WaitUntilTasksComplete(TaskWaitArray, ENamedThreads::RenderThread_Local);			

#if QUERY_SANITY_CHECK
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FetchVisibilityForPrimitivesSanity);
				TSet<int32> ReleaseQuerySet;
				TSet<int32> RunQuerySet;
				TSet<int32> MasterPrimsProcessed;
				for (int32 i = 0; i < NumTasks; ++i)
				{
					bool bAlreadyIn = false;
					for (auto ReleaseQueryIter = OutQueriesToRelease[i].CreateIterator(); ReleaseQueryIter; ++ReleaseQueryIter)
					{
						FPrimitiveOcclusionHistory* History = *ReleaseQueryIter;
						ReleaseQuerySet.Add(History->PrimitiveId.PrimIDValue, &bAlreadyIn);
						checkf(!bAlreadyIn, TEXT("Prim: %i double released query."), History->PrimitiveId.PrimIDValue);
					}

					for (auto RunQueriesIter = OutQueriesToRun[i].CreateIterator(); RunQueriesIter; ++RunQueriesIter)
					{
						FPrimitiveOcclusionHistory* History = RunQueriesIter->PrimitiveOcclusionHistory;
						RunQuerySet.Add(History->PrimitiveId.PrimIDValue, &bAlreadyIn);
						checkf(!bAlreadyIn, TEXT("Prim: %i double run query."), History->PrimitiveId.PrimIDValue);
					}					
				}
			}
#endif

			//Add/Release query ops use stored PrimitiveHistory pointers. We must do ALL of these from all tasks before adding any new PrimitiveHistories to the view.
			//Adding new histories to the view could cause the array to resize which would invalidate all the stored output pointers for the other operations.
			for (int32 i = 0; i < NumTasks; ++i)
			{
				//HZB output
				for (auto HZBBoundIter = OutHZBBounds[i].CreateIterator(); HZBBoundIter; ++HZBBoundIter)
				{
					HZBBoundIter->TargetHistory->HZBTestIndex = HZBOcclusionTests.AddBounds(HZBBoundIter->BoundsOrigin, HZBBoundIter->BoundsExtent);
				}

				//Manual query release handling
				for (auto ReleaseQueryIter = OutQueriesToRelease[i].CreateIterator(); ReleaseQueryIter; ++ReleaseQueryIter)
				{
					FPrimitiveOcclusionHistory* History = *ReleaseQueryIter;
					OcclusionQueryPool.ReleaseQuery(History->GetPastQuery(OcclusionFrameCounter, NumBufferedFrames));
				}
				
				//New query batching
				for (auto RunQueriesIter = OutQueriesToRun[i].CreateIterator(); RunQueriesIter; ++RunQueriesIter)
				{
					RunQueriesIter->PrimitiveOcclusionHistory->SetCurrentQuery(OcclusionFrameCounter,
						RunQueriesIter->bGroupedQuery ?
						View.GroupedOcclusionQueries.BatchPrimitive(RunQueriesIter->BoundsOrigin, RunQueriesIter->BoundsExtent) :
						View.IndividualOcclusionQueries.BatchPrimitive(RunQueriesIter->BoundsOrigin, RunQueriesIter->BoundsExtent),
						NumBufferedFrames
						);
				}
			}

			//now add new primitive histories to the view. may resize the view's array.
			for (int32 i = 0; i < NumTasks; ++i)
			{								
				const TArray<FPrimitiveOcclusionHistory>& NewHistoryArray = OutputOcclusionHistory[i];				
				for (int32 HistoryIndex = 0; HistoryIndex < NewHistoryArray.Num(); ++HistoryIndex)
				{
					const FPrimitiveOcclusionHistory& CopySourceHistory = NewHistoryArray[HistoryIndex];
					ViewPrimitiveOcclusionHistory.Add(CopySourceHistory);
				}

				//accumulate occluded prims across tasks
				NumOccludedPrims += Params[i].NumOccludedPrims;
			}
		}
		
		return NumOccludedPrims;
	}
	else
	{
		//SubIsOccluded stuff needs a frame's lifetime
		TArray<bool>& SubIsOccluded = View.FrameSubIsOccluded[SubIsOccludedArrayIndex];
		SubIsOccluded.Reset();

		FViewElementPDI OcclusionPDI(&View, NULL);
		int32 StartIndex = 0;
		int32 NumToProcess = View.PrimitiveVisibilityMap.Num();				
		FVisForPrimParams Params(
			Scene,
			&View,
			&OcclusionPDI,
			StartIndex,
			NumToProcess,
			bSubmitQueries,
			bHZBOcclusion,			
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			&SubIsOccluded
			);

		FetchVisibilityForPrimitives_Range<true>(Params);
		return Params.NumOccludedPrims;
	}
}

/**
 * Cull occluded primitives in the view.
 */
static int32 OcclusionCull(FRHICommandListImmediate& RHICmdList, const FScene* Scene, FViewInfo& View)
{
	SCOPE_CYCLE_COUNTER(STAT_OcclusionCull);	

	// INITVIEWS_TODO: This could be more efficient if broken up in to separate concerns:
	// - What is occluded?
	// - For which primitives should we render occlusion queries?
	// - Generate occlusion query geometry.

	int32 NumOccludedPrimitives = 0;
	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	
	// Disable HZB on OpenGL platforms to avoid rendering artefacts
	// It can be forced on by setting HZBOcclusion to 2
	bool bHZBOcclusion = (!IsOpenGLPlatform(GShaderPlatformForFeatureLevel[Scene->GetFeatureLevel()]) && GHZBOcclusion) || (GHZBOcclusion == 2);

	// Use precomputed visibility data if it is available.
	if (View.PrecomputedVisibilityData)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_LookupPrecomputedVisibility);

		FViewElementPDI OcclusionPDI(&View, NULL);
		uint8 PrecomputedVisibilityFlags = EOcclusionFlags::CanBeOccluded | EOcclusionFlags::HasPrecomputedVisibility;
		for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
		{
			if ((Scene->PrimitiveOcclusionFlags[BitIt.GetIndex()] & PrecomputedVisibilityFlags) == PrecomputedVisibilityFlags)
			{
				FPrimitiveVisibilityId VisibilityId = Scene->PrimitiveVisibilityIds[BitIt.GetIndex()];
				if ((View.PrecomputedVisibilityData[VisibilityId.ByteIndex] & VisibilityId.BitMask) == 0)
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
					INC_DWORD_STAT_BY(STAT_StaticallyOccludedPrimitives,1);
					STAT(NumOccludedPrimitives++);

					if (GVisualizeOccludedPrimitives)
					{
						const FBoxSphereBounds& Bounds = Scene->PrimitiveOcclusionBounds[BitIt.GetIndex()];
						DrawWireBox(&OcclusionPDI, Bounds.GetBox(), FColor(100, 50, 50), SDPG_Foreground);
					}
				}
			}
		}
	}

	float CurrentRealTime = View.Family->CurrentRealTime;
	if (ViewState)
	{
		if (Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM4)
		{
			bool bSubmitQueries = !View.bDisableQuerySubmissions;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			bSubmitQueries = bSubmitQueries && !ViewState->HasViewParent() && !ViewState->bIsFrozen;
#endif

			if( bHZBOcclusion )
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_MapHZBResults);
				check(!ViewState->HZBOcclusionTests.IsValidFrame(ViewState->OcclusionFrameCounter));
				ViewState->HZBOcclusionTests.MapResults(RHICmdList);
			}
			
			NumOccludedPrimitives += FetchVisibilityForPrimitives(Scene, View, bSubmitQueries, bHZBOcclusion);			

			if( bHZBOcclusion )
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_HZBUnmapResults);

				ViewState->HZBOcclusionTests.UnmapResults(RHICmdList);

				if( bSubmitQueries )
				{
					ViewState->HZBOcclusionTests.SetValidFrameNumber(ViewState->OcclusionFrameCounter);
				}
			}
		}
		else
		{
			// No occlusion queries, so mark primitives as not occluded
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				View.PrimitiveDefinitelyUnoccludedMap.AccessCorrespondingBit(BitIt) = true;
			}
		}
	}

	return NumOccludedPrimitives;
}

template<class T, int TAmplifyFactor = 1>
struct FRelevancePrimSet
{
	enum
	{
		MaxInputPrims = 127, //like 128, but we leave space for NumPrims
		MaxOutputPrims = MaxInputPrims * TAmplifyFactor
	};
	int32 NumPrims;

	T Prims[MaxOutputPrims];

	FORCEINLINE FRelevancePrimSet()
		: NumPrims(0)
	{
		//FMemory::Memzero(Prims, sizeof(T) * GetMaxOutputPrim());
	}
	FORCEINLINE void AddPrim(T Prim)
	{
		checkSlow(NumPrims < MaxOutputPrims);
		Prims[NumPrims++] = Prim;
	}
	FORCEINLINE bool IsFull() const
	{
		return NumPrims >= MaxOutputPrims;
	}
	template<class TARRAY>
	FORCEINLINE void AppendTo(TARRAY& DestArray)
	{
		DestArray.Append(Prims, NumPrims);
	}
};

struct FMarkRelevantStaticMeshesForViewData
{
	FVector ViewOrigin;
	float MaxDrawDistanceScaleSquared;
	int32 ForcedLODLevel;
	float LODScale;
	float InvLODScale;
	float MinScreenRadiusForCSMDepthSquared;
	float MinScreenRadiusForDepthPrepassSquared;
	bool bFullEarlyZPass;

	FMarkRelevantStaticMeshesForViewData(FViewInfo& View)
	{
		ViewOrigin = View.ViewMatrices.GetViewOrigin();

		MaxDrawDistanceScaleSquared = GetCachedScalabilityCVars().ViewDistanceScaleSquared;

		// outside of the loop to be more efficient
		ForcedLODLevel = (View.Family->EngineShowFlags.LOD) ? GetCVarForceLOD() : 0;

		LODScale = CVarStaticMeshLODDistanceScale.GetValueOnRenderThread() * View.LODDistanceFactor;
		InvLODScale = 1.0f / LODScale;

		MinScreenRadiusForCSMDepthSquared = GMinScreenRadiusForCSMDepth * GMinScreenRadiusForCSMDepth;
		MinScreenRadiusForDepthPrepassSquared = GMinScreenRadiusForDepthPrepass * GMinScreenRadiusForDepthPrepass;

		extern bool ShouldForceFullDepthPass(ERHIFeatureLevel::Type FeatureLevel);
		bFullEarlyZPass = ShouldForceFullDepthPass(View.GetFeatureLevel());
	}
};

namespace EMarkMaskBits
{
	enum Type
	{
		StaticMeshShadowDepthMapMask = 0x1,
		StaticMeshVisibilityMapMask = 0x2,
		StaticMeshVelocityMapMask = 0x4,
		StaticMeshOccluderMapMask = 0x8,
		StaticMeshFadeOutDitheredLODMapMask = 0x10,
		StaticMeshFadeInDitheredLODMapMask = 0x20,
		StaticMeshEditorSelectedMask = 0x40,
	};
}

struct FRelevancePacket
{
	const float CurrentWorldTime;
	const float DeltaWorldTime;

	FRHICommandListImmediate& RHICmdList;
	const FScene* Scene;
	const FViewInfo& View;
	const uint8 ViewBit;
	const FMarkRelevantStaticMeshesForViewData& ViewData;
	FPrimitiveViewMasks& OutHasDynamicMeshElementsMasks;
	FPrimitiveViewMasks& OutHasDynamicEditorMeshElementsMasks;
	uint8* RESTRICT MarkMasks;

	FRelevancePrimSet<int32> Input;
	FRelevancePrimSet<int32> RelevantStaticPrimitives;
	FRelevancePrimSet<int32> NotDrawRelevant;
	FRelevancePrimSet<FPrimitiveSceneInfo*> VisibleDynamicPrimitives;
	FRelevancePrimSet<FTranslucentPrimSet::FTranslucentSortedPrim, ETranslucencyPass::TPT_MAX> TranslucencyPrims;
	// belongs to TranslucencyPrims
	FTranslucenyPrimCount TranslucencyPrimCount;
	FRelevancePrimSet<FPrimitiveSceneProxy*> DistortionPrimSet;
	FRelevancePrimSet<FMeshDecalPrimSet::KeyType> MeshDecalPrimSet;
	FRelevancePrimSet<FPrimitiveSceneProxy*> CustomDepthSet;
	FRelevancePrimSet<FPrimitiveSceneInfo*> LazyUpdatePrimitives;
	FRelevancePrimSet<FPrimitiveSceneInfo*> DirtyPrecomputedLightingBufferPrimitives;
	FRelevancePrimSet<FPrimitiveSceneInfo*> VisibleEditorPrimitives;
	FRelevancePrimSet<FPrimitiveSceneProxy*> VolumetricPrimSet;
	uint16 CombinedShadingModelMask;
	bool bUsesGlobalDistanceField;
	bool bUsesLightingChannels;
	bool bTranslucentSurfaceLighting;
	bool bUsesSceneDepth;

	FRelevancePacket(
		FRHICommandListImmediate& InRHICmdList,
		const FScene* InScene, 
		const FViewInfo& InView, 
		uint8 InViewBit,
		const FMarkRelevantStaticMeshesForViewData& InViewData,
		FPrimitiveViewMasks& InOutHasDynamicMeshElementsMasks,
		FPrimitiveViewMasks& InOutHasDynamicEditorMeshElementsMasks,
		uint8* InMarkMasks)

		: CurrentWorldTime(InView.Family->CurrentWorldTime)
		, DeltaWorldTime(InView.Family->DeltaWorldTime)
		, RHICmdList(InRHICmdList)
		, Scene(InScene)
		, View(InView)
		, ViewBit(InViewBit)
		, ViewData(InViewData)
		, OutHasDynamicMeshElementsMasks(InOutHasDynamicMeshElementsMasks)
		, OutHasDynamicEditorMeshElementsMasks(InOutHasDynamicEditorMeshElementsMasks)
		, MarkMasks(InMarkMasks)
		, CombinedShadingModelMask(0)
		, bUsesGlobalDistanceField(false)
		, bUsesLightingChannels(false)
		, bTranslucentSurfaceLighting(false)
		, bUsesSceneDepth(false)
	{
	}

	void AnyThreadTask()
	{
		ComputeRelevance();
		MarkRelevant();
	}

	void ComputeRelevance()
	{
		CombinedShadingModelMask = 0;
		bUsesGlobalDistanceField = false;
		bUsesLightingChannels = false;
		bTranslucentSurfaceLighting = false;

		SCOPE_CYCLE_COUNTER(STAT_ComputeViewRelevance);
		for (int32 Index = 0; Index < Input.NumPrims; Index++)
		{
			int32 BitIndex = Input.Prims[Index];
			FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->Primitives[BitIndex];
			FPrimitiveViewRelevance& ViewRelevance = const_cast<FPrimitiveViewRelevance&>(View.PrimitiveViewRelevanceMap[BitIndex]);
			ViewRelevance = PrimitiveSceneInfo->Proxy->GetViewRelevance(&View);
			ViewRelevance.bInitializedThisFrame = true;

			const bool bStaticRelevance = ViewRelevance.bStaticRelevance;
			const bool bDrawRelevance = ViewRelevance.bDrawRelevance;
			const bool bDynamicRelevance = ViewRelevance.bDynamicRelevance;
			const bool bShadowRelevance = ViewRelevance.bShadowRelevance;
			const bool bEditorRelevance = ViewRelevance.bEditorPrimitiveRelevance;
			const bool bEditorSelectionRelevance = ViewRelevance.bEditorStaticSelectionRelevance;
			const bool bTranslucentRelevance = ViewRelevance.HasTranslucency();


			// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
			if (View.bIsVxgiVoxelization && bTranslucentRelevance)
			{
				NotDrawRelevant.AddPrim(BitIndex);
				continue;
			}
#endif
			// NVCHANGE_END: Add VXGI

			if (View.bIsReflectionCapture && !PrimitiveSceneInfo->Proxy->IsVisibleInReflectionCaptures())
			{
				NotDrawRelevant.AddPrim(BitIndex);
				continue;
			}

			if (bStaticRelevance && (bDrawRelevance || bShadowRelevance))
			{
				RelevantStaticPrimitives.AddPrim(BitIndex);
			}

			if (!bDrawRelevance)
			{
				NotDrawRelevant.AddPrim(BitIndex);
				continue;
			}

			if (ViewRelevance.bDecal)
			{
				MeshDecalPrimSet.AddPrim(FMeshDecalPrimSet::GenerateKey(PrimitiveSceneInfo, PrimitiveSceneInfo->Proxy->GetTranslucencySortPriority()));
			}

			if (bEditorRelevance)
			{
				// Editor primitives are rendered after post processing and composited onto the scene
				VisibleEditorPrimitives.AddPrim(PrimitiveSceneInfo);

				if (GIsEditor)
				{
					OutHasDynamicEditorMeshElementsMasks[BitIndex] |= ViewBit;
				}
			}
			else if(bDynamicRelevance)
			{
				// Keep track of visible dynamic primitives.
				VisibleDynamicPrimitives.AddPrim(PrimitiveSceneInfo);
				OutHasDynamicMeshElementsMasks[BitIndex] |= ViewBit;
			}

			if (bTranslucentRelevance && !bEditorRelevance && ViewRelevance.bRenderInMainPass)
			{
				// Add to set of dynamic translucent primitives
				FTranslucentPrimSet::PlaceScenePrimitive(PrimitiveSceneInfo, View, ViewRelevance, &TranslucencyPrims.Prims[0], TranslucencyPrims.NumPrims, TranslucencyPrimCount);

				if (ViewRelevance.bDistortionRelevance)
				{
					// Add to set of dynamic distortion primitives
					DistortionPrimSet.AddPrim(PrimitiveSceneInfo->Proxy);
				}
			}

			if (ViewRelevance.bHasVolumeMaterialDomain)
			{
				VolumetricPrimSet.AddPrim(PrimitiveSceneInfo->Proxy);
			}

			CombinedShadingModelMask |= ViewRelevance.ShadingModelMaskRelevance;
			bUsesGlobalDistanceField |= ViewRelevance.bUsesGlobalDistanceField;
			bUsesLightingChannels |= ViewRelevance.bUsesLightingChannels;
			bTranslucentSurfaceLighting |= ViewRelevance.bTranslucentSurfaceLighting;
			bUsesSceneDepth |= ViewRelevance.bUsesSceneDepth;

			if (ViewRelevance.bRenderCustomDepth)
			{
				// Add to set of dynamic distortion primitives
				CustomDepthSet.AddPrim(PrimitiveSceneInfo->Proxy);
			}

			// INITVIEWS_TODO: Do this in a separate pass? There are no dependencies
			// here except maybe ParentPrimitives. This could be done in a 
			// low-priority background task and forgotten about.

			// If the primitive's last render time is older than last frame, consider
			// it newly visible and update its visibility change time
			if (PrimitiveSceneInfo->LastRenderTime < CurrentWorldTime - DeltaWorldTime - DELTA)
			{
				PrimitiveSceneInfo->LastVisibilityChangeTime = CurrentWorldTime;
			}
			PrimitiveSceneInfo->LastRenderTime = CurrentWorldTime;

			// If the primitive is definitely unoccluded or if in Wireframe mode and the primitive is estimated
			// to be unoccluded, then update the primitive components's LastRenderTime 
			// on the game thread. This signals that the primitive is visible.
			if (View.PrimitiveDefinitelyUnoccludedMap[BitIndex] || (View.Family->EngineShowFlags.Wireframe && View.PrimitiveVisibilityMap[BitIndex]))
			{
				// Update the PrimitiveComponent's LastRenderTime.
				*(PrimitiveSceneInfo->ComponentLastRenderTime) = CurrentWorldTime;
				*(PrimitiveSceneInfo->ComponentLastRenderTimeOnScreen) = CurrentWorldTime;
			}

			// Cache the nearest reflection proxy if needed
			if (PrimitiveSceneInfo->bNeedsCachedReflectionCaptureUpdate
				// For mobile, the per-object reflection is used for everything
				&& (Scene->GetShadingPath() == EShadingPath::Mobile || bTranslucentRelevance || IsForwardShadingEnabled(Scene->GetFeatureLevel())))
			{
				PrimitiveSceneInfo->CachedReflectionCaptureProxy = Scene->FindClosestReflectionCapture(Scene->PrimitiveBounds[BitIndex].BoxSphereBounds.Origin);
				PrimitiveSceneInfo->CachedPlanarReflectionProxy = Scene->FindClosestPlanarReflection(Scene->PrimitiveBounds[BitIndex].BoxSphereBounds);

				if (Scene->GetShadingPath() == EShadingPath::Mobile)
				{
					// mobile HQ reflections
					Scene->FindClosestReflectionCaptures(Scene->PrimitiveBounds[BitIndex].BoxSphereBounds.Origin, PrimitiveSceneInfo->CachedReflectionCaptureProxies);
				}

				PrimitiveSceneInfo->bNeedsCachedReflectionCaptureUpdate = false;
			}
			if (PrimitiveSceneInfo->NeedsLazyUpdateForRendering())
			{
				LazyUpdatePrimitives.AddPrim(PrimitiveSceneInfo);
			}
			if (PrimitiveSceneInfo->NeedsPrecomputedLightingBufferUpdate())
			{
				DirtyPrecomputedLightingBufferPrimitives.AddPrim(PrimitiveSceneInfo);
			}
		}
	}
	void MarkRelevant()
	{
		SCOPE_CYCLE_COUNTER(STAT_StaticRelevance);

		// using a local counter to reduce memory traffic
		int32 NumVisibleStaticMeshElements = 0;
		FViewInfo& WriteView = const_cast<FViewInfo&>(View);
		FFrozenSceneViewMatricesGuard FrozenMatricesGuard(WriteView);
		const FSceneViewState* ViewState = (FSceneViewState*)View.State;

		const bool bHLODActive = Scene->SceneLODHierarchy.IsActive();

		for (int32 StaticPrimIndex = 0, Num = RelevantStaticPrimitives.NumPrims; StaticPrimIndex < Num; ++StaticPrimIndex)
		{
			int32 PrimitiveIndex = RelevantStaticPrimitives.Prims[StaticPrimIndex];
			const FPrimitiveSceneInfo* RESTRICT PrimitiveSceneInfo = Scene->Primitives[PrimitiveIndex];
			const FPrimitiveBounds& Bounds = Scene->PrimitiveBounds[PrimitiveIndex];
			const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveIndex];

			FLODMask LODToRender = ComputeLODForMeshes( PrimitiveSceneInfo->StaticMeshes, View, Bounds.BoxSphereBounds.Origin, Bounds.BoxSphereBounds.SphereRadius, ViewData.ForcedLODLevel, ViewData.LODScale);
			const bool bIsHLODFading = bHLODActive && ViewState && ViewState->HLODVisibilityState.IsNodeFading(PrimitiveIndex);
			const bool bIsHLODFadingOut = bHLODActive && ViewState && ViewState->HLODVisibilityState.IsNodeFadingOut(PrimitiveIndex);
			const bool bIsLODDithered = LODToRender.IsDithered();

			float DistanceSquared = (Bounds.BoxSphereBounds.Origin - ViewData.ViewOrigin).SizeSquared();
			const float LODFactorDistanceSquared = DistanceSquared * FMath::Square(View.LODDistanceFactor * ViewData.InvLODScale);
			const bool bDrawShadowDepth = FMath::Square(Bounds.BoxSphereBounds.SphereRadius) > ViewData.MinScreenRadiusForCSMDepthSquared * LODFactorDistanceSquared;
			const bool bDrawDepthOnly = ViewData.bFullEarlyZPass || FMath::Square(Bounds.BoxSphereBounds.SphereRadius) > GMinScreenRadiusForDepthPrepass * GMinScreenRadiusForDepthPrepass * LODFactorDistanceSquared;

			const int32 NumStaticMeshes = PrimitiveSceneInfo->StaticMeshes.Num();
			for(int32 MeshIndex = 0;MeshIndex < NumStaticMeshes;MeshIndex++)
			{
				const FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[MeshIndex];
				if (LODToRender.ContainsLOD(StaticMesh.LODIndex))
				{
					uint8 MarkMask = 0;
					bool bNeedsBatchVisibility = false;
					bool bHiddenByHLODFade = false; // Hide mesh LOD levels that HLOD is substituting

					if (bIsHLODFading)
					{
						if (bIsHLODFadingOut)
						{
							if (bIsLODDithered && LODToRender.DitheredLODIndices[1] == StaticMesh.LODIndex)
							{
								bHiddenByHLODFade = true;
							}
							else
							{
								MarkMask |= EMarkMaskBits::StaticMeshFadeOutDitheredLODMapMask;	
							}
						}
						else
						{
							if (bIsLODDithered && LODToRender.DitheredLODIndices[0] == StaticMesh.LODIndex)
							{
								bHiddenByHLODFade = true;
							}
							else
							{
								MarkMask |= EMarkMaskBits::StaticMeshFadeInDitheredLODMapMask;
							}
						}
					}
					else if (bIsLODDithered)
					{
						if (LODToRender.DitheredLODIndices[0] == StaticMesh.LODIndex)
						{
							MarkMask |= EMarkMaskBits::StaticMeshFadeOutDitheredLODMapMask;
						}
						else
						{
							MarkMask |= EMarkMaskBits::StaticMeshFadeInDitheredLODMapMask;
						}
					}

					if (ViewRelevance.bShadowRelevance && bDrawShadowDepth && StaticMesh.CastShadow)
					{
						// Mark static mesh as visible in shadows.
						MarkMask |= EMarkMaskBits::StaticMeshShadowDepthMapMask;
						bNeedsBatchVisibility = true;
					}

					if(ViewRelevance.bDrawRelevance && (StaticMesh.bUseForMaterial || StaticMesh.bUseAsOccluder) && (ViewRelevance.bRenderInMainPass || ViewRelevance.bRenderCustomDepth) && !bHiddenByHLODFade)
					{
						// Mark static mesh as visible for rendering
						if (StaticMesh.bUseForMaterial)
						{
							MarkMask |= EMarkMaskBits::StaticMeshVisibilityMapMask;
							if (PrimitiveSceneInfo->ShouldRenderVelocity(View, false))
							{
								MarkMask |= EMarkMaskBits::StaticMeshVelocityMapMask;
							}
							++NumVisibleStaticMeshElements;
						}

						// If the static mesh is an occluder, check whether it covers enough of the screen to be used as an occluder.
						if(	StaticMesh.bUseAsOccluder && bDrawDepthOnly )
						{
							MarkMask |= EMarkMaskBits::StaticMeshOccluderMapMask;
						}
						bNeedsBatchVisibility = true;
					}

#if WITH_EDITOR
					if(ViewRelevance.bDrawRelevance && ViewRelevance.bEditorStaticSelectionRelevance)
					{
						MarkMask |= EMarkMaskBits::StaticMeshEditorSelectedMask;
					}
#endif
					if (MarkMask)
					{
						MarkMasks[StaticMesh.Id] = MarkMask;
					}

					// Static meshes which don't need per-element visibility always draw all elements
					if (bNeedsBatchVisibility && StaticMesh.bRequiresPerElementVisibility)
					{
						WriteView.StaticMeshBatchVisibility[StaticMesh.BatchVisibilityId] = StaticMesh.VertexFactory->GetStaticBatchElementVisibility(View, &StaticMesh);
					}
				}
			}
		}
		static_assert(sizeof(WriteView.NumVisibleStaticMeshElements) == sizeof(int32), "Atomic is the wrong size");
		FPlatformAtomics::InterlockedAdd((volatile int32*)&WriteView.NumVisibleStaticMeshElements, NumVisibleStaticMeshElements);
	}

	void RenderThreadFinalize()
	{
		FViewInfo& WriteView = const_cast<FViewInfo&>(View);
		
		for (int32 Index = 0; Index < NotDrawRelevant.NumPrims; Index++)
		{
			WriteView.PrimitiveVisibilityMap[NotDrawRelevant.Prims[Index]] = false;
		}

		WriteView.ShadingModelMaskInView |= CombinedShadingModelMask;
		WriteView.bUsesGlobalDistanceField |= bUsesGlobalDistanceField;
		WriteView.bUsesLightingChannels |= bUsesLightingChannels;
		WriteView.bTranslucentSurfaceLighting |= bTranslucentSurfaceLighting;
		WriteView.bUsesSceneDepth |= bUsesSceneDepth;
		VisibleEditorPrimitives.AppendTo(WriteView.VisibleEditorPrimitives);
		VisibleDynamicPrimitives.AppendTo(WriteView.VisibleDynamicPrimitives);
		WriteView.TranslucentPrimSet.AppendScenePrimitives(TranslucencyPrims.Prims, TranslucencyPrims.NumPrims, TranslucencyPrimCount);
		DistortionPrimSet.AppendTo(WriteView.DistortionPrimSet);
		MeshDecalPrimSet.AppendTo(WriteView.MeshDecalPrimSet.Prims);
		CustomDepthSet.AppendTo(WriteView.CustomDepthSet);
		DirtyPrecomputedLightingBufferPrimitives.AppendTo(WriteView.DirtyPrecomputedLightingBufferPrimitives);
		VolumetricPrimSet.AppendTo(WriteView.VolumetricPrimSet);
		for (int32 Index = 0; Index < LazyUpdatePrimitives.NumPrims; Index++)
		{
			LazyUpdatePrimitives.Prims[Index]->ConditionalLazyUpdateForRendering(RHICmdList);
		}
	}
};

static void ComputeAndMarkRelevanceForViewParallel(
	FRHICommandListImmediate& RHICmdList,
	const FScene* Scene,
	FViewInfo& View,
	uint8 ViewBit,
	FPrimitiveViewMasks& OutHasDynamicMeshElementsMasks,
	FPrimitiveViewMasks& OutHasDynamicEditorMeshElementsMasks
	)
{
	check(OutHasDynamicMeshElementsMasks.Num() == Scene->Primitives.Num());

	const FMarkRelevantStaticMeshesForViewData ViewData(View);

	int32 NumMesh = View.StaticMeshVisibilityMap.Num();
	check(View.StaticMeshShadowDepthMap.Num() == NumMesh && View.StaticMeshVelocityMap.Num() == NumMesh && View.StaticMeshOccluderMap.Num() == NumMesh);
	uint8* RESTRICT MarkMasks = (uint8*)FMemStack::Get().Alloc(NumMesh + 31 , 8); // some padding to simplify the high speed transpose
	FMemory::Memzero(MarkMasks, NumMesh + 31);

	int32 EstimateOfNumPackets = NumMesh / (FRelevancePrimSet<int32>::MaxInputPrims * 4);

	TArray<FRelevancePacket*,SceneRenderingAllocator> Packets;

	Packets.Reserve(EstimateOfNumPackets);

	{
		FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap);
		if (BitIt)
		{

			FRelevancePacket* Packet = new(FMemStack::Get()) FRelevancePacket(
				RHICmdList,
				Scene, 
				View, 
				ViewBit,
				ViewData,
				OutHasDynamicMeshElementsMasks,
				OutHasDynamicEditorMeshElementsMasks,
				MarkMasks);
			Packets.Add(Packet);

			while (1)
			{
				Packet->Input.AddPrim(BitIt.GetIndex());
				++BitIt;
				if (Packet->Input.IsFull() || !BitIt)
				{
					if (!BitIt)
					{
						break;
					}
					else
					{
						Packet = new(FMemStack::Get()) FRelevancePacket(
							RHICmdList,
							Scene, 
							View, 
							ViewBit,
							ViewData,
							OutHasDynamicMeshElementsMasks,
							OutHasDynamicEditorMeshElementsMasks,
							MarkMasks);
						Packets.Add(Packet);
					}
				}
			}
		}
	}
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ComputeAndMarkRelevanceForViewParallel_ParallelFor);
		ParallelFor(Packets.Num(), 
			[&Packets](int32 Index)
			{
				Packets[Index]->AnyThreadTask();
			},
			!(FApp::ShouldUseThreadingForPerformance() && CVarParallelInitViews.GetValueOnRenderThread() > 0)
		);
	}
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ComputeAndMarkRelevanceForViewParallel_RenderThreadFinalize);
		for (auto Packet : Packets)
		{
			Packet->RenderThreadFinalize();
		}
	}
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ComputeAndMarkRelevanceForViewParallel_TransposeMeshBits);
	check(View.StaticMeshVelocityMap.Num() == NumMesh && 
		View.StaticMeshShadowDepthMap.Num() == NumMesh && 
		View.StaticMeshVisibilityMap.Num() == NumMesh && 
		View.StaticMeshOccluderMap.Num() == NumMesh &&
		View.StaticMeshFadeOutDitheredLODMap.Num() == NumMesh && 
		View.StaticMeshFadeInDitheredLODMap.Num() == NumMesh
		);
	uint32* RESTRICT StaticMeshVisibilityMap_Words = View.StaticMeshVisibilityMap.GetData();
	uint32* RESTRICT StaticMeshVelocityMap_Words = View.StaticMeshVelocityMap.GetData();
	uint32* RESTRICT StaticMeshShadowDepthMap_Words = View.StaticMeshShadowDepthMap.GetData();
	uint32* RESTRICT StaticMeshOccluderMap_Words = View.StaticMeshOccluderMap.GetData();
	uint32* RESTRICT StaticMeshFadeOutDitheredLODMap_Words = View.StaticMeshFadeOutDitheredLODMap.GetData();
	uint32* RESTRICT StaticMeshFadeInDitheredLODMap_Words = View.StaticMeshFadeInDitheredLODMap.GetData();
#if WITH_EDITOR
	uint32* RESTRICT StaticMeshEditorSelectionMap_Words = View.StaticMeshEditorSelectionMap.GetData();
#endif
	const uint64* RESTRICT MarkMasks64 = (const uint64* RESTRICT)MarkMasks;
	const uint8* RESTRICT MarkMasks8 = MarkMasks;
	for (int32 BaseIndex = 0; BaseIndex < NumMesh; BaseIndex += 32)
	{
		uint32 StaticMeshVisibilityMap_Word = 0;
		uint32 StaticMeshVelocityMap_Word = 0;
		uint32 StaticMeshShadowDepthMap_Word = 0;
		uint32 StaticMeshOccluderMap_Word = 0;
		uint32 StaticMeshFadeOutDitheredLODMap_Word = 0;
		uint32 StaticMeshFadeInDitheredLODMap_Word = 0;
		uint32 StaticMeshEditorSelectionMap_Word = 0;
		uint32 Mask = 1;
		bool bAny = false;
		for (int32 QWordIndex = 0; QWordIndex < 4; QWordIndex++)
		{
			if (*MarkMasks64++)
			{
				for (int32 ByteIndex = 0; ByteIndex < 8; ByteIndex++, Mask <<= 1, MarkMasks8++)
				{
					uint8 MaskMask = *MarkMasks8;
					StaticMeshVisibilityMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshVisibilityMapMask) ? Mask : 0;
					StaticMeshVelocityMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshVelocityMapMask) ? Mask : 0;
					StaticMeshShadowDepthMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshShadowDepthMapMask) ? Mask : 0;
					StaticMeshOccluderMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshOccluderMapMask) ? Mask : 0;
					StaticMeshFadeOutDitheredLODMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshFadeOutDitheredLODMapMask) ? Mask : 0;
					StaticMeshFadeInDitheredLODMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshFadeInDitheredLODMapMask) ? Mask : 0;
#if WITH_EDITOR
					StaticMeshEditorSelectionMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshEditorSelectedMask) ? Mask : 0;
#endif
				}
				bAny = true;
			}
			else
			{
				MarkMasks8 += 8;
				Mask <<= 8;
			}
		}
		if (bAny)
		{
			checkSlow(!*StaticMeshVisibilityMap_Words && !*StaticMeshVelocityMap_Words && !*StaticMeshShadowDepthMap_Words && !*StaticMeshOccluderMap_Words && !*StaticMeshFadeOutDitheredLODMap_Words && !*StaticMeshFadeInDitheredLODMap_Words);
			*StaticMeshVisibilityMap_Words = StaticMeshVisibilityMap_Word;
			*StaticMeshVelocityMap_Words = StaticMeshVelocityMap_Word;
			*StaticMeshShadowDepthMap_Words = StaticMeshShadowDepthMap_Word;
			*StaticMeshOccluderMap_Words = StaticMeshOccluderMap_Word;
			*StaticMeshFadeOutDitheredLODMap_Words = StaticMeshFadeOutDitheredLODMap_Word;
			*StaticMeshFadeInDitheredLODMap_Words = StaticMeshFadeInDitheredLODMap_Word;
#if WITH_EDITOR
			*StaticMeshEditorSelectionMap_Words = StaticMeshEditorSelectionMap_Word;
#endif
		}
		StaticMeshVisibilityMap_Words++;
		StaticMeshVelocityMap_Words++;
		StaticMeshShadowDepthMap_Words++;
		StaticMeshOccluderMap_Words++;
		StaticMeshFadeOutDitheredLODMap_Words++;
		StaticMeshFadeInDitheredLODMap_Words++;
#if WITH_EDITOR
		StaticMeshEditorSelectionMap_Words++;
#endif
	}
}

void FSceneRenderer::GatherDynamicMeshElements(
	TArray<FViewInfo>& InViews, 
	const FScene* InScene, 
	const FSceneViewFamily& InViewFamily, 
	const FPrimitiveViewMasks& HasDynamicMeshElementsMasks, 
	const FPrimitiveViewMasks& HasDynamicEditorMeshElementsMasks, 
	FMeshElementCollector& Collector)
{
	SCOPE_CYCLE_COUNTER(STAT_GetDynamicMeshElements);

	int32 NumPrimitives = InScene->Primitives.Num();
	check(HasDynamicMeshElementsMasks.Num() == NumPrimitives);

	int32 ViewCount = InViews.Num();
	{
		Collector.ClearViewMeshArrays();

		for (int32 ViewIndex = 0; ViewIndex < ViewCount; ViewIndex++)
		{
			Collector.AddViewMeshArrays(&InViews[ViewIndex], &InViews[ViewIndex].DynamicMeshElements, &InViews[ViewIndex].SimpleElementCollector, InViewFamily.GetFeatureLevel());
		}

		// NVCHANGE_BEGIN: Add VXGI
		TArray<const FSceneView*> LocalViews = InViewFamily.Views;
#if WITH_GFSDK_VXGI
		if (VxgiView)
		{
			LocalViews.Add(VxgiView);
			Collector.AddViewMeshArrays(VxgiView, &VxgiView->DynamicMeshElements, &VxgiView->SimpleElementCollector, InViewFamily.GetFeatureLevel());
		}
#endif
		// NVCHANGE_END: Add VXGI

		const bool bIsInstancedStereo = (ViewCount > 0) ? (InViews[0].IsInstancedStereoPass() || InViews[0].bIsMobileMultiViewEnabled) : false;

		for (int32 PrimitiveIndex = 0; PrimitiveIndex < NumPrimitives; ++PrimitiveIndex)
		{
			const uint8 ViewMask = HasDynamicMeshElementsMasks[PrimitiveIndex];

			if (ViewMask != 0)
			{
				// Don't cull a single eye when drawing a stereo pair
				const uint8 ViewMaskFinal = (bIsInstancedStereo) ? ViewMask | 0x3 : ViewMask;

				FPrimitiveSceneInfo* PrimitiveSceneInfo = InScene->Primitives[PrimitiveIndex];
				Collector.SetPrimitive(PrimitiveSceneInfo->Proxy, PrimitiveSceneInfo->DefaultDynamicHitProxyId);
				//PrimitiveSceneInfo->Proxy->GetDynamicMeshElements(InViewFamily.Views, InViewFamily, ViewMaskFinal, Collector);
				// NVCHANGE_BEGIN: Add VXGI
				PrimitiveSceneInfo->Proxy->GetDynamicMeshElements(LocalViews, InViewFamily, ViewMaskFinal, Collector);
				// NVCHANGE_END: Add VXGI
			}

			// to support GetDynamicMeshElementRange()
			for (int32 ViewIndex = 0; ViewIndex < ViewCount; ViewIndex++)
			{
				InViews[ViewIndex].DynamicMeshEndIndices[PrimitiveIndex] = Collector.GetMeshBatchCount(ViewIndex);
			}

			// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
			if (VxgiView)
			{
				VxgiView->DynamicMeshEndIndices[PrimitiveIndex] = Collector.GetMeshBatchCount(ViewCount);
			}
#endif
			// NVCHANGE_END: Add VXGI
		}
	}

	if (GIsEditor)
	{
		Collector.ClearViewMeshArrays();

		for (int32 ViewIndex = 0; ViewIndex < ViewCount; ViewIndex++)
		{
			Collector.AddViewMeshArrays(&InViews[ViewIndex], &InViews[ViewIndex].DynamicEditorMeshElements, &InViews[ViewIndex].EditorSimpleElementCollector, InViewFamily.GetFeatureLevel());
		}

		for (int32 PrimitiveIndex = 0; PrimitiveIndex < NumPrimitives; ++PrimitiveIndex)
		{
			const uint8 ViewMask = HasDynamicEditorMeshElementsMasks[PrimitiveIndex];

			if (ViewMask != 0)
			{
				FPrimitiveSceneInfo* PrimitiveSceneInfo = InScene->Primitives[PrimitiveIndex];
				Collector.SetPrimitive(PrimitiveSceneInfo->Proxy, PrimitiveSceneInfo->DefaultDynamicHitProxyId);
				PrimitiveSceneInfo->Proxy->GetDynamicMeshElements(InViewFamily.Views, InViewFamily, ViewMask, Collector);
			}
		}
	}
	MeshCollector.ProcessTasks();
}

static void MarkAllPrimitivesForReflectionProxyUpdate(FScene* Scene)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_MarkAllPrimitivesForReflectionProxyUpdate);

	if (Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged)
	{
		// Mark all primitives as needing an update
		// Note: Only visible primitives will actually update their reflection proxy
		for (int32 PrimitiveIndex = 0; PrimitiveIndex < Scene->Primitives.Num(); PrimitiveIndex++)
		{
			Scene->Primitives[PrimitiveIndex]->bNeedsCachedReflectionCaptureUpdate = true;
		}

		Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged = false;
	}
}

/**
 * Helper for InitViews to detect large camera movement, in both angle and position.
 */
static bool IsLargeCameraMovement(FSceneView& View, const FMatrix& PrevViewMatrix, const FVector& PrevViewOrigin, float CameraRotationThreshold, float CameraTranslationThreshold)
{
	float RotationThreshold = FMath::Cos(CameraRotationThreshold * PI / 180.0f);
	float ViewRightAngle = View.ViewMatrices.GetViewMatrix().GetColumn(0) | PrevViewMatrix.GetColumn(0);
	float ViewUpAngle = View.ViewMatrices.GetViewMatrix().GetColumn(1) | PrevViewMatrix.GetColumn(1);
	float ViewDirectionAngle = View.ViewMatrices.GetViewMatrix().GetColumn(2) | PrevViewMatrix.GetColumn(2);

	FVector Distance = FVector(View.ViewMatrices.GetViewOrigin()) - PrevViewOrigin;
	return 
		ViewRightAngle < RotationThreshold ||
		ViewUpAngle < RotationThreshold ||
		ViewDirectionAngle < RotationThreshold ||
		Distance.SizeSquared() > CameraTranslationThreshold * CameraTranslationThreshold;
}

float Halton( int32 Index, int32 Base )
{
	float Result = 0.0f;
	float InvBase = 1.0f / Base;
	float Fraction = InvBase;
	while( Index > 0 )
	{
		Result += ( Index % Base ) * Fraction;
		Index /= Base;
		Fraction *= InvBase;
	}
	return Result;
}

void FSceneRenderer::PreVisibilityFrameSetup(FRHICommandListImmediate& RHICmdList)
{
	// Notify the RHI we are beginning to render a scene.
	RHICmdList.BeginScene();

	// Notify the FX system that the scene is about to perform visibility checks.
	if (Scene->FXSystem  && !Views[0].bIsPlanarReflection)
	{
		Scene->FXSystem->PreInitViews();
	}

	// Draw lines to lights affecting this mesh if its selected.
	if (ViewFamily.EngineShowFlags.LightInfluences)
	{
		for (TArray<FPrimitiveSceneInfo*>::TConstIterator It(Scene->Primitives); It; ++It)
		{
			const FPrimitiveSceneInfo* PrimitiveSceneInfo = *It;
			if (PrimitiveSceneInfo->Proxy->IsSelected())
			{
				FLightPrimitiveInteraction *LightList = PrimitiveSceneInfo->LightList;
				while (LightList)
				{
					const FLightSceneInfo* LightSceneInfo = LightList->GetLight();

					bool bDynamic = true;
					bool bRelevant = false;
					bool bLightMapped = true;
					bool bShadowMapped = false;
					PrimitiveSceneInfo->Proxy->GetLightRelevance(LightSceneInfo->Proxy, bDynamic, bRelevant, bLightMapped, bShadowMapped);

					if (bRelevant)
					{
						// Draw blue for light-mapped lights and orange for dynamic lights
						const FColor LineColor = bLightMapped ? FColor(0,140,255) : FColor(255,140,0);
						for (int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
						{
							FViewInfo& View = Views[ViewIndex];
							FViewElementPDI LightInfluencesPDI(&View,NULL);
							LightInfluencesPDI.DrawLine(PrimitiveSceneInfo->Proxy->GetBounds().Origin, LightSceneInfo->Proxy->GetLightToWorld().GetOrigin(), LineColor, SDPG_World);
						}
					}
					LightList = LightList->GetNextLight();
				}
			}
		}
	}

	// Setup motion blur parameters (also check for camera movement thresholds)
	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		FSceneViewState* ViewState = View.ViewState;

		// Once per render increment the occlusion frame counter.
		if (ViewState)
		{
			ViewState->OcclusionFrameCounter++;
		}

		// HighResScreenshot should get best results so we don't do the occlusion optimization based on the former frame
		extern bool GIsHighResScreenshot;
		const bool bIsHitTesting = ViewFamily.EngineShowFlags.HitProxies;
		if (GIsHighResScreenshot || !DoOcclusionQueries(FeatureLevel) || bIsHitTesting)
		{
			View.bDisableQuerySubmissions = true;
			View.bIgnoreExistingQueries = true;
		}
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		// set up the screen area for occlusion
		float NumPossiblePixels = SceneContext.UseDownsizedOcclusionQueries() && IsValidRef(SceneContext.GetSmallDepthSurface()) ? 
			(float)View.ViewRect.Width() / SceneContext.GetSmallColorDepthDownsampleFactor() * (float)View.ViewRect.Height() / SceneContext.GetSmallColorDepthDownsampleFactor() :
			View.ViewRect.Width() * View.ViewRect.Height();
		View.OneOverNumPossiblePixels = NumPossiblePixels > 0.0 ? 1.0f / NumPossiblePixels : 0.0f;

		// Still need no jitter to be set for temporal feedback on SSR (it is enabled even when temporal AA is off).
		View.TemporalJitterPixelsX = 0.0f;
		View.TemporalJitterPixelsY = 0.0f;

		if ( ViewState )
		{
			ViewState->SetupDistanceFieldTemporalOffset(ViewFamily);
		}

		if( View.AntiAliasingMethod == AAM_TemporalAA && ViewState )
		{
			// Subpixel jitter for temporal AA
			int32 TemporalAASamples = CVarTemporalAASamples.GetValueOnRenderThread();
		
			if( TemporalAASamples > 1 && View.bAllowTemporalJitter )
			{
				float SampleX, SampleY;

				if (Scene->GetFeatureLevel() < ERHIFeatureLevel::SM4)
				{
					// Only support 2 samples for mobile temporal AA.
					TemporalAASamples = 2;
				}

				if( TemporalAASamples == 2 )
				{
					#if 0
						// 2xMSAA
						// Pattern docs: http://msdn.microsoft.com/en-us/library/windows/desktop/ff476218(v=vs.85).aspx
						//   N.
						//   .S
						float SamplesX[] = { -4.0f/16.0f, 4.0/16.0f };
						float SamplesY[] = { -4.0f/16.0f, 4.0/16.0f };
					#else
						// This pattern is only used for mobile.
						// Shift to reduce blur.
						float SamplesX[] = { -8.0f/16.0f, 0.0/16.0f };
						float SamplesY[] = { /* - */ 0.0f/16.0f, 8.0/16.0f };
					#endif
					ViewState->OnFrameRenderingSetup(ARRAY_COUNT(SamplesX), ViewFamily);
					uint32 Index = ViewState->GetCurrentTemporalAASampleIndex();
					SampleX = SamplesX[ Index ];
					SampleY = SamplesY[ Index ];
				}
				else if( TemporalAASamples == 3 )
				{
					// 3xMSAA
					//   A..
					//   ..B
					//   .C.
					// Rolling circle pattern (A,B,C).
					float SamplesX[] = { -2.0f/3.0f,  2.0/3.0f,  0.0/3.0f };
					float SamplesY[] = { -2.0f/3.0f,  0.0/3.0f,  2.0/3.0f };
					ViewState->OnFrameRenderingSetup(ARRAY_COUNT(SamplesX), ViewFamily);
					uint32 Index = ViewState->GetCurrentTemporalAASampleIndex();
					SampleX = SamplesX[ Index ];
					SampleY = SamplesY[ Index ];
				}
				else if( TemporalAASamples == 4 )
				{
					// 4xMSAA
					// Pattern docs: http://msdn.microsoft.com/en-us/library/windows/desktop/ff476218(v=vs.85).aspx
					//   .N..
					//   ...E
					//   W...
					//   ..S.
					// Rolling circle pattern (N,E,S,W).
					float SamplesX[] = { -2.0f/16.0f,  6.0/16.0f, 2.0/16.0f, -6.0/16.0f };
					float SamplesY[] = { -6.0f/16.0f, -2.0/16.0f, 6.0/16.0f,  2.0/16.0f };
					ViewState->OnFrameRenderingSetup(ARRAY_COUNT(SamplesX), ViewFamily);
					uint32 Index = ViewState->GetCurrentTemporalAASampleIndex();
					SampleX = SamplesX[ Index ];
					SampleY = SamplesY[ Index ];
				}
				else if( TemporalAASamples == 5 )
				{
					// Compressed 4 sample pattern on same vertical and horizontal line (less temporal flicker).
					// Compressed 1/2 works better than correct 2/3 (reduced temporal flicker).
					//   . N .
					//   W . E
					//   . S .
					// Rolling circle pattern (N,E,S,W).
					float SamplesX[] = {  0.0f/2.0f,  1.0/2.0f,  0.0/2.0f, -1.0/2.0f };
					float SamplesY[] = { -1.0f/2.0f,  0.0/2.0f,  1.0/2.0f,  0.0/2.0f };
					ViewState->OnFrameRenderingSetup(ARRAY_COUNT(SamplesX), ViewFamily);
					uint32 Index = ViewState->GetCurrentTemporalAASampleIndex();
					SampleX = SamplesX[ Index ];
					SampleY = SamplesY[ Index ];
				}
				else
				{
					ViewState->OnFrameRenderingSetup(TemporalAASamples, ViewFamily);
					uint32 Index = ViewState->GetCurrentTemporalAASampleIndex();

					float u1 = Halton( Index + 1, 2 );
					float u2 = Halton( Index + 1, 3 );

					// Generates samples in normal distribution
					// exp( x^2 / Sigma^2 )
					
					static auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TemporalAAFilterSize"));
					float FilterSize = CVar->GetFloat();

					// Scale distribution to set non-unit variance
					// Variance = Sigma^2
					float Sigma = 0.47f * FilterSize;

					// Window to [-0.5, 0.5] output
					// Without windowing we could generate samples far away on the infinite tails.
					float OutWindow = 0.5f;
					float InWindow = FMath::Exp( -0.5 * FMath::Square( OutWindow / Sigma ) );
					
					// Box-Muller transform
					float Theta = 2.0f * PI * u2;
					float r = Sigma * FMath::Sqrt( -2.0f * FMath::Loge( (1.0f - u1) * InWindow + u1 ) );
					
					SampleX = r * FMath::Cos( Theta );
					SampleY = r * FMath::Sin( Theta );
				}

				View.TemporalJitterPixelsX = SampleX;
				View.TemporalJitterPixelsY = SampleY;

				View.ViewMatrices.HackAddTemporalAAProjectionJitter(FVector2D(SampleX * 2.0f / View.ViewRect.Width(), SampleY * -2.0f / View.ViewRect.Height()));
			}
		}
		else if(ViewState)
		{
			// no TemporalAA
			ViewState->OnFrameRenderingSetup(1, ViewFamily);

			ViewState->TemporalAAHistoryRT.SafeRelease();
			ViewState->PendingTemporalAAHistoryRT.SafeRelease();
		}

		if ( ViewState )
		{
			// update previous frame matrices in case world origin was rebased on this frame
			if (!View.OriginOffsetThisFrame.IsZero())
			{
				ViewState->PrevViewMatrices.ApplyWorldOffset(View.OriginOffsetThisFrame);
				ViewState->PendingPrevViewMatrices.ApplyWorldOffset(View.OriginOffsetThisFrame);
			}
			
			// determine if we are initializing or we should reset the persistent state
			const float DeltaTime = View.Family->CurrentRealTime - ViewState->LastRenderTime;
			const bool bFirstFrameOrTimeWasReset = DeltaTime < -0.0001f || ViewState->LastRenderTime < 0.0001f;

			// detect conditions where we should reset occlusion queries
			if (bFirstFrameOrTimeWasReset || 
				ViewState->LastRenderTime + GEngine->PrimitiveProbablyVisibleTime < View.Family->CurrentRealTime ||
				View.bCameraCut ||
				IsLargeCameraMovement(
					View, 
				    ViewState->PrevViewMatrixForOcclusionQuery, 
				    ViewState->PrevViewOriginForOcclusionQuery, 
				    GEngine->CameraRotationThreshold, GEngine->CameraTranslationThreshold))
			{
				View.bIgnoreExistingQueries = true;
				View.bDisableDistanceBasedFadeTransitions = true;
			}
			ViewState->PrevViewMatrixForOcclusionQuery = View.ViewMatrices.GetViewMatrix();
			ViewState->PrevViewOriginForOcclusionQuery = View.ViewMatrices.GetViewOrigin();
				
			// store old view matrix and detect conditions where we should reset motion blur 
			{
				bool bResetCamera = bFirstFrameOrTimeWasReset
					|| View.bCameraCut
					|| IsLargeCameraMovement(View, ViewState->PrevViewMatrices.GetViewMatrix(), ViewState->PrevViewMatrices.GetViewOrigin(), 45.0f, 10000.0f);

				if (bResetCamera)
				{
					ViewState->PrevViewMatrices = View.ViewMatrices;

					ViewState->PendingPrevViewMatrices = ViewState->PrevViewMatrices;

					// PT: If the motion blur shader is the last shader in the post-processing chain then it is the one that is
					//     adjusting for the viewport offset.  So it is always required and we can't just disable the work the
					//     shader does.  The correct fix would be to disable the effect when we don't need it and to properly mark
					//     the uber-postprocessing effect as the last effect in the chain.

					View.bPrevTransformsReset				= true;
				}
				else
				{
					// check for pause so we can keep motion blur in paused mode (doesn't work in editor)
					if(!ViewFamily.bWorldIsPaused)
					{
						ViewState->PrevViewMatrices = ViewState->PendingPrevViewMatrices;
						if( ViewState->PendingTemporalAAHistoryRT.GetRefCount() )
						{
							ViewState->TemporalAAHistoryRT = ViewState->PendingTemporalAAHistoryRT;
							ViewState->PendingTemporalAAHistoryRT.SafeRelease();
						}

						// pending is needed as we are in init view and still need to render.
						ViewState->PendingPrevViewMatrices = View.ViewMatrices;
					}
				}
				// we don't use DeltaTime as it can be 0 (in editor) and is computed by subtracting floats (loses precision over time)
				// Clamp DeltaWorldTime to reasonable values for the purposes of motion blur, things like TimeDilation can make it very small
				if (!ViewFamily.bWorldIsPaused)
				{
					const bool bEnableTimeScale = !ViewState->bSequencerIsPaused;
					const float FixedBlurTimeScale = 2.0f;// 1 / (30 * 1 / 60)

					ViewState->MotionBlurTimeScale = bEnableTimeScale ? (1.0f / (FMath::Max(View.Family->DeltaWorldTime, .00833f) * 30.0f)) : FixedBlurTimeScale;
				}

				View.PrevViewMatrices = ViewState->PrevViewMatrices;
			}

			ViewState->PrevFrameNumber = ViewState->PendingPrevFrameNumber;
			ViewState->PendingPrevFrameNumber = View.Family->FrameNumber;

			// This finishes the update of view state
			ViewState->UpdateLastRenderTime(*View.Family);

			ViewState->UpdateTemporalLODTransition(View);
		}
	}
}

static TAutoConsoleVariable<int32> CVarAlsoUseSphereForFrustumCull(
	TEXT("r.AlsoUseSphereForFrustumCull"),
	0,  
	TEXT("Performance tweak. If > 0, then use a sphere cull before and in addition to a box for frustum culling."),
	ECVF_RenderThreadSafe
	);

void FSceneRenderer::ComputeViewVisibility(FRHICommandListImmediate& RHICmdList)
{
	SCOPE_CYCLE_COUNTER(STAT_ViewVisibilityTime);
	SCOPED_NAMED_EVENT(FSceneRenderer_ComputeViewVisibility, FColor::Magenta);

	STAT(int32 NumProcessedPrimitives = 0);
	STAT(int32 NumCulledPrimitives = 0);
	STAT(int32 NumOccludedPrimitives = 0);

	// Allocate the visible light info.
	if (Scene->Lights.GetMaxIndex() > 0)
	{
		VisibleLightInfos.AddZeroed(Scene->Lights.GetMaxIndex());
	}

	int32 NumPrimitives = Scene->Primitives.Num();
	float CurrentRealTime = ViewFamily.CurrentRealTime;

	FPrimitiveViewMasks HasDynamicMeshElementsMasks;
	HasDynamicMeshElementsMasks.AddZeroed(NumPrimitives);

	FPrimitiveViewMasks HasDynamicEditorMeshElementsMasks;

	if (GIsEditor)
	{
		HasDynamicEditorMeshElementsMasks.AddZeroed(NumPrimitives);
	}

	uint8 ViewBit = 0x1;
	//for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex, ViewBit <<= 1)
	// NVCHANGE_BEGIN: Add VXGI
	for (int32 ViewIndex = 0; ViewIndex < GetNumViewsWithVxgi(); ++ViewIndex, ViewBit <<= 1)
	{
		STAT(NumProcessedPrimitives += NumPrimitives);

		//FViewInfo& View = Views[ViewIndex];
		FViewInfo& View = GetViewWithVxgi(ViewIndex);
		// NVCHANGE_END: Add VXGI
		FSceneViewState* ViewState = (FSceneViewState*)View.State;

		// Allocate the view's visibility maps.
		View.PrimitiveVisibilityMap.Init(false,Scene->Primitives.Num());
		// we don't initialized as we overwrite the whole array (in GatherDynamicMeshElements)
		View.DynamicMeshEndIndices.SetNumUninitialized(Scene->Primitives.Num());
		View.PrimitiveDefinitelyUnoccludedMap.Init(false,Scene->Primitives.Num());
		View.PotentiallyFadingPrimitiveMap.Init(false,Scene->Primitives.Num());
		View.PrimitiveFadeUniformBuffers.AddZeroed(Scene->Primitives.Num());
		View.StaticMeshVisibilityMap.Init(false,Scene->StaticMeshes.GetMaxIndex());
		View.StaticMeshOccluderMap.Init(false,Scene->StaticMeshes.GetMaxIndex());
		View.StaticMeshFadeOutDitheredLODMap.Init(false,Scene->StaticMeshes.GetMaxIndex());
		View.StaticMeshFadeInDitheredLODMap.Init(false,Scene->StaticMeshes.GetMaxIndex());
		View.StaticMeshVelocityMap.Init(false,Scene->StaticMeshes.GetMaxIndex());
		View.StaticMeshShadowDepthMap.Init(false,Scene->StaticMeshes.GetMaxIndex());
		View.StaticMeshBatchVisibility.AddZeroed(Scene->StaticMeshBatchVisibility.GetMaxIndex());

		View.VisibleLightInfos.Empty(Scene->Lights.GetMaxIndex());

#if WITH_EDITOR
		View.StaticMeshEditorSelectionMap.Init(false, Scene->StaticMeshes.GetMaxIndex());
#endif

		// The dirty list allocation must take into account the max possible size because when GILCUpdatePrimTaskEnabled is true,
		// the indirect lighting cache will be update on by threaded job, which can not do reallocs on the buffer (since it uses the SceneRenderingAllocator).
		View.DirtyPrecomputedLightingBufferPrimitives.Reserve(Scene->Primitives.Num());

		for(int32 LightIndex = 0;LightIndex < Scene->Lights.GetMaxIndex();LightIndex++)
		{
			if( LightIndex+2 < Scene->Lights.GetMaxIndex() )
			{
				if (LightIndex > 2)
				{
					FLUSH_CACHE_LINE(&View.VisibleLightInfos(LightIndex-2));
				}
				// @todo optimization These prefetches cause asserts since LightIndex > View.VisibleLightInfos.Num() - 1
				//FPlatformMisc::Prefetch(&View.VisibleLightInfos[LightIndex+2]);
				//FPlatformMisc::Prefetch(&View.VisibleLightInfos[LightIndex+1]);
			}
			new(View.VisibleLightInfos) FVisibleLightViewInfo();
		}

		View.PrimitiveViewRelevanceMap.Empty(Scene->Primitives.Num());
		View.PrimitiveViewRelevanceMap.AddZeroed(Scene->Primitives.Num());

		// If this is the visibility-parent of other views, reset its ParentPrimitives list.
		const bool bIsParent = ViewState && ViewState->IsViewParent();
		if ( bIsParent )
		{
			// PVS-Studio does not understand the validation of ViewState above, so we're disabling
			// its warning that ViewState may be null:
			ViewState->ParentPrimitives.Empty(); //-V595
		}

		if (ViewState)
		{	
			SCOPE_CYCLE_COUNTER(STAT_DecompressPrecomputedOcclusion);
			View.PrecomputedVisibilityData = ViewState->GetPrecomputedVisibilityData(View, Scene);
		}
		else
		{
			View.PrecomputedVisibilityData = NULL;
		}

		if (View.PrecomputedVisibilityData)
		{
			bUsedPrecomputedVisibility = true;
		}

		bool bNeedsFrustumCulling = true;

		// Development builds sometimes override frustum culling, e.g. dependent views in the editor.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if( ViewState )
		{
#if WITH_EDITOR
			// For visibility child views, check if the primitive was visible in the parent view.
			const FSceneViewState* const ViewParent = (FSceneViewState*)ViewState->GetViewParent();
			if(ViewParent)
			{
				bNeedsFrustumCulling = false;
				for (FSceneBitArray::FIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
				{
					if (ViewParent->ParentPrimitives.Contains(Scene->PrimitiveComponentIds[BitIt.GetIndex()]))
					{
						BitIt.GetValue() = true;
					}
				}
			}
#endif
			// For views with frozen visibility, check if the primitive is in the frozen visibility set.
			if(ViewState->bIsFrozen)
			{
				bNeedsFrustumCulling = false;
				for (FSceneBitArray::FIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
				{
					if (ViewState->FrozenPrimitives.Contains(Scene->PrimitiveComponentIds[BitIt.GetIndex()]))
					{
						BitIt.GetValue() = true;
					}
				}
			}
		}
#endif

		// Most views use standard frustum culling.
		if (bNeedsFrustumCulling)
		{
			int32 NumCulledPrimitivesForView;
			if (View.CustomVisibilityQuery && View.CustomVisibilityQuery->Prepare())
			{
				if (CVarAlsoUseSphereForFrustumCull.GetValueOnRenderThread())
				{
					NumCulledPrimitivesForView = FrustumCull<true, true>(Scene, View);
				}
				else
				{
					NumCulledPrimitivesForView = FrustumCull<true, false>(Scene, View);
				}
			}
			else
			{
				if (CVarAlsoUseSphereForFrustumCull.GetValueOnRenderThread())
				{
					NumCulledPrimitivesForView = FrustumCull<false, true>(Scene, View);
				}
				else
				{
					NumCulledPrimitivesForView = FrustumCull<false, false>(Scene, View);
				}
			}
			STAT(NumCulledPrimitives += NumCulledPrimitivesForView);
			UpdatePrimitiveFading(Scene, View);			
		}

		// If any primitives are explicitly hidden, remove them now.
		if (View.HiddenPrimitives.Num())
		{
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				if (View.HiddenPrimitives.Contains(Scene->PrimitiveComponentIds[BitIt.GetIndex()]))
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
				}
			}
		}

		// If the view has any show only primitives, hide everything else
		if (View.ShowOnlyPrimitives.IsSet())
		{
			View.bHasNoVisiblePrimitive = View.ShowOnlyPrimitives->Num() == 0;
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				if (!View.ShowOnlyPrimitives->Contains(Scene->PrimitiveComponentIds[BitIt.GetIndex()]))
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
				}
			}
		}

		if (View.bStaticSceneOnly)
		{
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				// Reflection captures should only capture objects that won't move, since reflection captures won't update at runtime
				if (!Scene->Primitives[BitIt.GetIndex()]->Proxy->HasStaticLighting())
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
				}
			}
		}

		// Cull small objects in wireframe in ortho views
		// This is important for performance in the editor because wireframe disables any kind of occlusion culling
		if (View.Family->EngineShowFlags.Wireframe)
		{
			float ScreenSizeScale = FMath::Max(View.ViewMatrices.GetProjectionMatrix().M[0][0] * View.ViewRect.Width(), View.ViewMatrices.GetProjectionMatrix().M[1][1] * View.ViewRect.Height());
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				if (ScreenSizeScale * Scene->PrimitiveBounds[BitIt.GetIndex()].BoxSphereBounds.SphereRadius <= GWireframeCullThreshold)
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
				}
			}
		}

		// Occlusion cull for all primitives in the view frustum, but not in wireframe.
		if (!View.Family->EngineShowFlags.Wireframe)
		{
			int32 NumOccludedPrimitivesInView = OcclusionCull(RHICmdList, Scene, View);
			STAT(NumOccludedPrimitives += NumOccludedPrimitivesInView);
		}

		// visibility test is done, so now build the hidden flags based on visibility set up
		FLODSceneTree& HLODTree = Scene->SceneLODHierarchy;

		if (HLODTree.IsActive())
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_ViewVisibilityTime_HLOD);
			HLODTree.UpdateAndApplyVisibilityStates(View);
		}

		MarkAllPrimitivesForReflectionProxyUpdate(Scene);
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_ViewVisibilityTime_ConditionalMarkStaticMeshElementsForUpdate);
			Scene->ConditionalMarkStaticMeshElementsForUpdate();
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_ViewRelevance);
			ComputeAndMarkRelevanceForViewParallel(RHICmdList, Scene, View, ViewBit, HasDynamicMeshElementsMasks, HasDynamicEditorMeshElementsMasks);
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Store the primitive for parent occlusion rendering.
		if (FPlatformProperties::SupportsWindowedMode() && ViewState && ViewState->IsViewParent())
		{
			for (FSceneDualSetBitIterator BitIt(View.PrimitiveVisibilityMap, View.PrimitiveDefinitelyUnoccludedMap); BitIt; ++BitIt)
			{
				ViewState->ParentPrimitives.Add(Scene->PrimitiveComponentIds[BitIt.GetIndex()]);
			}
		}
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// if we are freezing the scene, then remember the primitives that are rendered.
		if (ViewState && ViewState->bIsFreezing)
		{
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				ViewState->FrozenPrimitives.Add(Scene->PrimitiveComponentIds[BitIt.GetIndex()]);
			}
		}
#endif

		// TODO: right now decals visibility computed right before rendering them, ideally it should be done in InitViews and this flag should be replaced with list of visible decals  
	    // Currently used to disable stencil operations in forward base pass when scene has no any decals
		View.bSceneHasDecals = (Scene->Decals.Num() > 0);
	}

	GatherDynamicMeshElements(Views, Scene, ViewFamily, HasDynamicMeshElementsMasks, HasDynamicEditorMeshElementsMasks, MeshCollector);

	INC_DWORD_STAT_BY(STAT_ProcessedPrimitives,NumProcessedPrimitives);
	INC_DWORD_STAT_BY(STAT_CulledPrimitives,NumCulledPrimitives);
	INC_DWORD_STAT_BY(STAT_OccludedPrimitives,NumOccludedPrimitives);
}

void FSceneRenderer::PostVisibilityFrameSetup(FILCUpdatePrimTaskData& OutILCTaskData)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PostVisibilityFrameSetup);

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_PostVisibilityFrameSetup_Sort);
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{		
			FViewInfo& View = Views[ViewIndex];

			View.TranslucentPrimSet.SortPrimitives();
			View.MeshDecalPrimSet.SortPrimitives();

			if (View.State)
			{
				((FSceneViewState*)View.State)->TrimHistoryRenderTargets(Scene);
			}
		}
	}

	bool bCheckLightShafts = false;
	if (Scene->GetFeatureLevel() <= ERHIFeatureLevel::ES3_1)
	{
		// Clear the mobile light shaft data.
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{		
			FViewInfo& View = Views[ViewIndex];
			View.bLightShaftUse = false;
			View.LightShaftCenter.X = 0.0f;
			View.LightShaftCenter.Y = 0.0f;
			View.LightShaftColorMask = FLinearColor(0.0f,0.0f,0.0f);
			View.LightShaftColorApply = FLinearColor(0.0f,0.0f,0.0f);
		}
		
		extern int32 GLightShafts;
		bCheckLightShafts = ViewFamily.EngineShowFlags.LightShafts && GLightShafts;
	}

	if (ViewFamily.EngineShowFlags.HitProxies == 0 && Scene->PrecomputedLightVolumes.Num() > 0)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_PostVisibilityFrameSetup_IndirectLightingCache_Update);
		if (GILCUpdatePrimTaskEnabled)
		{
			Scene->IndirectLightingCache.StartUpdateCachePrimitivesTask(Scene, *this, true, OutILCTaskData);
		}
		else
		{
			Scene->IndirectLightingCache.UpdateCache(Scene, *this, true);
		}		
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_PostVisibilityFrameSetup_Light_Visibility);
	// determine visibility of each light
	for(TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights);LightIt;++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		// view frustum cull lights in each view
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{		
			const FLightSceneProxy* Proxy = LightSceneInfo->Proxy;
			FViewInfo& View = Views[ViewIndex];
			FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightIt.GetIndex()];
			// dir lights are always visible, and point/spot only if in the frustum
			if (Proxy->GetLightType() == LightType_Point  
				|| Proxy->GetLightType() == LightType_Spot)
			{
				const float Radius = Proxy->GetRadius();

				if (View.ViewFrustum.IntersectSphere(Proxy->GetOrigin(), Radius))
				{
					if (View.IsPerspectiveProjection())
					{
						FSphere Bounds = Proxy->GetBoundingSphere();
						float DistanceSquared = (Bounds.Center - View.ViewMatrices.GetViewOrigin()).SizeSquared();
						float MaxDistSquared = Proxy->GetMaxDrawDistance() * Proxy->GetMaxDrawDistance() * GLightMaxDrawDistanceScale * GLightMaxDrawDistanceScale;
						const bool bDrawLight = (FMath::Square(FMath::Min(0.0002f, GMinScreenRadiusForLights / Bounds.W) * View.LODDistanceFactor) * DistanceSquared < 1.0f)
													&& (MaxDistSquared == 0 || DistanceSquared < MaxDistSquared);
							
						VisibleLightViewInfo.bInViewFrustum = bDrawLight;
					}
					else
					{
						VisibleLightViewInfo.bInViewFrustum = true;
					}
				}
			}
			else
			{
				VisibleLightViewInfo.bInViewFrustum = true;

				static const auto CVarMobileMSAA = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileMSAA"));
				bool bNotMobileMSAA = !(CVarMobileMSAA ? CVarMobileMSAA->GetValueOnRenderThread() > 1 : false);

				// Setup single sun-shaft from direction lights for mobile.
				if(bCheckLightShafts && LightSceneInfo->bEnableLightShaftBloom)
				{
					// Find directional light for sun shafts.
					// Tweaked values from UE3 implementation.
					const float PointLightFadeDistanceIncrease = 200.0f;
					const float PointLightRadiusFadeFactor = 5.0f;

					const FVector WorldSpaceBlurOrigin = LightSceneInfo->Proxy->GetPosition();
					// Transform into post projection space
					FVector4 ProjectedBlurOrigin = View.WorldToScreen(WorldSpaceBlurOrigin);

					const float DistanceToBlurOrigin = (View.ViewMatrices.GetViewOrigin() - WorldSpaceBlurOrigin).Size() + PointLightFadeDistanceIncrease;

					// Don't render if the light's origin is behind the view
					if(ProjectedBlurOrigin.W >= 0.0f
						// Don't render point lights that have completely faded out
						&& (LightSceneInfo->Proxy->GetLightType() == LightType_Directional 
							|| DistanceToBlurOrigin < LightSceneInfo->Proxy->GetRadius() * PointLightRadiusFadeFactor))
					{
						View.bLightShaftUse = bNotMobileMSAA;
						View.LightShaftCenter.X = ProjectedBlurOrigin.X / ProjectedBlurOrigin.W;
						View.LightShaftCenter.Y = ProjectedBlurOrigin.Y / ProjectedBlurOrigin.W;
						// TODO: Might want to hookup different colors for these.
						View.LightShaftColorMask = LightSceneInfo->BloomTint;
						View.LightShaftColorApply = LightSceneInfo->BloomTint;

						// Apply bloom scale
						View.LightShaftColorMask  *= FLinearColor(LightSceneInfo->BloomScale, LightSceneInfo->BloomScale, LightSceneInfo->BloomScale, 1.0f);
						View.LightShaftColorApply *= FLinearColor(LightSceneInfo->BloomScale, LightSceneInfo->BloomScale, LightSceneInfo->BloomScale, 1.0f);
					}
				}
			}

			// Draw shapes for reflection captures
			if( View.bIsReflectionCapture 
				&& VisibleLightViewInfo.bInViewFrustum
				&& Proxy->HasStaticLighting() 
				&& Proxy->GetLightType() != LightType_Directional 
				// Min roughness is used to hide the specular response of virtual area lights, so skip drawing the source shape when Min Roughness is 1
				&& Proxy->GetMinRoughness() < 1.0f)
			{
				FVector Origin = Proxy->GetOrigin();
				FVector ToLight = Origin - View.ViewMatrices.GetViewOrigin();
				float DistanceSqr = ToLight | ToLight;
				float Radius = Proxy->GetRadius();

				if( DistanceSqr < Radius * Radius )
				{
					FLightParameters LightParameters;

					Proxy->GetParameters(LightParameters);

					// Force to be at least 0.75 pixels
					float CubemapSize = 128.0f;
					float Distance = FMath::Sqrt( DistanceSqr );
					float MinRadius = Distance * 0.75f / CubemapSize;
					LightParameters.LightSourceRadius = FMath::Max( MinRadius, LightParameters.LightSourceRadius );

					// Snap to cubemap pixel center to reduce aliasing
					FVector Scale = ToLight.GetAbs();
					int32 MaxComponent = Scale.X > Scale.Y ? ( Scale.X > Scale.Z ? 0 : 2 ) : ( Scale.Y > Scale.Z ? 1 : 2 );
					for( int32 k = 1; k < 3; k++ )
					{
						float Projected = ToLight[ (MaxComponent + k) % 3 ] / Scale[ MaxComponent ];
						float Quantized = ( FMath::RoundToFloat( Projected * (0.5f * CubemapSize) - 0.5f ) + 0.5f ) / (0.5f * CubemapSize);
						ToLight[ (MaxComponent + k) % 3 ] = Quantized * Scale[ MaxComponent ];
					}
					Origin = ToLight + View.ViewMatrices.GetViewOrigin();
				
					FLinearColor Color( LightParameters.LightColorAndFalloffExponent );

					Color /= PI * FMath::Square( LightParameters.LightSourceRadius ) + 0.5f * PI * LightParameters.LightSourceRadius * LightParameters.LightSourceLength;

					if( Proxy->IsInverseSquared() )
					{
						// Correction for lumen units
						Color *= 16.0f;
						
						float LightRadiusMask = FMath::Square( 1.0f - FMath::Square( DistanceSqr * FMath::Square( LightParameters.LightPositionAndInvRadius.W ) ) );
						Color.A = LightRadiusMask;
					}
					else
					{
						// Remove inverse square falloff
						Color *= DistanceSqr + 1.0f;

						// Apply falloff
						Color.A = FMath::Pow( 1.0f - DistanceSqr * FMath::Square(LightParameters.LightPositionAndInvRadius.W ), LightParameters.LightColorAndFalloffExponent.W );
					}
					
					// Spot falloff
					FVector L = ToLight.GetSafeNormal();
					Color.A *= FMath::Square( FMath::Clamp( ( (L | LightParameters.NormalizedLightDirection) - LightParameters.SpotAngles.X ) * LightParameters.SpotAngles.Y, 0.0f, 1.0f ) );
				
					FMaterialRenderProxy* const ColoredMeshInstance = new(FMemStack::Get()) FColoredMaterialRenderProxy( GEngine->DebugMeshMaterial->GetRenderProxy(false), Color );

					FViewElementPDI LightPDI( &View, NULL );
					// Scaled sphere to handle SourceLength
					const float ZScale = FMath::Max(LightParameters.LightSourceRadius, LightParameters.LightSourceLength);
					DrawSphere(&LightPDI, Origin, FRotationMatrix::MakeFromZ(LightParameters.NormalizedLightDirection).Rotator(), FVector(LightParameters.LightSourceRadius, LightParameters.LightSourceRadius, ZScale), 36, 24, ColoredMeshInstance, SDPG_World);
				}
			}
		}
	}
	}
	{

		QUICK_SCOPE_CYCLE_COUNTER(STAT_PostVisibilityFrameSetup_InitFogConstants);
		InitFogConstants();
	}
}

uint32 GetShadowQuality();

/**
 * Initialize scene's views.
 * Check visibility, sort translucent items, etc.
 */
bool FDeferredShadingSceneRenderer::InitViews(FRHICommandListImmediate& RHICmdList, struct FILCUpdatePrimTaskData& ILCTaskData, FGraphEventArray& SortEvents)
{	
	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_InitViews, FColor::Emerald);
	SCOPE_CYCLE_COUNTER(STAT_InitViewsTime);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{		
		FViewInfo& View = Views[ViewIndex];

		const bool bWillApplyTemporalAA = GPostProcessing.AllowFullPostProcessing(View, FeatureLevel) || (View.bIsPlanarReflection && FeatureLevel >= ERHIFeatureLevel::SM4);

		if (!bWillApplyTemporalAA)
		{
			// Disable anti-aliasing if we are not going to be able to apply final post process effects
			View.AntiAliasingMethod = AAM_None;
		}
	}
	PreVisibilityFrameSetup(RHICmdList);
	ComputeViewVisibility(RHICmdList);

	// This has to happen before Scene->IndirectLightingCache.UpdateCache, since primitives in View.IndirectShadowPrimitives need ILC updates
	CreateIndirectCapsuleShadows();

	PostVisibilityFrameSetup(ILCTaskData);

	FVector AverageViewPosition(0);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{		
		FViewInfo& View = Views[ViewIndex];
		AverageViewPosition += View.ViewMatrices.GetViewOrigin() / Views.Num();
	}

	if (FApp::ShouldUseThreadingForPerformance() && CVarParallelInitViews.GetValueOnRenderThread() > 0)
	{
		AsyncSortBasePassStaticData(AverageViewPosition, SortEvents);
	}
	else
	{
		SortBasePassStaticData(AverageViewPosition);
	}

	bool bDoInitViewAftersPrepass = !!GDoInitViewsLightingAfterPrepass;

	if (!bDoInitViewAftersPrepass)
	{
		InitViewsPossiblyAfterPrepass(RHICmdList, ILCTaskData, SortEvents);
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_InitViews_InitRHIResources);
		// initialize per-view uniform buffer.
		//for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		// NVCHANGE_BEGIN: Add VXGI
		for (int32 ViewIndex = 0; ViewIndex < GetNumViewsWithVxgi(); ViewIndex++)
		{
			//FViewInfo& View = Views[ViewIndex];
			FViewInfo& View = GetViewWithVxgi(ViewIndex);
			// NVCHANGE_END: Add VXGI

			View.ForwardLightingResources = View.ViewState ? &View.ViewState->ForwardLightingResources : &View.ForwardLightingResourcesStorage;

			// Possible stencil dither optimization approach
			View.bAllowStencilDither = bDitheredLODTransitionsUseStencil;

			// Initialize the view's RHI resources.
			View.InitRHIResources();
		}
	}

	SetupVolumetricFog();

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_InitViews_OnStartFrame);
		OnStartFrame(RHICmdList);
	}

	return bDoInitViewAftersPrepass;
}

void FDeferredShadingSceneRenderer::InitViewsPossiblyAfterPrepass(FRHICommandListImmediate& RHICmdList, struct FILCUpdatePrimTaskData& ILCTaskData, FGraphEventArray& SortEvents)
{
	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_InitViewsPossiblyAfterPrepass, FColor::Emerald);
	SCOPE_CYCLE_COUNTER(STAT_InitViewsPossiblyAfterPrepass);

	// this cannot be moved later because of static mesh updates for stuff that is only visible in shadows
	if (SortEvents.Num())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_AsyncSortBasePassStaticData_Wait);
		FTaskGraphInterface::Get().WaitUntilTasksComplete(SortEvents, ENamedThreads::RenderThread);
	}

	if (ViewFamily.EngineShowFlags.DynamicShadows && !IsSimpleForwardShadingEnabled(GetFeatureLevelShaderPlatform(FeatureLevel)))
	{
		// Setup dynamic shadows.
		InitDynamicShadows(RHICmdList);
	}

	// if we kicked off ILC update via task, wait and finalize.
	if (ILCTaskData.TaskRef.IsValid())
	{
		Scene->IndirectLightingCache.FinalizeCacheUpdates(Scene, *this, ILCTaskData);
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_InitViews_UpdatePrimitivePrecomputedLightingBuffers);
		// Now that the indirect lighting cache is updated, we can update the primitive precomputed lighting buffers.
		UpdatePrimitivePrecomputedLightingBuffers();
	}

	// @third party code - BEGIN HairWorks
	// Setup views for hair
	if (!IsForwardShadingEnabled(FeatureLevel))
		HairWorksRenderer::SetupViews(Views);
	// @third party code - END HairWorks

	UpdateTranslucencyTimersAndSeparateTranslucencyBufferSize(RHICmdList);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		SetupReflectionCaptureBuffers(View, RHICmdList);
	}
}

/*------------------------------------------------------------------------------
	FLODSceneTree Implementation
------------------------------------------------------------------------------*/
void FLODSceneTree::AddChildNode(const FPrimitiveComponentId NodeId, FPrimitiveSceneInfo* ChildSceneInfo)
{
	if (NodeId.IsValid() && ChildSceneInfo)
	{
		FLODSceneNode* Node = SceneNodes.Find(NodeId);

		if(!Node)
		{
			Node = &SceneNodes.Add(NodeId, FLODSceneNode());

			// scene info can be added later depending on order of adding to the scene
			// but at least add componentId, that way when parent is added, it will add its info properly
			int32 ParentIndex = Scene->PrimitiveComponentIds.Find(NodeId);
			if(Scene->Primitives.IsValidIndex(ParentIndex))
			{
				Node->SceneInfo = Scene->Primitives[ParentIndex];
			}
			//new nodes that will need distance scale, reset since we don't keep stateful data about this per node.
			ResetHLODDistanceScaleApplication();
		}

		Node->AddChild(ChildSceneInfo);
	}
}

void FLODSceneTree::RemoveChildNode(const FPrimitiveComponentId NodeId, FPrimitiveSceneInfo* ChildSceneInfo)
{
	if(NodeId.IsValid() && ChildSceneInfo)
	{
		FLODSceneNode* Node = SceneNodes.Find(NodeId);
		if (Node)
		{
			Node->RemoveChild(ChildSceneInfo);

			// delete from scene if no children remains
			if(Node->ChildrenSceneInfos.Num() == 0)
			{
				SceneNodes.Remove(NodeId);
			}
		}
	}
}

void FLODSceneTree::UpdateNodeSceneInfo(FPrimitiveComponentId NodeId, FPrimitiveSceneInfo* SceneInfo)
{
	FLODSceneNode* Node = SceneNodes.Find(NodeId);
	if(Node)
	{
		Node->SceneInfo = SceneInfo;
	}
}

void FLODSceneTree::UpdateAndApplyVisibilityStates(FViewInfo& View)
{
	if (FSceneViewState* ViewState = (FSceneViewState*)View.State)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Skip update logic when frozen
		if (ViewState->bIsFrozen)
		{
			return;
		}
#endif

		const float HLODDistanceScale = FMath::Max(0.0f, CVarHLODDistanceScale.GetValueOnRenderThread());

		// Per-frame initialization
		FHLODVisibilityState& HLODState = ViewState->HLODVisibilityState;
		TMap<FPrimitiveComponentId, FHLODSceneNodeVisibilityState>& VisibilityStates = ViewState->HLODSceneNodeVisibilityStates;

		HLODState.PrimitiveFadingLODMap.Init(false, View.PrimitiveVisibilityMap.Num());
		HLODState.PrimitiveFadingOutLODMap.Init(false, View.PrimitiveVisibilityMap.Num());
		HLODState.HiddenChildPrimitiveMap.Init(false, View.PrimitiveVisibilityMap.Num());
		FSceneBitArray& VisibilityFlags = View.PrimitiveVisibilityMap;
		TArray<FPrimitiveViewRelevance, SceneRenderingAllocator>& RelevanceMap = View.PrimitiveViewRelevanceMap;

		int32 UpdateCount = ++HLODState.UpdateCount;

		// Update persistent state on temporal dither sync frames
		const FTemporalLODState& LODState = ViewState->GetTemporalLODState();
		bool bSyncFrame = false;
		
		if (HLODState.TemporalLODSyncTime != LODState.TemporalLODTime[0])
		{
			HLODState.TemporalLODSyncTime = LODState.TemporalLODTime[0];
			bSyncFrame = true;
		}

		for (auto Iter = SceneNodes.CreateIterator(); Iter; ++Iter)
		{
			FLODSceneNode& Node = Iter.Value();

			if (!Node.SceneInfo)
			{
				continue;
			}

			FHLODSceneNodeVisibilityState& NodeVisibility = VisibilityStates.FindOrAdd(Node.SceneInfo->PrimitiveComponentId);
			const TIndirectArray<FStaticMesh>& NodeMeshes = Node.SceneInfo->StaticMeshes;

			// Ignore already updated nodes, or those that we can't work with
			if (NodeVisibility.UpdateCount == UpdateCount || NodeMeshes.Num() == 0)
			{
				continue;
			}

			const int32 NodeIndex = Node.SceneInfo->GetIndex();
			bool bIsVisible = VisibilityFlags[NodeIndex];

			FPrimitiveBounds& Bounds = Scene->PrimitiveBounds[NodeIndex];
			{
				if (LastHLODDistanceScale != HLODDistanceScale)
				{
					// Determine desired HLOD state
					const float MinDrawDistance = Scene->Primitives[NodeIndex]->Proxy->GetMinDrawDistance();						
					const float AdjustedMinDrawDist = MinDrawDistance * HLODDistanceScale;
					Bounds.MinDrawDistanceSq = AdjustedMinDrawDist * AdjustedMinDrawDist;
				}
			}

			const float DistanceSquared = (Bounds.BoxSphereBounds.Origin - View.ViewMatrices.GetViewOrigin()).SizeSquared();
			const bool bIsInDrawRange = DistanceSquared >= Bounds.MinDrawDistanceSq;

			const bool bWasFadingPreUpdate = !!NodeVisibility.bIsFading;

			// Update fading state
			if (NodeMeshes[0].bDitheredLODTransition)
			{
				// Fade when HLODs change threshold on-screen, else snap
				// TODO: This logic can still be improved to clear state and
				//       transitions when off-screen, but needs better detection
				const bool bChangedRange = bIsInDrawRange != !!NodeVisibility.bWasVisible;
				const bool bIsOnScreen = bIsVisible || NodeVisibility.bWasVisible;
				
				// Update with syncs
				if (bSyncFrame)
				{
					if (NodeVisibility.bIsFading)
					{
						NodeVisibility.bIsFading = false;
					}
					else if (bChangedRange && bIsOnScreen)
					{
						NodeVisibility.bIsFading = true;	
					}

					NodeVisibility.bWasVisible = NodeVisibility.bIsVisible;
					NodeVisibility.bIsVisible = bIsInDrawRange;
				}

				// Flag as fading or freeze visibility if waiting for a fade
				if (NodeVisibility.bIsFading)
				{
					HLODState.PrimitiveFadingLODMap[NodeIndex] = true;
					HLODState.PrimitiveFadingOutLODMap[NodeIndex] = !NodeVisibility.bIsVisible;
				}
				else if (bChangedRange && bIsOnScreen)
				{
					VisibilityFlags[NodeIndex] = !!NodeVisibility.bWasVisible;
					bIsVisible = !!NodeVisibility.bWasVisible;
				}
			}
			else
			{
				// Instant transitions without dithering
				NodeVisibility.bWasVisible = NodeVisibility.bIsVisible;
				NodeVisibility.bIsVisible = bIsInDrawRange;
				NodeVisibility.bIsFading = false;
			}

			if (NodeVisibility.bIsFading)
			{
				// Fade until state back in sync
				ApplyNodeFadingToChildren(ViewState, Node, VisibilityFlags, true, !!NodeVisibility.bIsVisible);
			}
			else if (bIsVisible)
			{
				// If stable and visible, override hierarchy visibility
				HideNodeChildren(ViewState, Node, VisibilityFlags);
			}

			// Flush cached lighting data when changing visible contents
			if (NodeVisibility.bIsVisible != NodeVisibility.bWasVisible || bWasFadingPreUpdate || NodeVisibility.bIsFading)
			{
				FLightPrimitiveInteraction* NodeLightList = Node.SceneInfo->LightList;
				while (NodeLightList)
				{
					NodeLightList->FlushCachedShadowMapData();
					NodeLightList = NodeLightList->GetNextLight();
				}
			}

			// Force fully disabled view relevance so shadows don't attempt to recompute
			if (!NodeVisibility.bIsVisible)
			{
				FPrimitiveViewRelevance& ViewRelevance = RelevanceMap[NodeIndex];
				FMemory::Memzero(&ViewRelevance, sizeof(FPrimitiveViewRelevance));
				ViewRelevance.bInitializedThisFrame = true;
			}
		}
		LastHLODDistanceScale = HLODDistanceScale;
	}	
}

void FLODSceneTree::ApplyNodeFadingToChildren(FSceneViewState* ViewState, FLODSceneNode& Node, FSceneBitArray& VisibilityFlags, const bool bIsFading, const bool bIsFadingOut)
{
	checkSlow(ViewState);

	if (Node.SceneInfo)
	{
		FHLODVisibilityState& HLODState = ViewState->HLODVisibilityState;
		TMap<FPrimitiveComponentId, FHLODSceneNodeVisibilityState>& VisibilityStates = ViewState->HLODSceneNodeVisibilityStates;
		FHLODSceneNodeVisibilityState& NodeVisibility = VisibilityStates.FindOrAdd(Node.SceneInfo->PrimitiveComponentId);
		NodeVisibility.UpdateCount = HLODState.UpdateCount;

		// Force visibility during fades
		const int32 NodeIndex = Node.SceneInfo->GetIndex();
		VisibilityFlags[NodeIndex] = true;

		for (const auto& Child : Node.ChildrenSceneInfos)
		{
			const int32 ChildIndex = Child->GetIndex();

			HLODState.PrimitiveFadingLODMap[ChildIndex] = bIsFading;
			HLODState.PrimitiveFadingOutLODMap[ChildIndex] = bIsFadingOut;
			HLODState.HiddenChildPrimitiveMap[ChildIndex] = false;
			VisibilityFlags[ChildIndex] = true;

			// Fading only occurs at the adjacent hierarchy level, below should be hidden
			if (FLODSceneNode* ChildNode = SceneNodes.Find(Child->PrimitiveComponentId))
			{
				HideNodeChildren(ViewState, *ChildNode, VisibilityFlags);
			}
		}
	}
}

void FLODSceneTree::HideNodeChildren(FSceneViewState* ViewState, FLODSceneNode& Node, FSceneBitArray& VisibilityFlags)
{
	checkSlow(ViewState);
	FHLODVisibilityState& HLODState = ViewState->HLODVisibilityState;
	TMap<FPrimitiveComponentId, FHLODSceneNodeVisibilityState>& VisibilityStates = ViewState->HLODSceneNodeVisibilityStates;
	FHLODSceneNodeVisibilityState& NodeVisibility = VisibilityStates.FindOrAdd(Node.SceneInfo->PrimitiveComponentId);

	if (NodeVisibility.UpdateCount != HLODState.UpdateCount)
	{
		NodeVisibility.UpdateCount = HLODState.UpdateCount;

		for (const auto& Child : Node.ChildrenSceneInfos)
		{
			const int32 ChildIndex = Child->GetIndex();
			HLODState.HiddenChildPrimitiveMap[ChildIndex] = true;
			VisibilityFlags[ChildIndex] = false;

			if (FLODSceneNode* ChildNode = SceneNodes.Find(Child->PrimitiveComponentId))
			{
				HideNodeChildren(ViewState, *ChildNode, VisibilityFlags);
			}
		}
	}
}
