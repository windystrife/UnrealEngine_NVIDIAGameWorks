// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "Layout/SlateRect.h"

class FSlatePostProcessResource;
class IRendererModule;

struct FPostProcessRectParams
{
	FTexture2DRHIRef SourceTexture;
	FSlateRect SourceRect;
	FSlateRect DestRect;
	FIntPoint SourceTextureSize;
	TFunction<void(FGraphicsPipelineStateInitializer&)> RestoreStateFunc;
	TFunction<void()> RestoreStateFuncPostPipelineState;
};

struct FBlurRectParams
{
	int32 KernelSize;
	int32 DownsampleAmount;
	float Strength;
};

class FSlatePostProcessor
{
public:
	FSlatePostProcessor();
	~FSlatePostProcessor();

	void BlurRect(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FBlurRectParams& Params, const FPostProcessRectParams& RectParams);
	void ReleaseRenderTargets();
private:
	void DownsampleRect(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FPostProcessRectParams& Params, const FIntPoint& DownsampleSize);
	void UpsampleRect(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FPostProcessRectParams& Params, const FIntPoint& DownsampleSize);
	int32 ComputeBlurWeights(int32 KernelSize, float StdDev, TArray<FVector4>& OutWeightsAndOffsets);
private:
	FSlatePostProcessResource* IntermediateTargets;
};