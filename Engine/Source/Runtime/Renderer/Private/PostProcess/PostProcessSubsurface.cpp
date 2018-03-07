// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessSubsurface.cpp: Screenspace subsurface scattering implementation.
=============================================================================*/

#include "PostProcess/PostProcessSubsurface.h"
#include "EngineGlobals.h"
#include "Engine/SubsurfaceProfile.h"
#include "StaticBoundShaderState.h"
#include "CanvasTypes.h"
#include "UnrealEngine.h"
#include "SceneUtils.h"
#include "PostProcess/RenderTargetPool.h"
#include "RenderTargetTemp.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/SceneFilterRendering.h"
#include "SceneRenderTargetParameters.h"
#include "PostProcess/PostProcessing.h"
#include "CompositionLighting/PostProcessPassThrough.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"

ENGINE_API const IPooledRenderTarget* GetSubsufaceProfileTexture_RT(FRHICommandListImmediate& RHICmdList);


static TAutoConsoleVariable<int32> CVarSSSQuality(
	TEXT("r.SSS.Quality"),
	0,
	TEXT("Defines the quality of the recombine pass when using the SubsurfaceScatteringProfile shading model\n")
	TEXT(" 0: low (faster, default)\n")
	TEXT(" 1: high (sharper details but slower)\n")
	TEXT("-1: auto, 1 if TemporalAA is disabled (without TemporalAA the quality is more noticable)"),
	ECVF_RenderThreadSafe  | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarSSSFilter(
	TEXT("r.SSS.Filter"),
	1,
	TEXT("Defines the filter method for Screenspace Subsurface Scattering feature.\n")
	TEXT(" 0: point filter (useful for testing, could be cleaner)\n")
	TEXT(" 1: bilinear filter"),
	ECVF_RenderThreadSafe  | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarSSSSampleSet(
	TEXT("r.SSS.SampleSet"),
	2,
	TEXT("Defines how many samples we use for Screenspace Subsurface Scattering feature.\n")
	TEXT(" 0: lowest quality (6*2+1)\n")
	TEXT(" 1: medium quality (9*2+1)\n")
	TEXT(" 2: high quality (13*2+1) (default)"),
	ECVF_RenderThreadSafe  | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarCheckerboardSubsurfaceProfileRendering(
	TEXT("r.SSS.Checkerboard"),
	2,
	TEXT("Enables or disables checkerboard rendering for subsurface profile rendering.\n")
	TEXT("This is necessary if SceneColor does not include a floating point alpha channel (e.g 32-bit formats)\n")
	TEXT(" 0: Disabled (high quality) \n")
	TEXT(" 1: Enabled (low quality). Surface lighting will be at reduced resolution.\n")
	TEXT(" 2: Automatic. Non-checkerboard lighting will be applied if we have a suitable rendertarget format\n"),
	ECVF_RenderThreadSafe
	);

// -------------------------------------------------------------

float GetSubsurfaceRadiusScale()
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.SSS.Scale"));
	check(CVar);
	float Ret = CVar->GetValueOnRenderThread();

	return FMath::Max(0.0f, Ret);
}

// -------------------------------------------------------------

const bool FRCPassPostProcessSubsurface::RequiresCheckerboardSubsurfaceRendering(EPixelFormat SceneColorFormat)
{
	int CVarValue = CVarCheckerboardSubsurfaceProfileRendering.GetValueOnRenderThread();
	if (CVarValue == 0)
	{
		return false;
	}
	else if ( CVarValue == 1 )
	{
		return true;
	}
	else if (CVarValue == 2)
	{
		switch (SceneColorFormat)
		{
		case PF_A32B32G32R32F:
		case PF_FloatRGBA:
			return false;
		default:
			return true;
		}
	}
	return true;
}

// -------------------------------------------------------------

/** Shared shader parameters needed for screen space subsurface scattering. */
class FSubsurfaceParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		SSSParams.Bind(ParameterMap, TEXT("SSSParams"));
		SSProfilesTexture.Bind(ParameterMap, TEXT("SSProfilesTexture"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FPixelShaderRHIParamRef& ShaderRHI, const FRenderingCompositePassContext& Context) const
	{
		{
			// from Separabale.usf: float distanceToProjectionWindow = 1.0 / tan(0.5 * radians(SSSS_FOVY))
			// can be extracted out of projection matrix

			FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

			// Calculate the sssWidth scale (1.0 for a unit plane sitting on the projection window):
			float DistanceToProjectionWindow = Context.View.ViewMatrices.GetProjectionMatrix().M[0][0]; 

			float SSSScaleZ = DistanceToProjectionWindow * GetSubsurfaceRadiusScale();

			// * 0.5f: hacked in 0.5 - -1..1 to 0..1 but why this isn't in demo code?
			float SSSScaleX = SSSScaleZ / SUBSURFACE_KERNEL_SIZE * 0.5f;

			FVector4 ColorScale(SSSScaleX, SSSScaleZ, 0, 0);
			SetShaderValue(Context.RHICmdList, ShaderRHI, SSSParams, ColorScale);
		}

		{
			const IPooledRenderTarget* PooledRT = GetSubsufaceProfileTexture_RT(Context.RHICmdList);

			if(!PooledRT)
			{
				// no subsurface profile was used yet
				PooledRT = GSystemTextures.BlackDummy;
			}

			const FSceneRenderTargetItem& Item = PooledRT->GetRenderTargetItem();

			SetTextureParameter(Context.RHICmdList, ShaderRHI, SSProfilesTexture, Item.ShaderResourceTexture);
		}
	}

	friend FArchive& operator<<(FArchive& Ar,FSubsurfaceParameters& P)
	{
		Ar << P.SSSParams << P.SSProfilesTexture;
		return Ar;
	}

private:
	FShaderParameter SSSParams;
	FShaderResourceParameter SSProfilesTexture;
};

// ---------------------------------------------


/**
 * Encapsulates the post processing subsurface scattering pixel shader.
 */
class FPostProcessSubsurfaceVisualizePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSubsurfaceVisualizePS , Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SUBSURFACE_RADIUS_SCALE"), SUBSURFACE_RADIUS_SCALE);
		OutEnvironment.SetDefine(TEXT("SUBSURFACE_KERNEL_SIZE"), SUBSURFACE_KERNEL_SIZE);
	}

	/** Default constructor. */
	FPostProcessSubsurfaceVisualizePS () {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter MiniFontTexture;
	FSubsurfaceParameters SubsurfaceParameters;

	/** Initialization constructor. */
	FPostProcessSubsurfaceVisualizePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		MiniFontTexture.Bind(Initializer.ParameterMap, TEXT("MiniFontTexture"));
		SubsurfaceParameters.Bind(Initializer.ParameterMap);
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
	{
		const FFinalPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		DeferredParameters.Set(RHICmdList, ShaderRHI, Context.View, MD_PostProcess);
		SetTextureParameter(RHICmdList, ShaderRHI, MiniFontTexture, GEngine->MiniFontTexture ? GEngine->MiniFontTexture->Resource->TextureRHI : GSystemTextures.WhiteDummy->GetRenderTargetItem().TargetableTexture);
		SubsurfaceParameters.SetParameters(RHICmdList, ShaderRHI, Context);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << MiniFontTexture << SubsurfaceParameters;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/PostProcessSubsurface.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("VisualizePS");
	}
};

IMPLEMENT_SHADER_TYPE3(FPostProcessSubsurfaceVisualizePS, SF_Pixel);

void SetSubsurfaceVisualizeShader(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessSubsurfaceVisualizePS> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	PixelShader->SetParameters(Context.RHICmdList, Context);
	VertexShader->SetParameters(Context);
}

FRCPassPostProcessSubsurfaceVisualize::FRCPassPostProcessSubsurfaceVisualize(FRHICommandList& RHICmdList)
{
	// we need the GBuffer, we release it Process()
	FSceneRenderTargets::Get(RHICmdList).AdjustGBufferRefCount(RHICmdList, 1);
}

void FRCPassPostProcessSubsurfaceVisualize::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, SubsurfaceVisualize);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = SceneContext.GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	DrawClearQuad(Context.RHICmdList, true, FLinearColor::Black, false, 0, false, 0, PassOutputs[0].RenderTargetDesc.Extent, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	SetSubsurfaceVisualizeShader(Context);

	// Draw a quad mapping scene color to the view's render target
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	DrawRectangle(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	{
		FRenderTargetTemp TempRenderTarget(View, (const FTexture2DRHIRef&)DestRenderTarget.TargetableTexture);
		FCanvas Canvas(&TempRenderTarget, NULL, ViewFamily.CurrentRealTime, ViewFamily.CurrentWorldTime, ViewFamily.DeltaWorldTime, Context.GetFeatureLevel());

		float X = 30;
		float Y = 28;
		const float YStep = 14;

		FString Line;

		Line = FString::Printf(TEXT("Visualize Screen Space Subsurface Scattering"));
		Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));

		Y += YStep;

		uint32 Index = 0;
		while (GSubsurfaceProfileTextureObject.GetEntryString(Index++, Line))
		{
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		}

		Canvas.Flush_RenderThread(Context.RHICmdList);
	}

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	// we no longer need the GBuffer
	SceneContext.AdjustGBufferRefCount(Context.RHICmdList, -1);
}

FPooledRenderTargetDesc FRCPassPostProcessSubsurfaceVisualize::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = FSceneRenderTargets::Get_FrameConstantsOnly().GetSceneColor()->GetDesc();
	Ret.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	Ret.Reset();
	Ret.DebugName = TEXT("SubsurfaceVisualize");
	// alpha is used to store depth and renormalize (alpha==0 means there is no subsurface scattering)
	Ret.Format = PF_FloatRGBA;

	return Ret;
}


// ---------------------------------------------

/**
 * Encapsulates the post processing subsurface scattering pixel shader.
 * @param HalfRes 0:to full res, 1:to half res
 */
template <uint32 HalfRes, uint32 Checkerboard>
class FPostProcessSubsurfaceSetupPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSubsurfaceSetupPS , Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("HALF_RES"), HalfRes);
		OutEnvironment.SetDefine(TEXT("SUBSURFACE_RADIUS_SCALE"), SUBSURFACE_RADIUS_SCALE);
		OutEnvironment.SetDefine(TEXT("SUBSURFACE_KERNEL_SIZE"), SUBSURFACE_KERNEL_SIZE);
		OutEnvironment.SetDefine(TEXT("SUBSURFACE_PROFILE_CHECKERBOARD"), Checkerboard);
	}

	/** Default constructor. */
	FPostProcessSubsurfaceSetupPS () {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter MiniFontTexture;
	FSubsurfaceParameters SubsurfaceParameters;

	/** Initialization constructor. */
	FPostProcessSubsurfaceSetupPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		SubsurfaceParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FFinalPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View, MD_PostProcess);
		SubsurfaceParameters.SetParameters(Context.RHICmdList, ShaderRHI, Context);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << SubsurfaceParameters;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/PostProcessSubsurface.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("SetupPS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A)	VARIATION2(A,0)	VARIATION2(A,1)
#define VARIATION2(A,B) typedef FPostProcessSubsurfaceSetupPS<A,B> FPostProcessSubsurfaceSetupPS##A##B; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessSubsurfaceSetupPS##A##B, SF_Pixel);
	VARIATION1(0) VARIATION1(1)
#undef VARIATION1
#undef VARIATION2


template <uint32 HalfRes, uint32 Checkerboard>
void SetSubsurfaceSetupShader(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessSubsurfaceSetupPS<HalfRes, Checkerboard> > PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	PixelShader->SetParameters(Context);
	VertexShader->SetParameters(Context);
}

// --------------------------------------

FRCPassPostProcessSubsurfaceSetup::FRCPassPostProcessSubsurfaceSetup(FViewInfo& View, bool bInHalfRes)
	: ViewRect(View.ViewRect)
	, bHalfRes(bInHalfRes)
{
}

void FRCPassPostProcessSubsurfaceSetup::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, SubsurfaceSetup);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
	const bool bCheckerboard = FRCPassPostProcessSubsurface::RequiresCheckerboardSubsurfaceRendering( SceneContext.GetSceneColorFormat() );
	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	FIntRect DestRect = FIntRect(0, 0, DestSize.X, DestSize.Y);
	FIntRect SrcRect = View.ViewRect;
	
	if(bHalfRes)
	{
		// upscale rectangle to not slightly scale (might miss a pixel)
		SrcRect = DestRect * 2 + View.ViewRect.Min;
	}

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	if(bHalfRes)
	{
		if (bCheckerboard)
		{
			SetSubsurfaceSetupShader<1, 1>(Context);
		}
		else
		{
			SetSubsurfaceSetupShader<1,0>(Context);
		}
	}
	else
	{
		if (bCheckerboard)
		{
			SetSubsurfaceSetupShader<0, 1>(Context);
		}
		else
		{
			SetSubsurfaceSetupShader<0, 0>(Context);
		}
	}

	// Draw a quad mapping scene color to the view's render target
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

	DrawPostProcessPass(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestSize,
		SrcSize,
		*VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessSubsurfaceSetup::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = FSceneRenderTargets::Get_FrameConstantsOnly().GetSceneColor()->GetDesc();
	Ret.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	Ret.Reset();
	Ret.DebugName = TEXT("SubsurfaceSetup");
	// alpha is used to store depth and renormalize (alpha==0 means there is no subsurface scattering)
	Ret.Format = PF_FloatRGBA;

	Ret.Extent = ViewRect.Size();

	if(bHalfRes)
	{
		Ret.Extent = FIntPoint::DivideAndRoundUp(Ret.Extent, 2);
		Ret.Extent.X = FMath::Max(1, Ret.Extent.X);
		Ret.Extent.Y = FMath::Max(1, Ret.Extent.Y);
	}
	
	return Ret;
}


/** Encapsulates the post processing subsurface pixel shader. */
// @param Direction 0: horizontal, 1:vertical
// @param SampleSet 0:low, 1:med, 2:high
template <uint32 Direction, uint32 SampleSet>
class TPostProcessSubsurfacePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TPostProcessSubsurfacePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SSS_DIRECTION"), Direction);
		OutEnvironment.SetDefine(TEXT("SSS_SAMPLESET"), SampleSet);
		OutEnvironment.SetDefine(TEXT("SUBSURFACE_RADIUS_SCALE"), SUBSURFACE_RADIUS_SCALE);
		OutEnvironment.SetDefine(TEXT("SUBSURFACE_KERNEL_SIZE"), SUBSURFACE_KERNEL_SIZE);
	}

	/** Default constructor. */
	TPostProcessSubsurfacePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FSubsurfaceParameters SubsurfaceParameters;

	/** Initialization constructor. */
	TPostProcessSubsurfacePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		SubsurfaceParameters.Bind(Initializer.ParameterMap);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << SubsurfaceParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View, MD_PostProcess);

		if(CVarSSSFilter.GetValueOnRenderThread())
		{
			PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI());
		}
		else
		{
			PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Border,AM_Border,AM_Border>::GetRHI());
		}
		SubsurfaceParameters.SetParameters(Context.RHICmdList, ShaderRHI, Context);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/PostProcessSubsurface.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A)		VARIATION2(A,0)			VARIATION2(A,1)			VARIATION2(A,2)
#define VARIATION2(A, B) typedef TPostProcessSubsurfacePS<A, B> TPostProcessSubsurfacePS##A##B; \
	IMPLEMENT_SHADER_TYPE2(TPostProcessSubsurfacePS##A##B, SF_Pixel);
	VARIATION1(0) VARIATION1(1) VARIATION1(2)
#undef VARIATION1
#undef VARIATION2


FRCPassPostProcessSubsurface::FRCPassPostProcessSubsurface(uint32 InDirection, bool bInHalfRes)
	: Direction(InDirection) 
	, bHalfRes(bInHalfRes)
{
	check(InDirection < 2);
}

template <uint32 Direction, uint32 SampleSet>
void SetSubsurfaceShader(const FRenderingCompositePassContext& Context, TShaderMapRef<FPostProcessVS> &VertexShader)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<TPostProcessSubsurfacePS<Direction, SampleSet> > PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	PixelShader->SetParameters(Context);
	VertexShader->SetParameters(Context);
}

// 0:horizontal, 1: vertical
template <uint32 Direction>
void SetSubsurfaceShaderSampleSet(const FRenderingCompositePassContext& Context, TShaderMapRef<FPostProcessVS> &VertexShader, uint32 SampleSet)
{
	switch(SampleSet)
	{
		case 0: SetSubsurfaceShader<Direction, 0>(Context, VertexShader); break;
		case 1: SetSubsurfaceShader<Direction, 1>(Context, VertexShader); break;
		case 2: SetSubsurfaceShader<Direction, 2>(Context, VertexShader); break;

	default:
		check(0);
	}
}

void FRCPassPostProcessSubsurface::Process(FRenderingCompositePassContext& Context)
{
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	check(InputDesc);

	{
		const IPooledRenderTarget* PooledRT = GetSubsufaceProfileTexture_RT(Context.RHICmdList);

		check(PooledRT);

		// for debugging
		GRenderTargetPool.VisualizeTexture.SetCheckPoint(Context.RHICmdList, PooledRT);
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	check(DestSize.X);
	check(DestSize.Y);
	check(SrcSize.X);
	check(SrcSize.Y);

	FIntRect SrcRect = FIntRect(0, 0, DestSize.X, DestSize.Y);
	FIntRect DestRect = SrcRect;

	TRefCountPtr<IPooledRenderTarget> NewSceneColor;

	const FSceneRenderTargetItem* DestRenderTarget;
	{
		DestRenderTarget = &PassOutputs[0].RequestSurface(Context);

		check(DestRenderTarget);
	}

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget->TargetableTexture, FTextureRHIRef());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

	SCOPED_DRAW_EVENTF(Context.RHICmdList, SubsurfacePass, TEXT("SubsurfaceDirection#%d"), Direction);

	uint32 SampleSet = FMath::Clamp(CVarSSSSampleSet.GetValueOnRenderThread(), 0, 2);

	if (Direction == 0)
	{
		SetSubsurfaceShaderSampleSet<0>(Context, VertexShader, SampleSet);
	}
	else
	{
		SetSubsurfaceShaderSampleSet<1>(Context, VertexShader, SampleSet);
	}

	DrawPostProcessPass(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestSize,
		SrcSize,
		*VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget->TargetableTexture, DestRenderTarget->ShaderResourceTexture, false, FResolveParams());
}


FPooledRenderTargetDesc FRCPassPostProcessSubsurface::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = (Direction == 0) ? TEXT("SubsurfaceX") : TEXT("SubsurfaceY");

	return Ret;
}






/** Encapsulates the post processing subsurface recombine pixel shader. */
// @param RecombineMode 0:fullres, 1: halfres, 2:no scattering, just reconstruct the lighting (needed for scalability)
// @param RecombineQuality 0:low..1:high
template <uint32 RecombineMode, uint32 RecombineQuality, uint32 Checkerboard>
class TPostProcessSubsurfaceRecombinePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TPostProcessSubsurfaceRecombinePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("RECOMBINE_QUALITY"), RecombineQuality);
		OutEnvironment.SetDefine(TEXT("HALF_RES"), (uint32)(RecombineMode == 1));
		OutEnvironment.SetDefine(TEXT("RECOMBINE_SUBSURFACESCATTER"), (uint32)(RecombineMode != 2));
		OutEnvironment.SetDefine(TEXT("SUBSURFACE_RADIUS_SCALE"), SUBSURFACE_RADIUS_SCALE);
		OutEnvironment.SetDefine(TEXT("SUBSURFACE_KERNEL_SIZE"), SUBSURFACE_KERNEL_SIZE);
		OutEnvironment.SetDefine(TEXT("SUBSURFACE_PROFILE_CHECKERBOARD"), Checkerboard);
	}

	/** Default constructor. */
	TPostProcessSubsurfaceRecombinePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FSubsurfaceParameters SubsurfaceParameters;

	/** Initialization constructor. */
	TPostProcessSubsurfaceRecombinePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		SubsurfaceParameters.Bind(Initializer.ParameterMap);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << SubsurfaceParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View, MD_PostProcess);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI());
		SubsurfaceParameters.SetParameters(Context.RHICmdList, ShaderRHI, Context);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/PostProcessSubsurface.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("SubsurfaceRecombinePS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A)		VARIATION2(A,0)			VARIATION2(A,1)		
#define VARIATION2(A, B)	VARIATION3(A,B,0)		VARIATION3(A,B,1)
#define VARIATION3(A, B, C) typedef TPostProcessSubsurfaceRecombinePS<A, B, C> TPostProcessSubsurfaceRecombinePS##A##B##C; \
	IMPLEMENT_SHADER_TYPE2(TPostProcessSubsurfaceRecombinePS##A##B##C, SF_Pixel);
VARIATION1(0) VARIATION1(1) VARIATION1(2)
#undef VARIATION1
#undef VARIATION2
#undef VARIATION3

// @param RecombineMode 0:fullres, 1: halfres, 2:no scattering, just reconstruct the lighting (needed for scalability)
// @param RecombineQuality 0:low..1:high
template <uint32 RecombineMode, uint32 RecombineQuality, uint32 Checkerboard>
void SetSubsurfaceRecombineShader(const FRenderingCompositePassContext& Context, TShaderMapRef<FPostProcessVS> &VertexShader)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<TPostProcessSubsurfaceRecombinePS<RecombineMode, RecombineQuality, Checkerboard> > PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	PixelShader->SetParameters(Context);
	VertexShader->SetParameters(Context);
}

FRCPassPostProcessSubsurfaceRecombine::FRCPassPostProcessSubsurfaceRecombine(bool bInHalfRes, bool bInSingleViewportMode)
	: bHalfRes(bInHalfRes)
	, bSingleViewportMode(bInSingleViewportMode)
{
}

void FRCPassPostProcessSubsurfaceRecombine::Process(FRenderingCompositePassContext& Context)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	check(InputDesc);

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = SceneContext.GetBufferSizeXY();

	check(DestSize.X);
	check(DestSize.Y);
	check(SrcSize.X);
	check(SrcSize.Y);

	FIntRect SrcRect = FIntRect(0, 0, InputDesc->Extent.X, InputDesc->Extent.Y);
	FIntRect DestRect = View.ViewRect;

	TRefCountPtr<IPooledRenderTarget>& SceneColor = SceneContext.GetSceneColor();

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	CopyOverOtherViewportsIfNeeded(Context, View);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

	uint32 QualityCVar = CVarSSSQuality.GetValueOnRenderThread();
	const bool bCheckerboard = FRCPassPostProcessSubsurface::RequiresCheckerboardSubsurfaceRendering( SceneContext.GetSceneColorFormat() );

	// 0:low / 1:high
	uint32 RecombineQuality = 0;
	{
		if(QualityCVar == -1)
		{
			RecombineQuality = (View.AntiAliasingMethod == AAM_TemporalAA) ? 0 : 1;
		}
		else if(QualityCVar == 1)
		{
			RecombineQuality = 1;
		}
	}

	// needed for Scalability
	// 0:fullres, 1: halfres, 2:no scattering, just reconstruct the lighting (needed for scalability)
	uint32 RecombineMode = 2;

	if(GetInput(ePId_Input1)->IsValid())
	{
		RecombineMode = bHalfRes ? 1 : 0;
	}

	SCOPED_DRAW_EVENTF(Context.RHICmdList, SubsurfacePassRecombine, TEXT("SubsurfacePassRecombine Mode:%d Quality:%d"), RecombineMode, RecombineQuality);

	{
		if (bCheckerboard)
		{
			if (RecombineQuality == 0)
			{
				switch (RecombineMode)
				{
				case 0: SetSubsurfaceRecombineShader<0, 0, 1>(Context, VertexShader); break;
				case 1: SetSubsurfaceRecombineShader<1, 0, 1>(Context, VertexShader); break;
				case 2: SetSubsurfaceRecombineShader<2, 0, 1>(Context, VertexShader); break;
				default: check(0);
				}
			}
			else
			{
				switch (RecombineMode)
				{
				case 0: SetSubsurfaceRecombineShader<0, 1, 1>(Context, VertexShader); break;
				case 1: SetSubsurfaceRecombineShader<1, 1, 1>(Context, VertexShader); break;
				case 2: SetSubsurfaceRecombineShader<2, 1, 1>(Context, VertexShader); break;
				default: check(0);
				}
			}
		}
		else
		{
			if (RecombineQuality == 0)
			{
				switch (RecombineMode)
				{
				case 0: SetSubsurfaceRecombineShader<0, 0, 0>(Context, VertexShader); break;
				case 1: SetSubsurfaceRecombineShader<1, 0, 0>(Context, VertexShader); break;
				case 2: SetSubsurfaceRecombineShader<2, 0, 0>(Context, VertexShader); break;
				default: check(0);
				}
			}
			else
			{
				switch (RecombineMode)
				{
				case 0: SetSubsurfaceRecombineShader<0, 1, 0>(Context, VertexShader); break;
				case 1: SetSubsurfaceRecombineShader<1, 1, 0>(Context, VertexShader); break;
				case 2: SetSubsurfaceRecombineShader<2, 1, 0>(Context, VertexShader); break;
				default: check(0);
				}
			}
		}
	}

	DrawPostProcessPass(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestSize,
		SrcSize,
		*VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	// replace the current SceneColor with this one
	SceneContext.SetSceneColor(PassOutputs[0].PooledRenderTarget);
	PassOutputs[0].PooledRenderTarget.SafeRelease();
}


FPooledRenderTargetDesc FRCPassPostProcessSubsurfaceRecombine::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("SceneColorSubsurface");

	// we replace the HDR SceneColor with this one
	return Ret;
}

