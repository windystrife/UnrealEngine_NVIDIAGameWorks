// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Functionality for capturing the scene into reflection capture cubemaps, and prefiltering
=============================================================================*/

#include "ReflectionEnvironmentCapture.h"
#include "Misc/FeedbackContext.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "ShowFlags.h"
#include "UnrealClient.h"
#include "ShaderParameters.h"
#include "RendererInterface.h"
#include "RHIStaticStates.h"
#include "SceneView.h"
#include "Shader.h"
#include "TextureResource.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "SceneManagement.h"
#include "Components/SkyLightComponent.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Engine/TextureCube.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "SceneRenderTargetParameters.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "ScreenRendering.h"
#include "ReflectionEnvironment.h"
#include "OneColorShader.h"
#include "PipelineStateCache.h"
#include "MobileReflectionEnvironmentCapture.h"

/** Near plane to use when capturing the scene. */
float GReflectionCaptureNearPlane = 5;

int32 GSupersampleCaptureFactor = 1;

/** 
 * Mip map used by a Roughness of 0, counting down from the lowest resolution mip (MipCount - 1).  
 * This has been tweaked along with ReflectionCaptureRoughnessMipScale to make good use of the resolution in each mip, especially the highest resolution mips.
 * This value is duplicated in ReflectionEnvironmentShared.usf!
 */
float ReflectionCaptureRoughestMip = 1;

/** 
 * Scales the log2 of Roughness when computing which mip to use for a given roughness.
 * Larger values make the higher resolution mips sharper.
 * This has been tweaked along with ReflectionCaptureRoughnessMipScale to make good use of the resolution in each mip, especially the highest resolution mips.
 * This value is duplicated in ReflectionEnvironmentShared.usf!
 */
float ReflectionCaptureRoughnessMipScale = 1.2f;

int32 GDiffuseIrradianceCubemapSize = 32;

void OnUpdateReflectionCaptures( UWorld* InWorld )
{
	InWorld->UpdateAllReflectionCaptures();
}

FAutoConsoleCommandWithWorld CaptureConsoleCommand(
	TEXT("r.ReflectionCapture"),
	TEXT("Updates all reflection captures"),
	FConsoleCommandWithWorldDelegate::CreateStatic(OnUpdateReflectionCaptures)
	);

static TAutoConsoleVariable<int32> CVarReflectionCaptureGPUArrayCopy(
	TEXT("r.ReflectionCaptureGPUArrayCopy"),
	1,
	TEXT("Do a fast copy of the reflection capture array when resizing if possible. This avoids hitches on the rendering thread when the cubemap array needs to grow.\n")
	TEXT(" 0 is off, 1 is on (default)"),
	ECVF_ReadOnly);


bool DoGPUArrayCopy()
{
	return GRHISupportsResolveCubemapFaces && CVarReflectionCaptureGPUArrayCopy.GetValueOnAnyThread();
}

void FullyResolveReflectionScratchCubes(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, FullyResolveReflectionScratchCubes);
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FTextureRHIRef& Scratch0 = SceneContext.ReflectionColorScratchCubemap[0]->GetRenderTargetItem().TargetableTexture;
	FTextureRHIRef& Scratch1 = SceneContext.ReflectionColorScratchCubemap[1]->GetRenderTargetItem().TargetableTexture;
	FResolveParams ResolveParams(FResolveRect(), CubeFace_PosX, -1, -1, -1);
	RHICmdList.CopyToResolveTarget(Scratch0, Scratch0, true, ResolveParams);
	RHICmdList.CopyToResolveTarget(Scratch1, Scratch1, true, ResolveParams);  
}

IMPLEMENT_SHADER_TYPE(,FCubeFilterPS,TEXT("/Engine/Private/ReflectionEnvironmentShaders.usf"),TEXT("DownsamplePS"),SF_Pixel);

IMPLEMENT_SHADER_TYPE(template<>,TCubeFilterPS<0>,TEXT("/Engine/Private/ReflectionEnvironmentShaders.usf"),TEXT("FilterPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TCubeFilterPS<1>,TEXT("/Engine/Private/ReflectionEnvironmentShaders.usf"),TEXT("FilterPS"),SF_Pixel);

/** Computes the average brightness of a 1x1 mip of a cubemap. */
class FComputeBrightnessPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FComputeBrightnessPS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("COMPUTEBRIGHTNESS_PIXELSHADER"), 1);
	}

	FComputeBrightnessPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ReflectionEnvironmentColorTexture.Bind(Initializer.ParameterMap,TEXT("ReflectionEnvironmentColorTexture"));
		ReflectionEnvironmentColorSampler.Bind(Initializer.ParameterMap,TEXT("ReflectionEnvironmentColorSampler"));
		NumCaptureArrayMips.Bind(Initializer.ParameterMap, TEXT("NumCaptureArrayMips"));
	}

	FComputeBrightnessPS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, int32 TargetSize, FSceneRenderTargetItem& Cubemap)
	{
		const int32 EffectiveTopMipSize = TargetSize;
		const int32 NumMips = FMath::CeilLogTwo(EffectiveTopMipSize) + 1;
		// Read from the smallest mip that was downsampled to

		if (Cubemap.IsValid())
		{
			SetTextureParameter(
				RHICmdList,
				GetPixelShader(), 
				ReflectionEnvironmentColorTexture, 
				ReflectionEnvironmentColorSampler, 
				TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
				Cubemap.ShaderResourceTexture);
		}

		SetShaderValue(RHICmdList, GetPixelShader(), NumCaptureArrayMips, FMath::CeilLogTwo(TargetSize) + 1);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ReflectionEnvironmentColorTexture;
		Ar << ReflectionEnvironmentColorSampler;
		Ar << NumCaptureArrayMips;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter ReflectionEnvironmentColorTexture;
	FShaderResourceParameter ReflectionEnvironmentColorSampler;
	FShaderParameter NumCaptureArrayMips;
};

IMPLEMENT_SHADER_TYPE(,FComputeBrightnessPS,TEXT("/Engine/Private/ReflectionEnvironmentShaders.usf"),TEXT("ComputeBrightnessMain"),SF_Pixel);

void CreateCubeMips( FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, int32 NumMips, FSceneRenderTargetItem& Cubemap )
{	
	SCOPED_DRAW_EVENT(RHICmdList, CreateCubeMips);

	FTextureRHIParamRef CubeRef = Cubemap.TargetableTexture.GetReference();

	if (GSupportsGenerateMips)
	{
		RHICmdList.GenerateMips(CubeRef/*, NumMips*/);
	}
	else
	{
		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

		//Use RWBarrier since we don't transition individual subresources.  Basically treat the whole texture as R/W as we walk down the mip chain.
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWSubResBarrier, &CubeRef, 1);

		// Downsample all the mips, each one reads from the mip above it
		for (int32 MipIndex = 1; MipIndex < NumMips; MipIndex++)
		{
			const int32 MipSize = 1 << (NumMips - MipIndex - 1);

			for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
			{
				SetRenderTarget(RHICmdList, Cubemap.TargetableTexture, MipIndex, CubeFace, NULL, false);
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

				const FIntRect ViewRect(0, 0, MipSize, MipSize);
				RHICmdList.SetViewport(0, 0, 0.0f, MipSize, MipSize, 1.0f);


				TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
				TShaderMapRef<FCubeFilterPS> PixelShader(ShaderMap);

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

				{
					const FPixelShaderRHIParamRef ShaderRHI = PixelShader->GetPixelShader();

					SetShaderValue(RHICmdList, ShaderRHI, PixelShader->CubeFace, CubeFace);
					SetShaderValue(RHICmdList, ShaderRHI, PixelShader->MipIndex, MipIndex);

					SetShaderValue(RHICmdList, ShaderRHI, PixelShader->NumMips, NumMips);

					SetSRVParameter(RHICmdList, ShaderRHI, PixelShader->SourceTexture, Cubemap.MipSRVs[MipIndex - 1]);
					SetSamplerParameter(RHICmdList, ShaderRHI, PixelShader->SourceTextureSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
				}

				DrawRectangle(
					RHICmdList,
					ViewRect.Min.X, ViewRect.Min.Y,
					ViewRect.Width(), ViewRect.Height(),
					ViewRect.Min.X, ViewRect.Min.Y,
					ViewRect.Width(), ViewRect.Height(),
					FIntPoint(ViewRect.Width(), ViewRect.Height()),
					FIntPoint(MipSize, MipSize),
					*VertexShader);

				//Use ERWSubResBarrier since we don't transition individual subresources.  Basically treat the whole texture as R/W as we walk down the mip chain.
				RHICmdList.TransitionResources(EResourceTransitionAccess::ERWSubResBarrier, &CubeRef, 1);
			}
		}

		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, &CubeRef, 1);
	}
}

/** Computes the average brightness of the given reflection capture and stores it in the scene. */
float ComputeSingleAverageBrightnessFromCubemap(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, int32 TargetSize, FSceneRenderTargetItem& Cubemap)
{
	SCOPED_DRAW_EVENT(RHICmdList, ComputeSingleAverageBrightnessFromCubemap);

	TRefCountPtr<IPooledRenderTarget> ReflectionBrightnessTarget;
	FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(FIntPoint(1, 1), PF_FloatRGBA, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ReflectionBrightnessTarget, TEXT("ReflectionBrightness"));

	FTextureRHIRef& BrightnessTarget = ReflectionBrightnessTarget->GetRenderTargetItem().TargetableTexture;
	SetRenderTarget(RHICmdList, BrightnessTarget, NULL, true);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);
	TShaderMapRef<FComputeBrightnessPS> PixelShader(ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	PixelShader->SetParameters(RHICmdList, TargetSize, Cubemap);

	DrawRectangle( 
		RHICmdList,
		0, 0, 
		1, 1,
		0, 0, 
		1, 1,
		FIntPoint(1, 1),
		FIntPoint(1, 1),
		*VertexShader);

	RHICmdList.CopyToResolveTarget(BrightnessTarget, BrightnessTarget, true, FResolveParams());

	FSceneRenderTargetItem& EffectiveRT = ReflectionBrightnessTarget->GetRenderTargetItem();
	check(EffectiveRT.ShaderResourceTexture->GetFormat() == PF_FloatRGBA);

	TArray<FFloat16Color> SurfaceData;
	RHICmdList.ReadSurfaceFloatData(EffectiveRT.ShaderResourceTexture, FIntRect(0, 0, 1, 1), SurfaceData, CubeFace_PosX, 0, 0);

	float AverageBrightness = SurfaceData[0].R.GetFloat();
	return AverageBrightness;
}

void ComputeAverageBrightness(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, int32 CubmapSize, float& OutAverageBrightness)
{
	SCOPED_DRAW_EVENT(RHICmdList, ComputeAverageBrightness);

	const int32 EffectiveTopMipSize = CubmapSize;
	const int32 NumMips = FMath::CeilLogTwo(EffectiveTopMipSize) + 1;

	// necessary to resolve the clears which touched all the mips.  scene rendering only resolves mip 0.
	FullyResolveReflectionScratchCubes(RHICmdList);	

	FSceneRenderTargetItem& DownSampledCube = FSceneRenderTargets::Get(RHICmdList).ReflectionColorScratchCubemap[0]->GetRenderTargetItem();
	CreateCubeMips( RHICmdList, FeatureLevel, NumMips, DownSampledCube );

	OutAverageBrightness = ComputeSingleAverageBrightnessFromCubemap(RHICmdList, FeatureLevel, CubmapSize, DownSampledCube);
}

/** Generates mips for glossiness and filters the cubemap for a given reflection. */
void FilterReflectionEnvironment(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, int32 CubmapSize, FSHVectorRGB3* OutIrradianceEnvironmentMap)
{
	SCOPED_DRAW_EVENT(RHICmdList, FilterReflectionEnvironment);

	const int32 EffectiveTopMipSize = CubmapSize;
	const int32 NumMips = FMath::CeilLogTwo(EffectiveTopMipSize) + 1;

	FSceneRenderTargetItem& EffectiveColorRT = FSceneRenderTargets::Get(RHICmdList).ReflectionColorScratchCubemap[0]->GetRenderTargetItem();

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_Zero, BF_DestAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();

	// Premultiply alpha in-place using alpha blending
	for (uint32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
	{
		SetRenderTarget(RHICmdList, EffectiveColorRT.TargetableTexture, 0, CubeFace, NULL, true);
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

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

	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FSceneRenderTargetItem& DownSampledCube = FSceneRenderTargets::Get(RHICmdList).ReflectionColorScratchCubemap[0]->GetRenderTargetItem();
	FSceneRenderTargetItem& FilteredCube = FSceneRenderTargets::Get(RHICmdList).ReflectionColorScratchCubemap[1]->GetRenderTargetItem();

	CreateCubeMips( RHICmdList, FeatureLevel, NumMips, DownSampledCube );

	if (OutIrradianceEnvironmentMap)
	{
		SCOPED_DRAW_EVENT(RHICmdList, ComputeDiffuseIrradiance);
		
		const int32 NumDiffuseMips = FMath::CeilLogTwo( GDiffuseIrradianceCubemapSize ) + 1;
		const int32 DiffuseConvolutionSourceMip = NumMips - NumDiffuseMips;

		ComputeDiffuseIrradiance(RHICmdList, FeatureLevel, DownSampledCube.ShaderResourceTexture, DiffuseConvolutionSourceMip, OutIrradianceEnvironmentMap);
	}

	{	
		SCOPED_DRAW_EVENT(RHICmdList, FilterCubeMap);

		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

		// Filter all the mips
		for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
		{
			const int32 MipSize = 1 << (NumMips - MipIndex - 1);

			for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
			{
				SetRenderTarget(RHICmdList, FilteredCube.TargetableTexture, MipIndex, CubeFace, NULL, true);
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

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

					SetShaderValue( RHICmdList, ShaderRHI, PixelShader->CubeFace, CubeFace );
					SetShaderValue( RHICmdList, ShaderRHI, PixelShader->MipIndex, MipIndex );

					SetShaderValue( RHICmdList, ShaderRHI, PixelShader->NumMips, NumMips );

					SetTextureParameter(
						RHICmdList, 
						ShaderRHI, 
						PixelShader->SourceTexture, 
						PixelShader->SourceTextureSampler, 
						TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
						DownSampledCube.ShaderResourceTexture);
				}

				DrawRectangle( 
					RHICmdList,
					ViewRect.Min.X, ViewRect.Min.Y, 
					ViewRect.Width(), ViewRect.Height(),
					ViewRect.Min.X, ViewRect.Min.Y, 
					ViewRect.Width(), ViewRect.Height(),
					FIntPoint(ViewRect.Width(), ViewRect.Height()),
					FIntPoint(MipSize, MipSize),
					*VertexShader);

				RHICmdList.CopyToResolveTarget(FilteredCube.TargetableTexture, FilteredCube.ShaderResourceTexture, true, FResolveParams(FResolveRect(), (ECubeFace)CubeFace, MipIndex));
			}
		}
	}
}

/** Vertex shader used when writing to a cubemap. */
class FCopyToCubeFaceVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCopyToCubeFaceVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	FCopyToCubeFaceVS()	{}
	FCopyToCubeFaceVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(),View.ViewUniformBuffer);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FCopyToCubeFaceVS,TEXT("/Engine/Private/ReflectionEnvironmentShaders.usf"),TEXT("CopyToCubeFaceVS"),SF_Vertex);

/** Pixel shader used when copying scene color from a scene render into a face of a reflection capture cubemap. */
class FCopySceneColorToCubeFacePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCopySceneColorToCubeFacePS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	FCopySceneColorToCubeFacePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		InTexture.Bind(Initializer.ParameterMap,TEXT("InTexture"));
		InTextureSampler.Bind(Initializer.ParameterMap,TEXT("InTextureSampler"));
		SkyLightCaptureParameters.Bind(Initializer.ParameterMap,TEXT("SkyLightCaptureParameters"));
		LowerHemisphereColor.Bind(Initializer.ParameterMap,TEXT("LowerHemisphereColor"));
	}
	FCopySceneColorToCubeFacePS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, bool bCapturingForSkyLight, bool bLowerHemisphereIsBlack, const FLinearColor& LowerHemisphereColorValue)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View, MD_PostProcess);

		SetTextureParameter(
			RHICmdList, 
			ShaderRHI, 
			InTexture, 
			InTextureSampler, 
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
			FSceneRenderTargets::Get(RHICmdList).GetSceneColor()->GetRenderTargetItem().ShaderResourceTexture);		

		FVector SkyLightParametersValue = FVector::ZeroVector;
		FScene* Scene = (FScene*)View.Family->Scene;

		if (bCapturingForSkyLight)
		{
			// When capturing reflection captures, support forcing all low hemisphere lighting to be black
			SkyLightParametersValue = FVector(0, 0, bLowerHemisphereIsBlack ? 1.0f : 0.0f);
		}
		else if (Scene->SkyLight && !Scene->SkyLight->bHasStaticLighting)
		{
			// When capturing reflection captures and there's a stationary sky light, mask out any pixels whose depth classify it as part of the sky
			// This will allow changing the stationary sky light at runtime
			SkyLightParametersValue = FVector(1, Scene->SkyLight->SkyDistanceThreshold, 0);
		}
		else
		{
			// When capturing reflection captures and there's no sky light, or only a static sky light, capture all depth ranges
			SkyLightParametersValue = FVector(2, 0, 0);
		}

		SetShaderValue(RHICmdList, ShaderRHI, SkyLightCaptureParameters, SkyLightParametersValue);
		SetShaderValue(RHICmdList, ShaderRHI, LowerHemisphereColor, LowerHemisphereColorValue);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << InTexture;
		Ar << InTextureSampler;
		Ar << SkyLightCaptureParameters;
		Ar << LowerHemisphereColor;
		return bShaderHasOutdatedParameters;
	}

private:
	FDeferredPixelShaderParameters DeferredParameters;	
	FShaderResourceParameter InTexture;
	FShaderResourceParameter InTextureSampler;
	FShaderParameter SkyLightCaptureParameters;
	FShaderParameter LowerHemisphereColor;
};

IMPLEMENT_SHADER_TYPE(,FCopySceneColorToCubeFacePS,TEXT("/Engine/Private/ReflectionEnvironmentShaders.usf"),TEXT("CopySceneColorToCubeFaceColorPS"),SF_Pixel);

/** Pixel shader used when copying a cubemap into a face of a reflection capture cubemap. */
class FCopyCubemapToCubeFacePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCopyCubemapToCubeFacePS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	FCopyCubemapToCubeFacePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		CubeFace.Bind(Initializer.ParameterMap,TEXT("CubeFace"));
		SourceTexture.Bind(Initializer.ParameterMap,TEXT("SourceTexture"));
		SourceTextureSampler.Bind(Initializer.ParameterMap,TEXT("SourceTextureSampler"));
		SkyLightCaptureParameters.Bind(Initializer.ParameterMap,TEXT("SkyLightCaptureParameters"));
		LowerHemisphereColor.Bind(Initializer.ParameterMap,TEXT("LowerHemisphereColor"));
		SinCosSourceCubemapRotation.Bind(Initializer.ParameterMap,TEXT("SinCosSourceCubemapRotation"));
	}
	FCopyCubemapToCubeFacePS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FTexture* SourceCubemap, uint32 CubeFaceValue, bool bIsSkyLight, bool bLowerHemisphereIsBlack, float SourceCubemapRotation, const FLinearColor& LowerHemisphereColorValue)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		SetShaderValue(RHICmdList, ShaderRHI, CubeFace, CubeFaceValue);

		SetTextureParameter(
			RHICmdList, 
			ShaderRHI, 
			SourceTexture, 
			SourceTextureSampler, 
			SourceCubemap);

		SetShaderValue(RHICmdList, ShaderRHI, SkyLightCaptureParameters, FVector(bIsSkyLight ? 1.0f : 0.0f, 0.0f, bLowerHemisphereIsBlack ? 1.0f : 0.0f));
		SetShaderValue(RHICmdList, ShaderRHI, LowerHemisphereColor, LowerHemisphereColorValue);

		SetShaderValue(RHICmdList, ShaderRHI, SinCosSourceCubemapRotation, FVector2D(FMath::Sin(SourceCubemapRotation), FMath::Cos(SourceCubemapRotation)));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << CubeFace;
		Ar << SourceTexture;
		Ar << SourceTextureSampler;
		Ar << SkyLightCaptureParameters;
		Ar << LowerHemisphereColor;
		Ar << SinCosSourceCubemapRotation;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter CubeFace;
	FShaderResourceParameter SourceTexture;
	FShaderResourceParameter SourceTextureSampler;
	FShaderParameter SkyLightCaptureParameters;
	FShaderParameter LowerHemisphereColor;
	FShaderParameter SinCosSourceCubemapRotation;
};

IMPLEMENT_SHADER_TYPE(,FCopyCubemapToCubeFacePS,TEXT("/Engine/Private/ReflectionEnvironmentShaders.usf"),TEXT("CopyCubemapToCubeFaceColorPS"),SF_Pixel);

int32 FindOrAllocateCubemapIndex(FScene* Scene, const UReflectionCaptureComponent* Component)
{
	int32 CaptureIndex = -1;

	// Try to find an existing capture index for this component
	const FCaptureComponentSceneState* CaptureSceneStatePtr = Scene->ReflectionSceneData.AllocatedReflectionCaptureState.Find(Component);

	if (CaptureSceneStatePtr)
	{
		CaptureIndex = CaptureSceneStatePtr->CaptureIndex;
	}
	else
	{
		// Reuse a freed index if possible
		CaptureIndex = Scene->ReflectionSceneData.CubemapArraySlotsUsed.FindAndSetFirstZeroBit();
		if (CaptureIndex == INDEX_NONE)
		{
			// If we didn't find a free index, allocate a new one from the CubemapArraySlotsUsed bitfield
			CaptureIndex = Scene->ReflectionSceneData.CubemapArraySlotsUsed.Num();
			Scene->ReflectionSceneData.CubemapArraySlotsUsed.Add(true);
		}

		Scene->ReflectionSceneData.AllocatedReflectionCaptureState.Add(Component, FCaptureComponentSceneState(CaptureIndex));

		check(CaptureIndex < GMaxNumReflectionCaptures);
	}

	check(CaptureIndex >= 0);
	return CaptureIndex;
}

void ClearScratchCubemaps(FRHICommandList& RHICmdList, int32 TargetSize)
{
	SCOPED_DRAW_EVENT(RHICmdList, ClearScratchCubemaps);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SceneContext.AllocateReflectionTargets(RHICmdList, TargetSize);
	// Clear scratch render targets to a consistent but noticeable value
	// This makes debugging capture issues much easier, otherwise the random contents from previous captures is shown

	FSceneRenderTargetItem& RT0 = SceneContext.ReflectionColorScratchCubemap[0]->GetRenderTargetItem();
	int32 NumMips = (int32)RT0.TargetableTexture->GetNumMips();

	{
		SCOPED_DRAW_EVENT(RHICmdList, ClearScratchCubemapsRT0);

		for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
		{
			for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
			{
				TransitionSetRenderTargetsHelper(RHICmdList, RT0.TargetableTexture, FTextureRHIParamRef(), FExclusiveDepthStencil::DepthWrite_StencilWrite);

				FRHIRenderTargetView RtView = FRHIRenderTargetView(RT0.TargetableTexture, ERenderTargetLoadAction::EClear, MipIndex, CubeFace);
				FRHISetRenderTargetsInfo Info(1, &RtView, FRHIDepthRenderTargetView());
				RHICmdList.SetRenderTargetsAndClear(Info);
			}
		}
	}

	{
		SCOPED_DRAW_EVENT(RHICmdList, ClearScratchCubemapsRT1);

		FSceneRenderTargetItem& RT1 = SceneContext.ReflectionColorScratchCubemap[1]->GetRenderTargetItem();
		NumMips = (int32)RT1.TargetableTexture->GetNumMips();

		for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
		{
			for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
			{
				TransitionSetRenderTargetsHelper(RHICmdList, RT1.TargetableTexture, FTextureRHIParamRef(), FExclusiveDepthStencil::DepthWrite_StencilWrite);

				FRHIRenderTargetView RtView = FRHIRenderTargetView(RT1.TargetableTexture, ERenderTargetLoadAction::EClear, MipIndex, CubeFace);
				FRHISetRenderTargetsInfo Info(1, &RtView, FRHIDepthRenderTargetView());
				RHICmdList.SetRenderTargetsAndClear(Info);
			}
		}
	}
}

/** Captures the scene for a reflection capture by rendering the scene multiple times and copying into a cubemap texture. */
void CaptureSceneToScratchCubemap(FRHICommandListImmediate& RHICmdList, FSceneRenderer* SceneRenderer, ECubeFace CubeFace, int32 CubemapSize, bool bCapturingForSkyLight, bool bLowerHemisphereIsBlack, const FLinearColor& LowerHemisphereColor)
{
	FMemMark MemStackMark(FMemStack::Get());

	// update any resources that needed a deferred update
	FDeferredUpdateResource::UpdateResources(RHICmdList);

	const auto FeatureLevel = SceneRenderer->FeatureLevel;
	
	{
		SCOPED_DRAW_EVENT(RHICmdList, CubeMapCapture);

		// Render the scene normally for one face of the cubemap
		SceneRenderer->Render(RHICmdList);
		check(&RHICmdList == &FRHICommandListExecutor::GetImmediateCommandList());
		check(IsInRenderingThread());
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_CaptureSceneToScratchCubemap_Flush);
			FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		}

		// some platforms may not be able to keep enqueueing commands like crazy, this will
		// allow them to restart their command buffers
		RHICmdList.SubmitCommandsAndFlushGPU();

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		SceneContext.AllocateReflectionTargets(RHICmdList, CubemapSize);

		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

		const int32 EffectiveSize = CubemapSize;
		FSceneRenderTargetItem& EffectiveColorRT =  SceneContext.ReflectionColorScratchCubemap[0]->GetRenderTargetItem();

		{
			SCOPED_DRAW_EVENT(RHICmdList, CubeMapCopyScene);
			
			// Copy the captured scene into the cubemap face
			SetRenderTarget(RHICmdList, EffectiveColorRT.TargetableTexture, 0, CubeFace, NULL);

			const FIntRect ViewRect(0, 0, EffectiveSize, EffectiveSize);
			RHICmdList.SetViewport(0, 0, 0.0f, EffectiveSize, EffectiveSize, 1.0f);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

			TShaderMapRef<FCopyToCubeFaceVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
			TShaderMapRef<FCopySceneColorToCubeFacePS> PixelShader(GetGlobalShaderMap(FeatureLevel));

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			PixelShader->SetParameters(RHICmdList, SceneRenderer->Views[0], bCapturingForSkyLight, bLowerHemisphereIsBlack, LowerHemisphereColor);
			VertexShader->SetParameters(RHICmdList, SceneRenderer->Views[0]);

			DrawRectangle( 
				RHICmdList,
				ViewRect.Min.X, ViewRect.Min.Y, 
				ViewRect.Width(), ViewRect.Height(),
				ViewRect.Min.X, ViewRect.Min.Y, 
				ViewRect.Width() * GSupersampleCaptureFactor, ViewRect.Height() * GSupersampleCaptureFactor,
				FIntPoint(ViewRect.Width(), ViewRect.Height()),
				SceneContext.GetBufferSizeXY(),
				*VertexShader);

			RHICmdList.CopyToResolveTarget(EffectiveColorRT.TargetableTexture, EffectiveColorRT.ShaderResourceTexture, true, FResolveParams(FResolveRect(), CubeFace));			
		}
	}

	FSceneRenderer::WaitForTasksClearSnapshotsAndDeleteSceneRenderer(RHICmdList, SceneRenderer);
}

void CopyCubemapToScratchCubemap(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, UTextureCube* SourceCubemap, int32 CubemapSize, bool bIsSkyLight, bool bLowerHemisphereIsBlack, float SourceCubemapRotation, const FLinearColor& LowerHemisphereColorValue)
{
	SCOPED_DRAW_EVENT(RHICmdList, CopyCubemapToScratchCubemap);
	check(SourceCubemap);
	
	const int32 EffectiveSize = CubemapSize;
	FSceneRenderTargetItem& EffectiveColorRT =  FSceneRenderTargets::Get(RHICmdList).ReflectionColorScratchCubemap[0]->GetRenderTargetItem();

	for (uint32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
	{
		// Copy the captured scene into the cubemap face
		SetRenderTarget(RHICmdList, EffectiveColorRT.TargetableTexture, 0, CubeFace, NULL, true);

		const FTexture* SourceCubemapResource = SourceCubemap->Resource;
		const FIntPoint SourceDimensions(SourceCubemapResource->GetSizeX(), SourceCubemapResource->GetSizeY());
		const FIntRect ViewRect(0, 0, EffectiveSize, EffectiveSize);
		RHICmdList.SetViewport(0, 0, 0.0f, EffectiveSize, EffectiveSize, 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

		TShaderMapRef<FScreenVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
		TShaderMapRef<FCopyCubemapToCubeFacePS> PixelShader(GetGlobalShaderMap(FeatureLevel));

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
		PixelShader->SetParameters(RHICmdList, SourceCubemapResource, CubeFace, bIsSkyLight, bLowerHemisphereIsBlack, SourceCubemapRotation, LowerHemisphereColorValue);

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
}

/** 
 * Allocates reflection captures in the scene's reflection cubemap array and updates them by recapturing the scene.
 * Existing captures will only be updated.  Must be called from the game thread.
 */
void FScene::AllocateReflectionCaptures(const TArray<UReflectionCaptureComponent*>& NewCaptures)
{
	if (NewCaptures.Num() > 0)
	{
		if (GetFeatureLevel() >= ERHIFeatureLevel::SM5)
		{
			for (int32 CaptureIndex = 0; CaptureIndex < NewCaptures.Num(); CaptureIndex++)
			{
				bool bAlreadyExists = false;

				// Try to find an existing allocation
				for (TSparseArray<UReflectionCaptureComponent*>::TIterator It(ReflectionSceneData.AllocatedReflectionCapturesGameThread); It; ++It)
				{
					UReflectionCaptureComponent* OtherComponent = *It;

					if (OtherComponent == NewCaptures[CaptureIndex])
					{
						bAlreadyExists = true;
					}
				}

				// Add the capture to the allocated list
				if (!bAlreadyExists && ReflectionSceneData.AllocatedReflectionCapturesGameThread.Num() < GMaxNumReflectionCaptures)
				{
					ReflectionSceneData.AllocatedReflectionCapturesGameThread.Add(NewCaptures[CaptureIndex]);
				}
			}

			// Request the exact amount needed by default
			int32 DesiredMaxCubemaps = ReflectionSceneData.AllocatedReflectionCapturesGameThread.Num();
			const float MaxCubemapsRoundUpBase = 1.5f;

			// If this is not the first time the scene has allocated the cubemap array, include slack to reduce reallocations
			if (ReflectionSceneData.MaxAllocatedReflectionCubemapsGameThread > 0)
			{
				float Exponent = FMath::LogX(MaxCubemapsRoundUpBase, ReflectionSceneData.AllocatedReflectionCapturesGameThread.Num());

				// Round up to the next integer exponent to provide stability and reduce reallocations
				DesiredMaxCubemaps = FMath::Pow(MaxCubemapsRoundUpBase, FMath::TruncToInt(Exponent) + 1);
			}

			DesiredMaxCubemaps = FMath::Min(DesiredMaxCubemaps, GMaxNumReflectionCaptures);

			const int32 ReflectionCaptureSize = UReflectionCaptureComponent::GetReflectionCaptureSize_GameThread();
			bool bNeedsUpdateAllCaptures = DesiredMaxCubemaps != ReflectionSceneData.MaxAllocatedReflectionCubemapsGameThread || ReflectionCaptureSize != ReflectionSceneData.CubemapArray.GetCubemapSize();

			if (DoGPUArrayCopy() && bNeedsUpdateAllCaptures)
			{
				// If we're not in the editor, we discard the CPU-side reflection capture data after loading to save memory, so we can't resize if the resolution changes. If this happens, we assert
				check(GIsEditor || ReflectionCaptureSize == ReflectionSceneData.CubemapArray.GetCubemapSize() || ReflectionSceneData.CubemapArray.GetCubemapSize() == 0);

				if (ReflectionCaptureSize == ReflectionSceneData.CubemapArray.GetCubemapSize())
				{
					// We can do a fast GPU copy to realloc the array, so we don't need to update all captures
					ReflectionSceneData.MaxAllocatedReflectionCubemapsGameThread = DesiredMaxCubemaps;
					ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
						GPUResizeArrayCommand,
						FScene*, Scene, this,
						uint32, MaxSize, ReflectionSceneData.MaxAllocatedReflectionCubemapsGameThread,
						int32, ReflectionCaptureSize, ReflectionCaptureSize,
						{
							// Update the scene's cubemap array, preserving the original contents with a GPU-GPU copy
							Scene->ReflectionSceneData.ResizeCubemapArrayGPU(MaxSize, ReflectionCaptureSize);
						});

					bNeedsUpdateAllCaptures = false;
				}
			}

			if (bNeedsUpdateAllCaptures)
			{
				ReflectionSceneData.MaxAllocatedReflectionCubemapsGameThread = DesiredMaxCubemaps;

				ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER( 
					ResizeArrayCommand,
					FScene*, Scene, this,
					uint32, MaxSize, ReflectionSceneData.MaxAllocatedReflectionCubemapsGameThread,
					int32, ReflectionCaptureSize, ReflectionCaptureSize,
				{
					// Update the scene's cubemap array, which will reallocate it, so we no longer have the contents of existing entries
					Scene->ReflectionSceneData.CubemapArray.UpdateMaxCubemaps(MaxSize, ReflectionCaptureSize);
				});

				// Recapture all reflection captures now that we have reallocated the cubemap array
				UpdateAllReflectionCaptures();
			}
			else
			{
				// No teardown of the cubemap array was needed, just update the captures that were requested
				for (TSparseArray<UReflectionCaptureComponent*>::TIterator It(ReflectionSceneData.AllocatedReflectionCapturesGameThread); It; ++It)
				{
					UReflectionCaptureComponent* CurrentComponent = *It;

					if (NewCaptures.Contains(CurrentComponent))
					{
						UpdateReflectionCaptureContents(CurrentComponent);
					}
				}
			}
		}
		else if (GetFeatureLevel() == ERHIFeatureLevel::SM4)
		{
			for (int32 ComponentIndex = 0; ComponentIndex < NewCaptures.Num(); ComponentIndex++)
			{
				UReflectionCaptureComponent* CurrentComponent = NewCaptures[ComponentIndex];
				UpdateReflectionCaptureContents(CurrentComponent);
			}
		}

		for (int32 CaptureIndex = 0; CaptureIndex < NewCaptures.Num(); CaptureIndex++)
		{
			UReflectionCaptureComponent* Component = NewCaptures[CaptureIndex];

			Component->SetCaptureCompleted();

			if (Component->SceneProxy)
			{
				// Update the transform of the reflection capture
				// This is not done earlier by the reflection capture when it detects that it is dirty,
				// To ensure that the RT sees both the new transform and the new contents on the same frame.
				Component->SendRenderTransform_Concurrent();
			}
		}
	}
}

/** Updates the contents of all reflection captures in the scene.  Must be called from the game thread. */
void FScene::UpdateAllReflectionCaptures()
{
	if (IsReflectionEnvironmentAvailable(GetFeatureLevel()))
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( 
			CaptureCommand,
			FScene*, Scene, this,
		{
			Scene->ReflectionSceneData.AllocatedReflectionCaptureState.Empty();
			Scene->ReflectionSceneData.CubemapArraySlotsUsed.Reset();
		});

		const int32 UpdateDivisor = FMath::Max(ReflectionSceneData.AllocatedReflectionCapturesGameThread.Num() / 20, 1);
		const bool bDisplayStatus = ReflectionSceneData.AllocatedReflectionCapturesGameThread.Num() > 50;

		if (bDisplayStatus)
		{
			const FText Status = NSLOCTEXT("Engine", "BeginReflectionCapturesTask", "Updating Reflection Captures...");
			GWarn->BeginSlowTask( Status, true );
			GWarn->StatusUpdate(0, ReflectionSceneData.AllocatedReflectionCapturesGameThread.Num(), Status);
		}

		int32 CaptureIndex = 0;

		for (TSparseArray<UReflectionCaptureComponent*>::TIterator It(ReflectionSceneData.AllocatedReflectionCapturesGameThread); It; ++It)
		{
			// Update progress occasionally
			if (bDisplayStatus && CaptureIndex % UpdateDivisor == 0)
			{
				GWarn->UpdateProgress(CaptureIndex, ReflectionSceneData.AllocatedReflectionCapturesGameThread.Num());
			}

			CaptureIndex++;
			UReflectionCaptureComponent* CurrentComponent = *It;
			UpdateReflectionCaptureContents(CurrentComponent);
		}

		if (bDisplayStatus)
		{
			GWarn->EndSlowTask();
		}
	}
}

void GetReflectionCaptureData_RenderingThread(FRHICommandListImmediate& RHICmdList, FScene* Scene, const UReflectionCaptureComponent* Component, FReflectionCaptureFullHDR* OutDerivedData)
{
	const FCaptureComponentSceneState* ComponentStatePtr = Scene->ReflectionSceneData.AllocatedReflectionCaptureState.Find(Component);

	if (ComponentStatePtr)
	{
		FSceneRenderTargetItem& EffectiveDest = Scene->ReflectionSceneData.CubemapArray.GetRenderTarget();

		const int32 CaptureIndex = ComponentStatePtr->CaptureIndex;
		const int32 NumMips = EffectiveDest.ShaderResourceTexture->GetNumMips();
		const int32 EffectiveTopMipSize = FMath::Pow(2, NumMips - 1);

		TArray<uint8> CaptureData;
		int32 CaptureDataSize = 0;

		for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
		{
			const int32 MipSize = 1 << (NumMips - MipIndex - 1);

			for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
			{
				CaptureDataSize += MipSize * MipSize * sizeof(FFloat16Color);
			}
		}

		CaptureData.Empty(CaptureDataSize);
		CaptureData.AddZeroed(CaptureDataSize);
		int32 MipBaseIndex = 0;

		for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
		{
			check(EffectiveDest.ShaderResourceTexture->GetFormat() == PF_FloatRGBA);
			const int32 MipSize = 1 << (NumMips - MipIndex - 1);
			const int32 CubeFaceBytes = MipSize * MipSize * sizeof(FFloat16Color);

			for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
			{
				TArray<FFloat16Color> SurfaceData;
				// Read each mip face
				//@todo - do this without blocking the GPU so many times
				//@todo - pool the temporary textures in RHIReadSurfaceFloatData instead of always creating new ones
				RHICmdList.ReadSurfaceFloatData(EffectiveDest.ShaderResourceTexture, FIntRect(0, 0, MipSize, MipSize), SurfaceData, (ECubeFace)CubeFace, CaptureIndex, MipIndex);
				const int32 DestIndex = MipBaseIndex + CubeFace * CubeFaceBytes;
				uint8* FaceData = &CaptureData[DestIndex];
				check(SurfaceData.Num() * SurfaceData.GetTypeSize() == CubeFaceBytes);
				FMemory::Memcpy(FaceData, SurfaceData.GetData(), CubeFaceBytes);
			}

			MipBaseIndex += CubeFaceBytes * CubeFace_MAX;
		}

		OutDerivedData->InitializeFromUncompressedData(CaptureData, EffectiveTopMipSize);
	}
}

void FScene::GetReflectionCaptureData(UReflectionCaptureComponent* Component, FReflectionCaptureFullHDR& OutDerivedData) 
{
	check(GetFeatureLevel() >= ERHIFeatureLevel::SM5);

	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		GetReflectionDataCommand,
		FScene*,Scene,this,
		const UReflectionCaptureComponent*,Component,Component,
		FReflectionCaptureFullHDR*,OutDerivedData,&OutDerivedData,
	{
		GetReflectionCaptureData_RenderingThread(RHICmdList, Scene, Component, OutDerivedData);
	});

	// Necessary since the RT is writing to OutDerivedData directly
	FlushRenderingCommands();
}

void UploadReflectionCapture_RenderingThread(FScene* Scene, const FReflectionCaptureFullHDR* FullHDRData, const UReflectionCaptureComponent* CaptureComponent)
{
	const int32 EffectiveTopMipSize = FullHDRData->CubemapSize;
	const int32 NumMips = FMath::CeilLogTwo(EffectiveTopMipSize) + 1;

	const int32 CaptureIndex = FindOrAllocateCubemapIndex(Scene, CaptureComponent);
	FTextureCubeRHIRef& CubeMapArray = (FTextureCubeRHIRef&)Scene->ReflectionSceneData.CubemapArray.GetRenderTarget().ShaderResourceTexture;
	check(CubeMapArray->GetFormat() == PF_FloatRGBA);

	TRefCountPtr<FReflectionCaptureUncompressedData> SourceCubemapData = FullHDRData->GetUncompressedData();
	int32 MipBaseIndex = 0;

	for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		const int32 MipSize = 1 << (NumMips - MipIndex - 1);
		const int32 CubeFaceBytes = MipSize * MipSize * sizeof(FFloat16Color);

		for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			uint32 DestStride = 0;
			uint8* DestBuffer = (uint8*)RHILockTextureCubeFace(CubeMapArray, CubeFace, CaptureIndex, MipIndex, RLM_WriteOnly, DestStride, false);

			// Handle DestStride by copying each row
			for (int32 Y = 0; Y < MipSize; Y++)
			{
				FFloat16Color* DestPtr = (FFloat16Color*)((uint8*)DestBuffer + Y * DestStride);
				const int32 SourceIndex = MipBaseIndex + CubeFace * CubeFaceBytes + Y * MipSize * sizeof(FFloat16Color);
				const uint8* SourcePtr = SourceCubemapData->GetData(SourceIndex);
				FMemory::Memcpy(DestPtr, SourcePtr, MipSize * sizeof(FFloat16Color));
			}

			RHIUnlockTextureCubeFace(CubeMapArray, CubeFace, CaptureIndex, MipIndex, false);
		}

		MipBaseIndex += CubeFaceBytes * CubeFace_MAX;
	}
}

/** Creates a transformation for a cubemap face, following the D3D cubemap layout. */
FMatrix CalcCubeFaceViewRotationMatrix(ECubeFace Face)
{
	FMatrix Result(FMatrix::Identity);

	static const FVector XAxis(1.f,0.f,0.f);
	static const FVector YAxis(0.f,1.f,0.f);
	static const FVector ZAxis(0.f,0.f,1.f);

	// vectors we will need for our basis
	FVector vUp(YAxis);
	FVector vDir;

	switch( Face )
	{
	case CubeFace_PosX:
		vDir = XAxis;
		break;
	case CubeFace_NegX:
		vDir = -XAxis;
		break;
	case CubeFace_PosY:
		vUp = -ZAxis;
		vDir = YAxis;
		break;
	case CubeFace_NegY:
		vUp = ZAxis;
		vDir = -YAxis;
		break;
	case CubeFace_PosZ:
		vDir = ZAxis;
		break;
	case CubeFace_NegZ:
		vDir = -ZAxis;
		break;
	}

	// derive right vector
	FVector vRight( vUp ^ vDir );
	// create matrix from the 3 axes
	Result = FBasisVectorMatrix( vRight, vUp, vDir, FVector::ZeroVector );	

	return Result;
}

/** 
 * Render target class required for rendering the scene.
 * This doesn't actually allocate a render target as we read from scene color to get HDR results directly.
 */
class FCaptureRenderTarget : public FRenderResource, public FRenderTarget
{
public:

	FCaptureRenderTarget() :
		Size(0)
	{}

	virtual const FTexture2DRHIRef& GetRenderTargetTexture() const 
	{
		static FTexture2DRHIRef DummyTexture;
		return DummyTexture;
	}

	void SetSize(int32 TargetSize) { Size = TargetSize; }
	virtual FIntPoint GetSizeXY() const { return FIntPoint(Size, Size); }
	virtual float GetDisplayGamma() const { return 1.0f; }

private:

	int32 Size;
};

TGlobalResource<FCaptureRenderTarget> GReflectionCaptureRenderTarget;

void CaptureSceneIntoScratchCubemap(
	FScene* Scene, 
	FVector CapturePosition, 
	int32 CubemapSize,
	bool bCapturingForSkyLight,
	bool bStaticSceneOnly, 
	float SkyLightNearPlane,
	bool bLowerHemisphereIsBlack, 
	bool bCaptureEmissiveOnly,
	const FLinearColor& LowerHemisphereColor
	)
{
	for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
	{
		if( !bCapturingForSkyLight )
		{
			// Alert the RHI that we're rendering a new frame
			// Not really a new frame, but it will allow pooling mechanisms to update, like the uniform buffer pool
			ENQUEUE_UNIQUE_RENDER_COMMAND(
				BeginFrame,
			{
				GFrameNumberRenderThread++;
				RHICmdList.BeginFrame();
			})
		}

		GReflectionCaptureRenderTarget.SetSize(CubemapSize);

		auto ViewFamilyInit = FSceneViewFamily::ConstructionValues(
			&GReflectionCaptureRenderTarget,
			Scene,
			FEngineShowFlags(ESFIM_Game)
			)
			.SetResolveScene(false);

		if( bStaticSceneOnly )
		{
			ViewFamilyInit.SetWorldTimes( 0.0f, 0.0f, 0.0f );
		}

		FSceneViewFamilyContext ViewFamily( ViewFamilyInit );

		// Disable features that are not desired when capturing the scene
		ViewFamily.EngineShowFlags.PostProcessing = 0;
		ViewFamily.EngineShowFlags.MotionBlur = 0;
		ViewFamily.EngineShowFlags.SetOnScreenDebug(false);
		ViewFamily.EngineShowFlags.HMDDistortion = 0;
		// Exclude particles and light functions as they are usually dynamic, and can't be captured well
		ViewFamily.EngineShowFlags.Particles = 0;
		ViewFamily.EngineShowFlags.LightFunctions = 0;
		ViewFamily.EngineShowFlags.SetCompositeEditorPrimitives(false);
		// These are highly dynamic and can't be captured effectively
		ViewFamily.EngineShowFlags.LightShafts = 0;
		// Don't apply sky lighting diffuse when capturing the sky light source, or we would have feedback
		ViewFamily.EngineShowFlags.SkyLighting = !bCapturingForSkyLight;
		// Skip lighting for emissive only
		ViewFamily.EngineShowFlags.Lighting = !bCaptureEmissiveOnly;

		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.ViewFamily = &ViewFamily;
		ViewInitOptions.BackgroundColor = FLinearColor::Black;
		ViewInitOptions.OverlayColor = FLinearColor::Black;
		ViewInitOptions.SetViewRectangle(FIntRect(0, 0, CubemapSize * GSupersampleCaptureFactor, CubemapSize * GSupersampleCaptureFactor));

		const float NearPlane = bCapturingForSkyLight ? SkyLightNearPlane : GReflectionCaptureNearPlane;

		// Projection matrix based on the fov, near / far clip settings
		// Each face always uses a 90 degree field of view
		if ((bool)ERHIZBuffer::IsInverted)
		{
			ViewInitOptions.ProjectionMatrix = FReversedZPerspectiveMatrix(
				90.0f * (float)PI / 360.0f,
				(float)CubemapSize * GSupersampleCaptureFactor,
				(float)CubemapSize * GSupersampleCaptureFactor,
				NearPlane
				);
		}
		else
		{
			ViewInitOptions.ProjectionMatrix = FPerspectiveMatrix(
				90.0f * (float)PI / 360.0f,
				(float)CubemapSize * GSupersampleCaptureFactor,
				(float)CubemapSize * GSupersampleCaptureFactor,
				NearPlane
				);
		}

		ViewInitOptions.ViewOrigin = CapturePosition;
		ViewInitOptions.ViewRotationMatrix = CalcCubeFaceViewRotationMatrix((ECubeFace)CubeFace);

		FSceneView* View = new FSceneView(ViewInitOptions);

		// Force all surfaces diffuse
		View->RoughnessOverrideParameter = FVector2D( 1.0f, 0.0f );

		if (bCaptureEmissiveOnly)
		{
			View->DiffuseOverrideParameter = FVector4(0, 0, 0, 0);
			View->SpecularOverrideParameter = FVector4(0, 0, 0, 0);
		}

		View->bIsReflectionCapture = true;
		View->bStaticSceneOnly = bStaticSceneOnly;
		View->StartFinalPostprocessSettings(CapturePosition);
		View->EndFinalPostprocessSettings(ViewInitOptions);

		ViewFamily.Views.Add(View);

		FSceneRenderer* SceneRenderer = FSceneRenderer::CreateSceneRenderer(&ViewFamily, NULL);

		ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER( 
			CaptureCommand,
			FSceneRenderer*, SceneRenderer, SceneRenderer,
			ECubeFace, CubeFace, (ECubeFace)CubeFace,
			int32, CubemapSize, CubemapSize,
			bool, bCapturingForSkyLight, bCapturingForSkyLight,
			bool, bLowerHemisphereIsBlack, bLowerHemisphereIsBlack,
			FLinearColor, LowerHemisphereColor, LowerHemisphereColor,
		{
			CaptureSceneToScratchCubemap(RHICmdList, SceneRenderer, CubeFace, CubemapSize, bCapturingForSkyLight, bLowerHemisphereIsBlack, LowerHemisphereColor);

			if( !bCapturingForSkyLight )
			{
				RHICmdList.EndFrame();
			}
		});
	}
}

void CopyToSceneArray(FRHICommandListImmediate& RHICmdList, FScene* Scene, FReflectionCaptureProxy* ReflectionProxy)
{
	SCOPED_DRAW_EVENT(RHICmdList, CopyToSceneArray);
	const int32 EffectiveTopMipSize = UReflectionCaptureComponent::GetReflectionCaptureSize_RenderThread();
	const int32 NumMips = FMath::CeilLogTwo(EffectiveTopMipSize) + 1;

	const int32 CaptureIndex = FindOrAllocateCubemapIndex(Scene, ReflectionProxy->Component);
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	
	FSceneRenderTargetItem& FilteredCube = FSceneRenderTargets::Get(RHICmdList).ReflectionColorScratchCubemap[1]->GetRenderTargetItem();
	FSceneRenderTargetItem& DestCube = Scene->ReflectionSceneData.CubemapArray.GetRenderTarget();

	// GPU copy back to the scene's texture array, which is not a render target
	for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			RHICmdList.CopyToResolveTarget(FilteredCube.ShaderResourceTexture, DestCube.ShaderResourceTexture, true, FResolveParams(FResolveRect(), (ECubeFace)CubeFace, MipIndex, 0, CaptureIndex));
		}
	}
}

void CopyToComponentTexture(FRHICommandList& RHICmdList, FScene* Scene, FReflectionCaptureProxy* ReflectionProxy)
{
	SCOPED_DRAW_EVENT(RHICmdList, CopyToComponentTexture);
	check(ReflectionProxy->SM4FullHDRCubemap);

	const int32 EffectiveTopMipSize = UReflectionCaptureComponent::GetReflectionCaptureSize_RenderThread();
	const int32 NumMips = FMath::CeilLogTwo(EffectiveTopMipSize) + 1;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FSceneRenderTargetItem& FilteredCube = FSceneRenderTargets::Get(RHICmdList).ReflectionColorScratchCubemap[1]->GetRenderTargetItem();

	// GPU copy back to the component's cubemap texture, which is not a render target
	for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			RHICmdList.CopyToResolveTarget(FilteredCube.ShaderResourceTexture, ReflectionProxy->SM4FullHDRCubemap->TextureRHI, true, FResolveParams(FResolveRect(), (ECubeFace)CubeFace, MipIndex, 0, 0));
		}
	}
}

/** 
 * Updates the contents of the given reflection capture by rendering the scene. 
 * This must be called on the game thread.
 */
void FScene::UpdateReflectionCaptureContents(UReflectionCaptureComponent* CaptureComponent)
{
	const bool bCubemapSpecified = CaptureComponent->ReflectionSourceType == EReflectionSourceType::SpecifiedCubemap && CaptureComponent->Cubemap;
	const int32 ReflectionCaptureSize = UReflectionCaptureComponent::GetReflectionCaptureSize_GameThread();

	if (IsReflectionEnvironmentAvailable(GetFeatureLevel()) || bCubemapSpecified)
	{
		const FReflectionCaptureFullHDR* DerivedData = CaptureComponent->GetFullHDRData();

		// Upload existing derived data if it exists, instead of capturing
		if (DerivedData && DerivedData->HasValidData() )
		{
			// For other feature levels the reflection textures are stored on the component instead of in a scene-wide texture array
			if (GetFeatureLevel() >= ERHIFeatureLevel::SM5)
			{
				ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER( 
					UploadCaptureCommand,
					FScene*, Scene, this,
					const FReflectionCaptureFullHDR*, DerivedData, DerivedData,
					const UReflectionCaptureComponent*, CaptureComponent, CaptureComponent,
				{
					UploadReflectionCapture_RenderingThread(Scene, DerivedData, CaptureComponent);
				});

				if ( DoGPUArrayCopy() && !GIsEditor )
				{
					// We no longer need the HDR data, since we have a copy on the GPU
					// In the editor we need this data for serialization, however
					DerivedData = nullptr;
					CaptureComponent->ReleaseHDRData();
				}
			}
		}
		else
		{
			if (CaptureComponent->ReflectionSourceType == EReflectionSourceType::SpecifiedCubemap && !CaptureComponent->Cubemap)
			{
				return;
			}

			if (FPlatformProperties::RequiresCookedData())
			{
				UE_LOG(LogEngine, Warning, TEXT("No built data for %s, skipping generation in cooked build."), *CaptureComponent->GetPathName() )
				return;
			}

			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( 
				ClearCommand,
				int32, ReflectionCaptureSize, ReflectionCaptureSize,
			{
				ClearScratchCubemaps(RHICmdList, ReflectionCaptureSize);
			});
			
			if (CaptureComponent->ReflectionSourceType == EReflectionSourceType::CapturedScene)
			{
				CaptureSceneIntoScratchCubemap(this, CaptureComponent->GetComponentLocation() + CaptureComponent->CaptureOffset, ReflectionCaptureSize, false, true, 0, false, false, FLinearColor());
			}
			else if (CaptureComponent->ReflectionSourceType == EReflectionSourceType::SpecifiedCubemap)
			{
				ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(
					CopyCubemapCommand,
					UTextureCube*, SourceTexture, CaptureComponent->Cubemap,
					int32, ReflectionCaptureSize, ReflectionCaptureSize,
					float, SourceCubemapRotation, CaptureComponent->SourceCubemapAngle * (PI / 180.f),
					ERHIFeatureLevel::Type, FeatureLevel, GetFeatureLevel(),
					{
						CopyCubemapToScratchCubemap(RHICmdList, FeatureLevel, SourceTexture, ReflectionCaptureSize, false, false, SourceCubemapRotation, FLinearColor());
					});
			}
			else
			{
				check(!TEXT("Unknown reflection source type"));
			}

			ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER( 
				FilterCommand,
				ERHIFeatureLevel::Type, FeatureLevel, GetFeatureLevel(),
				int32, ReflectionCaptureSize, ReflectionCaptureSize,
				float&, AverageBrightness, *CaptureComponent->GetAverageBrightnessPtr(),
				{
					ComputeAverageBrightness(RHICmdList, FeatureLevel, ReflectionCaptureSize, AverageBrightness);
					FilterReflectionEnvironment(RHICmdList, FeatureLevel, ReflectionCaptureSize, NULL);
				}
			);

			// Create a proxy to represent the reflection capture to the rendering thread
			// The rendering thread will be responsible for deleting this when done with the filtering operation
			// We can't use the component's SceneProxy here because the component may not be registered with the scene
			FReflectionCaptureProxy* ReflectionProxy = new FReflectionCaptureProxy(CaptureComponent);

			ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER( 
				CopyCommand,
				FScene*, Scene, this,
				FReflectionCaptureProxy*, ReflectionProxy, ReflectionProxy,
				ERHIFeatureLevel::Type, FeatureLevel, GetFeatureLevel(),
			{
				if (FeatureLevel == ERHIFeatureLevel::SM5)
				{
					CopyToSceneArray(RHICmdList, Scene, ReflectionProxy);
				}
				else if (FeatureLevel == ERHIFeatureLevel::SM4)
				{
					CopyToComponentTexture(RHICmdList, Scene, ReflectionProxy);
				}

				// Clean up the proxy now that the rendering thread is done with it
				delete ReflectionProxy;
			});
		} //-V773
	}
}

void ReadbackRadianceMap(FRHICommandListImmediate& RHICmdList, int32 CubmapSize, TArray<FFloat16Color>& OutRadianceMap)
{
	OutRadianceMap.Empty(CubmapSize * CubmapSize * 6);
	OutRadianceMap.AddZeroed(CubmapSize * CubmapSize * 6);

	const int32 MipIndex = 0;

	FSceneRenderTargetItem& SourceCube = FSceneRenderTargets::Get(RHICmdList).ReflectionColorScratchCubemap[0]->GetRenderTargetItem();
	check(SourceCube.ShaderResourceTexture->GetFormat() == PF_FloatRGBA);
	const int32 CubeFaceBytes = CubmapSize * CubmapSize * OutRadianceMap.GetTypeSize();

	for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
	{
		TArray<FFloat16Color> SurfaceData;

		// Read each mip face
		RHICmdList.ReadSurfaceFloatData(SourceCube.ShaderResourceTexture, FIntRect(0, 0, CubmapSize, CubmapSize), SurfaceData, (ECubeFace)CubeFace, 0, MipIndex);
		const int32 DestIndex = CubeFace * CubmapSize * CubmapSize;
		FFloat16Color* FaceData = &OutRadianceMap[DestIndex];
		check(SurfaceData.Num() * SurfaceData.GetTypeSize() == CubeFaceBytes);
		FMemory::Memcpy(FaceData, SurfaceData.GetData(), CubeFaceBytes);
	}
}

void CopyToSkyTexture(FRHICommandList& RHICmdList, FScene* Scene, FTexture* ProcessedTexture)
{
	SCOPED_DRAW_EVENT(RHICmdList, CopyToSkyTexture);
	if (ProcessedTexture->TextureRHI)
	{
		const int32 EffectiveTopMipSize = ProcessedTexture->GetSizeX();
		const int32 NumMips = FMath::CeilLogTwo(EffectiveTopMipSize) + 1;
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		FSceneRenderTargetItem& FilteredCube = FSceneRenderTargets::Get(RHICmdList).ReflectionColorScratchCubemap[1]->GetRenderTargetItem();

		// GPU copy back to the skylight's texture, which is not a render target
		for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
		{
			for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
			{
				RHICmdList.CopyToResolveTarget(FilteredCube.ShaderResourceTexture, ProcessedTexture->TextureRHI, true, FResolveParams(FResolveRect(), (ECubeFace)CubeFace, MipIndex, 0, 0));
			}
		}
	}
}

// Warning: returns before writes to OutIrradianceEnvironmentMap have completed, as they are queued on the rendering thread
void FScene::UpdateSkyCaptureContents(
	const USkyLightComponent* CaptureComponent, 
	bool bCaptureEmissiveOnly, 
	UTextureCube* SourceCubemap, 
	FTexture* OutProcessedTexture, 
	float& OutAverageBrightness, 
	FSHVectorRGB3& OutIrradianceEnvironmentMap,
	TArray<FFloat16Color>* OutRadianceMap)
{	
	if (GSupportsRenderTargetFormat_PF_FloatRGBA || GetFeatureLevel() >= ERHIFeatureLevel::SM4)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UpdateSkyCaptureContents);
		{
			World = GetWorld();
			if (World)
			{
				//guarantee that all render proxies are up to date before kicking off this render
				World->SendAllEndOfFrameUpdates();
			}
		}
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ClearCommand,
			int32, CubemapSize, CaptureComponent->CubemapResolution,
		{
			ClearScratchCubemaps(RHICmdList, CubemapSize);
		});

		if (CaptureComponent->SourceType == SLS_CapturedScene)
		{
			bool bStaticSceneOnly = CaptureComponent->Mobility == EComponentMobility::Static;
			CaptureSceneIntoScratchCubemap(this, CaptureComponent->GetComponentLocation(), CaptureComponent->CubemapResolution, true, bStaticSceneOnly, CaptureComponent->SkyDistanceThreshold, CaptureComponent->bLowerHemisphereIsBlack, bCaptureEmissiveOnly, CaptureComponent->LowerHemisphereColor);
		}
		else if (CaptureComponent->SourceType == SLS_SpecifiedCubemap)
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER( 
				CopyCubemapCommand,
				UTextureCube*, SourceTexture, SourceCubemap,
				int32, CubemapSize, CaptureComponent->CubemapResolution,
				bool, bLowerHemisphereIsBlack, CaptureComponent->bLowerHemisphereIsBlack,
				float, SourceCubemapRotation, CaptureComponent->SourceCubemapAngle * (PI / 180.f),
				ERHIFeatureLevel::Type, FeatureLevel, GetFeatureLevel(),
				FLinearColor, LowerHemisphereColor, CaptureComponent->LowerHemisphereColor,
			{
				CopyCubemapToScratchCubemap(RHICmdList, FeatureLevel, SourceTexture, CubemapSize, true, bLowerHemisphereIsBlack, SourceCubemapRotation, LowerHemisphereColor);
			});
		}
		else
		{
			check(0);
		}

		if (OutRadianceMap)
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER( 
				ReadbackCommand,
				int32, CubemapSize, CaptureComponent->CubemapResolution,
				TArray<FFloat16Color>*, RadianceMap, OutRadianceMap,
			{
				ReadbackRadianceMap(RHICmdList, CubemapSize, *RadianceMap);
			});
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER( 
			FilterCommand,
			int32, CubemapSize, CaptureComponent->CubemapResolution,
			float&, AverageBrightness, OutAverageBrightness,
			FSHVectorRGB3*, IrradianceEnvironmentMap, &OutIrradianceEnvironmentMap,
			ERHIFeatureLevel::Type, FeatureLevel, GetFeatureLevel(),
		{
			if (FeatureLevel <= ERHIFeatureLevel::ES3_1)
			{
				MobileReflectionEnvironmentCapture::ComputeAverageBrightness(RHICmdList, FeatureLevel, CubemapSize, AverageBrightness);
				MobileReflectionEnvironmentCapture::FilterReflectionEnvironment(RHICmdList, FeatureLevel, CubemapSize, IrradianceEnvironmentMap);
			}
			else
			{
				ComputeAverageBrightness(RHICmdList, FeatureLevel, CubemapSize, AverageBrightness);
				FilterReflectionEnvironment(RHICmdList, FeatureLevel, CubemapSize, IrradianceEnvironmentMap);
			}
		});

		// Optionally copy the filtered mip chain to the output texture
		if (OutProcessedTexture)
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER( 
				CopyCommand,
				FScene*, Scene, this,
				FTexture*, ProcessedTexture, OutProcessedTexture,
				ERHIFeatureLevel::Type, FeatureLevel, GetFeatureLevel(),
			{
				if (FeatureLevel <= ERHIFeatureLevel::ES3_1)
				{
					MobileReflectionEnvironmentCapture::CopyToSkyTexture(RHICmdList, Scene, ProcessedTexture);
				}
				else
				{
					CopyToSkyTexture(RHICmdList, Scene, ProcessedTexture);
				}
			});
		}
	}
}
