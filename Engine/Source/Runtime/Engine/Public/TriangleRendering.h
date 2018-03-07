// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TriangleRendering.h: Simple triangle rendering implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HitProxies.h"

class FMaterialRenderProxy;
class FRHICommandListImmediate;
struct FDrawingPolicyRenderState;
struct FCanvasUVTri;

class FTriangleRenderer
{
public:

	/**
	 * Draw a triangle using the given vertices using optional color.
	 */
	ENGINE_API static void DrawTriangle(FRHICommandListImmediate& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const class FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, bool bNeedsToSwitchVerticalAxis, const FCanvasUVTri& Tri, bool bIsHitTesting = false, const FHitProxyId HitProxyId = FHitProxyId(), const FColor InVertexColor = FColor(255, 255, 255, 255));
	
	/**
	* Draw a triangle using the given vertices. Same as DrawTriangle but uses the colors in the supplied vertices
	*/
	ENGINE_API static void DrawTriangleUsingVertexColor(FRHICommandListImmediate& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const class FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, bool bNeedsToSwitchVerticalAxis, const FCanvasUVTri& Tri, bool bIsHitTesting, const FHitProxyId HitProxyId);

private:

	/** This class never needs to be instantiated. */
	FTriangleRenderer() {}
};

