// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MobileSceneCaptureRendering.cpp - Mobile specific scene capture code.
=============================================================================*/

#include "MobileSceneCaptureRendering.h"
#include "Misc/MemStack.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "UnrealClient.h"
#include "SceneInterface.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "RendererInterface.h"
#include "Shader.h"
#include "TextureResource.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "GlobalShader.h"
#include "SceneRenderTargetParameters.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneRendering.h"
#include "PostProcess/RenderTargetPool.h"
#include "PostProcess/SceneFilterRendering.h"
#include "ScreenRendering.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"

/**
* Shader set for the copy of scene color to capture target, decoding mosaic or RGBE encoded HDR image as part of a
* copy operation. Alpha channel will contain opacity information. (Determined from depth buffer content)
*/

// Use same defines as deferred for capture source defines,
extern const TCHAR* GShaderSourceModeDefineName[];

template<bool bDemosaic, ESceneCaptureSource CaptureSource>
class FMobileSceneCaptureCopyPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMobileSceneCaptureCopyPS, Global)
public:

	static bool ShouldCache(EShaderPlatform Platform) { return IsMobilePlatform(Platform); }

	FMobileSceneCaptureCopyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		InTexture.Bind(Initializer.ParameterMap, TEXT("InTexture"), SPF_Mandatory);
		InTextureSampler.Bind(Initializer.ParameterMap, TEXT("InTextureSampler"));
		SceneTextureParameters.Bind(Initializer.ParameterMap);
	}
	FMobileSceneCaptureCopyPS() {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("MOBILE_FORCE_DEPTH_TEXTURE_READS"), 1u);
		OutEnvironment.SetDefine(TEXT("DECODING_MOSAIC"), bDemosaic ? 1u : 0u);
		const TCHAR* DefineName = GShaderSourceModeDefineName[CaptureSource];
		if (DefineName)
		{
			OutEnvironment.SetDefine(DefineName, 1);
		}
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSamplerStateRHIParamRef SamplerStateRHI, FTextureRHIParamRef TextureRHI)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
		SetTextureParameter(RHICmdList, GetPixelShader(), InTexture, InTextureSampler, SamplerStateRHI, TextureRHI);
		SceneTextureParameters.Set(RHICmdList, GetPixelShader(), View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InTexture;
		Ar << InTextureSampler;
		Ar << SceneTextureParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter InTexture;
	FShaderResourceParameter InTextureSampler;
	FSceneTextureShaderParameters SceneTextureParameters;
};

/**
* A vertex shader for rendering a textured screen element.
* Additional texcoords are used when demosaic is required.
*/
template<bool bDemosaic>
class FMobileSceneCaptureCopyVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMobileSceneCaptureCopyVS, Global)
public:

	static bool ShouldCache(EShaderPlatform Platform) { return IsMobilePlatform(Platform); }

	FMobileSceneCaptureCopyVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		InvTexSizeParameter.Bind(Initializer.ParameterMap, TEXT("InvTexSize"));
	}
	FMobileSceneCaptureCopyVS() {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DECODING_MOSAIC"), bDemosaic ? 1u : 0u);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FIntPoint& SourceTexSize)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(), View.ViewUniformBuffer);
		if (InvTexSizeParameter.IsBound())
		{
			FVector2D InvTexSize(1.0f / SourceTexSize.X, 1.0f / SourceTexSize.Y);
			SetShaderValue(RHICmdList, GetVertexShader(), InvTexSizeParameter, InvTexSize);
		}
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InvTexSizeParameter;
		return bShaderHasOutdatedParameters;
	}

	FShaderParameter InvTexSizeParameter;
};

#define IMPLEMENT_MOBILE_SCENE_CAPTURECOPY(SCENETYPE) \
typedef FMobileSceneCaptureCopyPS<false,SCENETYPE> FMobileSceneCaptureCopyPS##SCENETYPE;\
typedef FMobileSceneCaptureCopyPS<true,SCENETYPE> FMobileSceneCaptureCopyPS_Mosaic##SCENETYPE;\
IMPLEMENT_SHADER_TYPE(template<>, FMobileSceneCaptureCopyPS##SCENETYPE, TEXT("/Engine/Private/MobileSceneCapture.usf"), TEXT("MainCopyPS"), SF_Pixel); \
IMPLEMENT_SHADER_TYPE(template<>, FMobileSceneCaptureCopyPS_Mosaic##SCENETYPE, TEXT("/Engine/Private/MobileSceneCapture.usf"), TEXT("MainCopyPS"), SF_Pixel);

IMPLEMENT_MOBILE_SCENE_CAPTURECOPY(SCS_SceneColorHDR);
IMPLEMENT_MOBILE_SCENE_CAPTURECOPY(SCS_SceneColorHDRNoAlpha);
IMPLEMENT_MOBILE_SCENE_CAPTURECOPY(SCS_SceneColorSceneDepth);
IMPLEMENT_MOBILE_SCENE_CAPTURECOPY(SCS_SceneDepth);
IMPLEMENT_SHADER_TYPE(template<>, FMobileSceneCaptureCopyVS<false>, TEXT("/Engine/Private/MobileSceneCapture.usf"), TEXT("MainCopyVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<>, FMobileSceneCaptureCopyVS<true>, TEXT("/Engine/Private/MobileSceneCapture.usf"), TEXT("MainCopyVS"), SF_Vertex);
 

template <bool bDemosaic, ESceneCaptureSource CaptureSource>
static FShader* SetCaptureToTargetShaders(FRHICommandListImmediate& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, FViewInfo& View, const FIntPoint& SourceTexSize, FTextureRHIParamRef SourceTextureRHI)
{
	TShaderMapRef<FMobileSceneCaptureCopyVS<bDemosaic>> VertexShader(View.ShaderMap);
	TShaderMapRef<FMobileSceneCaptureCopyPS<bDemosaic, CaptureSource>> PixelShader(View.ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	VertexShader->SetParameters(RHICmdList, View, SourceTexSize);
	PixelShader->SetParameters(RHICmdList, View, TStaticSamplerState<SF_Point>::GetRHI(), SourceTextureRHI);

	return *VertexShader;
}

template <bool bDemosaic>
static FShader* SetCaptureToTargetShaders(FRHICommandListImmediate& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ESceneCaptureSource CaptureSource, FViewInfo& View, const FIntPoint& SourceTexSize, FTextureRHIParamRef SourceTextureRHI)
{
	switch (CaptureSource)
	{
		case SCS_SceneColorHDR:
			return SetCaptureToTargetShaders<bDemosaic, SCS_SceneColorHDR>(RHICmdList, GraphicsPSOInit, View, SourceTexSize, SourceTextureRHI);
		case SCS_FinalColorLDR:
		case SCS_SceneColorHDRNoAlpha:
			return SetCaptureToTargetShaders<bDemosaic, SCS_SceneColorHDRNoAlpha>(RHICmdList, GraphicsPSOInit, View, SourceTexSize, SourceTextureRHI);
		case SCS_SceneColorSceneDepth:
			return SetCaptureToTargetShaders<bDemosaic, SCS_SceneColorSceneDepth>(RHICmdList, GraphicsPSOInit, View, SourceTexSize, SourceTextureRHI);
		case SCS_SceneDepth:
			return SetCaptureToTargetShaders<bDemosaic, SCS_SceneDepth>(RHICmdList, GraphicsPSOInit, View, SourceTexSize, SourceTextureRHI);
		default:
			checkNoEntry();
			return nullptr;
	}
}

// Copies into render target, optionally flipping it in the Y-axis
static void CopyCaptureToTarget(
	FRHICommandListImmediate& RHICmdList, 
	const FRenderTarget* Target, 
	const FIntPoint& TargetSize, 
	FViewInfo& View, 
	const FIntRect& ViewRect, 
	FTexture2DRHIParamRef SourceTextureRHI, 
	bool bNeedsFlippedRenderTarget,
	FSceneRenderer* SceneRenderer)
{
	check(SourceTextureRHI);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	ESceneCaptureSource CaptureSource = View.Family->SceneCaptureSource;
	ESceneCaptureCompositeMode CaptureCompositeMode = View.Family->SceneCaptureCompositeMode;

	// Normal and BaseColor not supported on mobile, fall back to scene colour.
	if (CaptureSource == SCS_Normal || CaptureSource == SCS_BaseColor)
	{
		CaptureSource = SCS_SceneColorHDR;
	}

	ERenderTargetLoadAction RTLoadAction;
	if (CaptureSource == SCS_SceneColorHDR && CaptureCompositeMode == SCCM_Composite)
	{
		// Blend with existing render target color. Scene capture color is already pre-multiplied by alpha.
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_SourceAlpha>::GetRHI();
		RTLoadAction = ERenderTargetLoadAction::ELoad;
	}
	else if (CaptureSource == SCS_SceneColorHDR && CaptureCompositeMode == SCCM_Additive)
	{
		// Add to existing render target color. Scene capture color is already pre-multiplied by alpha.
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_SourceAlpha>::GetRHI();
		RTLoadAction = ERenderTargetLoadAction::ELoad;
	}
	else
	{
		RTLoadAction = ERenderTargetLoadAction::ENoAction;
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	}

	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	FRHIRenderTargetView ColorView(Target->GetRenderTargetTexture(), 0, -1, RTLoadAction, ERenderTargetStoreAction::EStore);
	FRHISetRenderTargetsInfo Info(1, &ColorView, FRHIDepthRenderTargetView());
	RHICmdList.SetRenderTargetsAndClear(Info);
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();

	const bool bUsingDemosaic = IsMobileHDRMosaic();
	FShader* VertexShader;
	FIntPoint SourceTexSize = SourceTextureRHI->GetSizeXY();
	if (bUsingDemosaic)
	{
		VertexShader = SetCaptureToTargetShaders<true>(RHICmdList, GraphicsPSOInit, CaptureSource, View, SourceTexSize, SourceTextureRHI);
	}
	else
	{
		VertexShader = SetCaptureToTargetShaders<false>(RHICmdList, GraphicsPSOInit, CaptureSource, View, SourceTexSize, SourceTextureRHI);
	}

	if (bNeedsFlippedRenderTarget)
	{
		DrawRectangle(
			RHICmdList,
			ViewRect.Min.X, ViewRect.Min.Y,
			ViewRect.Width(), ViewRect.Height(),
			ViewRect.Min.X, ViewRect.Height() - ViewRect.Min.Y,
			ViewRect.Width(), -ViewRect.Height(),
			TargetSize,
			SourceTexSize,
			VertexShader,
			EDRF_UseTriangleOptimization);
	}
	else
	{
		DrawRectangle(
			RHICmdList,
			ViewRect.Min.X, ViewRect.Min.Y,
			ViewRect.Width(), ViewRect.Height(),
			ViewRect.Min.X, ViewRect.Min.Y,
			ViewRect.Width(), ViewRect.Height(),
			TargetSize,
			SourceTexSize,
			VertexShader,
			EDRF_UseTriangleOptimization);
	}

	// if opacity is needed.
	if (CaptureSource == SCS_SceneColorHDR)
	{
		// render translucent opacity. (to scene color)
		check(View.Family->Scene->GetShadingPath() == EShadingPath::Mobile);
		FMobileSceneRenderer* MobileSceneRenderer = (FMobileSceneRenderer*)SceneRenderer;
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EClearColorExistingDepth);

		MobileSceneRenderer->RenderInverseOpacity(RHICmdList, View);

		// Set capture target.
		FRHIRenderTargetView OpacityView(Target->GetRenderTargetTexture(), 0, -1, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore);
		FRHISetRenderTargetsInfo OpacityInfo(1, &OpacityView, FRHIDepthRenderTargetView());
		RHICmdList.SetRenderTargetsAndClear(OpacityInfo);
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		// Note lack of inverse, both the target and source images are already inverted.
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_ALPHA, BO_Add, BF_DestColor, BF_Zero, BO_Add, BF_Zero, BF_SourceAlpha>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		// Combine translucent opacity pass to earlier opaque pass to build final inverse opacity.
		TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
		TShaderMapRef<FScreenPS> PixelShader(View.ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*ScreenVertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		ScreenVertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
		PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SourceTextureRHI);

		int32 TargetPosY = ViewRect.Min.Y;
		int32 TargetHeight = ViewRect.Height();
	
		if (bNeedsFlippedRenderTarget)
		{
			TargetPosY = ViewRect.Height() - TargetPosY;
			TargetHeight = -TargetHeight;
		}

		DrawRectangle(
			RHICmdList,
			ViewRect.Min.X, ViewRect.Min.Y,
			ViewRect.Width(), ViewRect.Height(),
			ViewRect.Min.X, TargetPosY,
			ViewRect.Width(), TargetHeight,
			TargetSize,
			SourceTexSize,
			*ScreenVertexShader,
			EDRF_UseTriangleOptimization);
	}
}

void UpdateSceneCaptureContentMobile_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FSceneRenderer* SceneRenderer,
	FRenderTarget* RenderTarget,
	FTexture* RenderTargetTexture,
	const FName OwnerName,
	const FResolveParams& ResolveParams)
{
	FMemMark MemStackMark(FMemStack::Get());

	// update any resources that needed a deferred update
	FDeferredUpdateResource::UpdateResources(RHICmdList);
	bool bUseSceneTextures = SceneRenderer->ViewFamily.SceneCaptureSource != SCS_FinalColorLDR;

	{
#if WANTS_DRAW_MESH_EVENTS
		FString EventName;
		OwnerName.ToString(EventName);
		SCOPED_DRAW_EVENTF(RHICmdList, SceneCaptureMobile, TEXT("SceneCaptureMobile %s"), *EventName);
#else
		SCOPED_DRAW_EVENT(RHICmdList, UpdateSceneCaptureContentMobile_RenderThread);
#endif

		const bool bIsMobileHDR = IsMobileHDR();
		const bool bRHINeedsFlip = RHINeedsToSwitchVerticalAxis(GMaxRHIShaderPlatform);
		// note that GLES code will flip the image when:
		//	bIsMobileHDR && SceneCaptureSource == SCS_FinalColorLDR (flip performed during post processing)
		//	!bIsMobileHDR (rendering is flipped by vertex shader)
		// they need flipping again so it is correct for texture addressing.
		const bool bNeedsFlippedCopy = (!bIsMobileHDR || !bUseSceneTextures) && bRHINeedsFlip;
		const bool bNeedsFlippedFinalColor = bNeedsFlippedCopy && !bUseSceneTextures;

		// Intermediate render target that will need to be flipped (needed on !IsMobileHDR())
		TRefCountPtr<IPooledRenderTarget> FlippedPooledRenderTarget;

		const FRenderTarget* Target = SceneRenderer->ViewFamily.RenderTarget;
		if (bNeedsFlippedFinalColor)
		{
			// We need to use an intermediate render target since the result will be flipped
			auto& RenderTargetRHI = Target->GetRenderTargetTexture();
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Target->GetSizeXY(),
				RenderTargetRHI.GetReference()->GetFormat(),
				FClearValueBinding::None,
				TexCreate_None,
				TexCreate_RenderTargetable,
				false));
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, FlippedPooledRenderTarget, TEXT("SceneCaptureFlipped"));
		}

		// Helper class to allow setting render target
		struct FRenderTargetOverride : public FRenderTarget
		{
			FRenderTargetOverride(FRHITexture2D* In)
			{
				RenderTargetTextureRHI = In;
			}

			virtual FIntPoint GetSizeXY() const { return FIntPoint(RenderTargetTextureRHI->GetSizeX(), RenderTargetTextureRHI->GetSizeY()); }

			FTexture2DRHIRef GetTextureParamRef() { return RenderTargetTextureRHI; }
		} FlippedRenderTarget(
			FlippedPooledRenderTarget.GetReference()
			? FlippedPooledRenderTarget.GetReference()->GetRenderTargetItem().TargetableTexture->GetTexture2D()
			: nullptr);
		FViewInfo& View = SceneRenderer->Views[0];
		FIntRect ViewRect = View.ViewRect;
		FIntRect UnconstrainedViewRect = View.UnconstrainedViewRect;

		if(bNeedsFlippedFinalColor)
		{
			auto& RenderTargetRHI = Target->GetRenderTargetTexture();
			SetRenderTarget(RHICmdList, RenderTargetRHI, NULL, true);
			DrawClearQuad(RHICmdList, true, FLinearColor::Black, false, 0, false, 0, Target->GetSizeXY(), ViewRect);
		}

		// Render the scene normally
		{
			SCOPED_DRAW_EVENT(RHICmdList, RenderScene);

			if (bNeedsFlippedFinalColor)
			{
				// Hijack the render target
				SceneRenderer->ViewFamily.RenderTarget = &FlippedRenderTarget; //-V506
			}

			SceneRenderer->Render(RHICmdList);

			if (bNeedsFlippedFinalColor)
			{
				// And restore it
				SceneRenderer->ViewFamily.RenderTarget = Target;
			}
		}

		const FIntPoint TargetSize(UnconstrainedViewRect.Width(), UnconstrainedViewRect.Height());
		if (bNeedsFlippedFinalColor)
		{
			// We need to flip this texture upside down (since we depended on tonemapping to fix this on the hdr path)
			SCOPED_DRAW_EVENT(RHICmdList, FlipCapture);
			CopyCaptureToTarget(RHICmdList, Target, TargetSize, View, ViewRect, FlippedRenderTarget.GetTextureParamRef(), bNeedsFlippedCopy, SceneRenderer);
		}
		else if(bUseSceneTextures)
		{
			// Copy the captured scene into the destination texture
			SCOPED_DRAW_EVENT(RHICmdList, CaptureSceneColor);
			CopyCaptureToTarget(RHICmdList, Target, TargetSize, View, ViewRect, FSceneRenderTargets::Get(RHICmdList).GetSceneColorTexture()->GetTexture2D(), bNeedsFlippedCopy, SceneRenderer);
		}

		RHICmdList.CopyToResolveTarget(RenderTarget->GetRenderTargetTexture(), RenderTargetTexture->TextureRHI, false, ResolveParams);
	}
	FSceneRenderer::WaitForTasksClearSnapshotsAndDeleteSceneRenderer(RHICmdList, SceneRenderer);
}
