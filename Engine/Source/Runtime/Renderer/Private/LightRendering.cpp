// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightRendering.cpp: Light rendering implementation.
=============================================================================*/

#include "LightRendering.h"
#include "EngineGlobals.h"
#include "DeferredShadingRenderer.h"
#include "LightPropagationVolume.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
// @third party code - BEGIN HairWorks
#include "HairWorksRenderer.h"
// @third party code - END HairWorks

DECLARE_FLOAT_COUNTER_STAT(TEXT("Lights"), Stat_GPU_Lights, STATGROUP_GPU);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FDeferredLightUniformStruct,TEXT("DeferredLightUniforms"));

extern int32 GUseTranslucentLightingVolumes;


static int32 bAllowDepthBoundsTest = 1;
static FAutoConsoleVariableRef CVarAllowDepthBoundsTest(
	TEXT("r.AllowDepthBoundsTest"),
	bAllowDepthBoundsTest,
	TEXT("If true, use enable depth bounds test when rendering defered lights.")
	);

static int32 bAllowSimpleLights = 1;
static FAutoConsoleVariableRef CVarAllowSimpleLights(
	TEXT("r.AllowSimpleLights"),
	bAllowSimpleLights,
	TEXT("If true, we allow simple (ie particle) lights")
);

// Implement a version for directional lights, and a version for point / spot lights
IMPLEMENT_SHADER_TYPE(template<>,TDeferredLightVS<false>,TEXT("/Engine/Private/DeferredLightVertexShaders.usf"),TEXT("DirectionalVertexMain"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<>,TDeferredLightVS<true>,TEXT("/Engine/Private/DeferredLightVertexShaders.usf"),TEXT("RadialVertexMain"),SF_Vertex);

/** A pixel shader for rendering the light in a deferred pass. */
template<bool bUseIESProfile, bool bRadialAttenuation, bool bInverseSquaredFalloff, bool bVisualizeLightCulling, bool bUseLightingChannels>
class TDeferredLightPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDeferredLightPS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_IES_PROFILE"), (uint32)bUseIESProfile);
		OutEnvironment.SetDefine(TEXT("RADIAL_ATTENUATION"), (uint32)bRadialAttenuation);
		OutEnvironment.SetDefine(TEXT("INVERSE_SQUARED_FALLOFF"), (uint32)bInverseSquaredFalloff);
		OutEnvironment.SetDefine(TEXT("LIGHT_SOURCE_SHAPE"), 1);
		OutEnvironment.SetDefine(TEXT("VISUALIZE_LIGHT_CULLING"), (uint32)bVisualizeLightCulling);
		OutEnvironment.SetDefine(TEXT("USE_LIGHTING_CHANNELS"), (uint32)bUseLightingChannels);
	}

	TDeferredLightPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		LightAttenuationTexture.Bind(Initializer.ParameterMap, TEXT("LightAttenuationTexture"));
		LightAttenuationTextureSampler.Bind(Initializer.ParameterMap, TEXT("LightAttenuationTextureSampler"));
		// @third party code - BEGIN HairWorks
		HairDeferredParameters.Bind(Initializer.ParameterMap);
		// @third party code - END HairWorks
		PreIntegratedBRDF.Bind(Initializer.ParameterMap, TEXT("PreIntegratedBRDF"));
		PreIntegratedBRDFSampler.Bind(Initializer.ParameterMap, TEXT("PreIntegratedBRDFSampler"));
		IESTexture.Bind(Initializer.ParameterMap, TEXT("IESTexture"));
		IESTextureSampler.Bind(Initializer.ParameterMap, TEXT("IESTextureSampler"));
		LightingChannelsTexture.Bind(Initializer.ParameterMap, TEXT("LightingChannelsTexture"));
		LightingChannelsSampler.Bind(Initializer.ParameterMap, TEXT("LightingChannelsSampler"));
	}

	TDeferredLightPS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FLightSceneInfo* LightSceneInfo, IPooledRenderTarget* ScreenShadowMaskTexture
		// @third party code - BEGIN HairWorks
		, bool bLightenHair = false
		// @third party code - END HairWorks
		)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		SetParametersBase(RHICmdList, ShaderRHI, View, ScreenShadowMaskTexture, LightSceneInfo->Proxy->GetIESTextureResource());
		SetDeferredLightParameters(RHICmdList, ShaderRHI, GetUniformBufferParameter<FDeferredLightUniformStruct>(), LightSceneInfo, View);

		// @third party code - BEGIN HairWorks
		// Hair parameters
		HairDeferredParameters.SetParameters(RHICmdList, ShaderRHI, *this, bLightenHair);
		// @third party code - END HairWorks
	}

	void SetParametersSimpleLight(FRHICommandList& RHICmdList, const FSceneView& View, const FSimpleLightEntry& SimpleLight, const FSimpleLightPerViewEntry& SimpleLightPerViewData)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		SetParametersBase(RHICmdList, ShaderRHI, View, NULL, NULL);
		SetSimpleDeferredLightParameters(RHICmdList, ShaderRHI, GetUniformBufferParameter<FDeferredLightUniformStruct>(), SimpleLight, SimpleLightPerViewData, View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << LightAttenuationTexture;
		Ar << LightAttenuationTextureSampler;
		// @third party code - BEGIN HairWorks
		Ar << HairDeferredParameters;
		// @third party code - END HairWorks
		Ar << PreIntegratedBRDF;
		Ar << PreIntegratedBRDFSampler;
		Ar << IESTexture;
		Ar << IESTextureSampler;
		Ar << LightingChannelsTexture;
		Ar << LightingChannelsSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	void SetParametersBase(FRHICommandList& RHICmdList, const FPixelShaderRHIParamRef ShaderRHI, const FSceneView& View, IPooledRenderTarget* ScreenShadowMaskTexture, FTexture* IESTextureResource)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI,View.ViewUniformBuffer);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View, MD_PostProcess);

		FSceneRenderTargets& SceneRenderTargets = FSceneRenderTargets::Get(RHICmdList);

		if(LightAttenuationTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				LightAttenuationTexture,
				LightAttenuationTextureSampler,
				TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
				ScreenShadowMaskTexture ? ScreenShadowMaskTexture->GetRenderTargetItem().ShaderResourceTexture : GWhiteTexture->TextureRHI
				);
		}

		SetTextureParameter(
			RHICmdList, 
			ShaderRHI,
			PreIntegratedBRDF,
			PreIntegratedBRDFSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			GEngine->PreIntegratedSkinBRDFTexture->Resource->TextureRHI
			);

		{
			FTextureRHIParamRef TextureRHI = IESTextureResource ? IESTextureResource->TextureRHI : GSystemTextures.WhiteDummy->GetRenderTargetItem().TargetableTexture;

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				IESTexture,
				IESTextureSampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				TextureRHI
				);
		}

		if (bUseLightingChannels)
		{
			FTextureRHIParamRef LightingChannelsTextureRHI = SceneRenderTargets.LightingChannels ? SceneRenderTargets.LightingChannels->GetRenderTargetItem().ShaderResourceTexture : GSystemTextures.WhiteDummy->GetRenderTargetItem().TargetableTexture;

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				LightingChannelsTexture,
				LightingChannelsSampler,
				TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				LightingChannelsTextureRHI
				);
		}
	}

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter LightAttenuationTexture;
	FShaderResourceParameter LightAttenuationTextureSampler;
	// @third party code - BEGIN HairWorks
	HairWorksRenderer::FDeferredShadingParameters HairDeferredParameters;
	// @third party code - END HairWorks
	FShaderResourceParameter PreIntegratedBRDF;
	FShaderResourceParameter PreIntegratedBRDFSampler;
	FShaderResourceParameter IESTexture;
	FShaderResourceParameter IESTextureSampler;
	FShaderResourceParameter LightingChannelsTexture;
	FShaderResourceParameter LightingChannelsSampler;
};

// Typedef is necessary because the C preprocessor thinks the comma in the template parameter list is a comma in the macro parameter list.
#define IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(A, B, C, D, E, EntryName) \
	typedef TDeferredLightPS<A,B,C,D,E> TDeferredLightPS##A##B##C##D##E; \
	IMPLEMENT_SHADER_TYPE(template<>,TDeferredLightPS##A##B##C##D##E,TEXT("/Engine/Private/DeferredLightPixelShaders.usf"),EntryName,SF_Pixel);

// Implement a version for each light type, and it's shader permutations
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(true, true, true, false, false, TEXT("RadialPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(true, true, false, false, false, TEXT("RadialPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(true, false, false, false, false, TEXT("DirectionalPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(false, true, true, false, false, TEXT("RadialPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(false, true, false, false, false, TEXT("RadialPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(false, false, false, false, false, TEXT("DirectionalPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(false, false, false, true, false, TEXT("DirectionalPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(false, true, false, true, false, TEXT("RadialPixelMain"));

IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(true, true, true, false, true, TEXT("RadialPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(true, true, false, false, true, TEXT("RadialPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(true, false, false, false, true, TEXT("DirectionalPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(false, true, true, false, true, TEXT("RadialPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(false, true, false, false, true, TEXT("RadialPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(false, false, false, false, true, TEXT("DirectionalPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(false, false, false, true, true, TEXT("DirectionalPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(false, true, false, true, true, TEXT("RadialPixelMain"));

/** Shader used to visualize stationary light overlap. */
template<bool bRadialAttenuation>
class TDeferredLightOverlapPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDeferredLightOverlapPS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("RADIAL_ATTENUATION"), (uint32)bRadialAttenuation);
	}

	TDeferredLightOverlapPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader(Initializer)
	{
		HasValidChannel.Bind(Initializer.ParameterMap, TEXT("HasValidChannel"));
		DeferredParameters.Bind(Initializer.ParameterMap);
	}

	TDeferredLightOverlapPS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FLightSceneInfo* LightSceneInfo)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI,View.ViewUniformBuffer);
		const float HasValidChannelValue = LightSceneInfo->Proxy->GetPreviewShadowMapChannel() == INDEX_NONE ? 0.0f : 1.0f;
		SetShaderValue(RHICmdList, ShaderRHI, HasValidChannel, HasValidChannelValue);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View, MD_PostProcess);
		SetDeferredLightParameters(RHICmdList, ShaderRHI, GetUniformBufferParameter<FDeferredLightUniformStruct>(), LightSceneInfo, View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << HasValidChannel;
		Ar << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter HasValidChannel;
	FDeferredPixelShaderParameters DeferredParameters;
};

IMPLEMENT_SHADER_TYPE(template<>, TDeferredLightOverlapPS<true>, TEXT("/Engine/Private/StationaryLightOverlapShaders.usf"), TEXT("OverlapRadialPixelMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TDeferredLightOverlapPS<false>, TEXT("/Engine/Private/StationaryLightOverlapShaders.usf"), TEXT("OverlapDirectionalPixelMain"), SF_Pixel);

/** Gathers simple lights from visible primtives in the passed in views. */
void FSceneRenderer::GatherSimpleLights(const FSceneViewFamily& ViewFamily, const TArray<FViewInfo>& Views, FSimpleLightArray& SimpleLights)
{
	TArray<const FPrimitiveSceneInfo*, SceneRenderingAllocator> PrimitivesWithSimpleLights;

	// Gather visible primitives from all views that might have simple lights
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		for (int32 PrimitiveIndex = 0; PrimitiveIndex < View.VisibleDynamicPrimitives.Num(); PrimitiveIndex++)
		{
			const FPrimitiveSceneInfo* PrimitiveSceneInfo = View.VisibleDynamicPrimitives[PrimitiveIndex];
			const int32 PrimitiveId = PrimitiveSceneInfo->GetIndex();
			const FPrimitiveViewRelevance& PrimitiveViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveId];

			if (PrimitiveViewRelevance.bHasSimpleLights)
			{
				// TArray::AddUnique is slow, but not expecting many entries in PrimitivesWithSimpleLights
				PrimitivesWithSimpleLights.AddUnique(PrimitiveSceneInfo);
			}
		}
	}

	// Gather simple lights from the primitives
	for (int32 PrimitiveIndex = 0; PrimitiveIndex < PrimitivesWithSimpleLights.Num(); PrimitiveIndex++)
	{
		const FPrimitiveSceneInfo* Primitive = PrimitivesWithSimpleLights[PrimitiveIndex];
		Primitive->Proxy->GatherSimpleLights(ViewFamily, SimpleLights);
	}
}

/** Gets a readable light name for use with a draw event. */
void FSceneRenderer::GetLightNameForDrawEvent(const FLightSceneProxy* LightProxy, FString& LightNameWithLevel)
{
#if WANTS_DRAW_MESH_EVENTS
	if (GEmitDrawEvents)
	{
		FString FullLevelName = LightProxy->GetLevelName().ToString();
		const int32 LastSlashIndex = FullLevelName.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);

		if (LastSlashIndex != INDEX_NONE)
		{
			// Trim the leading path before the level name to make it more readable
			// The level FName was taken directly from the Outermost UObject, otherwise we would do this operation on the game thread
			FullLevelName = FullLevelName.Mid(LastSlashIndex + 1, FullLevelName.Len() - (LastSlashIndex + 1));
		}

		LightNameWithLevel = FullLevelName + TEXT(".") + LightProxy->GetComponentName().ToString();
	}
#endif
}

extern int32 GbEnableAsyncComputeTranslucencyLightingVolumeClear;

uint32 GetShadowQuality();

/** Renders the scene's lighting. */
void FDeferredShadingSceneRenderer::RenderLights(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_RenderLights, FColor::Emerald);
	SCOPED_DRAW_EVENT(RHICmdList, Lights);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_Lights);


	bool bStencilBufferDirty = false;	// The stencil buffer should've been cleared to 0 already

	SCOPE_CYCLE_COUNTER(STAT_LightingDrawTime);
	SCOPE_CYCLE_COUNTER(STAT_LightRendering);

	FSimpleLightArray SimpleLights;
	if (bAllowSimpleLights)
	{
		GatherSimpleLights(ViewFamily, Views, SimpleLights);
	}

	TArray<FSortedLightSceneInfo, SceneRenderingAllocator> SortedLights;
	SortedLights.Empty(Scene->Lights.Num());

	bool bDynamicShadows = ViewFamily.EngineShowFlags.DynamicShadows && GetShadowQuality() > 0;
	
	// Build a list of visible lights.
	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		if (LightSceneInfo->ShouldRenderLightViewIndependent()
			// Reflection override skips direct specular because it tends to be blindingly bright with a perfectly smooth surface
			&& !ViewFamily.EngineShowFlags.ReflectionOverride)
		{
			// Check if the light is visible in any of the views.
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (LightSceneInfo->ShouldRenderLight(Views[ViewIndex]))
				{
					FSortedLightSceneInfo* SortedLightInfo = new(SortedLights) FSortedLightSceneInfo(LightSceneInfo);

					// Check for shadows and light functions.
					SortedLightInfo->SortKey.Fields.LightType = LightSceneInfoCompact.LightType;
					SortedLightInfo->SortKey.Fields.bTextureProfile = ViewFamily.EngineShowFlags.TexturedLightProfiles && LightSceneInfo->Proxy->GetIESTextureResource();
					SortedLightInfo->SortKey.Fields.bShadowed = bDynamicShadows && CheckForProjectedShadows(LightSceneInfo);
					SortedLightInfo->SortKey.Fields.bLightFunction = ViewFamily.EngineShowFlags.LightFunctions && CheckForLightFunction(LightSceneInfo);
					break;
				}
			}
		}
	}

	// Sort non-shadowed, non-light function lights first to avoid render target switches.
	struct FCompareFSortedLightSceneInfo
	{
		FORCEINLINE bool operator()( const FSortedLightSceneInfo& A, const FSortedLightSceneInfo& B ) const
		{
			return A.SortKey.Packed < B.SortKey.Packed;
		}
	};
	SortedLights.Sort( FCompareFSortedLightSceneInfo() );

	{
		SCOPED_DRAW_EVENT(RHICmdList, DirectLighting);

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		int32 AttenuationLightStart = SortedLights.Num();
		int32 SupportedByTiledDeferredLightEnd = SortedLights.Num();
		bool bAnyUnsupportedByTiledDeferred = false;

		// Iterate over all lights to be rendered and build ranges for tiled deferred and unshadowed lights
		for (int32 LightIndex = 0; LightIndex < SortedLights.Num(); LightIndex++)
		{
			const FSortedLightSceneInfo& SortedLightInfo = SortedLights[LightIndex];
			bool bDrawShadows = SortedLightInfo.SortKey.Fields.bShadowed;
			bool bDrawLightFunction = SortedLightInfo.SortKey.Fields.bLightFunction;
			bool bTextureLightProfile = SortedLightInfo.SortKey.Fields.bTextureProfile;

			if (bTextureLightProfile && SupportedByTiledDeferredLightEnd == SortedLights.Num())
			{
				// Mark the first index to not support tiled deferred due to texture light profile
				SupportedByTiledDeferredLightEnd = LightIndex;
			}

			if( bDrawShadows || bDrawLightFunction )
			{
				AttenuationLightStart = LightIndex;

				if (SupportedByTiledDeferredLightEnd == SortedLights.Num())
				{
					// Mark the first index to not support tiled deferred due to shadowing
					SupportedByTiledDeferredLightEnd = LightIndex;
				}
				break;
			}

			if (LightIndex < SupportedByTiledDeferredLightEnd)
			{
				// Directional lights currently not supported by tiled deferred
				bAnyUnsupportedByTiledDeferred = bAnyUnsupportedByTiledDeferred 
					|| (SortedLightInfo.SortKey.Fields.LightType != LightType_Point && SortedLightInfo.SortKey.Fields.LightType != LightType_Spot);
			}
		}
		
		if (GbEnableAsyncComputeTranslucencyLightingVolumeClear && GSupportsEfficientAsyncCompute)
		{
			//Gfx pipe must wait for the async compute clear of the translucency volume clear.
			RHICmdList.WaitComputeFence(TranslucencyLightingVolumeClearEndFence);
		}

		if(ViewFamily.EngineShowFlags.DirectLighting)
		{
			SCOPED_DRAW_EVENT(RHICmdList, NonShadowedLights);
			INC_DWORD_STAT_BY(STAT_NumUnshadowedLights, AttenuationLightStart);

			int32 StandardDeferredStart = 0;

			bool bRenderSimpleLightsStandardDeferred = SimpleLights.InstanceData.Num() > 0;

			if (CanUseTiledDeferred())
			{
				bool bAnyViewIsStereo = false;
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
				{
					if (Views[ViewIndex].StereoPass != eSSP_FULL)
					{
						bAnyViewIsStereo = true;
						break;
					}
				}

				// Use tiled deferred shading on any unshadowed lights without a texture light profile
				if (ShouldUseTiledDeferred(SupportedByTiledDeferredLightEnd, SimpleLights.InstanceData.Num()) && !bAnyUnsupportedByTiledDeferred && !bAnyViewIsStereo)
				{
					// Update the range that needs to be processed by standard deferred to exclude the lights done with tiled
					StandardDeferredStart = SupportedByTiledDeferredLightEnd;
					bRenderSimpleLightsStandardDeferred = false;
					RenderTiledDeferredLighting(RHICmdList, SortedLights, SupportedByTiledDeferredLightEnd, SimpleLights);
				}
			}
			
			if (bRenderSimpleLightsStandardDeferred)
			{
				SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);
				RenderSimpleLightsStandardDeferred(RHICmdList, SimpleLights);
			}

			{
				SCOPED_DRAW_EVENT(RHICmdList, StandardDeferredLighting);

				// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
				{
					for (int32 LightIndex = StandardDeferredStart; LightIndex < AttenuationLightStart; LightIndex++)
					{
						const FSortedLightSceneInfo& SortedLightInfo = SortedLights[LightIndex];
						const FLightSceneInfo* const LightSceneInfo = SortedLightInfo.LightSceneInfo;

						if (!LightSceneInfo->Proxy->HasStaticShadowing()
							&& LightSceneInfo->Proxy->IsNVVolumetricLighting())
						{
							NVVolumetricLightingRenderVolume(RHICmdList, LightSceneInfo);
						}
					}
				}
#endif
				// NVCHANGE_END: Nvidia Volumetric Lighting

				// make sure we don't clear the depth
				SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, true);

				// @third party code - BEGIN HairWorks
				if(HairWorksRenderer::ViewsHasHair(Views))
					HairWorksRenderer::BeginRenderingSceneColor(RHICmdList);
				// @third party code - END HairWorks

				// Draw non-shadowed non-light function lights without changing render targets between them
				for (int32 LightIndex = StandardDeferredStart; LightIndex < AttenuationLightStart; LightIndex++)
				{
					const FSortedLightSceneInfo& SortedLightInfo = SortedLights[LightIndex];
					const FLightSceneInfo* const LightSceneInfo = SortedLightInfo.LightSceneInfo;

					// Render the light to the scene color buffer, using a 1x1 white texture as input 
					RenderLight(RHICmdList, LightSceneInfo, NULL, false, false);
				}
			}

			if (GUseTranslucentLightingVolumes && GSupportsVolumeTextureRendering)
			{
				if (AttenuationLightStart)
				{
					// Inject non-shadowed, non-light function lights in to the volume.
					SCOPED_DRAW_EVENT(RHICmdList, InjectNonShadowedTranslucentLighting);
					InjectTranslucentVolumeLightingArray(RHICmdList, SortedLights, AttenuationLightStart);
				}
				
				if (SimpleLights.InstanceData.Num() > 0)
				{
					SCOPED_DRAW_EVENT(RHICmdList, InjectSimpleLightsTranslucentLighting);
					InjectSimpleTranslucentVolumeLightingArray(RHICmdList, SimpleLights);
				}
			}
		}

		EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel]; 

		if ( IsFeatureLevelSupported(ShaderPlatform, ERHIFeatureLevel::SM5) )
		{
			SCOPED_DRAW_EVENT(RHICmdList, IndirectLighting);
			bool bRenderedRSM = false;
			// Render Reflective shadow maps
			// Draw shadowed and light function lights
			for (int32 LightIndex = AttenuationLightStart; LightIndex < SortedLights.Num(); LightIndex++)
			{
				const FSortedLightSceneInfo& SortedLightInfo = SortedLights[LightIndex];
				const FLightSceneInfo& LightSceneInfo = *SortedLightInfo.LightSceneInfo;
				// Render any reflective shadow maps (if necessary)
				if ( LightSceneInfo.Proxy && LightSceneInfo.Proxy->NeedsLPVInjection() )
				{
					if ( LightSceneInfo.Proxy->HasReflectiveShadowMap() )
					{
						INC_DWORD_STAT(STAT_NumReflectiveShadowMapLights);
						InjectReflectiveShadowMaps(RHICmdList, &LightSceneInfo);
						bRenderedRSM = true;
					}
				}
			}

			// LPV Direct Light Injection
			if ( bRenderedRSM )
			{
				for (int32 LightIndex = 0; LightIndex < SortedLights.Num(); LightIndex++)
				{
					const FSortedLightSceneInfo& SortedLightInfo = SortedLights[LightIndex];
					const FLightSceneInfo* const LightSceneInfo = SortedLightInfo.LightSceneInfo;

					// Render any reflective shadow maps (if necessary)
					if ( LightSceneInfo && LightSceneInfo->Proxy && LightSceneInfo->Proxy->NeedsLPVInjection() )
					{
						if ( !LightSceneInfo->Proxy->HasReflectiveShadowMap() )
						{
							// Inject the light directly into all relevant LPVs
							for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
							{
								FViewInfo& View = Views[ViewIndex];

								if (LightSceneInfo->ShouldRenderLight(View))
								{
									FSceneViewState* ViewState = (FSceneViewState*)View.State;
									if (ViewState)
									{
										FLightPropagationVolume* Lpv = ViewState->GetLightPropagationVolume(View.GetFeatureLevel());
										if (Lpv && LightSceneInfo->Proxy)
										{
											Lpv->InjectLightDirect(RHICmdList, *LightSceneInfo->Proxy, View);
										}
									}
								}
							}					
						}
					}
				}
			}

			// Kickoff the LPV update (asynchronously if possible)
			UpdateLPVs(RHICmdList);
		}

		{
			SCOPED_DRAW_EVENT(RHICmdList, ShadowedLights);

			bool bDirectLighting = ViewFamily.EngineShowFlags.DirectLighting;

			TRefCountPtr<IPooledRenderTarget> ScreenShadowMaskTexture;

			// Draw shadowed and light function lights
			for (int32 LightIndex = AttenuationLightStart; LightIndex < SortedLights.Num(); LightIndex++)
			{
				const FSortedLightSceneInfo& SortedLightInfo = SortedLights[LightIndex];
				const FLightSceneInfo& LightSceneInfo = *SortedLightInfo.LightSceneInfo;
				bool bDrawShadows = SortedLightInfo.SortKey.Fields.bShadowed;
				bool bDrawLightFunction = SortedLightInfo.SortKey.Fields.bLightFunction;
				bool bDrawPreviewIndicator = ViewFamily.EngineShowFlags.PreviewShadowsIndicator && !LightSceneInfo.IsPrecomputedLightingValid() && LightSceneInfo.Proxy->HasStaticShadowing();
				bool bInjectedTranslucentVolume = false;
				bool bUsedShadowMaskTexture = false;
				FScopeCycleCounter Context(LightSceneInfo.Proxy->GetStatId());

				if ((bDrawShadows || bDrawLightFunction || bDrawPreviewIndicator) && !ScreenShadowMaskTexture.IsValid())
				{
					FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SceneContext.GetBufferSizeXY(), PF_B8G8R8A8, FClearValueBinding::White, TexCreate_None, TexCreate_RenderTargetable, false));
					Desc.Flags |= GFastVRamConfig.ScreenSpaceShadowMask;
					Desc.NumSamples = SceneContext.GetNumSceneColorMSAASamples(SceneContext.GetCurrentFeatureLevel());
					GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ScreenShadowMaskTexture, TEXT("ScreenShadowMaskTexture"));
				}

				FString LightNameWithLevel;
				GetLightNameForDrawEvent(LightSceneInfo.Proxy, LightNameWithLevel);
				SCOPED_DRAW_EVENTF(RHICmdList, EventLightPass, *LightNameWithLevel);

				if (bDrawShadows)
				{
					INC_DWORD_STAT(STAT_NumShadowedLights);

					for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
					{
						const FViewInfo& View = Views[ViewIndex];
						View.HeightfieldLightingViewInfo.ClearShadowing(View, RHICmdList, LightSceneInfo);
					}

					// @third party code - BEGIN HairWorks
					// Clear for hair.
					if(HairWorksRenderer::ViewsHasHair(Views))
					{
						GRenderTargetPool.FindFreeElement(
							RHICmdList,
							ScreenShadowMaskTexture->GetDesc(),
							HairWorksRenderer::HairRenderTargets->LightAttenuation,
							TEXT("HairLightAttenuation")
						);

						SetRenderTarget(
							RHICmdList,
							HairWorksRenderer::HairRenderTargets->LightAttenuation->GetRenderTargetItem().TargetableTexture,
							nullptr,
							ESimpleRenderTargetMode::EClearColorExistingDepth
						);
					}
					// @third party code - END HairWorks

					// Clear light attenuation for local lights with a quad covering their extents
					const bool bClearLightScreenExtentsOnly = SortedLightInfo.SortKey.Fields.LightType != LightType_Directional;
					// All shadows render with min blending
					bool bClearToWhite = !bClearLightScreenExtentsOnly;

					SetRenderTarget(RHICmdList, ScreenShadowMaskTexture->GetRenderTargetItem().TargetableTexture, SceneContext.GetSceneDepthSurface(), bClearToWhite ? ESimpleRenderTargetMode::EClearColorExistingDepth : ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, true);

					if (bClearLightScreenExtentsOnly)
					{
						SCOPED_DRAW_EVENT(RHICmdList, ClearQuad);

						for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
						{
							const FViewInfo& View = Views[ViewIndex];
							FIntRect ScissorRect;

							if (!LightSceneInfo.Proxy->GetScissorRect(ScissorRect, View))
							{
								ScissorRect = View.ViewRect;
							}

							RHICmdList.SetViewport(ScissorRect.Min.X, ScissorRect.Min.Y, 0.0f, ScissorRect.Max.X, ScissorRect.Max.Y, 1.0f);
							DrawClearQuad(RHICmdList, true, FLinearColor(1, 1, 1, 1), false, 0, false, 0);
						}
					}

					RenderShadowProjections(RHICmdList, &LightSceneInfo, ScreenShadowMaskTexture, bInjectedTranslucentVolume);

					bUsedShadowMaskTexture = true;
				}

				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					const FViewInfo& View = Views[ViewIndex];
					View.HeightfieldLightingViewInfo.ComputeLighting(View, RHICmdList, LightSceneInfo);
				}
			
				// Render light function to the attenuation buffer.
				if (bDirectLighting)
				{
					if (bDrawLightFunction)
					{
						const bool bLightFunctionRendered = RenderLightFunction(RHICmdList, &LightSceneInfo, ScreenShadowMaskTexture, bDrawShadows, false);
						bUsedShadowMaskTexture |= bLightFunctionRendered;
					}

					if (bDrawPreviewIndicator)
					{
						RenderPreviewShadowsIndicator(RHICmdList, &LightSceneInfo, ScreenShadowMaskTexture, bUsedShadowMaskTexture);
					}

					if (!bDrawShadows)
					{
						INC_DWORD_STAT(STAT_NumLightFunctionOnlyLights);
					}
				}
			
				if (bUsedShadowMaskTexture)
				{
					RHICmdList.CopyToResolveTarget(ScreenShadowMaskTexture->GetRenderTargetItem().TargetableTexture, ScreenShadowMaskTexture->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams(FResolveRect()));
				}
			
				if(bDirectLighting && !bInjectedTranslucentVolume)
				{
					SCOPED_DRAW_EVENT(RHICmdList, InjectTranslucentVolume);
					// Accumulate this light's unshadowed contribution to the translucency lighting volume
					InjectTranslucentVolumeLighting(RHICmdList, LightSceneInfo, NULL);
				}

				SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);

				// @third party code - BEGIN HairWorks
				if(HairWorksRenderer::ViewsHasHair(Views))
					HairWorksRenderer::BeginRenderingSceneColor(RHICmdList);
				// @third party code - END HairWorks

				// Render the light to the scene color buffer, conditionally using the attenuation buffer or a 1x1 white texture as input 
				if(bDirectLighting)
				{
					RenderLight(RHICmdList, &LightSceneInfo, ScreenShadowMaskTexture, false, true);
				}
			}
		}
	}
}

void FDeferredShadingSceneRenderer::RenderLightArrayForOverlapViewmode(FRHICommandListImmediate& RHICmdList, const TSparseArray<FLightSceneInfoCompact>& LightArray)
{
	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(LightArray); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		// Nothing to do for black lights.
		if(LightSceneInfoCompact.Color.IsAlmostBlack())
		{
			continue;
		}

		bool bShouldRender = false;

		// Check if the light is visible in any of the views.
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			bShouldRender |= LightSceneInfo->ShouldRenderLight(Views[ViewIndex]);
		}

		if (bShouldRender 
			// Only render shadow casting stationary lights
			&& LightSceneInfo->Proxy->HasStaticShadowing() 
			&& !LightSceneInfo->Proxy->HasStaticLighting()
			&& LightSceneInfo->Proxy->CastsStaticShadow())
		{
			RenderLight(RHICmdList, LightSceneInfo, NULL, true, false);
		}
	}
}

void FDeferredShadingSceneRenderer::RenderStationaryLightOverlap(FRHICommandListImmediate& RHICmdList)
{
	if (Scene->bIsEditorScene)
	{
		FSceneRenderTargets::Get(RHICmdList).BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EUninitializedColorExistingDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);

		// Clear to discard base pass values in scene color since we didn't skip that, to have valid scene depths
		DrawClearQuad(RHICmdList, FLinearColor::Black);

		RenderLightArrayForOverlapViewmode(RHICmdList, Scene->Lights);

		//Note: making use of FScene::InvisibleLights, which contains lights that haven't been added to the scene in the same way as visible lights
		// So code called by RenderLightArrayForOverlapViewmode must be careful what it accesses
		RenderLightArrayForOverlapViewmode(RHICmdList, Scene->InvisibleLights);
	}
}

/** Sets up rasterizer and depth state for rendering bounding geometry in a deferred pass. */
void SetBoundingGeometryRasterizerAndDepthState(FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View, const FSphere& LightBounds)
{
	const bool bCameraInsideLightGeometry = ((FVector)View.ViewMatrices.GetViewOrigin() - LightBounds.Center).SizeSquared() < FMath::Square(LightBounds.W * 1.05f + View.NearClippingDistance * 2.0f)
		// Always draw backfaces in ortho
		//@todo - accurate ortho camera / light intersection
		|| !View.IsPerspectiveProjection();

	if (bCameraInsideLightGeometry)
	{
		// Render backfaces with depth tests disabled since the camera is inside (or close to inside) the light geometry
		GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI();
	}
	else
	{
		// Render frontfaces with depth tests on to get the speedup from HiZ since the camera is outside the light geometry
		GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
	}

	GraphicsPSOInit.DepthStencilState =
		bCameraInsideLightGeometry
		? TStaticDepthStencilState<false, CF_Always>::GetRHI()
		: TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
}

template <bool bRadialAttenuation>
static FVertexDeclarationRHIParamRef GetDeferredLightingVertexDeclaration()
{
	return bRadialAttenuation ? GetVertexDeclarationFVector4() : GFilterVertexDeclaration.VertexDeclarationRHI;
}

template<bool bUseIESProfile, bool bRadialAttenuation, bool bInverseSquaredFalloff>
static void SetShaderTemplLighting(
	FRHICommandList& RHICmdList,
	FGraphicsPipelineStateInitializer& GraphicsPSOInit,
	const FViewInfo& View,
	FShader* VertexShader,
	const FLightSceneInfo* LightSceneInfo, 
	IPooledRenderTarget* ScreenShadowMaskTexture
	// @third party code - BEGIN HairWorks
	, bool bLightenHair = false
	// @third party code - END HairWorks
	)
{
	if(View.Family->EngineShowFlags.VisualizeLightCulling)
	{
		TShaderMapRef<TDeferredLightPS<false, bRadialAttenuation, false, true, false> > PixelShader(View.ShaderMap);
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetDeferredLightingVertexDeclaration<bRadialAttenuation>();
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
		PixelShader->SetParameters(RHICmdList, View, LightSceneInfo, ScreenShadowMaskTexture);
	}
	else
	{
		if (View.bUsesLightingChannels)
		{
			TShaderMapRef<TDeferredLightPS<bUseIESProfile, bRadialAttenuation, bInverseSquaredFalloff, false, true> > PixelShader(View.ShaderMap);
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetDeferredLightingVertexDeclaration<bRadialAttenuation>();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			PixelShader->SetParameters(RHICmdList, View, LightSceneInfo, ScreenShadowMaskTexture
				// @third party code - BEGIN HairWorks
				, bLightenHair
				// @third party code - END HairWorks
				);
		}
		else
		{
			TShaderMapRef<TDeferredLightPS<bUseIESProfile, bRadialAttenuation, bInverseSquaredFalloff, false, false> > PixelShader(View.ShaderMap);
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetDeferredLightingVertexDeclaration<bRadialAttenuation>();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			PixelShader->SetParameters(RHICmdList, View, LightSceneInfo, ScreenShadowMaskTexture
				// @third party code - BEGIN HairWorks
				, bLightenHair
				// @third party code - END HairWorks
				);
		}
	}
}

template<bool bUseIESProfile, bool bRadialAttenuation, bool bInverseSquaredFalloff>
static void SetShaderTemplLightingSimple(
	FRHICommandList& RHICmdList,
	FGraphicsPipelineStateInitializer& GraphicsPSOInit,
	const FViewInfo& View,
	FShader* VertexShader,
	const FSimpleLightEntry& SimpleLight,
	const FSimpleLightPerViewEntry& SimpleLightPerViewData)
{
	if(View.Family->EngineShowFlags.VisualizeLightCulling)
	{
		TShaderMapRef<TDeferredLightPS<false, bRadialAttenuation, false, true, false> > PixelShader(View.ShaderMap);
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetDeferredLightingVertexDeclaration<bRadialAttenuation>();
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
		PixelShader->SetParametersSimpleLight(RHICmdList, View, SimpleLight, SimpleLightPerViewData);
	}
	else
	{
		TShaderMapRef<TDeferredLightPS<bUseIESProfile, bRadialAttenuation, bInverseSquaredFalloff, false, false> > PixelShader(View.ShaderMap);
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetDeferredLightingVertexDeclaration<bRadialAttenuation>();
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
		PixelShader->SetParametersSimpleLight(RHICmdList, View, SimpleLight, SimpleLightPerViewData);
	}
}

// Use DBT to allow work culling on shadow lights
void CalculateLightNearFarDepthFromBounds(const FViewInfo& View, const FSphere &LightBounds, float &NearDepth, float &FarDepth)
{
	const FMatrix ViewProjection = View.ViewMatrices.GetViewProjectionMatrix();
	const FVector ViewDirection = View.GetViewDirection();
	
	// push camera relative bounds center along view vec by its radius
	const FVector FarPoint = LightBounds.Center + LightBounds.W * ViewDirection;
	const FVector4 FarPoint4 = FVector4(FarPoint, 1.f);
	const FVector4 FarPoint4Clip = ViewProjection.TransformFVector4(FarPoint4);
	FarDepth = FarPoint4Clip.Z / FarPoint4Clip.W;

	// pull camera relative bounds center along -view vec by its radius
	const FVector NearPoint = LightBounds.Center - LightBounds.W * ViewDirection;
	const FVector4 NearPoint4 = FVector4(NearPoint, 1.f);
	const FVector4 NearPoint4Clip = ViewProjection.TransformFVector4(NearPoint4);
	NearDepth = NearPoint4Clip.Z / NearPoint4Clip.W;

	// negative means behind view, but we use a NearClipPlane==1.f depth	

	if (NearPoint4Clip.W < 0)
		NearDepth = 1;

	if (FarPoint4Clip.W < 0)
		FarDepth = 1;

	NearDepth = FMath::Clamp(NearDepth, 0.0f, 1.0f);
	FarDepth = FMath::Clamp(FarDepth, 0.0f, 1.0f);

}

/**
 * Used by RenderLights to render a light to the scene color buffer.
 *
 * @param LightSceneInfo Represents the current light
 * @param LightIndex The light's index into FScene::Lights
 * @return true if anything got rendered
 */
void FDeferredShadingSceneRenderer::RenderLight(FRHICommandList& RHICmdList, const FLightSceneInfo* LightSceneInfo, IPooledRenderTarget* ScreenShadowMaskTexture, bool bRenderOverlap, bool bIssueDrawEvent)
{
	SCOPE_CYCLE_COUNTER(STAT_DirectLightRenderingTime);
	INC_DWORD_STAT(STAT_NumLightsUsingStandardDeferred);
	SCOPED_CONDITIONAL_DRAW_EVENT(RHICmdList, StandardDeferredLighting, bIssueDrawEvent);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// Use additive blending for color
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	// @third party code - BEGIN HairWorks
	// Set blend state of second render target for hair
	if(HairWorksRenderer::ViewsHasHair(Views))
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One, CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
	// @third party code - END HairWorks

	bool bStencilDirty = false;
	const FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		// Ensure the light is valid for this view
		if (!LightSceneInfo->ShouldRenderLight(View))
		{
			continue;
		}

		bool bUseIESTexture = false;

		if(View.Family->EngineShowFlags.TexturedLightProfiles)
		{
			bUseIESTexture = (LightSceneInfo->Proxy->GetIESTextureResource() != 0);
		}

		// Set the device viewport for the view.
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		bool bClearCoatNeeded = (View.ShadingModelMaskInView & (1 << MSM_ClearCoat)) != 0;
		if (LightSceneInfo->Proxy->GetLightType() == LightType_Directional)
		{
			TShaderMapRef<TDeferredLightVS<false> > VertexShader(View.ShaderMap);

			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			if (bRenderOverlap)
			{
				TShaderMapRef<TDeferredLightOverlapPS<false> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetDeferredLightingVertexDeclaration<false>();
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				PixelShader->SetParameters(RHICmdList, View, LightSceneInfo);
			}
			else
			{
				if(bUseIESTexture)
				{
					SetShaderTemplLighting<true, false, false>(RHICmdList, GraphicsPSOInit, View, *VertexShader, LightSceneInfo, ScreenShadowMaskTexture);
				}
				else
				{
					SetShaderTemplLighting<false, false, false>(RHICmdList, GraphicsPSOInit, View, *VertexShader, LightSceneInfo, ScreenShadowMaskTexture, View.VisibleHairs.Num() > 0);
				}
			}

			VertexShader->SetParameters(RHICmdList, View, LightSceneInfo);

			// Apply the directional light as a full screen quad
			DrawRectangle( 
				RHICmdList,
				0, 0,
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Min.X, View.ViewRect.Min.Y, 
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Size(),
				FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY(),
				*VertexShader,
				EDRF_UseTriangleOptimization);
		}
		else
		{
			// @third party code - BEGIN HairWorks
			bool bHairPass = false;

RenderForHair:
			// @third party code - END HairWorks

			TShaderMapRef<TDeferredLightVS<true> > VertexShader(View.ShaderMap);

			SetBoundingGeometryRasterizerAndDepthState(GraphicsPSOInit, View, LightBounds);

			// @third party code - BEGIN HairWorks
			// Depth buffer is not for hair so we disable depth test
			if (bHairPass)
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			// @third party code - END HairWorks

			if (bRenderOverlap)
			{
				TShaderMapRef<TDeferredLightOverlapPS<true> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetDeferredLightingVertexDeclaration<true>();
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				PixelShader->SetParameters(RHICmdList, View, LightSceneInfo);
			}
			else
			{
				// @third party code - BEGIN HairWorks
				if( LightSceneInfo->Proxy->IsInverseSquared() )
				{
					if(bUseIESTexture)
					{
						SetShaderTemplLighting<true, true, true>(RHICmdList, GraphicsPSOInit, View, *VertexShader, LightSceneInfo, ScreenShadowMaskTexture, bHairPass);
					}
					else
					{
						SetShaderTemplLighting<false, true, true>(RHICmdList, GraphicsPSOInit, View, *VertexShader, LightSceneInfo, ScreenShadowMaskTexture, bHairPass);
					}
				}
				else
				{
					if(bUseIESTexture)
					{
						SetShaderTemplLighting<true, true, false>(RHICmdList, GraphicsPSOInit, View, *VertexShader, LightSceneInfo, ScreenShadowMaskTexture, bHairPass);
					}
					else
					{
						SetShaderTemplLighting<false, true, false>(RHICmdList, GraphicsPSOInit, View, *VertexShader, LightSceneInfo, ScreenShadowMaskTexture, bHairPass);
					}
				}
				// @third party code - END HairWorks
			}

			VertexShader->SetParameters(RHICmdList, View, LightSceneInfo);

			// NUse DBT to allow work culling on shadow lights
			if (GSupportsDepthBoundsTest && bAllowDepthBoundsTest != 0
				// @third party code - BEGIN HairWorks
				&& !bHairPass
				// @third party code - END HairWorks			
				)
			{
				// Can use the depth bounds test to skip work for pixels which won't be touched by the light (i.e outside the depth range)
				float NearDepth = 1.f;
				float FarDepth = 0.f;
				CalculateLightNearFarDepthFromBounds(View,LightBounds,NearDepth,FarDepth);

				if (NearDepth <= FarDepth)
				{
					NearDepth = 1.0f;
					FarDepth = 0.0f;
				}

				// UE4 uses reversed depth, so far < near
				RHICmdList.EnableDepthBoundsTest(true,FarDepth,NearDepth);
			}

			if (LightSceneInfo->Proxy->GetLightType() == LightType_Point)
			{
				// Apply the point or spot light with some approximately bounding geometry, 
				// So we can get speedups from depth testing and not processing pixels outside of the light's influence.
				StencilingGeometry::DrawSphere(RHICmdList);
			}
			else if (LightSceneInfo->Proxy->GetLightType() == LightType_Spot)
			{
				StencilingGeometry::DrawCone(RHICmdList);
			}

			// Use DBT to allow work culling on shadow lights
			if (GSupportsDepthBoundsTest && bAllowDepthBoundsTest != 0
				// @third party code - BEGIN HairWorks
				&& !bHairPass
				// @third party code - END HairWorks
				)
			{
				// Turn DBT back off
				RHICmdList.EnableDepthBoundsTest(false, 0, 1);
			}

			// @third party code - BEGIN HairWorks
			// Render light to hair buffer
			if(!bHairPass && HairWorksRenderer::IsLightAffectHair(*LightSceneInfo, View))
			{
				bHairPass = true;
				goto RenderForHair;
			}
			// @third party code - END HairWorks
		}
	}

	if (bStencilDirty)
	{
		// Clear the stencil buffer to 0.
		DrawClearQuad(RHICmdList, false, FLinearColor::Transparent, false, 0, true, 1);
	}
}

void FDeferredShadingSceneRenderer::RenderSimpleLightsStandardDeferred(FRHICommandListImmediate& RHICmdList, const FSimpleLightArray& SimpleLights)
{
	SCOPE_CYCLE_COUNTER(STAT_DirectLightRenderingTime);
	INC_DWORD_STAT_BY(STAT_NumLightsUsingStandardDeferred, SimpleLights.InstanceData.Num());
	SCOPED_DRAW_EVENT(RHICmdList, StandardDeferredSimpleLights);
	
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// Use additive blending for color
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	const int32 NumViews = Views.Num();
	for (int32 LightIndex = 0; LightIndex < SimpleLights.InstanceData.Num(); LightIndex++)
	{
		const FSimpleLightEntry& SimpleLight = SimpleLights.InstanceData[LightIndex];

		for (int32 ViewIndex = 0; ViewIndex < NumViews; ViewIndex++)
		{
			const FSimpleLightPerViewEntry& SimpleLightPerViewData = SimpleLights.GetViewDependentData(LightIndex, ViewIndex, NumViews);
			const FSphere LightBounds(SimpleLightPerViewData.Position, SimpleLight.Radius);

			FViewInfo& View = Views[ViewIndex];

			// Set the device viewport for the view.
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

			TShaderMapRef<TDeferredLightVS<true> > VertexShader(View.ShaderMap);

			SetBoundingGeometryRasterizerAndDepthState(GraphicsPSOInit, View, LightBounds);

			if (SimpleLight.Exponent == 0)
			{
				// inverse squared
				SetShaderTemplLightingSimple<false, true, true>(RHICmdList, GraphicsPSOInit, View, *VertexShader, SimpleLight, SimpleLightPerViewData);
			}
			else
			{
				// light's exponent, not inverse squared
				SetShaderTemplLightingSimple<false, true, false>(RHICmdList, GraphicsPSOInit, View, *VertexShader, SimpleLight, SimpleLightPerViewData);
			}

			VertexShader->SetSimpleLightParameters(RHICmdList, View, LightBounds);

			// Apply the point or spot light with some approximately bounding geometry, 
			// So we can get speedups from depth testing and not processing pixels outside of the light's influence.
			StencilingGeometry::DrawSphere(RHICmdList);
		}
	}
}
