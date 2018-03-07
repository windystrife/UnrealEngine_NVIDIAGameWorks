// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldLightingPost.cpp
=============================================================================*/

#include "DistanceFieldLightingPost.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "DistanceFieldLightingShared.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "CompositionLighting/PostProcessAmbientOcclusion.h"
#include "PipelineStateCache.h"

int32 GAOUseHistory = 1;
FAutoConsoleVariableRef CVarAOUseHistory(
	TEXT("r.AOUseHistory"),
	GAOUseHistory,
	TEXT("Whether to apply a temporal filter to the distance field AO, which reduces flickering but also adds trails when occluders are moving."),
	ECVF_RenderThreadSafe
	);

int32 GAOClearHistory = 0;
FAutoConsoleVariableRef CVarAOClearHistory(
	TEXT("r.AOClearHistory"),
	GAOClearHistory,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GAOHistoryStabilityPass = 1;
FAutoConsoleVariableRef CVarAOHistoryStabilityPass(
	TEXT("r.AOHistoryStabilityPass"),
	GAOHistoryStabilityPass,
	TEXT("Whether to gather stable results to fill in holes in the temporal reprojection.  Adds some GPU cost but improves temporal stability with foliage."),
	ECVF_RenderThreadSafe
	);

float GAOHistoryWeight = .85f;
FAutoConsoleVariableRef CVarAOHistoryWeight(
	TEXT("r.AOHistoryWeight"),
	GAOHistoryWeight,
	TEXT("Amount of last frame's AO to lerp into the final result.  Higher values increase stability, lower values have less streaking under occluder movement."),
	ECVF_RenderThreadSafe
	);

float GAOHistoryMinConfidenceScale = .8f;
FAutoConsoleVariableRef CVarAOHistoryMinConfidenceScale(
	TEXT("r.AOHistoryMinConfidenceScale"),
	GAOHistoryMinConfidenceScale,
	TEXT("Minimum amount that confidence can scale down the history weight. Pixels whose AO value was interpolated from foreground onto background incorrectly have a confidence of 0.\n")
	TEXT("At a value of 1, confidence is effectively disabled.  Lower values increase the convergence speed of AO history for pixels with low confidence, but introduce jittering (history is thrown away)."),
	ECVF_RenderThreadSafe
	);

float GAOHistoryDistanceThreshold = 30;
FAutoConsoleVariableRef CVarAOHistoryDistanceThreshold(
	TEXT("r.AOHistoryDistanceThreshold"),
	GAOHistoryDistanceThreshold,
	TEXT("World space distance threshold needed to discard last frame's DFAO results.  Lower values reduce ghosting from characters when near a wall but increase flickering artifacts."),
	ECVF_RenderThreadSafe
	);

float GAOViewFadeDistanceScale = .7f;
FAutoConsoleVariableRef CVarAOViewFadeDistanceScale(
	TEXT("r.AOViewFadeDistanceScale"),
	GAOViewFadeDistanceScale,
	TEXT("Distance over which AO will fade out as it approaches r.AOMaxViewDistance, as a fraction of r.AOMaxViewDistance."),
	ECVF_RenderThreadSafe
	);

template<bool bSupportIrradiance>
class TUpdateHistoryDepthRejectionPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TUpdateHistoryDepthRejectionPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance);
	}

	/** Default constructor. */
	TUpdateHistoryDepthRejectionPS() {}

	/** Initialization constructor. */
	TUpdateHistoryDepthRejectionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		BentNormalAOTexture.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexture"));
		ConfidenceTexture.Bind(Initializer.ParameterMap, TEXT("ConfidenceTexture"));
		ConfidenceSampler.Bind(Initializer.ParameterMap, TEXT("ConfidenceSampler"));
		BentNormalAOSampler.Bind(Initializer.ParameterMap, TEXT("BentNormalAOSampler"));
		BentNormalHistoryTexture.Bind(Initializer.ParameterMap, TEXT("BentNormalHistoryTexture"));
		ConfidenceHistorySampler.Bind(Initializer.ParameterMap, TEXT("ConfidenceHistorySampler"));
		ConfidenceHistoryTexture.Bind(Initializer.ParameterMap, TEXT("ConfidenceHistoryTexture"));
		BentNormalHistorySampler.Bind(Initializer.ParameterMap, TEXT("BentNormalHistorySampler"));
		IrradianceTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceTexture"));
		IrradianceSampler.Bind(Initializer.ParameterMap, TEXT("IrradianceSampler"));
		IrradianceHistoryTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceHistoryTexture"));
		IrradianceHistorySampler.Bind(Initializer.ParameterMap, TEXT("IrradianceHistorySampler"));
		HistoryWeight.Bind(Initializer.ParameterMap, TEXT("HistoryWeight"));
		AOHistoryMinConfidenceScale.Bind(Initializer.ParameterMap, TEXT("AOHistoryMinConfidenceScale"));
		HistoryDistanceThreshold.Bind(Initializer.ParameterMap, TEXT("HistoryDistanceThreshold"));
		UseHistoryFilter.Bind(Initializer.ParameterMap, TEXT("UseHistoryFilter"));
		VelocityTexture.Bind(Initializer.ParameterMap, TEXT("VelocityTexture"));
		VelocityTextureSampler.Bind(Initializer.ParameterMap, TEXT("VelocityTextureSampler"));
		DistanceFieldNormalTexture.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalTexture"));
		DistanceFieldNormalSampler.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalSampler"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		FSceneRenderTargetItem& DistanceFieldNormal, 
		FSceneRenderTargetItem& BentNormalHistoryTextureValue, 
		FSceneRenderTargetItem& ConfidenceHistoryTextureValue,
		TRefCountPtr<IPooledRenderTarget>* IrradianceHistoryRT, 
		FSceneRenderTargetItem& DistanceFieldAOBentNormal, 
		FSceneRenderTargetItem& DistanceFieldAOConfidence, 
		IPooledRenderTarget* DistanceFieldIrradiance,
		IPooledRenderTarget* VelocityTextureValue)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View, MD_PostProcess);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			BentNormalAOTexture,
			BentNormalAOSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldAOBentNormal.ShaderResourceTexture
			);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			ConfidenceTexture,
			ConfidenceSampler.IsBound() ? ConfidenceSampler : BentNormalAOSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldAOConfidence.ShaderResourceTexture
			);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			BentNormalHistoryTexture,
			BentNormalHistorySampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			BentNormalHistoryTextureValue.ShaderResourceTexture
			);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			ConfidenceHistoryTexture,
			ConfidenceHistorySampler.IsBound() ? ConfidenceHistorySampler : BentNormalHistorySampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			ConfidenceHistoryTextureValue.ShaderResourceTexture
			);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DistanceFieldNormalTexture,
			DistanceFieldNormalSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldNormal.ShaderResourceTexture
			);

		if (IrradianceTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				IrradianceTexture,
				IrradianceSampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				DistanceFieldIrradiance->GetRenderTargetItem().ShaderResourceTexture
				);
		}
		
		if (IrradianceHistoryTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				IrradianceHistoryTexture,
				IrradianceHistorySampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				(*IrradianceHistoryRT)->GetRenderTargetItem().ShaderResourceTexture
				);
		}

		SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<FCameraMotionParameters>(), CreateCameraMotionParametersUniformBuffer(View));

		SetShaderValue(RHICmdList, ShaderRHI, HistoryWeight, GAOHistoryWeight);
		SetShaderValue(RHICmdList, ShaderRHI, AOHistoryMinConfidenceScale, GAOHistoryMinConfidenceScale);
		SetShaderValue(RHICmdList, ShaderRHI, HistoryDistanceThreshold, GAOHistoryDistanceThreshold);
		SetShaderValue(RHICmdList, ShaderRHI, UseHistoryFilter, (GAOHistoryStabilityPass ? 1.0f : 0.0f));

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			VelocityTexture,
			VelocityTextureSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			VelocityTextureValue ? VelocityTextureValue->GetRenderTargetItem().ShaderResourceTexture : GBlackTexture->TextureRHI
			);
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << BentNormalAOTexture;
		Ar << ConfidenceTexture;
		Ar << ConfidenceSampler;
		Ar << BentNormalAOSampler;
		Ar << BentNormalHistoryTexture;
		Ar << ConfidenceHistoryTexture;
		Ar << ConfidenceHistorySampler;
		Ar << BentNormalHistorySampler;
		Ar << IrradianceTexture;
		Ar << IrradianceSampler;
		Ar << IrradianceHistoryTexture;
		Ar << IrradianceHistorySampler;
		Ar << HistoryWeight;
		Ar << AOHistoryMinConfidenceScale;
		Ar << HistoryDistanceThreshold;
		Ar << UseHistoryFilter;
		Ar << VelocityTexture;
		Ar << VelocityTextureSampler;
		Ar << DistanceFieldNormalTexture;
		Ar << DistanceFieldNormalSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter ConfidenceTexture;
	FShaderResourceParameter ConfidenceSampler;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderResourceParameter ConfidenceHistorySampler;
	FShaderResourceParameter ConfidenceHistoryTexture;
	FShaderResourceParameter BentNormalHistoryTexture;
	FShaderResourceParameter BentNormalHistorySampler;
	FShaderResourceParameter IrradianceTexture;
	FShaderResourceParameter IrradianceSampler;
	FShaderResourceParameter IrradianceHistoryTexture;
	FShaderResourceParameter IrradianceHistorySampler;
	FShaderParameter HistoryWeight;
	FShaderParameter AOHistoryMinConfidenceScale;
	FShaderParameter HistoryDistanceThreshold;
	FShaderParameter UseHistoryFilter;
	FShaderResourceParameter VelocityTexture;
	FShaderResourceParameter VelocityTextureSampler;
	FShaderResourceParameter DistanceFieldNormalTexture;
	FShaderResourceParameter DistanceFieldNormalSampler;
};

IMPLEMENT_SHADER_TYPE(template<>,TUpdateHistoryDepthRejectionPS<true>,TEXT("/Engine/Private/DistanceFieldLightingPost.usf"),TEXT("UpdateHistoryDepthRejectionPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TUpdateHistoryDepthRejectionPS<false>,TEXT("/Engine/Private/DistanceFieldLightingPost.usf"),TEXT("UpdateHistoryDepthRejectionPS"),SF_Pixel);


template<bool bSupportIrradiance>
class TFilterHistoryPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TFilterHistoryPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance);
	}

	/** Default constructor. */
	TFilterHistoryPS() {}

	/** Initialization constructor. */
	TFilterHistoryPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		BentNormalAOTexture.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexture"));
		ConfidenceTexture.Bind(Initializer.ParameterMap, TEXT("ConfidenceTexture"));
		ConfidenceSampler.Bind(Initializer.ParameterMap, TEXT("ConfidenceSampler"));
		BentNormalAOSampler.Bind(Initializer.ParameterMap, TEXT("BentNormalAOSampler"));
		IrradianceTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceTexture"));
		IrradianceSampler.Bind(Initializer.ParameterMap, TEXT("IrradianceSampler"));
		HistoryWeight.Bind(Initializer.ParameterMap, TEXT("HistoryWeight"));
		BentNormalAOTexelSize.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexelSize"));
		DistanceFieldNormalTexture.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalTexture"));
		DistanceFieldNormalSampler.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalSampler"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		FSceneRenderTargetItem& DistanceFieldNormal, 
		FSceneRenderTargetItem& BentNormalHistoryTextureValue, 
		FSceneRenderTargetItem& ConfidenceHistoryTextureValue, 
		IPooledRenderTarget* IrradianceHistoryRT)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			BentNormalAOTexture,
			BentNormalAOSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			BentNormalHistoryTextureValue.ShaderResourceTexture
			);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			ConfidenceTexture,
			ConfidenceSampler.IsBound() ? ConfidenceSampler : BentNormalAOSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			ConfidenceHistoryTextureValue.ShaderResourceTexture
			);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DistanceFieldNormalTexture,
			DistanceFieldNormalSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldNormal.ShaderResourceTexture
			);

		if (IrradianceTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				IrradianceTexture,
				IrradianceSampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				IrradianceHistoryRT->GetRenderTargetItem().ShaderResourceTexture
				);
		}

		SetShaderValue(RHICmdList, ShaderRHI, HistoryWeight, GAOHistoryWeight);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		
		const FIntPoint DownsampledBufferSize(SceneContext.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor));
		const FVector2D BaseLevelTexelSizeValue(1.0f / DownsampledBufferSize.X, 1.0f / DownsampledBufferSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, BentNormalAOTexelSize, BaseLevelTexelSizeValue);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << BentNormalAOTexture;
		Ar << ConfidenceTexture;
		Ar << ConfidenceSampler;
		Ar << BentNormalAOSampler;
		Ar << IrradianceTexture;
		Ar << IrradianceSampler;
		Ar << HistoryWeight;
		Ar << BentNormalAOTexelSize;
		Ar << DistanceFieldNormalTexture;
		Ar << DistanceFieldNormalSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter ConfidenceTexture;
	FShaderResourceParameter ConfidenceSampler;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderResourceParameter IrradianceTexture;
	FShaderResourceParameter IrradianceSampler;
	FShaderParameter HistoryWeight;
	FShaderParameter BentNormalAOTexelSize;
	FShaderResourceParameter DistanceFieldNormalTexture;
	FShaderResourceParameter DistanceFieldNormalSampler;
};

IMPLEMENT_SHADER_TYPE(template<>,TFilterHistoryPS<true>,TEXT("/Engine/Private/DistanceFieldLightingPost.usf"),TEXT("FilterHistoryPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TFilterHistoryPS<false>,TEXT("/Engine/Private/DistanceFieldLightingPost.usf"),TEXT("FilterHistoryPS"),SF_Pixel);


void AllocateOrReuseAORenderTarget(FRHICommandList& RHICmdList, TRefCountPtr<IPooledRenderTarget>& Target, const TCHAR* Name, EPixelFormat Format, uint32 Flags) 
{
	if (!Target)
	{
		FIntPoint BufferSize = GetBufferSizeForAO();

		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, Format, FClearValueBinding::None, Flags, TexCreate_RenderTargetable | TexCreate_UAV, false));
		Desc.AutoWritable = false;
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, Target, Name, true, ERenderTargetTransience::NonTransient);
	}
}

void UpdateHistory(
	FRHICommandList& RHICmdList,
	const FViewInfo& View, 
	const TCHAR* BentNormalHistoryRTName,
	const TCHAR* ConfidenceHistoryRTName,
	const TCHAR* IrradianceHistoryRTName,
	IPooledRenderTarget* VelocityTexture,
	FSceneRenderTargetItem& DistanceFieldNormal,
	/** Contains last frame's history, if non-NULL.  This will be updated with the new frame's history. */
	TRefCountPtr<IPooledRenderTarget>* BentNormalHistoryState,
	TRefCountPtr<IPooledRenderTarget>* ConfidenceHistoryState,
	TRefCountPtr<IPooledRenderTarget>* IrradianceHistoryState,
	/** Source */
	TRefCountPtr<IPooledRenderTarget>& BentNormalSource, 
	TRefCountPtr<IPooledRenderTarget>& ConfidenceSource,
	TRefCountPtr<IPooledRenderTarget>& IrradianceSource, 
	/** Output of Temporal Reprojection for the next step in the pipeline. */
	TRefCountPtr<IPooledRenderTarget>& BentNormalHistoryOutput,
	TRefCountPtr<IPooledRenderTarget>& IrradianceHistoryOutput)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	if (BentNormalHistoryState)
	{
		const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View);

		FIntPoint BufferSize = GetBufferSizeForAO();

		if (*BentNormalHistoryState 
			&& !View.bCameraCut 
			&& !View.bPrevTransformsReset 
			&& (!bUseDistanceFieldGI || (IrradianceHistoryState && *IrradianceHistoryState))
			&& !GAOClearHistory
			// If the scene render targets reallocate, toss the history so we don't read uninitialized data
			&& (*BentNormalHistoryState)->GetDesc().Extent == BufferSize)
		{
			uint32 HistoryPassOutputFlags = GAOHistoryStabilityPass ? GFastVRamConfig.DistanceFieldAOHistory : 0;
			// Reuse a render target from the pool with a consistent name, for vis purposes
			TRefCountPtr<IPooledRenderTarget> NewBentNormalHistory;
			AllocateOrReuseAORenderTarget(RHICmdList, NewBentNormalHistory, BentNormalHistoryRTName, PF_FloatRGBA, HistoryPassOutputFlags);

			TRefCountPtr<IPooledRenderTarget> NewConfidenceHistory;
			AllocateOrReuseAORenderTarget(RHICmdList, NewConfidenceHistory, ConfidenceHistoryRTName, PF_G8, HistoryPassOutputFlags);

			TRefCountPtr<IPooledRenderTarget> NewIrradianceHistory;

			if (bUseDistanceFieldGI)
			{
				AllocateOrReuseAORenderTarget(RHICmdList, NewIrradianceHistory, IrradianceHistoryRTName, PF_FloatRGB);
			}

			SCOPED_DRAW_EVENT(RHICmdList, UpdateHistory);

			{
				FTextureRHIParamRef RenderTargets[3] =
				{
					NewBentNormalHistory->GetRenderTargetItem().TargetableTexture,
					NewConfidenceHistory->GetRenderTargetItem().TargetableTexture,
					bUseDistanceFieldGI ? NewIrradianceHistory->GetRenderTargetItem().TargetableTexture : NULL,
				};

				SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets) - (bUseDistanceFieldGI ? 0 : 1), RenderTargets, FTextureRHIParamRef(), 0, NULL);
				RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);

				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

				TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;

				if (bUseDistanceFieldGI)
				{
					TShaderMapRef<TUpdateHistoryDepthRejectionPS<true> > PixelShader(View.ShaderMap);

					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
					PixelShader->SetParameters(RHICmdList, View, DistanceFieldNormal, (*BentNormalHistoryState)->GetRenderTargetItem(), (*ConfidenceHistoryState)->GetRenderTargetItem(), IrradianceHistoryState, BentNormalSource->GetRenderTargetItem(), ConfidenceSource->GetRenderTargetItem(), IrradianceSource, VelocityTexture);
				}
				else
				{
					TShaderMapRef<TUpdateHistoryDepthRejectionPS<false> > PixelShader(View.ShaderMap);

					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
					PixelShader->SetParameters(RHICmdList, View, DistanceFieldNormal, (*BentNormalHistoryState)->GetRenderTargetItem(), (*ConfidenceHistoryState)->GetRenderTargetItem(), IrradianceHistoryState, BentNormalSource->GetRenderTargetItem(), ConfidenceSource->GetRenderTargetItem(), IrradianceSource, VelocityTexture);
				}

				VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);

				DrawRectangle( 
					RHICmdList,
					0, 0, 
					View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
					View.ViewRect.Min.X / GAODownsampleFactor, View.ViewRect.Min.Y / GAODownsampleFactor,
					View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
					FIntPoint(View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor),
					SceneContext.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor),
					*VertexShader);

				RHICmdList.CopyToResolveTarget(NewBentNormalHistory->GetRenderTargetItem().TargetableTexture, NewBentNormalHistory->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
				RHICmdList.CopyToResolveTarget(NewConfidenceHistory->GetRenderTargetItem().TargetableTexture, NewConfidenceHistory->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
			
				if (bUseDistanceFieldGI)
				{
					RHICmdList.CopyToResolveTarget(NewIrradianceHistory->GetRenderTargetItem().TargetableTexture, NewIrradianceHistory->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
				}
			}

			if (GAOHistoryStabilityPass)
			{
				const FPooledRenderTargetDesc& HistoryDesc = (*BentNormalHistoryState)->GetDesc();

				// Reallocate history if buffer sizes have changed
				if (HistoryDesc.Extent != SceneContext.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor))
				{
					GRenderTargetPool.FreeUnusedResource(*BentNormalHistoryState);
					GRenderTargetPool.FreeUnusedResource(*ConfidenceHistoryState);
					*BentNormalHistoryState = NULL;
					*ConfidenceHistoryState = NULL;
					// Update the view state's render target reference with the new history
					AllocateOrReuseAORenderTarget(RHICmdList, *BentNormalHistoryState, BentNormalHistoryRTName, PF_FloatRGBA);
					AllocateOrReuseAORenderTarget(RHICmdList, *ConfidenceHistoryState, ConfidenceHistoryRTName, PF_G8);

					if (bUseDistanceFieldGI)
					{
						GRenderTargetPool.FreeUnusedResource(*IrradianceHistoryState);
						*IrradianceHistoryState = NULL;
						AllocateOrReuseAORenderTarget(RHICmdList, *IrradianceHistoryState, IrradianceHistoryRTName, PF_FloatRGB);
					}
				}

				{
					FTextureRHIParamRef RenderTargets[3] =
					{
						(*BentNormalHistoryState)->GetRenderTargetItem().TargetableTexture,
						(*ConfidenceHistoryState)->GetRenderTargetItem().TargetableTexture,
						bUseDistanceFieldGI ? (*IrradianceHistoryState)->GetRenderTargetItem().TargetableTexture : NULL,
					};

					SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets) - (bUseDistanceFieldGI ? 0 : 1), RenderTargets, FTextureRHIParamRef(), 0, NULL, true);
					RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);

					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

					TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					if (bUseDistanceFieldGI)
					{
						TShaderMapRef<TFilterHistoryPS<true> > PixelShader(View.ShaderMap);

						GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
						SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
						PixelShader->SetParameters(RHICmdList, View, DistanceFieldNormal, NewBentNormalHistory->GetRenderTargetItem(), NewConfidenceHistory->GetRenderTargetItem(), NewIrradianceHistory);
					}
					else
					{
						TShaderMapRef<TFilterHistoryPS<false> > PixelShader(View.ShaderMap);

						GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
						SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
						PixelShader->SetParameters(RHICmdList, View, DistanceFieldNormal, NewBentNormalHistory->GetRenderTargetItem(), NewConfidenceHistory->GetRenderTargetItem(), NewIrradianceHistory);
					}

					VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);

					DrawRectangle( 
						RHICmdList,
						0, 0, 
						View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
						0, 0,
						View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
						FIntPoint(View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor),
						SceneContext.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor),
						*VertexShader);

					RHICmdList.CopyToResolveTarget((*BentNormalHistoryState)->GetRenderTargetItem().TargetableTexture, (*BentNormalHistoryState)->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
					RHICmdList.CopyToResolveTarget((*ConfidenceHistoryState)->GetRenderTargetItem().TargetableTexture, (*ConfidenceHistoryState)->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());

					if (bUseDistanceFieldGI)
					{
						RHICmdList.CopyToResolveTarget((*IrradianceHistoryState)->GetRenderTargetItem().TargetableTexture, (*IrradianceHistoryState)->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
					}
				}

				BentNormalHistoryOutput = *BentNormalHistoryState;
				IrradianceHistoryOutput = *IrradianceHistoryState;
			}
			else
			{
				// Update the view state's render target reference with the new history
				*BentNormalHistoryState = NewBentNormalHistory;
				BentNormalHistoryOutput = NewBentNormalHistory;

				*ConfidenceHistoryState = NewConfidenceHistory;

				*IrradianceHistoryState = NewIrradianceHistory;
				IrradianceHistoryOutput = NewIrradianceHistory;
			}
		}
		else
		{
			// Use the current frame's mask for next frame's history
			*BentNormalHistoryState = BentNormalSource;
			BentNormalHistoryOutput = BentNormalSource;
			BentNormalSource = NULL;

			*ConfidenceHistoryState = ConfidenceSource;
			ConfidenceSource = NULL;

			*IrradianceHistoryState = IrradianceSource;
			IrradianceHistoryOutput = IrradianceSource;
			IrradianceSource = NULL;
		}
	}
	else
	{
		// Temporal reprojection is disabled or there is no view state - pass through
		BentNormalHistoryOutput = BentNormalSource;
		IrradianceHistoryOutput = IrradianceSource;
	}
}

enum EAOUpsampleType
{
	AOUpsample_OutputBentNormal,
	AOUpsample_OutputAO,
	AOUpsample_OutputBentNormalAndIrradiance,
	AOUpsample_OutputIrradiance
};

template<EAOUpsampleType UpsampleType, bool bModulateToSceneColor, bool bSupportSpecularOcclusion>
class TDistanceFieldAOUpsamplePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDistanceFieldAOUpsamplePS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("MODULATE_SCENE_COLOR"), bModulateToSceneColor);
		OutEnvironment.SetDefine(TEXT("OUTPUT_BENT_NORMAL"), (UpsampleType == AOUpsample_OutputBentNormal || UpsampleType == AOUpsample_OutputBentNormalAndIrradiance));
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), (UpsampleType == AOUpsample_OutputIrradiance || UpsampleType == AOUpsample_OutputBentNormalAndIrradiance));
		OutEnvironment.SetDefine(TEXT("SUPPORT_SPECULAR_OCCLUSION"), bSupportSpecularOcclusion);
		OutEnvironment.SetDefine(TEXT("OUTPUT_AO"), (UpsampleType == AOUpsample_OutputAO));
	}

	/** Default constructor. */
	TDistanceFieldAOUpsamplePS() {}

	/** Initialization constructor. */
	TDistanceFieldAOUpsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		BentNormalAOTexture.Bind(Initializer.ParameterMap,TEXT("BentNormalAOTexture"));
		BentNormalAOSampler.Bind(Initializer.ParameterMap,TEXT("BentNormalAOSampler"));
		IrradianceTexture.Bind(Initializer.ParameterMap,TEXT("IrradianceTexture"));
		IrradianceSampler.Bind(Initializer.ParameterMap,TEXT("IrradianceSampler"));
		SpecularOcclusionTexture.Bind(Initializer.ParameterMap,TEXT("SpecularOcclusionTexture"));
		SpecularOcclusionSampler.Bind(Initializer.ParameterMap,TEXT("SpecularOcclusionSampler"));
		MinIndirectDiffuseOcclusion.Bind(Initializer.ParameterMap,TEXT("MinIndirectDiffuseOcclusion"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, TRefCountPtr<IPooledRenderTarget>& DistanceFieldAOBentNormal, IPooledRenderTarget* DistanceFieldIrradiance)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View, MD_PostProcess);

		SetTextureParameter(RHICmdList, ShaderRHI, BentNormalAOTexture, BentNormalAOSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), DistanceFieldAOBentNormal->GetRenderTargetItem().ShaderResourceTexture);

		if (IrradianceTexture.IsBound())
		{
			SetTextureParameter(RHICmdList, ShaderRHI, IrradianceTexture, IrradianceSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), DistanceFieldIrradiance->GetRenderTargetItem().ShaderResourceTexture);
		}

		FScene* Scene = (FScene*)View.Family->Scene;
		const float MinOcclusion = Scene->SkyLight ? Scene->SkyLight->MinOcclusion : 0;
		SetShaderValue(RHICmdList, ShaderRHI, MinIndirectDiffuseOcclusion, MinOcclusion);
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << BentNormalAOTexture;
		Ar << BentNormalAOSampler;
		Ar << IrradianceTexture;
		Ar << IrradianceSampler;
		Ar << SpecularOcclusionTexture;
		Ar << SpecularOcclusionSampler;
		Ar << MinIndirectDiffuseOcclusion;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderResourceParameter IrradianceTexture;
	FShaderResourceParameter IrradianceSampler;
	FShaderResourceParameter SpecularOcclusionTexture;
	FShaderResourceParameter SpecularOcclusionSampler;
	FShaderParameter MinIndirectDiffuseOcclusion;
};

#define IMPLEMENT_UPSAMPLE_PS_TYPE(UpsampleType, bModulateToSceneColor, bSupportSpecularOcclusion) \
	typedef TDistanceFieldAOUpsamplePS<UpsampleType, bModulateToSceneColor, bSupportSpecularOcclusion> TDistanceFieldAOUpsamplePS##UpsampleType##bModulateToSceneColor##bSupportSpecularOcclusion; \
	IMPLEMENT_SHADER_TYPE(template<>,TDistanceFieldAOUpsamplePS##UpsampleType##bModulateToSceneColor##bSupportSpecularOcclusion,TEXT("/Engine/Private/DistanceFieldLightingPost.usf"),TEXT("AOUpsamplePS"),SF_Pixel);

IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputBentNormal, true, true)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputAO, true, true)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputBentNormalAndIrradiance, true, true)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputIrradiance, true, true)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputBentNormal, false, true)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputAO, false, true)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputBentNormalAndIrradiance, false, true)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputIrradiance, false, true)

IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputBentNormal, true, false)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputAO, true, false)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputBentNormalAndIrradiance, true, false)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputIrradiance, true, false)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputBentNormal, false, false)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputAO, false, false)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputBentNormalAndIrradiance, false, false)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputIrradiance, false, false)

template<bool bSupportSpecularOcclusion>
void SetUpsampleShaders(
	FRHICommandList& RHICmdList,
	FGraphicsPipelineStateInitializer& GraphicsPSOInit,
	const FViewInfo& View,
	TShaderMapRef<FPostProcessVS>& VertexShader,
	TRefCountPtr<IPooledRenderTarget>& DistanceFieldAOBentNormal, 
	TRefCountPtr<IPooledRenderTarget>& DistanceFieldIrradiance,
	bool bModulateSceneColor,
	bool bVisualizeAmbientOcclusion,
	bool bVisualizeGlobalIllumination)
{
	const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View);

	if (bModulateSceneColor)
	{
		if (bVisualizeAmbientOcclusion)
		{
			TShaderMapRef<TDistanceFieldAOUpsamplePS<AOUpsample_OutputAO, true, bSupportSpecularOcclusion> > PixelShader(View.ShaderMap);
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldIrradiance);
		}
		else if (bVisualizeGlobalIllumination && bUseDistanceFieldGI)
		{
			TShaderMapRef<TDistanceFieldAOUpsamplePS<AOUpsample_OutputIrradiance, true, bSupportSpecularOcclusion> > PixelShader(View.ShaderMap);
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldIrradiance);
		}
		else
		{
			if (bUseDistanceFieldGI)
			{
				TShaderMapRef<TDistanceFieldAOUpsamplePS<AOUpsample_OutputBentNormalAndIrradiance, true, bSupportSpecularOcclusion> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldIrradiance);
			}
			else
			{
				TShaderMapRef<TDistanceFieldAOUpsamplePS<AOUpsample_OutputBentNormal, true, bSupportSpecularOcclusion> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldIrradiance);
			}
		}
	}
	else
	{
		if (bVisualizeAmbientOcclusion)
		{
			TShaderMapRef<TDistanceFieldAOUpsamplePS<AOUpsample_OutputAO, false, bSupportSpecularOcclusion> > PixelShader(View.ShaderMap);
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldIrradiance);
		}
		else if (bVisualizeGlobalIllumination && bUseDistanceFieldGI)
		{
			TShaderMapRef<TDistanceFieldAOUpsamplePS<AOUpsample_OutputIrradiance, false, bSupportSpecularOcclusion> > PixelShader(View.ShaderMap);
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldIrradiance);
		}
		else
		{
			if (bUseDistanceFieldGI)
			{
				TShaderMapRef<TDistanceFieldAOUpsamplePS<AOUpsample_OutputBentNormalAndIrradiance, false, bSupportSpecularOcclusion> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldIrradiance);
			}
			else
			{
				TShaderMapRef<TDistanceFieldAOUpsamplePS<AOUpsample_OutputBentNormal, false, bSupportSpecularOcclusion> > PixelShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldIrradiance);
			}
		}
	}
}

void UpsampleBentNormalAO(
	FRHICommandList& RHICmdList, 
	const TArray<FViewInfo>& Views, 
	TRefCountPtr<IPooledRenderTarget>& DistanceFieldAOBentNormal, 
	TRefCountPtr<IPooledRenderTarget>& DistanceFieldIrradiance,
	bool bModulateSceneColor,
	bool bVisualizeAmbientOcclusion,
	bool bVisualizeGlobalIllumination)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		SCOPED_DRAW_EVENT(RHICmdList, UpsampleAO);

		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		if (bModulateSceneColor)
		{
			GraphicsPSOInit.BlendState = TStaticBlendState<
				// Opaque blending to DistanceFieldAOBentNormal
				CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
				// Modulate scene color target
				CW_RGB, BO_Add, BF_DestColor, BF_Zero, BO_Add, BF_Zero, BF_One,
				// Opaque blending to DistanceFieldIrradiance
				CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
		}
		else
		{
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		}

		TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

		SetUpsampleShaders<false>(RHICmdList, GraphicsPSOInit, View, VertexShader, DistanceFieldAOBentNormal, DistanceFieldIrradiance, bModulateSceneColor, bVisualizeAmbientOcclusion, bVisualizeGlobalIllumination);

		DrawRectangle( 
			RHICmdList,
			0, 0, 
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Min.X / GAODownsampleFactor, View.ViewRect.Min.Y / GAODownsampleFactor, 
			View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
			FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
			GetBufferSizeForAO(),
			*VertexShader);
	}
}
