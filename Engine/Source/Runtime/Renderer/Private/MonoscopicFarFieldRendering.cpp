// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MonoscopicFarFieldRendering.cpp: Monoscopic far field rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScreenRendering.h"
#include "SceneFilterRendering.h"
#include "PipelineStateCache.h"

/** Vertex shader to composite the monoscopic view into the stereo views. */
template<bool bMobileMultiView>
class FCompositeMonoscopicFarFieldViewVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCompositeMonoscopicFarFieldViewVS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FCompositeMonoscopicFarFieldViewVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		LateralOffsetNDCParameter.Bind(Initializer.ParameterMap, TEXT("LateralOffsetNDC"));
	}
	FCompositeMonoscopicFarFieldViewVS() {}


	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const float LateralOffset)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(), View.ViewUniformBuffer);
		SetShaderValue(RHICmdList, GetVertexShader(), LateralOffsetNDCParameter, LateralOffset);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		const bool bIsAndroidGLES = RHISupportsMobileMultiView(Platform);
		OutEnvironment.SetDefine(TEXT("MOBILE_MULTI_VIEW"), (bMobileMultiView && bIsAndroidGLES) ? 1u : 0u);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		const bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << LateralOffsetNDCParameter;
		return bShaderHasOutdatedParameters;
	}

	FShaderParameter LateralOffsetNDCParameter;
};

IMPLEMENT_SHADER_TYPE(template<>, FCompositeMonoscopicFarFieldViewVS<true>, TEXT("/Engine/Private/MonoscopicFarFieldRenderingVertexShader.usf"), TEXT("CompositeMonoscopicFarFieldView"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<>, FCompositeMonoscopicFarFieldViewVS<false>, TEXT("/Engine/Private/MonoscopicFarFieldRenderingVertexShader.usf"), TEXT("CompositeMonoscopicFarFieldView"), SF_Vertex);

/** Pixel shader to composite the monoscopic view into the stereo views. */
template<bool bMobileMultiView>
class FCompositeMonoscopicFarFieldViewPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCompositeMonoscopicFarFieldViewPS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	FCompositeMonoscopicFarFieldViewPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		MonoColorTextureParameter.Bind(Initializer.ParameterMap, TEXT("MonoColorTexture"));
		MonoColorTextureParameterSampler.Bind(Initializer.ParameterMap, TEXT("MonoColorTextureSampler"));
	}

	FCompositeMonoscopicFarFieldViewPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
		const FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		const FSamplerStateRHIRef Filter = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		SetTextureParameter(RHICmdList, GetPixelShader(), MonoColorTextureParameter, MonoColorTextureParameterSampler, Filter, SceneContext.GetSceneMonoColorTexture());
		SceneTextureParameters.Set(RHICmdList, GetPixelShader(), View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		const bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MonoColorTextureParameter;
		Ar << MonoColorTextureParameterSampler;
		Ar << SceneTextureParameters;
		return bShaderHasOutdatedParameters;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		const bool bIsAndroidGLES = RHISupportsMobileMultiView(Platform);
		OutEnvironment.SetDefine(TEXT("MOBILE_MULTI_VIEW"), (bMobileMultiView && bIsAndroidGLES) ? 1u : 0u);
	}

	FShaderResourceParameter MonoColorTextureParameter;
	FShaderResourceParameter MonoColorTextureParameterSampler;
	FSceneTextureShaderParameters SceneTextureParameters;
};

IMPLEMENT_SHADER_TYPE(template<>, FCompositeMonoscopicFarFieldViewPS<true>, TEXT("/Engine/Private/MonoscopicFarFieldRenderingPixelShader.usf"), TEXT("CompositeMonoscopicFarFieldView"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FCompositeMonoscopicFarFieldViewPS<false>, TEXT("/Engine/Private/MonoscopicFarFieldRenderingPixelShader.usf"), TEXT("CompositeMonoscopicFarFieldView"), SF_Pixel);

/**
Pixel Shader to mask the monoscopic far field view's depth buffer where pixels were rendered into the stereo views.
This ensures we only render pixels in the monoscopic far field view that will be visible.
*/
template<bool bMobileMultiView>
class FMonoscopicFarFieldMaskPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMonoscopicFarFieldMaskPS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	FMonoscopicFarFieldMaskPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		MobileSceneColorTexture.Bind(Initializer.ParameterMap, TEXT("MobileSceneColorTexture"));
		MobileSceneColorTextureSampler.Bind(Initializer.ParameterMap, TEXT("MobileSceneColorTextureSampler"));
		LeftViewWidthNDCParameter.Bind(Initializer.ParameterMap, TEXT("LeftViewWidthNDC"));
		LateralOffsetNDCParameter.Bind(Initializer.ParameterMap, TEXT("LateralOffsetNDC"));
	}

	FMonoscopicFarFieldMaskPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FTextureRHIParamRef CurrentSceneColor, const float LeftViewWidthNDC, const float LateralOffsetNDC)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
		const FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		const FSamplerStateRHIRef Filter = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		SetTextureParameter(RHICmdList, GetPixelShader(), MobileSceneColorTexture, MobileSceneColorTextureSampler, Filter, CurrentSceneColor);

		SetShaderValue(RHICmdList, GetPixelShader(), LeftViewWidthNDCParameter, LeftViewWidthNDC);
		SetShaderValue(RHICmdList, GetPixelShader(), LateralOffsetNDCParameter, LateralOffsetNDC);

		SceneTextureParameters.Set(RHICmdList, GetPixelShader(), View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		const bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MobileSceneColorTexture;
		Ar << MobileSceneColorTextureSampler;
		Ar << LeftViewWidthNDCParameter;
		Ar << LateralOffsetNDCParameter;
		Ar << SceneTextureParameters;
		return bShaderHasOutdatedParameters;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		const bool bIsAndroidGLES = RHISupportsMobileMultiView(Platform);
		OutEnvironment.SetDefine(TEXT("MOBILE_MULTI_VIEW"), (bMobileMultiView && bIsAndroidGLES) ? 1u : 0u);
	}

	FShaderResourceParameter MobileSceneColorTexture;
	FShaderResourceParameter MobileSceneColorTextureSampler;

	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderParameter LeftViewWidthNDCParameter;
	FShaderParameter LateralOffsetNDCParameter;
};

IMPLEMENT_SHADER_TYPE(template<>, FMonoscopicFarFieldMaskPS<true>, TEXT("/Engine/Private/MonoscopicFarFieldRenderingPixelShader.usf"), TEXT("MonoscopicFarFieldMask"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FMonoscopicFarFieldMaskPS<false>, TEXT("/Engine/Private/MonoscopicFarFieldRenderingPixelShader.usf"), TEXT("MonoscopicFarFieldMask"), SF_Pixel);

void FSceneRenderer::RenderMonoscopicFarFieldMask(FRHICommandListImmediate& RHICmdList)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	const FTextureRHIParamRef CurrentSceneColor = GetMultiViewSceneColor(SceneContext);

	SceneContext.BeginRenderingSceneMonoColor(RHICmdList, ESimpleRenderTargetMode::EClearColorAndDepth);

	const FViewInfo& LeftView = Views[0];
	const FViewInfo& RightView = Views[1];
	const FViewInfo& MonoView = Views[2];

	const float LeftViewWidthNDC = static_cast<float>(RightView.ViewRect.Min.X - LeftView.ViewRect.Min.X) / static_cast<float>(SceneContext.GetBufferSizeXY().X);
	const float LateralOffsetInPixels = roundf(ViewFamily.MonoParameters.LateralOffset * static_cast<float>(MonoView.ViewRect.Width()));
	const float LateralOffsetNDC = LateralOffsetInPixels / static_cast<float>(SceneContext.GetBufferSizeXY().X);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_Always>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

	TShaderMapRef<FScreenVS> VertexShader(MonoView.ShaderMap);
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);

	if (LeftView.bIsMobileMultiViewEnabled)
	{
		TShaderMapRef<FMonoscopicFarFieldMaskPS<true>> PixelShader(MonoView.ShaderMap);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		PixelShader->SetParameters(RHICmdList, MonoView, CurrentSceneColor, LeftViewWidthNDC, LateralOffsetNDC);

		RHICmdList.SetViewport(
			MonoView.ViewRect.Min.X,
			MonoView.ViewRect.Min.Y,
			1.0,
			MonoView.ViewRect.Max.X,
			MonoView.ViewRect.Max.Y,
			1.0);

		DrawRectangle(
			RHICmdList,
			0, 0,
			MonoView.ViewRect.Width(), MonoView.ViewRect.Height(),
			LeftView.ViewRect.Min.X, LeftView.ViewRect.Min.Y,
			LeftView.ViewRect.Width(), LeftView.ViewRect.Height(),
			FIntPoint(MonoView.ViewRect.Width(), MonoView.ViewRect.Height()),
			LeftView.ViewRect.Size(),
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}
	else
	{
		TShaderMapRef<FMonoscopicFarFieldMaskPS<false>> PixelShader(MonoView.ShaderMap);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		PixelShader->SetParameters(RHICmdList, MonoView, CurrentSceneColor, LeftViewWidthNDC, LateralOffsetNDC);

		RHICmdList.SetViewport(
			MonoView.ViewRect.Min.X,
			MonoView.ViewRect.Min.Y,
			1.0,
			MonoView.ViewRect.Max.X,
			MonoView.ViewRect.Max.Y,
			1.0);

		DrawRectangle(
			RHICmdList,
			0, 0,
			MonoView.ViewRect.Width(), MonoView.ViewRect.Height(),
			LeftView.ViewRect.Min.X, LeftView.ViewRect.Min.Y,
			LeftView.ViewRect.Width(), LeftView.ViewRect.Height(),
			FIntPoint(MonoView.ViewRect.Width(), MonoView.ViewRect.Height()),
			SceneContext.GetBufferSizeXY(),
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}
}

void FSceneRenderer::CompositeMonoscopicFarField(FRHICommandListImmediate& RHICmdList)
{
	if (ViewFamily.MonoParameters.Mode == EMonoscopicFarFieldMode::On || ViewFamily.MonoParameters.Mode == EMonoscopicFarFieldMode::MonoOnly)
	{
		const FViewInfo& LeftView = Views[0];
		const FViewInfo& RightView = Views[1];
		const FViewInfo& MonoView = Views[2];

		const FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		const FTextureRHIParamRef CurrentSceneColor = GetMultiViewSceneColor(SceneContext);

		const FTextureRHIParamRef SceneDepth = (LeftView.bIsMobileMultiViewEnabled) ? SceneContext.MobileMultiViewSceneDepthZ->GetRenderTargetItem().TargetableTexture : static_cast<FTextureRHIRef>(SceneContext.GetSceneDepthTexture());
		SetRenderTarget(RHICmdList, CurrentSceneColor, SceneDepth, ESimpleRenderTargetMode::EExistingColorAndDepth);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		if (ViewFamily.MonoParameters.Mode == EMonoscopicFarFieldMode::MonoOnly)
		{
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero>::GetRHI();
		}
		else
		{
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_InverseDestAlpha, BF_One>::GetRHI();
		}

		if (!LeftView.bIsMobileMultiViewEnabled)
		{
			TShaderMapRef<FCompositeMonoscopicFarFieldViewVS<false>> VertexShader(MonoView.ShaderMap);
			TShaderMapRef<FCompositeMonoscopicFarFieldViewPS<false>> PixelShader(MonoView.ShaderMap);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			PixelShader->SetParameters(RHICmdList, MonoView);

			VertexShader->SetParameters(RHICmdList, MonoView, ViewFamily.MonoParameters.LateralOffset);

			const int32 LateralOffsetInPixels = static_cast<int32>(roundf(ViewFamily.MonoParameters.LateralOffset * static_cast<float>(MonoView.ViewRect.Width())));

			RHICmdList.SetViewport(LeftView.ViewRect.Min.X, LeftView.ViewRect.Min.Y, 0.0f, LeftView.ViewRect.Max.X, LeftView.ViewRect.Max.Y, 1.0f);
			DrawRectangle(
				RHICmdList,
				0, 0,
				LeftView.ViewRect.Width(), LeftView.ViewRect.Height(),
				MonoView.ViewRect.Min.X + LateralOffsetInPixels, MonoView.ViewRect.Min.Y,
				LeftView.ViewRect.Width(), LeftView.ViewRect.Height(),
				LeftView.ViewRect.Size(),
				MonoView.ViewRect.Max,
				*VertexShader,
				EDRF_UseTriangleOptimization);

			RHICmdList.SetViewport(RightView.ViewRect.Min.X, RightView.ViewRect.Min.Y, 0.0f, RightView.ViewRect.Max.X, RightView.ViewRect.Max.Y, 1.0f);

			// Composite into right eye
			DrawRectangle(
				RHICmdList,
				0, 0,
				LeftView.ViewRect.Width(), LeftView.ViewRect.Height(),
				MonoView.ViewRect.Min.X - LateralOffsetInPixels, MonoView.ViewRect.Min.Y,
				LeftView.ViewRect.Width(), LeftView.ViewRect.Height(),
				LeftView.ViewRect.Size(),
				MonoView.ViewRect.Max,
				*VertexShader,
				EDRF_UseTriangleOptimization);

		}
		else
		{
			TShaderMapRef<FCompositeMonoscopicFarFieldViewVS<true>> VertexShader(MonoView.ShaderMap);
			TShaderMapRef<FCompositeMonoscopicFarFieldViewPS<true>> PixelShader(MonoView.ShaderMap);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			PixelShader->SetParameters(RHICmdList, MonoView);

			VertexShader->SetParameters(RHICmdList, MonoView, ViewFamily.MonoParameters.LateralOffset);

			RHICmdList.SetViewport(LeftView.ViewRect.Min.X, LeftView.ViewRect.Min.Y, 0.0f, LeftView.ViewRect.Max.X, LeftView.ViewRect.Max.Y, 1.0f);
			DrawRectangle(
				RHICmdList,
				0, 0,
				LeftView.ViewRect.Width(), LeftView.ViewRect.Height(),
				MonoView.ViewRect.Min.X, MonoView.ViewRect.Min.Y,
				LeftView.ViewRect.Width(), LeftView.ViewRect.Height(),
				LeftView.ViewRect.Size(),
				MonoView.ViewRect.Max,
				*VertexShader,
				EDRF_UseTriangleOptimization);
		}
	}

	// Remove the mono view before moving to post.
	Views.RemoveAt(2);
	ViewFamily.Views.RemoveAt(2);
}
