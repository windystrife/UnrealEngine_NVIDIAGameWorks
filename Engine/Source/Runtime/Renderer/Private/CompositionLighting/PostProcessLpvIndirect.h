//-----------------------------------------------------------------------------
// File:		PostProcessLpvIndirect.h
//
// Summary:		Light propagation volume postprocessing
//
// Created:		11/03/2013
//
// Author:		mailto:benwood@microsoft.com
//
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

// ePId_Input0: SceneColor
// ePId_Input1: optional AmbientOcclusion
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessLpvIndirect: public TRenderingCompositePassBase<2, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual const TCHAR* GetDebugName() { return TEXT("FRCPassPostProcessLpvIndirect"); }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	void DoDirectionalOcclusionPass(FRenderingCompositePassContext& Context) const;
};

// ePId_Input0: SceneColor
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessVisualizeLPV: public TRenderingCompositePassBase<1, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual const TCHAR* GetDebugName() { return TEXT("FRCPassPostProcessVisualizeLPV"); }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual bool FrameBufferBlendingWithInput0() const { return true; }
};




