// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
FlexFluidSurfaceRendering.h: Flex fluid surface rendering definitions.
=============================================================================*/

#pragma once

#include "ShaderBaseClasses.h"
#include "ScenePrivate.h"

class FFlexFluidSurfaceRenderer
{
public:

	/**
	 * Iterates through DynamicMeshElements picking out all corresponding fluid surface proxies and storing them 
	 * for later stages. Then selecting all visibe particle system mesh elements corresponding to thouse surface proxies.
	 * Allocates render textures per fluid surface.
	 */
	void UpdateProxiesAndResources(FRHICommandList& RHICmdList, TArray<FMeshBatchAndRelevance, SceneRenderingAllocator>& DynamicMeshElements, FSceneRenderTargets& SceneContext);

	/**
	 * Clears the textures, renders particles for depth and thickness, smoothes the depth texture.
	 */
	void RenderParticles(FRHICommandList& RHICmdList, const TArray<FViewInfo>& Views);

	/**
	 * Renders opaque surfaces.
	 */
	void RenderBasePass(FRHICommandList& RHICmdList, const TArray<FViewInfo>& Views);

	/**
	 * Figures out whether FFlexFluidSurfaceRenderer::RenderDepth needs to be called for a given scene proxy.
	 * Returns true for fluid surfaces and particle systems corresponding to fluid surfaces.
	 */
	bool IsDepthMaskingRequired(const FPrimitiveSceneProxy* SceneProxy);

	/**
	 * Render Depth for fluid surfaces used for masking the static pre-shadows, 
	 * skips any particle systems.
	 */
	void RenderDepth(FRHICommandList& RHICmdList, FPrimitiveSceneProxy* SceneProxy, const FViewInfo& View);

	/**
	 * Clears the temporarily stored surface proxies.
	 */
	void Cleanup();

private:

	TArray<class FFlexFluidSurfaceSceneProxy*> SurfaceSceneProxies;
};

extern FFlexFluidSurfaceRenderer GFlexFluidSurfaceRenderer;