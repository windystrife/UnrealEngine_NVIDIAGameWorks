// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessVisualizeBuffer.cpp: Post processing VisualizeBuffer implementation.
=============================================================================*/

#include "PostProcess/PostProcessBufferInspector.h"
#include "StaticBoundShaderState.h"
#include "CanvasTypes.h"
#include "RenderTargetTemp.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/SceneFilterRendering.h"
#include "CompositionLighting/PostProcessPassThrough.h"
#include "PostProcess/PostProcessing.h"
#include "ScenePrivate.h"
#include "PipelineStateCache.h"

FRCPassPostProcessBufferInspector::FRCPassPostProcessBufferInspector(FRHICommandList& RHICmdList)
{
	// AdjustGBufferRefCount(-1) call is done when the pass gets executed
	FSceneRenderTargets::Get(RHICmdList).AdjustGBufferRefCount(RHICmdList, 1);
}

template <typename TRHICmdList>
FShader* FRCPassPostProcessBufferInspector::SetShaderTempl(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// set the state
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessPassThroughPS> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	VertexShader->SetParameters(Context);
	PixelShader->SetParameters(RHICmdList, Context);

	return *VertexShader;
}

void FRCPassPostProcessBufferInspector::Process(FRenderingCompositePassContext& Context)
{
#if !WITH_EDITOR
	return;
#else
	SCOPED_DRAW_EVENT(Context.RHICmdList, BufferInspector);
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	const FPooledRenderTargetDesc* InputDescHDR = GetInputDesc(ePId_Input1);

	if (!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FScene* Scene = (FScene*)View.Family->Scene;
	int32 ViewUniqueId = View.State->GetViewKey();
	TArray<FIntPoint> ProcessRequests;
	//Process all request for this view
	for (auto kvp : Scene->PixelInspectorData.Requests)
	{
		FPixelInspectorRequest *PixelInspectorRequest = kvp.Value;
		if (PixelInspectorRequest->RequestComplete == true)
		{
			PixelInspectorRequest->RenderingCommandSend = true;
			ProcessRequests.Add(kvp.Key);
		}
		else if (PixelInspectorRequest->RenderingCommandSend == false && PixelInspectorRequest->ViewId == ViewUniqueId)
		{
			FVector2D SourcePoint(PixelInspectorRequest->SourcePixelPosition);
			FVector2D ExtendSize(1.0f, 1.0f);
			FBox2D SourceBox(SourcePoint, SourcePoint + ExtendSize);

			//////////////////////////////////////////////////////////////////////////
			// Pixel Depth
			if (Scene->PixelInspectorData.RenderTargetBufferDepth[PixelInspectorRequest->BufferIndex] != nullptr)
			{
				const FTexture2DRHIRef &DestinationBufferDepth = Scene->PixelInspectorData.RenderTargetBufferDepth[PixelInspectorRequest->BufferIndex]->GetRenderTargetTexture();
				if (DestinationBufferDepth.IsValid())
				{
					FTexture2DRHIRef SourceBufferSceneDepth = SceneContext.GetSceneDepthTexture();
					if (DestinationBufferDepth->GetFormat() == SourceBufferSceneDepth->GetFormat())
					{
						FBox2D DestinationBox(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));
						RHICmdList.CopySubTextureRegion(SourceBufferSceneDepth, DestinationBufferDepth, SourceBox, DestinationBox);
					}
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// FINAL COLOR
			const FTexture2DRHIRef &DestinationBufferFinalColor = Scene->PixelInspectorData.RenderTargetBufferFinalColor[PixelInspectorRequest->BufferIndex]->GetRenderTargetTexture();
			if (DestinationBufferFinalColor.IsValid())
			{
				const FRenderingCompositeOutputRef* OutputRef0 = GetInput(ePId_Input0);
				if (OutputRef0)
				{
					FRenderingCompositeOutput* Input = OutputRef0->GetOutput();
					if (Input && Input->PooledRenderTarget.IsValid() && Input->PooledRenderTarget->GetRenderTargetItem().ShaderResourceTexture.IsValid())
					{
						FTexture2DRHIRef SourceBufferFinalColor = (FRHITexture2D*)(Input->PooledRenderTarget->GetRenderTargetItem().ShaderResourceTexture.GetReference());
						if (DestinationBufferFinalColor->GetFormat() == SourceBufferFinalColor->GetFormat())
						{
							FVector2D ExtendSizeContextLeft(FMath::FloorToFloat((float)DestinationBufferFinalColor->GetSizeX()/2.0f), FMath::FloorToFloat((float)DestinationBufferFinalColor->GetSizeY()/2.0f));
							FVector2D ExtendSizeContextRight(FMath::CeilToFloat((float)DestinationBufferFinalColor->GetSizeX() / 2.0f), FMath::CeilToFloat((float)DestinationBufferFinalColor->GetSizeY() / 2.0f));
							FBox2D SourceBoxContext(SourcePoint- ExtendSizeContextLeft, SourcePoint+ ExtendSizeContextRight);
							FBox2D DestinationBox(FVector2D(0.0f, 0.0f), FVector2D((float)DestinationBufferFinalColor->GetSizeX(), (float)DestinationBufferFinalColor->GetSizeY()));
							RHICmdList.CopySubTextureRegion(SourceBufferFinalColor, DestinationBufferFinalColor, SourceBoxContext, DestinationBox);
						}
					}
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// SCENE COLOR
			const FTexture2DRHIRef &DestinationBufferSceneColor = Scene->PixelInspectorData.RenderTargetBufferSceneColor[PixelInspectorRequest->BufferIndex]->GetRenderTargetTexture();
			if (DestinationBufferSceneColor.IsValid())
			{
				const FRenderingCompositeOutputRef* OutputRef0 = GetInput(ePId_Input2);
				if (OutputRef0)
				{
					FRenderingCompositeOutput* Input = OutputRef0->GetOutput();
					if (Input && Input->PooledRenderTarget.IsValid() && Input->PooledRenderTarget->GetRenderTargetItem().ShaderResourceTexture.IsValid())
					{
						FTexture2DRHIRef SourceBufferSceneColor = (FRHITexture2D*)(Input->PooledRenderTarget->GetRenderTargetItem().ShaderResourceTexture.GetReference());
						if (DestinationBufferSceneColor->GetFormat() == SourceBufferSceneColor->GetFormat())
						{
							FBox2D DestinationBox(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));
							RHICmdList.CopySubTextureRegion(SourceBufferSceneColor, DestinationBufferSceneColor, SourceBox, DestinationBox);
						}
					}
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// HDR
			const FTexture2DRHIRef &DestinationBufferHDR = Scene->PixelInspectorData.RenderTargetBufferHDR[PixelInspectorRequest->BufferIndex]->GetRenderTargetTexture();
			if (InputDescHDR != nullptr && DestinationBufferHDR.IsValid())
			{
				const FRenderingCompositeOutputRef* OutputRef1 = GetInput(ePId_Input1);
				if (OutputRef1)
				{
					FRenderingCompositeOutput* Input = OutputRef1->GetOutput();
					if (Input && Input->PooledRenderTarget.IsValid() && Input->PooledRenderTarget->GetRenderTargetItem().ShaderResourceTexture.IsValid())
					{
						FTexture2DRHIRef SourceBufferHDR = (FRHITexture2D*)(Input->PooledRenderTarget->GetRenderTargetItem().ShaderResourceTexture.GetReference());
						if (SourceBufferHDR.IsValid() && DestinationBufferHDR->GetFormat() == SourceBufferHDR->GetFormat())
						{
							FBox2D DestinationBox(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));
							RHICmdList.CopySubTextureRegion(SourceBufferHDR, DestinationBufferHDR, SourceBox, DestinationBox);
						}
					}
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// GBuffer A
			if (Scene->PixelInspectorData.RenderTargetBufferA[PixelInspectorRequest->BufferIndex] != nullptr)
			{
				const FTexture2DRHIRef &DestinationBufferA = Scene->PixelInspectorData.RenderTargetBufferA[PixelInspectorRequest->BufferIndex]->GetRenderTargetTexture();
				if (DestinationBufferA.IsValid() && SceneContext.GBufferA.IsValid() && SceneContext.GBufferA->GetRenderTargetItem().ShaderResourceTexture.IsValid())
				{
					FTexture2DRHIRef SourceBufferA = (FRHITexture2D*)(SceneContext.GBufferA->GetRenderTargetItem().ShaderResourceTexture.GetReference());
					if (SourceBufferA.IsValid() && DestinationBufferA->GetFormat() == SourceBufferA->GetFormat())
					{
						FBox2D DestinationBox(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));
						RHICmdList.CopySubTextureRegion(SourceBufferA, DestinationBufferA, SourceBox, DestinationBox);
					}
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// GBuffer BCDE
			const FTexture2DRHIRef &DestinationBufferBCDE = Scene->PixelInspectorData.RenderTargetBufferBCDE[PixelInspectorRequest->BufferIndex]->GetRenderTargetTexture();
			if (DestinationBufferBCDE.IsValid())
			{
				if (SceneContext.GBufferB.IsValid() && SceneContext.GBufferB->GetRenderTargetItem().ShaderResourceTexture.IsValid())
				{
					FTexture2DRHIRef SourceBufferB = (FRHITexture2D*)(SceneContext.GBufferB->GetRenderTargetItem().ShaderResourceTexture.GetReference());
					if (SourceBufferB.IsValid() && DestinationBufferBCDE->GetFormat() == SourceBufferB->GetFormat())
					{
						FBox2D DestinationBox(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));
						RHICmdList.CopySubTextureRegion(SourceBufferB, DestinationBufferBCDE, SourceBox, DestinationBox);
					}
				}

				if (SceneContext.GBufferC.IsValid() && SceneContext.GBufferC->GetRenderTargetItem().ShaderResourceTexture.IsValid())
				{
					FTexture2DRHIRef SourceBufferC = (FRHITexture2D*)(SceneContext.GBufferC->GetRenderTargetItem().ShaderResourceTexture.GetReference());
					if (SourceBufferC.IsValid() && DestinationBufferBCDE->GetFormat() == SourceBufferC->GetFormat())
					{
						FBox2D DestinationBox(FVector2D(1.0f, 0.0f), FVector2D(2.0f, 1.0f));
						RHICmdList.CopySubTextureRegion(SourceBufferC, DestinationBufferBCDE, SourceBox, DestinationBox);
					}
				}

				if (SceneContext.GBufferD.IsValid() && SceneContext.GBufferD->GetRenderTargetItem().ShaderResourceTexture.IsValid())
				{
					FTexture2DRHIRef SourceBufferD = (FRHITexture2D*)(SceneContext.GBufferD->GetRenderTargetItem().ShaderResourceTexture.GetReference());
					if (SourceBufferD.IsValid() && DestinationBufferBCDE->GetFormat() == SourceBufferD->GetFormat())
					{
						FBox2D DestinationBox(FVector2D(2.0f, 0.0f), FVector2D(3.0f, 1.0f));
						RHICmdList.CopySubTextureRegion(SourceBufferD, DestinationBufferBCDE, SourceBox, DestinationBox);
					}
				}

				if (SceneContext.GBufferE.IsValid() && SceneContext.GBufferE->GetRenderTargetItem().ShaderResourceTexture.IsValid())
				{
					FTexture2DRHIRef SourceBufferE = (FRHITexture2D*)(SceneContext.GBufferE->GetRenderTargetItem().ShaderResourceTexture.GetReference());
					if (SourceBufferE.IsValid() && DestinationBufferBCDE->GetFormat() == SourceBufferE->GetFormat())
					{
						FBox2D DestinationBox(FVector2D(3.0f, 0.0f), FVector2D(4.0f, 1.0f));
						RHICmdList.CopySubTextureRegion(SourceBufferE, DestinationBufferBCDE, SourceBox, DestinationBox);
					}
				}
			}

			PixelInspectorRequest->RenderingCommandSend = true;
			ProcessRequests.Add(kvp.Key);
		}
	}
	
	//Remove request we just process
	for (FIntPoint RequestKey : ProcessRequests)
	{
		Scene->PixelInspectorData.Requests.Remove(RequestKey);
	}

	FIntRect ViewRect = View.ViewRect;
	FIntPoint SrcSize = InputDesc->Extent;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
	Context.SetViewportAndCallRHI(ViewRect);

	{
		FShader* VertexShader = SetShaderTempl(Context.RHICmdList, Context);

		DrawRectangle(
			Context.RHICmdList,
			0, 0,
			ViewRect.Width(), ViewRect.Height(),
			ViewRect.Min.X, ViewRect.Min.Y,
			ViewRect.Width(), ViewRect.Height(),
			ViewRect.Size(),
			SrcSize,
			VertexShader,
			EDRF_UseTriangleOptimization);
	}

	FRenderTargetTemp TempRenderTarget(View, (const FTexture2DRHIRef&)DestRenderTarget.TargetableTexture);
	FCanvas Canvas(&TempRenderTarget, NULL, ViewFamily.CurrentRealTime, ViewFamily.CurrentWorldTime, ViewFamily.DeltaWorldTime, Context.GetFeatureLevel());
	FLinearColor LabelColor(1, 1, 1);
	Canvas.DrawShadowedString(100, 50, TEXT("Pixel Inspector On"), GetStatsFont(), LabelColor);
	Canvas.Flush_RenderThread(Context.RHICmdList);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	// AdjustGBufferRefCount(1) call is done in constructor
	FSceneRenderTargets::Get(Context.RHICmdList).AdjustGBufferRefCount(Context.RHICmdList, -1);
#endif
}

FPooledRenderTargetDesc FRCPassPostProcessBufferInspector::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	Ret.Reset();
	Ret.DebugName = TEXT("BufferInspector");
	return Ret;
}
