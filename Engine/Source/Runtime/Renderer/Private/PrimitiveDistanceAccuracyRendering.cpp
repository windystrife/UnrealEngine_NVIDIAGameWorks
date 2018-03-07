// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PrimitiveDistanceAccuracyRendering.cpp: Contains definitions for rendering the viewmode.
=============================================================================*/

#include "PrimitiveDistanceAccuracyRendering.h"
#include "PrimitiveSceneProxy.h"
#include "EngineGlobals.h"
#include "MeshBatch.h"
#include "Engine/Engine.h"

IMPLEMENT_SHADER_TYPE(,FPrimitiveDistanceAccuracyPS,TEXT("/Engine/Private/PrimitiveDistanceAccuracyPixelShader.usf"),TEXT("Main"),SF_Pixel);

void FPrimitiveDistanceAccuracyPS::SetParameters(
	FRHICommandList& RHICmdList, 
	const FShader* OriginalVS, 
	const FShader* OriginalPS, 
	const FMaterialRenderProxy* MaterialRenderProxy,
	const FMaterial& Material,
	const FSceneView& View
	)
{
	const int32 NumEngineColors = FMath::Min<int32>(GEngine->StreamingAccuracyColors.Num(), NumStreamingAccuracyColors);
	int32 ColorIndex = 0;
	for (; ColorIndex < NumEngineColors; ++ColorIndex)
	{
		SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), AccuracyColorsParameter, GEngine->StreamingAccuracyColors[ColorIndex], ColorIndex);
	}
	for (; ColorIndex < NumStreamingAccuracyColors; ++ColorIndex)
	{
		SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), AccuracyColorsParameter, FLinearColor::Black, ColorIndex);
	}

	// Bind view params
	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, FGlobalShader::GetPixelShader(), View.ViewUniformBuffer);
}

void FPrimitiveDistanceAccuracyPS::SetMesh(
	FRHICommandList& RHICmdList, 
	const FVertexFactory* VertexFactory,
	const FSceneView& View,
	const FPrimitiveSceneProxy* Proxy,
	int32 VisualizeLODIndex,
	const FMeshBatchElement& BatchElement, 
	const FDrawingPolicyRenderState& DrawRenderState
	)
{
	float CPULogDistance = -1.f;
#if WITH_EDITORONLY_DATA
	float Distance = 0;
	if (Proxy && Proxy->GetPrimitiveDistance(VisualizeLODIndex, BatchElement.VisualizeElementIndex, View.ViewMatrices.GetViewOrigin(), Distance))
	{
		CPULogDistance =  FMath::Max<float>(0.f, FMath::Log2(FMath::Max<float>(1.f, Distance)));
	}
#endif
	// Because the streamer use FMath::FloorToFloat, here we need to use -1 to have a useful result.
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), CPULogDistanceParameter, CPULogDistance);
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), PrimitiveAlphaParameter, (!Proxy || Proxy->IsSelected()) ? 1.f : .2f);
}

void FPrimitiveDistanceAccuracyPS::SetMesh(FRHICommandList& RHICmdList, const FSceneView& View)
{
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), CPULogDistanceParameter, -1.f);
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), PrimitiveAlphaParameter, 1.f);
}
