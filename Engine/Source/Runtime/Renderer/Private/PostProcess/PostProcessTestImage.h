// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTestImage.h: Post processing TestImage implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
class FRCPassPostProcessTestImage : public TRenderingCompositePassBase<1, 1>
{
public:
	// constructor
	FRCPassPostProcessTestImage();

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

