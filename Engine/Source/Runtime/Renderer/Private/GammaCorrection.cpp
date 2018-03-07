// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GammaCorrection.cpp
=============================================================================*/

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "SceneRendering.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"

/** Encapsulates the gamma correction pixel shader. */
class FGammaCorrectionPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGammaCorrectionPS,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	/** Default constructor. */
	FGammaCorrectionPS() {}

public:

	FShaderResourceParameter SceneTexture;
	FShaderResourceParameter SceneTextureSampler;
	FShaderParameter InverseGamma;
	FShaderParameter ColorScale;
	FShaderParameter OverlayColor;

	/** Initialization constructor. */
	FGammaCorrectionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneTexture.Bind(Initializer.ParameterMap,TEXT("SceneColorTexture"));
		SceneTextureSampler.Bind(Initializer.ParameterMap,TEXT("SceneColorTextureSampler"));
		InverseGamma.Bind(Initializer.ParameterMap,TEXT("InverseGamma"));
		ColorScale.Bind(Initializer.ParameterMap,TEXT("ColorScale"));
		OverlayColor.Bind(Initializer.ParameterMap,TEXT("OverlayColor"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		
		Ar << SceneTexture << SceneTextureSampler << InverseGamma << ColorScale << OverlayColor;
		return bShaderHasOutdatedParameters;
	}
};

/** Encapsulates the gamma correction vertex shader. */
class FGammaCorrectionVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGammaCorrectionVS,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	/** Default constructor. */
	FGammaCorrectionVS() {}

public:

	/** Initialization constructor. */
	FGammaCorrectionVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
	}
};

IMPLEMENT_SHADER_TYPE(,FGammaCorrectionPS,TEXT("/Engine/Private/GammaCorrection.usf"),TEXT("MainPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FGammaCorrectionVS,TEXT("/Engine/Private/GammaCorrection.usf"),TEXT("MainVS"),SF_Vertex);

// TODO: REMOVE if no longer needed:
void FSceneRenderer::GammaCorrectToViewportRenderTarget(FRHICommandList& RHICmdList, const FViewInfo* View, float OverrideGamma)
{
	// Set the view family's render target/viewport.
	SetRenderTarget(RHICmdList, ViewFamily.RenderTarget->GetRenderTargetTexture(), FTextureRHIRef());

	// Deferred the clear until here so the garbage left in the non rendered regions by the post process effects do not show up
	if( ViewFamily.bDeferClear )
	{
		if (ensure(ViewFamily.RenderTarget->GetRenderTargetTexture()->GetClearColor() == FLinearColor::Black))
		{
			FRHIRenderTargetView RtView = FRHIRenderTargetView(ViewFamily.RenderTarget->GetRenderTargetTexture(), ERenderTargetLoadAction::EClear);
			FRHISetRenderTargetsInfo Info(1, &RtView, FRHIDepthRenderTargetView());
			RHICmdList.SetRenderTargetsAndClear(Info);
		}
		else
		{
			SetRenderTarget(RHICmdList, ViewFamily.RenderTarget->GetRenderTargetTexture(), FTextureRHIRef());
			DrawClearQuad(RHICmdList, FLinearColor::Black);
		}
		ViewFamily.bDeferClear = false;
	}
	else
	{
		SetRenderTarget(RHICmdList, ViewFamily.RenderTarget->GetRenderTargetTexture(), FTextureRHIRef());
	}

	SCOPED_DRAW_EVENT(RHICmdList, GammaCorrection);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// turn off culling and blending
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

	// turn off depth reads/writes
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FGammaCorrectionVS> VertexShader(View->ShaderMap);
	TShaderMapRef<FGammaCorrectionPS> PixelShader(View->ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	float InvDisplayGamma = 1.0f / ViewFamily.RenderTarget->GetDisplayGamma();

	if (OverrideGamma != 0)
	{
		InvDisplayGamma = 1 / OverrideGamma;
	}

	const FPixelShaderRHIParamRef ShaderRHI = PixelShader->GetPixelShader();

	SetShaderValue(
		RHICmdList, 
		ShaderRHI,
		PixelShader->InverseGamma,
		InvDisplayGamma
		);
	SetShaderValue(RHICmdList, ShaderRHI,PixelShader->ColorScale,View->ColorScale);
	SetShaderValue(RHICmdList, ShaderRHI,PixelShader->OverlayColor,View->OverlayColor);
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	const FTextureRHIRef DesiredSceneColorTexture = SceneContext.GetSceneColorTexture();

	SetTextureParameter(
		RHICmdList, 
		ShaderRHI,
		PixelShader->SceneTexture,
		PixelShader->SceneTextureSampler,
		TStaticSamplerState<SF_Bilinear>::GetRHI(),
		DesiredSceneColorTexture
		);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		RHICmdList,
		View->UnscaledViewRect.Min.X,View->UnscaledViewRect.Min.Y,
		View->UnscaledViewRect.Width(),View->UnscaledViewRect.Height(),
		View->ViewRect.Min.X,View->ViewRect.Min.Y,
		View->ViewRect.Width(),View->ViewRect.Height(),
		ViewFamily.RenderTarget->GetSizeXY(),
		SceneContext.GetBufferSizeXY(),
		*VertexShader,
		EDRF_UseTriangleOptimization);
}
