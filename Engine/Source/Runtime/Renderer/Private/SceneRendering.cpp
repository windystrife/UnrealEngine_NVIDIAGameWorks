// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneRendering.cpp: Scene rendering.
=============================================================================*/

#include "SceneRendering.h"
#include "ProfilingDebugging/ProfilingHelpers.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "EngineGlobals.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneCaptureComponentCube.h"
#include "StaticMeshDrawList.h"
#include "DeferredShadingRenderer.h"
#include "DynamicPrimitiveDrawing.h"
#include "RenderTargetTemp.h"
#include "RendererModule.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "PostProcess/PostProcessEyeAdaptation.h"
#include "CompositionLighting/CompositionLighting.h"
#include "SceneViewExtension.h"
#include "PostProcess/PostProcessBusyWait.h"
#include "PostProcess/PostProcessCircleDOF.h"
#include "AtmosphereRendering.h"
#include "Matinee/MatineeActor.h"
#include "ComponentRecreateRenderStateContext.h"
#include "PostProcess/PostProcessSubsurface.h"
#include "HdrCustomResolveShaders.h"
#include "WideCustomResolveShaders.h"
#include "PipelineStateCache.h"
#include "GPUSkinCache.h"
#include "PrecomputedVolumetricLightmap.h"
// @third party code - BEGIN HairWorks
#include "HairWorksRenderer.h"
// @third party code - END HairWorks

/*-----------------------------------------------------------------------------
	Globals
-----------------------------------------------------------------------------*/

extern ENGINE_API FLightMap2D* GDebugSelectedLightmap;
extern ENGINE_API UPrimitiveComponent* GDebugSelectedComponent;

DECLARE_FLOAT_COUNTER_STAT(TEXT("Custom Depth"), Stat_GPU_CustomDepth, STATGROUP_GPU);

static TAutoConsoleVariable<int32> CVarCustomDepthTemporalAAJitter(
	TEXT("r.CustomDepthTemporalAAJitter"),
	1,
	TEXT("If disabled the Engine will remove the TemporalAA Jitter from the Custom Depth Pass. Only has effect when TemporalAA is used."),
	ECVF_RenderThreadSafe
);


/**
 * Console variable controlling whether or not occlusion queries are allowed.
 */
static TAutoConsoleVariable<int32> CVarAllowOcclusionQueries(
	TEXT("r.AllowOcclusionQueries"),
	1,
	TEXT("If zero, occlusion queries will not be used to cull primitives."),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarDemosaicVposOffset(
	TEXT("r.DemosaicVposOffset"),
	0.0f,
	TEXT("This offset is added to the rasterized position used for demosaic in the ES2 tonemapping shader. It exists to workaround driver bugs on some Android devices that have a half-pixel offset."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRefractionQuality(
	TEXT("r.RefractionQuality"),
	2,
	TEXT("Defines the distorion/refraction quality which allows to adjust for quality or performance.\n")
	TEXT("<=0: off (fastest)\n")
	TEXT("  1: low quality (not yet implemented)\n")
	TEXT("  2: normal quality (default)\n")
	TEXT("  3: high quality (e.g. color fringe, not yet implemented)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarInstancedStereo(
	TEXT("vr.InstancedStereo"),
	0,
	TEXT("0 to disable instanced stereo (default), 1 to enable."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMultiView(
	TEXT("vr.MultiView"),
	0,
	TEXT("0 to disable multi-view instanced stereo, 1 to enable.\n")
	TEXT("Currently only supported by the PS4 & Metal RHIs."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMobileMultiView(
	TEXT("vr.MobileMultiView"),
	0,
	TEXT("0 to disable mobile multi-view, 1 to enable.\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMobileMultiViewDirect(
	TEXT("vr.MobileMultiView.Direct"),
	0,
	TEXT("0 to disable mobile multi-view direct, 1 to enable.\n")
	TEXT("When enabled the scene color render target array is provided by the hmd plugin so we can skip the blit.\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMonoscopicFarField(
	TEXT("vr.MonoscopicFarField"),
	0,
	TEXT("0 to disable (default), 1 to enable."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMonoscopicFarFieldMode(
	TEXT("vr.MonoscopicFarFieldMode"),
	1,
	TEXT("Experimental, mobile only")
	TEXT(", 0 to disable, 1 to enable (default)")
	TEXT(", 2 stereo near field only")
	TEXT(", 3 stereo near field with far field pixel depth test disabled")
	TEXT(", 4 mono far field only"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDebugCanvasInLayer(
	TEXT("vr.DebugCanvasInLayer"),
	0,
	TEXT("Experimental")
	TEXT("0 to disable (default), 1 to enable."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<float> CVarGeneralPurposeTweak(
	TEXT("r.GeneralPurposeTweak"),
	1.0f,
	TEXT("Useful for low level shader development to get quick iteration time without having to change any c++ code.\n")
	TEXT("Value maps to Frame.GeneralPurposeTweak inside the shaders.\n")
	TEXT("Example usage: Multiplier on some value to tweak, toggle to switch between different algorithms (Default: 1.0)\n")
	TEXT("DON'T USE THIS FOR ANYTHING THAT IS CHECKED IN. Compiled out in SHIPPING to make cheating a bit harder."),
	ECVF_RenderThreadSafe);


static TAutoConsoleVariable<int32> CVarDisplayInternals(
	TEXT("r.DisplayInternals"),
	0,
	TEXT("Allows to enable screen printouts that show the internals on the engine/renderer\n")
	TEXT("This is mostly useful to be able to reason why a screenshots looks different.\n")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: enabled"),
	ECVF_RenderThreadSafe | ECVF_Cheat);
#endif

/**
 * Console variable controlling the maximum number of shadow cascades to render with.
 *   DO NOT READ ON THE RENDERING THREAD. Use FSceneView::MaxShadowCascades.
 */
static TAutoConsoleVariable<int32> CVarMaxShadowCascades(
	TEXT("r.Shadow.CSM.MaxCascades"),
	10,
	TEXT("The maximum number of cascades with which to render dynamic directional light shadows."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarMaxMobileShadowCascades(
	TEXT("r.Shadow.CSM.MaxMobileCascades"),
	2,
	TEXT("The maximum number of cascades with which to render dynamic directional light shadows when using the mobile renderer."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSupportSimpleForwardShading(
	TEXT("r.SupportSimpleForwardShading"),
	0,
	TEXT("Whether to compile the shaders to support r.SimpleForwardShading being enabled (PC only)."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarSimpleForwardShading(
	TEXT("r.SimpleForwardShading"),
	0,
	TEXT("Whether to use the simple forward shading base pass shaders which only support lightmaps + stationary directional light + stationary skylight\n")
	TEXT("All other lighting features are disabled when true.  This is useful for supporting very low end hardware, and is only supported on PC platforms.\n")
	TEXT("0:off, 1:on"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarNormalCurvatureToRoughnessBias(
	TEXT("r.NormalCurvatureToRoughnessBias"),
	0.0f,
	TEXT("Biases the roughness resulting from screen space normal changes for materials with NormalCurvatureToRoughness enabled.  Valid range [-1, 1]"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarNormalCurvatureToRoughnessExponent(
	TEXT("r.NormalCurvatureToRoughnessExponent"),
	0.333f,
	TEXT("Exponent on the roughness resulting from screen space normal changes for materials with NormalCurvatureToRoughness enabled."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarNormalCurvatureToRoughnessScale(
	TEXT("r.NormalCurvatureToRoughnessScale"),
	1.0f,
	TEXT("Scales the roughness resulting from screen space normal changes for materials with NormalCurvatureToRoughness enabled.  Valid range [0, 2]"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

/*-----------------------------------------------------------------------------
	FParallelCommandListSet
-----------------------------------------------------------------------------*/


static TAutoConsoleVariable<int32> CVarRHICmdSpewParallelListBalance(
	TEXT("r.RHICmdSpewParallelListBalance"),
	0,
	TEXT("For debugging, spews the size of the parallel command lists. This stalls and otherwise wrecks performance.\n")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: enabled (default)"));

static TAutoConsoleVariable<int32> CVarRHICmdBalanceParallelLists(
	TEXT("r.RHICmdBalanceParallelLists"),
	2,
	TEXT("Allows to enable a preprocess of the drawlists to try to balance the load equally among the command lists.\n")
	TEXT(" 0: off \n")
	TEXT(" 1: enabled")
	TEXT(" 2: experiemental, uses previous frame results (does not do anything in split screen etc)"));

static TAutoConsoleVariable<int32> CVarRHICmdMinCmdlistForParallelSubmit(
	TEXT("r.RHICmdMinCmdlistForParallelSubmit"),
	2,
	TEXT("Minimum number of parallel translate command lists to submit. If there are fewer than this number, they just run on the RHI thread and immediate context."));

static TAutoConsoleVariable<int32> CVarRHICmdMinDrawsPerParallelCmdList(
	TEXT("r.RHICmdMinDrawsPerParallelCmdList"),
	64,
	TEXT("The minimum number of draws per cmdlist. If the total number of draws is less than this, then no parallel work will be done at all. This can't always be honored or done correctly. More effective with RHICmdBalanceParallelLists."));

static TAutoConsoleVariable<int32> CVarWideCustomResolve(
	TEXT("r.WideCustomResolve"),
	0,
	TEXT("Use a wide custom resolve filter when MSAA is enabled")
	TEXT("0: Disabled [hardware box filter]")
	TEXT("1: Wide (r=1.25, 12 samples)")
	TEXT("2: Wider (r=1.4, 16 samples)")
	TEXT("3: Widest (r=1.5, 20 samples)"),
	ECVF_RenderThreadSafe | ECVF_Scalability
	);

TAutoConsoleVariable<int32> CVarTransientResourceAliasing_Buffers(
	TEXT("r.TransientResourceAliasing.Buffers"),
	1,
	TEXT("Enables transient resource aliasing for specified buffers. Used only if GSupportsTransientResourceAliasing is true.\n"),
	ECVF_ReadOnly);

static FParallelCommandListSet* GOutstandingParallelCommandListSet = nullptr;

DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer UpdateMotionBlurCache"), STAT_FDeferredShadingSceneRenderer_UpdateMotionBlurCache, STATGROUP_SceneRendering);

#define FASTVRAM_CVAR(Name,DefaultValue) static TAutoConsoleVariable<int32> CVarFastVRam_##Name(TEXT("r.FastVRam."#Name), DefaultValue, TEXT(""))

FASTVRAM_CVAR(GBufferA, 0);
FASTVRAM_CVAR(GBufferB, 1);
FASTVRAM_CVAR(GBufferC, 0);
FASTVRAM_CVAR(GBufferD, 0);
FASTVRAM_CVAR(GBufferE, 0);
FASTVRAM_CVAR(GBufferVelocity, 0);
FASTVRAM_CVAR(HZB, 1);
FASTVRAM_CVAR(SceneDepth, 1);
FASTVRAM_CVAR(SceneColor, 1);
FASTVRAM_CVAR(LPV, 1);
FASTVRAM_CVAR(BokehDOF, 1);
FASTVRAM_CVAR(CircleDOF, 1);
FASTVRAM_CVAR(CombineLUTs, 1);
FASTVRAM_CVAR(Downsample, 1);
FASTVRAM_CVAR(EyeAdaptation, 1);
FASTVRAM_CVAR(Histogram, 1);
FASTVRAM_CVAR(HistogramReduce, 1);
FASTVRAM_CVAR(VelocityFlat, 1);
FASTVRAM_CVAR(VelocityMax, 1);
FASTVRAM_CVAR(MotionBlur, 1);
FASTVRAM_CVAR(Tonemap, 1);
FASTVRAM_CVAR(Upscale, 1);
FASTVRAM_CVAR(DistanceFieldNormal, 1);
FASTVRAM_CVAR(DistanceFieldAOHistory, 1);
FASTVRAM_CVAR(DistanceFieldAODownsampledBentNormal, 1); 
FASTVRAM_CVAR(DistanceFieldAOBentNormal, 0); 
FASTVRAM_CVAR(DistanceFieldAOConfidence, 0); 
FASTVRAM_CVAR(DistanceFieldIrradiance, 0); 
FASTVRAM_CVAR(DistanceFieldShadows, 1);
FASTVRAM_CVAR(Distortion, 1);
FASTVRAM_CVAR(ScreenSpaceShadowMask, 1);
FASTVRAM_CVAR(VolumetricFog, 1);
FASTVRAM_CVAR(SeparateTranslucency, 0); 
FASTVRAM_CVAR(LightAccumulation, 0); 
FASTVRAM_CVAR(LightAttenuation, 0); 
FASTVRAM_CVAR(ScreenSpaceAO,0);
FASTVRAM_CVAR(DBufferA, 0);
FASTVRAM_CVAR(DBufferB, 0);
FASTVRAM_CVAR(DBufferC, 0); 
FASTVRAM_CVAR(DBufferMask, 0);

FASTVRAM_CVAR(CustomDepth, 0);
FASTVRAM_CVAR(ShadowPointLight, 0);
FASTVRAM_CVAR(ShadowPerObject, 0);
FASTVRAM_CVAR(ShadowCSM, 0);

FASTVRAM_CVAR(DistanceFieldCulledObjectBuffers, 1);
FASTVRAM_CVAR(DistanceFieldTileIntersectionResources, 1);
FASTVRAM_CVAR(DistanceFieldAOScreenGridResources, 1);
FASTVRAM_CVAR(ForwardLightingCullingResources, 1);

FFastVramConfig::FFastVramConfig()
{
	FMemory::Memset(*this, 0);
}

void FFastVramConfig::Update()
{
	bDirty = false;
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_GBufferA, GBufferA);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_GBufferB, GBufferB);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_GBufferC, GBufferC);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_GBufferD, GBufferD);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_GBufferE, GBufferE);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_GBufferVelocity, GBufferVelocity);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_HZB, HZB);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_SceneDepth, SceneDepth);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_SceneColor, SceneColor);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_LPV, LPV);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_BokehDOF, BokehDOF);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_CircleDOF, CircleDOF);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_CombineLUTs, CombineLUTs);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_Downsample, Downsample);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_EyeAdaptation, EyeAdaptation);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_Histogram, Histogram);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_HistogramReduce, HistogramReduce);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_VelocityFlat, VelocityFlat);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_VelocityMax, VelocityMax);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_MotionBlur, MotionBlur);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_Tonemap, Tonemap);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_Upscale, Upscale);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_DistanceFieldNormal, DistanceFieldNormal);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_DistanceFieldAOHistory, DistanceFieldAOHistory);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_DistanceFieldAODownsampledBentNormal, DistanceFieldAODownsampledBentNormal);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_DistanceFieldAOBentNormal, DistanceFieldAOBentNormal);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_DistanceFieldAOConfidence, DistanceFieldAOConfidence);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_DistanceFieldIrradiance, DistanceFieldIrradiance);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_DistanceFieldShadows, DistanceFieldShadows);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_Distortion, Distortion);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_ScreenSpaceShadowMask, ScreenSpaceShadowMask);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_VolumetricFog, VolumetricFog);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_SeparateTranslucency, SeparateTranslucency);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_LightAccumulation, LightAccumulation);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_LightAttenuation, LightAttenuation);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_ScreenSpaceAO, ScreenSpaceAO);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_DBufferA, DBufferA);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_DBufferB, DBufferB);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_DBufferC, DBufferC);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_DBufferMask, DBufferMask);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_CustomDepth, CustomDepth);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_ShadowPointLight, ShadowPointLight);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_ShadowPerObject, ShadowPerObject);
	bDirty |= UpdateTextureFlagFromCVar(CVarFastVRam_ShadowCSM, ShadowCSM);

	bDirty |= UpdateBufferFlagFromCVar(CVarFastVRam_DistanceFieldCulledObjectBuffers, DistanceFieldCulledObjectBuffers);
	bDirty |= UpdateBufferFlagFromCVar(CVarFastVRam_DistanceFieldTileIntersectionResources, DistanceFieldTileIntersectionResources);
	bDirty |= UpdateBufferFlagFromCVar(CVarFastVRam_DistanceFieldAOScreenGridResources, DistanceFieldAOScreenGridResources);
	bDirty |= UpdateBufferFlagFromCVar(CVarFastVRam_ForwardLightingCullingResources, ForwardLightingCullingResources);
}

bool FFastVramConfig::UpdateTextureFlagFromCVar(TAutoConsoleVariable<int32>& CVar, ETextureCreateFlags& InOutValue)
{
	ETextureCreateFlags OldValue = InOutValue;
	InOutValue = CVar.GetValueOnRenderThread() ? ( TexCreate_FastVRAM ) : TexCreate_None;
	return OldValue != InOutValue;
}

bool FFastVramConfig::UpdateBufferFlagFromCVar(TAutoConsoleVariable<int32>& CVar, EBufferUsageFlags& InOutValue)
{
	EBufferUsageFlags OldValue = InOutValue;
	InOutValue = CVar.GetValueOnRenderThread() ? ( BUF_FastVRAM ) : BUF_None;
	return OldValue != InOutValue;
}

FFastVramConfig GFastVRamConfig;


FParallelCommandListSet::FParallelCommandListSet(TStatId InExecuteStat, const FViewInfo& InView, FRHICommandListImmediate& InParentCmdList, bool bInParallelExecute, bool bInCreateSceneContext)
	: View(InView)
	, DrawRenderState(InView)
	, ParentCmdList(InParentCmdList)
	, Snapshot(nullptr)
	, ExecuteStat(InExecuteStat)
	, NumAlloc(0)
	, bParallelExecute(GRHISupportsParallelRHIExecute && bInParallelExecute)
	, bCreateSceneContext(bInCreateSceneContext)
{
	Width = CVarRHICmdWidth.GetValueOnRenderThread();
	MinDrawsPerCommandList = CVarRHICmdMinDrawsPerParallelCmdList.GetValueOnRenderThread();
	bSpewBalance = !!CVarRHICmdSpewParallelListBalance.GetValueOnRenderThread();
	int32 IntBalance = CVarRHICmdBalanceParallelLists.GetValueOnRenderThread();
	bBalanceCommands = !!IntBalance;
	bBalanceCommandsWithLastFrame = IntBalance > 1;
	CommandLists.Reserve(Width * 8);
	Events.Reserve(Width * 8);
	NumDrawsIfKnown.Reserve(Width * 8);
	check(!GOutstandingParallelCommandListSet);
	GOutstandingParallelCommandListSet = this;
}

FRHICommandList* FParallelCommandListSet::AllocCommandList()
{
	NumAlloc++;
	return new FRHICommandList;
}

void FParallelCommandListSet::Dispatch(bool bHighPriority)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FParallelCommandListSet_Dispatch);
	check(IsInRenderingThread() && FMemStack::Get().GetNumMarks() == 1); // we do not want this popped before the end of the scene and it better be the scene allocator
	check(CommandLists.Num() == Events.Num());
	check(CommandLists.Num() == NumAlloc);
	if (bSpewBalance)
	{
		// finish them all
		for (auto& Event : Events)
		{
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(Event, ENamedThreads::RenderThread_Local);
		}
		// spew sizes
		int32 Index = 0;
		for (auto CmdList : CommandLists)
		{
			UE_LOG(LogTemp, Display, TEXT("CmdList %2d/%2d  : %8dKB"), Index, CommandLists.Num(), (CmdList->GetUsedMemory() + 1023) / 1024);
			Index++;
		}
	}
	bool bActuallyDoParallelTranslate = bParallelExecute && CommandLists.Num() >= CVarRHICmdMinCmdlistForParallelSubmit.GetValueOnRenderThread();
	if (bActuallyDoParallelTranslate)
	{
		int32 Total = 0;
		bool bIndeterminate = false;
		for (int32 Count : NumDrawsIfKnown)
		{
			if (Count < 0)
			{
				bIndeterminate = true;
				break; // can't determine how many are in this one; assume we should run parallel translate
			}
			Total += Count;
		}
		if (!bIndeterminate && Total < MinDrawsPerCommandList)
		{
			UE_CLOG(bSpewBalance, LogTemp, Display, TEXT("Disabling parallel translate because the number of draws is known to be small."));
			bActuallyDoParallelTranslate = false;
		}
	}

	if (bActuallyDoParallelTranslate)
	{
		UE_CLOG(bSpewBalance, LogTemp, Display, TEXT("%d cmdlists for parallel translate"), CommandLists.Num());
		check(GRHISupportsParallelRHIExecute);
		NumAlloc -= CommandLists.Num();
		ParentCmdList.QueueParallelAsyncCommandListSubmit(&Events[0], bHighPriority, &CommandLists[0], &NumDrawsIfKnown[0], CommandLists.Num(), (MinDrawsPerCommandList * 4) / 3, bSpewBalance);
		SetStateOnCommandList(ParentCmdList);
	}
	else
	{
		UE_CLOG(bSpewBalance, LogTemp, Display, TEXT("%d cmdlists (no parallel translate desired)"), CommandLists.Num());
		for (int32 Index = 0; Index < CommandLists.Num(); Index++)
		{
			ParentCmdList.QueueAsyncCommandListSubmit(Events[Index], CommandLists[Index]);
			NumAlloc--;
		}
	}
	CommandLists.Reset();
	Snapshot = nullptr;
	Events.Reset();
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FParallelCommandListSet_Dispatch_ServiceLocalQueue);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::RenderThread_Local);
}

FParallelCommandListSet::~FParallelCommandListSet()
{
	check(GOutstandingParallelCommandListSet == this);
	GOutstandingParallelCommandListSet = nullptr;

	check(IsInRenderingThread() && FMemStack::Get().GetNumMarks() == 1); // we do not want this popped before the end of the scene and it better be the scene allocator
	checkf(CommandLists.Num() == 0, TEXT("Derived class of FParallelCommandListSet did not call Dispatch in virtual destructor"));
	checkf(NumAlloc == 0, TEXT("Derived class of FParallelCommandListSet did not call Dispatch in virtual destructor"));
}

FRHICommandList* FParallelCommandListSet::NewParallelCommandList()
{
	FRHICommandList* Result = AllocCommandList();
	Result->ExecuteStat = ExecuteStat;
	SetStateOnCommandList(*Result);
	if (bCreateSceneContext)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(ParentCmdList);
		check(&SceneContext == &FSceneRenderTargets::Get_FrameConstantsOnly()); // the immediate should not have an overridden context
		if (!Snapshot)
		{
			Snapshot = SceneContext.CreateSnapshot(View);
		}
		Snapshot->SetSnapshotOnCmdList(*Result);
		check(&SceneContext != &FSceneRenderTargets::Get(*Result)); // the new commandlist should have a snapshot
	}
	return Result;
}

void FParallelCommandListSet::AddParallelCommandList(FRHICommandList* CmdList, FGraphEventRef& CompletionEvent, int32 InNumDrawsIfKnown)
{
	check(IsInRenderingThread() && FMemStack::Get().GetNumMarks() == 1); // we do not want this popped before the end of the scene and it better be the scene allocator
	check(CommandLists.Num() == Events.Num());
	CommandLists.Add(CmdList);
	Events.Add(CompletionEvent);
	NumDrawsIfKnown.Add(InNumDrawsIfKnown);
}

void FParallelCommandListSet::WaitForTasks()
{
	if (GOutstandingParallelCommandListSet)
	{
		GOutstandingParallelCommandListSet->WaitForTasksInternal();
	}
}

void FParallelCommandListSet::WaitForTasksInternal()
{
	check(IsInRenderingThread());
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FParallelCommandListSet_WaitForTasks);
	FGraphEventArray WaitOutstandingTasks;
	for (int32 Index = 0; Index < Events.Num(); Index++)
	{
		if (!Events[Index]->IsComplete())
		{
			WaitOutstandingTasks.Add(Events[Index]);
		}
	}
	if (WaitOutstandingTasks.Num())
	{
		check(!FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local));
		FTaskGraphInterface::Get().WaitUntilTasksComplete(WaitOutstandingTasks, ENamedThreads::RenderThread_Local);
	}
}



/*-----------------------------------------------------------------------------
	FViewInfo
-----------------------------------------------------------------------------*/

/** 
 * Initialization constructor. Passes all parameters to FSceneView constructor
 */
FViewInfo::FViewInfo(const FSceneViewInitOptions& InitOptions)
	:	FSceneView(InitOptions)
	,	IndividualOcclusionQueries((FSceneViewState*)InitOptions.SceneViewStateInterface, 1)
	,	GroupedOcclusionQueries((FSceneViewState*)InitOptions.SceneViewStateInterface, FOcclusionQueryBatcher::OccludedPrimitiveQueryBatchSize)
	// NVCHANGE_BEGIN: Add VXGI
	, CustomVisibilityQuery(nullptr)
	// NVCHANGE_END: Add VXGI
{
	Init();
}

/** 
 * Initialization constructor. 
 * @param InView - copy to init with
 */
FViewInfo::FViewInfo(const FSceneView* InView)
	:	FSceneView(*InView)
	,	IndividualOcclusionQueries((FSceneViewState*)InView->State,1)
	,	GroupedOcclusionQueries((FSceneViewState*)InView->State,FOcclusionQueryBatcher::OccludedPrimitiveQueryBatchSize)
	,	CustomVisibilityQuery(nullptr)
{
	Init();
}

void FViewInfo::Init()
{
	CachedViewUniformShaderParameters = nullptr;
	bHasNoVisiblePrimitive = false;
	bHasTranslucentViewMeshElements = 0;
	bPrevTransformsReset = false;
	bIgnoreExistingQueries = false;
	bDisableQuerySubmissions = false;
	bDisableDistanceBasedFadeTransitions = false;	
	ShadingModelMaskInView = 0;

	NumVisibleStaticMeshElements = 0;
	PrecomputedVisibilityData = 0;
	bSceneHasDecals = 0;

	bIsViewInfo = true;
	
	bUsesGlobalDistanceField = false;
	bUsesLightingChannels = false;
	bTranslucentSurfaceLighting = false;
	bUsesSceneDepth = false;

	ExponentialFogParameters = FVector4(0,1,1,0);
	ExponentialFogColor = FVector::ZeroVector;
	FogMaxOpacity = 1;
	ExponentialFogParameters3 = FVector4(0, 0, 0, 0);
	SinCosInscatteringColorCubemapRotation = FVector2D(0, 0);
	FogInscatteringColorCubemap = NULL;
	FogInscatteringTextureParameters = FVector::ZeroVector;

	bUseDirectionalInscattering = false;
	DirectionalInscatteringExponent = 0;
	DirectionalInscatteringStartDistance = 0;
	InscatteringLightDirection = FVector(0);
	DirectionalInscatteringColor = FLinearColor(ForceInit);

	for (int32 CascadeIndex = 0; CascadeIndex < TVC_MAX; CascadeIndex++)
	{
		TranslucencyLightingVolumeMin[CascadeIndex] = FVector(0);
		TranslucencyVolumeVoxelSize[CascadeIndex] = 0;
		TranslucencyLightingVolumeSize[CascadeIndex] = FVector(0);
	}

	const int32 MaxMobileShadowCascadeCount = FMath::Clamp(CVarMaxMobileShadowCascades.GetValueOnAnyThread(), 0, MAX_MOBILE_SHADOWCASCADES);
	const int32 MaxShadowCascadeCountUpperBound = GetFeatureLevel() >= ERHIFeatureLevel::SM4 ? 10 : MaxMobileShadowCascadeCount;

	MaxShadowCascades = FMath::Clamp<int32>(CVarMaxShadowCascades.GetValueOnAnyThread(), 0, MaxShadowCascadeCountUpperBound);

	ShaderMap = GetGlobalShaderMap(FeatureLevel);

	ViewState = (FSceneViewState*)State;
	bIsSnapshot = false;

	bAllowStencilDither = false;

	ForwardLightingResources = &ForwardLightingResourcesStorage;

	NumBoxReflectionCaptures = 0;
	NumSphereReflectionCaptures = 0;
	FurthestReflectionCaptureDistance = 0;

	// Disable HDR encoding for editor elements.
	EditorSimpleElementCollector.BatchedElements.EnableMobileHDREncoding(false);
}

FViewInfo::~FViewInfo()
{
	for(int32 ResourceIndex = 0;ResourceIndex < DynamicResources.Num();ResourceIndex++)
	{
		DynamicResources[ResourceIndex]->ReleasePrimitiveResource();
	}
	if (CustomVisibilityQuery)
	{
		CustomVisibilityQuery->Release();
	}
}

void FViewInfo::SetupSkyIrradianceEnvironmentMapConstants(FVector4* OutSkyIrradianceEnvironmentMap) const
{
	FScene* Scene = nullptr;

	if (Family->Scene)
	{
		Scene = Family->Scene->GetRenderScene();
	}

	if (Scene 
		&& Scene->SkyLight 
		// Skylights with static lighting already had their diffuse contribution baked into lightmaps
		&& !Scene->SkyLight->bHasStaticLighting
		&& Family->EngineShowFlags.SkyLighting)
	{
		const FSHVectorRGB3& SkyIrradiance = Scene->SkyLight->IrradianceEnvironmentMap;

		const float SqrtPI = FMath::Sqrt(PI);
		const float Coefficient0 = 1.0f / (2 * SqrtPI);
		const float Coefficient1 = FMath::Sqrt(3) / (3 * SqrtPI);
		const float Coefficient2 = FMath::Sqrt(15) / (8 * SqrtPI);
		const float Coefficient3 = FMath::Sqrt(5) / (16 * SqrtPI);
		const float Coefficient4 = .5f * Coefficient2;

		// Pack the SH coefficients in a way that makes applying the lighting use the least shader instructions
		// This has the diffuse convolution coefficients baked in
		// See "Stupid Spherical Harmonics (SH) Tricks"
		OutSkyIrradianceEnvironmentMap[0].X = -Coefficient1 * SkyIrradiance.R.V[3];
		OutSkyIrradianceEnvironmentMap[0].Y = -Coefficient1 * SkyIrradiance.R.V[1];
		OutSkyIrradianceEnvironmentMap[0].Z = Coefficient1 * SkyIrradiance.R.V[2];
		OutSkyIrradianceEnvironmentMap[0].W = Coefficient0 * SkyIrradiance.R.V[0] - Coefficient3 * SkyIrradiance.R.V[6];

		OutSkyIrradianceEnvironmentMap[1].X = -Coefficient1 * SkyIrradiance.G.V[3];
		OutSkyIrradianceEnvironmentMap[1].Y = -Coefficient1 * SkyIrradiance.G.V[1];
		OutSkyIrradianceEnvironmentMap[1].Z = Coefficient1 * SkyIrradiance.G.V[2];
		OutSkyIrradianceEnvironmentMap[1].W = Coefficient0 * SkyIrradiance.G.V[0] - Coefficient3 * SkyIrradiance.G.V[6];

		OutSkyIrradianceEnvironmentMap[2].X = -Coefficient1 * SkyIrradiance.B.V[3];
		OutSkyIrradianceEnvironmentMap[2].Y = -Coefficient1 * SkyIrradiance.B.V[1];
		OutSkyIrradianceEnvironmentMap[2].Z = Coefficient1 * SkyIrradiance.B.V[2];
		OutSkyIrradianceEnvironmentMap[2].W = Coefficient0 * SkyIrradiance.B.V[0] - Coefficient3 * SkyIrradiance.B.V[6];

		OutSkyIrradianceEnvironmentMap[3].X = Coefficient2 * SkyIrradiance.R.V[4];
		OutSkyIrradianceEnvironmentMap[3].Y = -Coefficient2 * SkyIrradiance.R.V[5];
		OutSkyIrradianceEnvironmentMap[3].Z = 3 * Coefficient3 * SkyIrradiance.R.V[6];
		OutSkyIrradianceEnvironmentMap[3].W = -Coefficient2 * SkyIrradiance.R.V[7];

		OutSkyIrradianceEnvironmentMap[4].X = Coefficient2 * SkyIrradiance.G.V[4];
		OutSkyIrradianceEnvironmentMap[4].Y = -Coefficient2 * SkyIrradiance.G.V[5];
		OutSkyIrradianceEnvironmentMap[4].Z = 3 * Coefficient3 * SkyIrradiance.G.V[6];
		OutSkyIrradianceEnvironmentMap[4].W = -Coefficient2 * SkyIrradiance.G.V[7];

		OutSkyIrradianceEnvironmentMap[5].X = Coefficient2 * SkyIrradiance.B.V[4];
		OutSkyIrradianceEnvironmentMap[5].Y = -Coefficient2 * SkyIrradiance.B.V[5];
		OutSkyIrradianceEnvironmentMap[5].Z = 3 * Coefficient3 * SkyIrradiance.B.V[6];
		OutSkyIrradianceEnvironmentMap[5].W = -Coefficient2 * SkyIrradiance.B.V[7];

		OutSkyIrradianceEnvironmentMap[6].X = Coefficient4 * SkyIrradiance.R.V[8];
		OutSkyIrradianceEnvironmentMap[6].Y = Coefficient4 * SkyIrradiance.G.V[8];
		OutSkyIrradianceEnvironmentMap[6].Z = Coefficient4 * SkyIrradiance.B.V[8];
		OutSkyIrradianceEnvironmentMap[6].W = 1;
	}
	else
	{
		FMemory::Memzero(OutSkyIrradianceEnvironmentMap, sizeof(FVector4) * 7);
	}
}

void UpdateNoiseTextureParameters(FViewUniformShaderParameters& ViewUniformShaderParameters)
{
	if (GSystemTextures.PerlinNoiseGradient.GetReference())
	{
		ViewUniformShaderParameters.PerlinNoiseGradientTexture = (FTexture2DRHIRef&)GSystemTextures.PerlinNoiseGradient->GetRenderTargetItem().ShaderResourceTexture;
		SetBlack2DIfNull(ViewUniformShaderParameters.PerlinNoiseGradientTexture);
	}
	check(ViewUniformShaderParameters.PerlinNoiseGradientTexture);
	ViewUniformShaderParameters.PerlinNoiseGradientTextureSampler = TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

	if (GSystemTextures.PerlinNoise3D.GetReference())
	{
		ViewUniformShaderParameters.PerlinNoise3DTexture = (FTexture3DRHIRef&)GSystemTextures.PerlinNoise3D->GetRenderTargetItem().ShaderResourceTexture;
		SetBlack3DIfNull(ViewUniformShaderParameters.PerlinNoise3DTexture);
	}
	check(ViewUniformShaderParameters.PerlinNoise3DTexture);
	ViewUniformShaderParameters.PerlinNoise3DTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

	if (GSystemTextures.SobolSampling.GetReference())
	{
		ViewUniformShaderParameters.SobolSamplingTexture = (FTexture2DRHIRef&)GSystemTextures.SobolSampling->GetRenderTargetItem().ShaderResourceTexture;
		SetBlack2DIfNull(ViewUniformShaderParameters.SobolSamplingTexture);
	}
	check(ViewUniformShaderParameters.SobolSamplingTexture);
}

void SetupPrecomputedVolumetricLightmapUniformBufferParameters(const FScene* Scene, FViewUniformShaderParameters& ViewUniformShaderParameters)
{
	if (Scene && Scene->VolumetricLightmapSceneData.GetLevelVolumetricLightmap())
	{
		const FPrecomputedVolumetricLightmapData* VolumetricLightmapData = Scene->VolumetricLightmapSceneData.GetLevelVolumetricLightmap()->Data;

		ViewUniformShaderParameters.VolumetricLightmapIndirectionTexture = OrBlack3DUintIfNull(VolumetricLightmapData->IndirectionTexture.Texture);
		ViewUniformShaderParameters.VolumetricLightmapBrickAmbientVector = OrBlack3DIfNull(VolumetricLightmapData->BrickData.AmbientVector.Texture);
		ViewUniformShaderParameters.VolumetricLightmapBrickSHCoefficients0 = OrBlack3DIfNull(VolumetricLightmapData->BrickData.SHCoefficients[0].Texture);
		ViewUniformShaderParameters.VolumetricLightmapBrickSHCoefficients1 = OrBlack3DIfNull(VolumetricLightmapData->BrickData.SHCoefficients[1].Texture);
		ViewUniformShaderParameters.VolumetricLightmapBrickSHCoefficients2 = OrBlack3DIfNull(VolumetricLightmapData->BrickData.SHCoefficients[2].Texture);
		ViewUniformShaderParameters.VolumetricLightmapBrickSHCoefficients3 = OrBlack3DIfNull(VolumetricLightmapData->BrickData.SHCoefficients[3].Texture);
		ViewUniformShaderParameters.VolumetricLightmapBrickSHCoefficients4 = OrBlack3DIfNull(VolumetricLightmapData->BrickData.SHCoefficients[4].Texture);
		ViewUniformShaderParameters.VolumetricLightmapBrickSHCoefficients5 = OrBlack3DIfNull(VolumetricLightmapData->BrickData.SHCoefficients[5].Texture);
		ViewUniformShaderParameters.SkyBentNormalBrickTexture = OrBlack3DIfNull(VolumetricLightmapData->BrickData.SkyBentNormal.Texture);
		ViewUniformShaderParameters.DirectionalLightShadowingBrickTexture = OrBlack3DIfNull(VolumetricLightmapData->BrickData.DirectionalLightShadowing.Texture);

		const FBox& VolumeBounds = VolumetricLightmapData->GetBounds();
		const FVector InvVolumeSize = FVector(1.0f) / VolumeBounds.GetSize();

		ViewUniformShaderParameters.VolumetricLightmapWorldToUVScale = InvVolumeSize;
		ViewUniformShaderParameters.VolumetricLightmapWorldToUVAdd = -VolumeBounds.Min * InvVolumeSize;
		ViewUniformShaderParameters.VolumetricLightmapIndirectionTextureSize = FVector(VolumetricLightmapData->IndirectionTextureDimensions);
		ViewUniformShaderParameters.VolumetricLightmapBrickSize = VolumetricLightmapData->BrickSize;
		ViewUniformShaderParameters.VolumetricLightmapBrickTexelSize = FVector(1.0f, 1.0f, 1.0f) / FVector(VolumetricLightmapData->BrickDataDimensions);
	}
	else
	{
		// Resources are initialized in FViewUniformShaderParameters ctor, only need to set defaults for non-resource types

		ViewUniformShaderParameters.VolumetricLightmapWorldToUVScale = FVector::ZeroVector;
		ViewUniformShaderParameters.VolumetricLightmapWorldToUVAdd = FVector::ZeroVector;
		ViewUniformShaderParameters.VolumetricLightmapIndirectionTextureSize = FVector::ZeroVector;
		ViewUniformShaderParameters.VolumetricLightmapBrickSize = 0;
		ViewUniformShaderParameters.VolumetricLightmapBrickTexelSize = FVector::ZeroVector;
	}
}

/** Creates the view's uniform buffers given a set of view transforms. */
void FViewInfo::SetupUniformBufferParameters(
	FSceneRenderTargets& SceneContext,
	const FViewMatrices& InViewMatrices,
	const FViewMatrices& InPrevViewMatrices,
	FBox* OutTranslucentCascadeBoundsArray, 
	int32 NumTranslucentCascades,
	FViewUniformShaderParameters& ViewUniformShaderParameters) const
{
	check(Family);

	// Create the view's uniform buffer.

	// Mobile multi-view is not side by side
	const FIntRect EffectiveViewRect = (bIsMobileMultiViewEnabled) ? FIntRect(0, 0, ViewRect.Width(), ViewRect.Height()) : ViewRect;

	// TODO: We should use a view and previous view uniform buffer to avoid code duplication and keep consistency
	SetupCommonViewUniformBufferParameters(
		ViewUniformShaderParameters, 
		SceneContext.GetBufferSizeXY(),
		SceneContext.GetMSAACount(),
		EffectiveViewRect,
		InViewMatrices,
		InPrevViewMatrices
	);

	const bool bCheckerboardSubsurfaceRendering = FRCPassPostProcessSubsurface::RequiresCheckerboardSubsurfaceRendering( SceneContext.GetSceneColorFormat() );
	ViewUniformShaderParameters.bCheckerboardSubsurfaceProfileRendering = bCheckerboardSubsurfaceRendering ? 1.0f : 0.0f;

	FScene* Scene = nullptr;

	if (Family->Scene)
	{
		Scene = Family->Scene->GetRenderScene();
	}	

	if (Scene)
	{		
		if (Scene->SimpleDirectionalLight)
		{			
			ViewUniformShaderParameters.DirectionalLightColor = Scene->SimpleDirectionalLight->Proxy->GetColor() / PI;
			ViewUniformShaderParameters.DirectionalLightDirection = -Scene->SimpleDirectionalLight->Proxy->GetDirection(); 
		}
		else
		{
			ViewUniformShaderParameters.DirectionalLightColor = FLinearColor::Black;
			ViewUniformShaderParameters.DirectionalLightDirection = FVector::ZeroVector;
		}

		// Atmospheric fog parameters
		if (ShouldRenderAtmosphere(*Family) && Scene->AtmosphericFog)
		{
			ViewUniformShaderParameters.AtmosphericFogSunPower = Scene->AtmosphericFog->SunMultiplier;
			ViewUniformShaderParameters.AtmosphericFogPower = Scene->AtmosphericFog->FogMultiplier;
			ViewUniformShaderParameters.AtmosphericFogDensityScale = Scene->AtmosphericFog->InvDensityMultiplier;
			ViewUniformShaderParameters.AtmosphericFogDensityOffset = Scene->AtmosphericFog->DensityOffset;
			ViewUniformShaderParameters.AtmosphericFogGroundOffset = Scene->AtmosphericFog->GroundOffset;
			ViewUniformShaderParameters.AtmosphericFogDistanceScale = Scene->AtmosphericFog->DistanceScale;
			ViewUniformShaderParameters.AtmosphericFogAltitudeScale = Scene->AtmosphericFog->AltitudeScale;
			ViewUniformShaderParameters.AtmosphericFogHeightScaleRayleigh = Scene->AtmosphericFog->RHeight;
			ViewUniformShaderParameters.AtmosphericFogStartDistance = Scene->AtmosphericFog->StartDistance;
			ViewUniformShaderParameters.AtmosphericFogDistanceOffset = Scene->AtmosphericFog->DistanceOffset;
			ViewUniformShaderParameters.AtmosphericFogSunDiscScale = Scene->AtmosphericFog->SunDiscScale;
			ViewUniformShaderParameters.AtmosphericFogSunColor = Scene->SunLight ? Scene->SunLight->Proxy->GetColor() : Scene->AtmosphericFog->DefaultSunColor;
			ViewUniformShaderParameters.AtmosphericFogSunDirection = Scene->SunLight ? -Scene->SunLight->Proxy->GetDirection() : -Scene->AtmosphericFog->DefaultSunDirection;
			ViewUniformShaderParameters.AtmosphericFogRenderMask = Scene->AtmosphericFog->RenderFlag & (EAtmosphereRenderFlag::E_DisableGroundScattering | EAtmosphereRenderFlag::E_DisableSunDisk);
			ViewUniformShaderParameters.AtmosphericFogInscatterAltitudeSampleNum = Scene->AtmosphericFog->InscatterAltitudeSampleNum;
		}
		else
		{
			ViewUniformShaderParameters.AtmosphericFogSunPower = 0.f;
			ViewUniformShaderParameters.AtmosphericFogPower = 0.f;
			ViewUniformShaderParameters.AtmosphericFogDensityScale = 0.f;
			ViewUniformShaderParameters.AtmosphericFogDensityOffset = 0.f;
			ViewUniformShaderParameters.AtmosphericFogGroundOffset = 0.f;
			ViewUniformShaderParameters.AtmosphericFogDistanceScale = 0.f;
			ViewUniformShaderParameters.AtmosphericFogAltitudeScale = 0.f;
			ViewUniformShaderParameters.AtmosphericFogHeightScaleRayleigh = 0.f;
			ViewUniformShaderParameters.AtmosphericFogStartDistance = 0.f;
			ViewUniformShaderParameters.AtmosphericFogDistanceOffset = 0.f;
			ViewUniformShaderParameters.AtmosphericFogSunDiscScale = 1.f;
			//Added check so atmospheric light color and vector can use a directional light without needing an atmospheric fog actor in the scene
			ViewUniformShaderParameters.AtmosphericFogSunColor = Scene->SunLight ? Scene->SunLight->Proxy->GetColor() : FLinearColor::Black;
			ViewUniformShaderParameters.AtmosphericFogSunDirection = Scene->SunLight ? -Scene->SunLight->Proxy->GetDirection() : FVector::ZeroVector;
			ViewUniformShaderParameters.AtmosphericFogRenderMask = EAtmosphereRenderFlag::E_EnableAll;
			ViewUniformShaderParameters.AtmosphericFogInscatterAltitudeSampleNum = 0;
		}
	}
	else
	{
		// Atmospheric fog parameters
		ViewUniformShaderParameters.AtmosphericFogSunPower = 0.f;
		ViewUniformShaderParameters.AtmosphericFogPower = 0.f;
		ViewUniformShaderParameters.AtmosphericFogDensityScale = 0.f;
		ViewUniformShaderParameters.AtmosphericFogDensityOffset = 0.f;
		ViewUniformShaderParameters.AtmosphericFogGroundOffset = 0.f;
		ViewUniformShaderParameters.AtmosphericFogDistanceScale = 0.f;
		ViewUniformShaderParameters.AtmosphericFogAltitudeScale = 0.f;
		ViewUniformShaderParameters.AtmosphericFogHeightScaleRayleigh = 0.f;
		ViewUniformShaderParameters.AtmosphericFogStartDistance = 0.f;
		ViewUniformShaderParameters.AtmosphericFogDistanceOffset = 0.f;
		ViewUniformShaderParameters.AtmosphericFogSunDiscScale = 1.f;
		ViewUniformShaderParameters.AtmosphericFogSunColor = FLinearColor::Black;
		ViewUniformShaderParameters.AtmosphericFogSunDirection = FVector::ZeroVector;
		ViewUniformShaderParameters.AtmosphericFogRenderMask = EAtmosphereRenderFlag::E_EnableAll;
		ViewUniformShaderParameters.AtmosphericFogInscatterAltitudeSampleNum = 0;
	}

	ViewUniformShaderParameters.AtmosphereTransmittanceTexture_UB = OrBlack2DIfNull(AtmosphereTransmittanceTexture);
	ViewUniformShaderParameters.AtmosphereIrradianceTexture_UB = OrBlack2DIfNull(AtmosphereIrradianceTexture);
	ViewUniformShaderParameters.AtmosphereInscatterTexture_UB = OrBlack3DIfNull(AtmosphereInscatterTexture);

	ViewUniformShaderParameters.AtmosphereTransmittanceTextureSampler_UB = TStaticSamplerState<SF_Bilinear>::GetRHI();
	ViewUniformShaderParameters.AtmosphereIrradianceTextureSampler_UB = TStaticSamplerState<SF_Bilinear>::GetRHI();
	ViewUniformShaderParameters.AtmosphereInscatterTextureSampler_UB = TStaticSamplerState<SF_Bilinear>::GetRHI();

	// This should probably be in SetupCommonViewUniformBufferParameters, but drags in too many dependencies
	UpdateNoiseTextureParameters(ViewUniformShaderParameters);

	SetupDefaultGlobalDistanceFieldUniformBufferParameters(ViewUniformShaderParameters);

	SetupVolumetricFogUniformBufferParameters(ViewUniformShaderParameters);

	SetupPrecomputedVolumetricLightmapUniformBufferParameters(Scene, ViewUniformShaderParameters);

	uint32 StateFrameIndexMod8 = 0;

	if(State)
	{
		ViewUniformShaderParameters.TemporalAAParams = FVector4(
			ViewState->GetCurrentTemporalAASampleIndex(), 
			ViewState->GetCurrentTemporalAASampleCount(),
			TemporalJitterPixelsX,
			TemporalJitterPixelsY);

		StateFrameIndexMod8 = ViewState->GetFrameIndexMod8();
	}
	else
	{
		ViewUniformShaderParameters.TemporalAAParams = FVector4(0, 1, 0, 0);
	}

	ViewUniformShaderParameters.StateFrameIndexMod8 = StateFrameIndexMod8;

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	if(!bIsVxgiVoxelization)
#endif
	// NVCHANGE_END: Add VXGI
	{
		// If rendering in stereo, the right eye uses the left eye's translucency lighting volume.
		const FViewInfo* PrimaryView = this;
		if (StereoPass == eSSP_RIGHT_EYE)
		{
			int32 ViewIndex = Family->Views.Find(this);
			if (Family->Views.IsValidIndex(ViewIndex) && Family->Views.IsValidIndex(ViewIndex - 1))
			{
				const FSceneView* LeftEyeView = Family->Views[ViewIndex - 1];
				if (LeftEyeView->bIsViewInfo && LeftEyeView->StereoPass == eSSP_LEFT_EYE)
				{
					PrimaryView = static_cast<const FViewInfo*>(LeftEyeView);
				}
			}
		}
		PrimaryView->CalcTranslucencyLightingVolumeBounds(OutTranslucentCascadeBoundsArray, NumTranslucentCascades);
    }
    // NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
    else
    {
        const FSceneView* PrimaryView = Family->Views[0];
        if (PrimaryView->bIsViewInfo)
        {
            const FViewInfo* PrimaryViewInfo = static_cast<const FViewInfo*>(PrimaryView);

            // Copy the view parameters that are used for tessellation factors from the primary view.
            ViewUniformShaderParameters.TranslatedWorldCameraOrigin = PrimaryViewInfo->CachedViewUniformShaderParameters->WorldCameraOrigin + CachedViewUniformShaderParameters->PreViewTranslation;
            ViewUniformShaderParameters.AdaptiveTessellationFactor = PrimaryViewInfo->CachedViewUniformShaderParameters->AdaptiveTessellationFactor;
        }
    }
#endif
    // NVCHANGE_END: Add VXGI

	for (int32 CascadeIndex = 0; CascadeIndex < NumTranslucentCascades; CascadeIndex++)
	{
		const float VolumeVoxelSize = (OutTranslucentCascadeBoundsArray[CascadeIndex].Max.X - OutTranslucentCascadeBoundsArray[CascadeIndex].Min.X) / GTranslucencyLightingVolumeDim;
		const FVector VolumeSize = OutTranslucentCascadeBoundsArray[CascadeIndex].Max - OutTranslucentCascadeBoundsArray[CascadeIndex].Min;
		ViewUniformShaderParameters.TranslucencyLightingVolumeMin[CascadeIndex] = FVector4(OutTranslucentCascadeBoundsArray[CascadeIndex].Min, 1.0f / GTranslucencyLightingVolumeDim);
		ViewUniformShaderParameters.TranslucencyLightingVolumeInvSize[CascadeIndex] = FVector4(FVector(1.0f) / VolumeSize, VolumeVoxelSize);
	}
	
	float ExposureScale = FRCPassPostProcessEyeAdaptation::ComputeExposureScaleValue( *this );
	ViewUniformShaderParameters.ExposureScale = ExposureScale; // Only used for MobileHDR == false
	ViewUniformShaderParameters.DepthOfFieldFocalDistance = FinalPostProcessSettings.DepthOfFieldFocalDistance;
	ViewUniformShaderParameters.DepthOfFieldSensorWidth = FinalPostProcessSettings.DepthOfFieldSensorWidth;
	ViewUniformShaderParameters.DepthOfFieldFocalRegion = FinalPostProcessSettings.DepthOfFieldFocalRegion;
	// clamped to avoid div by 0 in shader
	ViewUniformShaderParameters.DepthOfFieldNearTransitionRegion = FMath::Max(0.01f, FinalPostProcessSettings.DepthOfFieldNearTransitionRegion);
	// clamped to avoid div by 0 in shader
	ViewUniformShaderParameters.DepthOfFieldFarTransitionRegion = FMath::Max(0.01f, FinalPostProcessSettings.DepthOfFieldFarTransitionRegion);
	ViewUniformShaderParameters.DepthOfFieldScale = FinalPostProcessSettings.DepthOfFieldScale;
	ViewUniformShaderParameters.DepthOfFieldFocalLength = 50.0f;

	ViewUniformShaderParameters.bSubsurfacePostprocessEnabled = GCompositionLighting.IsSubsurfacePostprocessRequired() ? 1.0f : 0.0f;

	{
		// This is the CVar default
		float Value = 1.0f;

		// Compiled out in SHIPPING to make cheating a bit harder.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		Value = CVarGeneralPurposeTweak.GetValueOnRenderThread();
#endif

		ViewUniformShaderParameters.GeneralPurposeTweak = Value;
	}

	ViewUniformShaderParameters.DemosaicVposOffset = 0.0f;
	{
		ViewUniformShaderParameters.DemosaicVposOffset = CVarDemosaicVposOffset.GetValueOnRenderThread();
	}

	ViewUniformShaderParameters.IndirectLightingColorScale = FVector(FinalPostProcessSettings.IndirectLightingColor.R * FinalPostProcessSettings.IndirectLightingIntensity,
		FinalPostProcessSettings.IndirectLightingColor.G * FinalPostProcessSettings.IndirectLightingIntensity,
		FinalPostProcessSettings.IndirectLightingColor.B * FinalPostProcessSettings.IndirectLightingIntensity);

	ViewUniformShaderParameters.NormalCurvatureToRoughnessScaleBias.X = FMath::Clamp(CVarNormalCurvatureToRoughnessScale.GetValueOnAnyThread(), 0.0f, 2.0f);
	ViewUniformShaderParameters.NormalCurvatureToRoughnessScaleBias.Y = FMath::Clamp(CVarNormalCurvatureToRoughnessBias.GetValueOnAnyThread(), -1.0f, 1.0f);
	ViewUniformShaderParameters.NormalCurvatureToRoughnessScaleBias.Z = FMath::Clamp(CVarNormalCurvatureToRoughnessExponent.GetValueOnAnyThread(), .05f, 20.0f);

	ViewUniformShaderParameters.RenderingReflectionCaptureMask = bIsReflectionCapture ? 1.0f : 0.0f;

	ViewUniformShaderParameters.AmbientCubemapTint = FinalPostProcessSettings.AmbientCubemapTint;
	ViewUniformShaderParameters.AmbientCubemapIntensity = FinalPostProcessSettings.AmbientCubemapIntensity;

	{
		// Enables HDR encoding mode selection without recompile of all PC shaders during ES2 emulation.
		ViewUniformShaderParameters.HDR32bppEncodingMode = 0.0f;
		if (IsMobileHDR32bpp())
		{
			EMobileHDRMode MobileHDRMode = GetMobileHDRMode();
			switch (MobileHDRMode)
			{
				case EMobileHDRMode::EnabledMosaic:
					ViewUniformShaderParameters.HDR32bppEncodingMode = 1.0f;
				break;
				case EMobileHDRMode::EnabledRGBE:
					ViewUniformShaderParameters.HDR32bppEncodingMode = 2.0f;
				break;
				case EMobileHDRMode::EnabledRGBA8:
					ViewUniformShaderParameters.HDR32bppEncodingMode = 3.0f;
					break;
				default:
					checkNoEntry();
				break;
			}
		}
	}

	ViewUniformShaderParameters.CircleDOFParams = CircleDofHalfCoc(*this);

	if (Family->Scene)
	{
		Scene = Family->Scene->GetRenderScene();
	}

	ERHIFeatureLevel::Type RHIFeatureLevel = Scene == nullptr ? GMaxRHIFeatureLevel : Scene->GetFeatureLevel();

	if (Scene && Scene->SkyLight)
	{
		FSkyLightSceneProxy* SkyLight = Scene->SkyLight;

		ViewUniformShaderParameters.SkyLightColor = SkyLight->LightColor;

		bool bApplyPrecomputedBentNormalShadowing = 
			SkyLight->bCastShadows 
			&& SkyLight->bWantsStaticShadowing;

		ViewUniformShaderParameters.SkyLightParameters = bApplyPrecomputedBentNormalShadowing ? 1 : 0;
	}
	else
	{
		ViewUniformShaderParameters.SkyLightColor = FLinearColor::Black;
		ViewUniformShaderParameters.SkyLightParameters = 0;
	}

	// Make sure there's no padding since we're going to cast to FVector4*
	checkSlow(sizeof(ViewUniformShaderParameters.SkyIrradianceEnvironmentMap) == sizeof(FVector4)* 7);
	SetupSkyIrradianceEnvironmentMapConstants((FVector4*)&ViewUniformShaderParameters.SkyIrradianceEnvironmentMap);

	ViewUniformShaderParameters.MobilePreviewMode =
		(GIsEditor &&
		(RHIFeatureLevel == ERHIFeatureLevel::ES2 || RHIFeatureLevel == ERHIFeatureLevel::ES3_1) &&
		GMaxRHIFeatureLevel > ERHIFeatureLevel::ES3_1) ? 1.0f : 0.0f;

	// Padding between the left and right eye may be introduced by an HMD, which instanced stereo needs to account for.
	if ((Family != nullptr) && (StereoPass != eSSP_FULL) && (Family->Views.Num() > 1))
	{
		check(Family->Views.Num() >= 2);
		const float FamilySizeX = static_cast<float>(Family->InstancedStereoWidth);
		const float EyePaddingSize = static_cast<float>(Family->Views[1]->ViewRect.Min.X - Family->Views[0]->ViewRect.Max.X);
		ViewUniformShaderParameters.HMDEyePaddingOffset = (FamilySizeX - EyePaddingSize) / FamilySizeX;
	}
	else
	{
		ViewUniformShaderParameters.HMDEyePaddingOffset = 1.0f;
	}

	ViewUniformShaderParameters.ReflectionCubemapMaxMip = FMath::FloorLog2(UReflectionCaptureComponent::GetReflectionCaptureSize_RenderThread());

	ViewUniformShaderParameters.ShowDecalsMask = Family->EngineShowFlags.Decals ? 1.0f : 0.0f;

	extern int32 GDistanceFieldAOSpecularOcclusionMode;
	ViewUniformShaderParameters.DistanceFieldAOSpecularOcclusionMode = GDistanceFieldAOSpecularOcclusionMode;

	ViewUniformShaderParameters.IndirectCapsuleSelfShadowingIntensity = Scene ? Scene->DynamicIndirectShadowsSelfShadowingIntensity : 1.0f;

	extern FVector GetReflectionEnvironmentRoughnessMixingScaleBiasAndLargestWeight();
	ViewUniformShaderParameters.ReflectionEnvironmentRoughnessMixingScaleBiasAndLargestWeight = GetReflectionEnvironmentRoughnessMixingScaleBiasAndLargestWeight();

	ViewUniformShaderParameters.StereoPassIndex = (StereoPass != eSSP_RIGHT_EYE) ? 0 : 1;
}

void FViewInfo::InitRHIResources()
{
	FBox VolumeBounds[TVC_MAX];

	check(IsInRenderingThread());

	CachedViewUniformShaderParameters = MakeUnique<FViewUniformShaderParameters>();

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(FRHICommandListExecutor::GetImmediateCommandList());

	SetupUniformBufferParameters(
		SceneContext,
		VolumeBounds,
		TVC_MAX,
		*CachedViewUniformShaderParameters);

	ViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(*CachedViewUniformShaderParameters, UniformBuffer_SingleFrame);

	for (int32 CascadeIndex = 0; CascadeIndex < TVC_MAX; CascadeIndex++)
	{
		TranslucencyLightingVolumeMin[CascadeIndex] = VolumeBounds[CascadeIndex].Min;
		TranslucencyVolumeVoxelSize[CascadeIndex] = (VolumeBounds[CascadeIndex].Max.X - VolumeBounds[CascadeIndex].Min.X) / GTranslucencyLightingVolumeDim;
		TranslucencyLightingVolumeSize[CascadeIndex] = VolumeBounds[CascadeIndex].Max - VolumeBounds[CascadeIndex].Min;
	}

	// Initialize the dynamic resources used by the view's FViewElementDrawer.
	for(int32 ResourceIndex = 0;ResourceIndex < DynamicResources.Num();ResourceIndex++)
	{
		DynamicResources[ResourceIndex]->InitPrimitiveResource();
	}
}

// These are not real view infos, just dumb memory blocks
static TArray<FViewInfo*> ViewInfoSnapshots;
// these are never freed, even at program shutdown
static TArray<FViewInfo*> FreeViewInfoSnapshots;

extern TUniformBufferRef<FMobileDirectionalLightShaderParameters>& GetNullMobileDirectionalLightShaderParameters();

FViewInfo* FViewInfo::CreateSnapshot() const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FViewInfo_CreateSnapshot);

	check(IsInRenderingThread()); // we do not want this popped before the end of the scene and it better be the scene allocator
	FViewInfo* Result;
	if (FreeViewInfoSnapshots.Num())
	{
		Result = FreeViewInfoSnapshots.Pop(false);
	}
	else
	{
		Result = (FViewInfo*)FMemory::Malloc(sizeof(FViewInfo), alignof(FViewInfo));
	}
	FMemory::Memcpy(*Result, *this);

	// we want these to start null without a reference count, since we clear a ref later
	TUniformBufferRef<FViewUniformShaderParameters> NullViewUniformBuffer;
	FMemory::Memcpy(Result->ViewUniformBuffer, NullViewUniformBuffer); 
	FMemory::Memcpy(Result->DownsampledTranslucencyViewUniformBuffer, NullViewUniformBuffer);
	TUniformBufferRef<FMobileDirectionalLightShaderParameters> NullMobileDirectionalLightUniformBuffer;
	for (size_t i = 0; i < ARRAY_COUNT(Result->MobileDirectionalLightUniformBuffers); i++)
	{
		// This memcpy is necessary to clear the reference from the memcpy of the whole of this -> Result without releasing the pointer
		FMemory::Memcpy(Result->MobileDirectionalLightUniformBuffers[i], NullMobileDirectionalLightUniformBuffer);
		
		// But what we really want is the null buffer.
		Result->MobileDirectionalLightUniformBuffers[i] = GetNullMobileDirectionalLightShaderParameters();
	}

	TUniquePtr<FViewUniformShaderParameters> NullViewParameters;
	FMemory::Memcpy(Result->CachedViewUniformShaderParameters, NullViewParameters); 
	Result->bIsSnapshot = true;
	ViewInfoSnapshots.Add(Result);
	return Result;
}

void FViewInfo::DestroyAllSnapshots()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FViewInfo_DestroyAllSnapshots);

	check(IsInRenderingThread());
	// we will only keep double the number actually used, plus a few
	int32 NumToRemove = FreeViewInfoSnapshots.Num() - (ViewInfoSnapshots.Num() + 2);
	if (NumToRemove > 0)
	{
		for (int32 Index = 0; Index < NumToRemove; Index++)
		{
			FMemory::Free(FreeViewInfoSnapshots[Index]);
		}
		FreeViewInfoSnapshots.RemoveAt(0, NumToRemove, false);
	}
	for (FViewInfo* Snapshot : ViewInfoSnapshots)
	{
		Snapshot->ViewUniformBuffer.SafeRelease();
		Snapshot->CachedViewUniformShaderParameters.Reset();
		FreeViewInfoSnapshots.Add(Snapshot);
	}
	ViewInfoSnapshots.Reset();
}

FSceneViewState* FViewInfo::GetEffectiveViewState() const
{
	FSceneViewState* EffectiveViewState = ViewState;

	// When rendering in stereo we want to use the same exposure for both eyes.
	if (StereoPass == eSSP_RIGHT_EYE)
	{
		int32 ViewIndex = Family->Views.Find(this);
		if (Family->Views.IsValidIndex(ViewIndex))
		{
			// The left eye is always added before the right eye.
			ViewIndex = ViewIndex - 1;
			if (Family->Views.IsValidIndex(ViewIndex))
			{
				const FSceneView* PrimaryView = Family->Views[ViewIndex];
				if (PrimaryView->StereoPass == eSSP_LEFT_EYE)
				{
					EffectiveViewState = (FSceneViewState*)PrimaryView->State;
				}
			}
		}
	}

	return EffectiveViewState;
}

IPooledRenderTarget* FViewInfo::GetEyeAdaptation(FRHICommandList& RHICmdList) const
{
	return GetEyeAdaptationRT(RHICmdList);
}

IPooledRenderTarget* FViewInfo::GetEyeAdaptationRT(FRHICommandList& RHICmdList) const
{
	FSceneViewState* EffectiveViewState = GetEffectiveViewState();
	IPooledRenderTarget* Result = NULL;
	if (EffectiveViewState)
	{
		Result = EffectiveViewState->GetCurrentEyeAdaptationRT(RHICmdList);
	}
	return Result;
}

IPooledRenderTarget* FViewInfo::GetLastEyeAdaptationRT(FRHICommandList& RHICmdList) const
{
	FSceneViewState* EffectiveViewState = GetEffectiveViewState();
	IPooledRenderTarget* Result = NULL;
	if (EffectiveViewState)
	{
		Result = EffectiveViewState->GetLastEyeAdaptationRT(RHICmdList);
	}
	return Result;
}

void FViewInfo::SwapEyeAdaptationRTs() const
{
	FSceneViewState* EffectiveViewState = GetEffectiveViewState();
	if (EffectiveViewState)
	{
		EffectiveViewState->SwapEyeAdaptationRTs();
	}
}

bool FViewInfo::HasValidEyeAdaptation() const
{
	FSceneViewState* EffectiveViewState = GetEffectiveViewState();	

	if (EffectiveViewState)
	{
		return EffectiveViewState->HasValidEyeAdaptation();
	}
	return false;
}

void FViewInfo::SetValidEyeAdaptation() const
{
	FSceneViewState* EffectiveViewState = GetEffectiveViewState();	

	if (EffectiveViewState)
	{
		EffectiveViewState->SetValidEyeAdaptation();
	}
}

void FViewInfo::SetValidTonemappingLUT() const
{
	FSceneViewState* EffectiveViewState = GetEffectiveViewState();
	if (EffectiveViewState) EffectiveViewState->SetValidTonemappingLUT();
}

const FTextureRHIRef* FViewInfo::GetTonemappingLUTTexture() const
{
	const FTextureRHIRef* TextureRHIRef = NULL;
	FSceneViewState* EffectiveViewState = GetEffectiveViewState();
	if (EffectiveViewState && EffectiveViewState->HasValidTonemappingLUT())
	{
		TextureRHIRef = EffectiveViewState->GetTonemappingLUTTexture();
	}
	return TextureRHIRef;
};

FSceneRenderTargetItem* FViewInfo::GetTonemappingLUTRenderTarget(FRHICommandList& RHICmdList, const int32 LUTSize, const bool bUseVolumeLUT, const bool bNeedUAV) const 
{
	FSceneRenderTargetItem* TargetItem = NULL;
	FSceneViewState* EffectiveViewState = GetEffectiveViewState();
	if (EffectiveViewState)
	{
		TargetItem = &(EffectiveViewState->GetTonemappingLUTRenderTarget(RHICmdList, LUTSize, bUseVolumeLUT, bNeedUAV));
	}
	return TargetItem;
}


void FDisplayInternalsData::Setup(UWorld *World)
{
	DisplayInternalsCVarValue = 0;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	DisplayInternalsCVarValue = CVarDisplayInternals.GetValueOnGameThread();

	if(IsValid())
	{
		MatineeTime = -1.0f;
		uint32 Count = 0;

		for (TObjectIterator<AMatineeActor> It; It; ++It)
		{
			AMatineeActor* MatineeActor = *It;

			if(MatineeActor->GetWorld() == World && MatineeActor->bIsPlaying)
			{
				MatineeTime = MatineeActor->InterpPosition;
				++Count;
			}
		}

		if(Count > 1)
		{
			MatineeTime = -2;
		}

		check(IsValid());
		
		extern ENGINE_API uint32 GStreamAllResourcesStillInFlight;
		NumPendingStreamingRequests = GStreamAllResourcesStillInFlight;
	}
#endif
}

void FSortedShadowMaps::Release()
{
	for (int32 AtlasIndex = 0; AtlasIndex < ShadowMapAtlases.Num(); AtlasIndex++)
	{
		ShadowMapAtlases[AtlasIndex].RenderTargets.Release();
	}

	for (int32 AtlasIndex = 0; AtlasIndex < RSMAtlases.Num(); AtlasIndex++)
	{
		RSMAtlases[AtlasIndex].RenderTargets.Release();
	}

	for (int32 AtlasIndex = 0; AtlasIndex < ShadowMapCubemaps.Num(); AtlasIndex++)
	{
		ShadowMapCubemaps[AtlasIndex].RenderTargets.Release();
	}

	PreshadowCache.RenderTargets.Release();
}

/*-----------------------------------------------------------------------------
	FSceneRenderer
-----------------------------------------------------------------------------*/

FSceneRenderer::FSceneRenderer(const FSceneViewFamily* InViewFamily,FHitProxyConsumer* HitProxyConsumer)
:	Scene(InViewFamily->Scene ? InViewFamily->Scene->GetRenderScene() : NULL)
,	ViewFamily(*InViewFamily)
,	bUsedPrecomputedVisibility(false)
// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
,	VxgiView(nullptr)
#endif
// NVCHANGE_END: Add VXGI
{
	check(Scene != NULL);

	check(IsInGameThread());
	ViewFamily.FrameNumber = Scene ? Scene->GetFrameNumber() : GFrameNumber;

	// Copy the individual views.
	bool bAnyViewIsLocked = false;
	Views.Empty(InViewFamily->Views.Num());
	for(int32 ViewIndex = 0;ViewIndex < InViewFamily->Views.Num();ViewIndex++)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		for(int32 ViewIndex2 = 0;ViewIndex2 < InViewFamily->Views.Num();ViewIndex2++)
		{
			if (ViewIndex != ViewIndex2 && InViewFamily->Views[ViewIndex]->State != NULL)
			{
				// Verify that each view has a unique view state, as the occlusion query mechanism depends on it.
				check(InViewFamily->Views[ViewIndex]->State != InViewFamily->Views[ViewIndex2]->State);
			}
		}
#endif
		// Construct a FViewInfo with the FSceneView properties.
		FViewInfo* ViewInfo = new(Views) FViewInfo(InViewFamily->Views[ViewIndex]);
		ViewFamily.Views[ViewIndex] = ViewInfo;
		ViewInfo->Family = &ViewFamily;
		bAnyViewIsLocked |= ViewInfo->bIsLocked;

#if WITH_EDITOR
		// Should we allow the user to select translucent primitives?
		ViewInfo->bAllowTranslucentPrimitivesInHitProxy =
			GEngine->AllowSelectTranslucent() ||		// User preference enabled?
			!ViewInfo->IsPerspectiveProjection();		// Is orthographic view?
#endif

		// Batch the view's elements for later rendering.
		if(ViewInfo->Drawer)
		{
			FViewElementPDI ViewElementPDI(ViewInfo,HitProxyConsumer);
			ViewInfo->Drawer->Draw(ViewInfo,&ViewElementPDI);
		}
	}

	// If any viewpoint has been locked, set time to zero to avoid time-based
	// rendering differences in materials.
	if (bAnyViewIsLocked)
	{
		ViewFamily.CurrentRealTime = 0.0f;
		ViewFamily.CurrentWorldTime = 0.0f;
	}
	
	if(HitProxyConsumer)
	{
		// Set the hit proxies show flag.
		ViewFamily.EngineShowFlags.SetHitProxies(1);
	}

	// launch custom visibility queries for views
	if (GCustomCullingImpl)
	{
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			FViewInfo& ViewInfo = Views[ViewIndex];
			ViewInfo.CustomVisibilityQuery = GCustomCullingImpl->CreateQuery(ViewInfo);
		}
	}

	ViewFamily.ComputeFamilySize();

	// copy off the requests
	// (I apologize for the const_cast, but didn't seem worth refactoring just for the freezerendering command)
	bHasRequestedToggleFreeze = const_cast<FRenderTarget*>(InViewFamily->RenderTarget)->HasToggleFreezeCommand();

	FeatureLevel = Scene->GetFeatureLevel();
}

bool FSceneRenderer::DoOcclusionQueries(ERHIFeatureLevel::Type InFeatureLevel) const
{
	return !IsMobilePlatform(GShaderPlatformForFeatureLevel[InFeatureLevel])
		&& CVarAllowOcclusionQueries.GetValueOnRenderThread() != 0;
}

FSceneRenderer::~FSceneRenderer()
{
	// To prevent keeping persistent references to single frame buffers, clear any such reference at this point.
	ClearPrimitiveSingleFramePrecomputedLightingBuffers();

	if(Scene)
	{
		// Destruct the projected shadow infos.
		for(TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights);LightIt;++LightIt)
		{
			if( VisibleLightInfos.IsValidIndex(LightIt.GetIndex()) )
			{
				FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightIt.GetIndex()];
				for(int32 ShadowIndex = 0;ShadowIndex < VisibleLightInfo.MemStackProjectedShadows.Num();ShadowIndex++)
				{
					// FProjectedShadowInfo's in MemStackProjectedShadows were allocated on the rendering thread mem stack, 
					// Their memory will be freed when the stack is freed with no destructor call, so invoke the destructor explicitly
					VisibleLightInfo.MemStackProjectedShadows[ShadowIndex]->~FProjectedShadowInfo();
				}
			}
		}
	}

	// Manually release references to TRefCountPtrs that are allocated on the mem stack, which doesn't call dtors
	SortedShadowsForShadowDepthPass.Release();
}

/** 
* Finishes the view family rendering.
*/
void FSceneRenderer::RenderFinish(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, RenderFinish);

	if(FRCPassPostProcessBusyWait::IsPassRequired())
	{
		// mostly view independent but to be safe we use the first view
		FViewInfo& View = Views[0];

		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FRenderingCompositeOutputRef BusyWait;
		{
			// for debugging purpose, can be controlled by console variable
			FRenderingCompositePass* Node = CompositeContext.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBusyWait());
			BusyWait = FRenderingCompositeOutputRef(Node);
		}		
		
		if(BusyWait.IsValid())
		{
			CompositeContext.Process(BusyWait.GetPass(), TEXT("RenderFinish"));
		}
	}
	
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	{
		bool bShowPrecomputedVisibilityWarning = false;
		static const auto* CVarPrecomputedVisibilityWarning = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PrecomputedVisibilityWarning"));
		if (CVarPrecomputedVisibilityWarning && CVarPrecomputedVisibilityWarning->GetValueOnRenderThread() == 1)
		{
			bShowPrecomputedVisibilityWarning = !bUsedPrecomputedVisibility;
		}

		bool bShowGlobalClipPlaneWarning = false;

		if (Scene->PlanarReflections.Num() > 0)
		{
			static const auto* CVarClipPlane = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowGlobalClipPlane"));
			if (CVarClipPlane && CVarClipPlane->GetValueOnRenderThread() == 0)
			{
				bShowGlobalClipPlaneWarning = true;
			}
		}
		
		const FReadOnlyCVARCache& ReadOnlyCVARCache = Scene->ReadOnlyCVARCache;
		static auto* CVarSkinCacheOOM = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.SkinCache.SceneMemoryLimitInMB"));

		uint64 GPUSkinCacheExtraRequiredMemory = 0;
		extern ENGINE_API bool IsGPUSkinCacheAvailable();
		if (IsGPUSkinCacheAvailable())
		{
			if (FGPUSkinCache* SkinCache = Scene->GetGPUSkinCache())
			{
				GPUSkinCacheExtraRequiredMemory = SkinCache->GetExtraRequiredMemoryAndReset();
			}
		}
		const bool bShowSkinCacheOOM = CVarSkinCacheOOM != nullptr && GPUSkinCacheExtraRequiredMemory > 0;

		extern int32 GDistanceFieldAO;
		const bool bShowDFAODisabledWarning = !GDistanceFieldAO && (ViewFamily.EngineShowFlags.VisualizeMeshDistanceFields || ViewFamily.EngineShowFlags.VisualizeGlobalDistanceField || ViewFamily.EngineShowFlags.VisualizeDistanceFieldAO);

		const bool bShowAtmosphericFogWarning = Scene->AtmosphericFog != nullptr && !ReadOnlyCVARCache.bEnableAtmosphericFog;

		const bool bStationarySkylight = Scene->SkyLight && Scene->SkyLight->bWantsStaticShadowing;
		const bool bShowSkylightWarning = bStationarySkylight && !ReadOnlyCVARCache.bEnableStationarySkylight;

		const bool bShowPointLightWarning = UsedWholeScenePointLightNames.Num() > 0 && !ReadOnlyCVARCache.bEnablePointLightShadows;
		const bool bShowShadowedLightOverflowWarning = Scene->OverflowingDynamicShadowedLights.Num() > 0;

		// Mobile-specific warnings
		const bool bMobile = (FeatureLevel <= ERHIFeatureLevel::ES3_1);
		const bool bShowMobileLowQualityLightmapWarning = bMobile && !ReadOnlyCVARCache.bEnableLowQualityLightmaps && ReadOnlyCVARCache.bAllowStaticLighting;
		const bool bShowMobileDynamicCSMWarning = bMobile && Scene->NumMobileStaticAndCSMLights_RenderThread > 0 && !(ReadOnlyCVARCache.bMobileEnableStaticAndCSMShadowReceivers && ReadOnlyCVARCache.bMobileAllowDistanceFieldShadows);
		const bool bShowMobileMovableDirectionalLightWarning = bMobile && Scene->NumMobileMovableDirectionalLights_RenderThread > 0 && !ReadOnlyCVARCache.bMobileAllowMovableDirectionalLights;
		
		bool bMobileShowVertexFogWarning = false;
		if (bMobile && Scene->ExponentialFogs.Num() > 0)
		{
			static const auto* CVarDisableVertexFog = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.DisableVertexFog"));
			if (CVarDisableVertexFog && CVarDisableVertexFog->GetValueOnRenderThread() != 0)
			{
				bMobileShowVertexFogWarning = true;
			}
		}
		
		const bool bAnyWarning = bShowPrecomputedVisibilityWarning || bShowGlobalClipPlaneWarning || bShowAtmosphericFogWarning || bShowSkylightWarning || bShowPointLightWarning || bShowDFAODisabledWarning || bShowShadowedLightOverflowWarning || bShowMobileDynamicCSMWarning || bShowMobileLowQualityLightmapWarning || bShowMobileMovableDirectionalLightWarning || bMobileShowVertexFogWarning || bShowSkinCacheOOM;

		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{	
			FViewInfo& View = Views[ViewIndex];
			if (!View.bIsReflectionCapture && !View.bIsSceneCapture )
			{
				// display a message saying we're frozen
				FSceneViewState* ViewState = (FSceneViewState*)View.State;
				bool bViewParentOrFrozen = ViewState && (ViewState->HasViewParent() || ViewState->bIsFrozen);
				bool bLocked = View.bIsLocked;
				if (bViewParentOrFrozen || bLocked || bAnyWarning)
				{
					SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

					FRenderTargetTemp TempRenderTarget(View);

					// create a temporary FCanvas object with the temp render target
					// so it can get the screen size
					int32 Y = 130;
					FCanvas Canvas(&TempRenderTarget, NULL, View.Family->CurrentRealTime, View.Family->CurrentWorldTime, View.Family->DeltaWorldTime, FeatureLevel);
					// Make sure draws to the canvas are not rendered upside down.
					Canvas.SetAllowSwitchVerticalAxis(false);
					if (bViewParentOrFrozen)
					{
						const FText StateText =
							ViewState->bIsFrozen ?
							NSLOCTEXT("SceneRendering", "RenderingFrozen", "Rendering frozen...")
							:
							NSLOCTEXT("SceneRendering", "OcclusionChild", "Occlusion Child");
						Canvas.DrawShadowedText(10, Y, StateText, GetStatsFont(), FLinearColor(0.8, 1.0, 0.2, 1.0));
						Y += 14;
					}
					if (bShowPrecomputedVisibilityWarning)
					{
						static const FText Message = NSLOCTEXT("Renderer", "NoPrecomputedVisibility", "NO PRECOMPUTED VISIBILITY");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;
					}
					if (bShowGlobalClipPlaneWarning)
					{
						static const FText Message = NSLOCTEXT("Renderer", "NoGlobalClipPlane", "PLANAR REFLECTION REQUIRES GLOBAL CLIP PLANE PROJECT SETTING ENABLED TO WORK PROPERLY");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;
					}
					if (bShowDFAODisabledWarning)
					{
						static const FText Message = NSLOCTEXT("Renderer", "DFAODisabled", "Distance Field AO is disabled through scalability");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;
					}

					if (bShowAtmosphericFogWarning)
					{
						static const FText Message = NSLOCTEXT("Renderer", "AtmosphericFog", "PROJECT DOES NOT SUPPORT ATMOSPHERIC FOG");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;						
					}
					if (bShowSkylightWarning)
					{
						static const FText Message = NSLOCTEXT("Renderer", "Skylight", "PROJECT DOES NOT SUPPORT STATIONARY SKYLIGHT: ");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;
					}
					if (bShowPointLightWarning)
					{
						static const FText Message = NSLOCTEXT("Renderer", "PointLight", "PROJECT DOES NOT SUPPORT WHOLE SCENE POINT LIGHT SHADOWS: ");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;

						for (FName LightName : UsedWholeScenePointLightNames)
						{
							Canvas.DrawShadowedText(10, Y, FText::FromString(LightName.ToString()), GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
							Y += 14;
						}
					}
					if (bShowShadowedLightOverflowWarning)
					{
						static const FText Message = NSLOCTEXT("Renderer", "ShadowedLightOverflow", "TOO MANY OVERLAPPING SHADOWED MOVABLE LIGHTS, SHADOW CASTING DISABLED: ");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;

						for (FName LightName : Scene->OverflowingDynamicShadowedLights)
						{
							Canvas.DrawShadowedText(10, Y, FText::FromString(LightName.ToString()), GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
							Y += 14;
						}
					}
					if (bShowMobileLowQualityLightmapWarning)
					{
						static const FText Message = NSLOCTEXT("Renderer", "MobileLQLightmap", "MOBILE PROJECTS SUPPORTING STATIC LIGHTING MUST HAVE LQ LIGHTMAPS ENABLED");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;
					}
					if (bShowMobileMovableDirectionalLightWarning)
					{
						static const FText Message = NSLOCTEXT("Renderer", "MobileMovableDirectional", "PROJECT HAS MOVABLE DIRECTIONAL LIGHTS ON MOBILE DISABLED");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;
					}
					if (bShowMobileDynamicCSMWarning)
					{
						static const FText Message = (!ReadOnlyCVARCache.bMobileEnableStaticAndCSMShadowReceivers)
							? NSLOCTEXT("Renderer", "MobileDynamicCSM", "PROJECT HAS MOBILE CSM SHADOWS FROM STATIONARY DIRECTIONAL LIGHTS DISABLED")
							: NSLOCTEXT("Renderer", "MobileDynamicCSMDistFieldShadows", "MOBILE CSM+STATIC REQUIRES DISTANCE FIELD SHADOWS ENABLED FOR PROJECT");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;
					}
					if (bMobileShowVertexFogWarning)
					{
						static const FText Message = NSLOCTEXT("Renderer", "MobileVertexFog", "PROJECT HAS VERTEX FOG ON MOBILE DISABLED");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;
					}
					
					if (bShowSkinCacheOOM)
					{
						FString String = FString::Printf(TEXT("OUT OF MEMORY FOR SKIN CACHE, REQUIRES %.3f extra MB (currently at %.3f)"), (float)GPUSkinCacheExtraRequiredMemory / 1048576.0f, CVarSkinCacheOOM->GetValueOnAnyThread());
						Canvas.DrawShadowedText(10, Y, FText::FromString(String), GetStatsFont(), FLinearColor(1.0, 0.05, 0.05, 1.0));
						Y += 14;
					}

					if (bLocked)
					{
						static const FText Message = NSLOCTEXT("Renderer", "ViewLocked", "VIEW LOCKED");
						Canvas.DrawShadowedText(10, Y, Message, GetStatsFont(), FLinearColor(0.8, 1.0, 0.2, 1.0));
						Y += 14;
					}
					Canvas.Flush_RenderThread(RHICmdList);
				}
			}
		}
	}
	
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	// Save the post-occlusion visibility stats for the frame and freezing info
	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		INC_DWORD_STAT_BY(STAT_VisibleStaticMeshElements,View.NumVisibleStaticMeshElements);
		INC_DWORD_STAT_BY(STAT_VisibleDynamicPrimitives,View.VisibleDynamicPrimitives.Num());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// update freezing info
		FSceneViewState* ViewState = (FSceneViewState*)View.State;
		if (ViewState)
		{
			// if we're finished freezing, now we are frozen
			if (ViewState->bIsFreezing)
			{
				ViewState->bIsFreezing = false;
				ViewState->bIsFrozen = true;
				ViewState->bIsFrozenViewMatricesCached = true;
				ViewState->CachedViewMatrices = View.ViewMatrices;
			}

			// handle freeze toggle request
			if (bHasRequestedToggleFreeze)
			{
				// do we want to start freezing or stop?
				ViewState->bIsFreezing = !ViewState->bIsFrozen;
				ViewState->bIsFrozen = false;
				ViewState->bIsFrozenViewMatricesCached = false;
				ViewState->FrozenPrimitives.Empty();
			}
		}
#endif
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// clear the commands
	bHasRequestedToggleFreeze = false;

	if(ViewFamily.EngineShowFlags.OnScreenDebug)
	{
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			if(!View.IsPerspectiveProjection())
			{
				continue;
			}

			GRenderTargetPool.PresentContent(RHICmdList, View);
		}
	}
#endif

	for(int32 ViewExt = 0; ViewExt < ViewFamily.ViewExtensions.Num(); ++ViewExt)
	{
		ViewFamily.ViewExtensions[ViewExt]->PostRenderViewFamily_RenderThread(RHICmdList, ViewFamily);
		for(int32 ViewIndex = 0; ViewIndex < ViewFamily.Views.Num(); ++ViewIndex)
		{
			ViewFamily.ViewExtensions[ViewExt]->PostRenderView_RenderThread(RHICmdList, Views[ViewIndex]);
		}
	}

	// Notify the RHI we are done rendering a scene.
	RHICmdList.EndScene();
}

FSceneRenderer* FSceneRenderer::CreateSceneRenderer(const FSceneViewFamily* InViewFamily, FHitProxyConsumer* HitProxyConsumer)
{
	EShadingPath ShadingPath = InViewFamily->Scene->GetShadingPath();

	if (ShadingPath == EShadingPath::Deferred)
	{
		return new FDeferredShadingSceneRenderer(InViewFamily, HitProxyConsumer);
	}
	else 
	{
		check(ShadingPath == EShadingPath::Mobile);
		return new FMobileSceneRenderer(InViewFamily, HitProxyConsumer);
	}
}

void ServiceLocalQueue();

void FSceneRenderer::RenderCustomDepthPassAtLocation(FRHICommandListImmediate& RHICmdList, int32 Location)
{		
	extern TAutoConsoleVariable<int32> CVarCustomDepthOrder;
	int32 CustomDepthOrder = FMath::Clamp(CVarCustomDepthOrder.GetValueOnRenderThread(), 0, 1);

	if(CustomDepthOrder == Location)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_CustomDepthPass);
		RenderCustomDepthPass(RHICmdList);
		ServiceLocalQueue();
	}
}

void FSceneRenderer::RenderCustomDepthPass(FRHICommandListImmediate& RHICmdList)
{
	// do we have primitives in this pass?
	bool bPrimitives = false;

	if(!Scene->World || (Scene->World->WorldType != EWorldType::EditorPreview && Scene->World->WorldType != EWorldType::Inactive))
	{
		for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			const FViewInfo& View = Views[ViewIndex];
			if(View.CustomDepthSet.NumPrims())
			{
				bPrimitives = true;
				break;
			}
		}
	}

	// Render CustomDepth
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	if (SceneContext.BeginRenderingCustomDepth(RHICmdList, bPrimitives))
	{
		SCOPED_DRAW_EVENT(RHICmdList, CustomDepth);
		SCOPED_GPU_STAT(RHICmdList, Stat_GPU_CustomDepth);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];

			if (View.ShouldRenderView())
			{

				FDrawingPolicyRenderState DrawRenderState(View);

				if (!View.IsInstancedStereoPass())
				{
					RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
				}
				else
				{
					if (View.bIsMultiViewEnabled)
					{
						const uint32 LeftMinX = View.Family->Views[0]->ViewRect.Min.X;
						const uint32 LeftMaxX = View.Family->Views[0]->ViewRect.Max.X;
						const uint32 RightMinX = View.Family->Views[1]->ViewRect.Min.X;
						const uint32 RightMaxX = View.Family->Views[1]->ViewRect.Max.X;
						const uint32 LeftMaxY = View.Family->Views[0]->ViewRect.Max.Y;
						const uint32 RightMaxY = View.Family->Views[1]->ViewRect.Max.Y;
						RHICmdList.SetStereoViewport(LeftMinX, RightMinX, 0, 0, 0.0f, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, 1.0f);
					}
					else
					{
						RHICmdList.SetViewport(0, 0, 0, View.Family->InstancedStereoWidth, View.ViewRect.Max.Y, 1);
					}
				}

				DrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());

				const bool bWriteCustomStencilValues = SceneContext.IsCustomDepthPassWritingStencil();

				if (!bWriteCustomStencilValues)
				{
					DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
				}

				if ((CVarCustomDepthTemporalAAJitter.GetValueOnRenderThread() == 0) && (View.AntiAliasingMethod == AAM_TemporalAA))
				{
					FBox VolumeBounds[TVC_MAX];

					FViewMatrices ModifiedViewMatricies = View.ViewMatrices;
					ModifiedViewMatricies.HackRemoveTemporalAAProjectionJitter();
					FViewUniformShaderParameters OverriddenViewUniformShaderParameters;
					View.SetupUniformBufferParameters(
						SceneContext,
						ModifiedViewMatricies,
						ModifiedViewMatricies,
						VolumeBounds,
						TVC_MAX,
						OverriddenViewUniformShaderParameters);
					DrawRenderState.SetViewUniformBuffer(TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(OverriddenViewUniformShaderParameters, UniformBuffer_SingleFrame));
					View.CustomDepthSet.DrawPrims(RHICmdList, View, DrawRenderState, bWriteCustomStencilValues);

					// @third party code - BEGIN HairWorks
					HairWorksRenderer::RenderCustomStencil(RHICmdList, View);
					// @third party code - END HairWorks
				}
				else
				{
					View.CustomDepthSet.DrawPrims(RHICmdList, View, DrawRenderState, bWriteCustomStencilValues);

					// @third party code - BEGIN HairWorks
					HairWorksRenderer::RenderCustomStencil(RHICmdList, View);
					// @third party code - END HairWorks
				}
			}
		}

		// resolve using the current ResolveParams 
		SceneContext.FinishRenderingCustomDepth(RHICmdList);
	}
}

void FSceneRenderer::OnStartFrame(FRHICommandListImmediate& RHICmdList)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	GRenderTargetPool.VisualizeTexture.OnStartFrame(Views[0]);
	CompositionGraph_OnStartFrame();
	SceneContext.bScreenSpaceAOIsValid = false;
	SceneContext.bCustomDepthIsValid = false;

	for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{	
		FSceneView& View = Views[ViewIndex];
		FSceneViewStateInterface* State = View.State;

		if(State)
		{
			State->OnStartFrame(View, ViewFamily);
		}
	}
}

bool FSceneRenderer::ShouldCompositeEditorPrimitives(const FViewInfo& View)
{
	// If the show flag is enabled
	if (!View.Family->EngineShowFlags.CompositeEditorPrimitives)
	{
		return false;
	}

	if (View.Family->EngineShowFlags.VisualizeHDR || View.Family->UseDebugViewPS())
	{
		// certain visualize modes get obstructed too much
		return false;
	}

	if (GIsEditor && View.Family->EngineShowFlags.Wireframe)
	{
		// In Editor we want wire frame view modes to be in MSAA
		return true;
	}

	// Any elements that needed compositing were drawn then compositing should be done
	if (View.ViewMeshElements.Num() || View.TopViewMeshElements.Num() || View.BatchedViewElements.HasPrimsToDraw() || View.TopBatchedViewElements.HasPrimsToDraw() || View.VisibleEditorPrimitives.Num())
	{
		return true;
	}

	return false;
}

void FSceneRenderer::WaitForTasksClearSnapshotsAndDeleteSceneRenderer(FRHICommandListImmediate& RHICmdList, FSceneRenderer* SceneRenderer)
{
	// we are about to destroy things that are being used for async tasks, so we wait here for them.
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_DeleteSceneRenderer_WaitForTasks);
		RHICmdList.ImmediateFlush(EImmediateFlushType::WaitForOutstandingTasksOnly);
	}
	FViewInfo::DestroyAllSnapshots(); // this destroys viewinfo snapshots
	FSceneRenderTargets::Get(RHICmdList).DestroyAllSnapshots(); // this will destroy the render target snapshots
	static const IConsoleVariable* AsyncDispatch	= IConsoleManager::Get().FindConsoleVariable(TEXT("r.RHICmdAsyncRHIThreadDispatch"));

	if (AsyncDispatch->GetInt() == 0)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_DeleteSceneRenderer_Dispatch);
		RHICmdList.ImmediateFlush(EImmediateFlushType::WaitForDispatchToRHIThread); // we want to make sure this all gets to the rhi thread this frame and doesn't hang around
	}
	// Delete the scene renderer.
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_DeleteSceneRenderer);
		delete SceneRenderer;
	}
}


void FSceneRenderer::UpdatePrimitivePrecomputedLightingBuffers()
{
	// Use a bit array to prevent primitives from being updated more than once.
	FSceneBitArray UpdatedPrimitiveMap;
	UpdatedPrimitiveMap.Init(false, Scene->Primitives.Num());

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{		
		FViewInfo& View = Views[ViewIndex];

		for (int32 Index = 0; Index < View.DirtyPrecomputedLightingBufferPrimitives.Num(); ++Index)
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = View.DirtyPrecomputedLightingBufferPrimitives[Index];

			FBitReference bInserted = UpdatedPrimitiveMap[PrimitiveSceneInfo->GetIndex()];
			if (!bInserted)
			{
				PrimitiveSceneInfo->UpdatePrecomputedLightingBuffer();
				bInserted = true;
			}
			else
			{
				// This will prevent clearing it twice.
				View.DirtyPrecomputedLightingBufferPrimitives[Index] = nullptr;
			}
		}
	}

	const uint32 CurrentSceneFrameNumber = Scene->GetFrameNumber();

	// Trim old CPUInterpolationCache entries occasionally
	if (CurrentSceneFrameNumber % 10 == 0)
	{
		for (TMap<FVector, FVolumetricLightmapInterpolation>::TIterator It(Scene->VolumetricLightmapSceneData.CPUInterpolationCache); It; ++It)
		{
			FVolumetricLightmapInterpolation& Interpolation = It.Value();

			if (Interpolation.LastUsedSceneFrameNumber < CurrentSceneFrameNumber - 100)
			{
				It.RemoveCurrent();
			}
		}
	}
}

void FSceneRenderer::ClearPrimitiveSingleFramePrecomputedLightingBuffers()
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{		
		FViewInfo& View = Views[ViewIndex];

		for (int32 Index = 0; Index < View.DirtyPrecomputedLightingBufferPrimitives.Num(); ++Index)
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = View.DirtyPrecomputedLightingBufferPrimitives[Index];
	
			if (PrimitiveSceneInfo) // Could be null if it was a duplicate.
			{
				PrimitiveSceneInfo->ClearPrecomputedLightingBuffer(true);
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	FRendererModule
-----------------------------------------------------------------------------*/

/**
* Helper function performing actual work in render thread.
*
* @param SceneRenderer	Scene renderer to use for rendering.
*/
static void ViewExtensionPreRender_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneRenderer* SceneRenderer)
{
	FMemMark MemStackMark(FMemStack::Get());

	for (int ViewExt = 0; ViewExt < SceneRenderer->ViewFamily.ViewExtensions.Num(); ViewExt++)
	{
		SceneRenderer->ViewFamily.ViewExtensions[ViewExt]->PreRenderViewFamily_RenderThread(RHICmdList, SceneRenderer->ViewFamily);
		for (int ViewIndex = 0; ViewIndex < SceneRenderer->ViewFamily.Views.Num(); ViewIndex++)
		{
			SceneRenderer->ViewFamily.ViewExtensions[ViewExt]->PreRenderView_RenderThread(RHICmdList, SceneRenderer->Views[ViewIndex]);
		}
	}
    
    // update any resources that needed a deferred update
    FDeferredUpdateResource::UpdateResources(RHICmdList);
}

/**
 * Helper function performing actual work in render thread.
 *
 * @param SceneRenderer	Scene renderer to use for rendering.
 */
static void RenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneRenderer* SceneRenderer)
{
	FMemMark MemStackMark(FMemStack::Get());

	// update any resources that needed a deferred update
	FDeferredUpdateResource::UpdateResources(RHICmdList);

	if(SceneRenderer->ViewFamily.EngineShowFlags.OnScreenDebug)
	{
		GRenderTargetPool.SetEventRecordingActive(true);
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_TotalSceneRenderingTime);
	
		if(SceneRenderer->ViewFamily.EngineShowFlags.HitProxies)
		{
			// Render the scene's hit proxies.
			SceneRenderer->RenderHitProxies(RHICmdList);
		}
		else
		{
			// Render the scene.
			SceneRenderer->Render(RHICmdList);
		}

		// Only reset per-frame scene state once all views have processed their frame, including those in planar reflections
		for (int32 CacheType = 0; CacheType < ARRAY_COUNT(SceneRenderer->Scene->DistanceFieldSceneData.PrimitiveModifiedBounds); CacheType++)
		{
			SceneRenderer->Scene->DistanceFieldSceneData.PrimitiveModifiedBounds[CacheType].Reset();
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_UpdateMotionBlurCache);
			SceneRenderer->Scene->MotionBlurInfoData.UpdateMotionBlurCache(SceneRenderer->Scene);
		}

#if STATS
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderViewFamily_RenderThread_MemStats);

			// Update scene memory stats that couldn't be tracked continuously
			SET_MEMORY_STAT(STAT_StaticDrawListMemory, FStaticMeshDrawListBase::TotalBytesUsed);
			SET_MEMORY_STAT(STAT_RenderingSceneMemory, SceneRenderer->Scene->GetSizeBytes());

			SIZE_T ViewStateMemory = 0;
			for (int32 ViewIndex = 0; ViewIndex < SceneRenderer->Views.Num(); ViewIndex++)
			{
				if (SceneRenderer->Views[ViewIndex].State)
				{
					ViewStateMemory += SceneRenderer->Views[ViewIndex].State->GetSizeBytes();
				}
			}
			SET_MEMORY_STAT(STAT_ViewStateMemory, ViewStateMemory);
			SET_MEMORY_STAT(STAT_RenderingMemStackMemory, FMemStack::Get().GetByteCount());
			SET_MEMORY_STAT(STAT_LightInteractionMemory, FLightPrimitiveInteraction::GetMemoryPoolSize());
		}
#endif
		

		GRenderTargetPool.SetEventRecordingActive(false);

		FSceneRenderer::WaitForTasksClearSnapshotsAndDeleteSceneRenderer(RHICmdList, SceneRenderer);
	}

#if STATS
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderViewFamily_RenderThread_RHIGetGPUFrameCycles);
	if (FPlatformProperties::SupportsWindowedMode() == false)
	{
		/** Update STATS with the total GPU time taken to render the last frame. */
		SET_CYCLE_COUNTER(STAT_TotalGPUFrameTime, RHIGetGPUFrameCycles());
	}
#endif
}

void OnChangeSimpleForwardShading(IConsoleVariable* Var)
{
	static const auto SupportSimpleForwardShadingCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportSimpleForwardShading"));
	static const auto SimpleForwardShadingCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SimpleForwardShading"));

	if (SimpleForwardShadingCVar->GetValueOnAnyThread() != 0)
	{
		if (SupportSimpleForwardShadingCVar->GetValueOnAnyThread() == 0)
		{
			UE_LOG(LogRenderer, Warning, TEXT("r.SimpleForwardShading ignored as r.SupportSimpleForwardShading is not enabled"));
		}
		else if (!PlatformSupportsSimpleForwardShading(GMaxRHIShaderPlatform))
		{
			UE_LOG(LogRenderer, Warning, TEXT("r.SimpleForwardShading ignored, only supported on PC shader platforms.  Current shader platform %s"), *LegacyShaderPlatformToShaderFormat(GMaxRHIShaderPlatform).ToString());
		}
	}

	// Propgate cvar change to static draw lists
	FGlobalComponentRecreateRenderStateContext Context;
}

void OnChangeCVarRequiringRecreateRenderState(IConsoleVariable* Var)
{
	// Propgate cvar change to static draw lists
	FGlobalComponentRecreateRenderStateContext Context;
}

FRendererModule::FRendererModule()
	: CustomCullingImpl(nullptr)
{
	CVarSimpleForwardShading.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnChangeSimpleForwardShading));

	static auto CVarEarlyZPass = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EarlyZPass"));
	CVarEarlyZPass->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnChangeCVarRequiringRecreateRenderState));

	static auto CVarEarlyZPassMovable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EarlyZPassMovable"));
	CVarEarlyZPassMovable->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnChangeCVarRequiringRecreateRenderState));
}

void FRendererModule::CreateAndInitSingleView(FRHICommandListImmediate& RHICmdList, class FSceneViewFamily* ViewFamily, const struct FSceneViewInitOptions* ViewInitOptions)
{
	// Create and add the new view
	FViewInfo* NewView = new FViewInfo(*ViewInitOptions);
	ViewFamily->Views.Add(NewView);
	SetRenderTarget(RHICmdList, ViewFamily->RenderTarget->GetRenderTargetTexture(), nullptr, ESimpleRenderTargetMode::EClearColorExistingDepth);
	FViewInfo* View = (FViewInfo*)ViewFamily->Views[0];
	View->InitRHIResources();
}

void FRendererModule::BeginRenderingViewFamily(FCanvas* Canvas, FSceneViewFamily* ViewFamily)
{
	UWorld* World = nullptr; 
	check(ViewFamily->Scene);

	FScene* const Scene = ViewFamily->Scene->GetRenderScene();
	if (Scene)
	{
		World = Scene->GetWorld();
		if (World)
		{
			//guarantee that all render proxies are up to date before kicking off a BeginRenderViewFamily.
			World->SendAllEndOfFrameUpdates();
		}
	}

	ENQUEUE_RENDER_COMMAND(UpdateDeferredCachedUniformExpressions)(
		[](FRHICommandList& RHICmdList)
		{
			FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();
		});

	ENQUEUE_RENDER_COMMAND(UpdateFastVRamConfig)(
		[](FRHICommandList& RHICmdList)
	{
		GFastVRamConfig.Update();
	});

	// Flush the canvas first.
	Canvas->Flush_GameThread();

	if (Scene)
	{
		// We allow caching of per-frame, per-scene data
		Scene->IncrementFrameNumber();
		ViewFamily->FrameNumber = Scene->GetFrameNumber();
	}
	else
	{
	// this is passes to the render thread, better access that than GFrameNumberRenderThread
	ViewFamily->FrameNumber = GFrameNumber;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	{
		extern TSharedRef<ISceneViewExtension, ESPMode::ThreadSafe> GetRendererViewExtension();

		ViewFamily->ViewExtensions.Add(GetRendererViewExtension());
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)


	for (int ViewExt = 0; ViewExt < ViewFamily->ViewExtensions.Num(); ViewExt++)
	{
		ViewFamily->ViewExtensions[ViewExt]->BeginRenderViewFamily(*ViewFamily);
	}
	
	if (Scene)
	{		
		// Set the world's "needs full lighting rebuild" flag if the scene has any uncached static lighting interactions.
		if(World)
		{
			// Note: reading NumUncachedStaticLightingInteractions on the game thread here which is written to by the rendering thread
			// This is reliable because the RT uses interlocked mechanisms to update it
			World->SetMapNeedsLightingFullyRebuilt(Scene->NumUncachedStaticLightingInteractions);
		}
	
		// Construct the scene renderer.  This copies the view family attributes into its own structures.
		FSceneRenderer* SceneRenderer = FSceneRenderer::CreateSceneRenderer(ViewFamily, Canvas->GetHitProxyConsumer());

		Scene->EnsureMotionBlurCacheIsUpToDate(ViewFamily->bWorldIsPaused);

		if (!SceneRenderer->ViewFamily.EngineShowFlags.HitProxies)
		{
			USceneCaptureComponent::UpdateDeferredCaptures(Scene);
		}

		// We need to execute the pre-render view extensions before we do any view dependent work.
		// Anything between here and FDrawSceneCommand will add to HMD view latency
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FViewExtensionPreDrawCommand,
			FSceneRenderer*, SceneRenderer, SceneRenderer,
			{
				ViewExtensionPreRender_RenderThread(RHICmdList, SceneRenderer);
			});

		if (!SceneRenderer->ViewFamily.EngineShowFlags.HitProxies)
		{
			for (int32 ReflectionIndex = 0; ReflectionIndex < SceneRenderer->Scene->PlanarReflections_GameThread.Num(); ReflectionIndex++)
			{
				UPlanarReflectionComponent* ReflectionComponent = SceneRenderer->Scene->PlanarReflections_GameThread[ReflectionIndex];
				SceneRenderer->Scene->UpdatePlanarReflectionContents(ReflectionComponent, *SceneRenderer);
			}
		}

		SceneRenderer->ViewFamily.DisplayInternalsData.Setup(World);

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FDrawSceneCommand,
			FSceneRenderer*,SceneRenderer,SceneRenderer,
		{
			RenderViewFamily_RenderThread(RHICmdList, SceneRenderer);
			FlushPendingDeleteRHIResources_RenderThread();
		});

		Scene->ResetMotionBlurCacheTracking();
	}
}

void FRendererModule::PostRenderAllViewports()
{
	// Increment FrameNumber before render the scene. Wrapping around is no problem.
	// This is the only spot we change GFrameNumber, other places can only read.
	++GFrameNumber;
}

void FRendererModule::UpdateMapNeedsLightingFullyRebuiltState(UWorld* World)
{
	World->SetMapNeedsLightingFullyRebuilt(World->Scene->GetRenderScene()->NumUncachedStaticLightingInteractions);
}

void FRendererModule::DrawRectangle(
		FRHICommandList& RHICmdList,
		float X,
		float Y,
		float SizeX,
		float SizeY,
		float U,
		float V,
		float SizeU,
		float SizeV,
		FIntPoint TargetSize,
		FIntPoint TextureSize,
		class FShader* VertexShader,
		EDrawRectangleFlags Flags
		)
{
	::DrawRectangle( RHICmdList, X, Y, SizeX, SizeY, U, V, SizeU, SizeV, TargetSize, TextureSize, VertexShader, Flags );
}


TGlobalResource<FFilterVertexDeclaration>& FRendererModule::GetFilterVertexDeclaration()
{
	return GFilterVertexDeclaration;
}

void FRendererModule::RegisterPostOpaqueComputeDispatcher(FComputeDispatcher *Dispatcher)
{
	PostOpaqueDispatchers.AddUnique(Dispatcher);
}

void FRendererModule::UnRegisterPostOpaqueComputeDispatcher(FComputeDispatcher *Dispatcher)
{
	PostOpaqueDispatchers.Remove(Dispatcher);
}

void FRendererModule::DispatchPostOpaqueCompute(FRHICommandList &RHICmdList)
{
	for (FComputeDispatcher *Dispatcher : PostOpaqueDispatchers)
	{
		Dispatcher->Execute(RHICmdList);
	}
}

void FRendererModule::RegisterPostOpaqueRenderDelegate(const FPostOpaqueRenderDelegate& InPostOpaqueRenderDelegate)
{
	this->PostOpaqueRenderDelegate = InPostOpaqueRenderDelegate;
}

void FRendererModule::RegisterOverlayRenderDelegate(const FPostOpaqueRenderDelegate& InOverlayRenderDelegate)
{
	this->OverlayRenderDelegate = InOverlayRenderDelegate;
}

void FRendererModule::RenderPostOpaqueExtensions(const FSceneView& View, FRHICommandListImmediate& RHICmdList, FSceneRenderTargets& SceneContext)
{
	check(IsInRenderingThread());
	FPostOpaqueRenderParameters RenderParameters;
	RenderParameters.ViewMatrix = View.ViewMatrices.GetViewMatrix();
	RenderParameters.ProjMatrix = View.ViewMatrices.GetProjectionMatrix();
	RenderParameters.DepthTexture = SceneContext.GetSceneDepthSurface()->GetTexture2D();
	RenderParameters.SmallDepthTexture = SceneContext.GetSmallDepthSurface()->GetTexture2D();

	RenderParameters.ViewportRect = View.ViewRect;
	RenderParameters.RHICmdList = &RHICmdList;

	RenderParameters.Uid = (void*)(&View);
	PostOpaqueRenderDelegate.ExecuteIfBound(RenderParameters);
}

void FRendererModule::RenderOverlayExtensions(const FSceneView& View, FRHICommandListImmediate& RHICmdList, FSceneRenderTargets& SceneContext)
{
	check(IsInRenderingThread());
	FPostOpaqueRenderParameters RenderParameters;
	RenderParameters.ViewMatrix = View.ViewMatrices.GetViewMatrix();
	RenderParameters.ProjMatrix = View.ViewMatrices.GetProjectionMatrix();
	RenderParameters.DepthTexture = SceneContext.GetSceneDepthSurface()->GetTexture2D();
	RenderParameters.SmallDepthTexture = SceneContext.GetSmallDepthSurface()->GetTexture2D();

	RenderParameters.ViewportRect = View.ViewRect;
	RenderParameters.RHICmdList = &RHICmdList;

	RenderParameters.Uid=(void*)(&View);
	OverlayRenderDelegate.ExecuteIfBound(RenderParameters);
}

void FRendererModule::RenderPostResolvedSceneColorExtension(FRHICommandListImmediate& RHICmdList, class FSceneRenderTargets& SceneContext)
{
	PostResolvedSceneColorCallbacks.Broadcast(RHICmdList, SceneContext);
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

class FConsoleVariableAutoCompleteVisitor 
{
public:
	// @param Name must not be 0
	// @param CVar must not be 0
	static void OnConsoleVariable(const TCHAR *Name, IConsoleObject* CObj, uint32& Crc)
	{
		IConsoleVariable* CVar = CObj->AsVariable();
		if(CVar)
		{
			if(CObj->TestFlags(ECVF_Scalability) || CObj->TestFlags(ECVF_ScalabilityGroup))
			{
				// float should work on int32 as well
				float Value = CVar->GetFloat();
				Crc = FCrc::MemCrc32(&Value, sizeof(Value), Crc);
			}
		}
	}
};
static uint32 ComputeScalabilityCVarHash()
{
	uint32 Ret = 0;

	IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(FConsoleObjectVisitor::CreateStatic< uint32& >(&FConsoleVariableAutoCompleteVisitor::OnConsoleVariable, Ret));

	return Ret;
}

static void DisplayInternals(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	auto Family = InView.Family;
	// if r.DisplayInternals != 0
	if(Family->EngineShowFlags.OnScreenDebug && Family->DisplayInternalsData.IsValid())
	{
		// could be 0
		auto State = InView.State;

		FCanvas Canvas((FRenderTarget*)Family->RenderTarget, NULL, Family->CurrentRealTime, Family->CurrentWorldTime, Family->DeltaWorldTime, InView.GetFeatureLevel());
		Canvas.SetRenderTargetRect(FIntRect(0, 0, Family->RenderTarget->GetSizeXY().X, Family->RenderTarget->GetSizeXY().Y));

		SetRenderTarget(RHICmdList, Family->RenderTarget->GetRenderTargetTexture(), FTextureRHIRef());

		// further down to not intersect with "LIGHTING NEEDS TO BE REBUILT"
		FVector2D Pos(30, 140);
		const int32 FontSizeY = 14;

		// dark background
		const uint32 BackgroundHeight = 30;
		Canvas.DrawTile(Pos.X - 4, Pos.Y - 4, 500 + 8, FontSizeY * BackgroundHeight + 8, 0, 0, 1, 1, FLinearColor(0,0,0,0.6f), 0, true);

		UFont* Font = GEngine->GetSmallFont();
		FCanvasTextItem SmallTextItem( Pos, FText::GetEmpty(), GEngine->GetSmallFont(), FLinearColor::White );

		SmallTextItem.SetColor(FLinearColor::White);
		SmallTextItem.Text = FText::FromString(FString::Printf(TEXT("r.DisplayInternals = %d"), Family->DisplayInternalsData.DisplayInternalsCVarValue));
		Canvas.DrawItem(SmallTextItem, Pos);
		SmallTextItem.SetColor(FLinearColor::Gray);
		Pos.Y += 2 * FontSizeY;

#define CANVAS_HEADER(txt) \
		{ \
			SmallTextItem.SetColor(FLinearColor::Gray); \
			SmallTextItem.Text = FText::FromString(txt); \
			Canvas.DrawItem(SmallTextItem, Pos); \
			Pos.Y += FontSizeY; \
		}
#define CANVAS_LINE(bHighlight, txt, ... ) \
		{ \
			SmallTextItem.SetColor(bHighlight ? FLinearColor::Red : FLinearColor::Gray); \
			SmallTextItem.Text = FText::FromString(FString::Printf(txt, __VA_ARGS__)); \
			Canvas.DrawItem(SmallTextItem, Pos); \
			Pos.Y += FontSizeY; \
		}

		CANVAS_HEADER(TEXT("command line options:"))
		{
			bool bHighlight = !(FApp::UseFixedTimeStep() && FApp::bUseFixedSeed);
			CANVAS_LINE(bHighlight, TEXT("  -UseFixedTimeStep: %u"), FApp::UseFixedTimeStep())
			CANVAS_LINE(bHighlight, TEXT("  -FixedSeed: %u"), FApp::bUseFixedSeed)
			CANVAS_LINE(false, TEXT("  -gABC= (changelist): %d"), GetChangeListNumberForPerfTesting())
		}

		CANVAS_HEADER(TEXT("Global:"))
		CANVAS_LINE(false, TEXT("  FrameNumberRT: %u"), GFrameNumberRenderThread)
		CANVAS_LINE(false, TEXT("  Scalability CVar Hash: %x (use console command \"Scalability\")"), ComputeScalabilityCVarHash())
		//not really useful as it is non deterministic and should not be used for rendering features:  CANVAS_LINE(false, TEXT("  FrameNumberRT: %u"), GFrameNumberRenderThread)
		CANVAS_LINE(false, TEXT("  FrameCounter: %llu"), (uint64)GFrameCounter)
		CANVAS_LINE(false, TEXT("  rand()/SRand: %x/%x"), FMath::Rand(), FMath::GetRandSeed())
		{
			bool bHighlight = Family->DisplayInternalsData.NumPendingStreamingRequests != 0;
			CANVAS_LINE(bHighlight, TEXT("  FStreamAllResourcesLatentCommand: %d"), bHighlight)
		}
		{
			static auto* Var = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Streaming.FramesForFullUpdate"));
			int32 Value = Var->GetValueOnRenderThread();
			bool bHighlight = Value != 0;
			CANVAS_LINE(bHighlight, TEXT("  r.Streaming.FramesForFullUpdate: %u%s"), Value, bHighlight ? TEXT(" (should be 0)") : TEXT(""));
		}

		if(State)
		{
			CANVAS_HEADER(TEXT("State:"))
			CANVAS_LINE(false, TEXT("  TemporalAASample: %u"), State->GetCurrentTemporalAASampleIndex())
			CANVAS_LINE(false, TEXT("  FrameIndexMod8: %u"), State->GetFrameIndexMod8())
			CANVAS_LINE(false, TEXT("  LODTransition: %.2f"), State->GetTemporalLODTransition())
		}

		CANVAS_HEADER(TEXT("Family:"))
		CANVAS_LINE(false, TEXT("  Time (Real/World/DeltaWorld): %.2f/%.2f/%.2f"), Family->CurrentRealTime, Family->CurrentWorldTime, Family->DeltaWorldTime)
		CANVAS_LINE(false, TEXT("  MatineeTime: %f"), Family->DisplayInternalsData.MatineeTime)
		CANVAS_LINE(false, TEXT("  FrameNumber: %u"), Family->FrameNumber)
		CANVAS_LINE(false, TEXT("  ExposureSettings: %s"), *Family->ExposureSettings.ToString())
		CANVAS_LINE(false, TEXT("  GammaCorrection: %.2f"), Family->GammaCorrection)

		CANVAS_HEADER(TEXT("View:"))
		CANVAS_LINE(false, TEXT("  TemporalJitter: %.2f/%.2f"), InView.TemporalJitterPixelsX, InView.TemporalJitterPixelsY)
		CANVAS_LINE(false, TEXT("  ViewProjectionMatrix Hash: %x"), InView.ViewMatrices.GetViewProjectionMatrix().ComputeHash())
		CANVAS_LINE(false, TEXT("  ViewLocation: %s"), *InView.ViewLocation.ToString())
		CANVAS_LINE(false, TEXT("  ViewRotation: %s"), *InView.ViewRotation.ToString())
		CANVAS_LINE(false, TEXT("  ViewRect: %s"), *InView.ViewRect.ToString())

		FViewInfo& ViewInfo = (FViewInfo&)InView;

		CANVAS_LINE(false, TEXT("  DynMeshElements/TranslPrim: %d/%d"), ViewInfo.DynamicMeshElements.Num(), ViewInfo.TranslucentPrimSet.NumPrims())

#undef CANVAS_LINE
#undef CANVAS_HEADER

		Canvas.Flush_RenderThread(RHICmdList);
	}
#endif
}

TSharedRef<ISceneViewExtension, ESPMode::ThreadSafe> GetRendererViewExtension()
{
	class FRendererViewExtension : public ISceneViewExtension
	{
	public:
		virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) {}
		virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) {}
		virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) {}
		virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) {}
		virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) {}
		virtual int32 GetPriority() const { return 0; }
		virtual void PostRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
		{
			DisplayInternals(RHICmdList, InView);
		}
	};
	TSharedRef<FRendererViewExtension, ESPMode::ThreadSafe> ref(new FRendererViewExtension);
	return StaticCastSharedRef<ISceneViewExtension>(ref);
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

/**
* Saves a previously rendered scene color target
*/

class FDummySceneColorResolveBuffer : public FVertexBuffer
{
public:
	virtual void InitRHI() override
	{
		const int32 NumDummyVerts = 3;
		const uint32 Size = sizeof(FVector4) * NumDummyVerts;
		FRHIResourceCreateInfo CreateInfo;
		void* BufferData = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(Size, BUF_Static, CreateInfo, BufferData);
		FMemory::Memset(BufferData, 0, Size);		
		RHIUnlockVertexBuffer(VertexBufferRHI);		
	}
};

TGlobalResource<FDummySceneColorResolveBuffer> GResolveDummyVertexBuffer;
extern int32 GAllowCustomMSAAResolves;

void FSceneRenderer::ResolveSceneColor(FRHICommandList& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, ResolveSceneColor);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	auto& CurrentSceneColor = SceneContext.GetSceneColor();
	uint32 CurrentNumSamples = CurrentSceneColor->GetDesc().NumSamples;

	const EShaderPlatform CurrentShaderPlatform = GShaderPlatformForFeatureLevel[SceneContext.GetCurrentFeatureLevel()];
	if (CurrentNumSamples <= 1 || !RHISupportsSeparateMSAAAndResolveTextures(CurrentShaderPlatform) || !GAllowCustomMSAAResolves)
	{
		RHICmdList.CopyToResolveTarget(SceneContext.GetSceneColorSurface(), SceneContext.GetSceneColorTexture(), true, FResolveRect(0, 0, ViewFamily.FamilySizeX, ViewFamily.FamilySizeY));
	}
	else 
	{
		// Custom shader based color resolve for HDR color to emulate mobile.
		SetRenderTarget(RHICmdList, SceneContext.GetSceneColorTexture(), FTextureRHIParamRef());
		
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			RHICmdList.SetStreamSource(0, GResolveDummyVertexBuffer.VertexBufferRHI, 0);

			// Resolve views individually
			// In the case of adaptive resolution, the view family will be much larger than the views individually
			RHICmdList.SetScissorRect(true, View.ViewRect.Min.X, View.ViewRect.Min.Y, View.ViewRect.Max.X, View.ViewRect.Max.Y);

			int32 ResolveWidth = CVarWideCustomResolve.GetValueOnRenderThread();

			if (CurrentNumSamples <= 1)
			{
				ResolveWidth = 0;
			}

			if (ResolveWidth != 0)
			{
				ResolveFilterWide(RHICmdList, GraphicsPSOInit, SceneContext.GetCurrentFeatureLevel(), CurrentSceneColor->GetRenderTargetItem().TargetableTexture, FIntPoint(0, 0), CurrentNumSamples, ResolveWidth);
			}
			else
			{
				auto ShaderMap = GetGlobalShaderMap(SceneContext.GetCurrentFeatureLevel());
				TShaderMapRef<FHdrCustomResolveVS> VertexShader(ShaderMap);

				if (CurrentNumSamples == 2)
				{
					TShaderMapRef<FHdrCustomResolve2xPS> PixelShader(ShaderMap);
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
					PixelShader->SetParameters(RHICmdList, CurrentSceneColor->GetRenderTargetItem().TargetableTexture);
					RHICmdList.DrawPrimitive(PT_TriangleList, 0, 1, 1);
				}
				else if (CurrentNumSamples == 4)
				{
					TShaderMapRef<FHdrCustomResolve4xPS> PixelShader(ShaderMap);
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
					PixelShader->SetParameters(RHICmdList, CurrentSceneColor->GetRenderTargetItem().TargetableTexture);
					RHICmdList.DrawPrimitive(PT_TriangleList, 0, 1, 1);
				}
				else if (CurrentNumSamples == 8)
				{
					TShaderMapRef<FHdrCustomResolve8xPS> PixelShader(ShaderMap);
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
					PixelShader->SetParameters(RHICmdList, CurrentSceneColor->GetRenderTargetItem().TargetableTexture);
					RHICmdList.DrawPrimitive(PT_TriangleList, 0, 1, 1);
				}
				else
				{
					// Everything other than 2,4,8 samples is not implemented.
					check(0);
				}
			}
		}

		RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	}
}
FTextureRHIParamRef FSceneRenderer::GetMultiViewSceneColor(const FSceneRenderTargets& SceneContext) const
{
	const FViewInfo& View = Views[0];

	if (View.bIsMobileMultiViewEnabled && !View.bIsMobileMultiViewDirectEnabled)
	{
		return SceneContext.MobileMultiViewSceneColor->GetRenderTargetItem().TargetableTexture;
	}
	else
	{
		return static_cast<FTextureRHIRef>(ViewFamily.RenderTarget->GetRenderTargetTexture());
	}
}

