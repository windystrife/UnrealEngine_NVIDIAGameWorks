// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTestImage.cpp: Post processing TestImage implementation.
=============================================================================*/

#include "PostProcess/PostProcessTestImage.h"
#include "StaticBoundShaderState.h"
#include "CanvasTypes.h"
#include "UnrealEngine.h"
#include "RenderTargetTemp.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/SceneFilterRendering.h"
#include "SceneRenderTargetParameters.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessCombineLUTs.h"
#include "PipelineStateCache.h"

/** Encapsulates the post processing eye adaptation pixel shader. */
class FPostProcessTestImagePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessTestImagePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	/** Default constructor. */
	FPostProcessTestImagePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter FrameNumber;
	FShaderParameter FrameTime;
	FColorRemapShaderParameters ColorRemapShaderParameters;

	/** Initialization constructor. */
	FPostProcessTestImagePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
		, ColorRemapShaderParameters(Initializer.ParameterMap)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		FrameNumber.Bind(Initializer.ParameterMap,TEXT("FrameNumber"));
		FrameTime.Bind(Initializer.ParameterMap,TEXT("FrameTime"));
	}

	template <typename TRHICmdList>
	void SetPS(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		DeferredParameters.Set(RHICmdList, ShaderRHI, Context.View, MD_PostProcess);

		{
			uint32 FrameNumberValue = Context.View.Family->FrameNumber;
			SetShaderValue(RHICmdList, ShaderRHI, FrameNumber, FrameNumberValue);
		}

		{
			float FrameTimeValue = Context.View.Family->CurrentRealTime;
			SetShaderValue(RHICmdList, ShaderRHI, FrameTime, FrameTimeValue);
		}

		ColorRemapShaderParameters.Set(RHICmdList, ShaderRHI);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << FrameNumber << FrameTime << ColorRemapShaderParameters;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessTestImagePS,TEXT("/Engine/Private/PostProcessTestImage.usf"),TEXT("MainPS"),SF_Pixel);

FRCPassPostProcessTestImage::FRCPassPostProcessTestImage()
{
}

void FRCPassPostProcessTestImage::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, TestImage);

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	
	FIntRect SrcRect = View.UnscaledViewRect;
	FIntRect DestRect = View.UnscaledViewRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
	Context.SetViewportAndCallRHI(DestRect);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessTestImagePS> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	PixelShader->SetPS(Context.RHICmdList, Context);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY(),
		*VertexShader,
		EDRF_UseTriangleOptimization);

	{
		FRenderTargetTemp TempRenderTarget(View, (const FTexture2DRHIRef&)DestRenderTarget.TargetableTexture);
		FCanvas Canvas(&TempRenderTarget, NULL, ViewFamily.CurrentRealTime, ViewFamily.CurrentWorldTime, ViewFamily.DeltaWorldTime, Context.GetFeatureLevel());

		float X = 30;
		float Y = 8;
		const float YStep = 14;
		const float ColumnWidth = 250;

		FString Line;

		Line = FString::Printf(TEXT("Top bars:"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   Moving bars using FrameTime"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   Black and white raster, Pixel sized, Watch for Moire pattern"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   Black and white raster, 2x2 block sized"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("Bottom bars:"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   8 bars near white, 4 right bars should appear as one (HDTV)"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   8 bars near black, 4 left bars should appear as one (HDTV)"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   Linear Greyscale in sRGB from 0 to 255"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("Color bars:"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   Red, Green, Blue"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("Outside:"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   Moving bars using FrameNumber, Tearing without VSync"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("Circles:"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   Should be round and centered"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("Border:"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   4 white pixel sized lines (only visible without overscan)"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));

		Canvas.Flush_RenderThread(Context.RHICmdList);
	}


	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessTestImage::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret(FPooledRenderTargetDesc::Create2DDesc(FSceneRenderTargets::Get_FrameConstantsOnly().GetBufferSizeXY(), PF_B8G8R8A8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));

	Ret.DebugName = TEXT("TestImage");

	return Ret;
}
