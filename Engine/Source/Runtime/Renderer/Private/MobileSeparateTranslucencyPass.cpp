// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MobileSeparateTranslucencyPass.cpp - Mobile specific separate translucensy pass
=============================================================================*/

#include "MobileSeparateTranslucencyPass.h"
#include "TranslucentRendering.h"
#include "DynamicPrimitiveDrawing.h"

bool IsMobileSeparateTranslucencyActive(const FViewInfo& View)
{
	return View.TranslucentPrimSet.SortedPrimsNum.Num(ETranslucencyPass::TPT_TranslucencyAfterDOF) > 0;
}

void FRCSeparateTranslucensyPassES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, SeparateTranslucensyPass);
	
	const FViewInfo& View = Context.View;

	FSceneRenderTargets& SceneTargets = FSceneRenderTargets::Get(Context.RHICmdList);
	
	SetRenderTarget(Context.RHICmdList, SceneTargets.GetSceneColorSurface(), SceneTargets.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilRead);

	// Set the view family's render target/viewport.
	Context.SetViewportAndCallRHI(View.ViewRect);

	FDrawingPolicyRenderState DrawRenderState(View);

	// Enable depth test, disable depth writes.
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

	// Draw translucent prims
	FMobileTranslucencyDrawingPolicyFactory::ContextType DrawingContext(ESceneRenderTargetsMode::SetTextures, ETranslucencyPass::TPT_TranslucencyAfterDOF);
	View.TranslucentPrimSet.DrawPrimitivesForMobile<FMobileTranslucencyDrawingPolicyFactory>(Context.RHICmdList, View, DrawRenderState, DrawingContext);
	
	Context.RHICmdList.CopyToResolveTarget(SceneTargets.GetSceneColorSurface(), SceneTargets.GetSceneColorTexture(), false, FResolveParams());
}

FRenderingCompositeOutput* FRCSeparateTranslucensyPassES2::GetOutput(EPassOutputId InPassOutputId)
{
	// draw on top of input (scenecolor)
	if (InPassOutputId == ePId_Output0)
	{
		return GetInput(EPassInputId::ePId_Input0)->GetOutput();
	}
	
	return nullptr;
}

FPooledRenderTargetDesc FRCSeparateTranslucensyPassES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	Ret.DebugName = TEXT("SeparateTranslucensyPassES2");
	return Ret;
}
