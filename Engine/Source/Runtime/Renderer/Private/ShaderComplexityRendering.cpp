// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ShaderComplexityRendering.cpp: Contains definitions for rendering the shader complexity viewmode.
=============================================================================*/

#include "ShaderComplexityRendering.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/PostProcessVisualizeComplexity.h"

IMPLEMENT_SHADER_TYPE(template<>,TShaderComplexityAccumulatePS,TEXT("/Engine/Private/ShaderComplexityAccumulatePixelShader.usf"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TQuadComplexityAccumulatePS,TEXT("/Engine/Private/QuadComplexityAccumulatePixelShader.usf"),TEXT("Main"),SF_Pixel);

template <bool bQuadComplexity>
void TComplexityAccumulatePS<bQuadComplexity>::SetParameters(
	FRHICommandList& RHICmdList, 
	const FShader* OriginalVS, 
	const FShader* OriginalPS, 
	const FMaterialRenderProxy* MaterialRenderProxy,
	const FMaterial& Material,
	const FSceneView& View
	)
{
	EDebugViewShaderMode DebugViewShaderMode = View.Family->GetDebugViewShaderMode();

	const float NormalizeMul = 1.0f / GetMaxShaderComplexityCount(View.GetFeatureLevel());
	const int32 DeferredBasePassBuiltinInstructions = 83;
	const int32 ForwardBasePassBuiltinInstructions = 476;
	// Attempt to remove instructions from code features only present in the forward renderer, so we are showing users their graph cost
	const int32 LitBaseline = IsAnyForwardShadingEnabled(View.GetShaderPlatform()) ? (ForwardBasePassBuiltinInstructions - DeferredBasePassBuiltinInstructions) : 0;
	const int32 Baseline = Material.GetShadingModel() == MSM_Unlit ? 0 : LitBaseline;
	const int32 AdjustedInstructionCount = FMath::Max<int32>(OriginalPS->GetNumInstructions() - Baseline, 0);

	// normalize the complexity so we can fit it in a low precision scene color which is necessary on some platforms
	// late value is for overdraw which can be problmatic with a low precision float format, at some point the precision isn't there any more and it doesn't accumulate
	FVector Value = DebugViewShaderMode == DVSM_QuadComplexity ? FVector(NormalizedQuadComplexityValue) : FVector(AdjustedInstructionCount * NormalizeMul, OriginalVS->GetNumInstructions() * NormalizeMul, 1/32.0f);

	// Disable UAVs if something is wrong
	if (DebugViewShaderMode != DVSM_ShaderComplexity)
	{
		const FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		if (QuadBufferUAV.IsBound() && SceneContext.GetQuadOverdrawIndex() != QuadBufferUAV.GetBaseIndex())
		{
			DebugViewShaderMode = DVSM_ShaderComplexity;
		}
	}

	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), NormalizedComplexity, Value);
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), ShowQuadOverdraw, DebugViewShaderMode != DVSM_ShaderComplexity);
}

// Instantiate the template 
template class TComplexityAccumulatePS<false>;
template class TComplexityAccumulatePS<true>;
