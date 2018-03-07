// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessBusyWait.h: Post processing busy wait implementation. For Debugging GPU timing.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
class FRCPassPostProcessBusyWait : public TRenderingCompositePassBase<0, 1>
{
public:
	static bool IsPassRequired();

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

