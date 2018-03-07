// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessAA.h: Post processing anti aliasing implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessAA : public TRenderingCompositePassBase<1, 1>
{
public:
	// @param InQuality 1..6, larger values are ignored
	FRCPassPostProcessAA(uint32 InQuality) : Quality(InQuality)
	{
		check(InQuality > 0);
	}

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	// 1..6, larger values are ignored
	uint32 Quality;
};
