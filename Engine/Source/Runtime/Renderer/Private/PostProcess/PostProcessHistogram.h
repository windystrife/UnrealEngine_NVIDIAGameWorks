// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessHistogram.h: Post processing histogram implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
class FRCPassPostProcessHistogram : public TRenderingCompositePassBase<1, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	// -------------------------------------------

	// changing this number require Histogram.usf to be recompiled
	static const uint32 ThreadGroupSizeX = 8;
	// changing this number require Histogram.usf to be recompiled
	static const uint32 ThreadGroupSizeY = 4;

	static const uint32 HistogramSize = 64;

	// /4 as we store 4 buckets in one ARGB texel
	static const uint32 HistogramTexelCount = HistogramSize / 4;

	// one ThreadGroup processes LoopCountX*LoopCountY blocks of size ThreadGroupSizeX*ThreadGroupSizeY

	// changing this number require Histogram.usf to be recompiled
	static const uint32 LoopCountX = 8;
	// changing this number require Histogram.usf to be recompiled
	static const uint32 LoopCountY = 8;

	// -------------------------------------------

	static FIntPoint ComputeGatherExtent(const FSceneView& View);
	static FIntPoint ComputeThreadGroupCount(FIntPoint PixelExtent);
};
