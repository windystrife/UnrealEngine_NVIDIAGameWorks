// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessBloomSetup.cpp: Post processing bloom threshold pass implementation.
=============================================================================*/

#include "PostProcess/PostProcessBloomSetup.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessEyeAdaptation.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"

const int32 GBloomSetupComputeTileSizeX = 8;
const int32 GBloomSetupComputeTileSizeY = 8;

/** Encapsulates the post processing bloom threshold pixel shader. */
class FPostProcessBloomSetupPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomSetupPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}	
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);

		if( !IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) )
		{
			//Need to hack in exposure scale for < SM5
			OutEnvironment.SetDefine(TEXT("NO_EYEADAPTATION_EXPOSURE_FIX"), 1);
		}
	}

	/** Default constructor. */
	FPostProcessBloomSetupPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter BloomThreshold;

	/** Initialization constructor. */
	FPostProcessBloomSetupPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
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

		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		float ExposureScale = FRCPassPostProcessEyeAdaptation::ComputeExposureScaleValue(Context.View);

		FVector4 BloomThresholdValue(Settings.BloomThreshold, 0, 0, ExposureScale);
		SetShaderValue(RHICmdList, ShaderRHI, BloomThreshold, BloomThresholdValue);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << BloomThreshold;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomSetupPS,TEXT("/Engine/Private/PostProcessBloom.usf"),TEXT("MainPS"),SF_Pixel);


/** Encapsulates the post processing bloom setup vertex shader. */
class FPostProcessBloomSetupVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomSetupVS,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FPostProcessBloomSetupVS(){}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderResourceParameter EyeAdaptation;


	/** Initialization constructor. */
	FPostProcessBloomSetupVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		EyeAdaptation.Bind(Initializer.ParameterMap, TEXT("EyeAdaptation"));
	}
	
	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		if(EyeAdaptation.IsBound())
		{
			if (Context.View.HasValidEyeAdaptation())
			{
				IPooledRenderTarget* EyeAdaptationRT = Context.View.GetEyeAdaptation(Context.RHICmdList);
				SetTextureParameter(Context.RHICmdList, ShaderRHI, EyeAdaptation, EyeAdaptationRT->GetRenderTargetItem().TargetableTexture);
			}
			else
			{
				SetTextureParameter(Context.RHICmdList, ShaderRHI, EyeAdaptation, GWhiteTexture->TextureRHI);
			}
		}
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << EyeAdaptation;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomSetupVS,TEXT("/Engine/Private/PostProcessBloom.usf"),TEXT("MainVS"),SF_Vertex);

/** Encapsulates the post processing bloom threshold compute shader. */
class FPostProcessBloomSetupCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomSetupCS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}	
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GBloomSetupComputeTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GBloomSetupComputeTileSizeY);
	}

	/** Default constructor. */
	FPostProcessBloomSetupCS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderResourceParameter EyeAdaptation;
	FShaderParameter BloomSetupComputeParams;
	FShaderParameter OutComputeTex;

	/** Initialization constructor. */
	FPostProcessBloomSetupCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		EyeAdaptation.Bind(Initializer.ParameterMap, TEXT("EyeAdaptation"));
		BloomSetupComputeParams.Bind(Initializer.ParameterMap, TEXT("BloomSetupComputeParams"));
		OutComputeTex.Bind(Initializer.ParameterMap, TEXT("OutComputeTex"));
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, const FIntPoint& DestSize, FUnorderedAccessViewRHIParamRef DestUAV, FTextureRHIParamRef EyeAdaptationTex)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetCS(ShaderRHI, Context, RHICmdList, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		RHICmdList.SetUAVParameter(ShaderRHI, OutComputeTex.GetBaseIndex(), DestUAV);

		SetTextureParameter(RHICmdList, ShaderRHI, EyeAdaptation, EyeAdaptationTex);

		//float ExposureScale = FRCPassPostProcessEyeAdaptation::ComputeExposureScaleValue(Context.View);
		FVector4 BloomSetupComputeValues(Settings.BloomThreshold, 0, 1.f / (float)DestSize.X, 1.f / (float)DestSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, BloomSetupComputeParams, BloomSetupComputeValues);
	}

	template <typename TRHICmdList>
	void UnsetParameters(TRHICmdList& RHICmdList)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		RHICmdList.SetUAVParameter(ShaderRHI, OutComputeTex.GetBaseIndex(), NULL);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << EyeAdaptation << BloomSetupComputeParams << OutComputeTex;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomSetupCS,TEXT("/Engine/Private/PostProcessBloom.usf"),TEXT("MainCS"),SF_Compute);

void FRCPassPostProcessBloomSetup::Process(FRenderingCompositePassContext& Context)
{
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	AsyncEndFence = FComputeFenceRHIRef();

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	
	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FMath::DivideAndRoundUp(FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().Y, SrcSize.Y);

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	SCOPED_DRAW_EVENTF(Context.RHICmdList, PostProcessBloomSetup, TEXT("PostProcessBloomSetup%s %dx%d"), bIsComputePass?TEXT("Compute"):TEXT(""), DestRect.Width(), DestRect.Height());

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	
	if (bIsComputePass)
	{
		DestRect = {View.ViewRect.Min, View.ViewRect.Min + DestSize};
	
		// Common setup
		SetRenderTarget(Context.RHICmdList, nullptr, nullptr);
		Context.SetViewportAndCallRHI(DestRect, 0.0f, 1.0f);
		
		static FName AsyncEndFenceName(TEXT("AsyncBloomSetupEndFence"));
		AsyncEndFence = Context.RHICmdList.CreateComputeFence(AsyncEndFenceName);

		FTextureRHIRef EyeAdaptationTex = GWhiteTexture->TextureRHI;
		if (Context.View.HasValidEyeAdaptation())
		{
			EyeAdaptationTex = Context.View.GetEyeAdaptation(Context.RHICmdList)->GetRenderTargetItem().TargetableTexture;
		}

		if (IsAsyncComputePass())
		{
			// Async path
			FRHIAsyncComputeCommandListImmediate& RHICmdListComputeImmediate = FRHICommandListExecutor::GetImmediateAsyncComputeCommandList();
			{
				SCOPED_COMPUTE_EVENT(RHICmdListComputeImmediate, AsyncBloomSetup);
				WaitForInputPassComputeFences(RHICmdListComputeImmediate);
					
				RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget.UAV);
				DispatchCS(RHICmdListComputeImmediate, Context, DestRect, DestRenderTarget.UAV, EyeAdaptationTex);
				RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget.UAV, AsyncEndFence);
			}
			FRHIAsyncComputeCommandListImmediate::ImmediateDispatch(RHICmdListComputeImmediate);
		}
		else
		{
			// Direct path
			WaitForInputPassComputeFences(Context.RHICmdList);

			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget.UAV);
			DispatchCS(Context.RHICmdList, Context, DestRect, DestRenderTarget.UAV, EyeAdaptationTex);			
			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget.UAV, AsyncEndFence);
		}
	}
	else
	{
		WaitForInputPassComputeFences(Context.RHICmdList);

		// Set the view family's render target/viewport.
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

		// is optimized away if possible (RT size=view size, )
		DrawClearQuad(Context.RHICmdList, true, FLinearColor::Black, false, 0, false, 0, DestSize, DestRect);

		Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		// set the state
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		TShaderMapRef<FPostProcessBloomSetupVS> VertexShader(Context.GetShaderMap());
		TShaderMapRef<FPostProcessBloomSetupPS> PixelShader(Context.GetShaderMap());

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		VertexShader->SetVS(Context);
		PixelShader->SetPS(Context.RHICmdList, Context);

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
			false, // Disabled for correctness
			EDRF_UseTriangleOptimization);

		Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
	}
}

template <typename TRHICmdList>
void FRCPassPostProcessBloomSetup::DispatchCS(TRHICmdList& RHICmdList, FRenderingCompositePassContext& Context, const FIntRect& DestRect, FUnorderedAccessViewRHIParamRef DestUAV, FTextureRHIParamRef EyeAdaptationTex)
{
	auto ShaderMap = Context.GetShaderMap();
	TShaderMapRef<FPostProcessBloomSetupCS> ComputeShader(ShaderMap);
	RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

	FIntPoint DestSize(DestRect.Width(), DestRect.Height());
	ComputeShader->SetParameters(RHICmdList, Context, DestSize, DestUAV, EyeAdaptationTex);

	uint32 GroupSizeX = FMath::DivideAndRoundUp(DestSize.X, GBloomSetupComputeTileSizeX);
	uint32 GroupSizeY = FMath::DivideAndRoundUp(DestSize.Y, GBloomSetupComputeTileSizeY);
	DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

	ComputeShader->UnsetParameters(RHICmdList);
}

FPooledRenderTargetDesc FRCPassPostProcessBloomSetup::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("BloomSetup");
	Ret.TargetableFlags &= ~(TexCreate_RenderTargetable | TexCreate_UAV);
	Ret.TargetableFlags |= bIsComputePass ? TexCreate_UAV : TexCreate_RenderTargetable;
	Ret.AutoWritable = false;
	return Ret;
}


/** Encapsulates the visualize bloom setup pixel shader. */
class FPostProcessVisualizeBloomSetupPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessVisualizeBloomSetupPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	/** Default constructor. */
	FPostProcessVisualizeBloomSetupPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	/** Initialization constructor. */
	FPostProcessVisualizeBloomSetupPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI());
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessVisualizeBloomSetupPS,TEXT("/Engine/Private/PostProcessBloom.usf"),TEXT("VisualizeBloomSetupPS"),SF_Pixel);




void FRCPassPostProcessVisualizeBloomSetup::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, VisualizeBloomSetup);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	check(InputDesc && "Input is not hooked up correctly");

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FMath::DivideAndRoundUp(FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().Y, SrcSize.Y);

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	DrawClearQuad(Context.RHICmdList, true, FLinearColor(0, 0, 0, 0), false, 0, false, 0, DestSize, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestRect.Width(), DestRect.Height(), 1.0f );

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// set the state
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessVisualizeBloomSetupPS> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	PixelShader->SetParameters(Context.RHICmdList, Context);
	VertexShader->SetParameters(Context);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}


FPooledRenderTargetDesc FRCPassPostProcessVisualizeBloomSetup::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.TargetableFlags &= ~TexCreate_UAV;
	Ret.TargetableFlags |= TexCreate_RenderTargetable;
	Ret.DebugName = TEXT("VisualizeBloomSetup");

	return Ret;
}







/** Encapsulates the visualize bloom overlay pixel shader. */
class FPostProcessVisualizeBloomOverlayPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessVisualizeBloomOverlayPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	/** Default constructor. */
	FPostProcessVisualizeBloomOverlayPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter ColorScale1;

	/** Initialization constructor. */
	FPostProcessVisualizeBloomOverlayPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		ColorScale1.Bind(Initializer.ParameterMap, TEXT("ColorScale1"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << ColorScale1;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI());

		{
			FLinearColor Col = FLinearColor::White * Settings.BloomIntensity;
			FVector4 ColorScale(Col.R, Col.G, Col.B, 0);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorScale1, ColorScale);
		}
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessVisualizeBloomOverlayPS,TEXT("/Engine/Private/PostProcessBloom.usf"),TEXT("VisualizeBloomOverlayPS"),SF_Pixel);




void FRCPassPostProcessVisualizeBloomOverlay::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, VisualizeBloomOverlay);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	check(InputDesc && "Input is not hooked up correctly");

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FMath::DivideAndRoundUp(FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().Y, SrcSize.Y);

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	DrawClearQuad(Context.RHICmdList, true, FLinearColor(0, 0, 0 ,0), false, 0, false, 0, DestSize, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestRect.Width(), DestRect.Height(), 1.0f );

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// set the state
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessVisualizeBloomOverlayPS> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	PixelShader->SetParameters(Context);
	VertexShader->SetParameters(Context);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}


FPooledRenderTargetDesc FRCPassPostProcessVisualizeBloomOverlay::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.TargetableFlags &= ~TexCreate_UAV;
	Ret.TargetableFlags |= TexCreate_RenderTargetable;
	Ret.DebugName = TEXT("VisualizeBloomOverlay");

	return Ret;
}

