// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessBokehDOFRecombine.cpp: Post processing lens blur implementation.
=============================================================================*/

#include "PostProcess/PostProcessBokehDOFRecombine.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/SceneFilterRendering.h"
#include "SceneRenderTargetParameters.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessBokehDOF.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"
#include "TranslucentRendering.h"

const int32 GBokehDOFRecombineComputeTileSizeX = 8;
const int32 GBokehDOFRecombineComputeTileSizeY = 8;

/**
 * Encapsulates a shader to combined Depth of field and separate translucency layers.
 * @param Method 1:DOF, 2:SeparateTranslucency, 3:DOF+SeparateTranslucency, 4:SeparateTranslucency with Nearest-Depth Neighbor, 5:DOF+SeparateTranslucency with Nearest-Depth Neighbor
 */
template <uint32 Method>
class FPostProcessBokehDOFRecombinePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBokehDOFRecombinePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static int32 GetCombineFeatureMethod()
	{
		if (Method <= 3)
		{
			return Method;
		}
		else
		{
			return Method - 2;
		}
	}

	static bool UseNearestDepthNeighborUpsample()
	{
		return Method > 3;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("RECOMBINE_METHOD"), GetCombineFeatureMethod());
		OutEnvironment.SetDefine(TEXT("NEAREST_DEPTH_NEIGHBOR_UPSAMPLE"), UseNearestDepthNeighborUpsample());
	}

	/** Default constructor. */
	FPostProcessBokehDOFRecombinePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DepthOfFieldParams;
	FShaderParameter SeparateTranslucencyResMultParam;
	FShaderResourceParameter LowResDepthTexture;
	FShaderResourceParameter BilinearClampedSampler;
	FShaderResourceParameter PointClampedSampler;

	/** Initialization constructor. */
	FPostProcessBokehDOFRecombinePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		DepthOfFieldParams.Bind(Initializer.ParameterMap,TEXT("DepthOfFieldParams"));
		SeparateTranslucencyResMultParam.Bind(Initializer.ParameterMap, TEXT("SeparateTranslucencyResMult"));
		LowResDepthTexture.Bind(Initializer.ParameterMap, TEXT("LowResDepthTexture"));
		BilinearClampedSampler.Bind(Initializer.ParameterMap, TEXT("BilinearClampedSampler"));
		PointClampedSampler.Bind(Initializer.ParameterMap, TEXT("PointClampedSampler"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DepthOfFieldParams << DeferredParameters << SeparateTranslucencyResMultParam << LowResDepthTexture << BilinearClampedSampler << PointClampedSampler;
		return bShaderHasOutdatedParameters;
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		DeferredParameters.Set(RHICmdList, ShaderRHI, Context.View, MD_PostProcess);
		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
		FIntPoint OutScaledSize;
		float OutScale;
		SceneContext.GetSeparateTranslucencyDimensions(OutScaledSize, OutScale);

		SetShaderValue(RHICmdList, ShaderRHI, SeparateTranslucencyResMultParam, FVector4(OutScale, OutScale, OutScale, OutScale));

		{
			FVector4 DepthOfFieldParamValues[2];

			FRCPassPostProcessBokehDOF::ComputeDepthOfFieldParams(Context, DepthOfFieldParamValues);

			SetShaderValueArray(RHICmdList, ShaderRHI, DepthOfFieldParams, DepthOfFieldParamValues, 2);
		}

		if (UseNearestDepthNeighborUpsample())
		{
			check(SceneContext.IsDownsampledTranslucencyDepthValid());
			FTextureRHIParamRef LowResDepth = SceneContext.GetDownsampledTranslucencyDepthSurface();
			SetTextureParameter(RHICmdList, ShaderRHI, LowResDepthTexture, LowResDepth);

			const auto& BuiltinSamplersUBParameter = GetUniformBufferParameter<FBuiltinSamplersParameters>();
			SetUniformBufferParameter(RHICmdList, ShaderRHI, BuiltinSamplersUBParameter, GBuiltinSamplersUniformBuffer.GetUniformBufferRHI());
		}
		else
		{
			checkSlow(!LowResDepthTexture.IsBound());
		}

		SetSamplerParameter(Context.RHICmdList, ShaderRHI, BilinearClampedSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		SetSamplerParameter(Context.RHICmdList, ShaderRHI, PointClampedSampler, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/PostProcessBokehDOF.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainRecombinePS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessBokehDOFRecombinePS<A> FPostProcessBokehDOFRecombinePS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessBokehDOFRecombinePS##A, SF_Pixel);

VARIATION1(1)			VARIATION1(2)			VARIATION1(3)			VARIATION1(4)			VARIATION1(5)
#undef VARIATION1

/**
 * Encapsulates a shader to combined Depth of field and separate translucency layers.
 * @param Method 1:DOF, 2:SeparateTranslucency, 3:DOF+SeparateTranslucency, 4:SeparateTranslucency with Nearest-Depth Neighbor, 5:DOF+SeparateTranslucency with Nearest-Depth Neighbor
 */
template <uint32 Method>
class FPostProcessBokehDOFRecombineCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBokehDOFRecombineCS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static int32 GetCombineFeatureMethod()
	{
		if (Method <= 3)
		{
			return Method;
		}
		else
		{
			return Method - 2;
		}
	}

	static bool UseNearestDepthNeighborUpsample()
	{
		return Method > 3;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		// CS Params
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GBokehDOFRecombineComputeTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GBokehDOFRecombineComputeTileSizeY);

		// PS params
		OutEnvironment.SetDefine(TEXT("RECOMBINE_METHOD"), GetCombineFeatureMethod());
		OutEnvironment.SetDefine(TEXT("NEAREST_DEPTH_NEIGHBOR_UPSAMPLE"), UseNearestDepthNeighborUpsample());
	}

	/** Default constructor. */
	FPostProcessBokehDOFRecombineCS() {}

public:
	// CS params
	FRWShaderParameter OutComputeTex;
	FShaderParameter BokehDOFRecombineComputeParams;

	// PS params
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DepthOfFieldParams;
	FShaderParameter SeparateTranslucencyResMultParam;
	FShaderResourceParameter LowResDepthTexture;
	FShaderResourceParameter BilinearClampedSampler;
	FShaderResourceParameter PointClampedSampler;

	/** Initialization constructor. */
	FPostProcessBokehDOFRecombineCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		// CS params
		OutComputeTex.Bind(Initializer.ParameterMap, TEXT("OutComputeTex"));
		BokehDOFRecombineComputeParams.Bind(Initializer.ParameterMap, TEXT("BokehDOFRecombineComputeParams"));

		// PS params
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		DepthOfFieldParams.Bind(Initializer.ParameterMap,TEXT("DepthOfFieldParams"));
		SeparateTranslucencyResMultParam.Bind(Initializer.ParameterMap, TEXT("SeparateTranslucencyResMult"));
		LowResDepthTexture.Bind(Initializer.ParameterMap, TEXT("LowResDepthTexture"));
		BilinearClampedSampler.Bind(Initializer.ParameterMap, TEXT("BilinearClampedSampler"));
		PointClampedSampler.Bind(Initializer.ParameterMap, TEXT("PointClampedSampler"));
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, const FIntPoint& DestSize, FUnorderedAccessViewRHIParamRef DestUAV, float UVScaling)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		// CS params
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		OutComputeTex.SetTexture(RHICmdList, ShaderRHI, nullptr, DestUAV);
		
		FVector4 BokehDOFRecombineComputeValues(0, 0, 1.f / (float)DestSize.X, UVScaling / (float)DestSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, BokehDOFRecombineComputeParams, BokehDOFRecombineComputeValues);

		// PS params
		PostprocessParameter.SetCS(ShaderRHI, Context, RHICmdList, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		DeferredParameters.Set(RHICmdList, ShaderRHI, Context.View, MD_PostProcess);

		FIntPoint OutScaledSize;
		float OutScale;
		SceneContext.GetSeparateTranslucencyDimensions(OutScaledSize, OutScale);

		SetShaderValue(RHICmdList, ShaderRHI, SeparateTranslucencyResMultParam, FVector4(OutScale, OutScale, OutScale, OutScale));

		{
			FVector4 DepthOfFieldParamValues[2];
			FRCPassPostProcessBokehDOF::ComputeDepthOfFieldParams(Context, DepthOfFieldParamValues);
			SetShaderValueArray(RHICmdList, ShaderRHI, DepthOfFieldParams, DepthOfFieldParamValues, 2);
		}

		if (UseNearestDepthNeighborUpsample())
		{
			check(SceneContext.IsDownsampledTranslucencyDepthValid());
			FTextureRHIParamRef LowResDepth = SceneContext.GetDownsampledTranslucencyDepthSurface();
			SetTextureParameter(RHICmdList, ShaderRHI, LowResDepthTexture, LowResDepth);

			const auto& BuiltinSamplersUBParameter = GetUniformBufferParameter<FBuiltinSamplersParameters>();
			SetUniformBufferParameter(RHICmdList, ShaderRHI, BuiltinSamplersUBParameter, GBuiltinSamplersUniformBuffer.GetUniformBufferRHI());

			SetSamplerParameter(RHICmdList, ShaderRHI, BilinearClampedSampler, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
			SetSamplerParameter(RHICmdList, ShaderRHI, PointClampedSampler, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		}
		else
		{
			checkSlow(!LowResDepthTexture.IsBound());
			SetSamplerParameter(RHICmdList, ShaderRHI, BilinearClampedSampler, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		}
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
		// CS params
		Ar << OutComputeTex << BokehDOFRecombineComputeParams;
		// PS params
		Ar << PostprocessParameter << DepthOfFieldParams << DeferredParameters << SeparateTranslucencyResMultParam << LowResDepthTexture << BilinearClampedSampler << PointClampedSampler;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/PostProcessBokehDOF.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainRecombineCS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessBokehDOFRecombineCS<A> FPostProcessBokehDOFRecombineCS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessBokehDOFRecombineCS##A, SF_Compute);

VARIATION1(1)			VARIATION1(2)			VARIATION1(3)			VARIATION1(4)			VARIATION1(5)
#undef VARIATION1

template <uint32 Method>
void FRCPassPostProcessBokehDOFRecombine::SetShader(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// set the state
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessBokehDOFRecombinePS<Method> > PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	PixelShader->SetParameters(Context.RHICmdList, Context);
	VertexShader->SetParameters(Context);
}

void FRCPassPostProcessBokehDOFRecombine::Process(FRenderingCompositePassContext& Context)
{
	AsyncEndFence = FComputeFenceRHIRef();

	uint32 Method = 2;
	FRenderingCompositeOutputRef* Input1 = GetInput(ePId_Input1);

	if(Input1 && Input1->GetPass())
	{
		if(GetInput(ePId_Input2)->GetPass())
		{
			Method = 3;
		}
		else
		{
			Method = 1;
		}
	}
	else
	{
		check(GetInput(ePId_Input2)->GetPass());
	}

	const bool bUseNearestDepthNeighborUpsample = UseNearestDepthNeighborUpsampleForSeparateTranslucency(FSceneRenderTargets::Get(Context.RHICmdList));

	if (Method != 1 && bUseNearestDepthNeighborUpsample)
	{
		Method += 2;
	}

	const FSceneView& View = Context.View;

	SCOPED_DRAW_EVENTF(Context.RHICmdList, BokehDOFRecombine, TEXT("BokehDOFRecombine%s#%d %dx%d"),
		bIsComputePass?TEXT("Compute"):TEXT(""), Method, View.ViewRect.Width(), View.ViewRect.Height());

	const FPooledRenderTargetDesc* InputDesc0 = GetInputDesc(ePId_Input0);
	const FPooledRenderTargetDesc* InputDesc1 = GetInputDesc(ePId_Input1);

	FIntPoint TexSize = InputDesc1 ? InputDesc1->Extent : InputDesc0->Extent;

	// usually 1, 2, 4 or 8
	uint32 ScaleToFullRes = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / TexSize.X;

	FIntRect HalfResViewRect = FIntRect::DivideAndRoundUp(View.ViewRect, ScaleToFullRes);

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	if (bIsComputePass)
	{
		FIntRect DestRect = {View.ViewRect.Min, View.ViewRect.Min + PassOutputs[0].RenderTargetDesc.Extent};

		// Calculate the scaling required to convert UVs to bokeh accumulation space
		const float UVScaling = (float)HalfResViewRect.Height() / (float)TexSize.Y;
	
		// Common setup
		SetRenderTarget(Context.RHICmdList, nullptr, nullptr);
		Context.SetViewportAndCallRHI(DestRect, 0.0f, 1.0f);
		
		static FName AsyncEndFenceName(TEXT("AsyncBokehDOFRecombineEndFence"));
		AsyncEndFence = Context.RHICmdList.CreateComputeFence(AsyncEndFenceName);

		if (IsAsyncComputePass())
		{
			// Async path
			FRHIAsyncComputeCommandListImmediate& RHICmdListComputeImmediate = FRHICommandListExecutor::GetImmediateAsyncComputeCommandList();
			{
				SCOPED_COMPUTE_EVENT(RHICmdListComputeImmediate, AsyncBokehDOFRecombine);
				WaitForInputPassComputeFences(RHICmdListComputeImmediate);

				RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget.UAV);
				DispatchCS(RHICmdListComputeImmediate, Context, DestRect, DestRenderTarget.UAV, Method, UVScaling);
				RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget.UAV, AsyncEndFence);
			}
			FRHIAsyncComputeCommandListImmediate::ImmediateDispatch(RHICmdListComputeImmediate);
		}
		else
		{
			// Direct path
			WaitForInputPassComputeFences(Context.RHICmdList);
			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget.UAV);
			DispatchCS(Context.RHICmdList, Context, DestRect, DestRenderTarget.UAV, Method, UVScaling);			
			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget.UAV, AsyncEndFence);
		}
	}
	else
	{
		WaitForInputPassComputeFences(Context.RHICmdList);

		// Set the view family's render target/viewport.
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

		// is optimized away if possible (RT size=view size, )
		DrawClearQuad(Context.RHICmdList, true, FLinearColor::Black, false, 1.0f, false, 0, PassOutputs[0].RenderTargetDesc.Extent, View.ViewRect);

		Context.SetViewportAndCallRHI(View.ViewRect);

		switch(Method)
		{
			case 1: SetShader<1>(Context); break;
			case 2: SetShader<2>(Context); break;
			case 3: SetShader<3>(Context); break;
			case 4: SetShader<4>(Context); break;
			case 5: SetShader<5>(Context); break;
			default:
				check(0);
		}

		TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

		DrawPostProcessPass(
			Context.RHICmdList,
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			HalfResViewRect.Min.X, HalfResViewRect.Min.Y,
			HalfResViewRect.Width(), HalfResViewRect.Height(),
			View.ViewRect.Size(),
			TexSize,
			*VertexShader,
			View.StereoPass,
			false, // Disabled for correctness
			EDRF_UseTriangleOptimization);


		// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
		if (GNVVolumetricLightingRHI && GNVVolumetricLightingRHI->IsRendering() && (Method > 1))
		{
			NvVl::PostprocessDesc* PostprocessDesc = GNVVolumetricLightingRHI->GetSeparateTranslucencyPostprocessDesc();
			if (PostprocessDesc)
			{
				SCOPED_DRAW_EVENT(Context.RHICmdList, VolumetricLightingApplyLighting);
				SCOPED_GPU_STAT(Context.RHICmdList, Stat_GPU_ApplyLighting);
				PostprocessDesc->eStereoPass = (NvVl::StereoscopicPass)View.StereoPass;
				Context.RHICmdList.ApplyLighting(DestRenderTarget.TargetableTexture, *PostprocessDesc);
			}
		}
#endif
		// NVCHANGE_END: Nvidia Volumetric Lighting

		Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
	}
}

template <typename TRHICmdList>
void FRCPassPostProcessBokehDOFRecombine::DispatchCS(TRHICmdList& RHICmdList, FRenderingCompositePassContext& Context, const FIntRect& DestRect, FUnorderedAccessViewRHIParamRef DestUAV, uint32 Method, float UVScaling)
{
	auto ShaderMap = Context.GetShaderMap();

	FIntPoint DestSize(DestRect.Width(), DestRect.Height());
	uint32 GroupSizeX = FMath::DivideAndRoundUp(DestSize.X, GBokehDOFRecombineComputeTileSizeX);
	uint32 GroupSizeY = FMath::DivideAndRoundUp(DestSize.Y, GBokehDOFRecombineComputeTileSizeY);

#define DISPATCH_CASE(A)																\
	case A:																				\
	{																					\
		TShaderMapRef<FPostProcessBokehDOFRecombineCS<A>> ComputeShader(ShaderMap);		\
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());					\
		ComputeShader->SetParameters(RHICmdList, Context, DestSize, DestUAV, UVScaling);\
		DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);	\
		ComputeShader->UnsetParameters(RHICmdList);										\
	}																					\
	break;

	switch(Method)
	{
	DISPATCH_CASE(1)
	DISPATCH_CASE(2)
	DISPATCH_CASE(3)
	DISPATCH_CASE(4)
	DISPATCH_CASE(5)
	default:
		check(0);
	}

#undef DISPATCH_CASE
}

FPooledRenderTargetDesc FRCPassPostProcessBokehDOFRecombine::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	Ret.Reset();

	Ret.TargetableFlags &= ~(TexCreate_RenderTargetable | TexCreate_UAV);
	Ret.TargetableFlags |= bIsComputePass ? TexCreate_UAV : TexCreate_RenderTargetable;

	Ret.AutoWritable = false;
	Ret.DebugName = TEXT("BokehDOFRecombine");
	Ret.Flags |= GFastVRamConfig.BokehDOF;

	return Ret;
}
