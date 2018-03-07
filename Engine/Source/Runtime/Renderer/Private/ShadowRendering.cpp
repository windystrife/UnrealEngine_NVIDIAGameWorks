// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShadowRendering.cpp: Shadow rendering implementation
=============================================================================*/

#include "ShadowRendering.h"
#include "PrimitiveViewRelevance.h"
#include "DepthRendering.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "LightPropagationVolume.h"
#include "ScenePrivate.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
// @third party code - BEGIN HairWorks
#include "HairWorksRenderer.h"
// @third party code - END HairWorks

static TAutoConsoleVariable<float> CVarCSMShadowDepthBias(
	TEXT("r.Shadow.CSMDepthBias"),
	20.0f,
	TEXT("Constant depth bias used by CSM"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarPerObjectDirectionalShadowDepthBias(
	TEXT("r.Shadow.PerObjectDirectionalDepthBias"),
	20.0f,
	TEXT("Constant depth bias used by per-object shadows from directional lights\n")
	TEXT("Lower values give better self-shadowing, but increase self-shadowing artifacts"),
	ECVF_RenderThreadSafe);


static TAutoConsoleVariable<float> CVarCSMSplitPenumbraScale(
	TEXT("r.Shadow.CSMSplitPenumbraScale"),
	0.5f,
	TEXT("Scale applied to the penumbra size of Cascaded Shadow Map splits, useful for minimizing the transition between splits"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarCSMDepthBoundsTest(
	TEXT("r.Shadow.CSMDepthBoundsTest"),
	1,
	TEXT("Whether to use depth bounds tests rather than stencil tests for the CSM bounds"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarSpotLightShadowTransitionScale(
	TEXT("r.Shadow.SpotLightTransitionScale"),
	60.0f,
	TEXT("Transition scale for spotlights"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarShadowTransitionScale(
	TEXT("r.Shadow.TransitionScale"),
	60.0f,
	TEXT("This controls the 'fade in' region between a caster and where his shadow shows up.  Larger values make a smaller region which will have more self shadowing artifacts"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarPointLightShadowDepthBias(
	TEXT("r.Shadow.PointLightDepthBias"),
	0.05f,
	TEXT("Depth bias that is applied in the depth pass for shadows from point lights. (0.03 avoids peter paning but has some shadow acne)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarSpotLightShadowDepthBias(
	TEXT("r.Shadow.SpotLightDepthBias"),
	5.0f,
	TEXT("Depth bias that is applied in the depth pass for per object projected shadows from spot lights"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarEnableModulatedSelfShadow(
	TEXT("r.Shadow.EnableModulatedSelfShadow"),
	0,
	TEXT("Allows modulated shadows to affect the shadow caster. (mobile only)"),
	ECVF_RenderThreadSafe);

static int GStencilOptimization = 1;
static FAutoConsoleVariableRef CVarStencilOptimization(
	TEXT("r.Shadow.StencilOptimization"),
	GStencilOptimization,
	TEXT("Removes stencil clears between shadow projections by zeroing the stencil during testing"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarFilterMethod(
	TEXT("r.Shadow.FilterMethod"),
	0,
	TEXT("Chooses the shadow filtering method.\n")
	TEXT(" 0: Uniform PCF (default)\n")
	TEXT(" 1: PCSS (experimental)\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMaxSoftKernelSize(
	TEXT("r.Shadow.MaxSoftKernelSize"),
	40,
	TEXT("Mazimum size of the softening kernels in pixels."),
	ECVF_RenderThreadSafe);

DECLARE_FLOAT_COUNTER_STAT(TEXT("ShadowProjection"), Stat_GPU_ShadowProjection, STATGROUP_GPU);

// 0:off, 1:low, 2:med, 3:high, 4:very high, 5:max
uint32 GetShadowQuality()
{
	static const auto ICVarQuality = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ShadowQuality"));

	int Ret = ICVarQuality->GetValueOnRenderThread();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static const auto ICVarLimit = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.LimitRenderingFeatures"));
	if(ICVarLimit)
	{
		int32 Limit = ICVarLimit->GetValueOnRenderThread();

		if(Limit > 2)
		{
			Ret = 0;
		}
	}
#endif

	return FMath::Clamp(Ret, 0, 5);
}

float GetLightFadeFactor(const FSceneView& View, const FLightSceneProxy* Proxy)
{
	// Distance fade
	FSphere Bounds = Proxy->GetBoundingSphere();

	const float DistanceSquared = (Bounds.Center - View.ViewMatrices.GetViewOrigin()).SizeSquared();
	extern float GMinScreenRadiusForLights;
	float SizeFade = FMath::Square(FMath::Min(0.0002f, GMinScreenRadiusForLights / Bounds.W) * View.LODDistanceFactor) * DistanceSquared;
	SizeFade = FMath::Clamp(6.0f - 6.0f * SizeFade, 0.0f, 1.0f);

	extern float GLightMaxDrawDistanceScale;
	float MaxDist = Proxy->GetMaxDrawDistance() * GLightMaxDrawDistanceScale;
	float Range = Proxy->GetFadeRange();
	float DistanceFade = MaxDist ? (MaxDist - FMath::Sqrt(DistanceSquared)) / Range : 1.0f;
	DistanceFade = FMath::Clamp(DistanceFade, 0.0f, 1.0f);
	return SizeFade * DistanceFade;
}

/** The stencil sphere vertex buffer. */
TGlobalResource<StencilingGeometry::TStencilSphereVertexBuffer<18, 12, FVector4> > StencilingGeometry::GStencilSphereVertexBuffer;
TGlobalResource<StencilingGeometry::TStencilSphereVertexBuffer<18, 12, FVector> > StencilingGeometry::GStencilSphereVectorBuffer;

/** The stencil sphere index buffer. */
TGlobalResource<StencilingGeometry::TStencilSphereIndexBuffer<18, 12> > StencilingGeometry::GStencilSphereIndexBuffer;

TGlobalResource<StencilingGeometry::TStencilSphereVertexBuffer<4, 4, FVector4> > StencilingGeometry::GLowPolyStencilSphereVertexBuffer;
TGlobalResource<StencilingGeometry::TStencilSphereIndexBuffer<4, 4> > StencilingGeometry::GLowPolyStencilSphereIndexBuffer;

/** The (dummy) stencil cone vertex buffer. */
TGlobalResource<StencilingGeometry::FStencilConeVertexBuffer> StencilingGeometry::GStencilConeVertexBuffer;

/** The stencil cone index buffer. */
TGlobalResource<StencilingGeometry::FStencilConeIndexBuffer> StencilingGeometry::GStencilConeIndexBuffer;

/*-----------------------------------------------------------------------------
	FShadowVolumeBoundProjectionVS
-----------------------------------------------------------------------------*/

bool FShadowVolumeBoundProjectionVS::ShouldCache(EShaderPlatform Platform)
{
	return true;
}

void FShadowVolumeBoundProjectionVS::SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo)
{
	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(),View.ViewUniformBuffer);
	
	if(ShadowInfo->IsWholeSceneDirectionalShadow())
	{
		// Calculate bounding geometry transform for whole scene directional shadow.
		// Use a pair of pre-transformed planes for stenciling.
		StencilingGeometryParameters.Set(RHICmdList, this, FVector4(0,0,0,1));
	}
	else if(ShadowInfo->IsWholeScenePointLightShadow())
	{
		// Handle stenciling sphere for point light.
		StencilingGeometryParameters.Set(RHICmdList, this, View, ShadowInfo->LightSceneInfo);
	}
	else
	{
		// Other bounding geometry types are pre-transformed.
		StencilingGeometryParameters.Set(RHICmdList, this, FVector4(0,0,0,1));
	}
}

IMPLEMENT_SHADER_TYPE(,FShadowProjectionNoTransformVS,TEXT("/Engine/Private/ShadowProjectionVertexShader.usf"),TEXT("Main"),SF_Vertex);

IMPLEMENT_SHADER_TYPE(,FShadowVolumeBoundProjectionVS,TEXT("/Engine/Private/ShadowProjectionVertexShader.usf"),TEXT("Main"),SF_Vertex);

/**
 * Implementations for TShadowProjectionPS.  
 */
#if !UE_BUILD_DOCS
#define IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(Quality,UseFadePlane) \
	typedef TShadowProjectionPS<Quality, UseFadePlane> FShadowProjectionPS##Quality##UseFadePlane; \
	IMPLEMENT_SHADER_TYPE(template<>,FShadowProjectionPS##Quality##UseFadePlane,TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"),TEXT("Main"),SF_Pixel);

// Projection shaders without the distance fade, with different quality levels.
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(1,false);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(2,false);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(3,false);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(4,false);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(5,false);

// Projection shaders with the distance fade, with different quality levels.
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(1,true);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(2,true);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(3,true);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(4,true);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(5,true);

#undef IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER
#endif

// Implement a pixel shader for rendering modulated shadow projections.
IMPLEMENT_SHADER_TYPE(template<>, TModulatedShadowProjection<1>, TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"), TEXT("Main"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TModulatedShadowProjection<2>, TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"), TEXT("Main"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TModulatedShadowProjection<3>, TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"), TEXT("Main"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TModulatedShadowProjection<4>, TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"), TEXT("Main"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TModulatedShadowProjection<5>, TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"), TEXT("Main"), SF_Pixel);

// with different quality levels
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionFromTranslucencyPS<1>,TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionFromTranslucencyPS<2>,TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionFromTranslucencyPS<3>,TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionFromTranslucencyPS<4>,TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionFromTranslucencyPS<5>,TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"),TEXT("Main"),SF_Pixel);

// Implement a pixel shader for rendering one pass point light shadows with different quality levels
IMPLEMENT_SHADER_TYPE(template<>,TOnePassPointShadowProjectionPS<1>,TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"),TEXT("MainOnePassPointLightPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOnePassPointShadowProjectionPS<2>,TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"),TEXT("MainOnePassPointLightPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOnePassPointShadowProjectionPS<3>,TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"),TEXT("MainOnePassPointLightPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOnePassPointShadowProjectionPS<4>,TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"),TEXT("MainOnePassPointLightPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TOnePassPointShadowProjectionPS<5>, TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"), TEXT("MainOnePassPointLightPS"), SF_Pixel);

// Implements a pixel shader for directional light PCSS.
#define IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(Quality,UseFadePlane) \
	typedef TDirectionalPercentageCloserShadowProjectionPS<Quality, UseFadePlane> TDirectionalPercentageCloserShadowProjectionPS##Quality##UseFadePlane; \
	IMPLEMENT_SHADER_TYPE(template<>,TDirectionalPercentageCloserShadowProjectionPS##Quality##UseFadePlane,TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(5,false);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(5,true);
#undef IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER

// Implements a pixel shader for spot light PCSS.
#define IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(Quality,UseFadePlane) \
	typedef TSpotPercentageCloserShadowProjectionPS<Quality, UseFadePlane> TSpotPercentageCloserShadowProjectionPS##Quality##UseFadePlane; \
	IMPLEMENT_SHADER_TYPE(template<>,TSpotPercentageCloserShadowProjectionPS##Quality##UseFadePlane,TEXT("/Engine/Private/ShadowProjectionPixelShader.usf"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(5, false);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(5, true);
#undef IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER

void StencilingGeometry::DrawSphere(FRHICommandList& RHICmdList)
{
	RHICmdList.SetStreamSource(0, StencilingGeometry::GStencilSphereVertexBuffer.VertexBufferRHI, 0);
	RHICmdList.DrawIndexedPrimitive(StencilingGeometry::GStencilSphereIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0,
		StencilingGeometry::GStencilSphereVertexBuffer.GetVertexCount(), 0, 
		StencilingGeometry::GStencilSphereIndexBuffer.GetIndexCount() / 3, 1);
}
		
void StencilingGeometry::DrawVectorSphere(FRHICommandList& RHICmdList)
{
	RHICmdList.SetStreamSource(0, StencilingGeometry::GStencilSphereVectorBuffer.VertexBufferRHI, 0);
	RHICmdList.DrawIndexedPrimitive(StencilingGeometry::GStencilSphereIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0,
									StencilingGeometry::GStencilSphereVectorBuffer.GetVertexCount(), 0,
									StencilingGeometry::GStencilSphereIndexBuffer.GetIndexCount() / 3, 1);
}

void StencilingGeometry::DrawCone(FRHICommandList& RHICmdList)
{
	// No Stream Source needed since it will generate vertices on the fly
	RHICmdList.SetStreamSource(0, StencilingGeometry::GStencilConeVertexBuffer.VertexBufferRHI, 0);

	RHICmdList.DrawIndexedPrimitive(StencilingGeometry::GStencilConeIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0,
		FStencilConeIndexBuffer::NumVerts, 0, StencilingGeometry::GStencilConeIndexBuffer.GetIndexCount() / 3, 1);
}

static void GetShadowProjectionShaders(
	int32 Quality, const FViewInfo& View, const FProjectedShadowInfo* ShadowInfo, bool bMobileModulatedProjections,
	FShadowProjectionVertexShaderInterface** OutShadowProjVS, FShadowProjectionPixelShaderInterface** OutShadowProjPS)
{
	check(!*OutShadowProjVS);
	check(!*OutShadowProjPS);

	if (ShadowInfo->bTranslucentShadow)
	{
		*OutShadowProjVS = View.ShaderMap->GetShader<FShadowVolumeBoundProjectionVS>();

		switch (Quality)
		{
		case 1: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionFromTranslucencyPS<1> >(); break;
		case 2: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionFromTranslucencyPS<2> >(); break;
		case 3: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionFromTranslucencyPS<3> >(); break;
		case 4: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionFromTranslucencyPS<4> >(); break;
		case 5: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionFromTranslucencyPS<5> >(); break;
		default:
			check(0);
	}
	}
	else if (ShadowInfo->IsWholeSceneDirectionalShadow())
	{
		*OutShadowProjVS = View.ShaderMap->GetShader<FShadowProjectionNoTransformVS>();

		if (CVarFilterMethod.GetValueOnRenderThread() == 1)
		{
		if (ShadowInfo->CascadeSettings.FadePlaneLength > 0)
				*OutShadowProjPS = View.ShaderMap->GetShader<TDirectionalPercentageCloserShadowProjectionPS<5, true> >();
			else
				*OutShadowProjPS = View.ShaderMap->GetShader<TDirectionalPercentageCloserShadowProjectionPS<5, false> >();
		}
		else if (ShadowInfo->CascadeSettings.FadePlaneLength > 0)
		{
			switch (Quality)
			{
			case 1: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<1, true> >(); break;
			case 2: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<2, true> >(); break;
			case 3: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<3, true> >(); break;
			case 4: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<4, true> >(); break;
			case 5: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<5, true> >(); break;
			default:
				check(0);
		}
		}
		else
		{
			switch (Quality)
			{
			case 1: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<1, false> >(); break;
			case 2: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<2, false> >(); break;
			case 3: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<3, false> >(); break;
			case 4: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<4, false> >(); break;
			case 5: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<5, false> >(); break;
			default:
				check(0);
		}
	}
	}
	else
	{
		*OutShadowProjVS = View.ShaderMap->GetShader<FShadowVolumeBoundProjectionVS>();

		if(bMobileModulatedProjections)
		{
			switch (Quality)
			{
			case 1: *OutShadowProjPS = View.ShaderMap->GetShader<TModulatedShadowProjection<1> >(); break;
			case 2: *OutShadowProjPS = View.ShaderMap->GetShader<TModulatedShadowProjection<2> >(); break;
			case 3: *OutShadowProjPS = View.ShaderMap->GetShader<TModulatedShadowProjection<3> >(); break;
			case 4: *OutShadowProjPS = View.ShaderMap->GetShader<TModulatedShadowProjection<4> >(); break;
			case 5: *OutShadowProjPS = View.ShaderMap->GetShader<TModulatedShadowProjection<5> >(); break;
			default:
				check(0);
		}
		}
		else
		{
			if (CVarFilterMethod.GetValueOnRenderThread() == 1 && ShadowInfo->GetLightSceneInfo().Proxy->GetLightType() == LightType_Spot)
			{
				*OutShadowProjPS = View.ShaderMap->GetShader<TSpotPercentageCloserShadowProjectionPS<5, false> >();
			}
			else
			{
			switch (Quality)
			{
			case 1: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<1, false> >(); break;
			case 2: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<2, false> >(); break;
			case 3: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<3, false> >(); break;
			case 4: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<4, false> >(); break;
			case 5: *OutShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<5, false> >(); break;
			default:
				check(0);
				}
		}
	}
	}

	check(*OutShadowProjVS);
	check(*OutShadowProjPS);
}

void FProjectedShadowInfo::SetBlendStateForProjection(
	FGraphicsPipelineStateInitializer& GraphicsPSOInit,
	int32 ShadowMapChannel, 
	bool bIsWholeSceneDirectionalShadow,
	bool bUseFadePlane,
	bool bProjectingForForwardShading, 
	bool bMobileModulatedProjections)
{
	// With forward shading we are packing shadowing for all 4 possible stationary lights affecting each pixel into channels of the same texture, based on assigned shadowmap channels.
	// With deferred shading we have 4 channels for each light.  
	//	* CSM and per-object shadows are kept in separate channels to allow fading CSM out to precomputed shadowing while keeping per-object shadows past the fade distance.
	//	* Subsurface shadowing requires an extra channel for each

	if (bProjectingForForwardShading)
	{
		FBlendStateRHIParamRef BlendState = NULL;

		if (bUseFadePlane)
		{
			if (ShadowMapChannel == 0)
			{
				// alpha is used to fade between cascades
				BlendState = TStaticBlendState<CW_RED, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
			}
			else if (ShadowMapChannel == 1)
			{
				BlendState = TStaticBlendState<CW_GREEN, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
			}
			else if (ShadowMapChannel == 2)
			{
				BlendState = TStaticBlendState<CW_BLUE, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
			}
			else if (ShadowMapChannel == 3)
			{
				BlendState = TStaticBlendState<CW_ALPHA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
			}
		}
		else
		{
			if (ShadowMapChannel == 0)
			{
				BlendState = TStaticBlendState<CW_RED, BO_Min, BF_One, BF_One, BO_Min, BF_One, BF_One>::GetRHI();
			}
			else if (ShadowMapChannel == 1)
			{
				BlendState = TStaticBlendState<CW_GREEN, BO_Min, BF_One, BF_One, BO_Min, BF_One, BF_One>::GetRHI();
			}
			else if (ShadowMapChannel == 2)
			{
				BlendState = TStaticBlendState<CW_BLUE, BO_Min, BF_One, BF_One, BO_Min, BF_One, BF_One>::GetRHI();
			}
			else if (ShadowMapChannel == 3)
			{
				BlendState = TStaticBlendState<CW_ALPHA, BO_Min, BF_One, BF_One, BO_Min, BF_One, BF_One>::GetRHI();
			}
		}

		checkf(BlendState, TEXT("Only shadows whose stationary lights have a valid ShadowMapChannel can be projected with forward shading"));
		GraphicsPSOInit.BlendState = BlendState;
	}
	else
	{
		// Light Attenuation channel assignment:
		//  R:     WholeSceneShadows, non SSS
		//  G:     WholeSceneShadows,     SSS
		//  B: non WholeSceneShadows, non SSS
		//  A: non WholeSceneShadows,     SSS
		//
		// SSS: SubsurfaceScattering materials
		// non SSS: shadow for opaque materials
		// WholeSceneShadows: directional light CSM
		// non WholeSceneShadows: spotlight, per object shadows, translucency lighting, omni-directional lights

		if (bIsWholeSceneDirectionalShadow)
		{
			// Note: blend logic has to match ordering in FCompareFProjectedShadowInfoBySplitIndex.  For example the fade plane blend mode requires that shadow to be rendered first.
			// use R and G in Light Attenuation
			if (bUseFadePlane)
			{
				// alpha is used to fade between cascades, we don't don't need to do BO_Min as we leave B and A untouched which has translucency shadow
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RG, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
			}
			else
			{
				// first cascade rendered doesn't require fading (CO_Min is needed to combine multiple shadow passes)
				// RTDF shadows: CO_Min is needed to combine with far shadows which overlap the same depth range
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RG, BO_Min, BF_One, BF_One>::GetRHI();
			}
		}
		else
		{
			if (bMobileModulatedProjections)
			{
				bool bEncodedHDR = GetMobileHDRMode() == EMobileHDRMode::EnabledRGBE;
				if (bEncodedHDR)
				{
					GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
				}
				else
				{
					// Color modulate shadows, ignore alpha.
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_Zero, BF_SourceColor, BO_Add, BF_Zero, BF_One>::GetRHI();
				}
			}
			else
			{
				// use B and A in Light Attenuation
				// CO_Min is needed to combine multiple shadow passes
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_BA, BO_Min, BF_One, BF_One, BO_Min, BF_One, BF_One>::GetRHI();
			}
		}
	}
}

void FProjectedShadowInfo::SetBlendStateForProjection(FGraphicsPipelineStateInitializer& GraphicsPSOInit, bool bProjectingForForwardShading, bool bMobileModulatedProjections) const
{
	SetBlendStateForProjection(
		GraphicsPSOInit,
		GetLightSceneInfo().GetDynamicShadowMapChannel(), 
		IsWholeSceneDirectionalShadow(),
		CascadeSettings.FadePlaneLength > 0 && !bRayTracedDistanceField,
		bProjectingForForwardShading, 
		bMobileModulatedProjections);
}

void FProjectedShadowInfo::SetupFrustumForProjection(const FViewInfo* View, TArray<FVector4, TInlineAllocator<8>>& OutFrustumVertices, bool& bOutCameraInsideShadowFrustum) const
{
	bOutCameraInsideShadowFrustum = true;

	// Calculate whether the camera is inside the shadow frustum, or the near plane is potentially intersecting the frustum.
	if (!IsWholeSceneDirectionalShadow())
	{
		OutFrustumVertices.AddUninitialized(8);

		// The shadow transforms and view transforms are relative to different origins, so the world coordinates need to be translated.
		const FVector PreShadowToPreViewTranslation(View->ViewMatrices.GetPreViewTranslation() - PreShadowTranslation);

		// fill out the frustum vertices (this is only needed in the non-whole scene case)
		for(uint32 vZ = 0;vZ < 2;vZ++)
		{
			for(uint32 vY = 0;vY < 2;vY++)
			{
				for(uint32 vX = 0;vX < 2;vX++)
				{
					const FVector4 UnprojectedVertex = InvReceiverMatrix.TransformFVector4(
						FVector4(
							(vX ? -1.0f : 1.0f),
							(vY ? -1.0f : 1.0f),
							(vZ ?  0.0f : 1.0f),
							1.0f
							)
						);
					const FVector ProjectedVertex = UnprojectedVertex / UnprojectedVertex.W + PreShadowToPreViewTranslation;
					OutFrustumVertices[GetCubeVertexIndex(vX,vY,vZ)] = FVector4(ProjectedVertex, 0);
				}
			}
		}

		const FVector ShadowViewOrigin = View->ViewMatrices.GetViewOrigin();
		const FVector ShadowPreViewTranslation = View->ViewMatrices.GetPreViewTranslation();

		const FVector FrontTopRight		= OutFrustumVertices[GetCubeVertexIndex(0,0,1)] - ShadowPreViewTranslation;
		const FVector FrontTopLeft		= OutFrustumVertices[GetCubeVertexIndex(1,0,1)] - ShadowPreViewTranslation;
		const FVector FrontBottomLeft	= OutFrustumVertices[GetCubeVertexIndex(1,1,1)] - ShadowPreViewTranslation;
		const FVector FrontBottomRight	= OutFrustumVertices[GetCubeVertexIndex(0,1,1)] - ShadowPreViewTranslation;
		const FVector BackTopRight		= OutFrustumVertices[GetCubeVertexIndex(0,0,0)] - ShadowPreViewTranslation;
		const FVector BackTopLeft		= OutFrustumVertices[GetCubeVertexIndex(1,0,0)] - ShadowPreViewTranslation;
		const FVector BackBottomLeft	= OutFrustumVertices[GetCubeVertexIndex(1,1,0)] - ShadowPreViewTranslation;
		const FVector BackBottomRight	= OutFrustumVertices[GetCubeVertexIndex(0,1,0)] - ShadowPreViewTranslation;

		const FPlane Front(FrontTopRight, FrontTopLeft, FrontBottomLeft);
		const float FrontDistance = Front.PlaneDot(ShadowViewOrigin);

        const FPlane Right(BackBottomRight, BackTopRight, FrontTopRight);
		const float RightDistance = Right.PlaneDot(ShadowViewOrigin);

		const FPlane Back(BackTopLeft, BackTopRight, BackBottomRight);
		const float BackDistance = Back.PlaneDot(ShadowViewOrigin);

		const FPlane Left(FrontTopLeft, BackTopLeft, BackBottomLeft);
		const float LeftDistance = Left.PlaneDot(ShadowViewOrigin);

		const FPlane Top(BackTopRight, BackTopLeft, FrontTopLeft);
		const float TopDistance = Top.PlaneDot(ShadowViewOrigin);

		const FPlane Bottom(FrontBottomRight, FrontBottomLeft, BackBottomLeft);
		const float BottomDistance = Bottom.PlaneDot(ShadowViewOrigin);

		// Use a distance threshold to treat the case where the near plane is intersecting the frustum as the camera being inside
		// The near plane handling is not exact since it just needs to be conservative about saying the camera is outside the frustum
		const float DistanceThreshold = -View->NearClippingDistance * 3.0f;

		bOutCameraInsideShadowFrustum = 
			FrontDistance > DistanceThreshold && 
			RightDistance > DistanceThreshold && 
			BackDistance > DistanceThreshold && 
			LeftDistance > DistanceThreshold && 
			TopDistance > DistanceThreshold && 
			BottomDistance > DistanceThreshold;
	}
}

void FProjectedShadowInfo::SetupProjectionStencilMask(
	FRHICommandListImmediate& RHICmdList, 
	const FViewInfo* View, 
	const TArray<FVector4, TInlineAllocator<8>>& FrustumVertices,
	bool bMobileModulatedProjections,
	bool bCameraInsideShadowFrustum
	// @third party code - BEGIN HairWorks
	, bool bHairPass
	// @third party code - END HairWorks
) const
{
	FDrawingPolicyRenderState DrawRenderState(*View);

	// Depth test wo/ writes, no color writing.
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	DrawRenderState.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());

	// If this is a preshadow, mask the projection by the receiver primitives.
	// @third party code - BEGIN HairWorks
	if((bPreShadow || bSelfShadowOnly) && !bHairPass)	// For hairs, we use the same method of dynamic shadow to handle pre-shadow.
	// @third party code - END HairWorks
	{
		SCOPED_DRAW_EVENTF(RHICmdList, EventMaskSubjects, TEXT("Stencil Mask Subjects"));

		// If instanced stereo is enabled, we need to render each view of the stereo pair using the instanced stereo transform to avoid bias issues.
		const bool bIsInstancedStereoEmulated = View->bIsInstancedStereoEnabled && !View->bIsMultiViewEnabled && View->StereoPass != eSSP_FULL;
		if (bIsInstancedStereoEmulated)
		{
			RHICmdList.SetViewport(0, 0, 0, View->Family->InstancedStereoWidth, View->ViewRect.Max.Y, 1);
		}

		// Set stencil to one.
		DrawRenderState.SetDepthStencilState(
			TStaticDepthStencilState<
			false, CF_DepthNearOrEqual,
			true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
			false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
			0xff, 0xff
			>::GetRHI());
		DrawRenderState.SetStencilRef(1);

		// Pre-shadows mask by receiver elements, self-shadow mask by subject elements.
		// Note that self-shadow pre-shadows still mask by receiver elements.
		const TArray<FMeshBatchAndRelevance, SceneRenderingAllocator>& DynamicMeshElements = bPreShadow ? DynamicReceiverMeshElements : DynamicSubjectMeshElements;

		FDepthDrawingPolicyFactory::ContextType Context(DDM_AllOccluders, false);

#if WITH_FLEX
		bool bFlexDepthMasking = GFlexFluidSurfaceRenderer.IsDepthMaskingRequired(ParentSceneInfo->Proxy);

		if (!bFlexDepthMasking)
		{
			for (int32 MeshBatchIndex = 0; MeshBatchIndex < DynamicMeshElements.Num(); MeshBatchIndex++)
			{
				const FMeshBatchAndRelevance& MeshBatchAndRelevance = DynamicMeshElements[MeshBatchIndex];
				const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
				FDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, *View, Context, MeshBatch, false, DrawRenderState, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId, false, bIsInstancedStereoEmulated);
			}
		}
		else
		{
			GFlexFluidSurfaceRenderer.RenderDepth(RHICmdList, ParentSceneInfo->Proxy, *View);
		}
#else
		for (int32 MeshBatchIndex = 0; MeshBatchIndex < DynamicMeshElements.Num(); MeshBatchIndex++)
		{
			const FMeshBatchAndRelevance& MeshBatchAndRelevance = DynamicMeshElements[MeshBatchIndex];
			const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;

			FDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, *View, Context, MeshBatch, true, DrawRenderState, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId, false, bIsInstancedStereoEmulated);
		}
#endif

		// Pre-shadows mask by receiver elements, self-shadow mask by subject elements.
		// Note that self-shadow pre-shadows still mask by receiver elements.
		const PrimitiveArrayType& MaskPrimitives = bPreShadow ? ReceiverPrimitives : DynamicSubjectPrimitives;

		for (int32 PrimitiveIndex = 0, PrimitiveCount = MaskPrimitives.Num(); PrimitiveIndex < PrimitiveCount; PrimitiveIndex++)
		{
			const FPrimitiveSceneInfo* ReceiverPrimitiveSceneInfo = MaskPrimitives[PrimitiveIndex];

			if (View->PrimitiveVisibilityMap[ReceiverPrimitiveSceneInfo->GetIndex()])
			{
				const FPrimitiveViewRelevance& ViewRelevance = View->PrimitiveViewRelevanceMap[ReceiverPrimitiveSceneInfo->GetIndex()];

				if (ViewRelevance.bRenderInMainPass && ViewRelevance.bStaticRelevance)
				{
					for (int32 StaticMeshIdx = 0; StaticMeshIdx < ReceiverPrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++)
					{
						const FStaticMesh& StaticMesh = ReceiverPrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];

						if (View->StaticMeshVisibilityMap[StaticMesh.Id])
						{
							FDepthDrawingPolicyFactory::DrawStaticMesh(
								RHICmdList, 
								*View,
								FDepthDrawingPolicyFactory::ContextType(DDM_AllOccluders, false),
								StaticMesh,
								StaticMesh.bRequiresPerElementVisibility ? View->StaticMeshBatchVisibility[StaticMesh.BatchVisibilityId] : ((1ull << StaticMesh.Elements.Num() )- 1),
								true,
								DrawRenderState,
								ReceiverPrimitiveSceneInfo->Proxy,
								StaticMesh.BatchHitProxyId, 
								false, 
								bIsInstancedStereoEmulated
								);
						}
					}
				}
			}
		}

		if (bSelfShadowOnly && !bPreShadow)
		{
			for (int32 ElementIndex = 0; ElementIndex < StaticSubjectMeshElements.Num(); ++ElementIndex)
			{
				const FStaticMesh& StaticMesh = *StaticSubjectMeshElements[ElementIndex].Mesh;
				FDepthDrawingPolicyFactory::DrawStaticMesh(
					RHICmdList, 
					*View,
					FDepthDrawingPolicyFactory::ContextType(DDM_AllOccluders, false),
					StaticMesh,
					StaticMesh.bRequiresPerElementVisibility ? View->StaticMeshBatchVisibility[StaticMesh.BatchVisibilityId] : ((1ull << StaticMesh.Elements.Num() )- 1),
					true,
					DrawRenderState,
					StaticMesh.PrimitiveSceneInfo->Proxy,
					StaticMesh.BatchHitProxyId, 
					false, 
					bIsInstancedStereoEmulated
					);
			}
		}

		// Restore viewport
		if (bIsInstancedStereoEmulated)
		{
			RHICmdList.SetViewport(View->ViewRect.Min.X, View->ViewRect.Min.Y, 0.0f, View->ViewRect.Max.X, View->ViewRect.Max.Y, 1.0f);
		}
		
	}
	else if (IsWholeSceneDirectionalShadow())
	{
		// Increment stencil on front-facing zfail, decrement on back-facing zfail.
		DrawRenderState.SetDepthStencilState(
			TStaticDepthStencilState<
			false, CF_DepthNearOrEqual,
			true, CF_Always, SO_Keep, SO_Increment, SO_Keep,
			true, CF_Always, SO_Keep, SO_Decrement, SO_Keep,
			0xff, 0xff
			>::GetRHI());

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		DrawRenderState.ApplyToPSO(GraphicsPSOInit);

		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();

		checkSlow(CascadeSettings.ShadowSplitIndex >= 0);
		checkSlow(bDirectionalLight);

		// Draw 2 fullscreen planes, front facing one at the near subfrustum plane, and back facing one at the far.

		// Find the projection shaders.
		TShaderMapRef<FShadowProjectionNoTransformVS> VertexShaderNoTransform(View->ShaderMap);
		VertexShaderNoTransform->SetParameters(RHICmdList, View->ViewUniformBuffer);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShaderNoTransform);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		FVector4 Near = View->ViewMatrices.GetProjectionMatrix().TransformFVector4(FVector4(0, 0, CascadeSettings.SplitNear));
		FVector4 Far = View->ViewMatrices.GetProjectionMatrix().TransformFVector4(FVector4(0, 0, CascadeSettings.SplitFar));
		float StencilNear = Near.Z / Near.W;
		float StencilFar = Far.Z / Far.W;

		FVector4 Verts[] =
		{
			// Far Plane
			FVector4( 1,  1,  StencilFar),
			FVector4(-1,  1,  StencilFar),
			FVector4( 1, -1,  StencilFar),
			FVector4( 1, -1,  StencilFar),
			FVector4(-1,  1,  StencilFar),
			FVector4(-1, -1,  StencilFar),

			// Near Plane
			FVector4(-1,  1, StencilNear),
			FVector4( 1,  1, StencilNear),
			FVector4(-1, -1, StencilNear),
			FVector4(-1, -1, StencilNear),
			FVector4( 1,  1, StencilNear),
			FVector4( 1, -1, StencilNear),
		};

		// Only draw the near plane if this is not the nearest split
		DrawPrimitiveUP(RHICmdList, PT_TriangleList, (CascadeSettings.ShadowSplitIndex > 0) ? 4 : 2, Verts, sizeof(FVector4));
	}
	// Not a preshadow, mask the projection to any pixels inside the frustum.
	else
	{
		if (bCameraInsideShadowFrustum)
		{
			// Use zfail stenciling when the camera is inside the frustum or the near plane is potentially clipping, 
			// Because zfail handles these cases while zpass does not.
			// zfail stenciling is somewhat slower than zpass because on modern GPUs HiZ will be disabled when setting up stencil.
			// Increment stencil on front-facing zfail, decrement on back-facing zfail.
			DrawRenderState.SetDepthStencilState(
				TStaticDepthStencilState<
				false, CF_DepthNearOrEqual,
				true, CF_Always, SO_Keep, SO_Increment, SO_Keep,
				true, CF_Always, SO_Keep, SO_Decrement, SO_Keep,
				0xff, 0xff
				>::GetRHI());
		}
		else
		{
			// Increment stencil on front-facing zpass, decrement on back-facing zpass.
			// HiZ will be enabled on modern GPUs which will save a little GPU time.
			DrawRenderState.SetDepthStencilState(
				TStaticDepthStencilState<
				false, CF_DepthNearOrEqual,
				true, CF_Always, SO_Keep, SO_Keep, SO_Increment,
				true, CF_Always, SO_Keep, SO_Keep, SO_Decrement,
				0xff, 0xff
				>::GetRHI());
		}
		
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		DrawRenderState.ApplyToPSO(GraphicsPSOInit);
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();

		// Find the projection shaders.
		TShaderMapRef<FShadowVolumeBoundProjectionVS> VertexShader(View->ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		// Set the projection vertex shader parameters
		VertexShader->SetParameters(RHICmdList, *View, this);

		// Draw the frustum using the stencil buffer to mask just the pixels which are inside the shadow frustum.
		DrawIndexedPrimitiveUP(RHICmdList, PT_TriangleList, 0, 8, 12, GCubeIndices, sizeof(uint16), FrustumVertices.GetData(), sizeof(FVector4));

		// if rendering modulated shadows mask out subject mesh elements to prevent self shadowing.
		if (bMobileModulatedProjections && !CVarEnableModulatedSelfShadow.GetValueOnRenderThread())
		{
			DrawRenderState.SetDepthStencilState(
				TStaticDepthStencilState<
				false, CF_DepthNearOrEqual,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				0xff, 0xff
				>::GetRHI());
			DrawRenderState.SetStencilRef(0);

			FDepthDrawingPolicyFactory::ContextType Context(DDM_AllOccluders, false);
			for (int32 MeshBatchIndex = 0; MeshBatchIndex < DynamicSubjectMeshElements.Num(); MeshBatchIndex++)
			{
				const FMeshBatchAndRelevance& MeshBatchAndRelevance = DynamicSubjectMeshElements[MeshBatchIndex];
#if WITH_FLEX
		if (!MeshBatchAndRelevance.PrimitiveSceneProxy->IsFlexFluidSurface())
		{
				const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
				FDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, *View, Context, MeshBatch, true, DrawRenderState, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
			}

			for (int32 ElementIndex = 0; ElementIndex < StaticSubjectMeshElements.Num(); ++ElementIndex)
			{
				const FStaticMesh& StaticMesh = *StaticSubjectMeshElements[ElementIndex].Mesh;
				FDepthDrawingPolicyFactory::DrawStaticMesh(
					RHICmdList,
					*View,
					FDepthDrawingPolicyFactory::ContextType(DDM_AllOccluders, false),
					StaticMesh,
					StaticMesh.bRequiresPerElementVisibility ? View->StaticMeshBatchVisibility[StaticMesh.Id] : ((1ull << StaticMesh.Elements.Num()) - 1),
					true,
					DrawRenderState,
					StaticMesh.PrimitiveSceneInfo->Proxy,
					StaticMesh.BatchHitProxyId,
					false
				);
			}
		}
#else
		const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
		FShadowDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, *FoundView, Context, MeshBatch, false, true, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
#endif
	}
	}
}

void FProjectedShadowInfo::RenderProjection(FRHICommandListImmediate& RHICmdList, int32 ViewIndex, const FViewInfo* View, bool bProjectingForForwardShading, bool bMobileModulatedProjections
	// @third party code - BEGIN HairWorks
	, bool bHairPass
	// @third party code - END HairWorks
) const
{
#if WANTS_DRAW_MESH_EVENTS
	FString EventName;
	GetShadowTypeNameForDrawEvent(EventName);
	SCOPED_DRAW_EVENTF(RHICmdList, EventShadowProjectionActor, *EventName);
#endif

	FScopeCycleCounter Scope(bWholeSceneShadow ? GET_STATID(STAT_RenderWholeSceneShadowProjectionsTime) : GET_STATID(STAT_RenderPerObjectShadowProjectionsTime));

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// Find the shadow's view relevance.
	const FVisibleLightViewInfo& VisibleLightViewInfo = View->VisibleLightInfos[LightSceneInfo->Id];
	{
		FPrimitiveViewRelevance ViewRelevance = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap[ShadowId];

		// Don't render shadows for subjects which aren't view relevant.
		if (ViewRelevance.bShadowRelevance == false)
		{
			return;
		}
	}

	bool bCameraInsideShadowFrustum;
	TArray<FVector4, TInlineAllocator<8>> FrustumVertices;
	SetupFrustumForProjection(View, FrustumVertices, bCameraInsideShadowFrustum);

	const bool bDepthBoundsTestEnabled = IsWholeSceneDirectionalShadow() && GSupportsDepthBoundsTest && CVarCSMDepthBoundsTest.GetValueOnRenderThread() != 0;

	if (!bDepthBoundsTestEnabled)
	{
		SetupProjectionStencilMask(RHICmdList, View, FrustumVertices, bMobileModulatedProjections, bCameraInsideShadowFrustum
			// @third party code - BEGIN HairWorks
			, bHairPass
			// @third party code - END HairWorks
		);
	}

	// solid rasterization w/ back-face culling.
	GraphicsPSOInit.RasterizerState = (View->bReverseCulling || IsWholeSceneDirectionalShadow()) ? TStaticRasterizerState<FM_Solid,CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid,CM_CW>::GetRHI();

	if (bDepthBoundsTestEnabled)
	{
		EnableDepthBoundsTest(RHICmdList, CascadeSettings.SplitNear, CascadeSettings.SplitFar, View->ViewMatrices.GetProjectionMatrix());

		// no depth test or writes
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	}
	else
	{
		if (GStencilOptimization)
		{
			// No depth test or writes, zero the stencil
			// Note: this will disable hi-stencil on many GPUs, but still seems 
			// to be faster. However, early stencil still works 
			GraphicsPSOInit.DepthStencilState =
				TStaticDepthStencilState<
				false, CF_Always,
				true, CF_NotEqual, SO_Zero, SO_Zero, SO_Zero,
				false, CF_Always, SO_Zero, SO_Zero, SO_Zero,
				0xff, 0xff
				>::GetRHI();
		}
		else
		{
			// no depth test or writes, Test stencil for non-zero.
			GraphicsPSOInit.DepthStencilState = 
				TStaticDepthStencilState<
				false, CF_Always,
				true, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep,
				false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
				0xff, 0xff
				>::GetRHI();
		}
	}

	SetBlendStateForProjection(GraphicsPSOInit, bProjectingForForwardShading, bMobileModulatedProjections);

	GraphicsPSOInit.PrimitiveType = IsWholeSceneDirectionalShadow() ? PT_TriangleStrip : PT_TriangleList;

	{
		uint32 LocalQuality = GetShadowQuality();

		if (LocalQuality > 1)
		{
			if (IsWholeSceneDirectionalShadow() && CascadeSettings.ShadowSplitIndex > 0)
			{
				// adjust kernel size so that the penumbra size of distant splits will better match up with the closer ones
				const float SizeScale = CascadeSettings.ShadowSplitIndex / FMath::Max(0.001f, CVarCSMSplitPenumbraScale.GetValueOnRenderThread());
			}
			else if (LocalQuality > 2 && !bWholeSceneShadow)
			{
				static auto CVarPreShadowResolutionFactor = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Shadow.PreShadowResolutionFactor"));
				const int32 TargetResolution = bPreShadow ? FMath::TruncToInt(512 * CVarPreShadowResolutionFactor->GetValueOnRenderThread()) : 512;

				int32 Reduce = 0;

				{
					int32 Res = ResolutionX;

					while (Res < TargetResolution)
					{
						Res *= 2;
						++Reduce;
					}
				}

				// Never drop to quality 1 due to low resolution, aliasing is too bad
				LocalQuality = FMath::Clamp((int32)LocalQuality - Reduce, 3, 5);
			}
		}

		FShadowProjectionVertexShaderInterface* ShadowProjVS = nullptr;
		FShadowProjectionPixelShaderInterface* ShadowProjPS = nullptr;

		GetShadowProjectionShaders(LocalQuality, *View, this, bMobileModulatedProjections, &ShadowProjVS, &ShadowProjPS);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(ShadowProjVS);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(ShadowProjPS);

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
        
        RHICmdList.SetStencilRef(0);

		ShadowProjVS->SetParameters(RHICmdList, *View, this);
		ShadowProjPS->SetParameters(RHICmdList, ViewIndex, *View, this);
		}

	if (IsWholeSceneDirectionalShadow())
	{
		// Render a full screen quad.
		FVector4 Verts[4] =
		{
			FVector4(-1.0f, 1.0f, 0.0f),
			FVector4(1.0f, 1.0f, 0.0f),
			FVector4(-1.0f, -1.0f, 0.0f),
			FVector4(1.0f, -1.0f, 0.0f),
		};
		DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, Verts, sizeof(FVector4));
	}
	else
	{
		// Draw the frustum using the projection shader..
		DrawIndexedPrimitiveUP(RHICmdList, PT_TriangleList, 0, 8, 12, GCubeIndices, sizeof(uint16), FrustumVertices.GetData(), sizeof(FVector4));
	}

	if (bDepthBoundsTestEnabled)
	{
		DisableDepthBoundsTest(RHICmdList);
	}
	else
	{
		// Clear the stencil buffer to 0.
		if (!GStencilOptimization)
		{
			DrawClearQuad(RHICmdList, false, FLinearColor::Transparent, false, 0, true, 1);
		}
	}
}


template <uint32 Quality>
static void SetPointLightShaderTempl(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, int32 ViewIndex, const FViewInfo& View, const FProjectedShadowInfo* ShadowInfo)
{
	TShaderMapRef<FShadowVolumeBoundProjectionVS> VertexShader(View.ShaderMap);
	TShaderMapRef<TOnePassPointShadowProjectionPS<Quality> > PixelShader(View.ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
	
	VertexShader->SetParameters(RHICmdList, View, ShadowInfo);
	PixelShader->SetParameters(RHICmdList, ViewIndex, View, ShadowInfo);
}

/** Render one pass point light shadow projections. */
void FProjectedShadowInfo::RenderOnePassPointLightProjection(FRHICommandListImmediate& RHICmdList, int32 ViewIndex, const FViewInfo& View, bool bProjectingForForwardShading) const
{
	SCOPE_CYCLE_COUNTER(STAT_RenderWholeSceneShadowProjectionsTime);

	checkSlow(bOnePassPointLightShadow);
	
	const FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	SetBlendStateForProjection(GraphicsPSOInit, bProjectingForForwardShading, false);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	const bool bCameraInsideLightGeometry = ((FVector)View.ViewMatrices.GetViewOrigin() - LightBounds.Center).SizeSquared() < FMath::Square(LightBounds.W * 1.05f + View.NearClippingDistance * 2.0f);

	if (bCameraInsideLightGeometry)
	{
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		// Render backfaces with depth tests disabled since the camera is inside (or close to inside) the light geometry
		GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI();
	}
	else
	{
		// Render frontfaces with depth tests on to get the speedup from HiZ since the camera is outside the light geometry
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
		GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
	}	

	{
		uint32 LocalQuality = GetShadowQuality();

		if(LocalQuality > 1)
		{
			// adjust kernel size so that the penumbra size of distant splits will better match up with the closer ones
			//const float SizeScale = ShadowInfo->ResolutionX;
			int32 Reduce = 0;

			{
				int32 Res = ResolutionX;

				while(Res < 512)
				{
					Res *= 2;
					++Reduce;
				}
			}
		}

		switch(LocalQuality)
		{
			case 1: SetPointLightShaderTempl<1>(RHICmdList, GraphicsPSOInit, ViewIndex, View, this); break;
			case 2: SetPointLightShaderTempl<2>(RHICmdList, GraphicsPSOInit, ViewIndex, View, this); break;
			case 3: SetPointLightShaderTempl<3>(RHICmdList, GraphicsPSOInit, ViewIndex, View, this); break;
			case 4: SetPointLightShaderTempl<4>(RHICmdList, GraphicsPSOInit, ViewIndex, View, this); break;
			case 5: SetPointLightShaderTempl<5>(RHICmdList, GraphicsPSOInit, ViewIndex, View, this); break;
			default:
				check(0);
		}
	}

	// Project the point light shadow with some approximately bounding geometry, 
	// So we can get speedups from depth testing and not processing pixels outside of the light's influence.
	StencilingGeometry::DrawSphere(RHICmdList);
}

void FProjectedShadowInfo::RenderFrustumWireframe(FPrimitiveDrawInterface* PDI) const
{
	// Find the ID of an arbitrary subject primitive to use to color the shadow frustum.
	int32 SubjectPrimitiveId = 0;
	if(DynamicSubjectPrimitives.Num())
	{
		SubjectPrimitiveId = DynamicSubjectPrimitives[0]->GetIndex();
	}

	const FMatrix InvShadowTransform = (bWholeSceneShadow || bPreShadow) ? SubjectAndReceiverMatrix.InverseFast() : InvReceiverMatrix;

	FColor Color;

	if(IsWholeSceneDirectionalShadow())
	{
		Color = FColor::White;
		switch(CascadeSettings.ShadowSplitIndex)
		{
			case 0: Color = FColor::Red; break;
			case 1: Color = FColor::Yellow; break;
			case 2: Color = FColor::Green; break;
			case 3: Color = FColor::Blue; break;
		}
	}
	else
	{
		Color = FLinearColor::FGetHSV(( ( SubjectPrimitiveId + LightSceneInfo->Id ) * 31 ) & 255, 0, 255).ToFColor(true);
	}

	// Render the wireframe for the frustum derived from ReceiverMatrix.
	DrawFrustumWireframe(
		PDI,
		InvShadowTransform * FTranslationMatrix(-PreShadowTranslation),
		Color,
		SDPG_World
		);
}

FMatrix FProjectedShadowInfo::GetScreenToShadowMatrix(const FSceneView& View, uint32 TileOffsetX, uint32 TileOffsetY, uint32 TileResolutionX, uint32 TileResolutionY) const
{
	const FIntPoint ShadowBufferResolution = GetShadowBufferResolution();
	const float InvBufferResolutionX = 1.0f / (float)ShadowBufferResolution.X;
	const float ShadowResolutionFractionX = 0.5f * (float)TileResolutionX * InvBufferResolutionX;
	const float InvBufferResolutionY = 1.0f / (float)ShadowBufferResolution.Y;
	const float ShadowResolutionFractionY = 0.5f * (float)TileResolutionY * InvBufferResolutionY;
	// Calculate the matrix to transform a screenspace position into shadow map space
	FMatrix ScreenToShadow = 
		// Z of the position being transformed is actually view space Z, 
		// Transform it into post projection space by applying the projection matrix,
		// Which is the required space before applying View.InvTranslatedViewProjectionMatrix
		FMatrix(
			FPlane(1,0,0,0),
			FPlane(0,1,0,0),
			FPlane(0,0,View.ViewMatrices.GetProjectionMatrix().M[2][2],1),
			FPlane(0,0,View.ViewMatrices.GetProjectionMatrix().M[3][2],0)) * 
		// Transform the post projection space position into translated world space
		// Translated world space is normal world space translated to the view's origin, 
		// Which prevents floating point imprecision far from the world origin.
		View.ViewMatrices.GetInvTranslatedViewProjectionMatrix() *
		// Translate to the origin of the shadow's translated world space
		FTranslationMatrix(PreShadowTranslation - View.ViewMatrices.GetPreViewTranslation()) *
		// Transform into the shadow's post projection space
		// This has to be the same transform used to render the shadow depths
		SubjectAndReceiverMatrix *
		// Scale and translate x and y to be texture coordinates into the ShadowInfo's rectangle in the shadow depth buffer
		// Normalize z by MaxSubjectDepth, as was done when writing shadow depths
		FMatrix(
			FPlane(ShadowResolutionFractionX,0,							0,									0),
			FPlane(0,						 -ShadowResolutionFractionY,0,									0),
			FPlane(0,						0,							InvMaxSubjectDepth,	0),
			FPlane(
				(TileOffsetX + BorderSize) * InvBufferResolutionX + ShadowResolutionFractionX,
				(TileOffsetY + BorderSize) * InvBufferResolutionY + ShadowResolutionFractionY,
				0,
				1
			)
		);
	return ScreenToShadow;
}

FMatrix FProjectedShadowInfo::GetWorldToShadowMatrix(FVector4& ShadowmapMinMax, const FIntPoint* ShadowBufferResolutionOverride) const
{
	FIntPoint ShadowBufferResolution = ( ShadowBufferResolutionOverride ) ? *ShadowBufferResolutionOverride : GetShadowBufferResolution();

	const float InvBufferResolutionX = 1.0f / (float)ShadowBufferResolution.X;
	const float ShadowResolutionFractionX = 0.5f * (float)ResolutionX * InvBufferResolutionX;
	const float InvBufferResolutionY = 1.0f / (float)ShadowBufferResolution.Y;
	const float ShadowResolutionFractionY = 0.5f * (float)ResolutionY * InvBufferResolutionY;

	const FMatrix WorldToShadowMatrix =
		// Translate to the origin of the shadow's translated world space
		FTranslationMatrix(PreShadowTranslation) *
		// Transform into the shadow's post projection space
		// This has to be the same transform used to render the shadow depths
		SubjectAndReceiverMatrix *
		// Scale and translate x and y to be texture coordinates into the ShadowInfo's rectangle in the shadow depth buffer
		// Normalize z by MaxSubjectDepth, as was done when writing shadow depths
		FMatrix(
			FPlane(ShadowResolutionFractionX,0,							0,									0),
			FPlane(0,						 -ShadowResolutionFractionY,0,									0),
			FPlane(0,						0,							InvMaxSubjectDepth,	0),
			FPlane(
				(X + BorderSize) * InvBufferResolutionX + ShadowResolutionFractionX,
				(Y + BorderSize) * InvBufferResolutionY + ShadowResolutionFractionY,
				0,
				1
			)
		);

	ShadowmapMinMax = FVector4(
		(X + BorderSize) * InvBufferResolutionX, 
		(Y + BorderSize) * InvBufferResolutionY,
		(X + BorderSize * 2 + ResolutionX) * InvBufferResolutionX, 
		(Y + BorderSize * 2 + ResolutionY) * InvBufferResolutionY);

	return WorldToShadowMatrix;
}

void FProjectedShadowInfo::UpdateShaderDepthBias()
{
	float DepthBias = 0;

	if (IsWholeScenePointLightShadow())
	{
		DepthBias = CVarPointLightShadowDepthBias.GetValueOnRenderThread() * 512.0f / FMath::Max(ResolutionX, ResolutionY);
		// * 2.0f to be compatible with the system we had before ShadowBias
		DepthBias *= 2.0f * LightSceneInfo->Proxy->GetUserShadowBias();
	}
	else if (IsWholeSceneDirectionalShadow())
	{
		check(CascadeSettings.ShadowSplitIndex >= 0);

		// the z range is adjusted to we need to adjust here as well
		DepthBias = CVarCSMShadowDepthBias.GetValueOnRenderThread() / (MaxSubjectZ - MinSubjectZ);

		float WorldSpaceTexelScale = ShadowBounds.W / ResolutionX;
		
		DepthBias *= WorldSpaceTexelScale;
		DepthBias *= LightSceneInfo->Proxy->GetUserShadowBias();
	}
	else if (bPreShadow)
	{
		// Preshadows don't need a depth bias since there is no self shadowing
		DepthBias = 0;
	}
	else
	{
		// per object shadows
		if(bDirectionalLight)
		{
			// we use CSMShadowDepthBias cvar but this is per object shadows, maybe we want to use different settings

			// the z range is adjusted to we need to adjust here as well
			DepthBias = CVarPerObjectDirectionalShadowDepthBias.GetValueOnRenderThread() / (MaxSubjectZ - MinSubjectZ);

			float WorldSpaceTexelScale = ShadowBounds.W / FMath::Max(ResolutionX, ResolutionY);
		
			DepthBias *= WorldSpaceTexelScale;
			DepthBias *= 0.5f;	// avg GetUserShadowBias, in that case we don't want this adjustable
		}
		else
		{
			// spot lights (old code, might need to be improved)
			const float LightTypeDepthBias = CVarSpotLightShadowDepthBias.GetValueOnRenderThread();
			DepthBias = LightTypeDepthBias * 512.0f / ((MaxSubjectZ - MinSubjectZ) * FMath::Max(ResolutionX, ResolutionY));
			// * 2.0f to be compatible with the system we had before ShadowBias
			DepthBias *= 2.0f * LightSceneInfo->Proxy->GetUserShadowBias();
		}

		// Prevent a large depth bias due to low resolution from causing near plane clipping
		DepthBias = FMath::Min(DepthBias, .1f);
	}

	ShaderDepthBias = FMath::Max(DepthBias, 0.0f);
}

float FProjectedShadowInfo::ComputeTransitionSize() const
{
	float TransitionSize = 1;

	if (IsWholeScenePointLightShadow())
	{
		// todo: optimize
		TransitionSize = bDirectionalLight ? (1.0f / CVarShadowTransitionScale.GetValueOnRenderThread()) : (1.0f / CVarSpotLightShadowTransitionScale.GetValueOnRenderThread());
		// * 2.0f to be compatible with the system we had before ShadowBias
		TransitionSize *= 2.0f * LightSceneInfo->Proxy->GetUserShadowBias();
	}
	else if (IsWholeSceneDirectionalShadow())
	{
		check(CascadeSettings.ShadowSplitIndex >= 0);

		// todo: remove GetShadowTransitionScale()
		// make 1/ ShadowTransitionScale, SpotLightShadowTransitionScale

		// the z range is adjusted to we need to adjust here as well
		TransitionSize = CVarCSMShadowDepthBias.GetValueOnRenderThread() / (MaxSubjectZ - MinSubjectZ);

		float WorldSpaceTexelScale = ShadowBounds.W / ResolutionX;

		TransitionSize *= WorldSpaceTexelScale;
		TransitionSize *= LightSceneInfo->Proxy->GetUserShadowBias();
	}
	else if (bPreShadow)
	{
		// Preshadows don't have self shadowing, so make sure the shadow starts as close to the caster as possible
		TransitionSize = 0.00001f;
	}
	else
	{
		// todo: optimize
		TransitionSize = bDirectionalLight ? (1.0f / CVarShadowTransitionScale.GetValueOnRenderThread()) : (1.0f / CVarSpotLightShadowTransitionScale.GetValueOnRenderThread());
		// * 2.0f to be compatible with the system we had before ShadowBias
		TransitionSize *= 2.0f * LightSceneInfo->Proxy->GetUserShadowBias();
	}

	return TransitionSize;
}

/*-----------------------------------------------------------------------------
FDeferredShadingSceneRenderer
-----------------------------------------------------------------------------*/

/**
 * Used by RenderLights to figure out if projected shadows need to be rendered to the attenuation buffer.
 *
 * @param LightSceneInfo Represents the current light
 * @return true if anything needs to be rendered
 */
bool FSceneRenderer::CheckForProjectedShadows( const FLightSceneInfo* LightSceneInfo ) const
{
	// Find the projected shadows cast by this light.
	const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];
	for( int32 ShadowIndex=0; ShadowIndex<VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++ )
	{
		const FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.AllProjectedShadows[ShadowIndex];

		// Check that the shadow is visible in at least one view before rendering it.
		bool bShadowIsVisible = false;
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			if (ProjectedShadowInfo->DependentView && ProjectedShadowInfo->DependentView != &View)
			{
				continue;
			}
			const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightSceneInfo->Id];
			bShadowIsVisible |= VisibleLightViewInfo.ProjectedShadowVisibilityMap[ShadowIndex];
		}

		if(bShadowIsVisible)
		{
			return true;
		}
	}
	return false;
}

bool FDeferredShadingSceneRenderer::InjectReflectiveShadowMaps(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo)
{
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

	// Inject the RSM into the LPVs
	for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.RSMsToProject.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.RSMsToProject[ShadowIndex];

		check(ProjectedShadowInfo->bReflectiveShadowmap);

		if (ProjectedShadowInfo->bAllocated && ProjectedShadowInfo->DependentView)
		{
			FSceneViewState* ViewState = (FSceneViewState*)ProjectedShadowInfo->DependentView->State;

			FLightPropagationVolume* LightPropagationVolume = ViewState ? ViewState->GetLightPropagationVolume(FeatureLevel) : NULL;

			if (LightPropagationVolume)
			{
				if (ProjectedShadowInfo->bWholeSceneShadow)
				{
					LightPropagationVolume->InjectDirectionalLightRSM( 
						RHICmdList, 
						*ProjectedShadowInfo->DependentView,
						(const FTexture2DRHIRef&)ProjectedShadowInfo->RenderTargets.ColorTargets[0]->GetRenderTargetItem().ShaderResourceTexture,
						(const FTexture2DRHIRef&)ProjectedShadowInfo->RenderTargets.ColorTargets[1]->GetRenderTargetItem().ShaderResourceTexture, 
						(const FTexture2DRHIRef&)ProjectedShadowInfo->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture,
						*ProjectedShadowInfo, 
						LightSceneInfo->Proxy->GetColor() );
				}
			}
		}
	}

	return true;
}
	
extern int32 GCapsuleShadows;

bool FSceneRenderer::RenderShadowProjections(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, IPooledRenderTarget* ScreenShadowMaskTexture, bool bProjectingForForwardShading, bool bMobileModulatedProjections)
{
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	// @third party code - BEGIN HairWorks
	bool bHairPass = false;

RenderForHair:
	SCOPED_CONDITIONAL_DRAW_EVENT(RHICmdList, RenderForHair, bHairPass);

	if(bHairPass)
	{
		FSceneRenderTargets::Get(RHICmdList).SceneDepthZ.Swap(HairWorksRenderer::HairRenderTargets->HairDepthZForShadow);
		ScreenShadowMaskTexture = HairWorksRenderer::HairRenderTargets->LightAttenuation;
	}
	// @third party code - END HairWorks
	
	if (bMobileModulatedProjections)
	{
		SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);
	}
	else
	{
		// Normal deferred shadows render to the shadow mask
		SetRenderTarget(RHICmdList, ScreenShadowMaskTexture->GetRenderTargetItem().TargetableTexture, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, true);
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

		const FViewInfo& View = Views[ViewIndex];

		// Set the device viewport for the view.
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		// Set the light's scissor rectangle.
		LightSceneInfo->Proxy->SetScissorRect(RHICmdList, View);

		// Project the shadow depth buffers onto the scene.
		for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.ShadowsToProject.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.ShadowsToProject[ShadowIndex];

			if (ProjectedShadowInfo->bRayTracedDistanceField)
			{
				// @third party code - END HairWorks
				if(bHairPass)
					continue;
				// @third party code - BEGIN HairWorks

				ProjectedShadowInfo->RenderRayTracedDistanceFieldProjection(RHICmdList, View, ScreenShadowMaskTexture, bProjectingForForwardShading);
			}
			else if (ProjectedShadowInfo->bAllocated)
			{
				// Only project the shadow if it's large enough in this particular view (split screen, etc... may have shadows that are large in one view but irrelevantly small in others)
				if (ProjectedShadowInfo->FadeAlphas[ViewIndex] > 1.0f / 256.0f)
				{
					if (ProjectedShadowInfo->bOnePassPointLightShadow)
					{
						ProjectedShadowInfo->RenderOnePassPointLightProjection(RHICmdList, ViewIndex, View, bProjectingForForwardShading);
					}
					else 
					{
						ProjectedShadowInfo->RenderProjection(RHICmdList, ViewIndex, &View, bProjectingForForwardShading, bMobileModulatedProjections
						// @third party code - BEGIN HairWorks
						, bHairPass
						// @third party code - END HairWorks
						);
					}
					}
				}
			}

		// Reset the scissor rectangle.
		RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	}

	// @third party code - BEGIN HairWorks
	if(bHairPass)
	{
		FSceneRenderTargets::Get(RHICmdList).SceneDepthZ.Swap(HairWorksRenderer::HairRenderTargets->HairDepthZForShadow);
	}

	if(!bHairPass && HairWorksRenderer::ViewsHasHair(Views))
	{
		bHairPass = true;
		goto RenderForHair;
	}
	// @third party code - END HairWorks

	return true;
}	

// @third party code - BEGIN HairWorks
bool FProjectedShadowInfo::ShouldRenderForHair(const FViewInfo& View)const
{
	// If no hair is visible, skip. Also skip self shadow.
	if(!View.VisibleHairs.Num() || bSelfShadowOnly)
		return false;

	static TAutoConsoleVariable<int> CVarHairCullDynamicShadow(TEXT("r.HairWorks.CullDynamicShadow"), 1, TEXT(""), ECVF_RenderThreadSafe);

	// Check for point light
	if(bOnePassPointLightShadow)
	{
		if(bRayTracedDistanceField)
		{
			if(CVarHairCullDynamicShadow.GetValueOnRenderThread() == 0)
				return true;

			for(auto* PrimitiveInfo : View.VisibleHairs)	// This may not be efficient if there are too many hairs.
			{
				auto& HairBounds = PrimitiveInfo->Proxy->GetBounds();

				if(ShadowBounds.Intersects(HairBounds.GetSphere()))
					return true;
			}

			return false;
		}
		else
		{
			for(auto* PrimitiveSceneInfo : DynamicSubjectPrimitives)
			{
				auto& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveSceneInfo->GetIndex()];
				if(ViewRelevance.bHairWorks)
				{
					return true;
				}
			}
		}

		return false;
	}

	// Check pre-shadow. Whether any hair is receiver.
	if(bPreShadow)
	{
		for(auto* PrimitiveSceneInfo : ReceiverPrimitives)
		{
			auto& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveSceneInfo->GetIndex()];
			if(ViewRelevance.bHairWorks)
			{
				return true;
			}
		}

		return false;
	}
	// Check dynamic shadow. Whether receiver frustum touches any visible hairs.This may not be efficient if there are too many hairs.
	else
	{
		if(CVarHairCullDynamicShadow.GetValueOnRenderThread() == 0)
			return true;

		for(auto* PrimitiveInfo : View.VisibleHairs)
		{
			auto& HairBounds = PrimitiveInfo->Proxy->GetBounds();

			if(bWholeSceneShadow && bDirectionalLight && !bRayTracedDistanceField)
			{
				if(CascadeSettings.ShadowBoundsAccurate.IntersectBox(HairBounds.Origin, HairBounds.BoxExtent))
					return true;
			}
			else
			{
				if(ReceiverFrustum.IntersectBox(HairBounds.Origin + PreShadowTranslation, HairBounds.BoxExtent))
					return true;
			}
		}

		return false;
	}
}
// @third party code - END HairWorks

bool FDeferredShadingSceneRenderer::RenderShadowProjections(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, IPooledRenderTarget* ScreenShadowMaskTexture, bool& bInjectedTranslucentVolume)
{
	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_RenderShadowProjections, FColor::Emerald);
	SCOPE_CYCLE_COUNTER(STAT_ProjectedShadowDrawTime);
	SCOPED_DRAW_EVENT(RHICmdList, ShadowProjectionOnOpaque);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_ShadowProjection);

	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

	FSceneRenderer::RenderShadowProjections(RHICmdList, LightSceneInfo, ScreenShadowMaskTexture, false, false);

	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	TArray<FProjectedShadowInfo*, SceneRenderingAllocator> DirectionalShadows;
#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting

	for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.ShadowsToProject.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.ShadowsToProject[ShadowIndex];

		if (ProjectedShadowInfo->bAllocated
			&& ProjectedShadowInfo->bWholeSceneShadow
			// Not supported on translucency yet
			&& !ProjectedShadowInfo->bRayTracedDistanceField
			// Don't inject shadowed lighting with whole scene shadows used for previewing a light with static shadows,
			// Since that would cause a mismatch with the built lighting
			// However, stationary directional lights allow whole scene shadows that blend with precomputed shadowing
			&& (!LightSceneInfo->Proxy->HasStaticShadowing() || ProjectedShadowInfo->IsWholeSceneDirectionalShadow()))
		{
			bInjectedTranslucentVolume = true;
			SCOPED_DRAW_EVENT(RHICmdList, InjectTranslucentVolume);
			// Inject the shadowed light into the translucency lighting volumes
			InjectTranslucentVolumeLighting(RHICmdList, *LightSceneInfo, ProjectedShadowInfo);
			// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
			if (LightSceneInfo->Proxy->IsNVVolumetricLighting())
			{
				if (!ProjectedShadowInfo->IsWholeSceneDirectionalShadow())
				{
					NVVolumetricLightingRenderVolume(RHICmdList, LightSceneInfo, ProjectedShadowInfo);
				}
				else
				{
					DirectionalShadows.Add(ProjectedShadowInfo);
				}
			}
#endif
			// NVCHANGE_END: Nvidia Volumetric Lighting
		}
	}


	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	if (DirectionalShadows.Num() > 0)
	{
		NVVolumetricLightingRenderVolume(RHICmdList, LightSceneInfo, DirectionalShadows);
	}
#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting

	RenderCapsuleDirectShadows(RHICmdList, *LightSceneInfo, ScreenShadowMaskTexture, VisibleLightInfo.CapsuleShadowsToProject, false);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.ShadowsToProject.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.ShadowsToProject[ShadowIndex];

			if (ProjectedShadowInfo->bAllocated
				&& ProjectedShadowInfo->bWholeSceneShadow)
			{
				View.HeightfieldLightingViewInfo.ComputeShadowMapShadowing(View, RHICmdList, ProjectedShadowInfo);
			}
		}
	}

	return true;
}

void FMobileSceneRenderer::RenderModulatedShadowProjections(FRHICommandListImmediate& RHICmdList)
{
	if (IsSimpleForwardShadingEnabled(GetFeatureLevelShaderPlatform(FeatureLevel)) || !ViewFamily.EngineShowFlags.DynamicShadows || !IsMobileHDR())
	{
		return;
	}
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	// render shadowmaps for relevant lights.
	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;
		if(LightSceneInfo->ShouldRenderLightViewIndependent() && LightSceneInfo->Proxy && LightSceneInfo->Proxy->CastsModulatedShadows())
		{
			TArray<FProjectedShadowInfo*, SceneRenderingAllocator> Shadows;
			SCOPE_CYCLE_COUNTER(STAT_ProjectedShadowDrawTime);
			FSceneRenderer::RenderShadowProjections(RHICmdList, LightSceneInfo, NULL, false, true);
		}
	}
}
