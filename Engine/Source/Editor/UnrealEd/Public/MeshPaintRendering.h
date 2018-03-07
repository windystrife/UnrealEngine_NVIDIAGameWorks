// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	MeshPaintRendering.h: Mesh texture paint brush rendering
================================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"

class FRHICommandList;
class UTextureRenderTarget2D;
class FGraphicsPipelineStateInitializer;

namespace MeshPaintRendering
{
	/** Batched element parameters for mesh paint shaders */
	struct FMeshPaintShaderParameters
	{

	public:

		// @todo MeshPaint: Should be serialized no?
		UTextureRenderTarget2D* CloneTexture;

		FMatrix WorldToBrushMatrix;

		float BrushRadius;
		float BrushRadialFalloffRange;
		float BrushDepth;
		float BrushDepthFalloffRange;
		float BrushStrength;
		FLinearColor BrushColor;
		bool RedChannelFlag;
		bool BlueChannelFlag;
		bool GreenChannelFlag;
		bool AlphaChannelFlag;
		bool GenerateMaskFlag;


	};


	/** Batched element parameters for mesh paint dilation shaders used for seam painting */
	struct FMeshPaintDilateShaderParameters
	{

	public:

		UTextureRenderTarget2D* Texture0;
		UTextureRenderTarget2D* Texture1;
		UTextureRenderTarget2D* Texture2;

		float WidthPixelOffset;
		float HeightPixelOffset;

	};


	/** Binds the mesh paint vertex and pixel shaders to the graphics device */
	void UNREALED_API SetMeshPaintShaders(FRHICommandList& RHICmdList,
											FGraphicsPipelineStateInitializer& GraphicsPSOInit,
											ERHIFeatureLevel::Type InFeatureLevel, 
											const FMatrix& InTransform,
											const float InGamma,
											const FMeshPaintShaderParameters& InShaderParams );

	/** Binds the mesh paint dilation vertex and pixel shaders to the graphics device */
	void UNREALED_API SetMeshPaintDilateShaders(FRHICommandList& RHICmdList, 
													FGraphicsPipelineStateInitializer& GraphicsPSOInit,
													ERHIFeatureLevel::Type InFeatureLevel, 
													const FMatrix& InTransform,
													const float InGamma,
													const FMeshPaintDilateShaderParameters& InShaderParams );

}
