// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessAA.cpp: Post processing anti aliasing implementation.
=============================================================================*/

#include "PostProcess/PostProcessAA.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneFilterRendering.h"
#include "SceneRendering.h"
#include "PipelineStateCache.h"

/** Encapsulates the post processing anti aliasing pixel shader. */
// Quality 1..6
template<uint32 Quality>
class FPostProcessAntiAliasingPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessAntiAliasingPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("FXAA_PRESET"), Quality - 1);
	}

	/** Default constructor. */
	FPostProcessAntiAliasingPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter fxaaQualityRcpFrame;
	FShaderParameter fxaaConsoleRcpFrameOpt;
	FShaderParameter fxaaConsoleRcpFrameOpt2;
	FShaderParameter fxaaQualitySubpix;
	FShaderParameter fxaaQualityEdgeThreshold;
	FShaderParameter fxaaQualityEdgeThresholdMin;
	FShaderParameter fxaaConsoleEdgeSharpness;
	FShaderParameter fxaaConsoleEdgeThreshold;
	FShaderParameter fxaaConsoleEdgeThresholdMin;

	/** Initialization constructor. */
	FPostProcessAntiAliasingPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		fxaaQualityRcpFrame.Bind(Initializer.ParameterMap, TEXT("fxaaQualityRcpFrame"));
		fxaaConsoleRcpFrameOpt.Bind(Initializer.ParameterMap, TEXT("fxaaConsoleRcpFrameOpt"));
		fxaaConsoleRcpFrameOpt2.Bind(Initializer.ParameterMap, TEXT("fxaaConsoleRcpFrameOpt2"));
		fxaaQualitySubpix.Bind(Initializer.ParameterMap, TEXT("fxaaQualitySubpix"));
		fxaaQualityEdgeThreshold.Bind(Initializer.ParameterMap, TEXT("fxaaQualityEdgeThreshold"));
		fxaaQualityEdgeThresholdMin.Bind(Initializer.ParameterMap, TEXT("fxaaQualityEdgeThresholdMin"));
		fxaaConsoleEdgeSharpness.Bind(Initializer.ParameterMap, TEXT("fxaaConsoleEdgeSharpness"));
		fxaaConsoleEdgeThreshold.Bind(Initializer.ParameterMap, TEXT("fxaaConsoleEdgeThreshold"));
		fxaaConsoleEdgeThresholdMin.Bind(Initializer.ParameterMap, TEXT("fxaaConsoleEdgeThresholdMin"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << fxaaQualityRcpFrame
			<< fxaaConsoleRcpFrameOpt << fxaaConsoleRcpFrameOpt2
			<< fxaaQualitySubpix << fxaaQualityEdgeThreshold << fxaaQualityEdgeThresholdMin
			<< fxaaConsoleEdgeSharpness << fxaaConsoleEdgeThreshold << fxaaConsoleEdgeThresholdMin;
		return bShaderHasOutdatedParameters;
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		const FSceneView& View = Context.View;

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		
		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		const FPooledRenderTargetDesc* InputDesc = Context.Pass->GetInputDesc(ePId_Input0);

		if(!InputDesc)
		{
			// input is not hooked up correctly
			return;
		}

		FVector2D InvExtent = FVector2D(1.0f / InputDesc->Extent.X, 1.0f / InputDesc->Extent.Y);

		SetShaderValue(RHICmdList, ShaderRHI, fxaaQualityRcpFrame, InvExtent);

		{
			float N = 0.5f;
			FVector4 Value(-N * InvExtent.X, -N * InvExtent.Y, N * InvExtent.X, N * InvExtent.Y);
			SetShaderValue(RHICmdList, ShaderRHI, fxaaConsoleRcpFrameOpt, Value);
		}

		{
			float N = 2.0f;
			FVector4 Value(-N * InvExtent.X, -N * InvExtent.Y, N * InvExtent.X, N * InvExtent.Y);
			SetShaderValue(RHICmdList, ShaderRHI, fxaaConsoleRcpFrameOpt2, Value);
		}

		{
			float Value = 0.75f;
			SetShaderValue(RHICmdList, ShaderRHI, fxaaQualitySubpix, Value);
		}

		{
			float Value = 0.166f;
			SetShaderValue(RHICmdList, ShaderRHI, fxaaQualityEdgeThreshold, Value);
		}

		{
			float Value = 0.0833f;
			SetShaderValue(RHICmdList, ShaderRHI, fxaaQualityEdgeThresholdMin, Value);
		}

		{
			float Value = 8.0f;
			SetShaderValue(RHICmdList, ShaderRHI, fxaaConsoleEdgeSharpness, Value);
		}

		{
			float Value = 0.125f;
			SetShaderValue(RHICmdList, ShaderRHI, fxaaConsoleEdgeThreshold, Value);
		}

		{
			float Value = 0.05f;
			SetShaderValue(RHICmdList, ShaderRHI, fxaaConsoleEdgeThresholdMin, Value);
		}
	}
	
	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/FXAAShader.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("FxaaPS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessAntiAliasingPS<A> FPostProcessAntiAliasingPS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessAntiAliasingPS##A, SF_Pixel);

	VARIATION1(1)			VARIATION1(2)			VARIATION1(3)			VARIATION1(4)			VARIATION1(5)			VARIATION1(6)
#undef VARIATION1

/*-----------------------------------------------------------------------------
	FFXAAVertexShader
-----------------------------------------------------------------------------*/
class FFXAAVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FFXAAVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FFXAAVS() {}
	
	/** Initialization constructor. */
	FFXAAVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		fxaaQualityRcpFrame.Bind(Initializer.ParameterMap, TEXT("fxaaQualityRcpFrame"));
	}

	/** Serializer */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

		Ar << fxaaQualityRcpFrame;

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

		FVector2D InvExtent = FVector2D(1.0f / InputDesc->Extent.X, 1.0f / InputDesc->Extent.Y);

		SetShaderValue(Context.RHICmdList, ShaderRHI, fxaaQualityRcpFrame, InvExtent);
	}

	FShaderParameter fxaaQualityRcpFrame;
};

IMPLEMENT_SHADER_TYPE(,FFXAAVS,TEXT("/Engine/Private/FXAAShader.usf"),TEXT("FxaaVS"),SF_Vertex);


// @param Quality 1..6
template <uint32 Quality, typename TRHICmdList>
static void SetShaderTemplAA(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
{
	static_assert(Quality >= 1 && Quality <= 6, "Quality should be 1..6!");
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<FFXAAVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessAntiAliasingPS<Quality> > PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
	
	PixelShader->SetParameters(RHICmdList, Context);
	VertexShader->SetParameters(Context);
}

void FRCPassPostProcessAA::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessAA);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = View.ViewRect;
	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	switch(Quality)
	{
		case 1:  SetShaderTemplAA<1>(Context.RHICmdList, Context); break;
		case 2:  SetShaderTemplAA<2>(Context.RHICmdList, Context); break;
		case 3:  SetShaderTemplAA<3>(Context.RHICmdList, Context); break;
		case 4:  SetShaderTemplAA<4>(Context.RHICmdList, Context); break;
		case 5:  SetShaderTemplAA<5>(Context.RHICmdList, Context); break;
		default: SetShaderTemplAA<6>(Context.RHICmdList, Context); break;
	}
	
	TShaderMapRef<FFXAAVS> VertexShader(Context.GetShaderMap());

	DrawPostProcessPass(Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestSize,
		SrcSize,
		*VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_Default);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessAA::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("PostProcessAA");

	return Ret;
}
