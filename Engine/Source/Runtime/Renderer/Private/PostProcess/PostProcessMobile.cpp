// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessMobile.cpp: Uber post for mobile implementation.
=============================================================================*/

#include "PostProcess/PostProcessMobile.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "SceneRenderTargetParameters.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessEyeAdaptation.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"

static EPixelFormat GetHDRPixelFormat()
{
	// PF_B8G8R8A8 instead of floats for 32bpp hdr encoding.
	return IsMobileHDR32bpp() ? PF_B8G8R8A8 : PF_FloatRGBA;
}

// return Depth of Field Scale if Gaussian DoF mode is active. 0.0f otherwise.
float GetMobileDepthOfFieldScale(const FViewInfo& View)
{
	return View.FinalPostProcessSettings.DepthOfFieldMethod == DOFM_Gaussian ? View.FinalPostProcessSettings.DepthOfFieldScale : 0.0f;
}

//
// BLOOM SETUP
//

class FPostProcessBloomSetupVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomSetupVS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	/** Default constructor. */
	FPostProcessBloomSetupVS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	/** Initialization constructor. */
	FPostProcessBloomSetupVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

template <uint32 UseSunDof> // 0=none, 1=dof, 2=sun, 3=sun&dof, 4=msaa
class FPostProcessBloomSetupPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomSetupPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);

		//Need to hack in exposure scale for < SM5
		OutEnvironment.SetDefine(TEXT("NO_EYEADAPTATION_EXPOSURE_FIX"), 1);

		CA_SUPPRESS(6313);
		OutEnvironment.SetDefine(TEXT("ES2_USE_MSAA"), (UseSunDof & 4) ? (uint32)1 : (uint32)0);
		CA_SUPPRESS(6313);
		OutEnvironment.SetDefine(TEXT("ES2_USE_SUN"), (UseSunDof & 2) ? (uint32)1 : (uint32)0);
		CA_SUPPRESS(6313);
		OutEnvironment.SetDefine(TEXT("ES2_USE_DOF"), (UseSunDof & 1) ? (uint32)1 : (uint32)0);
	}

	FPostProcessBloomSetupPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter BloomThreshold;

	/** Initialization constructor. */
	FPostProcessBloomSetupPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		BloomThreshold.Bind(Initializer.ParameterMap, TEXT("BloomThreshold"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		SetShaderValue(Context.RHICmdList, ShaderRHI, BloomThreshold, Settings.BloomThreshold);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << BloomThreshold;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomSetupVS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("BloomVS_ES2"),SF_Vertex);

typedef FPostProcessBloomSetupPS_ES2<0> FPostProcessBloomSetupPS_ES2_0;
typedef FPostProcessBloomSetupPS_ES2<1> FPostProcessBloomSetupPS_ES2_1;
typedef FPostProcessBloomSetupPS_ES2<2> FPostProcessBloomSetupPS_ES2_2;
typedef FPostProcessBloomSetupPS_ES2<3> FPostProcessBloomSetupPS_ES2_3;
typedef FPostProcessBloomSetupPS_ES2<4> FPostProcessBloomSetupPS_ES2_4;
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessBloomSetupPS_ES2_0,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("BloomPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessBloomSetupPS_ES2_1,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("BloomPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessBloomSetupPS_ES2_2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("BloomPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessBloomSetupPS_ES2_3,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("BloomPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessBloomSetupPS_ES2_4,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("BloomPS_ES2"),SF_Pixel);

template <uint32 UseSunDof>
static void BloomSetup_SetShader(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessBloomSetupVS_ES2> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessBloomSetupPS_ES2<UseSunDof> > PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessBloomSetupES2::SetShader(const FRenderingCompositePassContext& Context)
{
	const FSceneView& View = Context.View;
	uint32 UseSun = Context.View.bLightShaftUse ? 1 : 0;
	uint32 UseDof = (GetMobileDepthOfFieldScale(Context.View) > 0.0f) ? 1 : 0;
	uint32 UseSunDof = (UseSun << 1) + UseDof;

	static const auto CVarMobileMSAA = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileMSAA"));
	const EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[Context.GetFeatureLevel()];
	UseSunDof += (GSupportsShaderFramebufferFetch && (ShaderPlatform == SP_METAL || IsVulkanMobilePlatform(ShaderPlatform))) ? (CVarMobileMSAA ? (CVarMobileMSAA->GetValueOnRenderThread() > 1 ? 4 : 0) : 0) : 0;

	switch(UseSunDof)
	{
		case 0: BloomSetup_SetShader<0>(Context); break;
		case 1: BloomSetup_SetShader<1>(Context); break;
		case 2: BloomSetup_SetShader<2>(Context); break;
		case 3: BloomSetup_SetShader<3>(Context); break;
		case 4: BloomSetup_SetShader<4>(Context); break;
	}
}

void FRCPassPostProcessBloomSetupES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessBloomSetup);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	FIntPoint PrePostSourceViewportSize = PrePostSourceViewportRect.Size();
	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/4);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/4);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	FIntPoint DstSize = PrePostSourceViewportSize / 4;

	FIntPoint SrcSize;
	FIntRect SrcRect;
	if(bUseViewRectSource)
	{
		// Mobile with framebuffer fetch uses view rect as source.
		const FSceneView& View = Context.View;
		SrcSize = InputDesc->Extent;
		//	uint32 ScaleFactor = View.ViewRect.Width() / SrcSize.X;
		//	SrcRect = View.ViewRect / ScaleFactor;
		// TODO: This won't work with scaled views.
		SrcRect = PrePostSourceViewportRect;
	}
	else
	{
		// Otherwise using exact size texture.
		SrcSize = DstSize;
		SrcRect = DstRect;
	}

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// OverrideRenderTarget might patch out final render target and we have no control of the clear color anymore
	if (DestRenderTarget.TargetableTexture->GetClearColor() == FLinearColor::Black)
	{
		FRHIRenderTargetView View = FRHIRenderTargetView(DestRenderTarget.TargetableTexture, ERenderTargetLoadAction::EClear);
		FRHISetRenderTargetsInfo Info(1, &View, FRHIDepthRenderTargetView());
		Context.RHICmdList.SetRenderTargetsAndClear(Info);
	}
	else
	{
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		DrawClearQuad(Context.RHICmdList, FLinearColor::Black);
	}

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	SetShader(Context);

	TShaderMapRef<FPostProcessBloomSetupVS_ES2> VertexShader(Context.GetShaderMap());

	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DstX, DstY,
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DstSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessBloomSetupES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = GetHDRPixelFormat();
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportRect.Width() / 4);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportRect.Height() / 4);
	Ret.DebugName = TEXT("BloomSetup");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	return Ret;
}




//
// BLOOM SETUP SMALL (BLOOM)
//

class FPostProcessBloomSetupSmallVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomSetupSmallVS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	/** Default constructor. */
	FPostProcessBloomSetupSmallVS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	/** Initialization constructor. */
	FPostProcessBloomSetupSmallVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

class FPostProcessBloomSetupSmallPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomSetupSmallPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessBloomSetupSmallPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter BloomThreshold;

	/** Initialization constructor. */
	FPostProcessBloomSetupSmallPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		BloomThreshold.Bind(Initializer.ParameterMap, TEXT("BloomThreshold"));
	}

	template <typename TRHICmdList>
	void SetPS(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		SetShaderValue(RHICmdList, ShaderRHI, BloomThreshold, Settings.BloomThreshold);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << BloomThreshold;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomSetupSmallVS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("BloomSmallVS_ES2"),SF_Vertex);

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomSetupSmallPS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("BloomSmallPS_ES2"),SF_Pixel);


void FRCPassPostProcessBloomSetupSmallES2::SetShader(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessBloomSetupSmallVS_ES2> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessBloomSetupSmallPS_ES2> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context.RHICmdList, Context);
}

void FRCPassPostProcessBloomSetupSmallES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessBloomSetupSmall);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/4);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/4);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	FIntPoint DstSize = PrePostSourceViewportSize / 4;

	FIntPoint SrcSize;
	FIntRect SrcRect;
	if(bUseViewRectSource)
	{
		// Mobile with framebuffer fetch uses view rect as source.
		const FSceneView& View = Context.View;
		SrcSize = InputDesc->Extent;
		//	uint32 ScaleFactor = View.ViewRect.Width() / SrcSize.X;
		//	SrcRect = View.ViewRect / ScaleFactor;
		// TODO: This won't work with scaled views.
		SrcRect = View.ViewRect;
	}
	else
	{
		// Otherwise using exact size texture.
		SrcSize = DstSize;
		SrcRect = DstRect;
	}

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	
	// OverrideRenderTarget might patch out final render target and we have no control of the clear color anymore
	if (DestRenderTarget.TargetableTexture->GetClearColor() == FLinearColor::Black)
	{
		FRHIRenderTargetView View = FRHIRenderTargetView(DestRenderTarget.TargetableTexture, ERenderTargetLoadAction::EClear);
		FRHISetRenderTargetsInfo Info(1, &View, FRHIDepthRenderTargetView());
		Context.RHICmdList.SetRenderTargetsAndClear(Info);
	}
	else
	{
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		DrawClearQuad(Context.RHICmdList, FLinearColor::Black);
	}

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	SetShader(Context);

	TShaderMapRef<FPostProcessBloomSetupSmallVS_ES2> VertexShader(Context.GetShaderMap());
	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DstX, DstY,
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DstSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessBloomSetupSmallES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = GetHDRPixelFormat();
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/4);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/4);
	Ret.DebugName = TEXT("BloomSetupSmall");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	return Ret;
}






//
// BLOOM DOWNSAMPLE
//

class FPostProcessBloomDownPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomDownPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessBloomDownPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessBloomDownPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	template <typename TRHICmdList>
	void SetPS(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomDownPS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("BloomDownPS_ES2"),SF_Pixel);


class FPostProcessBloomDownVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomDownVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessBloomDownVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter BloomDownScale;

	FPostProcessBloomDownVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		BloomDownScale.Bind(Initializer.ParameterMap, TEXT("BloomDownScale"));
	}

	void SetVS(const FRenderingCompositePassContext& Context, float InScale)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		SetShaderValue(Context.RHICmdList, ShaderRHI, BloomDownScale, InScale);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << BloomDownScale;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomDownVS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("BloomDownVS_ES2"),SF_Vertex);


void FRCPassPostProcessBloomDownES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessBloomDown);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/2);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/2);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	
	// OverrideRenderTarget might patch out final render target and we have no control of the clear color anymore
	if (DestRenderTarget.TargetableTexture->GetClearColor() == FLinearColor::Black)
	{
		FRHIRenderTargetView View = FRHIRenderTargetView(DestRenderTarget.TargetableTexture, ERenderTargetLoadAction::EClear);
		FRHISetRenderTargetsInfo Info(1, &View, FRHIDepthRenderTargetView());
		Context.RHICmdList.SetRenderTargetsAndClear(Info);
	}
	else
	{
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		DrawClearQuad(Context.RHICmdList, FLinearColor::Black);
	}

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessBloomDownVS_ES2> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessBloomDownPS_ES2> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetVS(Context, Scale);
	PixelShader->SetPS(Context.RHICmdList, Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize/2;

	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessBloomDownES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = GetHDRPixelFormat();
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/2);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/2);
	Ret.DebugName = TEXT("BloomDown");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	return Ret;
}





//
// BLOOM UPSAMPLE
//

class FPostProcessBloomUpPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomUpPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessBloomUpPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter TintA;
	FShaderParameter TintB;

	FPostProcessBloomUpPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		TintA.Bind(Initializer.ParameterMap, TEXT("BloomTintA"));
		TintB.Bind(Initializer.ParameterMap, TEXT("BloomTintB"));
	}

	template <typename TRHICmdList>
	void SetPS(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, FVector4& InTintA, FVector4& InTintB)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		SetShaderValue(RHICmdList, ShaderRHI, TintA, InTintA);
		SetShaderValue(RHICmdList, ShaderRHI, TintB, InTintB);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << TintA << TintB;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomUpPS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("BloomUpPS_ES2"),SF_Pixel);


class FPostProcessBloomUpVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomUpVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessBloomUpVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter BloomUpScales;

	FPostProcessBloomUpVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		BloomUpScales.Bind(Initializer.ParameterMap, TEXT("BloomUpScales"));
	}

	void SetVS(const FRenderingCompositePassContext& Context, FVector2D InScale)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		SetShaderValue(Context.RHICmdList, ShaderRHI, BloomUpScales, InScale);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << BloomUpScales;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomUpVS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("BloomUpVS_ES2"),SF_Vertex);


void FRCPassPostProcessBloomUpES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessBloomUp);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	
	// OverrideRenderTarget might patch out final render target and we have no control of the clear color anymore
	if (DestRenderTarget.TargetableTexture->GetClearColor() == FLinearColor::Black)
	{
		FRHIRenderTargetView View = FRHIRenderTargetView(DestRenderTarget.TargetableTexture, ERenderTargetLoadAction::EClear);
		FRHISetRenderTargetsInfo Info(1, &View, FRHIDepthRenderTargetView());
		Context.RHICmdList.SetRenderTargetsAndClear(Info);
	}
	else
	{
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		DrawClearQuad(Context.RHICmdList, FLinearColor::Black);
	}

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessBloomUpVS_ES2> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessBloomUpPS_ES2> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	// The 1/8 factor is because bloom is using 8 taps in the filter.
	VertexShader->SetVS(Context, FVector2D(ScaleAB.X, ScaleAB.Y));
	FVector4 TintAScaled = TintA * (1.0f/8.0f);
	FVector4 TintBScaled = TintB * (1.0f/8.0f);
	PixelShader->SetPS(Context.RHICmdList, Context, TintAScaled, TintBScaled);

	FIntPoint SrcDstSize = PrePostSourceViewportSize;

	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessBloomUpES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = GetHDRPixelFormat();
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y);
	Ret.DebugName = TEXT("BloomUp");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	return Ret;
}





//
// SUN MASK
//

template <uint32 UseFetchSunDof, bool bUseDepthTexture> // 0=none, 1=dof, 2=sun, 3=sun&dof, {4,5,6,7}=ES2_USE_FETCH, 8=MSAA-pre-resolve
class FPostProcessSunMaskPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunMaskPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ES2_USE_DEPTHTEXTURE"), bUseDepthTexture ? (uint32)1 : (uint32)0);
		CA_SUPPRESS(6313);
		OutEnvironment.SetDefine(TEXT("ES2_USE_MSAA"), (UseFetchSunDof & 8) ? (uint32)1 : (uint32)0);
		CA_SUPPRESS(6313);
		OutEnvironment.SetDefine(TEXT("ES2_USE_FETCH"), (UseFetchSunDof & 4) ? (uint32)1 : (uint32)0);
		CA_SUPPRESS(6313);
		OutEnvironment.SetDefine(TEXT("ES2_USE_SUN"), (UseFetchSunDof & 2) ? (uint32)1 : (uint32)0);
		CA_SUPPRESS(6313);
		OutEnvironment.SetDefine(TEXT("ES2_USE_DOF"), (UseFetchSunDof & 1) ? (uint32)1 : (uint32)0);
	}

	FPostProcessSunMaskPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter SunColorApertureDiv2Parameter;

	FDeferredPixelShaderParameters DeferredParameters;

	FPostProcessSunMaskPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		SunColorApertureDiv2Parameter.Bind(Initializer.ParameterMap, TEXT("SunColorApertureDiv2"));
		DeferredParameters.Bind(Initializer.ParameterMap);
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		FVector4 SunColorApertureDiv2;
		SunColorApertureDiv2.X = Context.View.LightShaftColorMask.R;
		SunColorApertureDiv2.Y = Context.View.LightShaftColorMask.G;
		SunColorApertureDiv2.Z = Context.View.LightShaftColorMask.B;
		SunColorApertureDiv2.W = GetMobileDepthOfFieldScale(Context.View) * 0.5f;
		SetShaderValue(Context.RHICmdList, ShaderRHI, SunColorApertureDiv2Parameter, SunColorApertureDiv2);

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View, MD_PostProcess);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << SunColorApertureDiv2Parameter << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}
};

// #define avoids a lot of code duplication
#define SUNMASK_PS_ES2(A) typedef FPostProcessSunMaskPS_ES2<A,true> FPostProcessSunMaskPS_ES2_DEPTH_##A; \
	IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMaskPS_ES2_DEPTH_##A,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunMaskPS_ES2"), SF_Pixel); \
	typedef FPostProcessSunMaskPS_ES2<A,false> FPostProcessSunMaskPS_ES2_##A; \
	IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMaskPS_ES2_##A,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunMaskPS_ES2"), SF_Pixel);

SUNMASK_PS_ES2(0)  SUNMASK_PS_ES2(1)  SUNMASK_PS_ES2(2)  SUNMASK_PS_ES2(3)  SUNMASK_PS_ES2(4)
SUNMASK_PS_ES2(5)  SUNMASK_PS_ES2(6)  SUNMASK_PS_ES2(7)  SUNMASK_PS_ES2(8)

#undef SUNMASK_PS_ES2


class FPostProcessSunMaskVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunMaskVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessSunMaskVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessSunMaskVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunMaskVS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunMaskVS_ES2"),SF_Vertex);

template <uint32 UseFetchSunDof, bool bUseDepthTexture>
static void SunMask_SetShader(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessSunMaskVS_ES2> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessSunMaskPS_ES2<UseFetchSunDof, bUseDepthTexture> > PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

template <bool bUseDepthTexture>
void FRCPassPostProcessSunMaskES2::SetShader(const FRenderingCompositePassContext& Context)
{
	const FSceneView& View = Context.View;
	uint32 UseSun = Context.View.bLightShaftUse ? 1 : 0;
	uint32 UseDof = (GetMobileDepthOfFieldScale(Context.View) > 0.0f) ? 1 : 0;
	uint32 UseFetch = GSupportsShaderFramebufferFetch ? 1 : 0;
	uint32 UseFetchSunDof = (UseFetch << 2) + (UseSun << 1) + UseDof;

	static const auto CVarMobileMSAA = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileMSAA"));
	const EShaderPlatform ShaderPlatform = Context.GetShaderPlatform();

	if ((GSupportsShaderFramebufferFetch && (ShaderPlatform == SP_METAL || IsVulkanMobilePlatform(ShaderPlatform))) && (CVarMobileMSAA ? CVarMobileMSAA->GetValueOnAnyThread() > 1 : false))
	{
		UseFetchSunDof = 8;
	}

	switch(UseFetchSunDof)
	{
		case 0: SunMask_SetShader<0, bUseDepthTexture>(Context); break;
		case 1: SunMask_SetShader<1, bUseDepthTexture>(Context); break;
		case 2: SunMask_SetShader<2, bUseDepthTexture>(Context); break;
		case 3: SunMask_SetShader<3, bUseDepthTexture>(Context); break;
		case 4: SunMask_SetShader<4, bUseDepthTexture>(Context); break;
		case 5: SunMask_SetShader<5, bUseDepthTexture>(Context); break;
		case 6: SunMask_SetShader<6, bUseDepthTexture>(Context); break;
		case 7: SunMask_SetShader<7, bUseDepthTexture>(Context); break;
		case 8: SunMask_SetShader<8, bUseDepthTexture>(Context); break;
	}
}

void FRCPassPostProcessSunMaskES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessSunMask);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	FIntPoint DstSize = PrePostSourceViewportSize;

	FIntPoint SrcSize;
	FIntRect SrcRect;
	const FSceneView& View = Context.View;

	TShaderMapRef<FPostProcessSunMaskVS_ES2> VertexShader(Context.GetShaderMap());
	if(bOnChip)
	{
		SrcSize = DstSize;
		//SrcRect = DstRect;
		SrcRect = View.ViewRect;

		Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

		if (InputDesc != NULL && InputDesc->Format == PF_FloatR11G11B10)
		{
			SetShader<true>(Context);
		}
		else
		{
			SetShader<false>(Context);
		}

		DrawRectangle(
			Context.RHICmdList,
			0, 0,
			DstX, DstY,
			SrcRect.Min.X, SrcRect.Min.Y,
			SrcRect.Width(), SrcRect.Height(),
			DstSize,
			SrcSize,
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}
	else
	{
		SrcSize = InputDesc->Extent; //-V595
		//	uint32 ScaleFactor = View.ViewRect.Width() / SrcSize.X;
		//	SrcRect = View.ViewRect / ScaleFactor;
		// TODO: This won't work with scaled views.
		SrcRect = View.ViewRect;

		const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
		
		// OverrideRenderTarget might patch out final render target and we have no control of the clear color anymore
		if (DestRenderTarget.TargetableTexture->GetClearColor() == FLinearColor::Black)
		{
			FRHIRenderTargetView RtView = FRHIRenderTargetView(DestRenderTarget.TargetableTexture, ERenderTargetLoadAction::EClear);
			FRHISetRenderTargetsInfo Info(1, &RtView, FRHIDepthRenderTargetView());
			Context.RHICmdList.SetRenderTargetsAndClear(Info);
		}
		else
		{
			SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
			DrawClearQuad(Context.RHICmdList, FLinearColor::Black);
		}

		Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

		if (InputDesc != NULL && InputDesc->Format == PF_FloatR11G11B10)
		{
			SetShader<true>(Context);
		}
		else
		{
			SetShader<false>(Context);
		}

		DrawRectangle(
			Context.RHICmdList,
			SrcRect.Min.X, SrcRect.Min.Y,
			SrcRect.Width(), SrcRect.Height(),
			SrcRect.Min.X, SrcRect.Min.Y,
			SrcRect.Width(), SrcRect.Height(),
			DstSize,
			SrcSize,
			*VertexShader,
			EDRF_UseTriangleOptimization);

		Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
	}
}

FPooledRenderTargetDesc FRCPassPostProcessSunMaskES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = GetHDRPixelFormat();
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y);
	Ret.DebugName = TEXT("SunMask");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	return Ret;
}




//
// SUN ALPHA
//

template<uint32 UseDof>
class FPostProcessSunAlphaPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunAlphaPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ES2_USE_DOF"), UseDof ? (uint32)1 : (uint32)0);
	}

	FPostProcessSunAlphaPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessSunAlphaPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

typedef FPostProcessSunAlphaPS_ES2<0> FPostProcessSunAlphaPS_ES2_0;
typedef FPostProcessSunAlphaPS_ES2<1> FPostProcessSunAlphaPS_ES2_1;
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunAlphaPS_ES2_0,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunAlphaPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunAlphaPS_ES2_1,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunAlphaPS_ES2"),SF_Pixel);

class FPostProcessSunAlphaVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunAlphaVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessSunAlphaVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter LightShaftCenter;

	FPostProcessSunAlphaVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		LightShaftCenter.Bind(Initializer.ParameterMap, TEXT("LightShaftCenter"));
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		SetShaderValue(Context.RHICmdList, ShaderRHI, LightShaftCenter, Context.View.LightShaftCenter);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << LightShaftCenter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunAlphaVS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunAlphaVS_ES2"),SF_Vertex);

template <uint32 UseDof>
static void SunAlpha_SetShader(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessSunAlphaVS_ES2> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessSunAlphaPS_ES2<UseDof> > PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessSunAlphaES2::SetShader(const FRenderingCompositePassContext& Context)
{
	if(GetMobileDepthOfFieldScale(Context.View))
	{
		SunAlpha_SetShader<1>(Context);
	}
	else
	{
		SunAlpha_SetShader<0>(Context);
	}
}

void FRCPassPostProcessSunAlphaES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessSunAlpha);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/4);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/4);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	
	// OverrideRenderTarget might patch out final render target and we have no control of the clear color anymore
	if (DestRenderTarget.TargetableTexture->GetClearColor() == FLinearColor::Black)
	{
		FRHIRenderTargetView View = FRHIRenderTargetView(DestRenderTarget.TargetableTexture, ERenderTargetLoadAction::EClear);
		FRHISetRenderTargetsInfo Info(1, &View, FRHIDepthRenderTargetView());
		Context.RHICmdList.SetRenderTargetsAndClear(Info);
	}
	else
	{
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		DrawClearQuad(Context.RHICmdList, FLinearColor::Black);
	}

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	SetShader(Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize / 4;
	TShaderMapRef<FPostProcessSunAlphaVS_ES2> VertexShader(Context.GetShaderMap());

	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessSunAlphaES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	// Only need one 8-bit channel as output (but mobile hardware often doesn't support that as a render target format).
	// Highlight compression (tonemapping) was used to keep this in 8-bit.
	Ret.Format = PF_B8G8R8A8;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/4);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/4);
	Ret.DebugName = TEXT("SunAlpha");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	return Ret;
}





//
// SUN BLUR
//

class FPostProcessSunBlurPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunBlurPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessSunBlurPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessSunBlurPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunBlurPS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunBlurPS_ES2"),SF_Pixel);


class FPostProcessSunBlurVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunBlurVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessSunBlurVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter LightShaftCenter;

	FPostProcessSunBlurVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		LightShaftCenter.Bind(Initializer.ParameterMap, TEXT("LightShaftCenter"));
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		SetShaderValue(Context.RHICmdList, ShaderRHI, LightShaftCenter, Context.View.LightShaftCenter);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << LightShaftCenter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunBlurVS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunBlurVS_ES2"),SF_Vertex);


void FRCPassPostProcessSunBlurES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessSunBlur);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/4);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/4);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	
	// OverrideRenderTarget might patch out final render target and we have no control of the clear color anymore
	if (DestRenderTarget.TargetableTexture->GetClearColor() == FLinearColor::Black)
	{
		FRHIRenderTargetView View = FRHIRenderTargetView(DestRenderTarget.TargetableTexture, ERenderTargetLoadAction::EClear);
		FRHISetRenderTargetsInfo Info(1, &View, FRHIDepthRenderTargetView());
		Context.RHICmdList.SetRenderTargetsAndClear(Info);
	}
	else
	{
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		DrawClearQuad(Context.RHICmdList, FLinearColor::Black);
	}

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessSunBlurVS_ES2> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessSunBlurPS_ES2> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize / 4;

	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessSunBlurES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	// Only need one 8-bit channel as output (but mobile hardware often doesn't support that as a render target format).
	// Highlight compression (tonemapping) was used to keep this in 8-bit.
	Ret.Format = PF_B8G8R8A8;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/4);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/4);
	Ret.DebugName = TEXT("SunBlur");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	return Ret;
}




//
// SUN MERGE
//

template <uint32 UseSunBloom>
class FPostProcessSunMergePS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunMergePS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		CA_SUPPRESS(6313);
		OutEnvironment.SetDefine(TEXT("ES2_USE_BLOOM"), (UseSunBloom & 1) ? (uint32)1 : (uint32)0);
		OutEnvironment.SetDefine(TEXT("ES2_USE_SUN"), (UseSunBloom >> 1) ? (uint32)1 : (uint32)0);
	}

	FPostProcessSunMergePS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter SunColorVignetteIntensity;
	FShaderParameter VignetteColor;
	FShaderParameter BloomColor;

	FPostProcessSunMergePS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		SunColorVignetteIntensity.Bind(Initializer.ParameterMap, TEXT("SunColorVignetteIntensity"));
		VignetteColor.Bind(Initializer.ParameterMap, TEXT("VignetteColor"));
		BloomColor.Bind(Initializer.ParameterMap, TEXT("BloomColor"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		FVector4 SunColorVignetteIntensityParam;
		SunColorVignetteIntensityParam.X = Context.View.LightShaftColorApply.R;
		SunColorVignetteIntensityParam.Y = Context.View.LightShaftColorApply.G;
		SunColorVignetteIntensityParam.Z = Context.View.LightShaftColorApply.B;
		SunColorVignetteIntensityParam.W = Settings.VignetteIntensity;
		SetShaderValue(Context.RHICmdList, ShaderRHI, SunColorVignetteIntensity, SunColorVignetteIntensityParam);

		// Scaling Bloom1 by extra factor to match filter area difference between PC default and mobile.
		SetShaderValue(Context.RHICmdList, ShaderRHI, BloomColor, Context.View.FinalPostProcessSettings.Bloom1Tint * Context.View.FinalPostProcessSettings.BloomIntensity * 0.5);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << SunColorVignetteIntensity << BloomColor;
		return bShaderHasOutdatedParameters;
	}
};

typedef FPostProcessSunMergePS_ES2<0> FPostProcessSunMergePS_ES2_0;
typedef FPostProcessSunMergePS_ES2<1> FPostProcessSunMergePS_ES2_1;
typedef FPostProcessSunMergePS_ES2<2> FPostProcessSunMergePS_ES2_2;
typedef FPostProcessSunMergePS_ES2<3> FPostProcessSunMergePS_ES2_3;
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMergePS_ES2_0,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunMergePS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMergePS_ES2_1,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunMergePS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMergePS_ES2_2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunMergePS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMergePS_ES2_3,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunMergePS_ES2"),SF_Pixel);


class FPostProcessSunMergeVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunMergeVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessSunMergeVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter LightShaftCenter;

	FPostProcessSunMergeVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		LightShaftCenter.Bind(Initializer.ParameterMap, TEXT("LightShaftCenter"));
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		SetShaderValue(Context.RHICmdList, ShaderRHI, LightShaftCenter, Context.View.LightShaftCenter);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << LightShaftCenter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunMergeVS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunMergeVS_ES2"),SF_Vertex);



template <uint32 UseSunBloom>
FShader* SunMerge_SetShader(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessSunMergeVS_ES2> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessSunMergePS_ES2<UseSunBloom> > PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);

	return *VertexShader;
}

FShader* FRCPassPostProcessSunMergeES2::SetShader(const FRenderingCompositePassContext& Context)
{
	const FSceneView& View = Context.View;
	uint32 UseBloom = (View.FinalPostProcessSettings.BloomIntensity > 0.0f) ? 1 : 0;
	uint32 UseSun = Context.View.bLightShaftUse ? 1 : 0;
	uint32 UseSunBloom = UseBloom + (UseSun<<1);

	switch(UseSunBloom)
	{
		case 0: return SunMerge_SetShader<0>(Context);
		case 1: return SunMerge_SetShader<1>(Context);
		case 2: return SunMerge_SetShader<2>(Context);
		case 3: return SunMerge_SetShader<3>(Context);
	}

	check(false);
	return NULL;
}

void FRCPassPostProcessSunMergeES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessSunMerge);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/4);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/4);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	
	// OverrideRenderTarget might patch out final render target and we have no control of the clear color anymore
	if (DestRenderTarget.TargetableTexture->GetClearColor() == FLinearColor::Black)
	{
		FRHIRenderTargetView View = FRHIRenderTargetView(DestRenderTarget.TargetableTexture, ERenderTargetLoadAction::EClear);
		FRHISetRenderTargetsInfo Info(1, &View, FRHIDepthRenderTargetView());
		Context.RHICmdList.SetRenderTargetsAndClear(Info);
	}
	else
	{
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		DrawClearQuad(Context.RHICmdList, FLinearColor::Black);
	}

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	FShader* VertexShader = SetShader(Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize / 4;

	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	// Double buffer sun+bloom+vignette composite.
	if(Context.View.AntiAliasingMethod == AAM_TemporalAA)
	{
		FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;
		if(ViewState) 
		{
			ViewState->MobileAaBloomSunVignette0 = PassOutputs[0].PooledRenderTarget;
		}
	}
}

FPooledRenderTargetDesc FRCPassPostProcessSunMergeES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	// This might not have a valid input texture.
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = GetHDRPixelFormat();
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/4);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/4);
	Ret.DebugName = TEXT("SunMerge");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	return Ret;
}





//
// SUN MERGE SMALL (BLOOM)
//

class FPostProcessSunMergeSmallPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunMergeSmallPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessSunMergeSmallPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter SunColorVignetteIntensity;
	FShaderParameter VignetteColor;
	FShaderParameter BloomColor;
	FShaderParameter BloomColor2;

	FPostProcessSunMergeSmallPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		SunColorVignetteIntensity.Bind(Initializer.ParameterMap, TEXT("SunColorVignetteIntensity"));
		VignetteColor.Bind(Initializer.ParameterMap, TEXT("VignetteColor"));
		BloomColor.Bind(Initializer.ParameterMap, TEXT("BloomColor"));
		BloomColor2.Bind(Initializer.ParameterMap, TEXT("BloomColor2"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		FVector4 SunColorVignetteIntensityParam;
		SunColorVignetteIntensityParam.X = Context.View.LightShaftColorApply.R;
		SunColorVignetteIntensityParam.Y = Context.View.LightShaftColorApply.G;
		SunColorVignetteIntensityParam.Z = Context.View.LightShaftColorApply.B;
		SunColorVignetteIntensityParam.W = Settings.VignetteIntensity;
		SetShaderValue(Context.RHICmdList, ShaderRHI, SunColorVignetteIntensity, SunColorVignetteIntensityParam);

		// Scaling Bloom1 by extra factor to match filter area difference between PC default and mobile.
		SetShaderValue(Context.RHICmdList, ShaderRHI, BloomColor, Context.View.FinalPostProcessSettings.Bloom1Tint * Context.View.FinalPostProcessSettings.BloomIntensity * 0.5);
		SetShaderValue(Context.RHICmdList, ShaderRHI, BloomColor2, Context.View.FinalPostProcessSettings.Bloom2Tint * Context.View.FinalPostProcessSettings.BloomIntensity);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << SunColorVignetteIntensity << BloomColor << BloomColor2;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunMergeSmallPS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunMergeSmallPS_ES2"),SF_Pixel);

class FPostProcessSunMergeSmallVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunMergeSmallVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessSunMergeSmallVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessSunMergeSmallVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunMergeSmallVS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunMergeSmallVS_ES2"),SF_Vertex);

void FRCPassPostProcessSunMergeSmallES2::SetShader(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessSunMergeSmallVS_ES2> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessSunMergeSmallPS_ES2> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessSunMergeSmallES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessSunMergeSmall);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/4);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/4);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	
	// OverrideRenderTarget might patch out final render target and we have no control of the clear color anymore
	if (DestRenderTarget.TargetableTexture->GetClearColor() == FLinearColor::Black)
	{
		FRHIRenderTargetView View = FRHIRenderTargetView(DestRenderTarget.TargetableTexture, ERenderTargetLoadAction::EClear);
		FRHISetRenderTargetsInfo Info(1, &View, FRHIDepthRenderTargetView());
		Context.RHICmdList.SetRenderTargetsAndClear(Info);
	}
	else
	{
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		DrawClearQuad(Context.RHICmdList, FLinearColor::Black);
	}

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	SetShader(Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize / 4;
	TShaderMapRef<FPostProcessSunMergeSmallVS_ES2> VertexShader(Context.GetShaderMap());

	DrawRectangle(
		Context.RHICmdList, 
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	// Double buffer sun+bloom+vignette composite.

	if (Context.View.AntiAliasingMethod == AAM_TemporalAA)
	{
		FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;
		if(ViewState) 
		{
			ViewState->MobileAaBloomSunVignette0 = PassOutputs[0].PooledRenderTarget;
		}
	}
}

FPooledRenderTargetDesc FRCPassPostProcessSunMergeSmallES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	// This might not have a valid input texture.
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = GetHDRPixelFormat();
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/4);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/4);
	Ret.DebugName = TEXT("SunMergeSmall");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	return Ret;
}









//
// DOF DOWNSAMPLE
//

class FPostProcessDofDownVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDofDownVS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	/** Default constructor. */
	FPostProcessDofDownVS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	/** Initialization constructor. */
	FPostProcessDofDownVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

template<uint32 UseSun>
class FPostProcessDofDownPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDofDownPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ES2_USE_SUN"), UseSun ? (uint32)1 : (uint32)0);
	}

	FPostProcessDofDownPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessDofDownPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessDofDownVS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("DofDownVS_ES2"),SF_Vertex);

typedef FPostProcessDofDownPS_ES2<0> FPostProcessDofDownPS_ES2_0;
typedef FPostProcessDofDownPS_ES2<1> FPostProcessDofDownPS_ES2_1;
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessDofDownPS_ES2_0,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("DofDownPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessDofDownPS_ES2_1,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("DofDownPS_ES2"),SF_Pixel);

template <uint32 UseSun>
static void DofDown_SetShader(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessDofDownVS_ES2> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessDofDownPS_ES2<UseSun> > PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessDofDownES2::SetShader(const FRenderingCompositePassContext& Context)
{
	const FSceneView& View = Context.View;
	if(Context.View.bLightShaftUse)
	{
		DofDown_SetShader<1>(Context);
	}
	else
	{
		DofDown_SetShader<0>(Context);
	}
}

void FRCPassPostProcessDofDownES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessDofDown);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	FIntPoint PrePostSourceViewportSize = PrePostSourceViewportRect.Size();
	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/2);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/2);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	FIntPoint DstSize = PrePostSourceViewportSize / 2;

	FIntPoint SrcSize;
	FIntRect SrcRect;
	if(bUseViewRectSource)
	{
		// Mobile with framebuffer fetch uses view rect as source.
		const FSceneView& View = Context.View;
		SrcSize = InputDesc->Extent;
		//	uint32 ScaleFactor = View.ViewRect.Width() / SrcSize.X;
		//	SrcRect = View.ViewRect / ScaleFactor;
		// TODO: This won't work with scaled views.
		SrcRect = PrePostSourceViewportRect;
	}
	else
	{
		// Otherwise using exact size texture.
		SrcSize = DstSize;
		SrcRect = DstRect;
	}

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// OverrideRenderTarget might patch out final render target and we have no control of the clear color anymore
	if (DestRenderTarget.TargetableTexture->GetClearColor() == FLinearColor::Black)
	{
		FRHIRenderTargetView View = FRHIRenderTargetView(DestRenderTarget.TargetableTexture, ERenderTargetLoadAction::EClear);
		FRHISetRenderTargetsInfo Info(1, &View, FRHIDepthRenderTargetView());
		Context.RHICmdList.SetRenderTargetsAndClear(Info);
	}
	else
	{
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		DrawClearQuad(Context.RHICmdList, FLinearColor::Black);
	}

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	SetShader(Context);

	TShaderMapRef<FPostProcessDofDownVS_ES2> VertexShader(Context.GetShaderMap());

	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DstX, DstY,
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DstSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessDofDownES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = GetHDRPixelFormat();
	Ret.NumSamples = 1;
	FIntPoint PrePostSourceViewportSize = PrePostSourceViewportRect.Size();
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/2);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/2);
	Ret.DebugName = TEXT("DofDown");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	return Ret;
}




//
// DOF NEAR
//

class FPostProcessDofNearVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDofNearVS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	/** Default constructor. */
	FPostProcessDofNearVS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	/** Initialization constructor. */
	FPostProcessDofNearVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

template<uint32 UseSun>
class FPostProcessDofNearPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDofNearPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ES2_USE_SUN"), UseSun ? (uint32)1 : (uint32)0);
	}

	FPostProcessDofNearPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessDofNearPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessDofNearVS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("DofNearVS_ES2"),SF_Vertex);

typedef FPostProcessDofNearPS_ES2<0> FPostProcessDofNearPS_ES2_0;
typedef FPostProcessDofNearPS_ES2<1> FPostProcessDofNearPS_ES2_1;
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessDofNearPS_ES2_0,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("DofNearPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessDofNearPS_ES2_1,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("DofNearPS_ES2"),SF_Pixel);

template <uint32 UseSun>
static void DofNear_SetShader(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessDofNearVS_ES2> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessDofNearPS_ES2<UseSun> > PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessDofNearES2::SetShader(const FRenderingCompositePassContext& Context)
{
	const FSceneView& View = Context.View;
	if(Context.View.bLightShaftUse)
	{
		DofNear_SetShader<1>(Context);
	}
	else
	{
		DofNear_SetShader<0>(Context);
	}
}

void FRCPassPostProcessDofNearES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessDofNear);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	FIntPoint SrcSize = InputDesc->Extent;

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/4);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/4);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// OverrideRenderTarget might patch out final render target and we have no control of the clear color anymore
	if (DestRenderTarget.TargetableTexture->GetClearColor() == FLinearColor::Black)
	{
		FRHIRenderTargetView View = FRHIRenderTargetView(DestRenderTarget.TargetableTexture, ERenderTargetLoadAction::EClear);
		FRHISetRenderTargetsInfo Info(1, &View, FRHIDepthRenderTargetView());
		Context.RHICmdList.SetRenderTargetsAndClear(Info);
	}
	else
	{
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		DrawClearQuad(Context.RHICmdList, FLinearColor::Black);
	}

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	SetShader(Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize / 4;
	TShaderMapRef<FPostProcessDofNearVS_ES2> VertexShader(Context.GetShaderMap());

	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessDofNearES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	// Only need one 8-bit channel as output (but mobile hardware often doesn't support that as a render target format).
	Ret.Format = PF_B8G8R8A8;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/4);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/4);
	Ret.DebugName = TEXT("DofNear");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	return Ret;
}



//
// DOF BLUR
//

class FPostProcessDofBlurPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDofBlurPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessDofBlurPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessDofBlurPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessDofBlurPS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("DofBlurPS_ES2"),SF_Pixel);


class FPostProcessDofBlurVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDofBlurVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessDofBlurVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessDofBlurVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessDofBlurVS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("DofBlurVS_ES2"),SF_Vertex);


void FRCPassPostProcessDofBlurES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessDofBlur);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/2);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/2);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	
	// OverrideRenderTarget might patch out final render target and we have no control of the clear color anymore
	if (DestRenderTarget.TargetableTexture->GetClearColor() == FLinearColor::Black)
	{
		FRHIRenderTargetView View = FRHIRenderTargetView(DestRenderTarget.TargetableTexture, ERenderTargetLoadAction::EClear);
		FRHISetRenderTargetsInfo Info(1, &View, FRHIDepthRenderTargetView());
		Context.RHICmdList.SetRenderTargetsAndClear(Info);
	}
	else
	{
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		DrawClearQuad(Context.RHICmdList, FLinearColor::Black);
	}

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessDofBlurVS_ES2> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessDofBlurPS_ES2> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize / 2;

	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessDofBlurES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = GetHDRPixelFormat();
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/2);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/2);
	Ret.DebugName = TEXT("DofBlur");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	return Ret;
}






//
// SUN AVG
//

class FPostProcessSunAvgPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunAvgPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessSunAvgPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessSunAvgPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunAvgPS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunAvgPS_ES2"),SF_Pixel);



class FPostProcessSunAvgVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunAvgVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessSunAvgVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessSunAvgVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunAvgVS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("SunAvgVS_ES2"),SF_Vertex);



static void SunAvg_SetShader(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessSunAvgVS_ES2> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessSunAvgPS_ES2> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessSunAvgES2::SetShader(const FRenderingCompositePassContext& Context)
{
	SunAvg_SetShader(Context);
}

void FRCPassPostProcessSunAvgES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessSunAvg);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/4);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/4);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	
	// OverrideRenderTarget might patch out final render target and we have no control of the clear color anymore
	if (DestRenderTarget.TargetableTexture->GetClearColor() == FLinearColor::Black)
	{
		FRHIRenderTargetView View = FRHIRenderTargetView(DestRenderTarget.TargetableTexture, ERenderTargetLoadAction::EClear);
		FRHISetRenderTargetsInfo Info(1, &View, FRHIDepthRenderTargetView());
		Context.RHICmdList.SetRenderTargetsAndClear(Info);
	}
	else
	{
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		DrawClearQuad(Context.RHICmdList, FLinearColor::Black);
	}

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	SetShader(Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize / 4;
	TShaderMapRef<FPostProcessSunAvgVS_ES2> VertexShader(Context.GetShaderMap());

	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessSunAvgES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = GetHDRPixelFormat();
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/4);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/4);
	Ret.DebugName = TEXT("SunAvg");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	return Ret;
}





//
// MOBILE AA
//

class FPostProcessAaPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessAaPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessAaPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter AaBlendAmount;

	FPostProcessAaPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		AaBlendAmount.Bind(Initializer.ParameterMap, TEXT("AaBlendAmount"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		// Compute the blend factor which decides the trade off between ghosting in motion and flicker when not moving.
		// This works by computing the screen space motion vector of distant point at the center of the screen.
		// This factor will effectively provide an idea of the amount of camera rotation.
		// Higher camera rotation = less blend factor (0.0).
		// Lower or no camera rotation = high blend factor (0.25).
		FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;
		if(ViewState)
		{
			const FViewInfo& View = Context.View;

			FMatrix Proj = View.ViewMatrices.ComputeProjectionNoAAMatrix();
			FMatrix PrevProj = ViewState->PrevViewMatrices.ComputeProjectionNoAAMatrix();

			FMatrix ViewProj = ( Context.View.ViewMatrices.GetViewMatrix() * Proj ).GetTransposed();
			FMatrix PrevViewProj = ( ViewState->PrevViewMatrices.GetViewMatrix() * PrevProj ).GetTransposed();

			double InvViewProj[16];
			Inverse4x4( InvViewProj, (float*)ViewProj.M );

			const float* p = (float*)PrevViewProj.M;

			const double cxx = InvViewProj[ 0]; const double cxy = InvViewProj[ 1]; const double cxz = InvViewProj[ 2]; const double cxw = InvViewProj[ 3];
			const double cyx = InvViewProj[ 4]; const double cyy = InvViewProj[ 5]; const double cyz = InvViewProj[ 6]; const double cyw = InvViewProj[ 7];
			const double czx = InvViewProj[ 8]; const double czy = InvViewProj[ 9]; const double czz = InvViewProj[10]; const double czw = InvViewProj[11];
			const double cwx = InvViewProj[12]; const double cwy = InvViewProj[13]; const double cwz = InvViewProj[14]; const double cww = InvViewProj[15];

			const double pxx = (double)(p[ 0]); const double pxy = (double)(p[ 1]); const double pxz = (double)(p[ 2]); const double pxw = (double)(p[ 3]);
			const double pyx = (double)(p[ 4]); const double pyy = (double)(p[ 5]); const double pyz = (double)(p[ 6]); const double pyw = (double)(p[ 7]);
			const double pwx = (double)(p[12]); const double pwy = (double)(p[13]); const double pwz = (double)(p[14]); const double pww = (double)(p[15]);

			float CameraMotion0W = (float)(2.0*(cww*pww - cwx*pww + cwy*pww + (cxw - cxx + cxy)*pwx + (cyw - cyx + cyy)*pwy + (czw - czx + czy)*pwz));
			float CameraMotion2Z = (float)(cwy*pww + cwy*pxw + cww*(pww + pxw) - cwx*(pww + pxw) + (cxw - cxx + cxy)*(pwx + pxx) + (cyw - cyx + cyy)*(pwy + pxy) + (czw - czx + czy)*(pwz + pxz));
			float CameraMotion4Z = (float)(cwy*pww + cww*(pww - pyw) - cwy*pyw + cwx*((-pww) + pyw) + (cxw - cxx + cxy)*(pwx - pyx) + (cyw - cyx + cyy)*(pwy - pyy) + (czw - czx + czy)*(pwz - pyz));

			// Depth surface 0=far, 1=near.
			// This is simplified to compute camera motion with depth = 0.0 (infinitely far away).
			// Camera motion for pixel (in ScreenPos space).
			float ScaleM = 1.0f / CameraMotion0W;
			// Back projection value (projected screen space).
			float BackX = CameraMotion2Z * ScaleM;
			float BackY = CameraMotion4Z * ScaleM;

			// Start with the distance in screen space.
			float BlendAmount = BackX * BackX + BackY * BackY;
			if(BlendAmount > 0.0f)
			{
				BlendAmount = sqrt(BlendAmount);
			}
			
			// Higher numbers truncate anti-aliasing and ghosting faster.
			float BlendEffect = 8.0f;
			BlendAmount = 0.25f - BlendAmount * BlendEffect;
			if(BlendAmount < 0.0f)
			{
				BlendAmount = 0.0f;
			}

			SetShaderValue(Context.RHICmdList, ShaderRHI, AaBlendAmount, BlendAmount);
		}
		else
		{
			float BlendAmount = 0.0;
			SetShaderValue(Context.RHICmdList, ShaderRHI, AaBlendAmount, BlendAmount);
		}
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << AaBlendAmount;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessAaPS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("AaPS_ES2"),SF_Pixel);



class FPostProcessAaVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessAaVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessAaVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessAaVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessAaVS_ES2,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("AaVS_ES2"),SF_Vertex);



static void Aa_SetShader(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessAaVS_ES2> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessAaPS_ES2> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessAaES2::SetShader(const FRenderingCompositePassContext& Context)
{
	Aa_SetShader(Context);
}

void FRCPassPostProcessAaES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessAa);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	const FPooledRenderTargetDesc& OutputDesc = PassOutputs[0].RenderTargetDesc;

	const FIntPoint& SrcSize = InputDesc->Extent;
	const FIntPoint& DestSize = OutputDesc.Extent;

	FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;
	if (ViewState) 
	{
		// Double buffer input for temporal AA.
		ViewState->MobileAaColor0 = GetInput(ePId_Input0)->GetOutput()->PooledRenderTarget;
	}
	
	check(SrcSize == DestSize);

	if (Context.View.StereoPass != eSSP_RIGHT_EYE)
	{
		// Full clear to avoid restore
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef(), ESimpleRenderTargetMode::EClearColorAndDepth);
	}
	else
	{
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef(), ESimpleRenderTargetMode::EExistingColorAndDepth);
	}

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f);

	SetShader(Context);

	const FIntRect& ViewRect = Context.View.UnscaledViewRect; // Simple upscaling, ES2 post process does not currently have a specific upscaling pass.
	float XPos = ViewRect.Min.X;
	float YPos = ViewRect.Min.Y;
	float Width = ViewRect.Width();
	float Height = ViewRect.Height();

	TShaderMapRef<FPostProcessAaVS_ES2> VertexShader(Context.GetShaderMap());

	DrawRectangle(
		Context.RHICmdList,
		XPos, YPos,
		Width, Height,
		XPos, YPos,
		Width, Height,
		DestSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	if (FSceneRenderer::ShouldCompositeEditorPrimitives(Context.View))
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::WaitForOutstandingTasksOnly);
		// because of the flush it's ok to remove the const, this is not ideal as the flush can cost performance
		FViewInfo& NonConstView = (FViewInfo&)Context.View;

		// Remove jitter (ensures editor prims are stable.)
		NonConstView.ViewMatrices.HackRemoveTemporalAAProjectionJitter();

		NonConstView.InitRHIResources();
	}
}

FPooledRenderTargetDesc FRCPassPostProcessAaES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = PF_B8G8R8A8;
	Ret.NumSamples = 1;
	Ret.DebugName = TEXT("Aa");
	Ret.Extent = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc.Extent;
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	return Ret;
}

