// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DeferredShadingRenderer.cpp: Top level rendering loop for deferred shading
=============================================================================*/

#include "DeferredShadingRenderer.h"
#include "VelocityRendering.h"
#include "AtmosphereRendering.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/ScreenSpaceReflections.h"
#include "CompositionLighting/CompositionLighting.h"
#include "FXSystem.h"
#include "OneColorShader.h"
#include "CompositionLighting/PostProcessDeferredDecals.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "GlobalDistanceField.h"
#include "PostProcess/PostProcessing.h"
#include "DistanceFieldAtlas.h"
#include "EngineModule.h"
#include "SceneViewExtension.h"
#include "GPUSkinCache.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
// @third party code - BEGIN HairWorks
#include "HairWorksRenderer.h"
// @third party code - END HairWorks

// NvFlow begin
#include "GameWorks/RendererHooksNvFlow.h"
// NvFlow end

// NVCHANGE_BEGIN: Add HBAO+
#if WITH_GFSDK_SSAO
#include "GFSDK_SSAO.h"
#endif
// NVCHANGE_END: Add HBAO+
TAutoConsoleVariable<int32> CVarEarlyZPass(
	TEXT("r.EarlyZPass"),
	3,	
	TEXT("Whether to use a depth only pass to initialize Z culling for the base pass. Cannot be changed at runtime.\n")
	TEXT("Note: also look at r.EarlyZPassMovable\n")
	TEXT("  0: off\n")
	TEXT("  1: good occluders only: not masked, and large on screen\n")
	TEXT("  2: all opaque (including masked)\n")
	TEXT("  x: use built in heuristic (default is 3)"),
	ECVF_Scalability);

int32 GEarlyZPassMovable = 1;

/** Affects static draw lists so must reload level to propagate. */
static FAutoConsoleVariableRef CVarEarlyZPassMovable(
	TEXT("r.EarlyZPassMovable"),
	GEarlyZPassMovable,
	TEXT("Whether to render movable objects into the depth only pass. Defaults to on.\n")
	TEXT("Note: also look at r.EarlyZPass"),
	ECVF_RenderThreadSafe | ECVF_Scalability
	);

/** Affects BasePassPixelShader.usf so must relaunch editor to recompile shaders. */
static TAutoConsoleVariable<int32> CVarEarlyZPassOnlyMaterialMasking(
	TEXT("r.EarlyZPassOnlyMaterialMasking"),
	0,
	TEXT("Whether to compute materials' mask opacity only in early Z pass. Changing this setting requires restarting the editor.\n")
	TEXT("Note: Needs r.EarlyZPass == 2 && r.EarlyZPassMovable == 1"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	);

static TAutoConsoleVariable<int32> CVarStencilForLODDither(
	TEXT("r.StencilForLODDither"),
	0,
	TEXT("Whether to use stencil tests in the prepass, and depth-equal tests in the base pass to implement LOD dithering.\n")
	TEXT("If disabled, LOD dithering will be done through clip() instructions in the prepass and base pass, which disables EarlyZ.\n")
	TEXT("Forces a full prepass when enabled."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

TAutoConsoleVariable<int32> CVarCustomDepthOrder(
	TEXT("r.CustomDepth.Order"),
	1,	
	TEXT("When CustomDepth (and CustomStencil) is getting rendered\n")
	TEXT("  0: Before GBuffer (can be more efficient with AsyncCompute, allows using it in DBuffer pass, no GBuffer blending decals allow GBuffer compression)\n")
	TEXT("  1: After Base Pass (default)"),
	ECVF_RenderThreadSafe);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<int32> CVarVisualizeTexturePool(
	TEXT("r.VisualizeTexturePool"),
	0,
	TEXT("Allows to enable the visualize the texture pool (currently only on console).\n")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: on"),
	ECVF_Cheat | ECVF_RenderThreadSafe);
#endif

static TAutoConsoleVariable<int32> CVarClearCoatNormal(
	TEXT("r.ClearCoatNormal"),
	0,
	TEXT("0 to disable clear coat normal.\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarFXSystemPreRenderAfterPrepass(
	TEXT("r.FXSystemPreRenderAfterPrepass"),
	0,
	TEXT("If > 0, then do the FX prerender after the prepass. This improves pipelining for greater performance. Experiemental option."),
	ECVF_RenderThreadSafe
	);

// NVCHANGE_BEGIN: Add HBAO+
#if WITH_GFSDK_SSAO

static TAutoConsoleVariable<int32> CVarHBAOEnable(
	TEXT("r.HBAO.Enable"),
	0,
	TEXT("Enable HBAO+"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarHBAOHighPrecisionDepth(
	TEXT("r.HBAO.HighPrecisionDepth"),
	0,
	TEXT("0: use FP16 for internal depth storage in HBAO+")
	TEXT("1: use FP32 for internal depth storage. Use this option to avoid self-occlusion bands on objects far away."),
	ECVF_RenderThreadSafe);

#endif
// NVCHANGE_END: Add HBAO+

int32 GbEnableAsyncComputeTranslucencyLightingVolumeClear = 1;
static FAutoConsoleVariableRef CVarEnableAsyncComputeTranslucencyLightingVolumeClear(
	TEXT("r.EnableAsyncComputeTranslucencyLightingVolumeClear"),
	GbEnableAsyncComputeTranslucencyLightingVolumeClear,
	TEXT("Whether to clear the translucency lighting volume using async compute.\n"),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<int32> CVarBasePassWriteDepthEvenWithFullPrepass(
	TEXT("r.BasePassWriteDepthEvenWithFullPrepass"),
	0,
	TEXT("0 to allow a readonly base pass, which skips an MSAA depth resolve, and allows masked materials to get EarlyZ (writing to depth while doing clip() disables EarlyZ) (default)\n")
	TEXT("1 to force depth writes in the base pass.  Useful for debugging when the prepass and base pass don't match what they render."));

DECLARE_CYCLE_STAT(TEXT("PostInitViews FlushDel"), STAT_PostInitViews_FlushDel, STATGROUP_InitViews);
DECLARE_CYCLE_STAT(TEXT("InitViews Intentional Stall"), STAT_InitViews_Intentional_Stall, STATGROUP_InitViews);

DEFINE_STAT(STAT_FDeferredShadingSceneRenderer_AsyncSortBasePassStaticData);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer UpdateDownsampledDepthSurface"), STAT_FDeferredShadingSceneRenderer_UpdateDownsampledDepthSurface, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer Render Init"), STAT_FDeferredShadingSceneRenderer_Render_Init, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer Render ServiceLocalQueue"), STAT_FDeferredShadingSceneRenderer_Render_ServiceLocalQueue, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer DistanceFieldAO Init"), STAT_FDeferredShadingSceneRenderer_DistanceFieldAO_Init, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer FGlobalDynamicVertexBuffer Commit"), STAT_FDeferredShadingSceneRenderer_FGlobalDynamicVertexBuffer_Commit, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer FXSystem PreRender"), STAT_FDeferredShadingSceneRenderer_FXSystem_PreRender, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer AllocGBufferTargets"), STAT_FDeferredShadingSceneRenderer_AllocGBufferTargets, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer ClearLPVs"), STAT_FDeferredShadingSceneRenderer_ClearLPVs, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer DBuffer"), STAT_FDeferredShadingSceneRenderer_DBuffer, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer SetAndClearViewGBuffer"), STAT_FDeferredShadingSceneRenderer_SetAndClearViewGBuffer, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer ClearGBufferAtMaxZ"), STAT_FDeferredShadingSceneRenderer_ClearGBufferAtMaxZ, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer ResolveDepth After Basepass"), STAT_FDeferredShadingSceneRenderer_ResolveDepth_After_Basepass, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer Resolve After Basepass"), STAT_FDeferredShadingSceneRenderer_Resolve_After_Basepass, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer FXSystem PostRenderOpaque"), STAT_FDeferredShadingSceneRenderer_FXSystem_PostRenderOpaque, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer AfterBasePass"), STAT_FDeferredShadingSceneRenderer_AfterBasePass, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer Lighting"), STAT_FDeferredShadingSceneRenderer_Lighting, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer RenderLightShaftOcclusion"), STAT_FDeferredShadingSceneRenderer_RenderLightShaftOcclusion, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer RenderAtmosphere"), STAT_FDeferredShadingSceneRenderer_RenderAtmosphere, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer RenderFog"), STAT_FDeferredShadingSceneRenderer_RenderFog, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer RenderLightShaftBloom"), STAT_FDeferredShadingSceneRenderer_RenderLightShaftBloom, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer RenderFinish"), STAT_FDeferredShadingSceneRenderer_RenderFinish, STATGROUP_SceneRendering);

DECLARE_CYCLE_STAT(TEXT("OcclusionSubmittedFence Dispatch"), STAT_OcclusionSubmittedFence_Dispatch, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("OcclusionSubmittedFence Wait"), STAT_OcclusionSubmittedFence_Wait, STATGROUP_SceneRendering);

DECLARE_FLOAT_COUNTER_STAT(TEXT("Postprocessing"), Stat_GPU_Postprocessing, STATGROUP_GPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("HZB"), Stat_GPU_HZB, STATGROUP_GPU);
DECLARE_FLOAT_COUNTER_STAT(TEXT("[unaccounted]"), Stat_GPU_Unaccounted, STATGROUP_GPU);

FForwardLightingViewResources* GetMinimalDummyForwardLightingResources();

bool ShouldForceFullDepthPass(ERHIFeatureLevel::Type FeatureLevel)
{
	static IConsoleVariable* CDBufferVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DBuffer"));
	const bool bDBufferAllowed = CDBufferVar ? CDBufferVar->GetInt() != 0 : false;

	static const auto StencilLODDitherCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.StencilForLODDither"));
	const bool bStencilLODDither = StencilLODDitherCVar->GetValueOnAnyThread() != 0;

	const bool bEarlyZMaterialMasking = CVarEarlyZPassOnlyMaterialMasking.GetValueOnAnyThread() != 0;

	// Note: ShouldForceFullDepthPass affects which static draw lists meshes go into, so nothing it depends on can change at runtime, unless you do a FGlobalComponentRecreateRenderStateContext to propagate the cvar change
	return bDBufferAllowed || bStencilLODDither || bEarlyZMaterialMasking || IsForwardShadingEnabled(FeatureLevel) || UseSelectiveBasePassOutputs();
}

void GetEarlyZPassMode(ERHIFeatureLevel::Type FeatureLevel, EDepthDrawingMode& EarlyZPassMode, bool& bEarlyZPassMovable)
{
	EarlyZPassMode = DDM_NonMaskedOnly;
	bEarlyZPassMovable = false;

	// developer override, good for profiling, can be useful as project setting
	{
		const int32 CVarValue = CVarEarlyZPass.GetValueOnAnyThread();

		switch(CVarValue)
		{
			case 0: EarlyZPassMode = DDM_None; break;
			case 1: EarlyZPassMode = DDM_NonMaskedOnly; break;
			case 2: EarlyZPassMode = DDM_AllOccluders; break;
			case 3: break;	// Note: 3 indicates "default behavior" and does not specify an override
		}
	}

	if (ShouldForceFullDepthPass(FeatureLevel))
	{
		// DBuffer decals and stencil LOD dithering force a full prepass
		EarlyZPassMode = DDM_AllOpaque;
		bEarlyZPassMovable = true;
	}
}

const TCHAR* GetDepthPassReason(bool bDitheredLODTransitionsUseStencil, ERHIFeatureLevel::Type FeatureLevel)
{
	if (IsForwardShadingEnabled(FeatureLevel))
	{
		return TEXT("(Forced by ForwardShading)");
	}

	static IConsoleVariable* CDBufferVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DBuffer"));
	bool bDBufferAllowed = CDBufferVar ? CDBufferVar->GetInt() != 0 : false;

	if (bDBufferAllowed)
	{
		return TEXT("(Forced by DBuffer)");
	}

	if (bDitheredLODTransitionsUseStencil)
	{
		return TEXT("(Forced by StencilLODDither)");
	}

	return TEXT("");
}

/*-----------------------------------------------------------------------------
	FDeferredShadingSceneRenderer
-----------------------------------------------------------------------------*/

FDeferredShadingSceneRenderer::FDeferredShadingSceneRenderer(const FSceneViewFamily* InViewFamily,FHitProxyConsumer* HitProxyConsumer)
	: FSceneRenderer(InViewFamily, HitProxyConsumer)
{
	static const auto StencilLODDitherCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.StencilForLODDither"));
	bDitheredLODTransitionsUseStencil = StencilLODDitherCVar->GetValueOnAnyThread() != 0;

	GetEarlyZPassMode(FeatureLevel, EarlyZPassMode, bEarlyZPassMovable);

	// Shader complexity requires depth only pass to display masked material cost correctly
	if (ViewFamily.UseDebugViewPS() && ViewFamily.GetDebugViewShaderMode() != DVSM_OutputMaterialTextureScales)
	{
		EarlyZPassMode = DDM_AllOpaque;
		bEarlyZPassMovable = true;
	}

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	InitVxgiRenderingState(InViewFamily);
	InitVxgiView();
#endif
	// NVCHANGE_END: Add VXGI
}

float GetSceneColorClearAlpha()
{
	// Scene color alpha is used during scene captures and planar reflections.  1 indicates background should be shown, 0 indicates foreground is fully present.
	return 1.0f;
}

/** 
* Clears view where Z is still at the maximum value (ie no geometry rendered)
*/
void FDeferredShadingSceneRenderer::ClearGBufferAtMaxZ(FRHICommandList& RHICmdList)
{
	// Assumes BeginRenderingSceneColor() has been called before this function
	SCOPED_DRAW_EVENT(RHICmdList, ClearGBufferAtMaxZ);

	// Clear the G Buffer render targets
	const bool bClearBlack = Views[0].Family->EngineShowFlags.ShaderComplexity || Views[0].Family->EngineShowFlags.StationaryLightOverlap;
	const float ClearAlpha = GetSceneColorClearAlpha();
	const FLinearColor ClearColor = bClearBlack ? FLinearColor(0, 0, 0, ClearAlpha) : FLinearColor(Views[0].BackgroundColor.R, Views[0].BackgroundColor.G, Views[0].BackgroundColor.B, ClearAlpha);
	FLinearColor ClearColors[MaxSimultaneousRenderTargets] = 
		{ClearColor, FLinearColor(0.5f,0.5f,0.5f,0), FLinearColor(0,0,0,1), FLinearColor(0,0,0,0), FLinearColor(0,1,1,1), FLinearColor(1,1,1,1), FLinearColor::Transparent, FLinearColor::Transparent};

	uint32 NumActiveRenderTargets = FSceneRenderTargets::Get(RHICmdList).GetNumGBufferTargets();
	
	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

	TShaderMapRef<TOneColorVS<true> > VertexShader(ShaderMap);
	FOneColorPS* PixelShader = NULL; 

	// Assume for now all code path supports SM4, otherwise render target numbers are changed
	switch(NumActiveRenderTargets)
	{
	case 5:
		{
			TShaderMapRef<TOneColorPixelShaderMRT<5> > MRTPixelShader(ShaderMap);
			PixelShader = *MRTPixelShader;
		}
		break;
	case 6:
		{
			TShaderMapRef<TOneColorPixelShaderMRT<6> > MRTPixelShader(ShaderMap);
			PixelShader = *MRTPixelShader;
		}
		break;
	default:
	case 1:
		{
			TShaderMapRef<TOneColorPixelShaderMRT<1> > MRTPixelShader(ShaderMap);
			PixelShader = *MRTPixelShader;
		}
		break;
	}

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// Opaque rendering, depth test but no depth writes
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	// Clear each viewport by drawing background color at MaxZ depth
	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("ClearView%d"), ViewIndex);

		FViewInfo& View = Views[ViewIndex];

		// Set viewport for this view
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);

		// Setup PS
		PixelShader->SetColors(RHICmdList, ClearColors, NumActiveRenderTargets);

		// Render quad
		static const FVector4 ClearQuadVertices[4] = 
		{
			FVector4( -1.0f,  1.0f, (float)ERHIZBuffer::FarPlane, 1.0f),
			FVector4(  1.0f,  1.0f, (float)ERHIZBuffer::FarPlane, 1.0f ),
			FVector4( -1.0f, -1.0f, (float)ERHIZBuffer::FarPlane, 1.0f ),
			FVector4(  1.0f, -1.0f, (float)ERHIZBuffer::FarPlane, 1.0f )
		};
		DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, ClearQuadVertices, sizeof(ClearQuadVertices[0]));
	}
}

/** Render the TexturePool texture */
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void FDeferredShadingSceneRenderer::RenderVisualizeTexturePool(FRHICommandListImmediate& RHICmdList)
{
	TRefCountPtr<IPooledRenderTarget> VisualizeTexturePool;

	/** Resolution for the texture pool visualizer texture. */
	enum
	{
		TexturePoolVisualizerSizeX = 280,
		TexturePoolVisualizerSizeY = 140,
	};

	FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(FIntPoint(TexturePoolVisualizerSizeX, TexturePoolVisualizerSizeY), PF_B8G8R8A8, FClearValueBinding::None, TexCreate_None, TexCreate_None, false));
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, VisualizeTexturePool, TEXT("VisualizeTexturePool"));
	
	uint32 Pitch;
	FColor* TextureData = (FColor*)RHICmdList.LockTexture2D((FTexture2DRHIRef&)VisualizeTexturePool->GetRenderTargetItem().ShaderResourceTexture, 0, RLM_WriteOnly, Pitch, false);
	if(TextureData)
	{
		// clear with grey to get reliable background color
		FMemory::Memset(TextureData, 0x88, TexturePoolVisualizerSizeX * TexturePoolVisualizerSizeY * 4);
		RHICmdList.GetTextureMemoryVisualizeData(TextureData, TexturePoolVisualizerSizeX, TexturePoolVisualizerSizeY, Pitch, 4096);
	}

	RHICmdList.UnlockTexture2D((FTexture2DRHIRef&)VisualizeTexturePool->GetRenderTargetItem().ShaderResourceTexture, 0, false);

	FIntPoint RTExtent = FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY();

	FVector2D Tex00 = FVector2D(0, 0);
	FVector2D Tex11 = FVector2D(1, 1);

//todo	VisualizeTexture(*VisualizeTexturePool, ViewFamily.RenderTarget, FIntRect(0, 0, RTExtent.X, RTExtent.Y), RTExtent, 1.0f, 0.0f, 0.0f, Tex00, Tex11, 1.0f, false);
}
#endif


/** 
* Finishes the view family rendering.
*/
void FDeferredShadingSceneRenderer::RenderFinish(FRHICommandListImmediate& RHICmdList)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	{
		if(CVarVisualizeTexturePool.GetValueOnRenderThread())
		{
			RenderVisualizeTexturePool(RHICmdList);
		}
	}

#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	FSceneRenderer::RenderFinish(RHICmdList);

	// Some RT should be released as early as possible to allow sharing of that memory for other purposes.
	// SceneColor is be released in tone mapping, if not we want to get access to the HDR scene color after this pass so we keep it.
	// This becomes even more important with some limited VRam (XBoxOne).
	FSceneRenderTargets::Get(RHICmdList).SetLightAttenuation(0);
}

void BuildHZB( FRHICommandListImmediate& RHICmdList, FViewInfo& View );

/** 
* Renders the view family. 
*/

DEFINE_STAT(STAT_CLM_PrePass);
DECLARE_CYCLE_STAT(TEXT("FXPreRender"), STAT_CLM_FXPreRender, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterPrePass"), STAT_CLM_AfterPrePass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("BasePass"), STAT_CLM_BasePass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterBasePass"), STAT_CLM_AfterBasePass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Lighting"), STAT_CLM_Lighting, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterLighting"), STAT_CLM_AfterLighting, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Translucency"), STAT_CLM_Translucency, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("RenderDistortion"), STAT_CLM_RenderDistortion, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterTranslucency"), STAT_CLM_AfterTranslucency, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("RenderDistanceFieldLighting"), STAT_CLM_RenderDistanceFieldLighting, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("LightShaftBloom"), STAT_CLM_LightShaftBloom, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("PostProcessing"), STAT_CLM_PostProcessing, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Velocity"), STAT_CLM_Velocity, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterVelocity"), STAT_CLM_AfterVelocity, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("RenderFinish"), STAT_CLM_RenderFinish, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterFrame"), STAT_CLM_AfterFrame, STATGROUP_CommandListMarkers);

FGraphEventRef FDeferredShadingSceneRenderer::OcclusionSubmittedFence[FOcclusionQueryHelpers::MaxBufferedOcclusionFrames];
FGraphEventRef FDeferredShadingSceneRenderer::TranslucencyTimestampQuerySubmittedFence[FOcclusionQueryHelpers::MaxBufferedOcclusionFrames + 1];

/**
 * Returns true if the depth Prepass needs to run
 */
static FORCEINLINE bool NeedsPrePass(const FDeferredShadingSceneRenderer* Renderer)
{
	return (RHIHasTiledGPU(Renderer->ViewFamily.GetShaderPlatform()) == false) && 
		(Renderer->EarlyZPassMode != DDM_None || Renderer->bEarlyZPassMovable != 0);
}

static void SetAndClearViewGBuffer(FRHICommandListImmediate& RHICmdList, FViewInfo& View, FExclusiveDepthStencil::Type DepthStencilAccess, bool bClearDepth)
{
	// if we didn't to the prepass above, then we will need to clear now, otherwise, it's already been cleared and rendered to
	ERenderTargetLoadAction DepthLoadAction = bClearDepth ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad;

	const bool bClearBlack = View.Family->EngineShowFlags.ShaderComplexity || View.Family->EngineShowFlags.StationaryLightOverlap;
	const float ClearAlpha = GetSceneColorClearAlpha();
	const FLinearColor ClearColor = bClearBlack ? FLinearColor(0, 0, 0, ClearAlpha) : FLinearColor(View.BackgroundColor.R, View.BackgroundColor.G, View.BackgroundColor.B, ClearAlpha);

	// clearing the GBuffer
	FSceneRenderTargets::Get(RHICmdList).BeginRenderingGBuffer(RHICmdList, ERenderTargetLoadAction::EClear, DepthLoadAction, DepthStencilAccess, View.Family->EngineShowFlags.ShaderComplexity, ClearColor);
}

bool FDeferredShadingSceneRenderer::RenderHzb(FRHICommandListImmediate& RHICmdList)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_HZB);

	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SceneContext.GetSceneDepthSurface());

	static const auto ICVarHZBOcc = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HZBOcclusion"));
	bool bHZBOcclusion = ICVarHZBOcc->GetInt() != 0;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		FSceneViewState* ViewState = (FSceneViewState*)View.State;

		const uint32 bSSR = ShouldRenderScreenSpaceReflections(View);
		const bool bSSAO = ShouldRenderScreenSpaceAmbientOcclusion(View);

		if (bSSAO || bHZBOcclusion || bSSR)
		{
			BuildHZB(RHICmdList, Views[ViewIndex]);
		}

		if (bHZBOcclusion && ViewState && ViewState->HZBOcclusionTests.GetNum() != 0)
		{
			check(ViewState->HZBOcclusionTests.IsValidFrame(ViewState->OcclusionFrameCounter));

			SCOPED_DRAW_EVENT(RHICmdList, HZB);
			ViewState->HZBOcclusionTests.Submit(RHICmdList, View);
		}
	}

	//async ssao only requires HZB and depth as inputs so get started ASAP
	if (GCompositionLighting.CanProcessAsyncSSAO(Views))
	{
		GCompositionLighting.ProcessAsyncSSAO(RHICmdList, Views);
	}

	return bHZBOcclusion;
}

void FDeferredShadingSceneRenderer::RenderOcclusion(FRHICommandListImmediate& RHICmdList)
{		
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_HZB);

	{
		// Update the quarter-sized depth buffer with the current contents of the scene depth texture.
		// This needs to happen before occlusion tests, which makes use of the small depth buffer.
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_UpdateDownsampledDepthSurface);
		UpdateDownsampledDepthSurface(RHICmdList);
	}
		
	// Issue occlusion queries
	// This is done after the downsampled depth buffer is created so that it can be used for issuing queries
	BeginOcclusionTests(RHICmdList, true);
}

void FDeferredShadingSceneRenderer::FinishOcclusion(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_HZB);

	// Hint to the RHI to submit commands up to this point to the GPU if possible.  Can help avoid CPU stalls next frame waiting
	// for these query results on some platforms.
	RHICmdList.SubmitCommandsHint();

	if (IsRunningRHIInSeparateThread())
	{
		SCOPE_CYCLE_COUNTER(STAT_OcclusionSubmittedFence_Dispatch);
		int32 NumFrames = FOcclusionQueryHelpers::GetNumBufferedFrames();
		for (int32 Dest = 1; Dest < NumFrames; Dest++)
		{
			CA_SUPPRESS(6385);
			OcclusionSubmittedFence[Dest] = OcclusionSubmittedFence[Dest - 1];
		}
		OcclusionSubmittedFence[0] = RHICmdList.RHIThreadFence();
		RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
	}
}
// The render thread is involved in sending stuff to the RHI, so we will periodically service that queue
void ServiceLocalQueue()
{
	SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_Render_ServiceLocalQueue);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::RenderThread_Local);
}

// @return 0/1
static int32 GetCustomDepthPassLocation()
{		
	return FMath::Clamp(CVarCustomDepthOrder.GetValueOnRenderThread(), 0, 1);
}

extern bool IsLpvIndirectPassRequired(const FViewInfo& View);

static TAutoConsoleVariable<float> CVarStallInitViews(
	TEXT("CriticalPathStall.AfterInitViews"),
	0.0f,
	TEXT("Sleep for the given time after InitViews. Time is given in ms. This is a debug option used for critical path analysis and forcing a change in the critical path."));

void FDeferredShadingSceneRenderer::Render(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_Render, FColor::Emerald);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	
	//make sure all the targets we're going to use will be safely writable.
	GRenderTargetPool.TransitionTargetsWritable(RHICmdList);

	// this way we make sure the SceneColor format is the correct one and not the one from the end of frame before
	SceneContext.ReleaseSceneColor();

	bool bDBuffer = IsDBufferEnabled();	

	if (IsRunningRHIInSeparateThread())
	{
		SCOPE_CYCLE_COUNTER(STAT_OcclusionSubmittedFence_Wait);
		int32 BlockFrame = FOcclusionQueryHelpers::GetNumBufferedFrames() - 1;
		FRHICommandListExecutor::WaitOnRHIThreadFence(OcclusionSubmittedFence[BlockFrame]);
		OcclusionSubmittedFence[BlockFrame] = nullptr;
	}

	if (!ViewFamily.EngineShowFlags.Rendering)
	{
		return;
	}
	SCOPED_DRAW_EVENT(RHICmdList, Scene);

	// Anything rendered inside Render() which isn't accounted for will fall into this stat
	// This works because child stat events do not contribute to their parents' times (see GPU_STATS_CHILD_TIMES_INCLUDED)
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_Unaccounted);
	
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_Render_Init);

		// Initialize global system textures (pass-through if already initialized).
		GSystemTextures.InitializeTextures(RHICmdList, FeatureLevel);

		// Allocate the maximum scene render target space for the current view family.
		SceneContext.Allocate(RHICmdList, ViewFamily);
	}
	SceneContext.AllocDummyGBufferTargets(RHICmdList);

	FGraphEventArray SortEvents;
	FILCUpdatePrimTaskData ILCTaskData;

	// Find the visible primitives.
	bool bDoInitViewAftersPrepass = InitViews(RHICmdList, ILCTaskData, SortEvents);

	for (int32 ViewExt = 0; ViewExt < ViewFamily.ViewExtensions.Num(); ++ViewExt)
	{
		ViewFamily.ViewExtensions[ViewExt]->PostInitViewFamily_RenderThread(RHICmdList, ViewFamily);
		for (int32 ViewIndex = 0; ViewIndex < ViewFamily.Views.Num(); ++ViewIndex)
		{
			ViewFamily.ViewExtensions[ViewExt]->PostInitView_RenderThread(RHICmdList, Views[ViewIndex]);
		}
	}

	TGuardValue<bool> LockDrawLists(GDrawListsLocked, true);
#if !UE_BUILD_SHIPPING
	if (CVarStallInitViews.GetValueOnRenderThread() > 0.0f)
	{
		SCOPE_CYCLE_COUNTER(STAT_InitViews_Intentional_Stall);
		FPlatformProcess::Sleep(CVarStallInitViews.GetValueOnRenderThread() / 1000.0f);
	}
#endif

	if (GRHICommandList.UseParallelAlgorithms())
	{
		// there are dynamic attempts to get this target during parallel rendering
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			Views[ViewIndex].GetEyeAdaptation(RHICmdList);
		}	
	}

	if (ShouldPrepareDistanceFieldScene(
		// NvFlow begin
		GRendererNvFlowHooks && GRendererNvFlowHooks->NvFlowUsesGlobalDistanceField()
		// NvFlow end
	))
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_DistanceFieldAO_Init);
		GDistanceFieldVolumeTextureAtlas.UpdateAllocations();
		UpdateGlobalDistanceFieldObjectBuffers(RHICmdList);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			Views[ViewIndex].HeightfieldLightingViewInfo.SetupVisibleHeightfields(Views[ViewIndex], RHICmdList);

			if (ShouldPrepareGlobalDistanceField(
				// NvFlow begin
				GRendererNvFlowHooks && GRendererNvFlowHooks->NvFlowUsesGlobalDistanceField()
				// NvFlow end
			))
			{
				float OcclusionMaxDistance = Scene->DefaultMaxDistanceFieldOcclusionDistance;

				// Use the skylight's max distance if there is one
				if (Scene->SkyLight && Scene->SkyLight->bCastShadows && !Scene->SkyLight->bWantsStaticShadowing)
				{
					OcclusionMaxDistance = Scene->SkyLight->OcclusionMaxDistance;
				}

				UpdateGlobalDistanceFieldVolume(RHICmdList, Views[ViewIndex], Scene, OcclusionMaxDistance, Views[ViewIndex].GlobalDistanceFieldInfo);
			}
		}	
	}

	if (IsRunningRHIInSeparateThread())
	{
		// we will probably stall on occlusion queries, so might as well have the RHI thread and GPU work while we wait.
		SCOPE_CYCLE_COUNTER(STAT_PostInitViews_FlushDel);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
		
		extern RHI_API void FlushPipelineStateCache();
		FlushPipelineStateCache();
	}

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;
	static const auto ClearMethodCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ClearSceneMethod"));
	bool bRequiresRHIClear = true;
	bool bRequiresFarZQuadClear = false;

	const bool bUseGBuffer = IsUsingGBuffers(GetFeatureLevelShaderPlatform(FeatureLevel));
	const bool bRenderDeferredLighting = ViewFamily.EngineShowFlags.Lighting
		&& FeatureLevel >= ERHIFeatureLevel::SM4
		&& ViewFamily.EngineShowFlags.DeferredLighting
		&& bUseGBuffer;

	bool bComputeLightGrid = false;
	if (bUseGBuffer)
	{
		bComputeLightGrid = bRenderDeferredLighting;
	}
	else
	{
		bComputeLightGrid = ViewFamily.EngineShowFlags.Lighting;
	}

	bComputeLightGrid |= (
		ShouldRenderVolumetricFog() ||
		ViewFamily.ViewMode != VMI_Lit);

	if (ClearMethodCVar)
	{
		int32 ClearMethod = ClearMethodCVar->GetValueOnRenderThread();

		if (ClearMethod == 0 && !ViewFamily.EngineShowFlags.Game)
		{
			// Do not clear the scene only if the view family is in game mode.
			ClearMethod = 1;
		}

		switch (ClearMethod)
		{
		case 0: // No clear
			{
				bRequiresRHIClear = false;
				bRequiresFarZQuadClear = false;
				break;
			}
		
		case 1: // RHICmdList.Clear
			{
				bRequiresRHIClear = true;
				bRequiresFarZQuadClear = false;
				break;
			}

		case 2: // Clear using far-z quad
			{
				bRequiresFarZQuadClear = true;
				bRequiresRHIClear = false;
				break;
			}
		}
	}

	// Always perform a full buffer clear for wireframe, shader complexity view mode, and stationary light overlap viewmode.
	if (bIsWireframe || ViewFamily.EngineShowFlags.ShaderComplexity || ViewFamily.EngineShowFlags.StationaryLightOverlap)
	{
		bRequiresRHIClear = true;
	}

	// force using occ queries for wireframe if rendering is parented or frozen in the first view
	check(Views.Num());
	#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const bool bIsViewFrozen = false;
		const bool bHasViewParent = false;
	#else
		const bool bIsViewFrozen = Views[0].State && ((FSceneViewState*)Views[0].State)->bIsFrozen;
		const bool bHasViewParent = Views[0].State && ((FSceneViewState*)Views[0].State)->HasViewParent();
	#endif

	
	const bool bIsOcclusionTesting = DoOcclusionQueries(FeatureLevel) && (!bIsWireframe || bIsViewFrozen || bHasViewParent);

	// Dynamic vertex and index buffers need to be committed before rendering.
	if (!bDoInitViewAftersPrepass)
	{
		{
			SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_FGlobalDynamicVertexBuffer_Commit);
			FGlobalDynamicVertexBuffer::Get().Commit();
			FGlobalDynamicIndexBuffer::Get().Commit();
		}
	}

	// NvFlow begin
	if (GRendererNvFlowHooks)
	{
		GRendererNvFlowHooks->NvFlowUpdateScene(RHICmdList, Scene->Primitives, &Views[0].GlobalDistanceFieldInfo.ParameterData);
	}
	// NvFlow end

	// Notify the FX system that the scene is about to be rendered.
	bool bLateFXPrerender = CVarFXSystemPreRenderAfterPrepass.GetValueOnRenderThread() > 0;
	bool bDoFXPrerender = Scene->FXSystem && Views.IsValidIndex(0) && !Views[0].bIsPlanarReflection;
	if (!bLateFXPrerender && bDoFXPrerender)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_FXSystem_PreRender);
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_FXPreRender));
		Scene->FXSystem->PreRender(RHICmdList, &Views[0].GlobalDistanceFieldInfo.ParameterData);
	}

	bool bDidAfterTaskWork = false;
	auto AfterTasksAreStarted = [&bDidAfterTaskWork, bDoInitViewAftersPrepass, this, &RHICmdList, &ILCTaskData, &SortEvents, bLateFXPrerender, bDoFXPrerender]()
	{
		if (!bDidAfterTaskWork)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_AfterPrepassTasksWork);
			bDidAfterTaskWork = true; // only do this once
			if (bDoInitViewAftersPrepass)
			{
				InitViewsPossiblyAfterPrepass(RHICmdList, ILCTaskData, SortEvents);
				{
					SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_FGlobalDynamicVertexBuffer_Commit);
					FGlobalDynamicVertexBuffer::Get().Commit();
					FGlobalDynamicIndexBuffer::Get().Commit();
				}
				ServiceLocalQueue();
			}
			if (bLateFXPrerender && bDoFXPrerender)
			{
				SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_FXSystem_PreRender);
				RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_FXPreRender));
				Scene->FXSystem->PreRender(RHICmdList, &Views[0].GlobalDistanceFieldInfo.ParameterData);
				ServiceLocalQueue();
			}
		}
	};
#if WITH_FLEX
	GFlexFluidSurfaceRenderer.UpdateProxiesAndResources(RHICmdList, Views[0].DynamicMeshElements, SceneContext);
#endif// @third party code - BEGIN HairWorks
	// Prepare hair rendering
	if (!IsForwardShadingEnabled(FeatureLevel))
	{
		// Do hair simulation
		{
			SCOPED_DRAW_EVENT(RHICmdList, HairSimulation);
			HairWorksRenderer::StepSimulation(RHICmdList, ViewFamily.CurrentWorldTime, ViewFamily.DeltaWorldTime);	 // Must be called before pin meshes are drawn. 
		}

		// Allocate hair render targets
		static auto* AlwaysCreateRenderTargets = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HairWorks.AlwaysCreateRenderTargets"));
		if ((!AlwaysCreateRenderTargets->GetInt() && HairWorksRenderer::ViewsHasHair(Views)) ||
			AlwaysCreateRenderTargets->GetInt()
			)
			HairWorksRenderer::AllocRenderTargets(RHICmdList, FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY());
	}
	// @third party code - END HairWorks
	// Draw the scene pre-pass / early z pass, populating the scene depth buffer and HiZ
	GRenderTargetPool.AddPhaseEvent(TEXT("EarlyZPass"));
	const bool bNeedsPrePass = NeedsPrePass(this);
	bool bDepthWasCleared;
	if (bNeedsPrePass)
	{
		bDepthWasCleared = RenderPrePass(RHICmdList, AfterTasksAreStarted);
	}
	else
	{
		if (FGPUSkinCache* GPUSkinCache = Scene->GetGPUSkinCache())
		{
			GPUSkinCache->TransitionAllToReadable(RHICmdList);
		}

		// we didn't do the prepass, but we still want the HMD mask if there is one
		AfterTasksAreStarted();
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_PrePass));
		bDepthWasCleared = RenderPrePassHMD(RHICmdList);
	}
	check(bDidAfterTaskWork);
	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterPrePass));
	ServiceLocalQueue();

	const bool bShouldRenderVelocities = ShouldRenderVelocities();
	const bool bUseVelocityGBuffer = FVelocityRendering::OutputsToGBuffer();
	const bool bUseSelectiveBasePassOutputs = UseSelectiveBasePassOutputs();

	// Use readonly depth in the base pass if we have a full depth prepass
	const bool bAllowReadonlyDepthBasePass = EarlyZPassMode == DDM_AllOpaque
		&& CVarBasePassWriteDepthEvenWithFullPrepass.GetValueOnRenderThread() == 0
		&& !ViewFamily.EngineShowFlags.ShaderComplexity
		&& !ViewFamily.UseDebugViewPS()
		&& !bIsWireframe
		&& !ViewFamily.EngineShowFlags.LightMapDensity;

	const FExclusiveDepthStencil::Type BasePassDepthStencilAccess = 
		bAllowReadonlyDepthBasePass
		? FExclusiveDepthStencil::DepthRead_StencilWrite 
		: FExclusiveDepthStencil::DepthWrite_StencilWrite;

	SceneContext.ResolveSceneDepthTexture(RHICmdList, FResolveRect(0, 0, ViewFamily.FamilySizeX, ViewFamily.FamilySizeY));

    if (bComputeLightGrid)
    {
        ComputeLightGrid(RHICmdList);
	}
	else
	{
		for (auto& View : Views)
		{
			View.ForwardLightingResources = GetMinimalDummyForwardLightingResources();
		}
	}

	if (bUseGBuffer || IsSimpleForwardShadingEnabled(GetFeatureLevelShaderPlatform(FeatureLevel)))
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_AllocGBufferTargets);
		SceneContext.PreallocGBufferTargets(); // Even if !bShouldRenderVelocities, the velocity buffer must be bound because it's a compile time option for the shader.
		SceneContext.AllocGBufferTargets(RHICmdList);
	}	

	const bool bOcclusionBeforeBasePass = (EarlyZPassMode == EDepthDrawingMode::DDM_AllOccluders) || (EarlyZPassMode == EDepthDrawingMode::DDM_AllOpaque);

	if (bOcclusionBeforeBasePass)
	{
		if (bIsOcclusionTesting)
		{
			RenderOcclusion(RHICmdList);
		}
		bool bUseHzbOcclusion = RenderHzb(RHICmdList);
		if (bUseHzbOcclusion || bIsOcclusionTesting)
		{
			FinishOcclusion(RHICmdList);
		}
	}

	ServiceLocalQueue();

	if (bOcclusionBeforeBasePass)
	{
		RenderShadowDepthMaps(RHICmdList);
		ServiceLocalQueue();
	}

	// Clear LPVs for all views
	if (FeatureLevel >= ERHIFeatureLevel::SM5)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_ClearLPVs);
		ClearLPVs(RHICmdList);
		ServiceLocalQueue();
	}

	if(GetCustomDepthPassLocation() == 0)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_CustomDepthPass0);
		RenderCustomDepthPassAtLocation(RHICmdList, 0);
	}

	if (bOcclusionBeforeBasePass)
	{
		ComputeVolumetricFog(RHICmdList);
	}

	if (IsForwardShadingEnabled(FeatureLevel))
	{
		RenderForwardShadingShadowProjections(RHICmdList);

		RenderIndirectCapsuleShadows(
			RHICmdList, 
			NULL, 
			NULL);
	}

	// only temporarily available after early z pass and until base pass
	check(!SceneContext.DBufferA);
	check(!SceneContext.DBufferB);
	check(!SceneContext.DBufferC);

	if (bDBuffer)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_DBuffer);
		SceneContext.ResolveSceneDepthToAuxiliaryTexture(RHICmdList);

		// e.g. DBuffer deferred decals
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{	
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView,Views.Num() > 1, TEXT("View%d"), ViewIndex);

			GCompositionLighting.ProcessBeforeBasePass(RHICmdList, Views[ViewIndex]);
		}
		//GBuffer pass will want to write to SceneDepthZ
		RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, SceneContext.GetSceneDepthTexture());
		ServiceLocalQueue();
	}
	
	if (bRenderDeferredLighting)
	{
		//Single point to catch UE-31578, UE-32536 and UE-22073 and attempt to recover by reallocating Deferred Render Targets
		if (!SceneContext.TranslucencyLightingVolumeAmbient[0] || !SceneContext.TranslucencyLightingVolumeDirectional[0] ||
			!SceneContext.TranslucencyLightingVolumeAmbient[1] || !SceneContext.TranslucencyLightingVolumeDirectional[1])
		{
			const char* str = SceneContext.ScreenSpaceAO ? "Allocated" : "Unallocated"; //ScreenSpaceAO is determining factor of detecting render target allocation
			ensureMsgf(SceneContext.TranslucencyLightingVolumeAmbient[0], TEXT("%s is unallocated, Deferred Render Targets would be detected as: %s"), "TranslucencyLightingVolumeAmbient0", str);
			ensureMsgf(SceneContext.TranslucencyLightingVolumeDirectional[0], TEXT("%s is unallocated, Deferred Render Targets would be detected as: %s"), "TranslucencyLightingVolumeDirectional0", str);
			ensureMsgf(SceneContext.TranslucencyLightingVolumeAmbient[1], TEXT("%s is unallocated, Deferred Render Targets would be detected as: %s"), "TranslucencyLightingVolumeAmbient1", str);
			ensureMsgf(SceneContext.TranslucencyLightingVolumeDirectional[1], TEXT("%s is unallocated, Deferred Render Targets would be detected as: %s"), "TranslucencyLightingVolumeDirectional1", str);
			SceneContext.AllocateDeferredShadingPathRenderTargets(RHICmdList);
		}
		
		if (GbEnableAsyncComputeTranslucencyLightingVolumeClear && GSupportsEfficientAsyncCompute)
		{
			ClearTranslucentVolumeLightingAsyncCompute(RHICmdList);
		}
	}

	// Clear the GBuffer render targets
	bool bIsGBufferCurrent = false;
	if (bRequiresRHIClear)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_SetAndClearViewGBuffer);
		// set GBuffer to be current, and clear it
		SetAndClearViewGBuffer(RHICmdList, Views[0], BasePassDepthStencilAccess, !bDepthWasCleared);
		
		// depth was cleared now no matter what
		bDepthWasCleared = true;
		bIsGBufferCurrent = true;
		ServiceLocalQueue();
	}
	
	if (bIsWireframe && FSceneRenderer::ShouldCompositeEditorPrimitives(Views[0]))
	{
		// In Editor we want wire frame view modes to be MSAA for better quality. Resolve will be done with EditorPrimitives
		SetRenderTarget(RHICmdList, SceneContext.GetEditorPrimitivesColor(RHICmdList), SceneContext.GetEditorPrimitivesDepth(RHICmdList), ESimpleRenderTargetMode::EClearColorAndDepth);
	}
	else if (!bIsGBufferCurrent)
	{
		// make sure the GBuffer is set, in case we didn't need to clear above
		ERenderTargetLoadAction DepthLoadAction = bDepthWasCleared ? ERenderTargetLoadAction::ELoad : ERenderTargetLoadAction::EClear;
		SceneContext.BeginRenderingGBuffer(RHICmdList, ERenderTargetLoadAction::ENoAction, DepthLoadAction, BasePassDepthStencilAccess, ViewFamily.EngineShowFlags.ShaderComplexity);
	}

	GRenderTargetPool.AddPhaseEvent(TEXT("BasePass"));

	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_BasePass));
	RenderBasePass(RHICmdList, BasePassDepthStencilAccess);
	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterBasePass));
	ServiceLocalQueue();

	if (!bAllowReadonlyDepthBasePass)
	{
		SceneContext.ResolveSceneDepthTexture(RHICmdList, FResolveRect(0, 0, ViewFamily.FamilySizeX, ViewFamily.FamilySizeY));
	}

#if WITH_FLEX
	GFlexFluidSurfaceRenderer.RenderParticles(RHICmdList, Views);
	
	//FSceneRenderTargets::Get(RHICmdList).BeginRenderingGBuffer(RHICmdList, ERenderTargetLoadAction::ENoAction, ERenderTargetLoadAction::ENoAction);
	SceneContext.BeginRenderingGBuffer(RHICmdList, ERenderTargetLoadAction::ENoAction, ERenderTargetLoadAction::ENoAction, BasePassDepthStencilAccess, ViewFamily.EngineShowFlags.ShaderComplexity);

	GFlexFluidSurfaceRenderer.RenderBasePass(RHICmdList, Views);
#endif

	if (ViewFamily.EngineShowFlags.VisualizeLightCulling)
	{
		// clear out emissive and baked lighting (not too efficient but simple and only needed for this debug view)
		SceneContext.BeginRenderingSceneColor(RHICmdList);
		DrawClearQuad(RHICmdList, FLinearColor(0, 0, 0, 0));
	}

	SceneContext.DBufferA.SafeRelease();
	SceneContext.DBufferB.SafeRelease();
	SceneContext.DBufferC.SafeRelease();

	// only temporarily available after early z pass and until base pass
	check(!SceneContext.DBufferA);
	check(!SceneContext.DBufferB);
	check(!SceneContext.DBufferC);

	if (bRequiresFarZQuadClear)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_ClearGBufferAtMaxZ);
		// Clears view by drawing quad at maximum Z
		// TODO: if all the platforms have fast color clears, we can replace this with an RHICmdList.Clear.
		ClearGBufferAtMaxZ(RHICmdList);
		ServiceLocalQueue();

		bRequiresFarZQuadClear = false;
	}

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	ViewFamily.bVxgiAvailable = false;

	bool bVxgiEnabled = InitializeVxgiVoxelizationParameters(RHICmdList);
#endif
	// NVCHANGE_END: Add VXGI

	VisualizeVolumetricLightmap(RHICmdList);

	SceneContext.ResolveSceneDepthToAuxiliaryTexture(RHICmdList);

	// NvFlow begin
	if (GRendererNvFlowHooks)
	{
		bool ShouldDoPreComposite = GRendererNvFlowHooks->NvFlowShouldDoPreComposite(RHICmdList);
		if (ShouldDoPreComposite)
		{
			SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthWrite_StencilWrite);

			for (int32 ViewIdx = 0; ViewIdx < Views.Num(); ViewIdx++)
			{
				const auto& View = Views[ViewIdx];

				RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

				GRendererNvFlowHooks->NvFlowDoPreComposite(RHICmdList, View);
			}
		}
	}
	// NvFlow end

	if (!bOcclusionBeforeBasePass)
	{
		if (bIsOcclusionTesting)
		{
			RenderOcclusion(RHICmdList);
		}
		bool bUseHzbOcclusion = RenderHzb(RHICmdList);
		if (bUseHzbOcclusion || bIsOcclusionTesting)
		{
			FinishOcclusion(RHICmdList);
		}
	}

	ServiceLocalQueue();

	if (bUseGBuffer)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_Resolve_After_Basepass);
		SceneContext.FinishRenderingGBuffer(RHICmdList);
	}

	if (!bOcclusionBeforeBasePass)
	{
		RenderShadowDepthMaps(RHICmdList);
		ComputeVolumetricFog(RHICmdList);
		ServiceLocalQueue();
	}

	if(GetCustomDepthPassLocation() == 1)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_CustomDepthPass1);
		RenderCustomDepthPassAtLocation(RHICmdList, 1);
	}

	ServiceLocalQueue();

	// Notify the FX system that opaque primitives have been rendered and we now have a valid depth buffer.
	if (Scene->FXSystem && Views.IsValidIndex(0) && !Views[0].bIsPlanarReflection)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_FXSystem_PostRenderOpaque);
		Scene->FXSystem->PostRenderOpaque(
			RHICmdList,
			Views[0].ViewUniformBuffer,
			SceneContext.GetSceneDepthTexture(),
			SceneContext.GBufferA ? SceneContext.GetGBufferATexture() : NULL
			);
		ServiceLocalQueue();
	}

	TRefCountPtr<IPooledRenderTarget> VelocityRT;

	if (bUseVelocityGBuffer)
	{
		VelocityRT = SceneContext.GetGBufferVelocityRT();
	}
	
	if (bShouldRenderVelocities && (!bUseVelocityGBuffer || bUseSelectiveBasePassOutputs))
	{
		// Render the velocities of movable objects
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_Velocity));
		RenderVelocities(RHICmdList, VelocityRT);
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterVelocity));
		ServiceLocalQueue();
	}

	// Copy lighting channels out of stencil before deferred decals which overwrite those values
	CopyStencilToLightingChannelTexture(RHICmdList);

	{
		GCompositionLighting.GfxWaitForAsyncSSAO(RHICmdList);
	}

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	
	GDynamicRHI->RHIVXGISetCommandList(&RHICmdList);

	if (bVxgiEnabled)
	{
		if (!Views[0].bIsSceneCapture)
		{
			RenderVxgiVoxelization(RHICmdList);
		}

		RenderVxgiTracing(RHICmdList);

		if (!bVxgiAmbientOcclusionMode)
		{
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				CompositeVxgiDiffuseTracing(RHICmdList, Views[ViewIndex]);
			}
		}
	}

	if (!bVxgiEnabled || !bVxgiAmbientOcclusionMode)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			Views[ViewIndex].FinalPostProcessSettings.VxgiAmbientMixIntensity = 0.0f;
		}
	}
#endif
	// NVCHANGE_END: Add VXGI

	// Pre-lighting composition lighting stage
	// e.g. deferred decals, SSAO
	if (FeatureLevel >= ERHIFeatureLevel::SM4)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_AfterBasePass);

		GRenderTargetPool.AddPhaseEvent(TEXT("AfterBasePass"));
		if (!IsForwardShadingEnabled(FeatureLevel))
		{
			SceneContext.ResolveSceneDepthTexture(RHICmdList, FResolveRect(0, 0, ViewFamily.FamilySizeX, ViewFamily.FamilySizeY));
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
			GCompositionLighting.ProcessAfterBasePass(RHICmdList, Views[ViewIndex]);
		}
		ServiceLocalQueue();
	}

	// TODO: Could entirely remove this by using STENCIL_SANDBOX_BIT in ShadowRendering.cpp and DistanceFieldSurfaceCacheLighting.cpp
	if (!IsForwardShadingEnabled(FeatureLevel))
	{
		SCOPED_DRAW_EVENT(RHICmdList, ClearStencilFromBasePass);

		FRHISetRenderTargetsInfo Info(0, NULL, FRHIDepthRenderTargetView(
			SceneContext.GetSceneDepthSurface(),
			ERenderTargetLoadAction::ENoAction,
			ERenderTargetStoreAction::ENoAction,
			ERenderTargetLoadAction::EClear,
			ERenderTargetStoreAction::EStore,
			FExclusiveDepthStencil::DepthNop_StencilWrite));

		// Clear stencil to 0 now that deferred decals are done using what was setup in the base pass
		// Shadow passes and other users of stencil assume it is cleared to 0 going in
		RHICmdList.SetRenderTargetsAndClear(Info);

		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SceneContext.GetSceneDepthSurface());
	}

	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	NVVolumetricLightingBeginAccumulation(RHICmdList);
#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting

	// Render lighting.
	if (bRenderDeferredLighting)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_Lighting);

		GRenderTargetPool.AddPhaseEvent(TEXT("Lighting"));

		// These modulate the scenecolor output from the basepass, which is assumed to be indirect lighting
		RenderIndirectCapsuleShadows(
			RHICmdList, 
			SceneContext.GetSceneColorSurface(), 
			SceneContext.bScreenSpaceAOIsValid ? SceneContext.ScreenSpaceAO->GetRenderTargetItem().TargetableTexture : NULL);

		TRefCountPtr<IPooledRenderTarget> DynamicBentNormalAO;
		// These modulate the scenecolor output from the basepass, which is assumed to be indirect lighting
		RenderDFAOAsIndirectShadowing(RHICmdList, VelocityRT, DynamicBentNormalAO);

		// Clear the translucent lighting volumes before we accumulate
		if ((GbEnableAsyncComputeTranslucencyLightingVolumeClear && GSupportsEfficientAsyncCompute) == false)
		{
			ClearTranslucentVolumeLighting(RHICmdList);
		}

		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_Lighting));
		RenderLights(RHICmdList);
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterLighting));
		ServiceLocalQueue();

		GRenderTargetPool.AddPhaseEvent(TEXT("AfterRenderLights"));

		InjectAmbientCubemapTranslucentVolumeLighting(RHICmdList);
		ServiceLocalQueue();

		// Filter the translucency lighting volume now that it is complete
		FilterTranslucentVolumeLighting(RHICmdList);
		ServiceLocalQueue();

		// Pre-lighting composition lighting stage
		// e.g. LPV indirect
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			FViewInfo& View = Views[ViewIndex]; 

			if(IsLpvIndirectPassRequired(View))
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView,Views.Num() > 1, TEXT("View%d"), ViewIndex);

				GCompositionLighting.ProcessLpvIndirect(RHICmdList, View);
				ServiceLocalQueue();
			}
		}

		RenderDynamicSkyLighting(RHICmdList, VelocityRT, DynamicBentNormalAO);
		ServiceLocalQueue();

		// SSS need the SceneColor finalized as an SRV.
		ResolveSceneColor(RHICmdList);

		// Render reflections that only operate on opaque pixels
		RenderDeferredReflections(RHICmdList, DynamicBentNormalAO, VelocityRT);
		ServiceLocalQueue();

		DynamicBentNormalAO = NULL;

		// Post-lighting composition lighting stage
		// e.g. ScreenSpaceSubsurfaceScattering
		for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{	
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView,Views.Num() > 1, TEXT("View%d"), ViewIndex);
			GCompositionLighting.ProcessAfterLighting(RHICmdList, Views[ViewIndex]);
		}
		ServiceLocalQueue();
	}

	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	NVVolumetricLightingEndAccumulation(RHICmdList);
#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting

	// @third party code - BEGIN HairWorks
	// Blend hair lighting
	if(HairWorksRenderer::ViewsHasHair(Views))
		HairWorksRenderer::BlendLightingColor(RHICmdList);
	// @third party code - END HairWorks

	// NVCHANGE_BEGIN: Add HBAO+
#if WITH_GFSDK_SSAO
	if (GMaxRHIShaderPlatform == SP_PCD3D_SM5 &&
		CVarHBAOEnable.GetValueOnRenderThread() &&
		ViewFamily.EngineShowFlags.HBAO)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			const FViewInfo& View = Views[ViewIndex];

			if (View.IsPerspectiveProjection() &&
				View.FinalPostProcessSettings.HBAOPowerExponent > 0.f)
			{
				// Set the viewport to the current view
				RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);

				GFSDK_SSAO_Parameters Params;
				Params.Radius = View.FinalPostProcessSettings.HBAORadius;
				Params.Bias = View.FinalPostProcessSettings.HBAOBias;
				Params.PowerExponent = View.FinalPostProcessSettings.HBAOPowerExponent;
				Params.SmallScaleAO = View.FinalPostProcessSettings.HBAOSmallScaleAO;
				Params.Blur.Enable = View.FinalPostProcessSettings.HBAOBlurRadius != AOBR_BlurRadius0;
				Params.Blur.Sharpness = View.FinalPostProcessSettings.HBAOBlurSharpness;
				Params.Blur.Radius = View.FinalPostProcessSettings.HBAOBlurRadius == AOBR_BlurRadius2 ? GFSDK_SSAO_BLUR_RADIUS_2 : GFSDK_SSAO_BLUR_RADIUS_4;
				Params.ForegroundAO.Enable = View.FinalPostProcessSettings.HBAOForegroundAOEnable;
				Params.ForegroundAO.ForegroundViewDepth = View.FinalPostProcessSettings.HBAOForegroundAODistance;
				Params.BackgroundAO.Enable = View.FinalPostProcessSettings.HBAOBackgroundAOEnable;
				Params.BackgroundAO.BackgroundViewDepth = View.FinalPostProcessSettings.HBAOBackgroundAODistance;
				Params.DepthStorage = CVarHBAOHighPrecisionDepth.GetValueOnRenderThread() ? GFSDK_SSAO_FP32_VIEW_DEPTHS : GFSDK_SSAO_FP16_VIEW_DEPTHS;

				// Render HBAO and multiply the AO over the SceneColorSurface.RGB, preserving destination alpha
				RHICmdList.RenderHBAO(
					SceneContext.GetSceneDepthTexture(),
					View.ViewMatrices.GetProjectionMatrix(),
					SceneContext.GetGBufferATexture(),
					View.ViewMatrices.GetViewMatrix(),
					SceneContext.GetSceneColorTexture(),
					Params);
			}
		}
	}
#endif
	// NVCHANGE_END: Add HBAO+

	if (ViewFamily.EngineShowFlags.StationaryLightOverlap &&
		FeatureLevel >= ERHIFeatureLevel::SM4)
	{
		RenderStationaryLightOverlap(RHICmdList);
		ServiceLocalQueue();
	}	FLightShaftsOutput LightShaftOutput;

	// Draw Lightshafts
	if (ViewFamily.EngineShowFlags.LightShafts)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderLightShaftOcclusion);
		RenderLightShaftOcclusion(RHICmdList, LightShaftOutput);
		ServiceLocalQueue();
	}

	// Draw atmosphere
	if (ShouldRenderAtmosphere(ViewFamily))
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderAtmosphere);
		if (Scene->AtmosphericFog)
		{
			// Update RenderFlag based on LightShaftTexture is valid or not
			if (LightShaftOutput.LightShaftOcclusion)
			{
				Scene->AtmosphericFog->RenderFlag &= EAtmosphereRenderFlag::E_LightShaftMask;
			}
			else
			{
				Scene->AtmosphericFog->RenderFlag |= EAtmosphereRenderFlag::E_DisableLightShaft;
			}
#if WITH_EDITOR
			if (Scene->bIsEditorScene)
			{
				// Precompute Atmospheric Textures
				Scene->AtmosphericFog->PrecomputeTextures(RHICmdList, Views.GetData(), &ViewFamily);
			}
#endif
			RenderAtmosphere(RHICmdList, LightShaftOutput);
			ServiceLocalQueue();
		}
	}

	GRenderTargetPool.AddPhaseEvent(TEXT("Fog"));

	// Draw fog.
	if (ShouldRenderFog(ViewFamily))
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderFog);
		RenderFog(RHICmdList, LightShaftOutput);
		ServiceLocalQueue();
	}

	IRendererModule& RendererModule = GetRendererModule();
	if (RendererModule.HasPostOpaqueExtentions())
	{
		SceneContext.BeginRenderingSceneColor(RHICmdList);
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			const FViewInfo& View = Views[ViewIndex];
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			RendererModule.RenderPostOpaqueExtensions(View, RHICmdList, SceneContext);
		}
	}

	RendererModule.DispatchPostOpaqueCompute(RHICmdList);

	// No longer needed, release
	LightShaftOutput.LightShaftOcclusion = NULL;

	GRenderTargetPool.AddPhaseEvent(TEXT("Translucency"));

	// Draw translucency.
	if (ViewFamily.EngineShowFlags.Translucency)
	{
		SCOPE_CYCLE_COUNTER(STAT_TranslucencyDrawTime);

		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_Translucency));

		// For now there is only one resolve for all translucency passes. This can be changed by enabling the resolve in RenderTranslucency()
		ConditionalResolveSceneColorForTranslucentMaterials(RHICmdList);

		// WaveWorks Start
		RenderWaveWorks(RHICmdList);
		// WaveWorks End

		if (ViewFamily.AllowTranslucencyAfterDOF())
		{
			RenderTranslucency(RHICmdList, ETranslucencyPass::TPT_StandardTranslucency);
			// Translucency after DOF is rendered now, but stored in the separate translucency RT for later use.
			RenderTranslucency(RHICmdList, ETranslucencyPass::TPT_TranslucencyAfterDOF);
		}
		else // Otherwise render translucent primitives in a single bucket.
		{
			RenderTranslucency(RHICmdList, ETranslucencyPass::TPT_AllTranslucency);
		}
		ServiceLocalQueue();

		static const auto DisableDistortionCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DisableDistortion"));
		const bool bAllowDistortion = DisableDistortionCVar->GetValueOnAnyThread() != 1;

		if (GetRefractionQuality(ViewFamily) > 0 && bAllowDistortion)
		{
			// To apply refraction effect by distorting the scene color.
			// After non separate translucency as that is considered at scene depth anyway
			// It allows skybox translucency (set to non separate translucency) to be refracted.
			RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_RenderDistortion));
			RenderDistortion(RHICmdList);
			ServiceLocalQueue();
		}

		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterTranslucency));
	}

	if (ViewFamily.EngineShowFlags.LightShafts)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderLightShaftBloom);
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_LightShaftBloom));
		RenderLightShaftBloom(RHICmdList);
		ServiceLocalQueue();
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
		RendererModule.RenderOverlayExtensions(View, RHICmdList, SceneContext);
	}

	if (ViewFamily.EngineShowFlags.VisualizeDistanceFieldAO || ViewFamily.EngineShowFlags.VisualizeDistanceFieldGI)
	{
		// Use the skylight's max distance if there is one, to be consistent with DFAO shadowing on the skylight
		const float OcclusionMaxDistance = Scene->SkyLight && !Scene->SkyLight->bWantsStaticShadowing ? Scene->SkyLight->OcclusionMaxDistance : Scene->DefaultMaxDistanceFieldOcclusionDistance;
		TRefCountPtr<IPooledRenderTarget> DummyOutput;
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_RenderDistanceFieldLighting));
		RenderDistanceFieldLighting(RHICmdList, FDistanceFieldAOParameters(OcclusionMaxDistance), VelocityRT, DummyOutput, DummyOutput, false, ViewFamily.EngineShowFlags.VisualizeDistanceFieldAO, ViewFamily.EngineShowFlags.VisualizeDistanceFieldGI); 
		ServiceLocalQueue();
	}

	// Draw visualizations just before use to avoid target contamination
	if (ViewFamily.EngineShowFlags.VisualizeMeshDistanceFields || ViewFamily.EngineShowFlags.VisualizeGlobalDistanceField)
	{
		RenderMeshDistanceFieldVisualization(RHICmdList, FDistanceFieldAOParameters(Scene->DefaultMaxDistanceFieldOcclusionDistance));
		ServiceLocalQueue();
	}

	if (ViewFamily.EngineShowFlags.StationaryLightOverlap &&
		FeatureLevel >= ERHIFeatureLevel::SM4 && 
		bUseGBuffer)
	{
		RenderStationaryLightOverlap(RHICmdList);
		ServiceLocalQueue();
	}

#if WITH_FLEX
	GFlexFluidSurfaceRenderer.Cleanup();
#endif

	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	NVVolumetricLightingApplyLighting(RHICmdList);
#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting

	// Resolve the scene color for post processing.
	ResolveSceneColor(RHICmdList);

	GetRendererModule().RenderPostResolvedSceneColorExtension(RHICmdList, SceneContext);

	CopySceneCaptureComponentToTarget(RHICmdList);

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
		RenderVxgiDebug(RHICmdList, Views[ViewIndex], ViewIndex);
	}
#endif
	// NVCHANGE_END: Add VXGI

	// Finish rendering for each view.
	if (ViewFamily.bResolveScene)
	{
		SCOPED_DRAW_EVENT(RHICmdList, PostProcessing);
   		SCOPED_GPU_STAT(RHICmdList, Stat_GPU_Postprocessing);

		SCOPE_CYCLE_COUNTER(STAT_FinishRenderViewTargetTime);

		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_PostProcessing));
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			GPostProcessing.Process(RHICmdList, Views[ ViewIndex ], VelocityRT);
		}

		// End of frame, we don't need it anymore
		FSceneRenderTargets::Get(RHICmdList).FreeDownsampledTranslucencyDepth();
// WaveWorks Start
		FSceneRenderTargets::Get(RHICmdList).FreeWaveWorksDepth();
// WaveWorks End

		// we rendered to it during the frame, seems we haven't made use of it, because it should be released
		check(!FSceneRenderTargets::Get(RHICmdList).SeparateTranslucencyRT);
	}
	else
	{
		// Release the original reference on the scene render targets
		SceneContext.AdjustGBufferRefCount(RHICmdList, -1);
	}

	//grab the new transform out of the proxies for next frame
	if (VelocityRT)
	{
		VelocityRT.SafeRelease();
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderFinish);
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_RenderFinish));
		RenderFinish(RHICmdList);
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterFrame));
	}
	ServiceLocalQueue();

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI

	GDynamicRHI->RHIVXGISetCommandList(nullptr);

	if (VxgiView)
	{
		delete VxgiView;
		VxgiView = nullptr;
	}
#endif
	// NVCHANGE_END: Add VXGI
}


/** A simple pixel shader used on PC to read scene depth from scene color alpha and write it to a downsized depth buffer. */
class FDownsampleSceneDepthPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDownsampleSceneDepthPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	FDownsampleSceneDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		ProjectionScaleBias.Bind(Initializer.ParameterMap,TEXT("ProjectionScaleBias"));
		SourceTexelOffsets01.Bind(Initializer.ParameterMap,TEXT("SourceTexelOffsets01"));
		SourceTexelOffsets23.Bind(Initializer.ParameterMap,TEXT("SourceTexelOffsets23"));
		UseMaxDepth.Bind(Initializer.ParameterMap, TEXT("UseMaxDepth"));
	}
	FDownsampleSceneDepthPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, bool bUseMaxDepth)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		// Used to remap view space Z (which is stored in scene color alpha) into post projection z and w so we can write z/w into the downsized depth buffer
		const FVector2D ProjectionScaleBiasValue(View.ViewMatrices.GetProjectionMatrix().M[2][2], View.ViewMatrices.GetProjectionMatrix().M[3][2]);
		SetShaderValue(RHICmdList, GetPixelShader(), ProjectionScaleBias, ProjectionScaleBiasValue);
		SetShaderValue(RHICmdList, GetPixelShader(), UseMaxDepth, (bUseMaxDepth ? 1.0f : 0.0f));

		FIntPoint BufferSize = SceneContext.GetBufferSizeXY();

		const uint32 DownsampledBufferSizeX = BufferSize.X / SceneContext.GetSmallColorDepthDownsampleFactor();
		const uint32 DownsampledBufferSizeY = BufferSize.Y / SceneContext.GetSmallColorDepthDownsampleFactor();

		// Offsets of the four full resolution pixels corresponding with a low resolution pixel
		const FVector4 Offsets01(0.0f, 0.0f, 1.0f / DownsampledBufferSizeX, 0.0f);
		SetShaderValue(RHICmdList, GetPixelShader(), SourceTexelOffsets01, Offsets01);
		const FVector4 Offsets23(0.0f, 1.0f / DownsampledBufferSizeY, 1.0f / DownsampledBufferSizeX, 1.0f / DownsampledBufferSizeY);
		SetShaderValue(RHICmdList, GetPixelShader(), SourceTexelOffsets23, Offsets23);
		SceneTextureParameters.Set(RHICmdList, GetPixelShader(), View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ProjectionScaleBias;
		Ar << SourceTexelOffsets01;
		Ar << SourceTexelOffsets23;
		Ar << SceneTextureParameters;
		Ar << UseMaxDepth;
		return bShaderHasOutdatedParameters;
	}

	FShaderParameter ProjectionScaleBias;
	FShaderParameter SourceTexelOffsets01;
	FShaderParameter SourceTexelOffsets23;
	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderParameter UseMaxDepth;
};

IMPLEMENT_SHADER_TYPE(,FDownsampleSceneDepthPS,TEXT("/Engine/Private/DownsampleDepthPixelShader.usf"),TEXT("Main"),SF_Pixel);

/** Updates the downsized depth buffer with the current full resolution depth buffer. */
void FDeferredShadingSceneRenderer::UpdateDownsampledDepthSurface(FRHICommandList& RHICmdList)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	if (SceneContext.UseDownsizedOcclusionQueries() && (FeatureLevel >= ERHIFeatureLevel::SM4))
	{
		RHICmdList.TransitionResource( EResourceTransitionAccess::EReadable, SceneContext.GetSceneDepthSurface() );

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			DownsampleDepthSurface(RHICmdList, SceneContext.GetSmallDepthSurface(), View, 1.0f / SceneContext.GetSmallColorDepthDownsampleFactor(), true);
		}
	}
}

/** Downsample the scene depth with a specified scale factor to a specified render target
*/
void FDeferredShadingSceneRenderer::DownsampleDepthSurface(FRHICommandList& RHICmdList, const FTexture2DRHIRef& RenderTarget, const FViewInfo& View, float ScaleFactor, bool bUseMaxDepth)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SetRenderTarget(RHICmdList, NULL, RenderTarget);
	SCOPED_DRAW_EVENT(RHICmdList, DownsampleDepth);

	// Set shaders and texture
	TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
	TShaderMapRef<FDownsampleSceneDepthPS> PixelShader(View.ShaderMap);

	extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	GraphicsPSOInit.BlendState = TStaticBlendState<CW_NONE>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_Always>::GetRHI();

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*ScreenVertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	PixelShader->SetParameters(RHICmdList, View, bUseMaxDepth);
	const uint32 DownsampledX = FMath::TruncToInt(View.ViewRect.Min.X * ScaleFactor);
	const uint32 DownsampledY = FMath::TruncToInt(View.ViewRect.Min.Y * ScaleFactor);
	const uint32 DownsampledSizeX = FMath::TruncToInt(View.ViewRect.Width() * ScaleFactor);
	const uint32 DownsampledSizeY = FMath::TruncToInt(View.ViewRect.Height() * ScaleFactor);

	RHICmdList.SetViewport(DownsampledX, DownsampledY, 0.0f, DownsampledX + DownsampledSizeX, DownsampledY + DownsampledSizeY, 1.0f);

	DrawRectangle(
		RHICmdList,
		0, 0,
		DownsampledSizeX, DownsampledSizeY,
		View.ViewRect.Min.X, View.ViewRect.Min.Y,
		View.ViewRect.Width(), View.ViewRect.Height(),
		FIntPoint(DownsampledSizeX, DownsampledSizeY),
		SceneContext.GetBufferSizeXY(),
		*ScreenVertexShader,
		EDRF_UseTriangleOptimization);

	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, RenderTarget);
}

/** */
class FCopyStencilToLightingChannelsPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCopyStencilToLightingChannelsPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("STENCIL_LIGHTING_CHANNELS_SHIFT"), STENCIL_LIGHTING_CHANNELS_BIT_ID);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_R16_UINT);
	}

	FCopyStencilToLightingChannelsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		SceneStencilTexture.Bind(Initializer.ParameterMap,TEXT("SceneStencilTexture"));
	}
	FCopyStencilToLightingChannelsPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		SetSRVParameter(RHICmdList, GetPixelShader(), SceneStencilTexture, SceneContext.SceneStencilSRV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneStencilTexture;
		return bShaderHasOutdatedParameters;
	}

	FShaderResourceParameter SceneStencilTexture;
};

IMPLEMENT_SHADER_TYPE(,FCopyStencilToLightingChannelsPS,TEXT("/Engine/Private/DownsampleDepthPixelShader.usf"),TEXT("CopyStencilToLightingChannelsPS"),SF_Pixel);

void FDeferredShadingSceneRenderer::CopyStencilToLightingChannelTexture(FRHICommandList& RHICmdList)
{
	bool bAnyViewUsesLightingChannels = false;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		bAnyViewUsesLightingChannels = bAnyViewUsesLightingChannels || View.bUsesLightingChannels;
	}

	if (bAnyViewUsesLightingChannels)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		SCOPED_DRAW_EVENT(RHICmdList, CopyStencilToLightingChannels);
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SceneContext.GetSceneDepthTexture());

		SceneContext.AllocateLightingChannelTexture(RHICmdList);

		// Set the light attenuation surface as the render target, and the scene depth buffer as the depth-stencil surface.
		SetRenderTarget(RHICmdList, SceneContext.LightingChannels->GetRenderTargetItem().TargetableTexture, nullptr, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthNop_StencilNop, true);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			// Set shaders and texture
			TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
			TShaderMapRef<FCopyStencilToLightingChannelsPS> PixelShader(View.ShaderMap);

			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*ScreenVertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			PixelShader->SetParameters(RHICmdList, View);

			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Min.X + View.ViewRect.Width(), View.ViewRect.Min.Y + View.ViewRect.Height(), 1.0f);

			DrawRectangle(
				RHICmdList,
				0, 0,
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Min.X, View.ViewRect.Min.Y,
				View.ViewRect.Width(), View.ViewRect.Height(),
				FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
				SceneContext.GetBufferSizeXY(),
				*ScreenVertexShader,
				EDRF_UseTriangleOptimization);
		}

		FResolveParams ResolveParams;

		RHICmdList.CopyToResolveTarget(
			SceneContext.LightingChannels->GetRenderTargetItem().TargetableTexture, 
			SceneContext.LightingChannels->GetRenderTargetItem().TargetableTexture,
			true,
			ResolveParams);
	}
}
