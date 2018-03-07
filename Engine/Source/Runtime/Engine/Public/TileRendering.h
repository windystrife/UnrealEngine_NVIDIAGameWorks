// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TileRendering.h: Simple tile rendering implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HitProxies.h"

class FMaterialRenderProxy;
class FRHICommandListImmediate;
struct FDrawingPolicyRenderState;

class FTileRenderer
{
public:

	/**
	 * Draw a tile at the given location and size, using the given UVs
	 * (UV = [0..1]
	 */
	ENGINE_API static void DrawTile(FRHICommandListImmediate& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const class FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, bool bNeedsToSwitchVerticalAxis, float X, float Y, float SizeX, float SizeY, float U, float V, float SizeU, float SizeV, bool bIsHitTesting = false, const FHitProxyId HitProxyId = FHitProxyId(), const FColor InVertexColor = FColor(255, 255, 255, 255));

	/**
	 * Draw a rotated tile at the given location and size, using the given UVs
	 * (UV = [0..1]
	 */
	ENGINE_API static void DrawRotatedTile(FRHICommandListImmediate& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const class FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, bool bNeedsToSwitchVerticalAxis, const FQuat& Rotation, float X, float Y, float SizeX, float SizeY, float U, float V, float SizeU, float SizeV, bool bIsHitTesting = false, const FHitProxyId HitProxyId = FHitProxyId(), const FColor InVertexColor = FColor(255, 255, 255, 255));
	
private:

	/** This class never needs to be instantiated. */
	FTileRenderer() {}
};

