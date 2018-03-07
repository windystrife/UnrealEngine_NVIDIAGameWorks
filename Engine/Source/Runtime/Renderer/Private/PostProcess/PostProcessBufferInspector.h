// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessBufferInspector.h: Post processing pixel inspector
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: SceneColor
// ePId_Input1: SeparateTranslucency
class FRCPassPostProcessBufferInspector : public TRenderingCompositePassBase<3, 1>
{
public:

	FRCPassPostProcessBufferInspector(FRHICommandList& RHICmdList);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
private:
	template <typename TRHICmdList>
	FShader* SetShaderTempl(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context);
};
