// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
RequiredTextureResolutionRendering.cpp: Contains definitions for rendering the viewmode.
=============================================================================*/

#include "RequiredTextureResolutionRendering.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"

IMPLEMENT_MATERIAL_SHADER_TYPE(,FRequiredTextureResolutionPS,TEXT("/Engine/Private/RequiredTextureResolutionPixelShader.usf"),TEXT("Main"),SF_Pixel);

void FRequiredTextureResolutionPS::SetParameters(
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

	const int32 ViewModeParam = View.Family->GetViewModeParam();
	const FName ViewModeParamName = View.Family->GetViewModeParamName();

	int32 AnalysisIndex = INDEX_NONE;
	int32 TextureResolution = 64;

	FMaterialRenderContext MaterialContext(MaterialRenderProxy, Material, &View);
	const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& ExpressionsByType = Material.GetUniform2DTextureExpressions();

	if (ViewModeParam != INDEX_NONE && ViewModeParamName == NAME_None) // If displaying texture per texture indices
	{
		AnalysisIndex = ViewModeParam;

		for (FMaterialUniformExpressionTexture* Expression : ExpressionsByType)
		{
			if (Expression && Expression->GetTextureIndex() == ViewModeParam)
			{
				const UTexture* Texture = nullptr;
				ESamplerSourceMode SourceMode;
				Expression->GetTextureValue(MaterialContext, Material, Texture, SourceMode);
				const UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
				if (Texture2D && Texture2D->Resource)
				{
					FTexture2DResource* Texture2DResource =  (FTexture2DResource*)Texture2D->Resource;
					if (Texture2DResource->GetTexture2DRHI().IsValid())
					{
						TextureResolution = 1 << (Texture2DResource->GetTexture2DRHI()->GetNumMips() - 1);
					}
					break;
				}
			}
		}
	}
	else if (ViewModeParam != INDEX_NONE) // Otherwise show only texture matching the given name
	{
		AnalysisIndex = 1024; // Make sure not to find anything by default.
		for (FMaterialUniformExpressionTexture* Expression : ExpressionsByType)
		{
			if (Expression)
			{
				const UTexture* Texture = nullptr;
				ESamplerSourceMode SourceMode;
				Expression->GetTextureValue(MaterialContext, Material, Texture, SourceMode);
				if (Texture && Texture->GetFName() == ViewModeParamName)
				{
					const UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
					if (Texture2D && Texture2D->Resource)
					{
						FTexture2DResource* Texture2DResource =  (FTexture2DResource*)Texture2D->Resource;
						if (Texture2DResource->GetTexture2DRHI().IsValid())
						{
							AnalysisIndex = Expression->GetTextureIndex();
							TextureResolution = 1 << (Texture2DResource->GetTexture2DRHI()->GetNumMips() - 1);
						}
						break;
					}
				}
			}
		}
	}

	SetShaderValue(RHICmdList, FMeshMaterialShader::GetPixelShader(), AnalysisParamsParameter, FIntVector4(AnalysisIndex, TextureResolution, 0, 0));

	FMeshMaterialShader::SetParameters(RHICmdList, FMeshMaterialShader::GetPixelShader(), MaterialRenderProxy, Material, View, View.ViewUniformBuffer, ESceneRenderTargetsMode::SetTextures);
}

void FRequiredTextureResolutionPS::SetMesh(
	FRHICommandList& RHICmdList, 
	const FVertexFactory* VertexFactory,
	const FSceneView& View,
	const FPrimitiveSceneProxy* Proxy,
	int32 VisualizeLODIndex,
	const FMeshBatchElement& BatchElement, 
	const FDrawingPolicyRenderState& DrawRenderState
	)
{
	SetShaderValue(RHICmdList, FMeshMaterialShader::GetPixelShader(), PrimitiveAlphaParameter, (!Proxy || Proxy->IsSelected()) ? 1.f : .2f);

	FMeshMaterialShader::SetMesh(RHICmdList, FMeshMaterialShader::GetPixelShader(), VertexFactory, View, Proxy, BatchElement, DrawRenderState);
}
