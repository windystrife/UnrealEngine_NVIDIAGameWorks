// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 
=============================================================================*/
#include "MobileReflectionEnvironmentCapture.h"
#include "ReflectionEnvironmentCapture.h"
#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"
#include "SceneRenderTargets.h"
#include "SceneUtils.h"
#include "ScreenRendering.h"
#include "PipelineStateCache.h"
#include "SceneFilterRendering.h"
#include "OneColorShader.h"

extern int32 GDiffuseIrradianceCubemapSize;
extern float ComputeSingleAverageBrightnessFromCubemap(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, int32 TargetSize, FSceneRenderTargetItem& Cubemap);
extern void FullyResolveReflectionScratchCubes(FRHICommandListImmediate& RHICmdList);

class FMobileDownsamplePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMobileDownsamplePS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsMobilePlatform(Platform);
	}

	FMobileDownsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		CubeFace.Bind(Initializer.ParameterMap, TEXT("CubeFace"));
		SourceMipIndex.Bind(Initializer.ParameterMap, TEXT("SourceMipIndex"));
		SourceTexture.Bind(Initializer.ParameterMap, TEXT("SourceTexture"));
		SourceTextureSampler.Bind(Initializer.ParameterMap, TEXT("SourceTextureSampler"));
	}
	FMobileDownsamplePS() {}

	void SetParameters(FRHICommandList& RHICmdList, int32 CubeFaceValue, int32 SourceMipIndexValue, FSceneRenderTargetItem& SourceTextureValue)
	{
		SetShaderValue(RHICmdList, GetPixelShader(), CubeFace, CubeFaceValue);
		SetShaderValue(RHICmdList, GetPixelShader(), SourceMipIndex, SourceMipIndexValue);

		SetTextureParameter(
			RHICmdList,
			GetPixelShader(),
			SourceTexture,
			SourceTextureSampler,
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(),
			SourceTextureValue.ShaderResourceTexture);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << CubeFace;
		Ar << SourceMipIndex;
		Ar << SourceTexture;
		Ar << SourceTextureSampler;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter CubeFace;
	FShaderParameter SourceMipIndex;
	FShaderResourceParameter SourceTexture;
	FShaderResourceParameter SourceTextureSampler;
};

IMPLEMENT_SHADER_TYPE(, FMobileDownsamplePS, TEXT("/Engine/Private/ReflectionEnvironmentShaders.usf"), TEXT("DownsamplePS_Mobile"), SF_Pixel);
namespace MobileReflectionEnvironmentCapture
{
	/** Encapsulates render target picking logic for cubemap mip generation. */
	FSceneRenderTargetItem& GetEffectiveRenderTarget(FSceneRenderTargets& SceneContext, bool bDownsamplePass, int32 TargetMipIndex)
	{
		int32 ScratchTextureIndex = TargetMipIndex % 2;

		if (!bDownsamplePass)
		{
			ScratchTextureIndex = 1 - ScratchTextureIndex;
		}

		return SceneContext.ReflectionColorScratchCubemap[ScratchTextureIndex]->GetRenderTargetItem();
	}

	/** Encapsulates source texture picking logic for cubemap mip generation. */
	FSceneRenderTargetItem& GetEffectiveSourceTexture(FSceneRenderTargets& SceneContext, bool bDownsamplePass, int32 TargetMipIndex)
	{
		int32 ScratchTextureIndex = TargetMipIndex % 2;

		if (bDownsamplePass)
		{
			ScratchTextureIndex = 1 - ScratchTextureIndex;
		}

		return SceneContext.ReflectionColorScratchCubemap[ScratchTextureIndex]->GetRenderTargetItem();
	}

	void ComputeAverageBrightness(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, int32 CubmapSize, float& OutAverageBrightness)
	{
		SCOPED_DRAW_EVENT(RHICmdList, ComputeAverageBrightness);

		const int32 EffectiveTopMipSize = CubmapSize;
		const int32 NumMips = FMath::CeilLogTwo(EffectiveTopMipSize) + 1;

		// necessary to resolve the clears which touched all the mips.  scene rendering only resolves mip 0.
		FullyResolveReflectionScratchCubes(RHICmdList);

		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		{
			SCOPED_DRAW_EVENT(RHICmdList, DownsampleCubeMips);

			// Downsample all the mips, each one reads from the mip above it
			for (int32 MipIndex = 1; MipIndex < NumMips; MipIndex++)
			{
				const int32 SourceMipIndex = FMath::Max(MipIndex - 1, 0);
				const int32 MipSize = 1 << (NumMips - MipIndex - 1);

				FSceneRenderTargetItem& EffectiveRT = GetEffectiveRenderTarget(SceneContext, true, MipIndex);
				FSceneRenderTargetItem& EffectiveSource = GetEffectiveSourceTexture(SceneContext, true, MipIndex);
				check(EffectiveRT.TargetableTexture != EffectiveSource.ShaderResourceTexture);

				for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
				{
					SetRenderTarget(RHICmdList, EffectiveRT.TargetableTexture, MipIndex, CubeFace, NULL, true);
					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

					const FIntRect ViewRect(0, 0, MipSize, MipSize);
					RHICmdList.SetViewport(0, 0, 0.0f, MipSize, MipSize, 1.0f);

					TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
					TShaderMapRef<FMobileDownsamplePS> PixelShader(ShaderMap);

					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					PixelShader->SetParameters(RHICmdList, CubeFace, SourceMipIndex, EffectiveSource);

					DrawRectangle(
						RHICmdList,
						ViewRect.Min.X, ViewRect.Min.Y,
						ViewRect.Width(), ViewRect.Height(),
						ViewRect.Min.X, ViewRect.Min.Y,
						ViewRect.Width(), ViewRect.Height(),
						FIntPoint(ViewRect.Width(), ViewRect.Height()),
						FIntPoint(MipSize, MipSize),
						*VertexShader);

					RHICmdList.CopyToResolveTarget(EffectiveRT.TargetableTexture, EffectiveRT.ShaderResourceTexture, true, FResolveParams(FResolveRect(), (ECubeFace)CubeFace, MipIndex));
				}
			}
		}

		OutAverageBrightness = ComputeSingleAverageBrightnessFromCubemap(RHICmdList, FeatureLevel, CubmapSize, GetEffectiveRenderTarget(FSceneRenderTargets::Get(RHICmdList), true, NumMips - 1));
	}

	void CopyToSkyTexture(FRHICommandList& RHICmdList, FScene* Scene, FTexture* ProcessedTexture)
	{
		SCOPED_DRAW_EVENT(RHICmdList, CopyToSkyTexture);
		if (ProcessedTexture->TextureRHI)
		{
			const int32 EffectiveTopMipSize = ProcessedTexture->GetSizeX();
			const int32 NumMips = FMath::CeilLogTwo(EffectiveTopMipSize) + 1;
			FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

			// GPU copy back to the skylight's texture, which is not a render target
			for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
			{
				// The source for this copy is the dest from the filtering pass
				FSceneRenderTargetItem& EffectiveSource = GetEffectiveRenderTarget(SceneContext, false, MipIndex);

				for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
				{
					RHICmdList.CopyToResolveTarget(EffectiveSource.ShaderResourceTexture, ProcessedTexture->TextureRHI, true, FResolveParams(FResolveRect(), (ECubeFace)CubeFace, MipIndex, 0, 0));
				}
			}
		}
	}

	/** Generates mips for glossiness and filters the cubemap for a given reflection. */
	void FilterReflectionEnvironment(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, int32 CubmapSize, FSHVectorRGB3* OutIrradianceEnvironmentMap)
	{
		SCOPED_DRAW_EVENT(RHICmdList, FilterReflectionEnvironment);

		const int32 EffectiveTopMipSize = CubmapSize;
		const int32 NumMips = FMath::CeilLogTwo(EffectiveTopMipSize) + 1;

		FSceneRenderTargetItem& EffectiveColorRT = FSceneRenderTargets::Get(RHICmdList).ReflectionColorScratchCubemap[0]->GetRenderTargetItem();
		// Premultiply alpha in-place using alpha blending
		for (uint32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			SetRenderTarget(RHICmdList, EffectiveColorRT.TargetableTexture, 0, CubeFace, NULL, true);
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_Zero, BF_DestAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();

			const FIntPoint SourceDimensions(CubmapSize, CubmapSize);
			const FIntRect ViewRect(0, 0, EffectiveTopMipSize, EffectiveTopMipSize);
			RHICmdList.SetViewport(0, 0, 0.0f, EffectiveTopMipSize, EffectiveTopMipSize, 1.0f);

			TShaderMapRef<FScreenVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
			TShaderMapRef<FOneColorPS> PixelShader(GetGlobalShaderMap(FeatureLevel));

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			FLinearColor UnusedColors[1] = { FLinearColor::Black };
			PixelShader->SetColors(RHICmdList, UnusedColors, ARRAY_COUNT(UnusedColors));

			DrawRectangle(
				RHICmdList,
				ViewRect.Min.X, ViewRect.Min.Y,
				ViewRect.Width(), ViewRect.Height(),
				0, 0,
				SourceDimensions.X, SourceDimensions.Y,
				FIntPoint(ViewRect.Width(), ViewRect.Height()),
				SourceDimensions,
				*VertexShader);

			RHICmdList.CopyToResolveTarget(EffectiveColorRT.TargetableTexture, EffectiveColorRT.ShaderResourceTexture, true, FResolveParams(FResolveRect(), (ECubeFace)CubeFace));
		}

		int32 DiffuseConvolutionSourceMip = INDEX_NONE;
		FSceneRenderTargetItem* DiffuseConvolutionSource = NULL;

		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		{
			SCOPED_DRAW_EVENT(RHICmdList, DownsampleCubeMips);
			// Downsample all the mips, each one reads from the mip above it
			for (int32 MipIndex = 1; MipIndex < NumMips; MipIndex++)
			{
				SCOPED_DRAW_EVENT(RHICmdList, DownsampleCubeMip);
				const int32 SourceMipIndex = FMath::Max(MipIndex - 1, 0);
				const int32 MipSize = 1 << (NumMips - MipIndex - 1);

				FSceneRenderTargetItem& EffectiveRT = GetEffectiveRenderTarget(SceneContext, true, MipIndex);
				FSceneRenderTargetItem& EffectiveSource = GetEffectiveSourceTexture(SceneContext, true, MipIndex);
				check(EffectiveRT.TargetableTexture != EffectiveSource.ShaderResourceTexture);

				for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
				{
					SetRenderTarget(RHICmdList, EffectiveRT.TargetableTexture, MipIndex, CubeFace, NULL, true);

					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

					const FIntRect ViewRect(0, 0, MipSize, MipSize);
					RHICmdList.SetViewport(0, 0, 0.0f, MipSize, MipSize, 1.0f);

					TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
					TShaderMapRef<FMobileDownsamplePS> PixelShader(ShaderMap);

					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

					PixelShader->SetParameters(RHICmdList, CubeFace, SourceMipIndex, EffectiveSource);

					DrawRectangle(
						RHICmdList,
						ViewRect.Min.X, ViewRect.Min.Y,
						ViewRect.Width(), ViewRect.Height(),
						ViewRect.Min.X, ViewRect.Min.Y,
						ViewRect.Width(), ViewRect.Height(),
						FIntPoint(ViewRect.Width(), ViewRect.Height()),
						FIntPoint(MipSize, MipSize),
						*VertexShader);

					RHICmdList.CopyToResolveTarget(EffectiveRT.TargetableTexture, EffectiveRT.ShaderResourceTexture, true, FResolveParams(FResolveRect(), (ECubeFace)CubeFace, MipIndex));
				}

				if (MipSize == GDiffuseIrradianceCubemapSize)
				{
					DiffuseConvolutionSourceMip = MipIndex;
					DiffuseConvolutionSource = &EffectiveRT;
				}
			}
		}

		if (OutIrradianceEnvironmentMap)
		{
			SCOPED_DRAW_EVENT(RHICmdList, ComputeDiffuseIrradiance);
			check(DiffuseConvolutionSource != NULL);
			ComputeDiffuseIrradiance(RHICmdList, FeatureLevel, DiffuseConvolutionSource->ShaderResourceTexture, DiffuseConvolutionSourceMip, OutIrradianceEnvironmentMap);
		}

		{
			SCOPED_DRAW_EVENT(RHICmdList, FilterCubeMap);
			// Filter all the mips, each one reads from whichever scratch render target holds the downsampled contents, and writes to the destination cubemap
			for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
			{
				SCOPED_DRAW_EVENT(RHICmdList, FilterCubeMip);
				FSceneRenderTargetItem& EffectiveRT = GetEffectiveRenderTarget(SceneContext, false, MipIndex);
				FSceneRenderTargetItem& EffectiveSource = GetEffectiveSourceTexture(SceneContext, false, MipIndex);
				check(EffectiveRT.TargetableTexture != EffectiveSource.ShaderResourceTexture);
				const int32 MipSize = 1 << (NumMips - MipIndex - 1);

				for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
				{
					SetRenderTarget(RHICmdList, EffectiveRT.TargetableTexture, MipIndex, CubeFace, NULL, true);

					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

					const FIntRect ViewRect(0, 0, MipSize, MipSize);
					RHICmdList.SetViewport(0, 0, 0.0f, MipSize, MipSize, 1.0f);

					TShaderMapRef<FScreenVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
					TShaderMapRef< TCubeFilterPS<1> > CaptureCubemapArrayPixelShader(GetGlobalShaderMap(FeatureLevel));

					FCubeFilterPS* PixelShader;
					PixelShader = *TShaderMapRef< TCubeFilterPS<0> >(ShaderMap);
					check(PixelShader);

					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(PixelShader);
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
					{
						const FPixelShaderRHIParamRef ShaderRHI = PixelShader->GetPixelShader();

						SetShaderValue(RHICmdList, ShaderRHI, PixelShader->CubeFace, CubeFace);
						SetShaderValue(RHICmdList, ShaderRHI, PixelShader->MipIndex, MipIndex);

						SetShaderValue(RHICmdList, ShaderRHI, PixelShader->NumMips, NumMips);

						SetTextureParameter(
							RHICmdList,
							ShaderRHI,
							PixelShader->SourceTexture,
							PixelShader->SourceTextureSampler,
							TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(),
							EffectiveSource.ShaderResourceTexture);
					}
					//PixelShader->SetParameters(RHICmdList, NumMips, CubeFace, MipIndex, EffectiveSource);

					DrawRectangle(
						RHICmdList,
						ViewRect.Min.X, ViewRect.Min.Y,
						ViewRect.Width(), ViewRect.Height(),
						ViewRect.Min.X, ViewRect.Min.Y,
						ViewRect.Width(), ViewRect.Height(),
						FIntPoint(ViewRect.Width(), ViewRect.Height()),
						FIntPoint(MipSize, MipSize),
						*VertexShader);

					RHICmdList.CopyToResolveTarget(EffectiveRT.TargetableTexture, EffectiveRT.ShaderResourceTexture, true, FResolveParams(FResolveRect(), (ECubeFace)CubeFace, MipIndex));
				}
			}
		}
	}
}
