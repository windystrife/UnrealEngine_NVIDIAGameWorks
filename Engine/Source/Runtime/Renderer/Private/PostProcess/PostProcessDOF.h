// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessDOF.h: Post process Depth of Field implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

// down sample and setup DOF input
// ePId_Input0: SceneColor
// ePId_Input1: SceneDepth
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessDOFSetup : public TRenderingCompositePassBase<2, 2>
{
public:
	FRCPassPostProcessDOFSetup(bool bInFarBlur, bool bInNearBlur) 
		: bFarBlur(bInFarBlur)
		, bNearBlur(bInNearBlur)
	{}

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	
	bool bFarBlur;
	bool bNearBlur;
};

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// ePId_Input0: Full res scene color
// ePId_Input1: FarBlur from the DOFSetup (possibly further blurred)
// ePId_Input2: NearBlur from the DOFSetup (possibly further blurred)
// ePId_Input3: optional SeparateTransluceny
class FRCPassPostProcessDOFRecombine : public TRenderingCompositePassBase<4, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};
