// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessDOF.cpp: Post process Depth of Field implementation.
=============================================================================*/

#include "PostProcess/PostProcessCircleDOF.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessBokehDOF.h"
#include "SceneRenderTargetParameters.h"
#include "PostProcess/PostProcessing.h"
#include "DeferredShadingRenderer.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"
#include "ScenePrivate.h"

static TAutoConsoleVariable<int32> CVarDepthOfFieldFarBlur(
	TEXT("r.DepthOfField.FarBlur"),
	1,
	TEXT("Temporary hack affecting only CircleDOF\n")
	TEXT(" 0: Off\n")
	TEXT(" 1: On (default)"),
	ECVF_RenderThreadSafe);


float ComputeFocalLengthFromFov(const FSceneView& View)
{
	// Convert FOV to focal length,
	// 
	// fov = 2 * atan(d/(2*f))
	// where,
	//   d = sensor dimension (APS-C 24.576 mm)
	//   f = focal length
	// 
	// f = 0.5 * d * (1/tan(fov/2))

	float const d = View.FinalPostProcessSettings.DepthOfFieldSensorWidth;
	float const HalfFOV = FMath::Atan(1.0f / View.ViewMatrices.GetProjectionMatrix().M[0][0]);
	float const FocalLength = 0.5f * d * (1.0f/FMath::Tan(HalfFOV));

	return FocalLength;
}

// Convert f-stop and focal distance into projected size in half resolution pixels.
// Setup depth based blur.
FVector4 CircleDofHalfCoc(const FSceneView& View)
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DepthOfFieldQuality"));
	bool bDepthOfField = View.Family->EngineShowFlags.DepthOfField && CVar->GetValueOnRenderThread() > 0;

	FVector4 Ret(0, 1, 0, 0);

	if(bDepthOfField && View.FinalPostProcessSettings.DepthOfFieldMethod == DOFM_CircleDOF)
	{
		float FocalLengthInMM = ComputeFocalLengthFromFov(View);
	 
		// Convert focal distance in world position to mm (from cm to mm)
		float FocalDistanceInMM = View.FinalPostProcessSettings.DepthOfFieldFocalDistance * 10.0f;

		// Convert f-stop, focal length, and focal distance to
		// projected circle of confusion size at infinity in mm.
		//
		// coc = f * f / (n * (d - f))
		// where,
		//   f = focal length
		//   d = focal distance
		//   n = fstop (where n is the "n" in "f/n")
		float Radius = FMath::Square(FocalLengthInMM) / (View.FinalPostProcessSettings.DepthOfFieldFstop * (FocalDistanceInMM - FocalLengthInMM));

		// Convert mm to pixels.
		float const Width = (float)View.ViewRect.Width();
		float const SensorWidth = View.FinalPostProcessSettings.DepthOfFieldSensorWidth;
		Radius = Radius * Width * (1.0f / SensorWidth);

		// Convert diameter to radius at half resolution (algorithm radius is at half resolution).
		Radius *= 0.25f;

		// Comment out for now, allowing settings which the algorithm cannot cleanly do.
		#if 0
			// Limit to algorithm max size.
			if(Radius > 6.0f) 
			{
				Radius = 6.0f; 
			}
		#endif

		// The DepthOfFieldDepthBlurAmount = km at which depth blur is 50%.
		// Need to convert to cm here.
		Ret = FVector4(
			Radius, 
			1.0f / (View.FinalPostProcessSettings.DepthOfFieldDepthBlurAmount * 100000.0f),
			View.FinalPostProcessSettings.DepthOfFieldDepthBlurRadius * Width / 1920.0f,
			Width / 1920.0f);
	}

	return Ret;
}

/** Encapsulates the Circle DOF setup pixel shader. */
template <uint32 FarBlurEnable>
class FPostProcessCircleDOFSetupPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessCircleDOFSetupPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_FAR_BLUR"), FarBlurEnable);
	}

	/** Default constructor. */
	FPostProcessCircleDOFSetupPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DepthOfFieldParams;

	/** Initialization constructor. */
	FPostProcessCircleDOFSetupPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		DepthOfFieldParams.Bind(Initializer.ParameterMap,TEXT("DepthOfFieldParams"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << DepthOfFieldParams;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Border,AM_Border,AM_Clamp>::GetRHI());

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View, MD_PostProcess);

		{
			FVector4 DepthOfFieldParamValues[2];

			FRCPassPostProcessBokehDOF::ComputeDepthOfFieldParams(Context, DepthOfFieldParamValues);

			SetShaderValueArray(Context.RHICmdList, ShaderRHI, DepthOfFieldParams, DepthOfFieldParamValues, 2);
		}
	}
};

IMPLEMENT_SHADER_TYPE(template<>,FPostProcessCircleDOFSetupPS<0>,TEXT("/Engine/Private/PostProcessCircleDOF.usf"),TEXT("CircleSetupPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessCircleDOFSetupPS<1>,TEXT("/Engine/Private/PostProcessCircleDOF.usf"),TEXT("CircleSetupPS"),SF_Pixel);

void FRCPassPostProcessCircleDOFSetup::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, CircleDOFSetup);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	const auto FeatureLevel = Context.GetFeatureLevel();
	auto ShaderMap = Context.GetShaderMap();

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect / 2;

	const FSceneRenderTargetItem& DestRenderTarget0 = PassOutputs[0].RequestSurface(Context);
	const FSceneRenderTargetItem& DestRenderTarget1 = FPostProcessing::HasAlphaChannelSupport() ? PassOutputs[1].RequestSurface(Context) : FSceneRenderTargetItem();

	// Set the view family's render target/viewport.
	FTextureRHIParamRef RenderTargets[2] =
	{
		DestRenderTarget0.TargetableTexture,
		DestRenderTarget1.TargetableTexture
	};
	uint32 NumRenderTargets = FPostProcessing::HasAlphaChannelSupport() ? 2 : 1;
	SetRenderTargets(Context.RHICmdList, NumRenderTargets, RenderTargets, FTextureRHIParamRef(), 0, NULL);

	FLinearColor ClearColors[2] = 
	{
		FLinearColor(0, 0, 0, 0),
		FLinearColor(0, 0, 0, 0)
	};
	// is optimized away if possible (RT size=view size, )
	DrawClearQuadMRT(Context.RHICmdList, true, NumRenderTargets, ClearColors, false, 0, false, 0, PassOutputs[0].RenderTargetDesc.Extent, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);

	if(CVarDepthOfFieldFarBlur.GetValueOnRenderThread())
	{
		TShaderMapRef< FPostProcessCircleDOFSetupPS<1> > PixelShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		PixelShader->SetParameters(Context);
	}
	else
	{
		TShaderMapRef< FPostProcessCircleDOFSetupPS<0> > PixelShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		PixelShader->SetParameters(Context);
	}

	VertexShader->SetParameters(Context);

	DrawPostProcessPass(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width() + 1, DestRect.Height() + 1,
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width() + 1, SrcRect.Height() + 1,
		DestSize,
		SrcSize,
		*VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget0.TargetableTexture, DestRenderTarget0.ShaderResourceTexture, false, FResolveParams());

	if (DestRenderTarget1.TargetableTexture)
	{
		Context.RHICmdList.CopyToResolveTarget(DestRenderTarget1.TargetableTexture, DestRenderTarget1.ShaderResourceTexture, false, FResolveParams());
	}
}

FPooledRenderTargetDesc FRCPassPostProcessCircleDOFSetup::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	//	Ret.Extent = FIntPoint::DivideAndRoundUp(ret.Extent, 2);
	Ret.Extent /= 2;
	Ret.Extent.X = FMath::Max(1, Ret.Extent.X);
	Ret.Extent.Y = FMath::Max(1, Ret.Extent.Y);

	Ret.Reset();
	Ret.TargetableFlags &= ~(uint32)TexCreate_UAV;
	Ret.TargetableFlags |= TexCreate_RenderTargetable;
	Ret.AutoWritable = false;
	Ret.Flags |= GFastVRamConfig.CircleDOF;

	if (FPostProcessing::HasAlphaChannelSupport())
	{
		if (InPassOutputId == ePId_Output0)
		{
			Ret.DebugName = TEXT("CircleDOFSceneColorSetup");
		}
		else if (InPassOutputId == ePId_Output1)
		{
			Ret.DebugName = TEXT("CircleDOFSetup0");
			Ret.Format = PF_R32_FLOAT;
		}
	}
	else
	{
		Ret.DebugName = (InPassOutputId == ePId_Output0) ? TEXT("CircleDOFSetup0") : TEXT("CircleDOFSetup1");

		// more precision for additive blending and we need the alpha channel
		Ret.Format = PF_FloatRGBA;
	}

	return Ret;
}




/** Encapsulates the Circle DOF Dilate pixel shader. */
template <uint32 NearBlurEnable>
class FPostProcessCircleDOFDilatePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessCircleDOFDilatePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_NEAR_BLUR"), NearBlurEnable);
	}

	/** Default constructor. */
	FPostProcessCircleDOFDilatePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DepthOfFieldParams;

	/** Initialization constructor. */
	FPostProcessCircleDOFDilatePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		DepthOfFieldParams.Bind(Initializer.ParameterMap,TEXT("DepthOfFieldParams"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << DepthOfFieldParams;
		return bShaderHasOutdatedParameters;
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Border, AM_Border, AM_Clamp>::GetRHI());

		DeferredParameters.Set(RHICmdList, ShaderRHI, Context.View, MD_PostProcess);

		{
			FVector4 DepthOfFieldParamValues[2];

			FRCPassPostProcessBokehDOF::ComputeDepthOfFieldParams(Context, DepthOfFieldParamValues);

			SetShaderValueArray(RHICmdList, ShaderRHI, DepthOfFieldParams, DepthOfFieldParamValues, 2);
		}
	}
};

IMPLEMENT_SHADER_TYPE(template<>,FPostProcessCircleDOFDilatePS<0>,TEXT("/Engine/Private/PostProcessCircleDOF.usf"),TEXT("CircleDilatePS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessCircleDOFDilatePS<1>,TEXT("/Engine/Private/PostProcessCircleDOF.usf"),TEXT("CircleDilatePS"),SF_Pixel);

void FRCPassPostProcessCircleDOFDilate::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, CircleDOFNear);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	uint32 NumRenderTargets = 1;

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	const auto FeatureLevel = Context.GetFeatureLevel();
	auto ShaderMap = Context.GetShaderMap();

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect / 2;

	const FSceneRenderTargetItem& DestRenderTarget0 = PassOutputs[0].RequestSurface(Context);
	const FSceneRenderTargetItem& DestRenderTarget1 = FSceneRenderTargetItem();

	// Set the view family's render target/viewport.
	FTextureRHIParamRef RenderTargets[2] =
	{
		DestRenderTarget0.TargetableTexture,
		DestRenderTarget1.TargetableTexture
	};
	SetRenderTargets(Context.RHICmdList, NumRenderTargets, RenderTargets, FTextureRHIParamRef(), 0, nullptr);

	FLinearColor ClearColors[2] = 
	{
		FLinearColor(0, 0, 0, 0),
		FLinearColor(0, 0, 0, 0)
	};
	// is optimized away if possible (RT size=view size, )
	DrawClearQuadMRT(Context.RHICmdList, true, NumRenderTargets, ClearColors, false, 0, false, 0, PassOutputs[0].RenderTargetDesc.Extent, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);

	if (false)
	{
		TShaderMapRef< FPostProcessCircleDOFDilatePS<1> > PixelShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);
		PixelShader->SetParameters(Context.RHICmdList, Context);
	}
	else
	{
		TShaderMapRef< FPostProcessCircleDOFDilatePS<0> > PixelShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		PixelShader->SetParameters(Context.RHICmdList, Context);
	}

	VertexShader->SetParameters(Context);

	DrawPostProcessPass(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width() + 1, DestRect.Height() + 1,
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width() + 1, SrcRect.Height() + 1,
		DestSize,
		SrcSize,
		*VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);
	
	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget0.TargetableTexture, DestRenderTarget0.ShaderResourceTexture, false, FResolveParams());

	if (DestRenderTarget1.TargetableTexture)
	{
		Context.RHICmdList.CopyToResolveTarget(DestRenderTarget1.TargetableTexture, DestRenderTarget1.ShaderResourceTexture, false, FResolveParams());
	}
}

FPooledRenderTargetDesc FRCPassPostProcessCircleDOFDilate::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	//	Ret.Extent = FIntPoint::DivideAndRoundUp(ret.Extent, 2);
	Ret.Extent /= 2;
	Ret.Extent.X = FMath::Max(1, Ret.Extent.X);
	Ret.Extent.Y = FMath::Max(1, Ret.Extent.Y);

	Ret.Reset();
	Ret.TargetableFlags &= ~(uint32)TexCreate_UAV;
	Ret.TargetableFlags |= TexCreate_RenderTargetable;

	Ret.DebugName = (InPassOutputId == ePId_Output0) ? TEXT("CircleDOFDilate0") : TEXT("CircleDOFDilate1");

//	Ret.Format = PF_FloatRGBA;
	// we only use one channel, maybe using 4 channels would save memory as we reuse
	Ret.Format = PF_R16F;
	Ret.Flags |= GFastVRamConfig.CircleDOF;

	return Ret;
}









/** Encapsulates the Circle DOF pixel shader. */

static float TemporalHalton2( int32 Index, int32 Base )
{
	float Result = 0.0f;
	float InvBase = 1.0f / Base;
	float Fraction = InvBase;
	while( Index > 0 )
	{
		Result += ( Index % Base ) * Fraction;
		Index /= Base;
		Fraction *= InvBase;
	}
	return Result;
}

static void TemporalRandom2(FVector2D* RESTRICT const Constant, uint32 FrameNumber)
{
	Constant->X = TemporalHalton2(FrameNumber & 1023, 2);
	Constant->Y = TemporalHalton2(FrameNumber & 1023, 3);
}

template <uint32 Quality>
class FPostProcessCircleDOFPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessCircleDOFPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("QUALITY"), Quality);
	}

	/** Default constructor. */
	FPostProcessCircleDOFPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DepthOfFieldParams;
	FShaderParameter RandomOffset;

	/** Initialization constructor. */
	FPostProcessCircleDOFPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		DepthOfFieldParams.Bind(Initializer.ParameterMap,TEXT("DepthOfFieldParams"));
		RandomOffset.Bind(Initializer.ParameterMap, TEXT("RandomOffset"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << DepthOfFieldParams << RandomOffset;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Border,AM_Border,AM_Clamp>::GetRHI());
/*
		{
			FSamplerStateRHIParamRef Filters[] =
			{
				TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			};

			PostprocessParameter.SetPS( ShaderRHI, Context, 0, false, Filters );
		}
*/

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View, MD_PostProcess);

		{
			FVector4 DepthOfFieldParamValues[2];

			FRCPassPostProcessBokehDOF::ComputeDepthOfFieldParams(Context, DepthOfFieldParamValues);

			SetShaderValueArray(Context.RHICmdList, ShaderRHI, DepthOfFieldParams, DepthOfFieldParamValues, 2);
		}

		FVector2D RandomOffsetValue;
		TemporalRandom2(&RandomOffsetValue, Context.View.Family->FrameNumber);
		SetShaderValue(Context.RHICmdList, ShaderRHI, RandomOffset, RandomOffsetValue);
	}
	
	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/PostProcessCircleDOF.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("CirclePS");
	}
};


// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessCircleDOFPS<A> FPostProcessCircleDOFPS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessCircleDOFPS##A, SF_Pixel);

VARIATION1(0)
VARIATION1(1)
VARIATION1(2)


#undef VARIATION1

template <uint32 Quality>
FShader* FRCPassPostProcessCircleDOF::SetShaderTempl(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessCircleDOFPS<Quality> > PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetParameters(Context);
	PixelShader->SetParameters(Context);
	
	return *VertexShader;
}

void FRCPassPostProcessCircleDOF::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, CircleDOFApply);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	const auto FeatureLevel = Context.GetFeatureLevel();
	auto ShaderMap = Context.GetShaderMap();

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget0 = PassOutputs[0].RequestSurface(Context);
	const FSceneRenderTargetItem& DestRenderTarget1 = FPostProcessing::HasAlphaChannelSupport() ? PassOutputs[1].RequestSurface(Context) : FSceneRenderTargetItem();

	// Set the view family's render target/viewport.
	FTextureRHIParamRef RenderTargets[2] =
	{
		DestRenderTarget0.TargetableTexture,
		DestRenderTarget1.TargetableTexture
	};
	uint32 NumRenderTargets = FPostProcessing::HasAlphaChannelSupport() ? 2 : 1;
	SetRenderTargets(Context.RHICmdList, NumRenderTargets, RenderTargets, FTextureRHIParamRef(), 0, NULL);

	FLinearColor ClearColors[2] = 
	{
		FLinearColor(0, 0, 0, 0),
		FLinearColor(0, 0, 0, 0)
	};

	// is optimized away if possible (RT size=view size, )
	DrawClearQuadMRT(Context.RHICmdList, true, NumRenderTargets, ClearColors, false, 0, false, 0, PassOutputs[0].RenderTargetDesc.Extent, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );
		
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DepthOfFieldQuality"));
	check(CVar);
	int32 DOFQualityCVarValue = CVar->GetValueOnRenderThread();
	
	

	FShader* VertexShader = 0;
	
	switch (DOFQualityCVarValue)
	{
		case 3:  VertexShader = SetShaderTempl<1>(Context); break;
		case 4:  VertexShader = SetShaderTempl<2>(Context); break;	
		default: VertexShader = SetShaderTempl<0>(Context);
	}

	DrawPostProcessPass(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width() + 1, DestRect.Height() + 1,
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width() + 1, SrcRect.Height() + 1,
		DestSize,
		SrcSize,
		VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget0.TargetableTexture, DestRenderTarget0.ShaderResourceTexture, false, FResolveParams());

	if (DestRenderTarget1.TargetableTexture)
	{
		Context.RHICmdList.CopyToResolveTarget(DestRenderTarget1.TargetableTexture, DestRenderTarget1.ShaderResourceTexture, false, FResolveParams());
	}
}

FPooledRenderTargetDesc FRCPassPostProcessCircleDOF::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Extent.X = FMath::Max(1, Ret.Extent.X);
	Ret.Extent.Y = FMath::Max(1, Ret.Extent.Y);

	Ret.Reset();
	Ret.TargetableFlags &= ~(uint32)TexCreate_UAV;
	Ret.TargetableFlags |= TexCreate_RenderTargetable;
	Ret.Flags |= GFastVRamConfig.CircleDOF;

	if (FPostProcessing::HasAlphaChannelSupport())
	{
		if (InPassOutputId == 0)
		{
			Ret.DebugName = TEXT("CircleDOFSceneColor");
		}
		else if (InPassOutputId == 1)
		{
			Ret.DebugName = TEXT("CircleDOFCoc");
			Ret.Format = GetInput(ePId_Input1)->GetOutput()->RenderTargetDesc.Format;
		}
	}
	else
	{
		Ret.DebugName = (InPassOutputId == ePId_Output0) ? TEXT("CircleDOF0") : TEXT("CircleDOF1");

		// more precision for additive blending and we need the alpha channel
		Ret.Format = PF_FloatRGBA;
	}

	return Ret;
}


/** Encapsulates  the Circle DOF recombine pixel shader. */
template <uint32 Quality>
class FPostProcessCircleDOFRecombinePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessCircleDOFRecombinePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("QUALITY"), Quality);
	}

	/** Default constructor. */
	FPostProcessCircleDOFRecombinePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DepthOfFieldUVLimit;
	FShaderParameter RandomOffset;

	/** Initialization constructor. */
	FPostProcessCircleDOFRecombinePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		DepthOfFieldUVLimit.Bind(Initializer.ParameterMap,TEXT("DepthOfFieldUVLimit"));
		RandomOffset.Bind(Initializer.ParameterMap, TEXT("RandomOffset"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << DepthOfFieldUVLimit << RandomOffset;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View, MD_PostProcess);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		// Compute out of bounds UVs in the source texture.
		FVector4 Bounds;
		Bounds.X = (((float)((Context.View.ViewRect.Min.X + 1) & (~1))) + 3.0f) / ((float)(SceneContext.GetBufferSizeXY().X));
		Bounds.Y = (((float)((Context.View.ViewRect.Min.Y + 1) & (~1))) + 3.0f) / ((float)(SceneContext.GetBufferSizeXY().Y));
		Bounds.Z = (((float)(Context.View.ViewRect.Max.X & (~1))) - 3.0f) / ((float)(SceneContext.GetBufferSizeXY().X));
		Bounds.W = (((float)(Context.View.ViewRect.Max.Y & (~1))) - 3.0f) / ((float)(SceneContext.GetBufferSizeXY().Y));

		SetShaderValue(Context.RHICmdList, ShaderRHI, DepthOfFieldUVLimit, Bounds);

		FVector2D RandomOffsetValue;
		TemporalRandom2(&RandomOffsetValue, Context.View.Family->FrameNumber);
		SetShaderValue(Context.RHICmdList, ShaderRHI, RandomOffset, RandomOffsetValue);
	}
	
	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/PostProcessCircleDOF.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainCircleRecombinePS");
	}
};


// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessCircleDOFRecombinePS<A> FPostProcessCircleDOFRecombinePS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessCircleDOFRecombinePS##A, SF_Pixel);

	VARIATION1(0)
	VARIATION1(1)

#undef VARIATION1

template <uint32 Quality>
FShader* FRCPassPostProcessCircleDOFRecombine::SetShaderTempl(const FRenderingCompositePassContext& Context)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessCircleDOFRecombinePS<Quality> > PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetParameters(Context);
	PixelShader->SetParameters(Context);
	
	return *VertexShader;
}

void FRCPassPostProcessCircleDOFRecombine::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, CircleDOFRecombine);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;

	const auto FeatureLevel = Context.GetFeatureLevel();
	auto ShaderMap = Context.GetShaderMap();

	FIntPoint TexSize = InputDesc->Extent;

	// usually 1, 2, 4 or 8
	uint32 ScaleToFullRes = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / TexSize.X;

	FIntRect HalfResViewRect = View.ViewRect / ScaleToFullRes;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	DrawClearQuad(Context.RHICmdList, true, FLinearColor::Black, false, 0, false, 0, PassOutputs[0].RenderTargetDesc.Extent, View.ViewRect);

	Context.SetViewportAndCallRHI(View.ViewRect);

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DepthOfFieldQuality"));
	check(CVar);
	int32 DOFQualityCVarValue = CVar->GetValueOnRenderThread();
	
	// 0:normal / 1:slow but very high quality
	uint32 Quality = DOFQualityCVarValue >= 3;

	FShader* VertexShader = 0;

	if (Quality)
		VertexShader = SetShaderTempl<1>(Context);
	else
		VertexShader = SetShaderTempl<0>(Context);

	DrawPostProcessPass(
		Context.RHICmdList,
		0, 0,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Min.X, View.ViewRect.Min.Y,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Size(),
		TexSize,
		VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessCircleDOFRecombine::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("CircleDOFRecombine");

	return Ret;
}
