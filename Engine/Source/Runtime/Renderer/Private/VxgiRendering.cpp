// Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.


// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI

#include "VxgiRendering.h"
#include "RendererPrivate.h"
#include "DeferredShadingRenderer.h"
#include "ScreenRendering.h"
#include "SceneFilterRendering.h"
#include "SceneUtils.h"
#include "EngineUtils.h"
#include "Engine/VxgiAnchor.h"
#include "Engine/TextureCube.h"
#include "PipelineStateCache.h"

static TAutoConsoleVariable<int32> CVarVxgiMapSize(
	TEXT("r.VXGI.MapSize"),
	128,
	TEXT("Size of every VXGI clipmap level, in voxels.\n")
	TEXT("Valid values are 16, 32, 64, 128, 256."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiStackLevels(
	TEXT("r.VXGI.StackLevels"),
	5,
	TEXT("Number of clip stack levels in VXGI clipmap (1-5)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiOpacity6D(
	TEXT("r.VXGI.Opacity6D"),
	1,
	TEXT("Whether to use 6 opacity projections per voxel.\n")
	TEXT("0: 3 projections, 1: 6 projections"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiEmittance6D(
	TEXT("r.VXGI.Emittance6D"),
	1,
	TEXT("Whether to use 6 emittance projections per voxel.\n")
	TEXT("0: 3 projections, 1: 6 projections"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiNvidiaExtensionsEnable(
	TEXT("r.VXGI.NvidiaExtensionsEnable"),
	1,
	TEXT("Controls the use of NVIDIA specific D3D extensions by VXGI.\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiGSPassthroughEnable(
    TEXT("r.VXGI.GSPassthroughEnable"),
    1,
    TEXT("Enables the use of Maxwell Geometry Shader Pass-Through feature for voxelization.\n")
    TEXT("Only effective when r.VXGI.NvidiaExtensionsEnable = 1.\n")
    TEXT("Sometimes pass-through shaders do not work properly (like wrong parts of emissive objects emit light)\n")
    TEXT("while other Maxwell features do, so this flag is to work around the issues at a small performance cost.")
    TEXT("0: Disable, 1: Enable"),
    ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiStoreEmittanceInHdrFormat(
	TEXT("r.VXGI.StoreEmittanceInHdrFormat"),
	1,
	TEXT("Sets the format of VXGI emittance voxel textures.\n")
	TEXT("0: UNORM8, 1: FP16 (on Maxwell) or FP32 (on other GPUs)."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarVxgiEmittanceStorageScale(
	TEXT("r.VXGI.EmittanceStorageScale"),
	1.0f,
	TEXT("Multiplier for the values stored in VXGI emittance textures (any value greater than 0).\n")
	TEXT("If you observe emittance clamping (e.g. white voxels on colored objects)\n") 
	TEXT("or quantization (color distortion in dim areas), try to change this parameter."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiEmittanceInterpolationEnable(
	TEXT("r.VXGI.EmittanceInterpolationEnable"),
	0,
	TEXT("Whether to interpolate between downsampled and directly voxelized emittance in coarse levels of detail.\n")
	TEXT("Sometimes this interpolation makes illumination smoother when the camera moves.\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiHighQualityEmittanceDownsamplingEnable(
	TEXT("r.VXGI.HighQualityEmittanceDownsamplingEnable"),
	0,
	TEXT("Whether to use a larger triangular filter for emittance downsampling.\n")
	TEXT("This filter improves stability of indirect lighting caused by moving objects, but has a negative effect on performance.\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiDiffuseTracingEnable(
	TEXT("r.VXGI.DiffuseTracingEnable"),
	1,
	TEXT("Whether to enable VXGI indirect lighting.\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiSpecularTracingEnable(
	TEXT("r.VXGI.SpecularTracingEnable"),
	1,
	TEXT("Whether to enable VXGI reflections.\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiTemporalReprojectionEnable(
	TEXT("r.VXGI.TemporalReprojectionEnable"),
	1,
	TEXT("Whether to enable temporal reprojection.\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiMultiBounceEnable(
	TEXT("r.VXGI.MultiBounceEnable"),
	0,
	TEXT("Whether to enable multi-bounce diffuse VXGI.\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiMultiBounceNormalizationEnable(
	TEXT("r.VXGI.MultiBounceNormalizationEnable"),
	1,
	TEXT("Whether to try preventing the indirect irradiance from blowing up exponentially due to high feedback.\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarVxgiRange(
	TEXT("r.VXGI.Range"),
	400.0f,
	TEXT("Size of the finest clipmap level, in world units."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarVxgiViewOffsetScale(
	TEXT("r.VXGI.ViewOffsetScale"),
	1.f,
	TEXT("Scale factor for the distance between the camera and the VXGI clipmap anchor point"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiDiffuseMaterialsEnable(
	TEXT("r.VXGI.DiffuseMaterialsEnable"),
	1,
	TEXT("Whether to include diffuse lighting in the VXGI voxelized emittance.\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiEmissiveMaterialsEnable(
	TEXT("r.VXGI.EmissiveMaterialsEnable"),
	1,
	TEXT("Whether to include emissive materials in the VXGI voxelized emittance.\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiEmittanceShadingMode(
	TEXT("r.VXGI.EmittanceShadingMode"),
	0,
	TEXT("0: Use DiffuseColor = BaseColor - BaseColor * Metallic")
	TEXT("1: Use DiffuseColor = BaseColor"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiEmittanceShadowEnable(
	TEXT("r.VXGI.EmittanceShadowEnable"),
	1,
	TEXT("[Debug] Whether to enable the emittance shadow term.\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiEmittanceShadowCascade(
	TEXT("r.VXGI.EmittanceShadowCascade"),
	-1,
	TEXT("[Debug] Restrict the emittance shadowing to a single cascade.\n")
	TEXT("<0: Use all cascades. Otherwise the index of the cascade to use."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiEmittanceShadowQuality(
	TEXT("r.VXGI.EmittanceShadowQuality"),
	1,
	TEXT("0: no filtering\n")
	TEXT("1: 2x2 samples"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiDebugClipmapLevel(
	TEXT("r.VXGI.DebugClipmapLevel"),
	15,
	TEXT("Current clipmap level visualized (for the opacity and emittance debug modes).\n")
	TEXT("15: visualize all levels at once"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiDebugVoxelsToSkip(
	TEXT("r.VXGI.DebugVoxelsToSkip"),
	0,
	TEXT("Number of initial voxels to skip in the ray casting if r.VXGI.DebugMode != 0"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiDebugBlendOutput(
	TEXT("r.VXGI.DebugBlendOutput"),
	0,
	TEXT("Alpha blend debug output\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiCompositingMode(
	TEXT("r.VXGI.CompositingMode"),
	0,
	TEXT("0: add the VXGI diffuse result over the UE lighting using additive blending (default)\n")
	TEXT("1: visualize the VXGI indirect lighting only, with no albedo and no AO\n")
	TEXT("2: visualize the direct lighting only"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarVxgiRoughnessOverride(
	TEXT("r.VXGI.RoughnessOverride"),
	0.f,
	TEXT("Override the GBuffer roughness"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiAmbientOcclusionMode(
	TEXT("r.VXGI.AmbientOcclusionMode"),
	0,
	TEXT("0: Default\n")
	TEXT("1: Replace lighting with Voxel AO"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVxgiForceTwoSided(TEXT("r.VXGI.ForceTwoSided"), 0, TEXT(""), ECVF_Default);
static TAutoConsoleVariable<int32> CVarVxgiForceFrontCounterClockwise(TEXT("r.VXGI.ForceFrontCounterClockwise"), 0, TEXT(""), ECVF_Default);
static TAutoConsoleVariable<int32> CVarVxgiForceDisableTonemapper(TEXT("r.VXGI.ForceDisableTonemapper"), 0, TEXT(""), ECVF_Default);

// With reverse infinite projections, the near plane is at Z=1 and the far plane is at Z=0.
// The VXGI library uses these 2 values along with the ViewProjMatrix to compute the ray directions.
#define VXGI_HARDWARE_DEPTH_NEAR 1.f
#define VXGI_HARDWARE_DEPTH_FAR 0.f

void FSceneRenderer::InitVxgiView()
{
	if (!IsVxgiEnabled())
		return;

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = &ViewFamily;
	ViewInitOptions.SetViewRectangle(FIntRect(0, 0, 1, 1));

	VXGI::IGlobalIllumination* VxgiInterface = GDynamicRHI->RHIVXGIGetInterface();
	VXGI::Box3f VxgiBox = VxgiInterface->calculateHypotheticalWorldRegion(VXGI::Vector3f(VxgiAnchorPoint.X, VxgiAnchorPoint.Y, VxgiAnchorPoint.Z), VxgiRange);
	VXGI::Vector3f Center = (VxgiBox.lower + VxgiBox.upper) * 0.5f;

	VxgiClipmapBounds = FBoxSphereBounds(FBox(VxgiBox));

	FMatrix TranslationMatrix = FMatrix::Identity;
	TranslationMatrix = TranslationMatrix.ConcatTranslation(FVector(-Center.x, -Center.y, -Center.z));

	FMatrix ScaleMatrix = FMatrix::Identity;
	ScaleMatrix = ScaleMatrix.ApplyScale(2.0f / (VxgiBox.upper.x - VxgiBox.lower.x));

	ViewInitOptions.ProjectionMatrix = ScaleMatrix;
	ViewInitOptions.ViewOrigin = FVector(0.f);
	ViewInitOptions.ViewRotationMatrix = TranslationMatrix;

	VxgiView = new FViewInfo(ViewInitOptions);

	// Setup the prev matrices for particle system factories
	VxgiView->PrevViewMatrices = VxgiView->ViewMatrices;
	VxgiView->bPrevTransformsReset = true;

	VxgiView->VxgiClipmapBounds = VxgiClipmapBounds;
	VxgiView->AntiAliasingMethod = AAM_None; //Turn off temporal AA jitter
	VxgiView->bIsVxgiVoxelization = true;
	VxgiView->bDisableDistanceBasedFadeTransitions = true;
	VxgiView->VxgiVoxelizationPass = VXGI::VoxelizationPass::OPACITY;
}

void FSceneRenderer::InitVxgiRenderingState(const FSceneViewFamily* InViewFamily)
{
	bVxgiPerformOpacityVoxelization = false;
	bVxgiPerformEmittanceVoxelization = false;

	//This must be done on the game thread
	const auto& PrimaryView = *(InViewFamily->Views[0]);
	bVxgiDebugRendering = ViewFamily.EngineShowFlags.VxgiOpacityVoxels || ViewFamily.EngineShowFlags.VxgiEmittanceVoxels || ViewFamily.EngineShowFlags.VxgiIrradianceVoxels;
	VxgiRange = CVarVxgiRange.GetValueOnGameThread();
	VxgiAnchorPoint = PrimaryView.ViewMatrices.GetViewOrigin() + PrimaryView.GetViewDirection() * VxgiRange * CVarVxgiViewOffsetScale.GetValueOnGameThread();
	
	for (TActorIterator<AVxgiAnchor> Itr(Scene->World); Itr; ++Itr)
	{
		if (Itr->bEnabled)
		{
			VxgiAnchorPoint = Itr->GetActorLocation();
			break;
		}
	}

	bVxgiUseDiffuseMaterials = !!CVarVxgiDiffuseMaterialsEnable.GetValueOnGameThread();
	bVxgiUseEmissiveMaterials = !!CVarVxgiEmissiveMaterialsEnable.GetValueOnGameThread();
	bVxgiTemporalReprojectionEnable = !!CVarVxgiTemporalReprojectionEnable.GetValueOnGameThread();
	bVxgiAmbientOcclusionMode = !!CVarVxgiAmbientOcclusionMode.GetValueOnGameThread();
	bVxgiMultiBounceEnable = !bVxgiAmbientOcclusionMode && !!CVarVxgiMultiBounceEnable.GetValueOnGameThread();

	bVxgiSkyLightEnable = !bVxgiAmbientOcclusionMode
		&& Scene->SkyLight
		&& Scene->SkyLight->ProcessedTexture
		&& !Scene->SkyLight->LightColor.IsAlmostBlack()
		&& ViewFamily.EngineShowFlags.SkyLighting
		&& Scene->SkyLight->bCastVxgiIndirectLighting;
}

bool FSceneRenderer::IsVxgiEnabled(const FViewInfo& View)
{
	if (!View.State && !View.bEnableVxgiForSceneCapture)
	{
		return false; //some editor panel or something
	}

	if (!View.IsPerspectiveProjection())
	{
		return false;
	}

	if (View.bIsSceneCapture && !View.bEnableVxgiForSceneCapture || View.bIsReflectionCapture || View.bIsPlanarReflection)
	{
		return false;
	}

	if (View.Family->ViewMode != VMI_Lit
		&& View.Family->ViewMode != VMI_Lit_DetailLighting
		&& View.Family->ViewMode != VMI_VxgiEmittanceVoxels
		&& View.Family->ViewMode != VMI_VxgiOpacityVoxels
		&& View.Family->ViewMode != VMI_ReflectionOverride
		&& View.Family->ViewMode != VMI_VisualizeBuffer)
	{
		return false;
	}
	
	if (!View.Family->EngineShowFlags.VxgiDiffuse && !View.Family->EngineShowFlags.VxgiSpecular)
	{
		return false;
	}
	
	if (bVxgiDebugRendering)
	{
		return true;
	}

	const auto& PostSettings = View.FinalPostProcessSettings;
	return PostSettings.VxgiDiffuseTracingEnabled || PostSettings.VxgiSpecularTracingEnabled;
}

bool FSceneRenderer::IsVxgiEnabled()
{
	check(Views.Num() > 0);
	const FViewInfo& PrimaryView = Views[0];
	return IsVxgiEnabled(PrimaryView);
}

void FSceneRenderer::SetVxgiVoxelizationParameters(VXGI::VoxelizationParameters& Params)
{
	Params.mapSize = CVarVxgiMapSize.GetValueOnRenderThread();
	Params.stackLevels = CVarVxgiStackLevels.GetValueOnRenderThread();
	Params.allocationMapLodBias = uint32(FMath::Max(2 - int(Params.stackLevels), (Params.mapSize == 256) ? 1 : 0));
	Params.indirectIrradianceMapLodBias = Params.allocationMapLodBias;
	Params.mipLevels = log2(Params.mapSize) - 2;
	Params.persistentVoxelData = false;
	Params.opacityDirectionCount = (CVarVxgiOpacity6D.GetValueOnRenderThread() != 0)
		? VXGI::OpacityDirections::SIX_DIMENSIONAL
		: VXGI::OpacityDirections::THREE_DIMENSIONAL;
	Params.enableNvidiaExtensions = !!CVarVxgiNvidiaExtensionsEnable.GetValueOnRenderThread();
    Params.enableGeometryShaderPassthrough = !!CVarVxgiGSPassthroughEnable.GetValueOnRenderThread();
	Params.emittanceFormat = bVxgiAmbientOcclusionMode
		? VXGI::EmittanceFormat::NONE
		: (CVarVxgiStoreEmittanceInHdrFormat.GetValueOnRenderThread() != 0)
			? VXGI::EmittanceFormat::QUALITY
			: VXGI::EmittanceFormat::UNORM8;
	Params.emittanceStorageScale = CVarVxgiEmittanceStorageScale.GetValueOnRenderThread();
	Params.useEmittanceInterpolation = !!CVarVxgiEmittanceInterpolationEnable.GetValueOnRenderThread();
	Params.useHighQualityEmittanceDownsampling = !!CVarVxgiHighQualityEmittanceDownsamplingEnable.GetValueOnRenderThread();
	Params.enableMultiBounce = bVxgiMultiBounceEnable;
}

void FSceneRenderer::PrepareForVxgiOpacityVoxelization(FRHICommandList& RHICmdList)
{
	VXGI::IGlobalIllumination* VxgiInterface = GDynamicRHI->RHIVXGIGetInterface();
	auto Status = VXGI::VFX_VXGI_VerifyInterfaceVersion();
	check(VXGI_SUCCEEDED(Status));

	SCOPED_DRAW_EVENT(RHICmdList, VXGIPrepareForVxgiOpacityVoxelization);

	VXGI::UpdateVoxelizationParameters Parameters;
	Parameters.clipmapAnchor = VXGI::Vector3f(VxgiAnchorPoint.X, VxgiAnchorPoint.Y, VxgiAnchorPoint.Z);
	Parameters.sceneExtents = GetVxgiWorldSpaceSceneBounds();
	Parameters.giRange = VxgiRange;
	Parameters.indirectIrradianceMapTracingParameters.irradianceScale = Views[0].FinalPostProcessSettings.VxgiMultiBounceIrradianceScale;
	Parameters.indirectIrradianceMapTracingParameters.useAutoNormalization = !!CVarVxgiMultiBounceNormalizationEnable.GetValueOnRenderThread();

	Status = VxgiInterface->prepareForOpacityVoxelization(
		Parameters,
		bVxgiPerformOpacityVoxelization,
		bVxgiPerformEmittanceVoxelization);

	check(VXGI_SUCCEEDED(Status));
}

void FSceneRenderer::PrepareForVxgiEmittanceVoxelization(FRHICommandList& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, PrepareForVxgiEmittanceVoxelization);

	auto Status = GDynamicRHI->RHIVXGIGetInterface()->prepareForEmittanceVoxelization();
	check(VXGI_SUCCEEDED(Status));
}

void FSceneRenderer::VoxelizeVxgiOpacity(FRHICommandList& RHICmdList)
{
	SCOPE_CYCLE_COUNTER(STAT_VxgiVoxelizeOpacity);
	SCOPED_DRAW_EVENT(RHICmdList, VXGIOpacity);

	VXGI::EmittanceVoxelizationArgs Args;
	RenderVxgiVoxelizationPass(RHICmdList, VXGI::VoxelizationPass::OPACITY, Args);
}

struct FCompareFProjectedShadowInfoBySplitNear
{
	inline bool operator()(const FProjectedShadowInfo& A, const FProjectedShadowInfo& B) const
	{
		return A.CascadeSettings.SplitNear < B.CascadeSettings.SplitNear;
	}
};

void FSceneRenderer::VoxelizeVxgiEmittance(FRHICommandList& RHICmdList)
{
	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		if (LightSceneInfo->ShouldRenderLightViewIndependent() &&
			LightSceneInfo->Proxy->CastVxgiIndirectLighting() &&
			LightSceneInfo->Proxy->AffectsBounds(VxgiClipmapBounds))
		{
			FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

			FString LightNameWithLevel;
			GetLightNameForDrawEvent(LightSceneInfo->Proxy, LightNameWithLevel);
			SCOPED_DRAW_EVENTF(RHICmdList, LightPass, *LightNameWithLevel);

			VXGI::EmittanceVoxelizationArgs Args;
			Args.LightSceneInfo = LightSceneInfo;

			for (FProjectedShadowInfo* Shadow : VisibleLightInfo.ShadowsToProject)
			{
				if (Shadow->RenderTargets.DepthTarget)
					Args.Shadows.Add(Shadow);
			}

			Args.Shadows.Sort(FCompareFProjectedShadowInfoBySplitNear());
			RenderVxgiVoxelizationPass(RHICmdList, VXGI::VoxelizationPass::LIGHT0 + LightSceneInfo->Id, Args);
		}
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_VxgiVoxelizeEmissiveAndIndirectIrradiance);
		SCOPED_DRAW_EVENT(RHICmdList, VXGIEmissiveAndIndirectIrradiance);

		VXGI::EmittanceVoxelizationArgs Args;
		RenderVxgiVoxelizationPass(RHICmdList, VXGI::VoxelizationPass::EMISSIVE_AND_IRRADIANCE, Args);
	}
}

bool FSceneRenderer::InitializeVxgiVoxelizationParameters(FRHICommandList& RHICmdList)
{	
	// Fill the voxelization params structure to latch the console vars, specifically AmbientOcclusionMode
	SetVxgiVoxelizationParameters(VxgiVoxelizationParameters);

	// Clamp the parameters first because they might affect the output of IsVxgiEnabled
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		EndVxgiFinalPostProcessSettings(Views[ViewIndex].FinalPostProcessSettings, VxgiVoxelizationParameters);
		if (Views[ViewIndex].State == nullptr)
		{
			//We need the viewstate to implement this
			Views[ViewIndex].FinalPostProcessSettings.bVxgiDiffuseTracingTemporalReprojectionEnabled = false;
		}

		Views[ViewIndex].bVxgiAmbientOcclusionMode = bVxgiAmbientOcclusionMode && Views[ViewIndex].FinalPostProcessSettings.VxgiDiffuseTracingEnabled;
	}

	if (!IsVxgiEnabled())
	{
		return false;
	}

	// Reset the VxgiLastVoxelizationPass values for all primitives
	for (int32 Index = 0; Index < Scene->Primitives.Num(); ++Index)
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->Primitives[Index];
		PrimitiveSceneInfo->VxgiLastVoxelizationPass = VXGI::VoxelizationPass::OPACITY;
	}

	GDynamicRHI->RHIVXGISetVoxelizationParameters(VxgiVoxelizationParameters);

	return true;
}

void FSceneRenderer::RenderVxgiVoxelization(FRHICommandList& RHICmdList)
{
	PrepareForVxgiOpacityVoxelization(RHICmdList);

	if (bVxgiPerformOpacityVoxelization)
	{
		VoxelizeVxgiOpacity(RHICmdList);
	}

	if (bVxgiPerformEmittanceVoxelization)
	{
		PrepareForVxgiEmittanceVoxelization(RHICmdList);
		VoxelizeVxgiEmittance(RHICmdList);
	}

	{
		SCOPED_DRAW_EVENT(RHICmdList, VXGIFinalizeVxgiVoxelization);

		auto Status = GDynamicRHI->RHIVXGIGetInterface()->finalizeVoxelization();
		check(VXGI_SUCCEEDED(Status));

		ViewFamily.bVxgiAvailable = true;
	}
}

void FSceneRenderer::RenderVxgiTracing(FRHICommandListImmediate& RHICmdList)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	SceneContext.VxgiOutputDiffuse.Reset(Views.Num());
	SceneContext.VxgiOutputDiffuse.SetNum(Views.Num());
	SceneContext.VxgiOutputSpec.Reset(Views.Num());
	SceneContext.VxgiOutputSpec.SetNum(Views.Num());

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		Views[ViewIndex].VxgiViewIndex = ViewIndex;
		PrepareVxgiGBuffer(RHICmdList, Views[ViewIndex]);
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		RenderVxgiTracingForView(RHICmdList, Views[ViewIndex]);

		FSceneViewState* ViewState = static_cast<FSceneViewState*>(Views[ViewIndex].State);
		if (ViewState != nullptr)
		{
			if (bVxgiTemporalReprojectionEnable)
			{
				ViewState->PrevSceneDepthZ = SceneContext.SceneDepthZ;
				ViewState->PrevVxgiNormalAndRoughness = SceneContext.VxgiNormalAndRoughness;
			}
			else
			{
				ViewState->PrevSceneDepthZ.SafeRelease();
				ViewState->PrevVxgiNormalAndRoughness.SafeRelease();
			}
		}
	}
}

void FSceneRenderer::EndVxgiFinalPostProcessSettings(FFinalPostProcessSettings& FinalPostProcessSettings, const VXGI::VoxelizationParameters& VParams)
{
	if (!CVarVxgiDiffuseTracingEnable.GetValueOnRenderThread() || !ViewFamily.EngineShowFlags.VxgiDiffuse)
	{
		FinalPostProcessSettings.VxgiDiffuseTracingEnabled = false;
	}
	if (!CVarVxgiSpecularTracingEnable.GetValueOnRenderThread() || !ViewFamily.EngineShowFlags.VxgiSpecular)
	{
		FinalPostProcessSettings.VxgiSpecularTracingEnabled = false;
	}
	if (!bVxgiTemporalReprojectionEnable)
	{
		FinalPostProcessSettings.bVxgiDiffuseTracingTemporalReprojectionEnabled = false;
	}

	switch (CVarVxgiCompositingMode.GetValueOnRenderThread())
	{
	case 1: // Indirect Diffuse Only
		FinalPostProcessSettings.VxgiDiffuseTracingEnabled = true;
		FinalPostProcessSettings.VxgiSpecularTracingEnabled = false;
		FinalPostProcessSettings.ScreenSpaceReflectionIntensity = 0.f;
		break;
	case 2: // Direct Only
		FinalPostProcessSettings.VxgiDiffuseTracingEnabled = false;
		FinalPostProcessSettings.VxgiSpecularTracingEnabled = false;
		FinalPostProcessSettings.ScreenSpaceReflectionIntensity = 0.f;
		break;
	}

	if (VParams.emittanceFormat == VXGI::EmittanceFormat::NONE)
	{
		// Ambient occlusion mode

		FinalPostProcessSettings.VxgiDiffuseTracingIntensity = 0.f;
		FinalPostProcessSettings.VxgiSpecularTracingIntensity = 0.f;
		FinalPostProcessSettings.VxgiSpecularTracingEnabled = false;
		FinalPostProcessSettings.VxgiAmbientColor = FLinearColor(1.0f, 1.0f, 1.0f);
	}
}

void FSceneRenderer::RenderVxgiVoxelizationPass(
	FRHICommandList& RHICmdList,
	int32 VoxelizationPass,
	const VXGI::EmittanceVoxelizationArgs& Args)
{
	if (Args.LightSceneInfo && !Args.LightSceneInfo->Proxy->CastVxgiIndirectLighting())
	{
		return;
	}

	if (VoxelizationPass == VXGI::VoxelizationPass::EMISSIVE_AND_IRRADIANCE)
	{
		if (!bVxgiMultiBounceEnable && !bVxgiSkyLightEnable && !bVxgiUseEmissiveMaterials)
		{
			return;
		}
	}

	SCOPED_DRAW_EVENT(RHICmdList, VXGIVoxelization);

	RHIPushVoxelizationFlag();

	FViewInfo& View = *VxgiView;

	View.VxgiEmittanceVoxelizationArgs = Args;
	View.VxgiEmittanceVoxelizationArgs.bEnableEmissiveMaterials = bVxgiUseEmissiveMaterials;
	View.VxgiEmittanceVoxelizationArgs.bEnableSkyLight = bVxgiSkyLightEnable;
	View.VxgiVoxelizationPass = VoxelizationPass;
	
	VXGI::IGlobalIllumination* VxgiInterface = GDynamicRHI->RHIVXGIGetInterface();
	VxgiInterface->beginVoxelizationDrawCallGroup();

	{
		SCOPE_CYCLE_COUNTER(STAT_VxgiVoxelizationStaticGeometry);
		SCOPED_DRAW_EVENT(RHICmdList, StaticGeometry);

		FSceneBitArray StaticMeshVisibilityMap = View.StaticMeshVisibilityMap;

		int NumCulled = 0;

		if (Args.LightSceneInfo)
		{
			// For passes that voxelize geometry for a light, perform culling against the light frustum:
			// iterate over meshes that intersect with the clipmap, and hide all that are not in affected by the light.

			FLightSceneInfoCompact LightSceneInfoCompact(const_cast<FLightSceneInfo*>(Args.LightSceneInfo));

			Scene->VxgiVoxelizationDrawList.IterateOverMeshes([&LightSceneInfoCompact, &StaticMeshVisibilityMap](FStaticMesh* Mesh)
			{
				FBitReference MeshBit = StaticMeshVisibilityMap.AccessCorrespondingBit(FRelativeBitReference(Mesh->Id));
				if (!MeshBit)
					return;

				FPrimitiveSceneProxy* Proxy = Mesh->PrimitiveSceneInfo->Proxy;
				if (!LightSceneInfoCompact.AffectsPrimitive(Proxy->GetBounds(), Proxy))
				{
					MeshBit = 0;
				}
			});
		}
		else if (VoxelizationPass == VXGI::VoxelizationPass::EMISSIVE_AND_IRRADIANCE)
		{
			// For the final emissive / indirect irradiance / sky light pass, only draw meshes that were not 
			// drawn in any of the previous emittance voxelization passes. If a mesh was drawn before, the emissive etc.
			// components were added on the first emittance voxelization pass.

			Scene->VxgiVoxelizationDrawList.IterateOverMeshes([this, &StaticMeshVisibilityMap](FStaticMesh* Mesh)
			{
				FBitReference MeshBit = StaticMeshVisibilityMap.AccessCorrespondingBit(FRelativeBitReference(Mesh->Id));
				if (!MeshBit)
					return;

				if (Mesh->PrimitiveSceneInfo->VxgiLastVoxelizationPass != VXGI::VoxelizationPass::OPACITY)
				{
					MeshBit = 0;
				}
				else 
				{
					bool bIsEmissive = Mesh->MaterialRenderProxy->GetMaterial(Scene->GetFeatureLevel())->HasEmissiveColorConnected();
					bIsEmissive = bIsEmissive && bVxgiUseEmissiveMaterials;

					if (!bIsEmissive && !bVxgiMultiBounceEnable && !bVxgiSkyLightEnable)
					{
						MeshBit = 0;
					}
				}
			});
		}

		FDrawingPolicyRenderState RenderState(View);
		RenderState.SetBlendState(TStaticBlendState<>::GetRHI());
		RenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		Scene->VxgiVoxelizationDrawList.DrawVisible(RHICmdList, View, RenderState, StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_VxgiVoxelizationDynamicGeometry);
		SCOPED_DRAW_EVENT(RHICmdList, DynamicGeometry);

		TVXGIVoxelizationDrawingPolicyFactory::ContextType Context;

		FDrawingPolicyRenderState RenderState(View);
		RenderState.SetBlendState(TStaticBlendState<>::GetRHI());
		RenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

		if (Args.LightSceneInfo)
		{
			FLightSceneInfoCompact LightSceneInfoCompact(const_cast<FLightSceneInfo*>(Args.LightSceneInfo));

			for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
			{
				const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

				if (MeshBatchAndRelevance.GetHasOpaqueOrMaskedMaterial() && MeshBatchAndRelevance.GetRenderInMainPass())
				{
					const FPrimitiveSceneProxy* Proxy = MeshBatchAndRelevance.PrimitiveSceneProxy;
					if (!LightSceneInfoCompact.AffectsPrimitive(Proxy->GetBounds(), Proxy))
					{
						continue;
					}

					const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;

					TVXGIVoxelizationDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, true, RenderState, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
				}
			}
		}
		else
		{
			for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
			{
				const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

				if (MeshBatchAndRelevance.GetHasOpaqueOrMaskedMaterial() && MeshBatchAndRelevance.GetRenderInMainPass())
				{
					const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;

					if (VoxelizationPass == VXGI::VoxelizationPass::EMISSIVE_AND_IRRADIANCE)
					{
						bool bIsEmissive = MeshBatch.MaterialRenderProxy->GetMaterial(Scene->GetFeatureLevel())->HasEmissiveColorConnected();
						bIsEmissive = bIsEmissive && bVxgiUseEmissiveMaterials;

						if (!bIsEmissive && !bVxgiMultiBounceEnable && !bVxgiSkyLightEnable)
						{
							continue;
						}
					}

					TVXGIVoxelizationDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, true, RenderState, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
				}
			}
		}

		View.SimpleElementCollector.DrawBatchedElements(RHICmdList, RenderState, View, FTexture2DRHIRef(), EBlendModeFilter::OpaqueAndMasked);

		if (!View.Family->EngineShowFlags.CompositeEditorPrimitives)
		{
			bool bDirty = false;
			bDirty = DrawViewElements<TVXGIVoxelizationDrawingPolicyFactory>(RHICmdList, View, RenderState, Context, SDPG_World, true) || bDirty;
			bDirty = DrawViewElements<TVXGIVoxelizationDrawingPolicyFactory>(RHICmdList, View, RenderState, Context, SDPG_Foreground, true) || bDirty;
		}
	}

	VxgiInterface->endVoxelizationDrawCallGroup();

	RHICmdList.VXGICleanupAfterVoxelization();
	RHIPopVoxelizationFlag();
}

VXGI::Box3f FSceneRenderer::GetVxgiWorldSpaceSceneBounds()
{
	VXGI::Box3f SceneExtents(VXGI::Vector3f(-FLT_MAX), VXGI::Vector3f(FLT_MAX));
	return SceneExtents;
}

void FSceneRenderer::SetVxgiDiffuseTracingParameters(const FViewInfo& View, VXGI::DiffuseTracingParameters &TracingParams)
{
	const auto& PostSettings = View.FinalPostProcessSettings;
	
	TracingParams.irradianceScale = PostSettings.VxgiDiffuseTracingIntensity;
	TracingParams.numCones = PostSettings.VxgiDiffuseTracingNumCones;
	TracingParams.autoConeAngle = !!PostSettings.bVxgiDiffuseTracingAutoAngle;
	TracingParams.tracingSparsity = PostSettings.VxgiDiffuseTracingSparsity;
	TracingParams.coneAngle = PostSettings.VxgiDiffuseTracingConeAngle;
	TracingParams.enableConeRotation = !!PostSettings.bVxgiDiffuseTracingConeRotation;
	TracingParams.enableRandomConeOffsets = !!PostSettings.bVxgiDiffuseTracingRandomConeOffsets;
	TracingParams.coneNormalGroupingFactor = PostSettings.VxgiDiffuseTracingConeNormalGroupingFactor;
	TracingParams.maxSamples = PostSettings.VxgiDiffuseTracingMaxSamples;
	TracingParams.tracingStep = PostSettings.VxgiDiffuseTracingStep;
	TracingParams.opacityCorrectionFactor = PostSettings.VxgiDiffuseTracingOpacityCorrectionFactor;
	TracingParams.normalOffsetFactor = PostSettings.VxgiDiffuseTracingNormalOffsetFactor;
	TracingParams.environmentMapTint = VXGI::Vector3f(
		PostSettings.VxgiDiffuseTracingEnvironmentMapTint.R,
		PostSettings.VxgiDiffuseTracingEnvironmentMapTint.G,
		PostSettings.VxgiDiffuseTracingEnvironmentMapTint.B);
	TracingParams.flipOpacityDirections = PostSettings.bVxgiDiffuseTracingFlipOpacityDirections;
	TracingParams.initialOffsetBias = PostSettings.VxgiDiffuseTracingInitialOffsetBias;
	TracingParams.initialOffsetDistanceFactor = PostSettings.VxgiDiffuseTracingInitialOffsetDistanceFactor;
	TracingParams.nearClipZ = VXGI_HARDWARE_DEPTH_NEAR;
	TracingParams.farClipZ = VXGI_HARDWARE_DEPTH_FAR;
	TracingParams.enableTemporalReprojection = !!PostSettings.bVxgiDiffuseTracingTemporalReprojectionEnabled;
	TracingParams.temporalReprojectionWeight = PostSettings.VxgiDiffuseTracingTemporalReprojectionPreviousFrameWeight;
	TracingParams.temporalReprojectionMaxDistanceInVoxels = PostSettings.VxgiDiffuseTracingTemporalReprojectionMaxDistanceInVoxels;
	TracingParams.temporalReprojectionNormalWeightExponent = PostSettings.VxgiDiffuseTracingTemporalReprojectionNormalWeightExponent;
	TracingParams.enableSparseTracingRefinement = PostSettings.bVxgiDiffuseTracingRefinementEnabled;

	TracingParams.ambientColor = VXGI::Vector3f(
		PostSettings.VxgiAmbientColor.R,
		PostSettings.VxgiAmbientColor.G,
		PostSettings.VxgiAmbientColor.B);

	TracingParams.ambientRange = PostSettings.VxgiAmbientRange;
	TracingParams.ambientScale = PostSettings.VxgiAmbientScale;
	TracingParams.ambientBias = PostSettings.VxgiAmbientBias;
	TracingParams.ambientPower = PostSettings.VxgiAmbientPowerExponent;
	TracingParams.ambientDistanceDarkening = PostSettings.VxgiAmbientDistanceDarkening;

			
	if (View.FinalPostProcessSettings.VxgiDiffuseTracingEnvironmentMap && View.FinalPostProcessSettings.VxgiDiffuseTracingEnvironmentMap->Resource)
	{
		FTextureResource* TextureResource = View.FinalPostProcessSettings.VxgiDiffuseTracingEnvironmentMap->Resource;
		FRHITexture* Texture = TextureResource->TextureRHI->GetTextureCube();
		TracingParams.environmentMap = GDynamicRHI->GetVXGITextureFromRHI(Texture);
	} 
}

void FSceneRenderer::SetVxgiSpecularTracingParameters(const FViewInfo& View, VXGI::SpecularTracingParameters &TracingParams)
{
	const auto& PostSettings = View.FinalPostProcessSettings;

	TracingParams.irradianceScale = PostSettings.VxgiSpecularTracingIntensity;
	TracingParams.maxSamples = PostSettings.VxgiSpecularTracingMaxSamples;
	TracingParams.tracingStep = PostSettings.VxgiSpecularTracingTracingStep;
	TracingParams.opacityCorrectionFactor = PostSettings.VxgiSpecularTracingOpacityCorrectionFactor;
	TracingParams.flipOpacityDirections = false;
	TracingParams.initialOffsetBias = PostSettings.VxgiSpecularTracingInitialOffsetBias;
	TracingParams.initialOffsetDistanceFactor = PostSettings.VxgiSpecularTracingInitialOffsetDistanceFactor;
	TracingParams.environmentMapTint = VXGI::Vector3f(
		PostSettings.VxgiSpecularTracingEnvironmentMapTint.R,
		PostSettings.VxgiSpecularTracingEnvironmentMapTint.G,
		PostSettings.VxgiSpecularTracingEnvironmentMapTint.B);
	TracingParams.nearClipZ = VXGI_HARDWARE_DEPTH_NEAR;
	TracingParams.farClipZ = VXGI_HARDWARE_DEPTH_FAR;
	TracingParams.tangentJitterScale = PostSettings.VxgiSpecularTracingTangentJitterScale;

	switch(PostSettings.VxgiSpecularTracingFilter)
	{
	case VXGISTF_Temporal:      TracingParams.filter = VXGI::SpecularTracingParameters::FILTER_TEMPORAL; break;
	case VXGISTF_Simple:        TracingParams.filter = VXGI::SpecularTracingParameters::FILTER_SIMPLE; break;
	default:                    TracingParams.filter = VXGI::SpecularTracingParameters::FILTER_NONE; break;
	}
	
	if (View.FinalPostProcessSettings.VxgiSpecularTracingEnvironmentMap && View.FinalPostProcessSettings.VxgiSpecularTracingEnvironmentMap->Resource)
	{
		FTextureResource* TextureResource = View.FinalPostProcessSettings.VxgiSpecularTracingEnvironmentMap->Resource;
		FRHITexture* Texture = TextureResource->TextureRHI->GetTextureCube();
		TracingParams.environmentMap = GDynamicRHI->GetVXGITextureFromRHI(Texture);
	} 
}

void FSceneRenderer::SetVxgiInputBuffers(FSceneRenderTargets& SceneContext, const FViewInfo& View, VXGI::IViewTracer::InputBuffers& InputBuffers, VXGI::IViewTracer::InputBuffers& InputBuffersPreviousFrame)
{
	InputBuffers.gbufferDepth = SceneContext.GetVxgiSceneDepthTextureHandle();
	InputBuffers.gbufferNormal = SceneContext.GetVxgiNormalAndRoughnessTextureHandle();

	FMemory::Memcpy(&InputBuffers.viewMatrix, &View.ViewMatrices.GetViewMatrix(), sizeof(VXGI::Matrix4f));
	FMemory::Memcpy(&InputBuffers.projMatrix, &View.ViewMatrices.GetProjectionMatrix(), sizeof(VXGI::Matrix4f));
	
	InputBuffers.gbufferViewport = NVRHI::Viewport(
		(float)View.ViewRect.Min.X, (float)View.ViewRect.Max.X,
		(float)View.ViewRect.Min.Y, (float)View.ViewRect.Max.Y, 0.f, 1.0f);

	// VXGI uses N = FetchedNormal.xyz * Scale + Bias
	InputBuffers.gbufferNormalScale = 1.f;
	InputBuffers.gbufferNormalBias = 0.f;

	if (View.State != nullptr)
	{
		FSceneViewState* ViewState = static_cast<FSceneViewState*>(View.State);
		InputBuffersPreviousFrame = InputBuffers;
		InputBuffersPreviousFrame.gbufferDepth = ViewState->GetPreviousVxgiSceneDepthTextureHandle();
		InputBuffersPreviousFrame.gbufferNormal = ViewState->GetPreviousVxgiNormalAndRoughnessTextureHandle();

		FMemory::Memcpy(&InputBuffersPreviousFrame.viewMatrix, &ViewState->PrevViewMatrices.GetViewMatrix(), sizeof(VXGI::Matrix4f));
		FMemory::Memcpy(&InputBuffersPreviousFrame.projMatrix, &ViewState->PrevViewMatrices.GetProjectionMatrix(), sizeof(VXGI::Matrix4f));
	}
}

class FComposeVxgiGBufferPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FComposeVxgiGBufferPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FComposeVxgiGBufferPS() {}

	FDeferredPixelShaderParameters DeferredParameters;
public:
	// we need:  specular intensity in albedo.w and specular roughness in normal.w

	/** Initialization constructor. */
	FComposeVxgiGBufferPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
		DeferredParameters.Set(RHICmdList, GetPixelShader(), View, EMaterialDomain::MD_PostProcess);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(, FComposeVxgiGBufferPS, TEXT("/Engine/VXGICompositing.usf"), TEXT("ComposeVxgiGBufferPS"), SF_Pixel);

void FSceneRenderer::PrepareVxgiGBuffer(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	if (!IsVxgiEnabled(View) || bVxgiDebugRendering)
	{
		return;
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	// we need  specular roughness in normal.w
	SCOPED_DRAW_EVENT(RHICmdList, PrepareTracingInputs);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FScreenVS> VertexShader(View.ShaderMap);
	TShaderMapRef<FComposeVxgiGBufferPS> ComposeVxgiGBufferPS(View.ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*ComposeVxgiGBufferPS);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	FTextureRHIParamRef Targets[] = { SceneContext.VxgiNormalAndRoughness->GetRenderTargetItem().TargetableTexture };
	SetRenderTargets(RHICmdList, 1, Targets, NULL, 0, NULL);

	RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

	VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
	ComposeVxgiGBufferPS->SetParameters(RHICmdList, View);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		RHICmdList,
		0, 0,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Min.X, View.ViewRect.Min.Y,
		View.ViewRect.Width(), View.ViewRect.Height(),
		FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
		SceneContext.GetBufferSizeXY(),
		*VertexShader);
}

void FSceneRenderer::RenderVxgiTracingForView(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	if (!IsVxgiEnabled(View) || bVxgiDebugRendering)
	{
		return;
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	VXGI::IGlobalIllumination* VxgiInterface = GDynamicRHI->RHIVXGIGetInterface();
	VXGI::IViewTracer* VxgiViewTracer = nullptr;
	bool bVxgiTemporaryTracer = false;
	if (View.State != nullptr)
	{
		VxgiViewTracer = ((FSceneViewState*)View.State)->GetVXGITracer();
	}
	else
	{
		VXGI::Status::Enum Status = VxgiInterface->createNewTracer(&VxgiViewTracer);

		if (VXGI_FAILED(Status))
			return;

		bVxgiTemporaryTracer = true;
	}

	VXGI::IViewTracer::InputBuffers InputBuffers, InputBuffersPreviousFrame;
	SetVxgiInputBuffers(SceneContext, View, InputBuffers, InputBuffersPreviousFrame);
	
	const bool bPreviousBuffersValid = View.State != nullptr && !View.bPrevTransformsReset &&
		InputBuffersPreviousFrame.gbufferDepth != nullptr && InputBuffersPreviousFrame.gbufferNormal != nullptr;

	SCOPED_DRAW_EVENT(RHICmdList, VXGITracing);

	{
		SCOPED_DRAW_EVENT(RHICmdList, DiffuseConeTracing);
		NVRHI::TextureHandle IlluminationDiffuseHandle = NULL;

		VXGI::DiffuseTracingParameters DiffuseTracingParams;
		SetVxgiDiffuseTracingParameters(View, DiffuseTracingParams);

		if (View.FinalPostProcessSettings.VxgiDiffuseTracingEnabled)
		{
			auto Status = VxgiViewTracer->computeDiffuseChannel(DiffuseTracingParams, IlluminationDiffuseHandle, InputBuffers, bPreviousBuffersValid ? &InputBuffersPreviousFrame : NULL);
			check(VXGI_SUCCEEDED(Status));
		}

		FTextureRHIRef IlluminationDiffuse(GDynamicRHI->GetRHITextureFromVXGI(IlluminationDiffuseHandle));
		if (IlluminationDiffuse)
		{
			SceneContext.VxgiOutputDiffuse[View.VxgiViewIndex] = IlluminationDiffuse->GetTexture2D();
		}
	}

	{
		SCOPED_DRAW_EVENT(RHICmdList, SpecularConeTracing);
		NVRHI::TextureHandle IlluminationSpecHandle = NULL;

		VXGI::SpecularTracingParameters SpecularTracingParams;
		SetVxgiSpecularTracingParameters(View, SpecularTracingParams);

		if (View.FinalPostProcessSettings.VxgiSpecularTracingEnabled)
		{
			auto Status = VxgiViewTracer->computeSpecularChannel(SpecularTracingParams, IlluminationSpecHandle, InputBuffers, bPreviousBuffersValid ? &InputBuffersPreviousFrame : NULL);
			check(VXGI_SUCCEEDED(Status));
		}

		FTextureRHIRef IlluminationSpec(GDynamicRHI->GetRHITextureFromVXGI(IlluminationSpecHandle));
		if (IlluminationSpec)
		{
			SceneContext.VxgiOutputSpec[View.VxgiViewIndex] = IlluminationSpec->GetTexture2D();
		}
	}

	if (bVxgiTemporaryTracer)
	{
		VxgiInterface->destroyTracer(VxgiViewTracer);
	}

	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
}

void FSceneRenderer::RenderVxgiDebug(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, int ViewIndex)
{
	if (!IsVxgiEnabled(View))
	{
		return;
	}

	VXGI::DebugRenderParameters Params;

	if (ViewFamily.EngineShowFlags.VxgiOpacityVoxels)
		Params.debugMode = VXGI::DebugRenderMode::OPACITY_TEXTURE;
	else if (ViewFamily.EngineShowFlags.VxgiEmittanceVoxels)
		Params.debugMode = VXGI::DebugRenderMode::EMITTANCE_TEXTURE;
	else if (ViewFamily.EngineShowFlags.VxgiIrradianceVoxels)
		Params.debugMode = VXGI::DebugRenderMode::INDIRECT_IRRADIANCE_TEXTURE;
	else
		return;


	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	SCOPED_DRAW_EVENT(RHICmdList, VXGI);

	VXGI::IGlobalIllumination* VxgiInterface = GDynamicRHI->RHIVXGIGetInterface();


	// With reverse infinite projections, the near plane is at Z=1 and the far plane is at Z=0
	// The lib uses these 2 values along with the ViewProjMatrix to compute the ray directions
	const float nearClipZ = 1.0f;
	const float farClipZ = 0.0f;

	Params.viewport = NVRHI::Viewport(View.ViewRect.Min.X, View.ViewRect.Max.X, View.ViewRect.Min.Y, View.ViewRect.Max.Y, 0.f, 1.f);

	Params.destinationTexture = GDynamicRHI->GetVXGITextureFromRHI((FRHITexture*)SceneContext.GetSceneColorSurface().GetReference());
	
	SCOPED_DRAW_EVENT(RHICmdList, RenderDebug);

	const int32 BlendDebug = CVarVxgiDebugBlendOutput.GetValueOnRenderThread();

	FRHISetRenderTargetsInfo RTInfo;
	RTInfo.bClearColor = false;
	RTInfo.bClearDepth = true;
	RTInfo.NumColorRenderTargets = 1;
	RTInfo.ColorRenderTarget[0] = FRHIRenderTargetView(SceneContext.GetSceneColorSurface(), ERenderTargetLoadAction::ELoad);
	RTInfo.DepthStencilRenderTarget = FRHIDepthRenderTargetView(SceneContext.GetSceneDepthSurface(), ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore);
	RHICmdList.SetRenderTargetsAndClear(RTInfo);

	RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

	Params.destinationDepth = GDynamicRHI->GetVXGITextureFromRHI((FRHITexture*)SceneContext.GetSceneDepthSurface().GetReference());

	VXGI::Matrix4f ViewMatrix, ProjMatrix;
	FMemory::Memcpy(&Params.viewMatrix, &View.ViewMatrices.GetViewMatrix(), sizeof(VXGI::Matrix4f));
	FMemory::Memcpy(&Params.projMatrix, &View.ViewMatrices.GetProjectionMatrix(), sizeof(VXGI::Matrix4f));

	if (Params.debugMode == VXGI::DebugRenderMode::OPACITY_TEXTURE || Params.debugMode == VXGI::DebugRenderMode::EMITTANCE_TEXTURE)
	{
		Params.level = CVarVxgiDebugClipmapLevel.GetValueOnRenderThread();
		Params.level = FMath::Min(Params.level, VxgiVoxelizationParameters.stackLevels * 2 + VxgiVoxelizationParameters.mipLevels);
	}
	else
	{
		Params.level = 0;
	}

	Params.bitToDisplay = 0;
	Params.voxelsToSkip = CVarVxgiDebugVoxelsToSkip.GetValueOnRenderThread();
	Params.nearClipZ = nearClipZ;
	Params.farClipZ = farClipZ;

	Params.depthStencilState.depthEnable = true;
	Params.depthStencilState.depthFunc = NVRHI::DepthStencilState::COMPARISON_GREATER;

	auto Status = VxgiInterface->renderDebug(Params);

	check(VXGI_SUCCEEDED(Status));
}

/** Encapsulates the post processing ambient occlusion pixel shader. */
template<bool bRawDiffuse>
class FAddVxgiDiffusePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FAddVxgiDiffusePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	/** Default constructor. */
	FAddVxgiDiffusePS() {}

	FDeferredPixelShaderParameters DeferredParameters;

public:
	/** Initialization constructor. */
	FAddVxgiDiffusePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
		DeferredParameters.Set(RHICmdList, GetPixelShader(), View, EMaterialDomain::MD_PostProcess);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}
};

typedef FAddVxgiDiffusePS<false> FAddVxgiCompositedDiffusePS;
typedef FAddVxgiDiffusePS<true>  FAddVxgiRawDiffusePS;

IMPLEMENT_SHADER_TYPE(, FAddVxgiCompositedDiffusePS, TEXT("/Engine/VXGICompositing.usf"), TEXT("AddVxgiDiffusePS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FAddVxgiRawDiffusePS, TEXT("/Engine/VXGICompositing.usf"), TEXT("AddVxgiRawDiffusePS"), SF_Pixel);

void FSceneRenderer::CompositeVxgiDiffuseTracing(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	check(!bVxgiAmbientOcclusionMode);

	//Make sure this after tracing always
	if (IsValidRef(SceneContext.VxgiOutputDiffuse[View.VxgiViewIndex])) //if it's on and we outputted something
	{
		SCOPED_DRAW_EVENT(RHICmdList, VXGICompositeDiffuse);

		SceneContext.BeginRenderingSceneColor(RHICmdList);

		//Blend in the results
		const FSceneRenderTargetItem& DestRenderTarget = bVxgiAmbientOcclusionMode 
			? SceneContext.ScreenSpaceAO->GetRenderTargetItem()
			: SceneContext.GetSceneColor()->GetRenderTargetItem();
		SetRenderTarget(RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		TShaderMapRef<FScreenVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FComposeVxgiGBufferPS> ComposeVxgiGBufferPS(View.ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		if (CVarVxgiCompositingMode.GetValueOnRenderThread())
		{
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA>::GetRHI();

			TShaderMapRef<FAddVxgiRawDiffusePS> PixelShader(View.ShaderMap);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			PixelShader->SetParameters(RHICmdList, View);
		}
		else
		{
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
			TShaderMapRef<FAddVxgiCompositedDiffusePS> PixelShader(View.ShaderMap);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			PixelShader->SetParameters(RHICmdList, View);
		}

		VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);

		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		// Draw a quad mapping scene color to the view's render target
		DrawRectangle(
			RHICmdList,
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Size(),
			SceneContext.GetBufferSizeXY(),
			*VertexShader);
	}
}

bool FVXGIVoxelizationNoLightMapPolicy::ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
{
	return true;
}

void FVXGIVoxelizationNoLightMapPolicy::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("SIMPLE_DYNAMIC_LIGHTING"),TEXT("1"));
	FNoLightMapPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
}

//Override this and add the VXGI internal hash to trigger recompiles if the version changes
const FSHAHash& FVXGIVoxelizationMeshMaterialShaderType::GetSourceHash() const
{
	if (HashWithVXGIHash == FSHAHash())
	{
		FSHA1 HashState;
		{
			FSHAHash FileHash = FMeshMaterialShaderType::GetSourceHash();
			HashState.Update(&FileHash.Hash[0], sizeof(FileHash.Hash));
		}
		{
			FWindowsPlatformMisc::LoadVxgiModule(); //Might not be loaded with a no-op RHI
			auto Status = VXGI::VFX_VXGI_VerifyInterfaceVersion();
			check(VXGI_SUCCEEDED(Status));
			uint64_t VXGIHash = VXGI::VFX_VXGI_GetInternalShaderHash();
			FWindowsPlatformMisc::UnloadVxgiModule(); //Do we want to bother unloading? Might be slower if we get called a bunch of times

			// VXGIHash += 1; // To trigger shader recompile when changing ShaderCompiler

			HashState.Update((uint8*)&VXGIHash, sizeof(VXGIHash));
		}
		HashState.Final();
		HashState.GetHash(&HashWithVXGIHash.Hash[0]);
	}
	return HashWithVXGIHash;
}

#define IMPLEMENT_VXGI_VOXELIZATION_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	typedef TVXGIVoxelizationHS< LightMapPolicyType > TVXGIVoxelizationHS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TVXGIVoxelizationHS##LightMapPolicyName,TEXT("/Engine/Private/MobileBasePassVertexShader.usf"),TEXT("MainHull"),SF_Hull); \
	typedef TVXGIVoxelizationDS< LightMapPolicyType > TVXGIVoxelizationDS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TVXGIVoxelizationDS##LightMapPolicyName,TEXT("/Engine/Private/MobileBasePassVertexShader.usf"),TEXT("MainDomain"),SF_Domain); \
	typedef TVXGIVoxelizationVS< LightMapPolicyType > TVXGIVoxelizationVS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TVXGIVoxelizationVS##LightMapPolicyName,TEXT("/Engine/Private/MobileBasePassVertexShader.usf"),TEXT("Main"),SF_Vertex); \
	typedef TVXGIVoxelizationPS< LightMapPolicyType > TVXGIVoxelizationPS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TVXGIVoxelizationPS##LightMapPolicyName,TEXT("/Engine/VXGIVoxelizationPixelShader.usf"),TEXT("Main"),SF_Pixel); \
	typedef TVXGIVoxelizationShaderPermutationPS< LightMapPolicyType > TVXGIVoxelizationShaderPermutationPS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TVXGIVoxelizationShaderPermutationPS##LightMapPolicyName,TEXT("/Engine/VXGIVoxelizationPixelShader.usf"),TEXT("Main"),SF_Pixel);

// Implement shader types only for FVXGIVoxelizationNoLightMapPolicy because we control the drawing process
IMPLEMENT_VXGI_VOXELIZATION_SHADER_TYPE(FVXGIVoxelizationNoLightMapPolicy, FVXGIVoxelizationNoLightMapPolicy);

#define IMPLEMENT_VXGI_CONE_TRACING_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	typedef TVXGIConeTracingPS< LightMapPolicyType > TVXGIConeTracingPS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TVXGIConeTracingPS##LightMapPolicyName,TEXT("/Engine/Private/BasePassPixelShader.usf"),TEXT("MainPS"),SF_Pixel); \
	typedef TVXGIConeTracingShaderPermutationPS< LightMapPolicyType > TVXGIConeTracingShaderPermutationPS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TVXGIConeTracingShaderPermutationPS##LightMapPolicyName,TEXT("/Engine/Private/BasePassPixelShader.usf"),TEXT("MainPS"),SF_Pixel);

// Implement shader types for all light map policies that are used in ProcessBasePassMesh
IMPLEMENT_VXGI_CONE_TRACING_SHADER_TYPE(TUniformLightMapPolicy<LMP_NO_LIGHTMAP>, _LMP_NO_LIGHTMAP);
IMPLEMENT_VXGI_CONE_TRACING_SHADER_TYPE(TUniformLightMapPolicy<LMP_HQ_LIGHTMAP>, _LMP_HQ_LIGHTMAP);
IMPLEMENT_VXGI_CONE_TRACING_SHADER_TYPE(TUniformLightMapPolicy<LMP_LQ_LIGHTMAP>, _LMP_LQ_LIGHTMAP);
IMPLEMENT_VXGI_CONE_TRACING_SHADER_TYPE(TUniformLightMapPolicy<LMP_SIMPLE_NO_LIGHTMAP>, _LMP_SIMPLE_NO_LIGHTMAP);
IMPLEMENT_VXGI_CONE_TRACING_SHADER_TYPE(TUniformLightMapPolicy<LMP_SIMPLE_LIGHTMAP_ONLY_LIGHTING>, _LMP_SIMPLE_LIGHTMAP_ONLY_LIGHTING);
IMPLEMENT_VXGI_CONE_TRACING_SHADER_TYPE(TUniformLightMapPolicy<LMP_SIMPLE_DIRECTIONAL_LIGHT_LIGHTING>, _LMP_SIMPLE_DIRECTIONAL_LIGHT_LIGHTING);
IMPLEMENT_VXGI_CONE_TRACING_SHADER_TYPE(TUniformLightMapPolicy<LMP_SIMPLE_STATIONARY_PRECOMPUTED_SHADOW_LIGHTING>, _LMP_SIMPLE_STATIONARY_PRECOMPUTED_SHADOW_LIGHTING);
IMPLEMENT_VXGI_CONE_TRACING_SHADER_TYPE(TUniformLightMapPolicy<LMP_SIMPLE_STATIONARY_SINGLESAMPLE_SHADOW_LIGHTING>, _LMP_SIMPLE_STATIONARY_SINGLESAMPLE_SHADOW_LIGHTING);
IMPLEMENT_VXGI_CONE_TRACING_SHADER_TYPE(TUniformLightMapPolicy<LMP_CACHED_VOLUME_INDIRECT_LIGHTING>, _LMP_CACHED_VOLUME_INDIRECT_LIGHTING);
IMPLEMENT_VXGI_CONE_TRACING_SHADER_TYPE(TUniformLightMapPolicy<LMP_CACHED_POINT_INDIRECT_LIGHTING>, _LMP_CACHED_POINT_INDIRECT_LIGHTING);
IMPLEMENT_VXGI_CONE_TRACING_SHADER_TYPE(TUniformLightMapPolicy<LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP>, _LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP);
IMPLEMENT_VXGI_CONE_TRACING_SHADER_TYPE(FSelfShadowedCachedPointIndirectLightingPolicy, FSelfShadowedCachedPointIndirectLightingPolicy);
IMPLEMENT_VXGI_CONE_TRACING_SHADER_TYPE(FSelfShadowedTranslucencyPolicy, FSelfShadowedTranslucencyPolicy);
IMPLEMENT_VXGI_CONE_TRACING_SHADER_TYPE(FSelfShadowedVolumetricLightmapPolicy, FSelfShadowedVolumetricLightmapPolicy);

template <>
void GetConeTracingPixelShader<FUniformLightMapPolicy>(
	const FVertexFactory* InVertexFactory,
	const FMaterial& InMaterialResource,
	FUniformLightMapPolicy* InLightMapPolicy,
	TBasePassPixelShaderPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& PixelShader)
{
	switch (InLightMapPolicy->GetIndirectPolicy())
	{
	case LMP_CACHED_VOLUME_INDIRECT_LIGHTING:
		GetConeTracingPixelShader<TUniformLightMapPolicy<LMP_CACHED_VOLUME_INDIRECT_LIGHTING>>(InVertexFactory, InMaterialResource, nullptr, PixelShader);
		break;
	case LMP_CACHED_POINT_INDIRECT_LIGHTING:
		GetConeTracingPixelShader<TUniformLightMapPolicy<LMP_CACHED_POINT_INDIRECT_LIGHTING>>(InVertexFactory, InMaterialResource, nullptr, PixelShader);
		break;
	case LMP_SIMPLE_NO_LIGHTMAP:
		GetConeTracingPixelShader<TUniformLightMapPolicy<LMP_SIMPLE_NO_LIGHTMAP>>(InVertexFactory, InMaterialResource, nullptr, PixelShader);
	case LMP_SIMPLE_LIGHTMAP_ONLY_LIGHTING:
		GetConeTracingPixelShader<TUniformLightMapPolicy<LMP_SIMPLE_LIGHTMAP_ONLY_LIGHTING>>(InVertexFactory, InMaterialResource, nullptr, PixelShader);
	case LMP_SIMPLE_DIRECTIONAL_LIGHT_LIGHTING:
		GetConeTracingPixelShader<TUniformLightMapPolicy<LMP_SIMPLE_DIRECTIONAL_LIGHT_LIGHTING>>(InVertexFactory, InMaterialResource, nullptr, PixelShader);
	case LMP_SIMPLE_STATIONARY_PRECOMPUTED_SHADOW_LIGHTING:
		GetConeTracingPixelShader<TUniformLightMapPolicy<LMP_SIMPLE_STATIONARY_PRECOMPUTED_SHADOW_LIGHTING>>(InVertexFactory, InMaterialResource, nullptr, PixelShader);
	case LMP_SIMPLE_STATIONARY_SINGLESAMPLE_SHADOW_LIGHTING:
		GetConeTracingPixelShader<TUniformLightMapPolicy<LMP_SIMPLE_STATIONARY_SINGLESAMPLE_SHADOW_LIGHTING>>(InVertexFactory, InMaterialResource, nullptr, PixelShader);
		break;
	case LMP_LQ_LIGHTMAP:
		GetConeTracingPixelShader<TUniformLightMapPolicy<LMP_LQ_LIGHTMAP>>(InVertexFactory, InMaterialResource, nullptr, PixelShader);
		break;
	case LMP_HQ_LIGHTMAP:
		GetConeTracingPixelShader<TUniformLightMapPolicy<LMP_HQ_LIGHTMAP>>(InVertexFactory, InMaterialResource, nullptr, PixelShader);
		break;
	case LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP:
		GetConeTracingPixelShader<TUniformLightMapPolicy<LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP>>(InVertexFactory, InMaterialResource, nullptr, PixelShader);
		break;
	case LMP_NO_LIGHTMAP:
		GetConeTracingPixelShader<TUniformLightMapPolicy<LMP_NO_LIGHTMAP>>(InVertexFactory, InMaterialResource, nullptr, PixelShader);
		break;
	default:										
		check(false);
	}
}


void TVXGIVoxelizationDrawingPolicyFactory::AddStaticMesh(FRHICommandList& RHICmdList, FScene* Scene, FStaticMesh* StaticMesh)
{
	const FMaterial* Material = StaticMesh->MaterialRenderProxy->GetMaterial(Scene->GetFeatureLevel());
	if (IsMaterialIgnored(StaticMesh->MaterialRenderProxy, Scene->GetFeatureLevel()))
	{
		return;
	}

	Scene->VxgiVoxelizationDrawList.AddMesh(
		StaticMesh,
		typename TVXGIVoxelizationDrawingPolicy<FVXGIVoxelizationNoLightMapPolicy>::ElementDataType(StaticMesh->PrimitiveSceneInfo),
		TVXGIVoxelizationDrawingPolicy<FVXGIVoxelizationNoLightMapPolicy>(
			StaticMesh->VertexFactory,
			StaticMesh->MaterialRenderProxy,
			*Material,
			ComputeMeshOverrideSettings(*StaticMesh),
			FVXGIVoxelizationNoLightMapPolicy()
			),
		Scene->GetFeatureLevel()
	);
}

bool TVXGIVoxelizationDrawingPolicyFactory::DrawDynamicMesh(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	bool bPreFog,
	FDrawingPolicyRenderState& DrawRenderState,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId
	)
{
	const FMaterial* Material = Mesh.MaterialRenderProxy->GetMaterial(View.GetFeatureLevel());
	if (IsMaterialIgnored(Mesh.MaterialRenderProxy, View.GetFeatureLevel()))
	{
		return false;
	}

	TVXGIVoxelizationDrawingPolicy<FVXGIVoxelizationNoLightMapPolicy> DrawingPolicy(
		Mesh.VertexFactory,
		Mesh.MaterialRenderProxy,
		*Material,
		ComputeMeshOverrideSettings(Mesh),
		FVXGIVoxelizationNoLightMapPolicy()
	);

	DrawingPolicy.SetSharedState(RHICmdList, DrawRenderState, &View, typename TVXGIVoxelizationDrawingPolicy<FVXGIVoxelizationNoLightMapPolicy>::ContextDataType());

	for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
	{
		TDrawEvent<FRHICommandList> MeshEvent;
		BeginMeshDrawEvent(RHICmdList, PrimitiveSceneProxy, Mesh, MeshEvent);

		DrawingPolicy.SetMeshRenderState(
			RHICmdList,
			View,
			PrimitiveSceneProxy,
			Mesh,
			BatchElementIndex,
			DrawRenderState,
			typename TVXGIVoxelizationDrawingPolicy<FVXGIVoxelizationNoLightMapPolicy>::ElementDataType(nullptr),
			typename TVXGIVoxelizationDrawingPolicy<FVXGIVoxelizationNoLightMapPolicy>::ContextDataType()
		);

		DrawingPolicy.DrawMesh(RHICmdList, Mesh, BatchElementIndex, false);
	}

	return true;
}

#endif
// NVCHANGE_END: Add VXGI
