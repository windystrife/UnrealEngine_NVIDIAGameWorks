// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================

=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "SHMath.h"
#include "ScenePrivate.h"

namespace MobileReflectionEnvironmentCapture
{
	void ComputeAverageBrightness(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, int32 CubmapSize, float& OutAverageBrightness);
	void FilterReflectionEnvironment(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, int32 CubmapSize, FSHVectorRGB3* OutIrradianceEnvironmentMap);
	void CopyToSkyTexture(FRHICommandList& RHICmdList, FScene* Scene, FTexture* ProcessedTexture);
}