// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DecalRenderingShared.h
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "DecalRenderingCommon.h"

class FDeferredDecalProxy;
class FScene;
class FViewInfo;

/**
 * Compact decal data for rendering
 */
struct FTransientDecalRenderData
{
	const FMaterialRenderProxy* MaterialProxy;
	const FMaterial* MaterialResource;
	const FDeferredDecalProxy* DecalProxy;
	float FadeAlpha;
	float ConservativeRadius;
	EDecalBlendMode DecalBlendMode;
	bool bHasNormal;

	FTransientDecalRenderData(const FScene& InScene, const FDeferredDecalProxy* InDecalProxy, float InConservativeRadius);
};
	
typedef TArray<FTransientDecalRenderData, SceneRenderingAllocator> FTransientDecalRenderDataList;

/**
 * Shared decal functionality for deferred and forward shading
 */
struct FDecalRendering
{
	static FMatrix ComputeComponentToClipMatrix(const FViewInfo& View, const FMatrix& DecalComponentToWorld);
	static void SetVertexShaderOnly(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View, const FMatrix& FrustumComponentToClip);
	static void BuildVisibleDecalList(const FScene& Scene, const FViewInfo& View, EDecalRenderStage DecalRenderStage, FTransientDecalRenderDataList& OutVisibleDecals);
	static void SetShader(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View, const FTransientDecalRenderData& DecalData, const FMatrix& FrustumComponentToClip);
};
