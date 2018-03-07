// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessSubsurface.h: Screenspace subsurface scattering implementation
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

class FViewInfo;

// ePId_Input0: SceneColor
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// uses some GBuffer attributes
// alpha is unused
class FRCPassPostProcessSubsurfaceVisualize : public TRenderingCompositePassBase<1, 1>
{
public:
	FRCPassPostProcessSubsurfaceVisualize(FRHICommandList& RHICmdList);

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
};

// ePId_Input0: SceneColor
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// uses some GBuffer attributes
// alpha is unused
class FRCPassPostProcessSubsurfaceSetup : public TRenderingCompositePassBase<1, 1>
{
public:
	// constructor
	FRCPassPostProcessSubsurfaceSetup(FViewInfo& View, bool bInHalfRes);

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }

	FIntRect ViewRect;
	bool bHalfRes;
};


// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: SceneColor (horizontal blur) or the pass before (vertical blur)
// ePId_Input1: optional Setup pass (only for InDirection==1)
// modifies SceneColor, uses some GBuffer attributes
class FRCPassPostProcessSubsurface : public TRenderingCompositePassBase<2, 1>
{
public:
	// constructor
	// @param InDirection 0:horizontal/1:vertical
	FRCPassPostProcessSubsurface(uint32 InDirection, bool bInHalfRes);

	static const bool RequiresCheckerboardSubsurfaceRendering(EPixelFormat SceneColorFormat);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	// 0:horizontal/1:vertical
	uint32 Direction;
	bool bHalfRes;
};


// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: SceneColor before Screen Space Subsurface input
// ePId_Input1: optional output from FRCPassPostProcessSubsurface (if not present we do cheap reconstruction for Scalability)
// ePId_Input2: optional SubsurfaceSetup, can be half res
// modifies SceneColor, uses some GBuffer attributes
class FRCPassPostProcessSubsurfaceRecombine : public TRenderingCompositePassBase<3, 1>
{
public:
	// constructor
	FRCPassPostProcessSubsurfaceRecombine(bool bInHalfRes, bool bInSingleViewportMode);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	bool bHalfRes;
	bool bSingleViewportMode;
};
