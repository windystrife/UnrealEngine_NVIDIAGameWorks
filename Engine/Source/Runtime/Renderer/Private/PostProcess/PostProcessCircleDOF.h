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
class FRCPassPostProcessCircleDOFSetup : public TRenderingCompositePassBase<2, 2>
{
public:

	FRCPassPostProcessCircleDOFSetup(void) {}

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};


// ePId_Input0: DOFInput or DOFInputTemporalAA
class FRCPassPostProcessCircleDOFDilate : public TRenderingCompositePassBase<1, 1>
{
public:

	FRCPassPostProcessCircleDOFDilate(void) {}

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};




// ePId_Input0: setup results for far
// ePId_Input1: setup results for near, might have been processed by the dilate pass
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessCircleDOF : public TRenderingCompositePassBase<3, 2>
{
public:

	FRCPassPostProcessCircleDOF(void) {}

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:

	template <uint32 Quality>
	FShader* SetShaderTempl(const FRenderingCompositePassContext& Context);
};

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// ePId_Input0: Full res scene color
// ePId_Input1: output 0 from the DOFSetup (possibly further blurred)
// ePId_Input2: output 1 from the DOFSetup (possibly further blurred)
class FRCPassPostProcessCircleDOFRecombine : public TRenderingCompositePassBase<3, 1>
{
public:

	FRCPassPostProcessCircleDOFRecombine() {}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:

	template <uint32 Quality>
	FShader* SetShaderTempl(const FRenderingCompositePassContext& Context);
};

// to verify this can be used: http://www.radical.org/aov
// @return in mm, assuming the sensor in the DepthOfField settings
float ComputeFocalLengthFromFov(const FSceneView& View);

FVector4 CircleDofHalfCoc(const FSceneView& View);
