// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessBokehDOF.h: Post processing lens blur implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

struct FDepthOfFieldStats
{
	FDepthOfFieldStats()
		: bNear(true)
		, bFar(true)
	{
	}

	bool bNear;
	bool bFar;
};


// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// ePId_Input0: Color input
class FRCPassPostProcessVisualizeDOF : public TRenderingCompositePassBase<1, 1>
{
public:
	// constructor
	FRCPassPostProcessVisualizeDOF(const FDepthOfFieldStats& InDepthOfFieldStats)
		: DepthOfFieldStats(InDepthOfFieldStats)
	{}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	FDepthOfFieldStats DepthOfFieldStats;
};

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// ePId_Input0: Color input
// ePId_Input1: Depth input
class FRCPassPostProcessBokehDOFSetup : public TRenderingCompositePassBase<2, 1>
{
public:
	FRCPassPostProcessBokehDOFSetup(bool bInIsComputePass)
	{
		bIsComputePass = bInIsComputePass;
	}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	template <typename TRHICmdList>
	void DispatchCS(TRHICmdList& RHICmdList, FRenderingCompositePassContext& Context, const FIntRect& DestRect, FUnorderedAccessViewRHIParamRef DestUAV);
};

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// ePId_Input0: Half res scene with depth in alpha
// ePId_Input1: SceneColor for high quality input (experimental)
// ePId_Input2: SceneDepth for high quality input (experimental)
class FRCPassPostProcessBokehDOF : public TRenderingCompositePassBase<3, 1>
{
public:
	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	static void ComputeDepthOfFieldParams(const FRenderingCompositePassContext& Context, FVector4 Out[2]);

private:

	template <uint32 HighQuality>
	void SetShaderTempl(const FRenderingCompositePassContext& Context, FIntPoint LeftTop, FIntPoint TileCount, uint32 TileSize, float PixelKernelSize);

	// border between front and back layer as we don't use viewports (only possible with GS)
	const static uint32 SafetyBorder = 40;
};
