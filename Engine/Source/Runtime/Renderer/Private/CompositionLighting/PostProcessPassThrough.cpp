// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessPassThrough.cpp: Post processing pass through implementation.
=============================================================================*/

#include "CompositionLighting/PostProcessPassThrough.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "PipelineStateCache.h"

FPostProcessPassThroughPS::FPostProcessPassThroughPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader(Initializer)
{
	PostprocessParameter.Bind(Initializer.ParameterMap);
}

bool FPostProcessPassThroughPS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << PostprocessParameter;
	return bShaderHasOutdatedParameters;
}

template <typename TRHICmdList>
void FPostProcessPassThroughPS::SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
{
	const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

	PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
}

template void FPostProcessPassThroughPS::SetParameters<FRHICommandListImmediate>(FRHICommandListImmediate& RHICmdList, const FRenderingCompositePassContext& Context);


IMPLEMENT_SHADER_TYPE(,FPostProcessPassThroughPS,TEXT("/Engine/Private/PostProcessPassThrough.usf"),TEXT("MainPS"),SF_Pixel);


FRCPassPostProcessPassThrough::FRCPassPostProcessPassThrough(IPooledRenderTarget* InDest, bool bInAdditiveBlend)
	: Dest(InDest)
	, bAdditiveBlend(bInAdditiveBlend)
{
}

FRCPassPostProcessPassThrough::FRCPassPostProcessPassThrough(FPooledRenderTargetDesc InNewDesc)
	: Dest(0)
	, bAdditiveBlend(false)
	, NewDesc(InNewDesc)
{
}

void FRCPassPostProcessPassThrough::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PassThrough);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	// May need to wait on the inputs to complete
	WaitForInputPassComputeFences(Context.RHICmdList);

	const FSceneView& View = Context.View;

	FIntPoint TexSize = InputDesc->Extent;

	// we assume the input and output is full resolution

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = Dest ? Dest->GetDesc().Extent : PassOutputs[0].RenderTargetDesc.Extent;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 InputScaleFactor = SceneContext.GetBufferSizeXY().X / SrcSize.X;
	uint32 OutputScaleFactor = SceneContext.GetBufferSizeXY().X / DestSize.X;

	FIntRect SrcRect = View.ViewRect / InputScaleFactor;
	FIntRect DestRect = View.ViewRect / OutputScaleFactor;

	const FSceneRenderTargetItem& DestRenderTarget = Dest ? Dest->GetRenderTargetItem() : PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// set the state
	if(bAdditiveBlend)
	{
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
	}
	else
	{
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	}

	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessPassThroughPS> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetParameters(Context);
	PixelShader->SetParameters(Context.RHICmdList, Context);

	DrawPostProcessPass(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestSize,
		SrcSize,
		*VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);

	// Draw custom data (like legends) for derived types
	DrawCustom(Context);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessPassThrough::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;

	// we assume this pass is additively blended with the scene color so an intermediate is not always needed
	if(!Dest)
	{
		Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

		if(NewDesc.IsValid())
		{
			Ret = NewDesc;
		}
	}

	Ret.Reset();
	Ret.DebugName = TEXT("PassThrough");

	return Ret;
}

void CopyOverOtherViewportsIfNeeded(FRenderingCompositePassContext& Context, const FSceneView& ExcludeView)
{
	const FSceneView& View = Context.View;
	const FSceneViewFamily* ViewFamily = View.Family;

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	// only for multiple views we need to do this
	if(ViewFamily->Views.Num())
	{
		SCOPED_DRAW_EVENT(Context.RHICmdList, CopyOverOtherViewportsIfNeeded);

		TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
		TShaderMapRef<FPostProcessPassThroughPS> PixelShader(Context.GetShaderMap());

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		VertexShader->SetParameters(Context);
		PixelShader->SetParameters(Context.RHICmdList, Context);

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);

		FIntPoint Size = SceneContext.GetBufferSizeXY();
	
		for(uint32 ViewId = 0, ViewCount = ViewFamily->Views.Num(); ViewId < ViewCount; ++ViewId)
		{
			const FSceneView& LocalView = *ViewFamily->Views[ViewId];
			
			if(&LocalView != &ExcludeView)
			{
				FIntRect Rect = LocalView.ViewRect;

				DrawPostProcessPass(Context.RHICmdList,
					Rect.Min.X, Rect.Min.Y,
					Rect.Width(), Rect.Height(),
					Rect.Min.X, Rect.Min.Y,
					Rect.Width(), Rect.Height(),
					Size,
					Size,
					*VertexShader,
					LocalView.StereoPass, 
					Context.HasHmdMesh(),
					EDRF_UseTriangleOptimization);
			}
		}
	}
}
