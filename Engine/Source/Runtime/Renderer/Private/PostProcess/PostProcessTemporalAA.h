// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTemporalAA.h: Post process MotionBlur implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

// ePId_Input0: Reflections (point)
// ePId_Input1: Previous frame's output (bilinear)
// ePId_Input2: Previous frame's output (point)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessSSRTemporalAA : public TRenderingCompositePassBase<4, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

// ePId_Input0: Half Res DOF input (point)
// ePId_Input1: Previous frame's output (bilinear)
// ePId_Input2: Previous frame's output (point)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessDOFTemporalAA : public TRenderingCompositePassBase<4, 1>
{
public:
	FRCPassPostProcessDOFTemporalAA(bool bInIsComputePass)
	{
		bIsComputePass = bInIsComputePass;
		bPreferAsyncCompute = false;
		bPreferAsyncCompute &= (GNumActiveGPUsForRendering == 1); // Can't handle multi-frame updates on async pipe
	}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	virtual FComputeFenceRHIParamRef GetComputePassEndFence() const override { return AsyncEndFence; }

private:
	template <typename TRHICmdList>
	void DispatchCS(TRHICmdList& RHICmdList, FRenderingCompositePassContext& Context, const FIntRect& DestRect, FUnorderedAccessViewRHIParamRef DestUAV, FTextureRHIParamRef EyeAdaptationTex);

	FComputeFenceRHIRef AsyncEndFence;
};

// ePId_Input0: Half Res DOF input (point)
// ePId_Input1: Previous frame's output (bilinear)
// ePId_Input2: Previous frame's output (point)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessDOFTemporalAANear : public TRenderingCompositePassBase<4, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};


// ePId_Input0: Half Res light shaft input (point)
// ePId_Input1: Previous frame's output (bilinear)
// ePId_Input2: Previous frame's output (point)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessLightShaftTemporalAA : public TRenderingCompositePassBase<3, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

// ePId_Input0: Full Res Scene color (point)
// ePId_Input1: Previous frame's output (bilinear)
// ePId_Input2: Previous frame's output (point)
// ePId_Input3: Velocity (point)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessTemporalAA : public TRenderingCompositePassBase<4, 1>
{
public:
	FRCPassPostProcessTemporalAA(bool bInIsComputePass)
	{
		bIsComputePass = bInIsComputePass;
		bPreferAsyncCompute = false;
		bPreferAsyncCompute &= (GNumActiveGPUsForRendering == 1); // Can't handle multi-frame updates on async pipe
	}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	virtual FComputeFenceRHIParamRef GetComputePassEndFence() const override { return AsyncEndFence; }

private:
	template <typename TRHICmdList>
	void DispatchCS(TRHICmdList& RHICmdList, FRenderingCompositePassContext& Context, const FIntRect& DestRect, FUnorderedAccessViewRHIParamRef DestUAV, const bool bUseFast, const bool bUseDither, FTextureRHIParamRef EyeAdaptationTex);

	FComputeFenceRHIRef AsyncEndFence;
};
