// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessLensFlares.h: Post processing lens flares implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

// ePId_Input0: Bloom
// ePId_Input1: Lensflare image input
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessLensFlares : public TRenderingCompositePassBase<2, 1>
{
public:
	// constructor
	FRCPassPostProcessLensFlares(float InSizeScale, bool bCompositBloomIn = true);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	float SizeScale;
	bool bCompositeBloom;
};
