// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessVisualizeHDR.h: Post processing VisualizeHDR implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: LDR SceneColor
// ePId_Input1: output of the Histogram pass
// ePId_Input2: HDR SceneColor
// ePId_Input3: output of the Histogram pass over screen (not reduced yet)
class FRCPassPostProcessVisualizeHDR : public TRenderingCompositePassBase<4, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

