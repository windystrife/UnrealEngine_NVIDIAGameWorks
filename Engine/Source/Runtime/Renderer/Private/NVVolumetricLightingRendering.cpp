
/*=============================================================================
	NVVolumetricLightingRendering.cpp: Nvidia Volumetric Lighting rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"

#if WITH_NVVOLUMETRICLIGHTING

#include "NVVolumetricLightingRHI.h"

#define VRWORKS_SUPPORT	0

static TAutoConsoleVariable<int32> CVarNvVlDebugMode(
	TEXT("r.NvVl.DebugMode"),
	0,
	TEXT("Debug Mode\n")
	TEXT("  0: off\n")
	TEXT("  1: wireframe\n")
	TEXT("  2: no blend\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarNvVlEnable(
	TEXT("r.NvVl.Enable"),
	1,
	TEXT("Enable Nvidia Volumetric Lighting\n")
	TEXT("  0: off\n")
	TEXT("  1: on\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarNvVlScatterScale(
	TEXT("r.NvVl.ScatterScale"),
	1.0f,
	TEXT("Scattering Scale\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarNvVlFog(
	TEXT("r.NvVl.Fog"),
	1,
	TEXT("Enable Scattering Fogging\n")
	TEXT("  0: off\n")
	TEXT("  1: on\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarNvVlSPS(
	TEXT("r.NvVl.SPS"),
	1,
	TEXT("Enable Single Pass Stereo\n")
	TEXT("  0: off\n")
	TEXT("  1: on\n"),
	ECVF_RenderThreadSafe);

static NvVl::ContextDesc NvVlContextDesc;
static NvVl::ViewerDesc NvVlViewerDesc;
static NvVl::MediumDesc NvVlMediumDesc;
static NvVl::ShadowMapDesc NvVlShadowMapDesc;
static NvVl::LightDesc NvVlLightDesc;
static NvVl::VolumeDesc NvVlVolumeDesc;
static NvVl::PostprocessDesc NvVlPostprocessDesc;

static FORCEINLINE float RemapTransmittance(const float Range, const float InValue)
{
	return InValue * Range + (1.0f - Range);
}

static FORCEINLINE float GetOpticalDepth(const float InValue)
{
	return -FMath::Loge(InValue) * CVarNvVlScatterScale.GetValueOnRenderThread();
}

#define OPTICAL_DEPTH(x)	(GetOpticalDepth(RemapTransmittance(Properties->TransmittanceRange, FinalPostProcessSettings.##x##Transmittance)))
#define DENSITY(x)			(OPTICAL_DEPTH(x) * FinalPostProcessSettings.##x##Color)

void FDeferredShadingSceneRenderer::NVVolumetricLightingBeginAccumulation(FRHICommandListImmediate& RHICmdList)
{
	if (GNVVolumetricLightingRHI == nullptr)
	{
		return;
	}

	if (!CVarNvVlEnable.GetValueOnRenderThread() || GetShadowQuality() == 0)
	{
		GNVVolumetricLightingRHI->UpdateRendering(false);

		// cleanup render resource
		GNVVolumetricLightingRHI->ReleaseContext();
		return;
	}

	check(Views.Num());
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	
	TArray<NvVl::ViewerDesc> ViewerDescs;

	for(int i = 0; i < ((Views.Num() > 1) ? 2:1); ++i)
	{
		const FViewInfo& View = Views[i];
		FMemory::Memzero(NvVlViewerDesc);
		FMatrix ProjMatrix = View.ViewMatrices.GetProjectionMatrix();
		NvVlViewerDesc.mProj = *reinterpret_cast<const NvcMat44*>(&ProjMatrix.M[0][0]);
		FMatrix ViewProjMatrix = View.ViewMatrices.GetViewProjectionMatrix();
		NvVlViewerDesc.mViewProj = *reinterpret_cast<const NvcMat44*>(&ViewProjMatrix.M[0][0]);
		NvVlViewerDesc.fZNear = NvVlViewerDesc.fZFar = GNearClippingPlane; // UE4 z near equals with z far, far = 1000000 unit internally
		NvVlViewerDesc.vEyePosition = *reinterpret_cast<const NvcVec3 *>(&View.ViewMatrices.GetViewOrigin());
		NvVlViewerDesc.uViewportTopLeftX = View.ViewRect.Min.X;
		NvVlViewerDesc.uViewportTopLeftY = View.ViewRect.Min.Y;
		NvVlViewerDesc.uViewportWidth = View.ViewRect.Width();
		NvVlViewerDesc.uViewportHeight = View.ViewRect.Height();
#if VRWORKS_SUPPORT
		NvVlViewerDesc.uNonVRProjectViewportWidth = View.NonVRProjectViewRect.Width();
		NvVlViewerDesc.uNonVRProjectViewportHeight = View.NonVRProjectViewRect.Height();
#else
		NvVlViewerDesc.uNonVRProjectViewportWidth = View.ViewRect.Width();
		NvVlViewerDesc.uNonVRProjectViewportHeight = View.ViewRect.Height();
#endif
		ViewerDescs.Add(NvVlViewerDesc);
	}

	const FViewInfo& View = Views[0];
	FMemory::Memzero(NvVlMediumDesc);
	const FNVVolumetricLightingProperties* Properties = Scene->VolumetricLightingProperties;
	const FFinalPostProcessSettings& FinalPostProcessSettings = View.FinalPostProcessSettings;

	FVector Absorption = DENSITY(Absorption);
	NvVlMediumDesc.vAbsorption = *reinterpret_cast<const NvcVec3 *>(&Absorption);
	NvVlMediumDesc.uNumPhaseTerms = 0;
	// Rayleigh
	if (FinalPostProcessSettings.RayleighTransmittance < 1.0f)
	{
		NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].ePhaseFunc = NvVl::PhaseFunctionType::RAYLEIGH;
		FVector Density = OPTICAL_DEPTH(Rayleigh) * FVector(5.8f, 13.6f, 33.1f) * 0.01f;
		NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
		NvVlMediumDesc.uNumPhaseTerms++;
	}

	// Simple Approach
	// Mie
	if (FinalPostProcessSettings.MieBlendFactor > 0.0f && (FinalPostProcessSettings.MieTransmittance < 1.0f && FinalPostProcessSettings.MieColor != FLinearColor::Black))
	{
		float BlendMieHazy = 1.0f - FMath::Abs(1.0f - 2 * FinalPostProcessSettings.MieBlendFactor);
		float BlendMieMurky = FMath::Max(0.0f, 2.0f * FinalPostProcessSettings.MieBlendFactor - 1.0f);
		
		FVector MieDensity = DENSITY(Mie);
		NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].ePhaseFunc = NvVl::PhaseFunctionType::MIE_HAZY;
		FVector Density = BlendMieHazy * MieDensity;
		NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
		NvVlMediumDesc.uNumPhaseTerms++;

		NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].ePhaseFunc = NvVl::PhaseFunctionType::MIE_MURKY;
		Density = BlendMieMurky * MieDensity;
		NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
		NvVlMediumDesc.uNumPhaseTerms++;
	}
	else
	// Three-Variable Approach
	// HG
	{
		if (FinalPostProcessSettings.HGTransmittance < 1.0f && FinalPostProcessSettings.HGColor != FLinearColor::Black)
		{
			FVector HGDensity = DENSITY(HG);
			NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].ePhaseFunc = NvVl::PhaseFunctionType::HENYEYGREENSTEIN;
			FVector Density = (1.0f - FinalPostProcessSettings.HGEccentricityRatio) * HGDensity;
			NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
			NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].fEccentricity = FinalPostProcessSettings.HGEccentricity1;
			NvVlMediumDesc.uNumPhaseTerms++;

			NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].ePhaseFunc = NvVl::PhaseFunctionType::HENYEYGREENSTEIN;
			Density = FinalPostProcessSettings.HGEccentricityRatio * HGDensity;
			NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
			NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].fEccentricity = FinalPostProcessSettings.HGEccentricity2;
			NvVlMediumDesc.uNumPhaseTerms++;
		}

		if (FinalPostProcessSettings.IsotropicTransmittance < 1.0f && FinalPostProcessSettings.IsotropicColor != FLinearColor::Black)
		{
			NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].ePhaseFunc = NvVl::PhaseFunctionType::ISOTROPIC;
			FVector Density = DENSITY(Isotropic);
			NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
			NvVlMediumDesc.uNumPhaseTerms++;
		}
	}

	GNVVolumetricLightingRHI->UpdateRendering(NvVlMediumDesc.uNumPhaseTerms != 0);

	if (GNVVolumetricLightingRHI->IsRendering())
	{
		SCOPED_DRAW_EVENT(RHICmdList, VolumetricLightingBeginAccumulation);
		SCOPED_GPU_STAT(RHICmdList, Stat_GPU_BeginAccumulation);

		FIntPoint BufferSize = SceneContext.GetBufferSizeXY();
		
		FMemory::Memzero(NvVlContextDesc);
		NvVlContextDesc.framebuffer.uWidth = BufferSize.X;
		NvVlContextDesc.framebuffer.uHeight = BufferSize.Y;
		NvVlContextDesc.framebuffer.uSamples = 1;
		NvVlContextDesc.bStereoEnabled = Views.Num() > 1;
#if VRWORKS_SUPPORT
		NvVlContextDesc.bSinglePassStereo = (CVarNvVlSPS.GetValueOnRenderThread() != 0) && View.bAllowSinglePassStereo;
#else
		NvVlContextDesc.bSinglePassStereo = CVarNvVlSPS.GetValueOnRenderThread() != 0;
#endif
		NvVlContextDesc.bReversedZ = (int32)ERHIZBuffer::IsInverted != 0;
		NvVlContextDesc.eDownsampleMode = (NvVl::DownsampleMode)Properties->DownsampleMode.GetValue();
		NvVlContextDesc.eInternalSampleMode = (NvVl::MultisampleMode)Properties->MsaaMode.GetValue();
		NvVlContextDesc.eFilterMode = (NvVl::FilterMode)Properties->FilterMode.GetValue();
#if VRWORKS_SUPPORT
		NvVlContextDesc.eHMDDevice = NvVl::HMDDeviceType::STEAMVR; //TODO - Device check on runtime?
		NvVlContextDesc.eMultiResConfig = (NvVl::VRProjectConfiguration)FMath::Clamp(View.MultiResLevel, 0, (int32)NvVl::VRProjectConfiguration::COUNT-1);
		NvVlContextDesc.eLensMatchedConfig = (NvVl::VRProjectConfiguration)FMath::Clamp(View.LensMatchedShadingLevel, 0, (int32)NvVl::VRProjectConfiguration::COUNT-1);
#endif

		GNVVolumetricLightingRHI->UpdateContext(NvVlContextDesc);

		int32 DebugMode = FMath::Clamp((int32)CVarNvVlDebugMode.GetValueOnRenderThread(), 0, 2);
		RHICmdList.BeginAccumulation(SceneContext.GetSceneDepthTexture(), ViewerDescs, NvVlMediumDesc, (Nv::VolumetricLighting::DebugFlags)DebugMode);
	}
}

void GetLightMatrix(const FWholeSceneProjectedShadowInitializer& Initializer, float& MinSubjectZ, float& MaxSubjectZ, FVector& PreShadowTranslation, FMatrix& SubjectAndReceiverMatrix)
{
	FVector	XAxis, YAxis;
	Initializer.FaceDirection.FindBestAxisVectors(XAxis,YAxis);
	const FMatrix WorldToLightScaled = Initializer.WorldToLight * FScaleMatrix(Initializer.Scales);
	const FMatrix WorldToFace = WorldToLightScaled * FBasisVectorMatrix(-XAxis,YAxis,Initializer.FaceDirection.GetSafeNormal(),FVector::ZeroVector);

	MaxSubjectZ = WorldToFace.TransformPosition(Initializer.SubjectBounds.Origin).Z + Initializer.SubjectBounds.SphereRadius;
	MinSubjectZ = FMath::Max(MaxSubjectZ - Initializer.SubjectBounds.SphereRadius * 2,Initializer.MinLightW);
	PreShadowTranslation = Initializer.PreShadowTranslation;
	SubjectAndReceiverMatrix = WorldToFace * FShadowProjectionMatrix(MinSubjectZ, MaxSubjectZ, Initializer.WAxis);
}

void GetLightDesc(const FLightSceneInfo* LightSceneInfo, const FVector& PreShadowTranslation, const FMatrix& SubjectAndReceiverMatrix, float MinSubjectZ, float MaxSubjectZ)
{
	FMemory::Memzero(NvVlLightDesc);

	FVector LightPosition = LightSceneInfo->Proxy->GetOrigin();
	FVector LightDirection = LightSceneInfo->Proxy->GetDirection();
	LightDirection.Normalize();

	FMatrix LightViewProj;
	if (LightSceneInfo->Proxy->GetLightType() == LightType_Point)
	{
		LightViewProj = FTranslationMatrix(-LightPosition);
	}
	else
	{
		LightViewProj = FTranslationMatrix(PreShadowTranslation) * SubjectAndReceiverMatrix;
	}

    FMatrix LightViewProjInv = LightViewProj.InverseFast();
    NvVlLightDesc.mLightToWorld = *reinterpret_cast<const NvcMat44*>(&LightViewProjInv.M[0][0]);

    FVector Intensity = LightSceneInfo->Proxy->GetNvVlIntensity();
    NvVlLightDesc.vIntensity = *reinterpret_cast<const NvcVec3 *>(&Intensity);

	switch (LightSceneInfo->Proxy->GetLightType())
	{
		case LightType_Point:
		{
			NvVlLightDesc.eType = NvVl::LightType::OMNI;
			NvVlLightDesc.Omni.fZNear = MinSubjectZ;
			NvVlLightDesc.Omni.fZFar = MaxSubjectZ;
			NvVlLightDesc.Omni.vPosition = *reinterpret_cast<const NvcVec3 *>(&LightPosition);
			NvVlLightDesc.Omni.eAttenuationMode = (NvVl::AttenuationMode)LightSceneInfo->Proxy->GetNvVlAttenuationMode();
			const FVector4& AttenuationFactors = LightSceneInfo->Proxy->GetNvVlAttenuationFactors();
			if (NvVlLightDesc.Omni.eAttenuationMode == NvVl::AttenuationMode::INV_POLYNOMIAL)
			{
				float InvRadius = 1.0f / FMath::Max(0.00001f, AttenuationFactors.W);
				NvVlLightDesc.Omni.fAttenuationFactors[0] = 1.0f;
				NvVlLightDesc.Omni.fAttenuationFactors[1] = 2.0f * InvRadius;
				NvVlLightDesc.Omni.fAttenuationFactors[2] = InvRadius * InvRadius;
			}
			else
			{
				NvVlLightDesc.Omni.fAttenuationFactors[0] = AttenuationFactors.X;
				NvVlLightDesc.Omni.fAttenuationFactors[1] = AttenuationFactors.Y;
				NvVlLightDesc.Omni.fAttenuationFactors[2] = AttenuationFactors.Z;
			}
			NvVlLightDesc.Omni.fAttenuationFactors[3] = 0.0f;
		}
		break;
		case LightType_Spot:
		{
			NvVlLightDesc.eType = NvVl::LightType::SPOTLIGHT;
			NvVlLightDesc.Spotlight.fZNear = MinSubjectZ;
			NvVlLightDesc.Spotlight.fZFar = MaxSubjectZ;

			NvVlLightDesc.Spotlight.eFalloffMode = (NvVl::SpotlightFalloffMode)LightSceneInfo->Proxy->GetNvVlFalloffMode();
			const FVector2D& AngleAndPower = LightSceneInfo->Proxy->GetNvVlFalloffAngleAndPower();
			NvVlLightDesc.Spotlight.fFalloff_CosTheta = FMath::Cos(AngleAndPower.X);
			NvVlLightDesc.Spotlight.fFalloff_Power = AngleAndPower.Y;
            
			NvVlLightDesc.Spotlight.vDirection = *reinterpret_cast<const NvcVec3 *>(&LightDirection);
			NvVlLightDesc.Spotlight.vPosition = *reinterpret_cast<const NvcVec3 *>(&LightPosition);
			NvVlLightDesc.Spotlight.eAttenuationMode = (NvVl::AttenuationMode)LightSceneInfo->Proxy->GetNvVlAttenuationMode();
			const FVector4& AttenuationFactors = LightSceneInfo->Proxy->GetNvVlAttenuationFactors();
			if (NvVlLightDesc.Spotlight.eAttenuationMode == NvVl::AttenuationMode::INV_POLYNOMIAL)
			{
				float InvRadius = 1.0f / FMath::Max(0.00001f, AttenuationFactors.W);
				NvVlLightDesc.Spotlight.fAttenuationFactors[0] = 1.0f;
				NvVlLightDesc.Spotlight.fAttenuationFactors[1] = 2.0f * InvRadius;
				NvVlLightDesc.Spotlight.fAttenuationFactors[2] = InvRadius * InvRadius;
			}
			else
			{
				NvVlLightDesc.Spotlight.fAttenuationFactors[0] = AttenuationFactors.X;
				NvVlLightDesc.Spotlight.fAttenuationFactors[1] = AttenuationFactors.Y;
				NvVlLightDesc.Spotlight.fAttenuationFactors[2] = AttenuationFactors.Z;
			}
			NvVlLightDesc.Spotlight.fAttenuationFactors[3] = 0.0f;
		}
		break;
		default:
		case LightType_Directional:
		{
			NvVlLightDesc.eType = NvVl::LightType::DIRECTIONAL;
			NvVlLightDesc.Directional.vDirection = *reinterpret_cast<const NvcVec3 *>(&LightDirection);
		}
	}
}

void GetVolumeDesc(const FSceneRenderTargets& SceneContext, const FLightSceneInfo* LightSceneInfo)
{
	FMemory::Memzero(NvVlVolumeDesc);
	{
		NvVlVolumeDesc.fTargetRayResolution = LightSceneInfo->Proxy->GetNvVlTargetRayResolution();
		NvVlVolumeDesc.uMaxMeshResolution = SceneContext.GetShadowDepthTextureResolution().X;
		NvVlVolumeDesc.fDepthBias = LightSceneInfo->Proxy->GetNvVlDepthBias();
		NvVlVolumeDesc.eTessQuality = (NvVl::TessellationQuality)LightSceneInfo->Proxy->GetNvVlTessQuality();
	}
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo)
{
	if (GNVVolumetricLightingRHI == nullptr || !GNVVolumetricLightingRHI->IsRendering())
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, VolumetricLightingRenderVolume);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_RenderVolume);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	float MinSubjectZ = 0, MaxSubjectZ = 0;
	FVector PreShadowTranslation;
	FMatrix SubjectAndReceiverMatrix;

	if (LightSceneInfo->Proxy->GetLightType() == LightType_Directional)
	{
		FWholeSceneProjectedShadowInitializer ProjectedShadowInitializer;
		if (LightSceneInfo->Proxy->GetViewDependentWholeSceneProjectedShadowInitializer(Views[0], INDEX_NONE, LightSceneInfo->IsPrecomputedLightingValid(), ProjectedShadowInitializer))
		{
			GetLightMatrix(ProjectedShadowInitializer, MinSubjectZ, MaxSubjectZ, PreShadowTranslation, SubjectAndReceiverMatrix);
		}
	}
	else
	{
		TArray<FWholeSceneProjectedShadowInitializer, TInlineAllocator<6> > ProjectedShadowInitializers;
		if (LightSceneInfo->Proxy->GetWholeSceneProjectedShadowInitializer(ViewFamily, ProjectedShadowInitializers))
		{
			GetLightMatrix(ProjectedShadowInitializers[0], MinSubjectZ, MaxSubjectZ, PreShadowTranslation, SubjectAndReceiverMatrix);
		}
	}

	FMemory::Memzero(NvVlShadowMapDesc);
	FIntPoint Resolution = SceneContext.GetShadowDepthTextureResolution();

	NvVlShadowMapDesc.eType = NvVl::ShadowMapLayout::SIMPLE;
	NvVlShadowMapDesc.uWidth = Resolution.X;
	NvVlShadowMapDesc.uHeight = Resolution.Y;
	// Shadow depth type
	NvVlShadowMapDesc.bLinearizedDepth = false;
	// shadow space
	NvVlShadowMapDesc.bShadowSpace = false;

	NvVlShadowMapDesc.uElementCount = 1;
	NvVlShadowMapDesc.Elements[0].uOffsetX = 0;
	NvVlShadowMapDesc.Elements[0].uOffsetY = 0;
	NvVlShadowMapDesc.Elements[0].uWidth = NvVlShadowMapDesc.uWidth;
	NvVlShadowMapDesc.Elements[0].uHeight = NvVlShadowMapDesc.uHeight;
	NvVlShadowMapDesc.Elements[0].mArrayIndex = 0;

	GetLightDesc(LightSceneInfo, PreShadowTranslation, SubjectAndReceiverMatrix, MinSubjectZ, MaxSubjectZ);

	GetVolumeDesc(SceneContext, LightSceneInfo);

	TArray<FTextureRHIParamRef> ShadowDepthTextures;
	RHICmdList.RenderVolume(ShadowDepthTextures, NvVlShadowMapDesc, NvVlLightDesc, NvVlVolumeDesc);
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, const FProjectedShadowInfo* ShadowInfo)
{
	if (GNVVolumetricLightingRHI == nullptr || !GNVVolumetricLightingRHI->IsRendering())
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, VolumetricLightingRenderVolume);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_RenderVolume);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FMemory::Memzero(NvVlShadowMapDesc);
	uint32 ShadowmapWidth = 0, ShadowmapHeight = 0;
	TArray<FTextureRHIParamRef> ShadowDepthTextures;

	if (LightSceneInfo->Proxy->GetLightType() == LightType_Point)
	{
		const FTextureCubeRHIRef& ShadowDepthTexture = ShadowInfo->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture->GetTextureCube();
		if (ShadowDepthTexture)
		{
			ShadowmapWidth = ShadowDepthTexture->GetSize();
			ShadowmapHeight = ShadowDepthTexture->GetSize();
		}

		NvVlShadowMapDesc.eType = NvVl::ShadowMapLayout::CUBE;
		NvVlShadowMapDesc.uWidth = ShadowmapWidth;
		NvVlShadowMapDesc.uHeight = ShadowmapHeight;
		// Shadow depth type
		NvVlShadowMapDesc.bLinearizedDepth = false;
		// shadow space
		NvVlShadowMapDesc.bShadowSpace = false;

		for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
		{
			NvVlShadowMapDesc.mCubeViewProj[FaceIndex] = *reinterpret_cast<const NvcMat44*>(&(ShadowInfo->OnePassShadowViewProjectionMatrices[FaceIndex]).M[0][0]);
		}

		NvVlShadowMapDesc.uElementCount = 1;
		NvVlShadowMapDesc.Elements[0].uOffsetX = 0;
		NvVlShadowMapDesc.Elements[0].uOffsetY = 0;
		NvVlShadowMapDesc.Elements[0].uWidth = NvVlShadowMapDesc.uWidth;
		NvVlShadowMapDesc.Elements[0].uHeight = NvVlShadowMapDesc.uHeight;
		NvVlShadowMapDesc.Elements[0].mArrayIndex = 0;

		ShadowDepthTextures.Add(ShadowDepthTexture);
	}
	else // Spot light
	{
		const FTexture2DRHIRef& ShadowDepthTexture = ShadowInfo->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture->GetTexture2D();

		if (ShadowDepthTexture)
		{
			ShadowmapWidth = ShadowDepthTexture->GetSizeX();
			ShadowmapHeight = ShadowDepthTexture->GetSizeY();
		}

		FVector4 ShadowmapMinMaxValue;
		FMatrix WorldToShadowMatrixValue = ShadowInfo->GetWorldToShadowMatrix(ShadowmapMinMaxValue);

		NvVlShadowMapDesc.eType = NvVl::ShadowMapLayout::SIMPLE;
		NvVlShadowMapDesc.uWidth = ShadowmapWidth;
		NvVlShadowMapDesc.uHeight = ShadowmapHeight;
		// Shadow depth type
		NvVlShadowMapDesc.bLinearizedDepth = true;
		// shadow space
		NvVlShadowMapDesc.bShadowSpace = true;

		NvVlShadowMapDesc.uElementCount = 1;
		NvVlShadowMapDesc.Elements[0].uOffsetX = 0;
		NvVlShadowMapDesc.Elements[0].uOffsetY = 0;
		NvVlShadowMapDesc.Elements[0].uWidth = NvVlShadowMapDesc.uWidth;
		NvVlShadowMapDesc.Elements[0].uHeight = NvVlShadowMapDesc.uHeight;
		NvVlShadowMapDesc.Elements[0].mViewProj = *reinterpret_cast<const NvcMat44*>(&WorldToShadowMatrixValue.M[0][0]);
		NvVlShadowMapDesc.Elements[0].mArrayIndex = 0;
		NvVlShadowMapDesc.Elements[0].fInvMaxSubjectDepth = ShadowInfo->InvMaxSubjectDepth;
		NvVlShadowMapDesc.Elements[0].vShadowmapMinMaxValue = *reinterpret_cast<const NvcVec4 *>(&ShadowmapMinMaxValue);

		ShadowDepthTextures.Add(ShadowDepthTexture);
	}

	GetLightDesc(LightSceneInfo, ShadowInfo->PreShadowTranslation, ShadowInfo->SubjectAndReceiverMatrix, ShadowInfo->MinSubjectZ, ShadowInfo->MaxSubjectZ);

	GetVolumeDesc(SceneContext, LightSceneInfo);

	RHICmdList.RenderVolume(ShadowDepthTextures, NvVlShadowMapDesc, NvVlLightDesc, NvVlVolumeDesc);
}

// for cascaded shadow
void FDeferredShadingSceneRenderer::NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& ShadowInfos)
{
	if (GNVVolumetricLightingRHI == nullptr || !GNVVolumetricLightingRHI->IsRendering())
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, VolumetricLightingRenderVolume);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_RenderVolume);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FMemory::Memzero(NvVlShadowMapDesc);
	TArray<FTextureRHIParamRef> ShadowDepthTextures;
	uint32 ShadowmapWidth = 0, ShadowmapHeight = 0;

	bool bAtlassing = !GRHINeedsUnatlasedCSMDepthsWorkaround;
	const FTexture2DRHIRef& ShadowDepthTexture = ShadowInfos[0]->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture->GetTexture2D();

	if (bAtlassing)
	{
		if (ShadowDepthTexture)
		{
			ShadowmapWidth = ShadowDepthTexture->GetSizeX();
			ShadowmapHeight = ShadowDepthTexture->GetSizeY();
		}
		else
		{
			FIntPoint Resolution = SceneContext.GetShadowDepthTextureResolution();
			ShadowmapWidth = Resolution.X;
			ShadowmapHeight = Resolution.Y;
		}
		NvVlShadowMapDesc.uWidth = ShadowmapWidth;
		NvVlShadowMapDesc.uHeight = ShadowmapHeight;

		ShadowDepthTextures.Add(ShadowDepthTexture);
	}

	NvVlShadowMapDesc.eType = bAtlassing ? NvVl::ShadowMapLayout::CASCADE_ATLAS : NvVl::ShadowMapLayout::CASCADE_MULTI;
	// Shadow depth type
	NvVlShadowMapDesc.bLinearizedDepth = false;
	// shadow space
	NvVlShadowMapDesc.bShadowSpace = true;

	NvVlShadowMapDesc.uElementCount = FMath::Min((uint32)ShadowInfos.Num(), NvVl::MAX_SHADOWMAP_ELEMENTS);

	for (uint32 ElementIndex = 0; ElementIndex < NvVlShadowMapDesc.uElementCount; ElementIndex++)
	{
		FVector4 ShadowmapMinMaxValue;
		uint32 ShadowIndex = ShadowInfos.Num() - NvVlShadowMapDesc.uElementCount + ElementIndex;
		FMatrix WorldToShadowMatrixValue = ShadowInfos[ShadowIndex]->GetWorldToShadowMatrix(ShadowmapMinMaxValue);
		NvVlShadowMapDesc.Elements[ElementIndex].uOffsetX = 0;
		NvVlShadowMapDesc.Elements[ElementIndex].uOffsetY = 0;
		NvVlShadowMapDesc.Elements[ElementIndex].mViewProj = *reinterpret_cast<const NvcMat44*>(&WorldToShadowMatrixValue.M[0][0]);
		NvVlShadowMapDesc.Elements[ElementIndex].mArrayIndex = ElementIndex;
		NvVlShadowMapDesc.Elements[ElementIndex].vShadowmapMinMaxValue = *reinterpret_cast<const NvcVec4 *>(&ShadowmapMinMaxValue);

		if (bAtlassing)
		{
			NvVlShadowMapDesc.Elements[ElementIndex].uWidth = ShadowmapWidth;
			NvVlShadowMapDesc.Elements[ElementIndex].uHeight = ShadowmapHeight;
		}
		else
		{
			const FTexture2DRHIRef& DepthTexture = ShadowInfos[ShadowIndex]->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture->GetTexture2D();

			NvVlShadowMapDesc.Elements[ElementIndex].uWidth = DepthTexture->GetSizeX();
			NvVlShadowMapDesc.Elements[ElementIndex].uHeight = DepthTexture->GetSizeY();
			ShadowDepthTextures.Add(DepthTexture);
		}
	}

	GetLightDesc(LightSceneInfo, ShadowInfos[0]->PreShadowTranslation, ShadowInfos[0]->SubjectAndReceiverMatrix, 0, 0);

	GetVolumeDesc(SceneContext, LightSceneInfo);

	RHICmdList.RenderVolume(ShadowDepthTextures, NvVlShadowMapDesc, NvVlLightDesc, NvVlVolumeDesc);
}


void FDeferredShadingSceneRenderer::NVVolumetricLightingEndAccumulation(FRHICommandListImmediate& RHICmdList)
{
	if (GNVVolumetricLightingRHI == nullptr || !GNVVolumetricLightingRHI->IsRendering())
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, VolumetricLightingEndAccumulation);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_EndAccumulation);
	RHICmdList.EndAccumulation();
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingApplyLighting(FRHICommandListImmediate& RHICmdList)
{
	if (GNVVolumetricLightingRHI == nullptr || !GNVVolumetricLightingRHI->IsRendering())
	{
		return;
	}

	check(Views.Num());
	const FViewInfo& View = Views[0];

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	const FNVVolumetricLightingProperties* Properties = Scene->VolumetricLightingProperties;
	const FFinalPostProcessSettings& FinalPostProcessSettings = View.FinalPostProcessSettings;

	FMemory::Memzero(NvVlPostprocessDesc);
	NvVlPostprocessDesc.bDoFog = (FinalPostProcessSettings.FogMode != EFogMode::FOG_NONE) && CVarNvVlFog.GetValueOnRenderThread();
    NvVlPostprocessDesc.bIgnoreSkyFog = FinalPostProcessSettings.FogMode == EFogMode::FOG_NOSKY;
    NvVlPostprocessDesc.eUpsampleQuality = (NvVl::UpsampleQuality)Properties->UpsampleQuality.GetValue();
    NvVlPostprocessDesc.fBlendfactor = Properties->Blendfactor;
    NvVlPostprocessDesc.fTemporalFactor = Properties->TemporalFactor;
    NvVlPostprocessDesc.fFilterThreshold = Properties->FilterThreshold;
	FMatrix ViewProjNoAAMatrix = View.ViewMatrices.GetViewMatrix() * View.ViewMatrices.ComputeProjectionNoAAMatrix();
	NvVlPostprocessDesc.mUnjitteredViewProj = *reinterpret_cast<const NvcMat44*>(&ViewProjNoAAMatrix.M[0][0]);

	FVector FogLight = FinalPostProcessSettings.FogColor * FinalPostProcessSettings.FogIntensity;
    NvVlPostprocessDesc.vFogLight = *reinterpret_cast<const NvcVec3 *>(&FogLight);
    NvVlPostprocessDesc.fMultiscatter = OPTICAL_DEPTH(Fog);
	NvVlPostprocessDesc.eStereoPass = NvVl::StereoscopicPass::FULL;

	if (!SceneContext.IsSeparateTranslucencyPass())
	{
		SCOPED_DRAW_EVENT(RHICmdList, VolumetricLightingApplyLighting);
		SCOPED_GPU_STAT(RHICmdList, Stat_GPU_ApplyLighting);

		RHICmdList.ApplyLighting(SceneContext.GetSceneColorSurface(), NvVlPostprocessDesc);
	}
	else
	{
		GNVVolumetricLightingRHI->SetSeparateTranslucencyPostprocessDesc(NvVlPostprocessDesc);
	}
}

#endif