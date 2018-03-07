// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessUpscale.cpp: Post processing Upscale implementation.
=============================================================================*/

#include "PostProcess/PostProcessUpscale.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneFilterRendering.h"
#include "SceneRenderTargetParameters.h"
#include "PostProcess/PostProcessing.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"

static TAutoConsoleVariable<float> CVarUpscaleSoftness(
	TEXT("r.Upscale.Softness"),
	1.0f,
	TEXT("Amount of sharpening for Gaussian Unsharp filter (r.UpscaleQuality=5). Reduce if ringing is visible\n")
	TEXT("  1: Normal sharpening (default)\n")
	TEXT("  0: No sharpening (pure Gaussian)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarUpscalePaniniD(
	TEXT("r.Upscale.Panini.D"),
	0,
	TEXT("Allow and configure to apply a panini distortion to the rendered image. Values between 0 and 1 allow to fade the effect (lerp).\n")
	TEXT("Implementation from research paper \"Pannini: A New Projection for Rendering Wide Angle Perspective Images\"\n")
	TEXT(" 0: off (default)\n")
	TEXT(">0: enabled (requires an extra post processing pass if upsampling wasn't used - see r.ScreenPercentage)\n")
	TEXT(" 1: Panini cylindrical stereographic projection"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarUpscalePaniniS(
	TEXT("r.Upscale.Panini.S"),
	0,
	TEXT("Panini projection's hard vertical compression factor.\n")
	TEXT(" 0: no vertical compression factor (default)\n")
	TEXT(" 1: Hard vertical compression"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarUpscalePaniniScreenFit(
	TEXT("r.Upscale.Panini.ScreenFit"),
	1.0f,
	TEXT("Panini projection screen fit effect factor (lerp).\n")
	TEXT(" 0: fit vertically\n")
	TEXT(" 1: fit horizontally (default)"),
	ECVF_RenderThreadSafe);

const FRCPassPostProcessUpscale::PaniniParams FRCPassPostProcessUpscale::PaniniParams::Default;

FRCPassPostProcessUpscale::PaniniParams::PaniniParams(const FViewInfo& View)
{
	*this = PaniniParams();

	if (View.IsPerspectiveProjection() && !GEngine->StereoRenderingDevice.IsValid())
	{
		D = FMath::Max(CVarUpscalePaniniD.GetValueOnRenderThread(), 0.0f);
		S = CVarUpscalePaniniS.GetValueOnRenderThread();
		ScreenFit = FMath::Max(CVarUpscalePaniniScreenFit.GetValueOnRenderThread(), 0.0f);
	}
}

static FVector2D PaniniProjection(FVector2D OM, float d, float s)
{
	float PaniniDirectionXZInvLength = 1.0f / FMath::Sqrt(1.0f + OM.X * OM.X);
	float SinPhi = OM.X * PaniniDirectionXZInvLength;
	float TanTheta = OM.Y * PaniniDirectionXZInvLength;
	float CosPhi = FMath::Sqrt(1.0f - SinPhi * SinPhi);
	float S = (d + 1.0f) / (d + CosPhi);

	return S * FVector2D(SinPhi, FMath::Lerp(TanTheta, TanTheta / CosPhi, s));
}

/** Encapsulates the upscale vertex shader. */
class FPostProcessUpscaleVS : public FPostProcessVS
{
	DECLARE_SHADER_TYPE(FPostProcessUpscaleVS,Global);

	/** Default constructor. */
	FPostProcessUpscaleVS() {}

public:
	FShaderParameter PaniniParameters;

	/** Initialization constructor. */
	FPostProcessUpscaleVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FPostProcessVS(Initializer)
	{
		PaniniParameters.Bind(Initializer.ParameterMap,TEXT("PaniniParams"));
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPostProcessVS::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("TESS_RECT_X"), FTesselatedScreenRectangleIndexBuffer::Width);
		OutEnvironment.SetDefine(TEXT("TESS_RECT_Y"), FTesselatedScreenRectangleIndexBuffer::Height);
	}

	void SetParameters(const FRenderingCompositePassContext& Context, const FRCPassPostProcessUpscale::PaniniParams& InPaniniConfig)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		{
			const FVector2D FOVPerAxis = Context.View.ViewMatrices.ComputeHalfFieldOfViewPerAxis();
			const FVector2D ScreenPosToPaniniFactor = FVector2D(FMath::Tan(FOVPerAxis.X), FMath::Tan(FOVPerAxis.Y));
			const FVector2D PaniniDirection = FVector2D(1.0f, 0.0f) * ScreenPosToPaniniFactor;
			const FVector2D PaniniPosition = PaniniProjection(PaniniDirection, InPaniniConfig.D, InPaniniConfig.S);

			const float WidthFit = ScreenPosToPaniniFactor.X / PaniniPosition.X;
			const float OutScreenPosScale = FMath::Lerp(1.0f, WidthFit, InPaniniConfig.ScreenFit);

			FVector Value(InPaniniConfig.D, InPaniniConfig.S, OutScreenPosScale);

			SetShaderValue(Context.RHICmdList, ShaderRHI, PaniniParameters, Value);
		}
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FPostProcessVS::Serialize(Ar);
		Ar << PaniniParameters;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessUpscaleVS,TEXT("/Engine/Private/PostProcessUpscale.usf"),TEXT("MainVS"),SF_Vertex);

/** Encapsulates the post processing upscale pixel shader. */
template <uint32 Method>
class FPostProcessUpscalePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessUpscalePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		// Always allow point and bilinear upscale. (Provides upscaling for ES2 emulation)
		if (Method == 0 || Method == 1)
		{
			return true;
		}

		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("METHOD"), Method);
	}

	/** Default constructor. */
	FPostProcessUpscalePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter UpscaleSoftness;

	/** Initialization constructor. */
	FPostProcessUpscalePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		UpscaleSoftness.Bind(Initializer.ParameterMap,TEXT("UpscaleSoftness"));
	}

	template <typename TRHICmdList>
	void SetPS(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		FSamplerStateRHIParamRef FilterTable[2];
		FilterTable[0] = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		FilterTable[1] = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
			
		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, 0, eFC_0000, FilterTable);
		DeferredParameters.Set(RHICmdList, ShaderRHI, Context.View, MD_PostProcess);

		{
			float UpscaleSoftnessValue = FMath::Clamp(CVarUpscaleSoftness.GetValueOnRenderThread(), 0.0f, 1.0f);

			SetShaderValue(RHICmdList, ShaderRHI, UpscaleSoftness, UpscaleSoftnessValue);
		}
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << UpscaleSoftness;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/PostProcessUpscale.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS");
	}
};


// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessUpscalePS<A> FPostProcessUpscalePS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessUpscalePS##A, SF_Pixel);

VARIATION1(0)
VARIATION1(1)
VARIATION1(2)
VARIATION1(3)
VARIATION1(4)
VARIATION1(5)

#undef VARIATION1

FRCPassPostProcessUpscale::FRCPassPostProcessUpscale(const FViewInfo& InView, uint32 InUpscaleQuality, const PaniniParams& InPaniniConfig)
	: UpscaleQuality(InUpscaleQuality)
{
	PaniniConfig.D = FMath::Max(InPaniniConfig.D, 0.0f);
	PaniniConfig.S = InPaniniConfig.S;
	PaniniConfig.ScreenFit = FMath::Max(InPaniniConfig.ScreenFit, 0.0f);

	// Explicitly set output to viewport size, can't use input0 as downsized.
	if (InView.Family->RenderTarget->GetRenderTargetTexture())
	{
		OutputExtent.X = InView.Family->RenderTarget->GetRenderTargetTexture()->GetSizeX();
		OutputExtent.Y = InView.Family->RenderTarget->GetRenderTargetTexture()->GetSizeY();
	}
	else
	{
		OutputExtent = InView.Family->RenderTarget->GetSizeXY();
	}
}

template <uint32 Method, uint32 bTesselatedQuad>
FShader* FRCPassPostProcessUpscale::SetShader(const FRenderingCompositePassContext& Context, const PaniniParams& PaniniConfig)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	if(bTesselatedQuad)
	{
		check(PaniniConfig.D > 0.0f);

		TShaderMapRef<FPostProcessUpscaleVS> VertexShader(Context.GetShaderMap());
		TShaderMapRef<FPostProcessUpscalePS<Method> > PixelShader(Context.GetShaderMap());

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		PixelShader->SetPS(Context.RHICmdList, Context);
		VertexShader->SetParameters(Context, PaniniConfig);
		return *VertexShader;
	}
	else
	{
		check(PaniniConfig.D == 0.0f);

		TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
		TShaderMapRef<FPostProcessUpscalePS<Method> > PixelShader(Context.GetShaderMap());

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		PixelShader->SetPS(Context.RHICmdList, Context);
		VertexShader->SetParameters(Context);
		return *VertexShader;
	}
}

void FRCPassPostProcessUpscale::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessUpscale);
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	
	FIntRect SrcRect = View.ViewRect;
	// no upscale if separate ren target is used.
	FIntRect DestRect = (ViewFamily.bUseSeparateRenderTarget) ? View.ViewRect : View.UnscaledViewRect;
	FIntPoint SrcSize = InputDesc->Extent;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	if (!DestRenderTarget.TargetableTexture)
	{
		return;
	}

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	bool bTessellatedQuad = PaniniConfig.D >= 0.01f;

	// with distortion (bTessellatedQuad) we need to clear the background
	FIntRect ExcludeRect = bTessellatedQuad ? FIntRect() : DestRect;

	Context.SetViewportAndCallRHI(DestRect);
	if (View.StereoPass == eSSP_FULL || View.StereoPass == eSSP_LEFT_EYE)
	{
		DrawClearQuad(Context.RHICmdList, true, FLinearColor::Black, false, 0, false, 0, PassOutputs[0].RenderTargetDesc.Extent, ExcludeRect);
	}

	FShader* VertexShader = 0;

	if(bTessellatedQuad)
	{
		switch (UpscaleQuality)
		{
			case 0:	VertexShader = SetShader<0, 1>(Context, PaniniConfig); break;
			case 1:	VertexShader = SetShader<1, 1>(Context, PaniniConfig); break;
			case 2:	VertexShader = SetShader<2, 1>(Context, PaniniConfig); break;
			case 3:	VertexShader = SetShader<3, 1>(Context, PaniniConfig); break;
			case 4:	VertexShader = SetShader<4, 1>(Context, PaniniConfig); break;
			case 5:	VertexShader = SetShader<5, 1>(Context, PaniniConfig); break;
			default:
				checkNoEntry();
				break;
		}
	}
	else
	{
		switch (UpscaleQuality)
		{
			case 0:	VertexShader = SetShader<0, 0>(Context, PaniniParams::Default); break;
			case 1:	VertexShader = SetShader<1, 0>(Context, PaniniParams::Default); break;
			case 2:	VertexShader = SetShader<2, 0>(Context, PaniniParams::Default); break;
			case 3:	VertexShader = SetShader<3, 0>(Context, PaniniParams::Default); break;
			case 4:	VertexShader = SetShader<4, 0>(Context, PaniniParams::Default); break;
			case 5:	VertexShader = SetShader<5, 0>(Context, PaniniParams::Default); break;
			default:
				checkNoEntry();
				break;
		}
	}

	// Draw a quad, a triangle or a tessellated quad
	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		SrcSize,
		VertexShader,
		bTessellatedQuad ? EDRF_UseTesselatedIndexBuffer: EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessUpscale::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("Upscale");
	Ret.Extent = OutputExtent;
	Ret.Flags |= GFastVRamConfig.Upscale;

	return Ret;
}

FRCPassPostProcessUpscaleES2::FRCPassPostProcessUpscaleES2(const FViewInfo& InView)
:	FRCPassPostProcessUpscale(InView, 1 /* bilinear */)
,	View(InView)
{
}

FPooledRenderTargetDesc FRCPassPostProcessUpscaleES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = FRCPassPostProcessUpscale::ComputeOutputDesc(InPassOutputId);
	Ret.Extent = View.UnscaledViewRect.Max;
	return Ret;
}