// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessMaterial.h: Post processing Material
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

class UMaterialInterface;

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: former pass
// ePId_Input1: optional, depends on EBlendableLocation
// ePId_Input2: optional, depends on EBlendableLocation
// ePId_Input3: optional, depends on EBlendableLocation
class FRCPassPostProcessMaterial : public TRenderingCompositePassBase<4,1>
{
public:
	// constructor
	FRCPassPostProcessMaterial(UMaterialInterface* InMaterialInterface, ERHIFeatureLevel::Type InFeatureLevel, EPixelFormat OutputFormatIN = PF_Unknown);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	void SetShader(const FRenderingCompositePassContext& Context);

	UMaterialInterface* MaterialInterface;

	// PF_Unknown for default behavior.
	EPixelFormat OutputFormat;
};
