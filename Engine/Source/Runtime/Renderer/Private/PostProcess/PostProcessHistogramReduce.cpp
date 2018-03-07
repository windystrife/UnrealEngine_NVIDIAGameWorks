// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessHistogramReduce.cpp: Post processing histogram reduce implementation.
=============================================================================*/

#include "PostProcess/PostProcessHistogramReduce.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "PipelineStateCache.h"

/** Encapsulates the post processing histogram reduce compute shader. */
class FPostProcessHistogramReducePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessHistogramReducePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_A32B32G32R32F);
	}

	/** Default constructor. */
	FPostProcessHistogramReducePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter LoopSize;
	FShaderResourceParameter EyeAdaptationTexture;
	FShaderParameter EyeAdapationTemporalParams;

	/** Initialization constructor. */
	FPostProcessHistogramReducePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		LoopSize.Bind(Initializer.ParameterMap, TEXT("LoopSize"));
		EyeAdaptationTexture.Bind(Initializer.ParameterMap, TEXT("EyeAdaptationTexture"));
		EyeAdapationTemporalParams.Bind(Initializer.ParameterMap, TEXT("EyeAdapationTemporalParams"));
	}

	template <typename TRHICmdList>
	void SetPS(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, uint32 LoopSizeValue)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		SetShaderValue(RHICmdList, ShaderRHI, LoopSize, LoopSizeValue);

		if(EyeAdaptationTexture.IsBound())
		{
			if (Context.View.HasValidEyeAdaptation())
			{
				IPooledRenderTarget* EyeAdaptationRT = Context.View.GetEyeAdaptation(Context.RHICmdList);
				SetTextureParameter(RHICmdList, ShaderRHI, EyeAdaptationTexture, EyeAdaptationRT->GetRenderTargetItem().TargetableTexture);
			}
			else
			{
				// some views don't have a state, thumbnail rendering?
				SetTextureParameter(RHICmdList, ShaderRHI, EyeAdaptationTexture, GWhiteTexture->TextureRHI);
			}
		}

		// todo
		FVector4 EyeAdapationTemporalParamsValue(0, 0, 0, 0);
		SetShaderValue(RHICmdList, ShaderRHI, EyeAdapationTemporalParams, EyeAdapationTemporalParamsValue);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << LoopSize << EyeAdaptationTexture << EyeAdapationTemporalParams;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessHistogramReducePS,TEXT("/Engine/Private/PostProcessHistogramReduce.usf"),TEXT("MainPS"),SF_Pixel);

void FRCPassPostProcessHistogramReduce::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessHistogramReduce);
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

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessHistogramReducePS> PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	// we currently assume the input is half res, one full res pixel less to avoid getting bilinear filtered input
	FIntPoint GatherExtent = (View.ViewRect.Size() - FIntPoint(1, 1)) / 2;

	uint32 LoopSizeValue = ComputeLoopSize(GatherExtent);

	PixelShader->SetPS(Context.RHICmdList, Context, LoopSizeValue);

	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DestSize.X, DestSize.Y,
		0, 0,
		SrcSize.X, 0,
		DestSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

uint32 FRCPassPostProcessHistogramReduce::ComputeLoopSize(FIntPoint PixelExtent)
{
	FIntPoint ThreadGroupCount = FRCPassPostProcessHistogram::ComputeThreadGroupCount(PixelExtent);
	return ThreadGroupCount.X * ThreadGroupCount.Y;
}

FPooledRenderTargetDesc FRCPassPostProcessHistogramReduce::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc UnmodifiedRet = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	
	UnmodifiedRet.Reset();

	FIntPoint PixelExtent = UnmodifiedRet.Extent;

	// each ThreadGroup outputs one histogram
	FIntPoint NewSize = FIntPoint(FRCPassPostProcessHistogram::HistogramTexelCount, 2);

	// for quality float4 to get best quality for smooth eye adaptation transitions
	FPooledRenderTargetDesc Ret(FPooledRenderTargetDesc::Create2DDesc(NewSize, PF_A32B32G32R32F, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
	Ret.Flags |= GFastVRamConfig.HistogramReduce;
	Ret.DebugName = TEXT("HistogramReduce");
	
	return Ret;
}
