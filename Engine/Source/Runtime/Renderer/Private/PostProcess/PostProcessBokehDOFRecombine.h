// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessBokehDOFRecombine.h: Post processing lens blur implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// ePId_Input0: Full res scene color
// ePId_Input1: optional output from the BokehDOF (two blurred images, for in front and behind the focal plane)
// ePId_Input2: optional SeparateTranslucency
class FRCPassPostProcessBokehDOFRecombine : public TRenderingCompositePassBase<3, 1>
{
public:
	FRCPassPostProcessBokehDOFRecombine(bool InIsComputePass)
	{
		bIsComputePass = InIsComputePass;
		bPreferAsyncCompute = false;
	}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	virtual FComputeFenceRHIParamRef GetComputePassEndFence() const override { return AsyncEndFence; }

private:
	template <uint32 Method>
	static void SetShader(const FRenderingCompositePassContext& Context);

	template <typename TRHICmdList>
	void DispatchCS(TRHICmdList& RHICmdList, FRenderingCompositePassContext& Context, const FIntRect& DestRect, FUnorderedAccessViewRHIParamRef DestUAV, uint32 Method, float UVScaling);
	
	FComputeFenceRHIRef AsyncEndFence;
};
