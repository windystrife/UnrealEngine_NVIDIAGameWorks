// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessNoiseBlur.h: Post processing down sample implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

// ePId_Input0: input to define the RT output size
// ePId_Input1: input to blur
class FRCPassPostProcessNoiseBlur : public TRenderingCompositePassBase<2, 1>
{
public:
	// constructor
	// @param InRadius in pixels in the full res image
	FRCPassPostProcessNoiseBlur(float InRadius, EPixelFormat InOverrideFormat = PF_Unknown, uint32 InQuality = 1);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	// in pixels in the full res image
	float Radius;
	// 0/1/2
	uint32 Quality;

	EPixelFormat OverrideFormat;
};
