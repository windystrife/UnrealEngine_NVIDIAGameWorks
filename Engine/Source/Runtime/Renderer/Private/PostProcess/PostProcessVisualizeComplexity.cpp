// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PostProcessVisualizeComplexity.cpp: Contains definitions for complexity viewmode.
=============================================================================*/

#include "PostProcess/PostProcessVisualizeComplexity.h"
#include "EngineGlobals.h"
#include "StaticBoundShaderState.h"
#include "CanvasTypes.h"
#include "UnrealEngine.h"
#include "RenderTargetTemp.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "PipelineStateCache.h"

/**
 * Gets the maximum shader complexity count from the ini settings.
 */
float GetMaxShaderComplexityCount(ERHIFeatureLevel::Type InFeatureType)
{
	return InFeatureType == ERHIFeatureLevel::ES2 ? GEngine->MaxES2PixelShaderAdditiveComplexityCount : GEngine->MaxPixelShaderAdditiveComplexityCount;
}


/** 
* Constructor - binds all shader params
* @param Initializer - init data from shader compiler
*/
FVisualizeComplexityApplyPS::FVisualizeComplexityApplyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader(Initializer)
{
	PostprocessParameter.Bind(Initializer.ParameterMap);
	ShaderComplexityColors.Bind(Initializer.ParameterMap,TEXT("ShaderComplexityColors"));
	MiniFontTexture.Bind(Initializer.ParameterMap, TEXT("MiniFontTexture"));
	ShaderComplexityParams.Bind(Initializer.ParameterMap, TEXT("ShaderComplexityParams"));
	ShaderComplexityParams2.Bind(Initializer.ParameterMap, TEXT("ShaderComplexityParams2"));
	QuadOverdrawTexture.Bind(Initializer.ParameterMap,TEXT("QuadOverdrawTexture"));
}

template <typename TRHICmdList>
void FVisualizeComplexityApplyPS::SetParameters(
	TRHICmdList& RHICmdList,
	const FRenderingCompositePassContext& Context,
	const TArray<FLinearColor>& Colors,
	EColorSampling ColorSampling,
	float ComplexityScale,
	bool bLegend
	)
{
	const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

	PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

	int32 NumColors = FMath::Min<int32>(Colors.Num(), MaxNumShaderComplexityColors);
	if (NumColors > 0)
	{	//pass the complexity -> color mapping into the pixel shader
		for (int32 ColorIndex = 0; ColorIndex < NumColors; ColorIndex++)
		{
			SetShaderValue(RHICmdList, ShaderRHI, ShaderComplexityColors, Colors[ColorIndex], ColorIndex);
		}
	}
	else // Otherwise fallback to a safe value.
	{
		NumColors = 1;
		SetShaderValue(RHICmdList, ShaderRHI, ShaderComplexityColors, FLinearColor::Gray, 0);
	}

	SetTextureParameter(RHICmdList, ShaderRHI, MiniFontTexture, GEngine->MiniFontTexture ? GEngine->MiniFontTexture->Resource->TextureRHI : GSystemTextures.WhiteDummy->GetRenderTargetItem().TargetableTexture);

	// Whether acccess or not the QuadOverdraw buffer.
	EDebugViewShaderMode DebugViewShaderMode = Context.View.Family->GetDebugViewShaderMode();
	if (QuadOverdrawTexture.IsBound())
	{
		const FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
		if (SceneContext.QuadOverdrawBuffer.IsValid() && SceneContext.QuadOverdrawBuffer->GetRenderTargetItem().ShaderResourceTexture.IsValid())
		{
			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, SceneContext.QuadOverdrawBuffer->GetRenderTargetItem().UAV);
			SetTextureParameter(RHICmdList, ShaderRHI, QuadOverdrawTexture, SceneContext.QuadOverdrawBuffer->GetRenderTargetItem().ShaderResourceTexture);
		}
		else // Otherwise fallback to a complexity mode that does not require the QuadOverdraw resources.
		{
			SetTextureParameter(RHICmdList, ShaderRHI, QuadOverdrawTexture, FTextureRHIRef());
			DebugViewShaderMode = DVSM_ShaderComplexity;
		}
	}

	FIntPoint UsedQuadBufferSize = (Context.View.ViewRect.Size() + FIntPoint(1, 1)) / 2;
	SetShaderValue(RHICmdList, ShaderRHI, ShaderComplexityParams, FVector4(bLegend, DebugViewShaderMode, ColorSampling, ComplexityScale));
	SetShaderValue(RHICmdList, ShaderRHI, ShaderComplexityParams2, FVector4((float)Colors.Num(), 0, (float)UsedQuadBufferSize.X, (float)UsedQuadBufferSize.Y));
}

IMPLEMENT_SHADER_TYPE(,FVisualizeComplexityApplyPS,TEXT("/Engine/Private/ShaderComplexityApplyPixelShader.usf"),TEXT("Main"),SF_Pixel);

//reuse the generic filter vertex declaration
extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

void FRCPassPostProcessVisualizeComplexity::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessVisualizeComplexity);
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	
	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = View.ViewRect;
	FIntPoint SrcSize = InputDesc->Extent;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
	Context.SetViewportAndCallRHI(DestRect);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// turn off culling and blending
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

	// turn off depth reads/writes
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	//reuse this generic vertex shader
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FVisualizeComplexityApplyPS> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	PixelShader->SetParameters(Context.RHICmdList, Context, Colors, ColorSampling, ComplexityScale, bLegend);
	
	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	if(bLegend)
	{
		FRenderTargetTemp TempRenderTarget(View, (const FTexture2DRHIRef&)DestRenderTarget.TargetableTexture);
		FCanvas Canvas(&TempRenderTarget, NULL, ViewFamily.CurrentRealTime, ViewFamily.CurrentWorldTime, ViewFamily.DeltaWorldTime, Context.GetFeatureLevel());

//later?		Canvas.DrawShadowedString(DestRect.Max.X - DestRect.Width() / 3 - 64 + 8, DestRect.Max.Y - 80, TEXT("Overdraw"), GetStatsFont(), FLinearColor(0.7f, 0.7f, 0.7f), FLinearColor(0,0,0,0));
//later?		Canvas.DrawShadowedString(DestRect.Min.X + 64 + 4, DestRect.Max.Y - 80, TEXT("VS Instructions"), GetStatsFont(), FLinearColor(0.0f, 0.0f, 0.0f), FLinearColor(0,0,0,0));

		if (View.Family->GetDebugViewShaderMode() == DVSM_QuadComplexity)
		{
			int32 StartX = DestRect.Min.X + 62;
			int32 EndX = DestRect.Max.X - 66;
			int32 NumOffset = (EndX - StartX) / (Colors.Num() - 1);
			for (int32 PosX = StartX, Number = 0; PosX <= EndX; PosX += NumOffset, ++Number)
			{
				FString Line;
				Line = FString::Printf(TEXT("%d"), Number);
				Canvas.DrawShadowedString(PosX, DestRect.Max.Y - 87, *Line, GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));
			}
		}
		else
		{
			Canvas.DrawShadowedString(DestRect.Min.X + 63, DestRect.Max.Y - 51, TEXT("Good"), GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));
			Canvas.DrawShadowedString(DestRect.Min.X + 63 + (int32)(DestRect.Width() * 107.0f / 397.0f), DestRect.Max.Y - 51, TEXT("Bad"), GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));
			Canvas.DrawShadowedString(DestRect.Max.X - 162, DestRect.Max.Y - 51, TEXT("Extremely bad"), GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));

			Canvas.DrawShadowedString(DestRect.Min.X + 62, DestRect.Max.Y - 87, TEXT("0"), GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));

			FString Line;
			Line = FString::Printf(TEXT("MaxShaderComplexityCount=%d"), (int32)GetMaxShaderComplexityCount(Context.GetFeatureLevel()));
			Canvas.DrawShadowedString(DestRect.Max.X - 260, DestRect.Max.Y - 88, *Line, GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));
		}

		Canvas.Flush_RenderThread(Context.RHICmdList);
	}
	
	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessVisualizeComplexity::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("VisualizeComplexity");

	return Ret;
}
