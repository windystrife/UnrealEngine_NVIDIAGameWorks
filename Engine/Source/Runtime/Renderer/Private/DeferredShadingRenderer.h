// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DeferredShadingRenderer.h: Scene rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "RendererInterface.h"
#include "StaticBoundShaderState.h"
#include "ScenePrivateBase.h"
#include "LightSceneInfo.h"
#include "SceneRendering.h"
#include "DepthRendering.h"

class FDistanceFieldAOParameters;
class UStaticMeshComponent;

// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
#include "GFSDK_VXGI.h"
#endif
// NVCHANGE_END: Add VXGI

class FLightShaftsOutput
{
public:
	// 0 if not rendered
	TRefCountPtr<IPooledRenderTarget> LightShaftOcclusion;
};

/**
 * Scene renderer that implements a deferred shading pipeline and associated features.
 */
class FDeferredShadingSceneRenderer : public FSceneRenderer
{
public:

	/** Defines which objects we want to render in the EarlyZPass. */
	EDepthDrawingMode EarlyZPassMode;
	bool bEarlyZPassMovable;
	bool bDitheredLODTransitionsUseStencil;
	
	FComputeFenceRHIRef TranslucencyLightingVolumeClearEndFence;

	FDeferredShadingSceneRenderer(const FSceneViewFamily* InViewFamily,FHitProxyConsumer* HitProxyConsumer);

	/** Clears a view */
	void ClearView(FRHICommandListImmediate& RHICmdList);

	/** Clears gbuffer where Z is still at the maximum value (ie no geometry rendered) */
	void ClearGBufferAtMaxZ(FRHICommandList& RHICmdList);

	/** Clears LPVs for all views */
	void ClearLPVs(FRHICommandListImmediate& RHICmdList);

	/** Propagates LPVs for all views */
	void UpdateLPVs(FRHICommandListImmediate& RHICmdList);

	/**
	 * Renders the dynamic scene's prepass for a particular view
	 * @return true if anything was rendered
	 */
	bool RenderPrePassViewDynamic(FRHICommandList& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState);

	/**
	 * Renders the scene's prepass for a particular view
	 * @return true if anything was rendered
	 */
	bool RenderPrePassView(FRHICommandList& RHICmdList, const FViewInfo& View);

	/**
	 * Renders the scene's prepass for a particular view in parallel
	 * @return true if the depth was cleared
	 */
	bool RenderPrePassViewParallel(const FViewInfo& View, FRHICommandListImmediate& ParentCmdList, TFunctionRef<void()> AfterTasksAreStarted, bool bDoPrePre);

	/** Culls local lights to a grid in frustum space.  Needed for forward shading or translucency using the Surface lighting mode. */
	void ComputeLightGrid(FRHICommandListImmediate& RHICmdList);

	/** Renders the basepass for the static data of a given View. */
	bool RenderBasePassStaticData(FRHICommandList& RHICmdList, FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState);
	bool RenderBasePassStaticDataType(FRHICommandList& RHICmdList, FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, const EBasePassDrawListType DrawType);

	/** Renders the basepass for the static data of a given View. Parallel versions.*/
	void RenderBasePassStaticDataParallel(FParallelCommandListSet& ParallelCommandListSet);
	void RenderBasePassStaticDataTypeParallel(FParallelCommandListSet& ParallelCommandListSet, const EBasePassDrawListType DrawType);

	/** Asynchronously sorts base pass draw lists front to back for improved GPU culling. */
	void AsyncSortBasePassStaticData(const FVector ViewPosition, FGraphEventArray &SortEvents);

	/** Sorts base pass draw lists front to back for improved GPU culling. */
	void SortBasePassStaticData(FVector ViewPosition);

	/** Renders the basepass for the dynamic data of a given View. */
	void RenderBasePassDynamicData(FRHICommandList& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, bool& bOutDirty);

	/** Renders the basepass for the dynamic data of a given View, in parallel. */
	void RenderBasePassDynamicDataParallel(FParallelCommandListSet& ParallelCommandListSet);

	/** Renders the basepass for a given View, in parallel */
	void RenderBasePassViewParallel(FViewInfo& View, FRHICommandListImmediate& ParentCmdList, FExclusiveDepthStencil::Type BasePassDepthStencilAccess);

	/** Renders the basepass for a given View. */
	bool RenderBasePassView(FRHICommandListImmediate& RHICmdList, FViewInfo& View, FExclusiveDepthStencil::Type BasePassDepthStencilAccess);

	/** Renders editor primitives for a given View. */
	void RenderEditorPrimitives(FRHICommandList& RHICmdList, const FViewInfo& View, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, bool& bOutDirty);
	
	/** 
	* Renders the scene's base pass 
	* @return true if anything was rendered
	*/
	bool RenderBasePass(FRHICommandListImmediate& RHICmdList, FExclusiveDepthStencil::Type BasePassDepthStencilAccess);

	/** Finishes the view family rendering. */
	void RenderFinish(FRHICommandListImmediate& RHICmdList);

	bool RenderHzb(FRHICommandListImmediate& RHICmdList);

	void RenderOcclusion(FRHICommandListImmediate& RHICmdList);

	void FinishOcclusion(FRHICommandListImmediate& RHICmdList);

	/** Renders the view family. */
	virtual void Render(FRHICommandListImmediate& RHICmdList) override;

	/** Render the view family's hit proxies. */
	virtual void RenderHitProxies(FRHICommandListImmediate& RHICmdList) override;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	void RenderVisualizeTexturePool(FRHICommandListImmediate& RHICmdList);
#endif

private:

	// fences to make sure the rhi thread has digested the occlusion query renders before we attempt to read them back async
	static FGraphEventRef OcclusionSubmittedFence[FOcclusionQueryHelpers::MaxBufferedOcclusionFrames];
	static FGraphEventRef TranslucencyTimestampQuerySubmittedFence[FOcclusionQueryHelpers::MaxBufferedOcclusionFrames + 1];

	/** Creates a per object projected shadow for the given interaction. */
	void CreatePerObjectProjectedShadow(
		FRHICommandListImmediate& RHICmdList,
		FLightPrimitiveInteraction* Interaction,
		bool bCreateTranslucentObjectShadow,
		bool bCreateInsetObjectShadow,
		const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& ViewDependentWholeSceneShadows,
		TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& OutPreShadows);

	/**
	* Used by RenderLights to figure out if light functions need to be rendered to the attenuation buffer.
	*
	* @param LightSceneInfo Represents the current light
	* @return true if anything got rendered
	*/
	bool CheckForLightFunction(const FLightSceneInfo* LightSceneInfo) const;

	/** Determines which primitives are visible for each view. */
	bool InitViews(FRHICommandListImmediate& RHICmdList, struct FILCUpdatePrimTaskData& ILCTaskData, FGraphEventArray& SortEvents);

	void InitViewsPossiblyAfterPrepass(FRHICommandListImmediate& RHICmdList, struct FILCUpdatePrimTaskData& ILCTaskData, FGraphEventArray& SortEvents);

	void SetupReflectionCaptureBuffers(FViewInfo& View, FRHICommandListImmediate& RHICmdList);

	/**
	Updates auto-downsampling of separate translucency and sets FSceneRenderTargets::SeparateTranslucencyBufferSize.
	Also updates timers for stats on GPU translucency times.
	*/
	void UpdateTranslucencyTimersAndSeparateTranslucencyBufferSize(FRHICommandListImmediate& RHICmdList);

	void CreateIndirectCapsuleShadows();

	/**
	* Setup the prepass. This is split out so that in parallel we can do the fx prerender after we start the parallel tasks
	* @return true if the depth was cleared
	*/
	bool PreRenderPrePass(FRHICommandListImmediate& RHICmdList);

	void RenderPrePassEditorPrimitives(FRHICommandList& RHICmdList, const FViewInfo& View, FDepthDrawingPolicyFactory::ContextType Context);

	/**
	 * Renders the scene's prepass and occlusion queries.
	 * @return true if the depth was cleared
	 */
	bool RenderPrePass(FRHICommandListImmediate& RHICmdList, TFunctionRef<void()> AfterTasksAreStarted);

	/**
	 * Renders the active HMD's hidden area mask as a depth prepass, if available.
	 * @return true if depth is cleared
	 */
	bool RenderPrePassHMD(FRHICommandListImmediate& RHICmdList);

	/** Issues occlusion queries. */
	void BeginOcclusionTests(FRHICommandListImmediate& RHICmdList, bool bRenderQueries);

	/** Renders the scene's fogging. */
	bool RenderFog(FRHICommandListImmediate& RHICmdList, const FLightShaftsOutput& LightShaftsOutput);

	/** Renders the scene's atmosphere. */
	void RenderAtmosphere(FRHICommandListImmediate& RHICmdList, const FLightShaftsOutput& LightShaftsOutput);

	/** Renders reflections that can be done in a deferred pass. */
	void RenderDeferredReflections(FRHICommandListImmediate& RHICmdList, const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO, TRefCountPtr<IPooledRenderTarget>& VelocityRT);

	/** Render dynamic sky lighting from Movable sky lights. */
	void RenderDynamicSkyLighting(FRHICommandListImmediate& RHICmdList, const TRefCountPtr<IPooledRenderTarget>& VelocityTexture, TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO);

	/** Computes DFAO, modulates it to scene color (which is assumed to contain diffuse indirect lighting), and stores the output bent normal for use occluding specular. */
	void RenderDFAOAsIndirectShadowing(
		FRHICommandListImmediate& RHICmdList,
		const TRefCountPtr<IPooledRenderTarget>& VelocityTexture,
		TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO);

	/** Render Ambient Occlusion using mesh distance fields and the surface cache, which supports dynamic rigid meshes. */
	bool RenderDistanceFieldLighting(
		FRHICommandListImmediate& RHICmdList, 
		const class FDistanceFieldAOParameters& Parameters, 
		const TRefCountPtr<IPooledRenderTarget>& VelocityTexture,
		TRefCountPtr<IPooledRenderTarget>& OutDynamicBentNormalAO, 
		TRefCountPtr<IPooledRenderTarget>& OutDynamicIrradiance,
		bool bModulateToSceneColor,
		bool bVisualizeAmbientOcclusion,
		bool bVisualizeGlobalIllumination);

	/** Render Ambient Occlusion using mesh distance fields on a screen based grid. */
	void RenderDistanceFieldAOScreenGrid(
		FRHICommandListImmediate& RHICmdList, 
		const FViewInfo& View,
		FIntPoint TileListGroupSize,
		const FDistanceFieldAOParameters& Parameters, 
		const TRefCountPtr<IPooledRenderTarget>& VelocityTexture,
		const TRefCountPtr<IPooledRenderTarget>& DistanceFieldNormal, 
		TRefCountPtr<IPooledRenderTarget>& OutDynamicBentNormalAO, 
		TRefCountPtr<IPooledRenderTarget>& OutDynamicIrradiance);

	void RenderMeshDistanceFieldVisualization(FRHICommandListImmediate& RHICmdList, const FDistanceFieldAOParameters& Parameters);

	/** Whether tiled deferred is supported and can be used at all. */
	bool CanUseTiledDeferred() const;

	/** Whether to use tiled deferred shading given a number of lights that support it. */
	bool ShouldUseTiledDeferred(int32 NumUnshadowedLights, int32 NumSimpleLights) const;

	/** Renders the lights in SortedLights in the range [0, NumUnshadowedLights) using tiled deferred shading. */
	void RenderTiledDeferredLighting(FRHICommandListImmediate& RHICmdList, const TArray<FSortedLightSceneInfo, SceneRenderingAllocator>& SortedLights, int32 NumUnshadowedLights, const FSimpleLightArray& SimpleLights);

	/** Renders the scene's lighting. */
	void RenderLights(FRHICommandListImmediate& RHICmdList);

	/** Renders an array of lights for the stationary light overlap viewmode. */
	void RenderLightArrayForOverlapViewmode(FRHICommandListImmediate& RHICmdList, const TSparseArray<FLightSceneInfoCompact>& LightArray);

	/** Render stationary light overlap as complexity to scene color. */
	void RenderStationaryLightOverlap(FRHICommandListImmediate& RHICmdList);
	
	/** Issues a timestamp query for the beginning of the separate translucency pass. */
	void BeginTimingSeparateTranslucencyPass(FRHICommandListImmediate& RHICmdList, const FViewInfo& View);

	/** Issues a timestamp query for the end of the separate translucency pass. */
	void EndTimingSeparateTranslucencyPass(FRHICommandListImmediate& RHICmdList, const FViewInfo& View);


	/** Setup the downsampled view uniform buffer if it was not already built */
	void SetupDownsampledTranslucencyViewUniformBuffer(FRHICommandListImmediate& RHICmdList, FViewInfo& View);

	/** Resolve the scene color if any translucent material needs it. */
	void ConditionalResolveSceneColorForTranslucentMaterials(FRHICommandListImmediate& RHICmdList);

	/** Renders the scene's translucency. */
	void RenderTranslucency(FRHICommandListImmediate& RHICmdList, ETranslucencyPass::Type TranslucencyPass);

	// WaveWorks Start
	/** Renders the waveworks */
	void RenderWaveWorks(FRHICommandListImmediate& RHICmdList);
	// WaveWorks End

	/** Renders the scene's light shafts */
	void RenderLightShaftOcclusion(FRHICommandListImmediate& RHICmdList, FLightShaftsOutput& Output);

	void RenderLightShaftBloom(FRHICommandListImmediate& RHICmdList);

	/** Reuses an existing translucent shadow map if possible or re-renders one if necessary. */
	const FProjectedShadowInfo* PrepareTranslucentShadowMap(FRHICommandList& RHICmdList, const FViewInfo& View, FPrimitiveSceneInfo* PrimitiveSceneInfo, ETranslucencyPass::Type TranslucenyPassType);

	bool ShouldRenderVelocities() const;

	/** Renders the velocities of movable objects for the motion blur effect. */
	void RenderVelocities(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT);

	/** Renders the velocities for a subset of movable objects for the motion blur effect. */
	friend class FRenderVelocityDynamicThreadTask;
	void RenderDynamicVelocitiesMeshElementsInner(FRHICommandList& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, int32 FirstIndex, int32 LastIndex);

	/** Renders the velocities of movable objects for the motion blur effect. */
	void RenderVelocitiesInner(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT);
	void RenderVelocitiesInnerParallel(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT);

	/** Renders world-space lightmap density instead of the normal color. */
	bool RenderLightMapDensities(FRHICommandListImmediate& RHICmdList);

	/** Updates the downsized depth buffer with the current full resolution depth buffer. */
	void UpdateDownsampledDepthSurface(FRHICommandList& RHICmdList);

	/** Downsample the scene depth with a specified scale factor to a specified render target*/
	void DownsampleDepthSurface(FRHICommandList& RHICmdList, const FTexture2DRHIRef& RenderTarget, const FViewInfo& View, float ScaleFactor, bool bUseMaxDepth);

	void CopyStencilToLightingChannelTexture(FRHICommandList& RHICmdList);

	/** Injects reflective shadowmaps into LPVs */
	bool InjectReflectiveShadowMaps(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo);

	/** Renders capsule shadows for all per-object shadows using it for the given light. */
	bool RenderCapsuleDirectShadows(
		FRHICommandListImmediate& RHICmdList, 
		const FLightSceneInfo& LightSceneInfo, 
		IPooledRenderTarget* ScreenShadowMaskTexture,
		const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& CapsuleShadows, 
		bool bProjectingForForwardShading) const;

	/** Sets up ViewState buffers for rendering capsule shadows. */
	void SetupIndirectCapsuleShadows(
		FRHICommandListImmediate& RHICmdList, 
		const FViewInfo& View, 
		int32& NumCapsuleShapes, 
		int32& NumMeshesWithCapsules, 
		int32& NumMeshDistanceFieldCasters,
		FShaderResourceViewRHIParamRef& IndirectShadowLightDirectionSRV) const;

	/** Renders indirect shadows from capsules modulated onto scene color. */
	void RenderIndirectCapsuleShadows(
		FRHICommandListImmediate& RHICmdList, 
		FTextureRHIParamRef IndirectLightingTexture, 
		FTextureRHIParamRef ExistingIndirectOcclusionTexture) const;

	/** Renders capsule shadows for movable skylights, using the cone of visibility (bent normal) from DFAO. */
	void RenderCapsuleShadowsForMovableSkylight(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& BentNormalOutput) const;

	/** Render deferred projections of shadows for a given light into the light attenuation buffer. */
	bool RenderShadowProjections(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, IPooledRenderTarget* ScreenShadowMaskTexture, bool& bInjectedTranslucentVolume);

	/** Render shadow projections when forward rendering. */
	void RenderForwardShadingShadowProjections(FRHICommandListImmediate& RHICmdList);

	/**
	  * Used by RenderLights to render a light function to the attenuation buffer.
	  *
	  * @param LightSceneInfo Represents the current light
	  * @param LightIndex The light's index into FScene::Lights
	  */
	bool RenderLightFunction(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, IPooledRenderTarget* ScreenShadowMaskTexture, bool bLightAttenuationCleared, bool bProjectingForForwardShading);

	/** Renders a light function indicating that whole scene shadowing being displayed is for previewing only, and will go away in game. */
	bool RenderPreviewShadowsIndicator(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, IPooledRenderTarget* ScreenShadowMaskTexture, bool bLightAttenuationCleared);

	/** Renders a light function with the given material. */
	bool RenderLightFunctionForMaterial(
		FRHICommandListImmediate& RHICmdList, 
		const FLightSceneInfo* LightSceneInfo, 
		IPooledRenderTarget* ScreenShadowMaskTexture,
		const FMaterialRenderProxy* MaterialProxy, 
		bool bLightAttenuationCleared,
		bool bProjectingForForwardShading, 
		bool bRenderingPreviewShadowsIndicator);

	/**
	  * Used by RenderLights to render a light to the scene color buffer.
	  *
	  * @param LightSceneInfo Represents the current light
	  * @param LightIndex The light's index into FScene::Lights
	  * @return true if anything got rendered
	  */
	void RenderLight(FRHICommandList& RHICmdList, const FLightSceneInfo* LightSceneInfo, IPooledRenderTarget* ScreenShadowMaskTexture, bool bRenderOverlap, bool bIssueDrawEvent);

	/** Renders an array of simple lights using standard deferred shading. */
	void RenderSimpleLightsStandardDeferred(FRHICommandListImmediate& RHICmdList, const FSimpleLightArray& SimpleLights);

	/** Clears the translucency lighting volumes before light accumulation. */
	void ClearTranslucentVolumeLighting(FRHICommandListImmediate& RHICmdList);

	/** Clears the translucency lighting volume via an async compute shader overlapped with the basepass. */
	void ClearTranslucentVolumeLightingAsyncCompute(FRHICommandListImmediate& RHICmdList);

	/** Add AmbientCubemap to the lighting volumes. */
	void InjectAmbientCubemapTranslucentVolumeLighting(FRHICommandList& RHICmdList);

	/** Clears the volume texture used to accumulate per object shadows for translucency. */
	void ClearTranslucentVolumePerObjectShadowing(FRHICommandList& RHICmdList);

	/** Accumulates the per object shadow's contribution for translucency. */
	void AccumulateTranslucentVolumeObjectShadowing(FRHICommandList& RHICmdList, const FProjectedShadowInfo* InProjectedShadowInfo, bool bClearVolume);

	/** Accumulates direct lighting for the given light.  InProjectedShadowInfo can be NULL in which case the light will be unshadowed. */
	void InjectTranslucentVolumeLighting(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo& LightSceneInfo, const FProjectedShadowInfo* InProjectedShadowInfo);

	/** Accumulates direct lighting for an array of unshadowed lights. */
	void InjectTranslucentVolumeLightingArray(FRHICommandListImmediate& RHICmdList, const TArray<FSortedLightSceneInfo, SceneRenderingAllocator>& SortedLights, int32 NumLights);

	/** Accumulates direct lighting for simple lights. */
	void InjectSimpleTranslucentVolumeLightingArray(FRHICommandListImmediate& RHICmdList, const FSimpleLightArray& SimpleLights);

	/** Filters the translucency lighting volumes to reduce aliasing. */
	void FilterTranslucentVolumeLighting(FRHICommandListImmediate& RHICmdList);

	bool ShouldRenderVolumetricFog() const;

	void SetupVolumetricFog();

	void RenderLocalLightsForVolumetricFog(
		FRHICommandListImmediate& RHICmdList,
		FViewInfo& View,
		bool bUseTemporalReprojection,
		const struct FVolumetricFogIntegrationParameterData& IntegrationData,
		const FExponentialHeightFogSceneInfo& FogInfo,
		FIntVector VolumetricFogGridSize,
		FVector GridZParams,
		const FPooledRenderTargetDesc& VolumeDesc,
		TRefCountPtr<IPooledRenderTarget>& OutLocalShadowedLightScattering);

	void RenderLightFunctionForVolumetricFog(
		FRHICommandListImmediate& RHICmdList,
		FViewInfo& View,
		FIntVector VolumetricFogGridSize,
		float VolumetricFogMaxDistance,
		FMatrix& OutLightFunctionWorldToShadow,
		TRefCountPtr<IPooledRenderTarget>& OutLightFunctionTexture,
		bool& bOutUseDirectionalLightShadowing);

	void VoxelizeFogVolumePrimitives(
		FRHICommandListImmediate& RHICmdList,
		const FViewInfo& View,
		const FVolumetricFogIntegrationParameterData& IntegrationData,
		FIntVector VolumetricFogGridSize,
		FVector GridZParams,
		float VolumetricFogDistance);

	void ComputeVolumetricFog(FRHICommandListImmediate& RHICmdList);

	void VisualizeVolumetricLightmap(FRHICommandListImmediate& RHICmdList);

	/** Output SpecularColor * IndirectDiffuseGI for metals so they are not black in reflections */
	void RenderReflectionCaptureSpecularBounceForAllViews(FRHICommandListImmediate& RHICmdList);

	/** Render image based reflections (SSR, Env, SkyLight) with compute shaders */
	void RenderTiledDeferredImageBasedReflections(FRHICommandListImmediate& RHICmdList, const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO, TRefCountPtr<IPooledRenderTarget>& VelocityRT);

	/** Render image based reflections (SSR, Env, SkyLight) without compute shaders */
	void RenderStandardDeferredImageBasedReflections(FRHICommandListImmediate& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, bool bReflectionEnv, const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO, TRefCountPtr<IPooledRenderTarget>& VelocityRT);

	bool RenderDeferredPlanarReflections(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, bool bLightAccumulationIsInUse, TRefCountPtr<IPooledRenderTarget>& Output);

	bool ShouldDoReflectionEnvironment() const;
	
	bool ShouldRenderDistanceFieldAO() const;

	/** Whether distance field global data structures should be prepared for features that use it. */
	bool ShouldPrepareForDistanceFieldShadows() const;
	bool ShouldPrepareForDistanceFieldAO() const;
	bool ShouldPrepareForDFInsetIndirectShadow() const;

	bool ShouldPrepareDistanceFieldScene(
// NvFlow begin
		bool bCustomShouldPrepare = false
// NvFlow end
	) const;
	bool ShouldPrepareGlobalDistanceField(
// NvFlow begin
		bool bCustomShouldPrepare = false
// NvFlow end
	) const;

	void UpdateGlobalDistanceFieldObjectBuffers(FRHICommandListImmediate& RHICmdList);

	void RenderViewTranslucency(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, ETranslucencyPass::Type TranslucenyPass);
	void RenderViewTranslucencyParallel(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, ETranslucencyPass::Type TranslucencyPass);

	void CopySceneCaptureComponentToTarget(FRHICommandListImmediate& RHICmdList);

	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	void NVVolumetricLightingBeginAccumulation(FRHICommandListImmediate& RHICmdList);
	void NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& ShadowInfos);
	void NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, const FProjectedShadowInfo* ShadowInfo);
	void NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo);
	void NVVolumetricLightingEndAccumulation(FRHICommandListImmediate& RHICmdList);
	void NVVolumetricLightingApplyLighting(FRHICommandListImmediate& RHICmdList);
#endif
	
	// WaveWorks Start
	void DrawAllWaveWorksPasses(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState);
	// WaveWorks End

	friend class FTranslucentPrimSet;
};

DECLARE_STATS_GROUP(TEXT("Command List Markers"), STATGROUP_CommandListMarkers, STATCAT_Advanced);

DECLARE_CYCLE_STAT_EXTERN(TEXT("PrePass"), STAT_CLM_PrePass, STATGROUP_CommandListMarkers, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("DeferredShadingSceneRenderer AsyncSortBasePassStaticData"), STAT_FDeferredShadingSceneRenderer_AsyncSortBasePassStaticData, STATGROUP_SceneRendering, );
