// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MaterialTexCoordScalesRendering.cpp: Contains definitions for rendering the viewmode.
=============================================================================*/

#include "MaterialTexCoordScalesRendering.h"
#include "PrimitiveSceneProxy.h"
#include "EngineGlobals.h"
#include "MeshBatch.h"
#include "Engine/Engine.h"

IMPLEMENT_MATERIAL_SHADER_TYPE(,FMaterialTexCoordScalePS,TEXT("/Engine/Private/MaterialTexCoordScalesPixelShader.usf"),TEXT("Main"),SF_Pixel);

void FMaterialTexCoordScalePS::SetParameters(
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
		SetShaderValue(RHICmdList, FMeshMaterialShader::GetPixelShader(), AccuracyColorsParameter, GEngine->StreamingAccuracyColors[ColorIndex], ColorIndex);
	}
	for (; ColorIndex < NumStreamingAccuracyColors; ++ColorIndex)
	{
		SetShaderValue(RHICmdList, FMeshMaterialShader::GetPixelShader(), AccuracyColorsParameter, FLinearColor::Black, ColorIndex);
	}

	// Don't allow scene texture as this create issues when running command let.
	FMeshMaterialShader::SetParameters(RHICmdList, FMeshMaterialShader::GetPixelShader(), MaterialRenderProxy, Material, View, View.ViewUniformBuffer, ESceneRenderTargetsMode::DontSet);
}

void FMaterialTexCoordScalePS::SetMesh(
	FRHICommandList& RHICmdList, 
	const FVertexFactory* VertexFactory,
	const FSceneView& View,
	const FPrimitiveSceneProxy* Proxy,
	int32 VisualizeLODIndex,
	const FMeshBatchElement& BatchElement, 
	const FDrawingPolicyRenderState& DrawRenderState
	)
{
	const int32 AnalysisIndex = View.Family->GetViewModeParam() >= 0 ? FMath::Clamp<int32>(View.Family->GetViewModeParam(), 0, TEXSTREAM_MAX_NUM_TEXTURES_PER_MATERIAL - 1) : -1;

	FVector4 OneOverCPUTexCoordScales[TEXSTREAM_MAX_NUM_TEXTURES_PER_MATERIAL / 4];
	FIntVector4 TexCoordIndices[TEXSTREAM_MAX_NUM_TEXTURES_PER_MATERIAL / 4];

	FMemory::Memzero(OneOverCPUTexCoordScales); // 0 remap to irrelevant data.
	FMemory::Memzero(TexCoordIndices);

#if WITH_EDITORONLY_DATA
	if (Proxy)
	{
		Proxy->GetMaterialTextureScales(VisualizeLODIndex, BatchElement.VisualizeElementIndex, nullptr, OneOverCPUTexCoordScales, TexCoordIndices);
	}
#endif
	const bool bOutputScales = View.Family->GetDebugViewShaderMode() == DVSM_OutputMaterialTextureScales;

	SetShaderValueArray(RHICmdList, FMeshMaterialShader::GetPixelShader(), OneOverCPUTexCoordScalesParameter, OneOverCPUTexCoordScales, ARRAY_COUNT(OneOverCPUTexCoordScales));
	SetShaderValueArray(RHICmdList, FMeshMaterialShader::GetPixelShader(), TexCoordIndicesParameter, TexCoordIndices, ARRAY_COUNT(TexCoordIndices));
	SetShaderValue(RHICmdList, FMeshMaterialShader::GetPixelShader(), AnalysisParamsParameter, FIntVector4(bOutputScales ? -1 : AnalysisIndex, bOutputScales ? 1 : 0, 0, 0));
	SetShaderValue(RHICmdList, FMeshMaterialShader::GetPixelShader(), PrimitiveAlphaParameter, (!Proxy || Proxy->IsSelected()) ? 1.f : .2f);

	FMeshMaterialShader::SetMesh(RHICmdList, FMeshMaterialShader::GetPixelShader(), VertexFactory, View, Proxy, BatchElement, DrawRenderState);
}
