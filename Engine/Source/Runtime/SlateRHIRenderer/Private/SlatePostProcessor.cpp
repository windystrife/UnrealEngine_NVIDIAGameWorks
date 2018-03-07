// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SlatePostProcessor.h"
#include "SlatePostProcessResource.h"
#include "SlateShaders.h"
#include "ScreenRendering.h"
#include "SceneUtils.h"
#include "RendererInterface.h"
#include "StaticBoundShaderState.h"
#include "PipelineStateCache.h"

DECLARE_CYCLE_STAT(TEXT("Slate PostProcessing RT"), STAT_SlatePostProcessingRTTime, STATGROUP_Slate);

FSlatePostProcessor::FSlatePostProcessor()
{
	const int32 NumIntermediateTargets = 2;
	IntermediateTargets = new FSlatePostProcessResource(NumIntermediateTargets);
	BeginInitResource(IntermediateTargets);
}

FSlatePostProcessor::~FSlatePostProcessor()
{
	// Note this is deleted automatically because it implements FDeferredCleanupInterface.
	IntermediateTargets->CleanUp();

}

void FSlatePostProcessor::BlurRect(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FBlurRectParams& Params, const FPostProcessRectParams& RectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_SlatePostProcessingRTTime);

	TArray<FVector4> WeightsAndOffsets;
	const int32 SampleCount = ComputeBlurWeights(Params.KernelSize, Params.Strength, WeightsAndOffsets);


	const bool bDownsample = Params.DownsampleAmount > 0;

	FIntPoint DestRectSize = RectParams.DestRect.GetSize().IntPoint();
	FIntPoint RequiredSize = bDownsample
										? FIntPoint(FMath::DivideAndRoundUp(DestRectSize.X, Params.DownsampleAmount), FMath::DivideAndRoundUp(DestRectSize.Y, Params.DownsampleAmount))
										: DestRectSize;

	// The max size can get ridiculous with large scale values.  Clamp to size of the backbuffer
	RequiredSize.X = FMath::Min(RequiredSize.X, RectParams.SourceTextureSize.X);
	RequiredSize.Y = FMath::Min(RequiredSize.Y, RectParams.SourceTextureSize.Y);
	
	SCOPED_DRAW_EVENTF(RHICmdList, SlatePostProcess, TEXT("Slate Post Process Blur Background Kernel: %dx%d Size: %dx%d"), SampleCount, SampleCount, RequiredSize.X, RequiredSize.Y);


	const FIntPoint DownsampleSize = RequiredSize;

	IntermediateTargets->Update(RequiredSize);

	if(bDownsample)
	{
		DownsampleRect(RHICmdList, RendererModule, RectParams, DownsampleSize);
	}

#if 1
	FSamplerStateRHIRef BilinearClamp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	check(ShaderMap);

	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
	TShaderMapRef<FSlatePostProcessBlurPS> PixelShader(ShaderMap);

	const int32 SrcTextureWidth = RectParams.SourceTextureSize.X;
	const int32 SrcTextureHeight = RectParams.SourceTextureSize.Y;

	const int32 DestTextureWidth = IntermediateTargets->GetWidth();
	const int32 DestTextureHeight = IntermediateTargets->GetHeight();

	const FSlateRect& SourceRect = RectParams.SourceRect;
	const FSlateRect& DestRect = RectParams.DestRect;

	FVertexDeclarationRHIRef VertexDecl = RendererModule.GetFilterVertexDeclaration().VertexDeclarationRHI;
	check(IsValidRef(VertexDecl));
	
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	RHICmdList.SetViewport(0, 0, 0, DestTextureWidth, DestTextureHeight, 0.0f);
	
	const FVector2D InvBufferSize = FVector2D(1.0f / DestTextureWidth, 1.0f / DestTextureHeight);
	const FVector2D HalfTexelOffset = FVector2D(0.5f/ DestTextureWidth, 0.5f/ DestTextureHeight);

	for (int32 PassIndex = 0; PassIndex < 2; ++PassIndex)
	{
		// First pass render to the render target with the post process fx
		if (PassIndex == 0)
		{
			FTexture2DRHIRef SourceTexture = bDownsample ? IntermediateTargets->GetRenderTarget(0) : RectParams.SourceTexture;
			FTexture2DRHIRef DestTexture = IntermediateTargets->GetRenderTarget(1);

			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SourceTexture);
			RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, DestTexture);


			SetRenderTarget(RHICmdList, DestTexture, FTextureRHIRef());
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = VertexDecl;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			PixelShader->SetWeightsAndOffsets(RHICmdList, WeightsAndOffsets, SampleCount);
			PixelShader->SetTexture(RHICmdList, SourceTexture, BilinearClamp);
		
			if(bDownsample)
			{
				PixelShader->SetUVBounds(RHICmdList, FVector4(FVector2D::ZeroVector, FVector2D((float)DownsampleSize.X/DestTextureWidth, (float)DownsampleSize.Y / DestTextureHeight) - HalfTexelOffset));
				PixelShader->SetBufferSizeAndDirection(RHICmdList, InvBufferSize, FVector2D(1, 0));

				RendererModule.DrawRectangle(
					RHICmdList,
					0, 0,
					DownsampleSize.X, DownsampleSize.Y,
					0, 0,
					DownsampleSize.X, DownsampleSize.Y,
					FIntPoint(DestTextureWidth, DestTextureHeight),
					FIntPoint(DestTextureWidth, DestTextureHeight),
					*VertexShader,
					EDRF_Default);
			}
			else
			{
				const FVector2D InvSrcTetureSize(1.f / SrcTextureWidth, 1.f / SrcTextureHeight);

				const FVector2D UVStart = FVector2D(DestRect.Left, DestRect.Top) * InvSrcTetureSize;
				const FVector2D UVEnd = FVector2D(DestRect.Right, DestRect.Bottom) * InvSrcTetureSize;
				const FVector2D SizeUV = UVEnd - UVStart;

				PixelShader->SetUVBounds(RHICmdList, FVector4(UVStart, UVEnd));
				PixelShader->SetBufferSizeAndDirection(RHICmdList, InvSrcTetureSize, FVector2D(1, 0));

				RendererModule.DrawRectangle(
					RHICmdList,
					0, 0,
					RequiredSize.X, RequiredSize.Y,
					UVStart.X, UVStart.Y,
					SizeUV.X, SizeUV.Y,
					FIntPoint(DestTextureWidth, DestTextureHeight),
					FIntPoint(1, 1),
					*VertexShader,
					EDRF_Default);
			}

		}
		else
		{
			FTexture2DRHIRef SourceTexture = IntermediateTargets->GetRenderTarget(1);
			FTexture2DRHIRef DestTexture = IntermediateTargets->GetRenderTarget(0);

			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SourceTexture);
			RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, DestTexture);

			SetRenderTarget(RHICmdList, DestTexture, FTextureRHIRef());
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = VertexDecl;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			
			PixelShader->SetWeightsAndOffsets(RHICmdList, WeightsAndOffsets, SampleCount);
			PixelShader->SetUVBounds(RHICmdList, FVector4(FVector2D::ZeroVector, FVector2D((float)DownsampleSize.X/DestTextureWidth, (float)DownsampleSize.Y / DestTextureHeight) - HalfTexelOffset));
			PixelShader->SetTexture(RHICmdList, SourceTexture, BilinearClamp);
			PixelShader->SetBufferSizeAndDirection(RHICmdList, InvBufferSize, FVector2D(0, 1));

			RendererModule.DrawRectangle(
				RHICmdList,
				0, 0,
				DownsampleSize.X, DownsampleSize.Y,
				0, 0,
				DownsampleSize.X, DownsampleSize.Y,
				FIntPoint(DestTextureWidth, DestTextureHeight),
				FIntPoint(DestTextureWidth, DestTextureHeight),
				*VertexShader,
				EDRF_Default);
		}

	
	}

#endif
	UpsampleRect(RHICmdList, RendererModule, RectParams, DownsampleSize);
}

void FSlatePostProcessor::ReleaseRenderTargets()
{
	check(IsInGameThread());
	// Only release the resource not delete it.  Deleting it could cause issues on any RHI thread
	BeginReleaseResource(IntermediateTargets);
}

void FSlatePostProcessor::DownsampleRect(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FPostProcessRectParams& Params, const FIntPoint& DownsampleSize)
{
	SCOPED_DRAW_EVENT(RHICmdList, SlatePostProcessDownsample);

	// Source is the viewport.  This is the width and height of the viewport backbuffer
	const int32 SrcTextureWidth = Params.SourceTextureSize.X;
	const int32 SrcTextureHeight = Params.SourceTextureSize.Y;

	// Dest is the destination quad for the downsample
	const int32 DestTextureWidth = IntermediateTargets->GetWidth();
	const int32 DestTextureHeight = IntermediateTargets->GetHeight();

	// Rect of the viewport
	const FSlateRect& SourceRect = Params.SourceRect;

	// Rect of the final destination post process effect (not downsample rect).  This is the area we sample from
	const FSlateRect& DestRect = Params.DestRect;

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);

	FSamplerStateRHIRef BilinearClamp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	FTexture2DRHIRef DestTexture = IntermediateTargets->GetRenderTarget(0);

	// Downsample and store in intermediate texture
	{
		TShaderMapRef<FSlatePostProcessDownsamplePS> PixelShader(ShaderMap);

		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, Params.SourceTexture);
		RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, DestTexture);

		const FVector2D InvSrcTetureSize(1.f/SrcTextureWidth, 1.f/SrcTextureHeight);

		const FVector2D UVStart = FVector2D(DestRect.Left, DestRect.Top) * InvSrcTetureSize;
		const FVector2D UVEnd = FVector2D(DestRect.Right, DestRect.Bottom) * InvSrcTetureSize;
		const FVector2D SizeUV = UVEnd - UVStart;
		
		RHICmdList.SetViewport(0, 0, 0, DestTextureWidth, DestTextureHeight, 0.0f);
		RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

		SetRenderTarget(RHICmdList, DestTexture, FTextureRHIRef());
		
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = RendererModule.GetFilterVertexDeclaration().VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
		
		PixelShader->SetShaderParams(RHICmdList, FVector4(InvSrcTetureSize.X, InvSrcTetureSize.Y, 0, 0));
		PixelShader->SetUVBounds(RHICmdList, FVector4(UVStart, UVEnd));
		PixelShader->SetTexture(RHICmdList, Params.SourceTexture, BilinearClamp);

		RendererModule.DrawRectangle(
			RHICmdList,
			0, 0,
			DownsampleSize.X, DownsampleSize.Y,
			UVStart.X, UVStart.Y,
			SizeUV.X, SizeUV.Y,
			FIntPoint(DestTextureWidth, DestTextureHeight),
			FIntPoint(1,1), 
			*VertexShader,
			EDRF_Default);
	}
	
	// Testing only
#if 0
	UpsampleRect(RHICmdList, RendererModule, Params, DownsampleSize);
#endif
}

void FSlatePostProcessor::UpsampleRect(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FPostProcessRectParams& Params, const FIntPoint& DownsampleSize)
{
	SCOPED_DRAW_EVENT(RHICmdList, SlatePostProcessUpsample);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	// Original source texture is now the destination texture
	FTexture2DRHIRef DestTexture = Params.SourceTexture;
	const int32 DestTextureWidth = Params.SourceTextureSize.X;
	const int32 DestTextureHeight = Params.SourceTextureSize.Y;

	const int32 DownsampledWidth = DownsampleSize.X;
	const int32 DownsampledHeight = DownsampleSize.Y;

	// Source texture is the texture that was originally downsampled
	FTexture2DRHIRef SrcTexture = IntermediateTargets->GetRenderTarget(0);
	const int32 SrcTextureWidth = IntermediateTargets->GetWidth();
	const int32 SrcTextureHeight = IntermediateTargets->GetHeight();

	const FSlateRect& SourceRect = Params.SourceRect;
	const FSlateRect& DestRect = Params.DestRect;

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);

	FSamplerStateRHIRef BilinearClamp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	RHICmdList.SetViewport(0, 0, 0, DestTextureWidth, DestTextureHeight, 0.0f);

	// Perform Writable transitions first
	RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, DestTexture);
	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SrcTexture);

	SetRenderTarget(RHICmdList, DestTexture, FTextureRHIRef());
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	Params.RestoreStateFunc(GraphicsPSOInit);

	TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = RendererModule.GetFilterVertexDeclaration().VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	//SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	FLocalGraphicsPipelineState BaseGraphicsPSO = RHICmdList.BuildLocalGraphicsPipelineState(GraphicsPSOInit);
	RHICmdList.SetLocalGraphicsPipelineState(BaseGraphicsPSO);

	Params.RestoreStateFuncPostPipelineState();

	PixelShader->SetParameters(RHICmdList, BilinearClamp, SrcTexture);

	RendererModule.DrawRectangle(RHICmdList,
		DestRect.Left, DestRect.Top,
		DestRect.Right - DestRect.Left, DestRect.Bottom - DestRect.Top,
		0, 0,
		(float)DownsampledWidth/SrcTextureWidth-1.0/ SrcTextureWidth, (float)DownsampledHeight/SrcTextureHeight-1.0 / SrcTextureHeight,
		Params.SourceTextureSize,
		FIntPoint(1, 1),
		*VertexShader,
		EDRF_Default);

}
#define BILINEAR_FILTER_METHOD 1

#if !BILINEAR_FILTER_METHOD

static int32 ComputeWeights(int32 KernelSize, float Sigma, TArray<FVector4>& OutWeightsAndOffsets)
{
	OutWeightsAndOffsets.AddUninitialized(KernelSize / 2 + 1);

	int32 SampleIndex = 0;
	for (int32 X = 0; X < KernelSize; X += 2)
	{
		float Dist = X;
		FVector4 WeightAndOffset;
		WeightAndOffset.X = (1.0f / FMath::Sqrt(2 * PI*Sigma*Sigma))*FMath::Exp(-(Dist*Dist) / (2 * Sigma*Sigma));
		WeightAndOffset.Y = Dist;

		Dist = X + 1;
		WeightAndOffset.Z = (1.0f / FMath::Sqrt(2 * PI*Sigma*Sigma))*FMath::Exp(-(Dist*Dist) / (2 * Sigma*Sigma));
		WeightAndOffset.W = Dist;

		OutWeightsAndOffsets[SampleIndex] = WeightAndOffset;

		++SampleIndex;
	}

	return KernelSize;
};

#else

static float GetWeight(float Dist, float Strength)
{
	// from https://en.wikipedia.org/wiki/Gaussian_blur
	float Strength2 = Strength*Strength;
	return (1.0f / FMath::Sqrt(2 * PI*Strength2))*FMath::Exp(-(Dist*Dist) / (2 * Strength2));
}

static FVector2D GetWeightAndOffset(float Dist, float Sigma)
{
	float Offset1 = Dist;
	float Weight1 = GetWeight(Offset1, Sigma);

	float Offset2 = Dist + 1;
	float Weight2 = GetWeight(Offset2, Sigma);

	float TotalWeight = Weight1 + Weight2;

	float Offset = 0;
	if (TotalWeight > 0)
	{
		Offset = (Weight1*Offset1 + Weight2*Offset2) / TotalWeight;
	}


	return FVector2D(TotalWeight, Offset);
}

static int32 ComputeWeights(int32 KernelSize, float Sigma, TArray<FVector4>& OutWeightsAndOffsets)
{
	int32 NumSamples = FMath::DivideAndRoundUp(KernelSize, 2);

	// We need half of the sample count array because we're packing two samples into one float4

	OutWeightsAndOffsets.AddUninitialized(NumSamples%2 == 0 ? NumSamples / 2 : NumSamples/2+1);

	OutWeightsAndOffsets[0] = FVector4(FVector2D(GetWeight(0,Sigma), 0), GetWeightAndOffset(1, Sigma) );
	int32 SampleIndex = 1;
	for (int32 X = 3; X < KernelSize; X += 4)
	{
		OutWeightsAndOffsets[SampleIndex] = FVector4(GetWeightAndOffset(X, Sigma), GetWeightAndOffset(X + 2, Sigma));

		++SampleIndex;
	}

	return NumSamples;
};

#endif

int32 FSlatePostProcessor::ComputeBlurWeights(int32 KernelSize, float StdDev, TArray<FVector4>& OutWeightsAndOffsets)
{
	return ComputeWeights(KernelSize, StdDev, OutWeightsAndOffsets);
}
