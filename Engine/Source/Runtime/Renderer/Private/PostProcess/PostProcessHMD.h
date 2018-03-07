// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessHMD.h: Post processing for HMD devices 
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "RenderingCompositionGraph.h"

/** The vertex data used to filter a texture. */
struct FDistortionVertex
{
	FVector2D	Position;
	FVector2D	TexR;
	FVector2D	TexG;
	FVector2D	TexB;
	float		VignetteFactor;
	float		TimewarpFactor;
};

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: SceneColor
class FRCPassPostProcessHMD : public TRenderingCompositePassBase<1, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

