// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GlobalDistanceField.h
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "DistanceFieldAmbientOcclusion.h"

extern int32 GAOGlobalDistanceField;

inline bool UseGlobalDistanceField()
{
	return GAOGlobalDistanceField != 0;
}

inline bool UseGlobalDistanceField(const FDistanceFieldAOParameters& Parameters)
{
	return UseGlobalDistanceField() && Parameters.GlobalMaxOcclusionDistance > 0;
}

/** 
 * Updates the global distance field for a view.  
 * Typically issues updates for just the newly exposed regions of the volume due to camera movement.
 * In the worst case of a camera cut or large distance field scene changes, a full update of the global distance field will be done.
 **/
extern void UpdateGlobalDistanceFieldVolume(
	FRHICommandListImmediate& RHICmdList, 
	FViewInfo& View, 
	const FScene* Scene, 
	float MaxOcclusionDistance, 
	FGlobalDistanceFieldInfo& Info);
