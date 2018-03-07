// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessMorpheus.h: Post processing for Sony Morpheus HMD device 
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

#if defined(HAS_MORPHEUS) && HAS_MORPHEUS

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: SceneColor
class FRCPassPostProcessMorpheus : public TRenderingCompositePassBase<1, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() { delete this; }
	FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

#endif
