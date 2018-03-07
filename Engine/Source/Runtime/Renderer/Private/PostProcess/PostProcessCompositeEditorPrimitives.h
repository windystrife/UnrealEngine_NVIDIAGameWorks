// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

class FViewInfo;
struct FDrawingPolicyRenderState;

#if WITH_EDITOR


// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: SceneColor
class FRCPassPostProcessCompositeEditorPrimitives : public TRenderingCompositePassBase<1, 1>
{
public:
	FRCPassPostProcessCompositeEditorPrimitives(bool bInDeferredBasePass) : bDeferredBasePass(bInDeferredBasePass) {}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	/**
	 * Draws primitives that need to be composited
	 *
	 * @param View	The view to draw in
	 */
	template <typename TBasePass>
	void RenderPrimitivesToComposite(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, FDrawingPolicyRenderState& DrawRenderState);

	bool bDeferredBasePass;
};

#endif
