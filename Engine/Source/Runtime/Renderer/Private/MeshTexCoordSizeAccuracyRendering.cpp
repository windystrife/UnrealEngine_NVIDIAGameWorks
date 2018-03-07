// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MeshTexCoordSizeAccuracyRendering.cpp: Contains definitions for rendering the viewmode.
=============================================================================*/

#include "MeshTexCoordSizeAccuracyRendering.h"
#include "Components.h"
#include "PrimitiveSceneProxy.h"
#include "EngineGlobals.h"
#include "MeshBatch.h"
#include "Engine/Engine.h"

IMPLEMENT_SHADER_TYPE(,FMeshTexCoordSizeAccuracyPS,TEXT("/Engine/Private/MeshTexCoordSizeAccuracyPixelShader.usf"),TEXT("Main"),SF_Pixel);

void FMeshTexCoordSizeAccuracyPS::SetParameters(
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

void FMeshTexCoordSizeAccuracyPS::SetMesh(
	FRHICommandList& RHICmdList, 
	const FVertexFactory* VertexFactory,
	const FSceneView& View,
	const FPrimitiveSceneProxy* Proxy,
	int32 VisualizeLODIndex,
	const FMeshBatchElement& BatchElement, 
	const FDrawingPolicyRenderState& DrawRenderState
	)
{
	const int32 AnalysisIndex = View.Family->GetViewModeParam() >= 0 ? FMath::Clamp<int32>(View.Family->GetViewModeParam(), 0, MAX_TEXCOORDS - 1) : -1;

	FVector4 WorldUVDensities;
#if WITH_EDITORONLY_DATA
	if (!Proxy || !Proxy->GetMeshUVDensities(VisualizeLODIndex, BatchElement.VisualizeElementIndex, WorldUVDensities))
#endif
	{
		FMemory::Memzero(WorldUVDensities);
	}

	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), CPUTexelFactorParameter, WorldUVDensities);
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), PrimitiveAlphaParameter, (!Proxy || Proxy->IsSelected()) ? 1.f : .2f);
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), TexCoordAnalysisIndexParameter, AnalysisIndex);
}

void FMeshTexCoordSizeAccuracyPS::SetMesh(FRHICommandList& RHICmdList, const FSceneView& View)
{
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), CPUTexelFactorParameter, -1.f);
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), PrimitiveAlphaParameter, 1.f);
}
