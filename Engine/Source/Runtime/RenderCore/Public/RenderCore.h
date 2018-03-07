// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RenderCore.h: Render core module definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRendererCore, Log, All);

/**
 * The scene rendering stats.
 */

DECLARE_CYCLE_STAT_EXTERN(TEXT("BeginOcclusionTests"),STAT_BeginOcclusionTestsTime,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("RenderQuery Result"),STAT_RenderQueryResultTime,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("InitViews"),STAT_InitViewsTime,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("InitViewsPossiblyAfterPrepass"), STAT_InitViewsPossiblyAfterPrepass, STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Dynamic shadow setup"), STAT_DynamicShadowSetupTime, STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Total GPU rendering"),STAT_TotalGPUFrameTime,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("RenderViewFamily"),STAT_TotalSceneRenderingTime,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("FinishRenderViewTarget"),STAT_FinishRenderViewTargetTime,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("RenderVelocities"),STAT_RenderVelocities,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Depth drawing"),STAT_DepthDrawTime,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Base pass drawing"),STAT_BasePassDrawTime,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Lighting drawing"),STAT_LightingDrawTime,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Proj Shadow drawing"),STAT_ProjectedShadowDrawTime,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Reflective Shadow map drawing"),STAT_ReflectiveShadowMapDrawTime,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update LPVs"),STAT_UpdateLPVs,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Translucency drawing"),STAT_TranslucencyDrawTime,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Dynamic Primitive drawing"),STAT_DynamicPrimitiveDrawTime,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("StaticDrawList drawing"),STAT_StaticDrawListDrawTime,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Lights in scene"),STAT_SceneLights,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Mesh draw calls"),STAT_MeshDrawCalls,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Dynamic path draw calls"),STAT_DynamicPathMeshDrawCalls,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Static list draw calls"),STAT_StaticDrawListMeshDrawCalls,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Present time"),STAT_PresentTime,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Decals in scene"),STAT_SceneDecals,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Decals in view"),STAT_Decals,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Decal drawing"),STAT_DecalsDrawTime,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Cache Uniform Expressions"),STAT_CacheUniformExpressions,STATGROUP_SceneRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Lights using light shafts"), STAT_LightShaftsLights, STATGROUP_SceneRendering, RENDERCORE_API);

DECLARE_CYCLE_STAT_EXTERN(TEXT("View Visibility"),STAT_ViewVisibilityTime,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Decompress Occlusion"),STAT_DecompressPrecomputedOcclusion,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Frustum Cull"),STAT_FrustumCull,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update Fading"),STAT_UpdatePrimitiveFading,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Occlusion Cull"),STAT_OcclusionCull,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("View Relevance"),STAT_ViewRelevance,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Compute View Relevance"),STAT_ComputeViewRelevance,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Static Mesh Relevance"),STAT_StaticRelevance,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("UpdateStaticMeshes"),STAT_UpdateStaticMeshesTime,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("GetDynamicMeshElements"),STAT_GetDynamicMeshElements,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Init dynamic shadows"),STAT_InitDynamicShadowsTime,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update Preshadow Cache"),STAT_UpdatePreshadowCache,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Init Projected Shadow"),STAT_InitProjectedShadowVisibility,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Create WholeScene Shadow"),STAT_CreateWholeSceneProjectedShadow,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Add View Whole Scene Shadows"),STAT_AddViewDependentWholeSceneShadowsForView,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Setup Interaction Shadows"),STAT_SetupInteractionShadows,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("SortStaticDrawLists"),STAT_SortStaticDrawLists,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update Indirect Lighting Cache"),STAT_UpdateIndirectLightingCache,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update Indirect Lighting Cache Prims"), STAT_UpdateIndirectLightingCachePrims, STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update Indirect Lighting Cache Blocks"), STAT_UpdateIndirectLightingCacheBlocks, STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update Indirect Lighting Cache Transitions"), STAT_UpdateIndirectLightingCacheTransitions, STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update Indirect Lighting Cache Finalize"), STAT_UpdateIndirectLightingCacheFinalize, STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Interpolate Volumetric Lightmap"), STAT_InterpolateVolumetricLightmapOnCPU, STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("GatherShadowPrimitives"),STAT_GatherShadowPrimitivesTime,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("BuildCombinedStaticAndCSMVisibilityState"), STAT_BuildCombinedStaticAndCSMVisibilityState, STATGROUP_InitViews, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Processed primitives"),STAT_ProcessedPrimitives,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Frustum Culled primitives"),STAT_CulledPrimitives,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Statically occluded primitives"),STAT_StaticallyOccludedPrimitives,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Occluded primitives"),STAT_OccludedPrimitives,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Occlusion queries"),STAT_OcclusionQueries,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Visible static mesh elements"),STAT_VisibleStaticMeshElements,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Visible dynamic primitives"),STAT_VisibleDynamicPrimitives,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Indirect Lighting Cache updates"),STAT_IndirectLightingCacheUpdates,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Precomputed Lighting Buffer updates"),STAT_PrecomputedLightingBufferUpdates,STATGROUP_InitViews, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("CSM Subject Primitives"), STAT_CSMSubjects, STATGROUP_InitViews, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Static+CSM Static Mesh Receivers"), STAT_CSMStaticMeshReceivers, STATGROUP_InitViews, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Static+CSM Static Primitive Receivers"), STAT_CSMStaticPrimitiveReceivers, STATGROUP_InitViews, RENDERCORE_API);

DECLARE_CYCLE_STAT_EXTERN(TEXT("WholeScene Shadow Projections"),STAT_RenderWholeSceneShadowProjectionsTime,STATGROUP_ShadowRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("WholeScene Shadow Depths"),STAT_RenderWholeSceneShadowDepthsTime,STATGROUP_ShadowRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("PerObject Shadow Projections"),STAT_RenderPerObjectShadowProjectionsTime,STATGROUP_ShadowRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("PerObject Shadow Depths"),STAT_RenderPerObjectShadowDepthsTime,STATGROUP_ShadowRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Static Draw List"),STAT_WholeSceneStaticDrawListShadowDepthsTime,STATGROUP_ShadowRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Static Primitives"),STAT_WholeSceneStaticShadowDepthsTime,STATGROUP_ShadowRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Dynamic Primitives"),STAT_WholeSceneDynamicShadowDepthsTime,STATGROUP_ShadowRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("WholeScene Reflective Shadow maps"),STAT_RenderWholeSceneReflectiveShadowMapsTime,STATGROUP_ShadowRendering, RENDERCORE_API); 
DECLARE_MEMORY_STAT_EXTERN(TEXT("Per-Frame Shadowmap Atlases"),STAT_ShadowmapAtlasMemory,STATGROUP_ShadowRendering, RENDERCORE_API); 
DECLARE_MEMORY_STAT_EXTERN(TEXT("Cached Shadowmaps"),STAT_CachedShadowmapMemory,STATGROUP_ShadowRendering, RENDERCORE_API); 
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Whole Scene shadows"),STAT_WholeSceneShadows,STATGROUP_ShadowRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Cached Whole Scene shadows"),STAT_CachedWholeSceneShadows,STATGROUP_ShadowRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("PreShadows"),STAT_PreShadows,STATGROUP_ShadowRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Reflective Shadow Maps"),STAT_ReflectiveShadowMaps,STATGROUP_ShadowRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Cached PreShadows"),STAT_CachedPreShadows,STATGROUP_ShadowRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Per Object shadows"),STAT_PerObjectShadows,STATGROUP_ShadowRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Dynamic path draw calls"),STAT_ShadowDynamicPathDrawCalls,STATGROUP_ShadowRendering, RENDERCORE_API);

// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI

DECLARE_STATS_GROUP(TEXT("VXGI Voxelization"), STATGROUP_VxgiVoxelization, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("VXGI Opacity"), STAT_VxgiVoxelizeOpacity, STATGROUP_VxgiVoxelization, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("VXGI Emissive Materials and Indirect Irradiance"), STAT_VxgiVoxelizeEmissiveAndIndirectIrradiance, STATGROUP_VxgiVoxelization, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("VXGI Emittance From Diffuse Materials Unshadowed"), STAT_VxgiVoxelizeEmittanceFromDiffuseMaterials, STATGROUP_VxgiVoxelization, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("VXGI Emittance From Diffuse Materials Shadowed"), STAT_VxgiVoxelizeEmittanceFromDiffuseMaterialsShadowed, STATGROUP_VxgiVoxelization, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("VXGI Visibility"), STAT_VxgiVoxelizationVisibility, STATGROUP_VxgiVoxelization, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("VXGI Static Geometry"), STAT_VxgiVoxelizationStaticGeometry, STATGROUP_VxgiVoxelization, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("VXGI Dynamic Geometry"), STAT_VxgiVoxelizationDynamicGeometry, STATGROUP_VxgiVoxelization, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("VXGI SetMeshRenderState"), STAT_VxgiVoxelizationSetMeshRenderState, STATGROUP_VxgiVoxelization, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("VXGI SetSharedState"), STAT_VxgiVoxelizationSetSharedState, STATGROUP_VxgiVoxelization, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("VXGI ApplyNonShaderState"), STAT_VxgiVoxelizationApplyNonShaderState, STATGROUP_VxgiVoxelization, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("VXGI SaveUserState"), STAT_VxgiVoxelizationSaveUserState, STATGROUP_VxgiVoxelization, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("VXGI RestoreUserState"), STAT_VxgiVoxelizationRestoreUserState, STATGROUP_VxgiVoxelization, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("VXGI ApplyVoxelizationDrawCallState"), STAT_VxgiApplyVoxelizationMaterialInfo, STATGROUP_VxgiVoxelization, RENDERCORE_API);

#endif
// NVCHANGE_END: Add VXGI

DECLARE_CYCLE_STAT_EXTERN(TEXT("Render Lights and Shadows"),STAT_LightRendering,STATGROUP_LightRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Direct lighting"),STAT_DirectLightRenderingTime,STATGROUP_LightRendering, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Translucent injection"),STAT_TranslucentInjectTime,STATGROUP_LightRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num using standard deferred"),STAT_NumLightsUsingStandardDeferred,STATGROUP_LightRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num using tiled deferred"),STAT_NumLightsUsingTiledDeferred,STATGROUP_LightRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num using simple lighting model"),STAT_NumLightsUsingSimpleTiledDeferred,STATGROUP_LightRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num injected into translucency"),STAT_NumLightsInjectedIntoTranslucency,STATGROUP_LightRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num unshadowed"),STAT_NumUnshadowedLights,STATGROUP_LightRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num light function only"),STAT_NumLightFunctionOnlyLights,STATGROUP_LightRendering, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num shadowed"),STAT_NumShadowedLights,STATGROUP_LightRendering, RENDERCORE_API);

/** The scene update stats. */
// The purpose of the SceneUpdate stat group is to show where rendering thread time is going from a high level outside of RenderViewFamily.
// It should only contain stats that are likely to track a lot of time in a typical scene, not edge case stats that are rarely non-zero.
// Use a more detailed profiler (like an instruction trace or sampling capture on Xbox 360) to track down where time is going in more detail.
DECLARE_CYCLE_STAT_EXTERN(TEXT("AddPrimitive"),STAT_AddScenePrimitiveRenderThreadTime,STATGROUP_SceneUpdate, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AddLight"),STAT_AddSceneLightTime,STATGROUP_SceneUpdate, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("RemovePrimitive"),STAT_RemoveScenePrimitiveTime,STATGROUP_SceneUpdate, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("RemoveLight"),STAT_RemoveSceneLightTime,STATGROUP_SceneUpdate, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("UpdateLight"),STAT_UpdateSceneLightTime,STATGROUP_SceneUpdate, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("UpdatePrimitiveTransform"),STAT_UpdatePrimitiveTransformRenderThreadTime,STATGROUP_SceneUpdate, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update CPU Skin"),STAT_CPUSkinUpdateRTTime,STATGROUP_SceneUpdate, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update GPU Skin"),STAT_GPUSkinUpdateRTTime,STATGROUP_SceneUpdate, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update particles"),STAT_ParticleUpdateRTTime,STATGROUP_SceneUpdate, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update Influence Weights"),STAT_InfluenceWeightsUpdateRTTime,STATGROUP_SceneUpdate, RENDERCORE_API);

DECLARE_CYCLE_STAT_EXTERN(TEXT("RemovePrimitive (GT)"),STAT_RemoveScenePrimitiveGT,STATGROUP_SceneUpdate, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("AddPrimitive (GT)"),STAT_AddScenePrimitiveGT,STATGROUP_SceneUpdate, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("UpdatePrimitiveTransform (GT)"),STAT_UpdatePrimitiveTransformGT,STATGROUP_SceneUpdate, RENDERCORE_API);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Set Shader Maps On Material Resources (RT)"),STAT_Scene_SetShaderMapsOnMaterialResources_RT,STATGROUP_SceneUpdate, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update Static Draw Lists For Materials (RT)"),STAT_Scene_UpdateStaticDrawListsForMaterials_RT,STATGROUP_SceneUpdate, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN( TEXT( "Proxy Total" ), STAT_GameToRendererMallocTotal, STATGROUP_Memory, RENDERCORE_API );

/** Memory stats for tracking virtual allocations used by the renderer to represent the scene. */
DECLARE_MEMORY_STAT_EXTERN(TEXT("Static draw list memory"),STAT_StaticDrawListMemory,STATGROUP_SceneMemory, RENDERCORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Primitive memory"),STAT_PrimitiveInfoMemory,STATGROUP_SceneMemory, RENDERCORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Scene memory"),STAT_RenderingSceneMemory,STATGROUP_SceneMemory, RENDERCORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("ViewState memory"),STAT_ViewStateMemory,STATGROUP_SceneMemory, RENDERCORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Rendering mem stack memory"),STAT_RenderingMemStackMemory,STATGROUP_SceneMemory, RENDERCORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Light interaction memory"),STAT_LightInteractionMemory,STATGROUP_SceneMemory, RENDERCORE_API);

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num reflective shadow map lights"),STAT_NumReflectiveShadowMapLights,STATGROUP_LightRendering, RENDERCORE_API);

DECLARE_MEMORY_STAT_EXTERN(TEXT("Pool Size"), STAT_RenderTargetPoolSize, STATGROUP_RenderTargetPool, RENDERCORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Pool Used"), STAT_RenderTargetPoolUsed, STATGROUP_RenderTargetPool, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Pool Count"), STAT_RenderTargetPoolCount, STATGROUP_RenderTargetPool, RENDERCORE_API);

/**
 *	Timer helper class.
 **/
class FTimer
{
public:
	/**
	 *	Constructor
	 **/
	FTimer()
		: CurrentDeltaTime(0.0f)
		, CurrentTime(0.0f)
	{
	}

	/**
	 *	Returns the current time, in seconds.
	 *	@return Current time, in seconds
	 */
	float GetCurrentTime() const
	{
		return CurrentTime;
	}

	/**
	 *	Returns the current delta time.
	 *	@return Current delta time (number of seconds that passed between the last two tick)
	 */
	float GetCurrentDeltaTime() const
	{
		return CurrentDeltaTime;
	}

	/**
	 *	Updates the timer.
	 *	@param DeltaTime	Number of seconds that have passed since the last tick
	 **/
	void Tick( float DeltaTime )
	{
		CurrentDeltaTime = DeltaTime;
		CurrentTime += DeltaTime;
	}

protected:
	/** Current delta time (number of seconds that passed between the last two tick). */
	float CurrentDeltaTime;
	/** Current time, in seconds. */
	float CurrentTime;
};

/** Whether to pause the global realtime clock for the rendering thread (read and write only on main thread). */
extern RENDERCORE_API bool GPauseRenderingRealtimeClock;

/** Global realtime clock for the rendering thread. */
extern RENDERCORE_API FTimer GRenderingRealtimeClock;

/**
 * Encapsulates a latency timer that measures the time from when mouse input
 * is read on the gamethread until that frame is fully displayed by the GPU.
 */
struct FInputLatencyTimer
{
	/**
	 * Constructor
	 * @param InUpdateFrequency	How often the timer should be updated (in seconds).
	 */
	FInputLatencyTimer( float InUpdateFrequency )
	{
		bInitialized = false;
		RenderThreadTrigger = false;
		GameThreadTrigger = false;
		StartTime = 0;
		DeltaTime = 0;
		LastCaptureTime = 0.0f;
		UpdateFrequency = InUpdateFrequency;
	}

	/** Potentially starts the timer on the gamethread, based on the UpdateFrequency. */
	RENDERCORE_API void GameThreadTick();

	/** @return The number of seconds of input latency. */
	inline float GetDeltaSeconds() const
	{
		return FPlatformTime::ToSeconds(DeltaTime);
	}

	/** Weather GInputLatencyTimer is initialized or not. */
	bool	bInitialized;

	/** Weather a measurement has been triggered on the gamethread. */
	bool	GameThreadTrigger;

	/** Weather a measurement has been triggered on the renderthread. */
	bool	RenderThreadTrigger;

	/** Start time (in FPlatformTime::Cycles). */
	uint32	StartTime;

	/** Last delta time that was measured (in FPlatformTime::Cycles). */
	uint32	DeltaTime;

	/** Last time we did a measurement (in seconds). */
	float	LastCaptureTime;

	/** How often we should do a measurement (in seconds). */
	float	UpdateFrequency;
};

namespace ERenderThreadIdleTypes
{
	enum Type
	{
		WaitingForAllOtherSleep, 
		WaitingForGPUQuery, 
		WaitingForGPUPresent, 
		Num
	};
}

/** Accumulates how many cycles the renderthread has been idle. It's defined in RenderingThread.cpp. */
extern RENDERCORE_API uint32 GRenderThreadIdle[ERenderThreadIdleTypes::Num];
/** Accumulates how times renderthread was idle. It's defined in RenderingThread.cpp. */
extern RENDERCORE_API uint32 GRenderThreadNumIdle[ERenderThreadIdleTypes::Num];
/** Global input latency timer. Defined in UnrealClient.cpp */
extern RENDERCORE_API FInputLatencyTimer GInputLatencyTimer;
/** How many cycles the renderthread used (excluding idle time). It's set once per frame in FViewport::Draw. */
extern RENDERCORE_API uint32 GRenderThreadTime;
/** How many cycles the gamethread used (excluding idle time). It's set once per frame in FViewport::Draw. */
extern RENDERCORE_API uint32 GGameThreadTime;
/** How many cycles it took to swap buffers to present the frame. */
extern RENDERCORE_API uint32 GSwapBufferTime;

// shared by renderer and engine, compiles down to a constant in final release
RENDERCORE_API int32 GetCVarForceLOD();

RENDERCORE_API int32 GetCVarForceLODShadow();

RENDERCORE_API bool IsHDREnabled();


