// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessLensBlur.h: Post processing lens blur implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

// ePId_Input0: Input image
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessLensBlur : public TRenderingCompositePassBase<1, 1>
{
public:
	// constructor
	FRCPassPostProcessLensBlur(float InPercentKernelSize, float InThreshold);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	

	float PercentKernelSize;
	float Threshold;
};
