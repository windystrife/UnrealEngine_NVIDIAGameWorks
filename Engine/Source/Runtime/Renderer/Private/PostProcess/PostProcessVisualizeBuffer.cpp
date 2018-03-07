// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessVisualizeBuffer.cpp: Post processing VisualizeBuffer implementation.
=============================================================================*/

#include "PostProcess/PostProcessVisualizeBuffer.h"
#include "StaticBoundShaderState.h"
#include "CanvasTypes.h"
#include "UnrealEngine.h"
#include "RenderTargetTemp.h"
#include "SceneUtils.h"
#include "PostProcess/SceneFilterRendering.h"
#include "SceneRenderTargetParameters.h"
#include "PostProcess/PostProcessing.h"
#include "PipelineStateCache.h"

/** Encapsulates the post processing Buffer visualization pixel shader. */
template<bool bDrawingTile>
class FPostProcessVisualizeBufferPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessVisualizeBufferPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES3_1);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("DRAWING_TILE"), bDrawingTile);
	}

	/** Default constructor. */
	FPostProcessVisualizeBufferPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter SourceTexture;
	FShaderResourceParameter SourceTextureSampler;

	/** Initialization constructor. */
	FPostProcessVisualizeBufferPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);

		if (bDrawingTile)
		{
			SourceTexture.Bind(Initializer.ParameterMap, TEXT("PostprocessInput0"));
			SourceTextureSampler.Bind(Initializer.ParameterMap, TEXT("PostprocessInput0Sampler"));
		}
	}

	template <typename TRHICmdList>
	void SetPS(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		DeferredParameters.Set(RHICmdList, ShaderRHI, Context.View, MD_PostProcess);
	}

	void SetSourceTexture(FRHICommandList& RHICmdList, FTextureRHIRef Texture)
	{
		if (bDrawingTile && SourceTexture.IsBound())
		{
			const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				SourceTexture,
				SourceTextureSampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				Texture);
		}
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << SourceTexture << SourceTextureSampler;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/PostProcessVisualizeBuffer.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS");
	}
};

void FRCPassPostProcessVisualizeBuffer::AddVisualizationBuffer(FRenderingCompositeOutputRef InSource, const FString& InName)
{
	Tiles.Add(TileData(InSource, InName));

	if (InSource.IsValid())
	{
		AddDependency(InSource);
	}
}

IMPLEMENT_SHADER_TYPE2(FPostProcessVisualizeBufferPS<true>, SF_Pixel);
IMPLEMENT_SHADER_TYPE2(FPostProcessVisualizeBufferPS<false>, SF_Pixel);

template <bool bDrawingTile>
FShader* FRCPassPostProcessVisualizeBuffer::SetShaderTempl(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessVisualizeBufferPS<bDrawingTile> > PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	PixelShader->SetPS(Context.RHICmdList, Context);

	return *VertexShader;
}

void FRCPassPostProcessVisualizeBuffer::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, VisualizeBuffer);
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

	{
		FShader* VertexShader = SetShaderTempl<false>(Context);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		SrcSize,
			VertexShader,
		EDRF_UseTriangleOptimization);
	}

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessVisualizeBufferPS<true> > PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	PixelShader->SetPS(Context.RHICmdList, Context);

	// Track the name and position of each tile we draw so we can write text labels over them
	struct LabelRecord
	{
		FString Label;
		int32 LocationX;
		int32 LocationY;
	};

	TArray<LabelRecord> Labels;

	const int32 MaxTilesX = 4;
	const int32 MaxTilesY = 4;
	const int32 TileWidth = DestRect.Width() / MaxTilesX;
	const int32 TileHeight = DestRect.Height() / MaxTilesY;
	int32 CurrentTileIndex = 0; 

	for (TArray<TileData>::TConstIterator It = Tiles.CreateConstIterator(); It; ++It, ++CurrentTileIndex)
	{
		FRenderingCompositeOutputRef Tile = It->Source;

		if (Tile.IsValid())
		{
			FTextureRHIRef Texture = Tile.GetOutput()->PooledRenderTarget->GetRenderTargetItem().TargetableTexture;

			int32 TileX = CurrentTileIndex % MaxTilesX;
			int32 TileY = CurrentTileIndex / MaxTilesX;

			PixelShader->SetSourceTexture(Context.RHICmdList, Texture);

			DrawRectangle(
				Context.RHICmdList,
				TileX * TileWidth, TileY * TileHeight,
				TileWidth, TileHeight,
				SrcRect.Min.X, SrcRect.Min.Y,
				SrcRect.Width(), SrcRect.Height(),
				DestRect.Size(),
				SrcSize,
				*VertexShader,
				EDRF_Default
				);

			Labels.Add(LabelRecord());
			Labels.Last().Label = It->Name;
			Labels.Last().LocationX = 8 + TileX * TileWidth;
			Labels.Last().LocationY = (TileY + 1) * TileHeight - 19;
		}
	}

	// Draw tile labels

	FRenderTargetTemp TempRenderTarget(View, (const FTexture2DRHIRef&)DestRenderTarget.TargetableTexture);
	FCanvas Canvas(&TempRenderTarget, NULL, ViewFamily.CurrentRealTime, ViewFamily.CurrentWorldTime, ViewFamily.DeltaWorldTime, Context.GetFeatureLevel());
	FLinearColor LabelColor(1, 1, 0);
	for (auto It = Labels.CreateConstIterator(); It; ++It)
	{
		Canvas.DrawShadowedString(It->LocationX, It->LocationY, *It->Label, GetStatsFont(), LabelColor);
	}
	Canvas.Flush_RenderThread(Context.RHICmdList);


	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessVisualizeBuffer::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("VisualizeBuffer");

	return Ret;
}
