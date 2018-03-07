// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessAmbientOcclusion.h: Post processing ambient occlusion implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UniformBuffer.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

class FViewInfo;

enum class ESSAOType
{
	// pixel shader
	EPS,
	// non async compute shader
	ECS,
	// async compute shader
	EAsyncCS,
};

class FSSAOHelper
{
public:

	// Utility functions for deciding AO logic.
	// for render thread
	// @return usually in 0..100 range but could be outside, combines the view with the cvar setting
	static float GetAmbientOcclusionQualityRT(const FSceneView& View);

	// @return returns actual shader quality level to use. 0-4 currently.
	static int32 GetAmbientOcclusionShaderLevel(const FSceneView& View);

	// @return whether AmbientOcclusion should run a compute shader.
	static bool IsAmbientOcclusionCompute(const FSceneView& View);

	static int32 GetNumAmbientOcclusionLevels();
	static float GetAmbientOcclusionStepMipLevelFactor();
	static EAsyncComputeBudget GetAmbientOcclusionAsyncComputeBudget();

	static bool IsBasePassAmbientOcclusionRequired(const FViewInfo& View);
	static bool IsAmbientOcclusionAsyncCompute(const FViewInfo& View, uint32 AOPassCount);
		
};

// ePId_Input0: SceneDepth
// ePId_Input1: optional from former downsampling pass
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessAmbientOcclusionSetup : public TRenderingCompositePassBase<2, 1>
{
public:

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	// otherwise this is a down sampling pass which takes two MRT inputs from the setup pass before
	bool IsInitialPass() const;
};


// ePId_Input0: defines the resolution we compute AO and provides the normal (only needed if bInAOSetupAsInput)
// ePId_Input1: setup in same resolution as ePId_Input1 for depth expect when running in full resolution, then it's half (only needed if bInAOSetupAsInput)
// ePId_Input2: optional AO result one lower resolution
// ePId_Input3: optional HZB
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessAmbientOcclusion : public TRenderingCompositePassBase<4, 1>
{
public:
	// @param bInAOSetupAsInput true:use AO setup as input, false: use GBuffer normal and native z depth
	FRCPassPostProcessAmbientOcclusion(const FSceneView& View, ESSAOType InAOType, bool bInAOSetupAsInput = true);

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	
	void ProcessCS(FRenderingCompositePassContext& Context, const FSceneRenderTargetItem* DestRenderTarget, const FIntRect& ViewRect, const FIntPoint& TexSize, int32 ShaderQuality, bool bDoUpsample);
	void ProcessPS(FRenderingCompositePassContext& Context, const FSceneRenderTargetItem* DestRenderTarget, const FIntRect& ViewRect, const FIntPoint& TexSize, int32 ShaderQuality, bool bDoUpsample);

	template <uint32 bAOSetupAsInput, uint32 bDoUpsample, uint32 SampleSetQuality>
	FShader* SetShaderTemplPS(const FRenderingCompositePassContext& Context, FGraphicsPipelineStateInitializer& GraphicsPSOInit);

	template <uint32 bAOSetupAsInput, uint32 bDoUpsample, uint32 SampleSetQuality, typename TRHICmdList>
	void DispatchCS(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, const FIntPoint& TexSize, FUnorderedAccessViewRHIParamRef OutTextureUAV);
	
	const ESSAOType AOType;
	const bool bAOSetupAsInput;
};

// apply the AO to the SceneColor (lightmapped object), extra pas that is not always needed
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessBasePassAO : public TRenderingCompositePassBase<0, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};






/** The uniform shader parameters needed for screen space TemporalAA and SSAO passes. */
BEGIN_UNIFORM_BUFFER_STRUCT(FCameraMotionParameters,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4, Value, [5])
END_UNIFORM_BUFFER_STRUCT(FCameraMotionParameters)

TUniformBufferRef<FCameraMotionParameters> CreateCameraMotionParametersUniformBuffer(const FSceneView& View);
