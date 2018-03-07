// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldLightingPost.h
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "SceneRendering.h"

class FDistanceFieldAOParameters;

extern void AllocateOrReuseAORenderTarget(FRHICommandList& RHICmdList, TRefCountPtr<IPooledRenderTarget>& Target, const TCHAR* Name, EPixelFormat Format, uint32 Flags = 0);

extern void UpdateHistory(
	FRHICommandList& RHICmdList,
	const FViewInfo& View, 
	const TCHAR* BentNormalHistoryRTName,
	const TCHAR* ConfidenceHistoryRTName,
	const TCHAR* IrradianceHistoryRTName,
	IPooledRenderTarget* VelocityTexture,
	FSceneRenderTargetItem& DistanceFieldNormal,
	/** Contains last frame's history, if non-NULL.  This will be updated with the new frame's history. */
	TRefCountPtr<IPooledRenderTarget>* BentNormalHistoryState,
	TRefCountPtr<IPooledRenderTarget>* ConfidenceHistoryState,
	TRefCountPtr<IPooledRenderTarget>* IrradianceHistoryState,
	/** Source */
	TRefCountPtr<IPooledRenderTarget>& BentNormalSource, 
	TRefCountPtr<IPooledRenderTarget>& ConfidenceSource,
	TRefCountPtr<IPooledRenderTarget>& IrradianceSource, 
	/** Output of Temporal Reprojection for the next step in the pipeline. */
	TRefCountPtr<IPooledRenderTarget>& BentNormalHistoryOutput,
	TRefCountPtr<IPooledRenderTarget>& IrradianceHistoryOutput);

extern void UpsampleBentNormalAO(
	FRHICommandList& RHICmdList, 
	const TArray<FViewInfo>& Views, 
	TRefCountPtr<IPooledRenderTarget>& DistanceFieldAOBentNormal, 
	TRefCountPtr<IPooledRenderTarget>& DistanceFieldIrradiance,
	bool bModulateSceneColor,
	bool bVisualizeAmbientOcclusion,
	bool bVisualizeGlobalIllumination);
