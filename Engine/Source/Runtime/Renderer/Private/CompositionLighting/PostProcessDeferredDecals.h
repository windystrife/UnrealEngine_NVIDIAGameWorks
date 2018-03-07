// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessDeferredDecals.h: Deferred Decals implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "RendererInterface.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "DecalRenderingCommon.h"
#include "DrawingPolicy.h"

// ePId_Input0: SceneColor (not needed for DBuffer decals)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessDeferredDecals : public TRenderingCompositePassBase<1, 1>
{
public:
	// One instance for each render stage
	FRCPassPostProcessDeferredDecals(EDecalRenderStage InDecalRenderStage);

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	// see EDecalRenderStage
	EDecalRenderStage CurrentStage;
	void DecodeRTWriteMask(FRenderingCompositePassContext& Context);
};

bool IsDBufferEnabled();

static inline bool IsWritingToGBufferA(FDecalRenderingCommon::ERenderTargetMode RenderTargetMode)
{
	return RenderTargetMode == FDecalRenderingCommon::RTM_SceneColorAndGBufferWithNormal
		|| RenderTargetMode == FDecalRenderingCommon::RTM_SceneColorAndGBufferDepthWriteWithNormal
		|| RenderTargetMode == FDecalRenderingCommon::RTM_GBufferNormal;
}

struct FDecalRenderTargetManager
{
	enum EDecalResolveBufferIndex
	{
		SceneColorIndex,
		GBufferAIndex,
		GBufferBIndex,
		GBufferCIndex,
		GBufferEIndex,
		DBufferAIndex,
		DBufferBIndex,
		DBufferCIndex,
		ResolveBufferMax,
	};
	//
	FRHICommandList& RHICmdList;
	//
	bool TargetsToTransitionWritable[ResolveBufferMax];
	//
	FTextureRHIParamRef TargetsToResolve[ResolveBufferMax];
	//
	bool bGufferADirty;

	// constructor
	FDecalRenderTargetManager(FRHICommandList& InRHICmdList, EShaderPlatform ShaderPlatform, EDecalRenderStage CurrentStage);

	// destructor
	~FDecalRenderTargetManager()
	{

	}

	void ResolveTargets();

	void FlushMetaData(FTextureRHIParamRef* Textures, uint32 NumTextures);

	void SetRenderTargetMode(FDecalRenderingCommon::ERenderTargetMode CurrentRenderTargetMode, bool bHasNormal);
};

extern FBlendStateRHIParamRef GetDecalBlendState(const ERHIFeatureLevel::Type SMFeatureLevel, EDecalRenderStage InDecalRenderStage, EDecalBlendMode DecalBlendMode, bool bHasNormal);

extern void RenderMeshDecals(FRenderingCompositePassContext& Context, EDecalRenderStage CurrentDecalStage);
