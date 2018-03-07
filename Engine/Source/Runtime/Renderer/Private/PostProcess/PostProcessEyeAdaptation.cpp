// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessEyeAdaptation.cpp: Post processing eye adaptation implementation.
=============================================================================*/

#include "PostProcess/PostProcessEyeAdaptation.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"
#include "UnrealMathUtility.h"


/**
 *   Shared functionality used in computing the eye-adaptation parameters
 *   Compute the parameters used for eye-adaptation.  These will default to values
 *   that disable eye-adaptation if the hardware doesn't support the minimum feature level
 */
inline static void ComputeEyeAdaptationValues(const ERHIFeatureLevel::Type MinFeatureLevel, const FViewInfo& View, FVector4 Out[3])
{
	const FPostProcessSettings& Settings = View.FinalPostProcessSettings;
	const FEngineShowFlags& EngineShowFlags = View.Family->EngineShowFlags;

	float EyeAdaptationMin = Settings.AutoExposureMinBrightness;
	float EyeAdaptationMax = Settings.AutoExposureMaxBrightness;

	// FLT_MAX means no override
	float LocalOverrideExposure = FLT_MAX;

	// Eye adaptation is disabled except for highend right now because the histogram is not computed.
	if (!EngineShowFlags.EyeAdaptation || View.GetFeatureLevel() < MinFeatureLevel)
	{
		LocalOverrideExposure = 0;
	}


	float LocalExposureMultipler = FMath::Pow(2.0f, Settings.AutoExposureBias);

	if (View.Family->ExposureSettings.bFixed)
	{
		// editor wants to override the setting with it's own fixed setting
		LocalOverrideExposure = View.Family->ExposureSettings.LogOffset;
		LocalExposureMultipler = 1;
	}

	if (LocalOverrideExposure != FLT_MAX)
	{
		// set the eye adaptation to a fixed value
		EyeAdaptationMin = EyeAdaptationMax = FMath::Pow(2.0f, -LocalOverrideExposure);
	}

	if (EyeAdaptationMin > EyeAdaptationMax)
	{
		EyeAdaptationMin = EyeAdaptationMax;
	}

	float LowPercent = FMath::Clamp(Settings.AutoExposureLowPercent, 1.0f, 99.0f) * 0.01f;
	float HighPercent = FMath::Clamp(Settings.AutoExposureHighPercent, 1.0f, 99.0f) * 0.01f;

	if (LowPercent > HighPercent)
	{
		LowPercent = HighPercent;
	}

	Out[0] = FVector4(LowPercent, HighPercent, EyeAdaptationMin, EyeAdaptationMax);

	// ----------

	Out[1] = FVector4(LocalExposureMultipler, View.Family->DeltaWorldTime, Settings.AutoExposureSpeedUp, Settings.AutoExposureSpeedDown);

	// ----------

	// example min/max: -8 .. 4   means a range from 1/256 to 4  pow(2,-8) .. pow(2,4)
	float HistogramLogMin = Settings.HistogramLogMin;
	float HistogramLogMax = Settings.HistogramLogMax;

	float DeltaLog = HistogramLogMax - HistogramLogMin;
	float Multiply = 1.0f / DeltaLog;
	float Add = -HistogramLogMin * Multiply;
	float MinIntensity = FMath::Exp2(HistogramLogMin);
	Out[2] = FVector4(Multiply, Add, MinIntensity, 0);
}

// Basic AutoExposure requires at least ES3_1
static ERHIFeatureLevel::Type BasicEyeAdaptationMinFeatureLevel = ERHIFeatureLevel::ES3_1;

// Initialize the static CVar
TAutoConsoleVariable<int32> CVarEyeAdaptationMethodOveride(
	TEXT("r.EyeAdaptation.MethodOveride"),
	-1,
	TEXT("Overide the eye adapation method set in post processing volumes\n")
	TEXT("-2: override with custom settings (for testing Basic Mode)\n")
	TEXT("-1: no override\n")
	TEXT(" 1: Histogram-based\n")
	TEXT(" 2: Basic"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

// Initialize the static CVar used in computing the weighting focus in basic eye-adaptation
TAutoConsoleVariable<float> CVarEyeAdaptationFocus(
	TEXT("r.EyeAdaptation.Focus"),
	1.0f,
	TEXT("Applies to basic adapation mode only\n")
	TEXT(" 0: Uniform weighting\n")
	TEXT(">0: Center focus, 1 is a good number (default)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

/** Encapsulates the histogram-based post processing eye adaptation pixel shader. */
class FPostProcessEyeAdaptationPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessEyeAdaptationPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_A32B32G32R32F);
	}

	/** Default constructor. */
	FPostProcessEyeAdaptationPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter EyeAdaptationParams;

	/** Initialization constructor. */
	FPostProcessEyeAdaptationPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		EyeAdaptationParams.Bind(Initializer.ParameterMap, TEXT("EyeAdaptationParams"));
	}

	template <typename TRHICmdList>
	void SetPS(const FRenderingCompositePassContext& Context, TRHICmdList& RHICmdList)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		{
			FVector4 Temp[3];

			FRCPassPostProcessEyeAdaptation::ComputeEyeAdaptationParamsValue(Context.View, Temp);
			SetShaderValueArray(RHICmdList, ShaderRHI, EyeAdaptationParams, Temp, 3);
		}
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << EyeAdaptationParams;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessEyeAdaptationPS,TEXT("/Engine/Private/PostProcessEyeAdaptation.usf"),TEXT("MainPS"),SF_Pixel);

/** Encapsulates the histogram-based post processing eye adaptation compute shader. */
class FPostProcessEyeAdaptationCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessEyeAdaptationCS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_A32B32G32R32F);
	}

	/** Default constructor. */
	FPostProcessEyeAdaptationCS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FRWShaderParameter OutComputeTex;
	FShaderParameter EyeAdaptationParams;

	/** Initialization constructor. */
	FPostProcessEyeAdaptationCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		OutComputeTex.Bind(Initializer.ParameterMap, TEXT("OutComputeTex"));
		EyeAdaptationParams.Bind(Initializer.ParameterMap, TEXT("EyeAdaptationParams"));
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, FUnorderedAccessViewRHIParamRef DestUAV)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		// CS params
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetCS(ShaderRHI, Context, RHICmdList, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		OutComputeTex.SetTexture(RHICmdList, ShaderRHI, nullptr, DestUAV);		
		
		// PS params
		FVector4 EyeAdaptationParamValues[3];
		FRCPassPostProcessEyeAdaptation::ComputeEyeAdaptationParamsValue(Context.View, EyeAdaptationParamValues);
		SetShaderValueArray(RHICmdList, ShaderRHI, EyeAdaptationParams, EyeAdaptationParamValues, 3);
	}

	template <typename TRHICmdList>
	void UnsetParameters(TRHICmdList& RHICmdList)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		OutComputeTex.UnsetUAV(RHICmdList, ShaderRHI);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << OutComputeTex << EyeAdaptationParams;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessEyeAdaptationCS,TEXT("/Engine/Private/PostProcessEyeAdaptation.usf"),TEXT("MainCS"),SF_Compute);

void FRCPassPostProcessEyeAdaptation::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENTF(Context.RHICmdList, PostProcessEyeAdaptation, TEXT("PostProcessEyeAdaptation%s"), bIsComputePass?TEXT("Compute"):TEXT(""));
	AsyncEndFence = FComputeFenceRHIRef();

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	IPooledRenderTarget* EyeAdaptation = Context.View.GetEyeAdaptation(Context.RHICmdList);
	check(EyeAdaptation);

	FIntPoint DestSize = EyeAdaptation->GetDesc().Extent;

	const FSceneRenderTargetItem& DestRenderTarget = EyeAdaptation->GetRenderTargetItem();

	static auto* RenderPassCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RHIRenderPasses"));
	if (bIsComputePass)
	{
		FIntRect DestRect(0, 0, DestSize.X, DestSize.Y);

		// Common setup
		SetRenderTarget(Context.RHICmdList, nullptr, nullptr);
		Context.SetViewportAndCallRHI(DestRect, 0.0f, 1.0f);
		
		static FName AsyncEndFenceName(TEXT("AsyncEyeAdaptationEndFence"));
		AsyncEndFence = Context.RHICmdList.CreateComputeFence(AsyncEndFenceName);

		if (IsAsyncComputePass())
		{
			// Async path
			FRHIAsyncComputeCommandListImmediate& RHICmdListComputeImmediate = FRHICommandListExecutor::GetImmediateAsyncComputeCommandList();
			{
				SCOPED_COMPUTE_EVENT(RHICmdListComputeImmediate, AsyncEyeAdaptation);
				WaitForInputPassComputeFences(RHICmdListComputeImmediate);
				RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget.UAV);
				DispatchCS(RHICmdListComputeImmediate, Context, DestRenderTarget.UAV);
				RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget.UAV, AsyncEndFence);
			}
			FRHIAsyncComputeCommandListImmediate::ImmediateDispatch(RHICmdListComputeImmediate);
		}
		else
		{
			// Direct path
			WaitForInputPassComputeFences(Context.RHICmdList);
			Context.RHICmdList.BeginUpdateMultiFrameResource(DestRenderTarget.ShaderResourceTexture);

			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget.UAV);
			DispatchCS(Context.RHICmdList, Context, DestRenderTarget.UAV);
			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget.UAV, AsyncEndFence);

			Context.RHICmdList.EndUpdateMultiFrameResource(DestRenderTarget.ShaderResourceTexture);
		}
	}
	else
	{
		// Inform MultiGPU systems that we're starting to update this texture for this frame
		Context.RHICmdList.BeginUpdateMultiFrameResource(DestRenderTarget.ShaderResourceTexture);

		// we render to our own output render target, not the intermediate one created by the compositing system
		// Set the view family's render target/viewport.
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef(), true);
		Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
		TShaderMapRef<FPostProcessEyeAdaptationPS> PixelShader(Context.GetShaderMap());

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		PixelShader->SetPS(Context, Context.RHICmdList);

		// Draw a quad mapping scene color to the view's render target
		DrawRectangle(
			Context.RHICmdList,
			0, 0,
			DestSize.X, DestSize.Y,
			0, 0,
			DestSize.X, DestSize.Y,
			DestSize,
			DestSize,
			*VertexShader,
			EDRF_UseTriangleOptimization);

		Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

		// Inform MultiGPU systems that we've finished updating this texture for this frame
		Context.RHICmdList.EndUpdateMultiFrameResource(DestRenderTarget.ShaderResourceTexture);
	}

	Context.View.SetValidEyeAdaptation();
}

template <typename TRHICmdList>
void FRCPassPostProcessEyeAdaptation::DispatchCS(TRHICmdList& RHICmdList, FRenderingCompositePassContext& Context, FUnorderedAccessViewRHIParamRef DestUAV)
{
	auto ShaderMap = Context.GetShaderMap();
	TShaderMapRef<FPostProcessEyeAdaptationCS> ComputeShader(ShaderMap);
	RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

	ComputeShader->SetParameters(RHICmdList, Context, DestUAV);
	DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);
	ComputeShader->UnsetParameters(RHICmdList);
}

void FRCPassPostProcessEyeAdaptation::ComputeEyeAdaptationParamsValue(const FViewInfo& View, FVector4 Out[3])
{
	ComputeEyeAdaptationValues(ERHIFeatureLevel::SM5, View, Out);
}

float FRCPassPostProcessEyeAdaptation::ComputeExposureScaleValue(const FViewInfo& View)
{
	FVector4 EyeAdaptationParams[3];

	FRCPassPostProcessEyeAdaptation::ComputeEyeAdaptationParamsValue(View, EyeAdaptationParams);
	
	// like in PostProcessEyeAdaptation.usf
	float Exposure = (EyeAdaptationParams[0].Z + EyeAdaptationParams[0].W) * 0.5f;
	float ExposureScale = 1.0f / FMath::Max(0.0001f, Exposure);

	float ExposureOffsetMultipler = EyeAdaptationParams[1].X;

	return ExposureScale * ExposureOffsetMultipler;
}

FPooledRenderTargetDesc FRCPassPostProcessEyeAdaptation::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	// Specify invalid description to avoid getting intermediate rendertargets created.
	// We want to use ViewState->GetEyeAdaptation() instead
	FPooledRenderTargetDesc Ret;

	Ret.DebugName = TEXT("EyeAdaptation");

	return Ret;
}


/** Encapsulates the post process computation of Log2 Luminance pixel shader. */
class FPostProcessBasicEyeAdaptationSetupPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBasicEyeAdaptationSetupPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, BasicEyeAdaptationMinFeatureLevel);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FPostProcessBasicEyeAdaptationSetupPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter EyeAdaptationParams;

	/** Initialization constructor. */
	FPostProcessBasicEyeAdaptationSetupPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		EyeAdaptationParams.Bind(Initializer.ParameterMap, TEXT("EyeAdaptationParams"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		{
			FVector4 Temp[3];

			ComputeEyeAdaptationValues(BasicEyeAdaptationMinFeatureLevel, Context.View, Temp);
			SetShaderValueArray(Context.RHICmdList, ShaderRHI, EyeAdaptationParams, Temp, 3);
		}
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << EyeAdaptationParams;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(, FPostProcessBasicEyeAdaptationSetupPS, TEXT("/Engine/Private/PostProcessEyeAdaptation.usf"), TEXT("MainBasicEyeAdaptationSetupPS"), SF_Pixel);

void FRCPassPostProcessBasicEyeAdaptationSetUp::Process(FRenderingCompositePassContext& Context)
{
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if (!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	SCOPED_DRAW_EVENTF(Context.RHICmdList, PostProcessBasicEyeAdaptationSetup, TEXT("PostProcessBasicEyeAdaptationSetup %dx%d"), DestRect.Width(), DestRect.Height());

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	DrawClearQuad(Context.RHICmdList, true, FLinearColor::Black, false, 0, false, 0, PassOutputs[0].RenderTargetDesc.Extent, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessBasicEyeAdaptationSetupPS> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	PixelShader->SetPS(Context);

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

FPooledRenderTargetDesc FRCPassPostProcessBasicEyeAdaptationSetUp::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("EyeAdaptationBasicSetup");
	// Require alpha channel for log2 information.
	Ret.Format = PF_FloatRGBA;
	Ret.Flags |= GFastVRamConfig.EyeAdaptation;
	return Ret;
}



/** Encapsulates the post process computation of the exposure scale  pixel shader. */
class FPostProcessLogLuminance2ExposureScalePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessLogLuminance2ExposureScalePS, Global);



public:
	/** Default constructor. */
	FPostProcessLogLuminance2ExposureScalePS() {}

	/** Initialization constructor. */
	FPostProcessLogLuminance2ExposureScalePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		EyeAdaptationTexture.Bind(Initializer.ParameterMap, TEXT("EyeAdaptationTexture"));
		EyeAdaptationParams.Bind(Initializer.ParameterMap, TEXT("EyeAdaptationParams"));
		EyeAdaptationSrcRect.Bind(Initializer.ParameterMap, TEXT("EyeAdaptionSrcRect"));
	}

public:

	/** Static Shader boilerplate */
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, BasicEyeAdaptationMinFeatureLevel);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_A32B32G32R32F);
	}


	void SetPS(const FRenderingCompositePassContext& Context, const FIntRect& SrcRect, IPooledRenderTarget* EyeAdaptationLastFrameRT)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		// Associate the eye adaptation buffer from the previous frame with a texture to be read in this frame. 
		
		if (Context.View.HasValidEyeAdaptation())
		{
			SetTextureParameter(Context.RHICmdList, ShaderRHI, EyeAdaptationTexture, EyeAdaptationLastFrameRT->GetRenderTargetItem().TargetableTexture);
		}
		else
		{
			// some views don't have a state, thumbnail rendering?
			SetTextureParameter(Context.RHICmdList, ShaderRHI, EyeAdaptationTexture, GWhiteTexture->TextureRHI);
		}

		// Pack the eye adaptation parameters for the shader
		{
			FVector4 Temp[3];
			// static computation function
			ComputeEyeAdaptationValues(BasicEyeAdaptationMinFeatureLevel, Context.View, Temp);
			// Log-based computation of the exposure scale has a built in scaling.
			//Temp[1].X *= 0.16;  
			//Encode the eye-focus slope
			// Get the focus value for the eye-focus weighting
			Temp[2].W = GetBasicAutoExposureFocus();
			SetShaderValueArray(Context.RHICmdList, ShaderRHI, EyeAdaptationParams, Temp, 3);
		}

		// Set the src extent for the shader
		SetShaderValue(Context.RHICmdList, ShaderRHI, EyeAdaptationSrcRect, SrcRect);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << EyeAdaptationTexture 
			<< EyeAdaptationParams << EyeAdaptationSrcRect;
		return bShaderHasOutdatedParameters;
	}

private:
	FPostProcessPassParameters PostprocessParameter;
	FShaderResourceParameter EyeAdaptationTexture;
	FShaderParameter EyeAdaptationParams;
	FShaderParameter EyeAdaptationSrcRect;
};

IMPLEMENT_SHADER_TYPE(, FPostProcessLogLuminance2ExposureScalePS, TEXT("/Engine/Private/PostProcessEyeAdaptation.usf"), TEXT("MainLogLuminance2ExposureScalePS"), SF_Pixel);

void FRCPassPostProcessBasicEyeAdaptation::Process(FRenderingCompositePassContext& Context)
{
	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	// Get the custom 1x1 target used to store exposure value and Toggle the two render targets used to store new and old.
	Context.View.SwapEyeAdaptationRTs();
	IPooledRenderTarget* EyeAdaptationThisFrameRT = Context.View.GetEyeAdaptationRT(Context.RHICmdList);
	IPooledRenderTarget* EyeAdaptationLastFrameRT = Context.View.GetLastEyeAdaptationRT(Context.RHICmdList);

	check(EyeAdaptationThisFrameRT && EyeAdaptationLastFrameRT);

	FIntPoint DestSize = EyeAdaptationThisFrameRT->GetDesc().Extent;

	// The input texture sample size.  Averaged in the pixel shader.
	FIntPoint SrcSize = GetInputDesc(ePId_Input0)->Extent;

	// Compute the region of interest in the source texture.
	uint32 ScaleFactor = FMath::DivideAndRoundUp(FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().Y, SrcSize.Y);

	FIntRect SrcRect = View.ViewRect / ScaleFactor;

	SCOPED_DRAW_EVENTF(Context.RHICmdList, PostProcessBasicEyeAdaptation, TEXT("PostProcessBasicEyeAdaptation %dx%d"), SrcSize.X, SrcSize.Y);

	const FSceneRenderTargetItem& DestRenderTarget = EyeAdaptationThisFrameRT->GetRenderTargetItem();

	// Inform MultiGPU systems that we're starting to update this texture for this frame
	Context.RHICmdList.BeginUpdateMultiFrameResource(DestRenderTarget.ShaderResourceTexture);

	// we render to our own output render target, not the intermediate one created by the compositing system
	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef(), true);
	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessLogLuminance2ExposureScalePS> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	// Set the parameters used by the pixel shader.

	PixelShader->SetPS(Context, SrcRect, EyeAdaptationLastFrameRT);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DestSize.X, DestSize.Y,
		0, 0,
		DestSize.X, DestSize.Y,
		DestSize,
		DestSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	// Inform MultiGPU systems that we've finished with this texture for this frame
	Context.RHICmdList.EndUpdateMultiFrameResource(DestRenderTarget.ShaderResourceTexture);

	Context.View.SetValidEyeAdaptation();
}


FPooledRenderTargetDesc FRCPassPostProcessBasicEyeAdaptation::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	// Specify invalid description to avoid getting intermediate rendertargets created.
	// We want to use ViewState->GetEyeAdaptation() instead
	FPooledRenderTargetDesc Ret;

	Ret.DebugName = TEXT("EyeAdaptationBasic");
	Ret.Flags |= GFastVRamConfig.EyeAdaptation;
	return Ret;
}
