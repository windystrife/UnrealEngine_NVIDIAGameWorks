// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessLensFlares.cpp: Post processing lens fares implementation.
=============================================================================*/

#include "PostProcess/PostProcessLensFlares.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"

/** Encapsulates a simple copy pixel shader. */
template <bool bClearRegion = false>
class TPostProcessLensFlareBasePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TPostProcessLensFlareBasePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	TPostProcessLensFlareBasePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter CompositeBloomParameter;

	/** Initialization constructor. */
	TPostProcessLensFlareBasePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	// FShader interface.
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{

		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		if (bClearRegion)
		{
			OutEnvironment.SetDefine(TEXT("CLEAR_REGION"), 1);
		}
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
	}
};

#define IMPLEMENT_LENSE_FLARE_BASE(_bClearRegion) \
typedef TPostProcessLensFlareBasePS< _bClearRegion > FPostProcessLensFlareBasePS##_bClearRegion ;\
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessLensFlareBasePS##_bClearRegion ,TEXT("/Engine/Private/PostProcessLensFlares.usf"),TEXT("CopyPS"),SF_Pixel);

IMPLEMENT_LENSE_FLARE_BASE(true)
IMPLEMENT_LENSE_FLARE_BASE(false)

#undef IMPLEMENT_LENSE_FLARE_BASE


/** Encapsulates the post processing lens flare pixel shader. */
class FPostProcessLensFlaresPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessLensFlaresPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FPostProcessLensFlaresPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter FlareColor;
	FShaderParameter TexScale;

	/** Initialization constructor. */
	FPostProcessLensFlaresPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		FlareColor.Bind(Initializer.ParameterMap, TEXT("FlareColor"));
		TexScale.Bind(Initializer.ParameterMap, TEXT("TexScale"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << FlareColor << TexScale;
		return bShaderHasOutdatedParameters;
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, FVector2D TexScaleValue)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		SetShaderValue(RHICmdList, ShaderRHI, TexScale, TexScaleValue);
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessLensFlaresPS,TEXT("/Engine/Private/PostProcessLensFlares.usf"),TEXT("MainPS"),SF_Pixel);



FRCPassPostProcessLensFlares::FRCPassPostProcessLensFlares(float InSizeScale, bool InbCompositeBloom)
	: SizeScale(InSizeScale), bCompositeBloom(InbCompositeBloom)
{
}

void FRCPassPostProcessLensFlares::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, LensFlares);

	const FPooledRenderTargetDesc* InputDesc1 = GetInputDesc(ePId_Input0);
	const FPooledRenderTargetDesc* InputDesc2 = GetInputDesc(ePId_Input1);
	
	if(!InputDesc1 || !InputDesc2)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint TexSize1 = InputDesc1->Extent;
	FIntPoint TexSize2 = InputDesc2->Extent;

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
	uint32 ScaleToFullRes1 = SceneContext.GetBufferSizeXY().X / TexSize1.X;
	uint32 ScaleToFullRes2 = SceneContext.GetBufferSizeXY().X / TexSize2.X;

	FIntRect ViewRect1 = FIntRect::DivideAndRoundUp(View.ViewRect, ScaleToFullRes1);
	FIntRect ViewRect2 = FIntRect::DivideAndRoundUp(View.ViewRect, ScaleToFullRes2);

	FIntPoint ViewSize1 = ViewRect1.Size();
	FIntPoint ViewSize2 = ViewRect2.Size();

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		
	if (Context.HasHmdMesh() && View.StereoPass == eSSP_LEFT_EYE)
	{
		DrawClearQuad(Context.RHICmdList, true, FLinearColor::Black, false, 0, false, 0);
	}
	else
	{
		// is optimized away if possible (RT size=view size, )
		DrawClearQuad(Context.RHICmdList, true, FLinearColor::Black, false, 0, false, 0, PassOutputs[0].RenderTargetDesc.Extent, ViewRect1);
	}

	Context.SetViewportAndCallRHI(ViewRect1);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

	
	// setup background (bloom), can be implemented to use additive blending to avoid the read here
	if (bCompositeBloom)
	{
		TShaderMapRef<TPostProcessLensFlareBasePS<false>> PixelShader(Context.GetShaderMap());

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		VertexShader->SetParameters(Context);
		PixelShader->SetParameters(Context.RHICmdList, Context);

		// Draw a quad mapping scene color to the view's render target
		DrawRectangle(
			Context.RHICmdList,
			0, 0,
			ViewSize1.X, ViewSize1.Y,
			ViewRect1.Min.X, ViewRect1.Min.Y,
			ViewSize1.X, ViewSize1.Y,
			ViewSize1,
			TexSize1,
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}
	else
	{
		TShaderMapRef<TPostProcessLensFlareBasePS<true>> PixelShader(Context.GetShaderMap());

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		VertexShader->SetParameters(Context);
		PixelShader->SetParameters(Context.RHICmdList, Context);

		// Draw a quad mapping scene color to the view's render target
		DrawRectangle(
			Context.RHICmdList,
			0, 0,
			ViewSize1.X, ViewSize1.Y,
			ViewRect1.Min.X, ViewRect1.Min.Y,
			ViewSize1.X, ViewSize1.Y,
			ViewSize1,
			TexSize1,
			*VertexShader,
			EDRF_UseTriangleOptimization);

	}

	// additive blend
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();

	// add lens flares on top of that
	{
		TShaderMapRef<FPostProcessLensFlaresPS> PixelShader(Context.GetShaderMap());

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		FVector2D TexScaleValue = FVector2D(TexSize2) / ViewSize2;

		VertexShader->SetParameters(Context);
		PixelShader->SetParameters(Context.RHICmdList, Context, TexScaleValue);

		// todo: expose
		const uint32 Count = 8;

		// we assume the center of the view is the center of the lens (would not be correct for tiled rendering)
		FVector2D Center = FVector2D(ViewSize1) * 0.5f;

		FLinearColor LensFlareHDRColor = Context.View.FinalPostProcessSettings.LensFlareTint * Context.View.FinalPostProcessSettings.LensFlareIntensity;
	
		// to get the same brightness with 4x more quads (TileSize=1 in LensBlur)
		LensFlareHDRColor.R *= 0.25f;
		LensFlareHDRColor.G *= 0.25f;
		LensFlareHDRColor.B *= 0.25f;

		for(uint32 i = 0; i < Count; ++i)
		{
			FLinearColor FlareColor = Context.View.FinalPostProcessSettings.LensFlareTints[i % 8];
			float NormalizedAlpha = FlareColor.A;
			float Alpha = NormalizedAlpha * 7.0f - 3.5f; 

			// scale to blur outside of the view (only if we use LensBlur)
			Alpha *= SizeScale;
			
			// set the individual flare color
			SetShaderValue(Context.RHICmdList, PixelShader->GetPixelShader(), PixelShader->FlareColor, FlareColor * LensFlareHDRColor);

			// Draw a quad mapping scene color to the view's render target
			DrawRectangle(
				Context.RHICmdList,
				Center.X - 0.5f * ViewSize1.X * Alpha, Center.Y - 0.5f * ViewSize1.Y * Alpha,
				ViewSize1.X * Alpha, ViewSize1.Y * Alpha,
				ViewRect2.Min.X, ViewRect2.Min.Y,
				ViewSize2.X, ViewSize2.Y,
				ViewSize1,
				TexSize2,
				*VertexShader,
				EDRF_Default);
		}
	}

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessLensFlares::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("LensFlares");

	return Ret;
}
