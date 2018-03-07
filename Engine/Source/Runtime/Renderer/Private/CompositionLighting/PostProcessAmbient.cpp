// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessAmbient.cpp: Post processing ambient implementation.
=============================================================================*/

#include "CompositionLighting/PostProcessAmbient.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneRenderTargetParameters.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "AmbientCubemapParameters.h"
#include "PipelineStateCache.h"

/** Encapsulates the post processing ambient pixel shader. */
class FPostProcessAmbientPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessAmbientPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}	

	/** Default constructor. */
	FPostProcessAmbientPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FCubemapShaderParameters CubemapShaderParameters;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter PreIntegratedGF;
	FShaderResourceParameter PreIntegratedGFSampler;

	/** Initialization constructor. */
	FPostProcessAmbientPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		CubemapShaderParameters.Bind(Initializer.ParameterMap);
		PreIntegratedGF.Bind(Initializer.ParameterMap, TEXT("PreIntegratedGF"));
		PreIntegratedGFSampler.Bind(Initializer.ParameterMap, TEXT("PreIntegratedGFSampler"));
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, const FFinalPostProcessSettings::FCubemapEntry& Entry)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		DeferredParameters.Set(RHICmdList, ShaderRHI, Context.View, MD_PostProcess);
		CubemapShaderParameters.SetParameters(RHICmdList, ShaderRHI, Entry);
		SetTextureParameter(RHICmdList, ShaderRHI, PreIntegratedGF, PreIntegratedGFSampler, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), GSystemTextures.PreintegratedGF->GetRenderTargetItem().ShaderResourceTexture );
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << CubemapShaderParameters << DeferredParameters << PreIntegratedGF << PreIntegratedGFSampler;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(, FPostProcessAmbientPS, TEXT("/Engine/Private/PostProcessAmbient.usf"), TEXT("MainPS"), SF_Pixel);

void FRCPassPostProcessAmbient::Render(FRenderingCompositePassContext& Context, FGraphicsPipelineStateInitializer& GraphicsPSOInit)
{
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessAmbientPS> PixelShader(Context.GetShaderMap());
	const FSceneView& View = Context.View;

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	uint32 Count = Context.View.FinalPostProcessSettings.ContributingCubemaps.Num();
	for (uint32 i = 0; i < Count; ++i)
	{
		PixelShader->SetParameters(Context.RHICmdList, Context, Context.View.FinalPostProcessSettings.ContributingCubemaps[i]);

		DrawPostProcessPass(
			Context.RHICmdList,
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Size(),
			FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY(),
			*VertexShader,
			View.StereoPass, 
			Context.HasHmdMesh(),
			EDRF_UseTriangleOptimization);
	}
}

void FRCPassPostProcessAmbient::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessAmbient);

	const FViewInfo& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntRect SrcRect = View.ViewRect;
	// todo: view size should scale with input texture size so we can do SSAO in half resolution as well
	FIntRect DestRect = View.ViewRect;
	FIntPoint DestSize = DestRect.Size();

	const FSceneRenderTargetItem& DestRenderTarget = FSceneRenderTargets::Get(Context.RHICmdList).GetSceneColor()->GetRenderTargetItem();

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef(), true);
	Context.SetViewportAndCallRHI(View.ViewRect);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// set the state
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	FScene* Scene = ViewFamily.Scene->GetRenderScene();
	check(Scene);
	uint32 NumReflectionCaptures = Scene->ReflectionSceneData.RegisteredReflectionCaptures.Num();
	
	// Ambient cubemap specular will be applied in the reflection environment pass if it is enabled
	//const bool bApplySpecular = View.Family->EngineShowFlags.ReflectionEnvironment == 0 || NumReflectionCaptures == 0;
	//bool bClearCoatNeeded = (View.ShadingModelMaskInView & (1 << MSM_ClearCoat)) != 0;

	Render(Context, GraphicsPSOInit);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessAmbient::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	// we assume this pass is additively blended with the scene color so this data is not needed
	FPooledRenderTargetDesc Ret;

	Ret.DebugName = TEXT("AmbientCubeMap");

	return Ret;
}

/*-----------------------------------------------------------------------------
FCubemapShaderParameters
-----------------------------------------------------------------------------*/

void FCubemapShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	AmbientCubemapColor.Bind(ParameterMap, TEXT("AmbientCubemapColor"));
	AmbientCubemapMipAdjust.Bind(ParameterMap, TEXT("AmbientCubemapMipAdjust"));
	AmbientCubemap.Bind(ParameterMap, TEXT("AmbientCubemap"));
	AmbientCubemapSampler.Bind(ParameterMap, TEXT("AmbientCubemapSampler"));
}

void FCubemapShaderParameters::SetParameters(FRHICommandList& RHICmdList, const FPixelShaderRHIParamRef ShaderRHI, const FFinalPostProcessSettings::FCubemapEntry& Entry) const
{
	SetParametersTemplate(RHICmdList, ShaderRHI, Entry);
}

void FCubemapShaderParameters::SetParameters(FRHICommandList& RHICmdList, const FComputeShaderRHIParamRef ShaderRHI, const FFinalPostProcessSettings::FCubemapEntry& Entry) const
{
	SetParametersTemplate(RHICmdList, ShaderRHI, Entry);
}

template<typename TShaderRHIRef>
void FCubemapShaderParameters::SetParametersTemplate(FRHICommandList& RHICmdList, const TShaderRHIRef& ShaderRHI, const FFinalPostProcessSettings::FCubemapEntry& Entry) const
{
	// floats to render the cubemap
	{
		float MipCount = 0;

		if(Entry.AmbientCubemap)
		{
			int32 CubemapWidth = Entry.AmbientCubemap->GetSurfaceWidth();
			MipCount = FMath::Log2(CubemapWidth) + 1.0f;
		}

		{
			FLinearColor AmbientCubemapTintMulScaleValue = Entry.AmbientCubemapTintMulScaleValue;

			SetShaderValue(RHICmdList, ShaderRHI, AmbientCubemapColor, AmbientCubemapTintMulScaleValue);
		}

		{
			FVector4 AmbientCubemapMipAdjustValue(0, 0, 0, 0);

			AmbientCubemapMipAdjustValue.X =  1.0f - GDiffuseConvolveMipLevel / MipCount;
			AmbientCubemapMipAdjustValue.Y = (MipCount - 1.0f) * AmbientCubemapMipAdjustValue.X;
			AmbientCubemapMipAdjustValue.Z = MipCount - GDiffuseConvolveMipLevel;
			AmbientCubemapMipAdjustValue.W = MipCount;

			SetShaderValue(RHICmdList, ShaderRHI, AmbientCubemapMipAdjust, AmbientCubemapMipAdjustValue);
		}
	}

	// cubemap texture
	{
		FTexture* InternalSpec = Entry.AmbientCubemap ? Entry.AmbientCubemap->Resource : GBlackTextureCube;

		SetTextureParameter(RHICmdList, ShaderRHI, AmbientCubemap, AmbientCubemapSampler, InternalSpec);
	}
}

FArchive& operator<<(FArchive& Ar, FCubemapShaderParameters& P)
{
	Ar << P.AmbientCubemapColor << P.AmbientCubemap << P.AmbientCubemapSampler << P.AmbientCubemapMipAdjust;

	return Ar;
}
