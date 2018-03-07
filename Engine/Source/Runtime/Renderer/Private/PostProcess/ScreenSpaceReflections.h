// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenSpaceReflections.h: Post processing Screen Space Reflections implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

class FViewInfo;

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessDepthDownSample : public TRenderingCompositePassBase<1, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

// ePId_Input0: scene color
// ePId_Input1: scene depth
// ePId_Input2: hierarchical scene color (optional)
// ePId_Input3: velocity (optional)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessScreenSpaceReflections : public TRenderingCompositePassBase<4, 1>
{
public:
	FRCPassPostProcessScreenSpaceReflections( bool InPrevFrame )
	: bPrevFrame( InPrevFrame )
	{}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	bool bPrevFrame;
};


// ePId_Input0: half res scene color
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessApplyScreenSpaceReflections : public TRenderingCompositePassBase<2, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

void RenderScreenSpaceReflections(FRHICommandListImmediate& RHICmdList, FViewInfo& View, TRefCountPtr<IPooledRenderTarget>& SSROutput, TRefCountPtr<IPooledRenderTarget>& VelocityRT);

bool ShouldRenderScreenSpaceReflections(const FViewInfo& View);

bool IsSSRTemporalPassRequired(const FViewInfo& View, bool bCheckSSREnabled = true);