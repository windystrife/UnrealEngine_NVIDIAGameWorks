// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessDownsample.cpp: Post processing down sample implementation.
=============================================================================*/

#include "PostProcess/PostProcessDownsample.h"
#include "ClearQuad.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/SceneFilterRendering.h"
#include "SceneRenderTargetParameters.h"
#include "SceneRendering.h"
#include "PipelineStateCache.h"

const int32 GDownsampleTileSizeX = 8;
const int32 GDownsampleTileSizeY = 8;

/** Encapsulates the post processing down sample pixel shader. */
template <uint32 Method>
class FPostProcessDownsamplePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDownsamplePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return Method != 2 || IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("METHOD"), Method);
	}

	/** Default constructor. */
	FPostProcessDownsamplePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DownsampleParams;

	/** Initialization constructor. */
	FPostProcessDownsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		DownsampleParams.Bind(Initializer.ParameterMap, TEXT("DownsampleParams"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << DownsampleParams;
		return bShaderHasOutdatedParameters;
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, const FPooledRenderTargetDesc* InputDesc)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		DeferredParameters.Set(RHICmdList, ShaderRHI, Context.View, MD_PostProcess);

		// filter only if needed for better performance
		FSamplerStateRHIParamRef Filter = (Method == 2) ? 
			TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI():
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		
		{
			float PixelScale = (Method == 2) ? 0.5f : 1.0f;
 
			FVector4 DownsampleParamsValue(PixelScale / InputDesc->Extent.X, PixelScale / InputDesc->Extent.Y, 0, 0);
			SetShaderValue(RHICmdList, ShaderRHI, DownsampleParams, DownsampleParamsValue);
		}

		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, Filter);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/PostProcessDownsample.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessDownsamplePS<A> FPostProcessDownsamplePS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessDownsamplePS##A, SF_Pixel);

VARIATION1(0)			VARIATION1(1)			VARIATION1(2)
#undef VARIATION1


/** Encapsulates the post processing down sample vertex shader. */
class FPostProcessDownsampleVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDownsampleVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	/** Default constructor. */
	FPostProcessDownsampleVS() {}
	
	/** Initialization constructor. */
	FPostProcessDownsampleVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
	}

	/** Serializer */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		const FPooledRenderTargetDesc* InputDesc = Context.Pass->GetInputDesc(ePId_Input0);

		if(!InputDesc)
		{
			// input is not hooked up correctly
			return;
		}
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessDownsampleVS,TEXT("/Engine/Private/PostProcessDownsample.usf"),TEXT("MainDownsampleVS"),SF_Vertex);

/** Encapsulates the post processing down sample compute shader. */
template <uint32 Method>
class FPostProcessDownsampleCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDownsampleCS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("METHOD"), Method);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDownsampleTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDownsampleTileSizeY);
	}

	/** Default constructor. */
	FPostProcessDownsampleCS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DownsampleComputeParams;
	FShaderParameter OutComputeTex;

	/** Initialization constructor. */
	FPostProcessDownsampleCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		DownsampleComputeParams.Bind(Initializer.ParameterMap, TEXT("DownsampleComputeParams"));
		OutComputeTex.Bind(Initializer.ParameterMap, TEXT("OutComputeTex"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << DownsampleComputeParams << OutComputeTex;
		return bShaderHasOutdatedParameters;
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, const FIntPoint& SrcSize, FUnorderedAccessViewRHIParamRef DestUAV)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		// filter only if needed for better performance
		FSamplerStateRHIParamRef Filter = (Method == 2) ? 
			TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI():
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		PostprocessParameter.SetCS(ShaderRHI, Context, RHICmdList, Filter);
		DeferredParameters.Set(RHICmdList, ShaderRHI, Context.View, MD_PostProcess);
		RHICmdList.SetUAVParameter(ShaderRHI, OutComputeTex.GetBaseIndex(), DestUAV);
		
		const float PixelScale = (Method == 2) ? 0.5f : 1.0f;
		FVector4 DownsampleComputeValues(PixelScale / SrcSize.X, PixelScale / SrcSize.Y, 2.f / SrcSize.X, 2.f / SrcSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, DownsampleComputeParams, DownsampleComputeValues);
	}

	template <typename TRHICmdList>
	void UnsetParameters(TRHICmdList& RHICmdList)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		RHICmdList.SetUAVParameter(ShaderRHI, OutComputeTex.GetBaseIndex(), NULL);
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessDownsampleCS<A> FPostProcessDownsampleCS##A; \
	IMPLEMENT_SHADER_TYPE(template<>,FPostProcessDownsampleCS##A,TEXT("/Engine/Private/PostProcessDownsample.usf"),TEXT("MainCS"),SF_Compute);

VARIATION1(0)			VARIATION1(1)			VARIATION1(2)
#undef VARIATION1


FRCPassPostProcessDownsample::FRCPassPostProcessDownsample(EPixelFormat InOverrideFormat, uint32 InQuality, bool bInIsComputePass, const TCHAR *InDebugName)
	: OverrideFormat(InOverrideFormat)
	, Quality(InQuality)
	, DebugName(InDebugName)
{
	bIsComputePass = bInIsComputePass;
	bPreferAsyncCompute = false;
}

template <uint32 Method>
void FRCPassPostProcessDownsample::SetShader(const FRenderingCompositePassContext& Context, const FPooledRenderTargetDesc* InputDesc)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	auto ShaderMap = Context.GetShaderMap();
	TShaderMapRef<FPostProcessDownsampleVS> VertexShader(ShaderMap);
	TShaderMapRef<FPostProcessDownsamplePS<Method> > PixelShader(ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	PixelShader->SetParameters(Context.RHICmdList, Context, InputDesc);
	VertexShader->SetParameters(Context);
}

void FRCPassPostProcessDownsample::Process(FRenderingCompositePassContext& Context)
{
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	AsyncEndFence = FComputeFenceRHIRef();

	if (!InputDesc)
	{
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FMath::DivideAndRoundUp(FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().Y, SrcSize.Y);

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = FIntRect::DivideAndRoundUp(SrcRect, 2);
	SrcRect = DestRect * 2;

	SCOPED_DRAW_EVENTF(Context.RHICmdList, Downsample, TEXT("Downsample%s %dx%d"), bIsComputePass?TEXT("Compute"):TEXT(""), DestRect.Width(), DestRect.Height());

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	const bool bIsDepthInputAvailable = (GetInputDesc(ePId_Input1) != nullptr);

	if (bIsComputePass)
	{	
		DestRect = {View.ViewRect.Min, View.ViewRect.Min + DestSize};

		// Common setup
		SetRenderTarget(Context.RHICmdList, nullptr, nullptr);
		Context.SetViewportAndCallRHI(DestRect, 0.0f, 1.0f);
		
		static FName AsyncEndFenceName(TEXT("AsyncDownsampleEndFence"));
		AsyncEndFence = Context.RHICmdList.CreateComputeFence(AsyncEndFenceName);

		if (IsAsyncComputePass())
		{
			// Async path
			FRHIAsyncComputeCommandListImmediate& RHICmdListComputeImmediate = FRHICommandListExecutor::GetImmediateAsyncComputeCommandList();
			{
	 			SCOPED_COMPUTE_EVENT(RHICmdListComputeImmediate, AsyncDownsample);

				WaitForInputPassComputeFences(RHICmdListComputeImmediate);
				RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget.UAV);

				if (bIsDepthInputAvailable)
				{
					DispatchCS<2>(RHICmdListComputeImmediate, Context, SrcSize, DestRect, DestRenderTarget.UAV);
				}
				else if (Quality == 0)
				{
					DispatchCS<0>(RHICmdListComputeImmediate, Context, SrcSize, DestRect, DestRenderTarget.UAV);
				}
				else
				{
					DispatchCS<1>(RHICmdListComputeImmediate, Context, SrcSize, DestRect, DestRenderTarget.UAV);
				}

				RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget.UAV, AsyncEndFence);
			}
			FRHIAsyncComputeCommandListImmediate::ImmediateDispatch(RHICmdListComputeImmediate);		
		}
		else
		{
			// Direct path
			WaitForInputPassComputeFences(Context.RHICmdList);
			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget.UAV);

			if (bIsDepthInputAvailable)
			{
				DispatchCS<2>(Context.RHICmdList, Context, SrcSize, DestRect, DestRenderTarget.UAV);
			}
			else if (Quality == 0)
			{
				DispatchCS<0>(Context.RHICmdList, Context, SrcSize, DestRect, DestRenderTarget.UAV);
			}
			else
			{
				DispatchCS<1>(Context.RHICmdList, Context, SrcSize, DestRect, DestRenderTarget.UAV);
			}

			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget.UAV, AsyncEndFence);
		}
	}
	else
	{
		// check if we have to clear the whole surface.
		// Otherwise perform the clear when the dest rectangle has been computed.
		auto FeatureLevel = Context.View.GetFeatureLevel();
		if (FeatureLevel == ERHIFeatureLevel::ES2 || FeatureLevel == ERHIFeatureLevel::ES3_1)
		{
			// Set the view family's render target/viewport.
			SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef(), ESimpleRenderTargetMode::EClearColorAndDepth);
			Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f);
		}
		else
		{
			// Set the view family's render target/viewport.
			SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef(), ESimpleRenderTargetMode::EExistingColorAndDepth);
			Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f);
			DrawClearQuad(Context.RHICmdList, true, FLinearColor(0, 0, 0, 0), false, 1.0f, false, 0, DestSize, DestRect);
		}

		// InflateSize increases the size of the source/dest rectangle to compensate for bilinear reads and UIBlur pass requirements.
		int32 InflateSize;
		// if second input is hooked up
		if (bIsDepthInputAvailable)
		{
			// also put depth in alpha
			InflateSize = 2;
			SetShader<2>(Context, InputDesc);
		}
		else
		{
			if (Quality == 0)
			{
				SetShader<0>(Context, InputDesc);
				InflateSize = 1;
			}
			else
			{
				SetShader<1>(Context, InputDesc);
				InflateSize = 2;
			}
		}

		TShaderMapRef<FPostProcessDownsampleVS> VertexShader(Context.GetShaderMap());

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
			false, // This pass is input for passes that can't use the hmd mask, so we need to disable it to ensure valid input data
			EDRF_UseTriangleOptimization);

		Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
	}
}

template <uint32 Method, typename TRHICmdList>
void FRCPassPostProcessDownsample::DispatchCS(TRHICmdList& RHICmdList, FRenderingCompositePassContext& Context, const FIntPoint& SrcSize, const FIntRect& DestRect, FUnorderedAccessViewRHIParamRef DestUAV)
{
	auto ShaderMap = Context.GetShaderMap();
	TShaderMapRef<FPostProcessDownsampleCS<Method>> ComputeShader(ShaderMap);
	RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

	ComputeShader->SetParameters(RHICmdList, Context, SrcSize, DestUAV);

	uint32 GroupSizeX = FMath::DivideAndRoundUp(DestRect.Width(), GDownsampleTileSizeX);
	uint32 GroupSizeY = FMath::DivideAndRoundUp(DestRect.Height(), GDownsampleTileSizeY);
	DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

	ComputeShader->UnsetParameters(RHICmdList);
}

FPooledRenderTargetDesc FRCPassPostProcessDownsample::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();

	Ret.Extent  = FIntPoint::DivideAndRoundUp(Ret.Extent, 2);

	Ret.Extent.X = FMath::Max(1, Ret.Extent.X);
	Ret.Extent.Y = FMath::Max(1, Ret.Extent.Y);

	if(OverrideFormat != PF_Unknown)
	{
		Ret.Format = OverrideFormat;
	}

	Ret.TargetableFlags &= ~(TexCreate_RenderTargetable | TexCreate_UAV);
	Ret.TargetableFlags |= bIsComputePass ? TexCreate_UAV : TexCreate_RenderTargetable;
	Ret.Flags |= GFastVRamConfig.Downsample;
	Ret.AutoWritable = false;
	Ret.DebugName = DebugName;

	Ret.ClearValue = FClearValueBinding(FLinearColor(0,0,0,0));

	return Ret;
}
