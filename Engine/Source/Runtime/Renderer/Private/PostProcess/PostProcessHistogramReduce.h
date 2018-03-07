// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessHistogramReduce.h: Post processing histogram reduce implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "PostProcess/PostProcessHistogram.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
class FRCPassPostProcessHistogramReduce : public TRenderingCompositePassBase<1, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	
	static const uint32 ThreadGroupSizeX = FRCPassPostProcessHistogram::HistogramTexelCount;
	static const uint32 ThreadGroupSizeY = 4;

private:

	static uint32 ComputeLoopSize(FIntPoint PixelExtent);
};
