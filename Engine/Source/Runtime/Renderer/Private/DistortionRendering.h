// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistortionRendering.h: Distortion rendering implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"

class FPrimitiveSceneProxy;
struct FDrawingPolicyRenderState;

/** 
* Set of distortion scene prims  
*/
class FDistortionPrimSet
{
public:

	/** 
	* Iterate over the distortion prims and draw their accumulated offsets
	* @param ViewInfo - current view used to draw items
	* @param DPGIndex - current DPG used to draw items
	* @return true if anything was drawn
	*/
	bool DrawAccumulatedOffsets(FRHICommandListImmediate& RHICmdList, const class FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, bool bInitializeOffsets);

	/**
	* Adds a new primitives to the list of distortion prims
	* @param PrimitiveSceneProxies - primitive info to add.
	*/
	void Append(FPrimitiveSceneProxy** PrimitiveSceneProxies, int32 NumProxies)
	{
		Prims.Append(PrimitiveSceneProxies, NumProxies);
	}

	/** 
	* @return number of prims to render
	*/
	int32 NumPrims() const
	{
		return Prims.Num();
	}

	/** 
	* @return a prim currently set to render
	*/
	const FPrimitiveSceneProxy* GetPrim(int32 i)const
	{
		check(i>=0 && i<NumPrims());
		return Prims[i];
	}

private:
	/** list of distortion prims added from the scene */
	TArray<FPrimitiveSceneProxy*, SceneRenderingAllocator> Prims;
};
