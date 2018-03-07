// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenSpaceReflections.cpp: Post processing Screen Space Reflections implementation.
=============================================================================*/

#include "PostProcess/ScreenSpaceReflections.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneRenderTargetParameters.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessInput.h"
#include "PostProcess/PostProcessOutput.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessTemporalAA.h"
#include "PostProcess/PostProcessHierarchical.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"

static TAutoConsoleVariable<int32> CVarSSRQuality(
	TEXT("r.SSR.Quality"),
	3,
	TEXT("Whether to use screen space reflections and at what quality setting.\n")
	TEXT("(limits the setting in the post process settings which has a different scale)\n")
	TEXT("(costs performance, adds more visual realism but the technique has limits)\n")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: low (no glossy)\n")
	TEXT(" 2: medium (no glossy)\n")
	TEXT(" 3: high (glossy/using roughness, few samples)\n")
	TEXT(" 4: very high (likely too slow for real-time)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSSRTemporal(
	TEXT("r.SSR.Temporal"),
	0,
	TEXT("Defines if we use the temporal smoothing for the screen space reflection\n")
	TEXT(" 0 is off (for debugging), 1 is on (default)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSSRStencil(
	TEXT("r.SSR.Stencil"),
	0,
	TEXT("Defines if we use the stencil prepass for the screen space reflection\n")
	TEXT(" 0 is off (default), 1 is on"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSSRCone(
	TEXT("r.SSR.Cone"),
	0,
	TEXT("Defines if we use cone traced screen space reflection\n")
	TEXT(" 0 is off (default), 1 is on"),
	ECVF_RenderThreadSafe);

// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
static TAutoConsoleVariable<int32> CVarCombineVXGISpecularWithSSR(
	TEXT("r.VXGI.CombineSpecularWithSSR"),
	0,
	TEXT("Defines if VXGI specular tracing fills is combined with SSR or replaces it\n")
	TEXT(" 0 is replace, 1 is combine"),
	ECVF_RenderThreadSafe);
#endif
// NVCHANGE_END: Add VXGI

DECLARE_FLOAT_COUNTER_STAT(TEXT("ScreenSpace Reflections"), Stat_GPU_ScreenSpaceReflections, STATGROUP_GPU);

bool ShouldRenderScreenSpaceReflections(const FViewInfo& View)
{
	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	if (View.FinalPostProcessSettings.VxgiSpecularTracingEnabled && !CVarCombineVXGISpecularWithSSR.GetValueOnRenderThread())
	{
		return false;
	}
#endif
	// NVCHANGE_END: Add VXGI

	if(!View.Family->EngineShowFlags.ScreenSpaceReflections)
	{
		return false;
	}

	if(!View.State)
	{
		// not view state (e.g. thumbnail rendering?), no HZB (no screen space reflections or occlusion culling)
		return false;
	}

	int SSRQuality = CVarSSRQuality.GetValueOnRenderThread();

	if(SSRQuality <= 0)
	{
		return false;
	}

	if(View.FinalPostProcessSettings.ScreenSpaceReflectionIntensity < 1.0f)
	{
		return false;
	}

	if (IsAnyForwardShadingEnabled(View.GetShaderPlatform()))
	{
		return false;
	}

	return true;
}

bool IsSSRTemporalPassRequired(const FViewInfo& View, bool bCheckSSREnabled)
{
	if (bCheckSSREnabled && !ShouldRenderScreenSpaceReflections(View))
	{
		return false;
	}
	if (!View.State)
	{
		return false;
	}
	return View.AntiAliasingMethod != AAM_TemporalAA || CVarSSRTemporal.GetValueOnRenderThread() != 0;
}


static float ComputeRoughnessMaskScale(const FRenderingCompositePassContext& Context, uint32 SSRQuality)
{
	float MaxRoughness = FMath::Clamp(Context.View.FinalPostProcessSettings.ScreenSpaceReflectionMaxRoughness, 0.01f, 1.0f);

	// f(x) = x * Scale + Bias
	// f(MaxRoughness) = 0
	// f(MaxRoughness/2) = 1

	float RoughnessMaskScale = -2.0f / MaxRoughness;
	return RoughnessMaskScale * (SSRQuality < 3 ? 2.0f : 1.0f);
}

FLinearColor ComputeSSRParams(const FRenderingCompositePassContext& Context, uint32 SSRQuality, bool bEnableDiscard)
{
	float RoughnessMaskScale = ComputeRoughnessMaskScale(Context, SSRQuality);

	float FrameRandom = 0;

	if(Context.ViewState)
	{
		bool bTemporalAAIsOn = Context.View.AntiAliasingMethod == AAM_TemporalAA;

		if(bTemporalAAIsOn)
		{
			// usually this number is in the 0..7 range but it depends on the TemporalAA quality
			FrameRandom = Context.ViewState->GetCurrentTemporalAASampleIndex() * 1551;
		}
		else
		{
			// 8 aligns with the temporal smoothing, larger number will do more flickering (power of two for best performance)
			FrameRandom = Context.ViewState->GetFrameIndexMod8() * 1551;
		}
	}

	return FLinearColor(
		FMath::Clamp(Context.View.FinalPostProcessSettings.ScreenSpaceReflectionIntensity * 0.01f, 0.0f, 1.0f), 
		RoughnessMaskScale,
		(float)bEnableDiscard,	// TODO 
		FrameRandom);
}

/**
 * Encapsulates the post processing screen space reflections pixel shader stencil pass.
 */
class FPostProcessScreenSpaceReflectionsStencilPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessScreenSpaceReflectionsStencilPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine( TEXT("PREV_FRAME_COLOR"), uint32(0) );
		OutEnvironment.SetDefine( TEXT("SSR_QUALITY"), uint32(0) );
	}

	/** Default constructor. */
	FPostProcessScreenSpaceReflectionsStencilPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter SSRParams;

	/** Initialization constructor. */
	FPostProcessScreenSpaceReflectionsStencilPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		SSRParams.Bind(Initializer.ParameterMap, TEXT("SSRParams"));
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, uint32 SSRQuality, bool EnableDiscard)
	{
		const FFinalPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		DeferredParameters.Set(RHICmdList, ShaderRHI, Context.View, MD_PostProcess);

		{
			FLinearColor Value = ComputeSSRParams(Context, SSRQuality, EnableDiscard);

			SetShaderValue(RHICmdList, ShaderRHI, SSRParams, Value);
		}
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << SSRParams;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessScreenSpaceReflectionsStencilPS,TEXT("/Engine/Private/ScreenSpaceReflections.usf"),TEXT("ScreenSpaceReflectionsStencilPS"),SF_Pixel);
static const uint32 SSRConeQuality = 5;

/**
 * Encapsulates the post processing screen space reflections pixel shader.
 * @param SSRQuality 0:Visualize Mask
 */
template<uint32 PrevFrame, uint32 SSRQuality >
class FPostProcessScreenSpaceReflectionsPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessScreenSpaceReflectionsPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine( TEXT("PREV_FRAME_COLOR"), PrevFrame );
		OutEnvironment.SetDefine( TEXT("SSR_QUALITY"), SSRQuality );
		OutEnvironment.SetDefine( TEXT("SSR_CONE_QUALITY"), SSRConeQuality );
	}

	/** Default constructor. */
	FPostProcessScreenSpaceReflectionsPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter SSRParams;
	FShaderParameter HZBUvFactorAndInvFactor;

	/** Initialization constructor. */
	FPostProcessScreenSpaceReflectionsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		SSRParams.Bind(Initializer.ParameterMap, TEXT("SSRParams"));
		HZBUvFactorAndInvFactor.Bind(Initializer.ParameterMap, TEXT("HZBUvFactorAndInvFactor"));
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
	{
		const FFinalPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		DeferredParameters.Set(RHICmdList, ShaderRHI, Context.View, MD_PostProcess);

		{
			FLinearColor Value = ComputeSSRParams(Context, SSRQuality, false);

			SetShaderValue(RHICmdList, ShaderRHI, SSRParams, Value);
		}

		{
			const FVector2D HZBUvFactor(
				float(Context.View.ViewRect.Width()) / float(2 * Context.View.HZBMipmap0Size.X),
				float(Context.View.ViewRect.Height()) / float(2 * Context.View.HZBMipmap0Size.Y)
				);
			const FVector4 HZBUvFactorAndInvFactorValue(
				HZBUvFactor.X,
				HZBUvFactor.Y,
				1.0f / HZBUvFactor.X,
				1.0f / HZBUvFactor.Y
				);
			
			SetShaderValue(RHICmdList, ShaderRHI, HZBUvFactorAndInvFactor, HZBUvFactorAndInvFactorValue);
		}
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << SSRParams << HZBUvFactorAndInvFactor;
		return bShaderHasOutdatedParameters;
	}
};

// Typedef is necessary because the C preprocessor thinks the comma in the template parameter list is a comma in the macro parameter list.
#define IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(A, B) \
	typedef FPostProcessScreenSpaceReflectionsPS<A,B> FPostProcessScreenSpaceReflectionsPS##A##B; \
	IMPLEMENT_SHADER_TYPE(template<>,FPostProcessScreenSpaceReflectionsPS##A##B,TEXT("/Engine/Private/ScreenSpaceReflections.usf"),TEXT("ScreenSpaceReflectionsPS"),SF_Pixel)

IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(0,0);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(0,1);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(1,1);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(0,2);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(1,2);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(0,3);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(1,3);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(0,4);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(1,4);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(0,5); // SSRConeQuality
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(1,5); // SSRConeQuality

// --------------------------------------------------------


// @param Quality usually in 0..100 range, default is 50
// @return see CVarSSRQuality, never 0
static int32 ComputeSSRQuality(float Quality)
{
	int32 Ret;

	if(Quality >= 60.0f)
	{
		Ret = (Quality >= 80.0f) ? 4 : 3;
	}
	else
	{
		Ret = (Quality >= 40.0f) ? 2 : 1;
	}

	int SSRQualityCVar = FMath::Max(0, CVarSSRQuality.GetValueOnRenderThread());

	return FMath::Min(Ret, SSRQualityCVar);
}

void FRCPassPostProcessScreenSpaceReflections::Process(FRenderingCompositePassContext& Context)
{
	auto& RHICmdList = Context.RHICmdList;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	const FSceneView& View = Context.View;
	const auto FeatureLevel = Context.GetFeatureLevel();

	int32 SSRQuality = ComputeSSRQuality(View.FinalPostProcessSettings.ScreenSpaceReflectionQuality);
	uint32 iPreFrame = bPrevFrame ? 1 : 0;

	SSRQuality = FMath::Clamp(SSRQuality, 1, 4);
	
	const bool VisualizeSSR = View.Family->EngineShowFlags.VisualizeSSR;
	const bool SSRStencilPrePass = CVarSSRStencil.GetValueOnRenderThread() != 0 && !VisualizeSSR;

	FRenderingCompositeOutputRef* Input2 = GetInput(ePId_Input2);

	const bool SSRConeTracing = Input2 && Input2->GetOutput();
	
	if (VisualizeSSR)
	{
		iPreFrame = 0;
		SSRQuality = 0;
	}
	else if (SSRConeTracing)
	{
		SSRQuality = SSRConeQuality;
	}
	
	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_ScreenSpaceReflections);
	
	if (SSRStencilPrePass)
	{ // ScreenSpaceReflectionsStencil draw event
		SCOPED_DRAW_EVENTF(Context.RHICmdList, ScreenSpaceReflectionsStencil, TEXT("ScreenSpaceReflectionsStencil %dx%d"), View.ViewRect.Width(), View.ViewRect.Height());

		TShaderMapRef< FPostProcessVS > VertexShader(Context.GetShaderMap());
		TShaderMapRef< FPostProcessScreenSpaceReflectionsStencilPS > PixelShader(Context.GetShaderMap());
		
		// bind the dest render target and the depth stencil render target
		SetRenderTarget(RHICmdList, DestRenderTarget.TargetableTexture, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EUninitializedColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);
		Context.SetViewportAndCallRHI(View.ViewRect);

		// Clear stencil to 0
		DrawClearQuad(RHICmdList, false, FLinearColor(), false, 0, true, 0, PassOutputs[0].RenderTargetDesc.Extent, View.ViewRect);
	
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		// Clobers the stencil to pixel that should not compute SSR
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always, true, CF_Always, SO_Replace, SO_Replace, SO_Replace>::GetRHI();

		// Set rasterizer state to solid
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();

		// disable blend mode
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

		// bind shader
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);
		RHICmdList.SetStencilRef(0x80);

		VertexShader->SetParameters(Context);
		PixelShader->SetParameters(Context.RHICmdList, Context, SSRQuality, true);
	
		DrawPostProcessPass(
			Context.RHICmdList,
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Size(),
			SceneContext.GetBufferSizeXY(),
			*VertexShader,
			View.StereoPass,
			Context.HasHmdMesh(),
			EDRF_UseTriangleOptimization);

	} // ScreenSpaceReflectionsStencil draw event

	{ // ScreenSpaceReflections draw event
		SCOPED_DRAW_EVENTF(Context.RHICmdList, ScreenSpaceReflections, TEXT("ScreenSpaceReflections %dx%d"), View.ViewRect.Width(), View.ViewRect.Height());

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		if (SSRStencilPrePass)
		{
			// set up the stencil test to match 0, meaning FPostProcessScreenSpaceReflectionsStencilPS has been discarded
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep>::GetRHI();
		}
		else
		{
			// bind only the dest render target
			SetRenderTarget(RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
			Context.SetViewportAndCallRHI(View.ViewRect);

			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		}
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		// clear DestRenderTarget only outside of the view's rectangle
		DrawClearQuad(RHICmdList, true, FLinearColor::Black, false, 0, false, 0, PassOutputs[0].RenderTargetDesc.Extent, View.ViewRect);

		// set the state
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		TShaderMapRef< FPostProcessVS > VertexShader(Context.GetShaderMap());

		#define CASE(A, B) \
			case (A + 2 * (B + 3 * 0 )): \
			{ \
				TShaderMapRef< FPostProcessScreenSpaceReflectionsPS<A, B> > PixelShader(Context.GetShaderMap()); \
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI; \
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader); \
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader); \
				SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit); \
				VertexShader->SetParameters(Context); \
				PixelShader->SetParameters(RHICmdList, Context); \
			}; \
			break

		switch (iPreFrame + 2 * (SSRQuality + 3 * 0))
		{
			CASE(0,0);
			CASE(0,1);	CASE(1,1);
			CASE(0,2);	CASE(1,2);
			CASE(0,3);	CASE(1,3);
			CASE(0,4);	CASE(1,4);
			CASE(0,5);	CASE(1,5); //SSRConeQuality
			default:
				check(!"Missing case in FRCPassPostProcessScreenSpaceReflections");
		}
		#undef CASE

		DrawPostProcessPass(
			RHICmdList,
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Size(),
			FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY(),
			*VertexShader,
			View.StereoPass,
			Context.HasHmdMesh(),
			EDRF_UseTriangleOptimization);
	
		RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
	} // ScreenSpaceReflections
}

FPooledRenderTargetDesc FRCPassPostProcessScreenSpaceReflections::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret(FPooledRenderTargetDesc::Create2DDesc(FSceneRenderTargets::Get_FrameConstantsOnly().GetBufferSizeXY(), PF_FloatRGBA, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));

	Ret.DebugName = TEXT("ScreenSpaceReflections");
	Ret.AutoWritable = false;
	return Ret;
}

void RenderScreenSpaceReflections(FRHICommandListImmediate& RHICmdList, FViewInfo& View, TRefCountPtr<IPooledRenderTarget>& SSROutput, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	FRenderingCompositePassContext CompositeContext(RHICmdList, View);	
	FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View );

	FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;

	FRenderingCompositePass* SceneColorInput = Context.Graph.RegisterPass( new FRCPassPostProcessInput( FSceneRenderTargets::Get(RHICmdList).GetSceneColor() ) );
	FRenderingCompositePass* HZBInput = Context.Graph.RegisterPass( new FRCPassPostProcessInput( View.HZB ) );
	FRenderingCompositePass* HCBInput = nullptr;

	bool bPrevFrame = 0;
	if( ViewState && ViewState->TemporalAAHistoryRT && !Context.View.bCameraCut )
	{
		SceneColorInput = Context.Graph.RegisterPass( new FRCPassPostProcessInput( ViewState->TemporalAAHistoryRT ) );
		bPrevFrame = 1;
	}
	
	FRenderingCompositeOutputRef VelocityInput;
	if ( VelocityRT && !Context.View.bCameraCut )
	{
		VelocityInput = Context.Graph.RegisterPass(new FRCPassPostProcessInput(VelocityRT));
	}
	else
	{
		// No velocity, use black
		VelocityInput = Context.Graph.RegisterPass(new FRCPassPostProcessInput(GSystemTextures.BlackDummy));
	}
	
	if (CVarSSRCone.GetValueOnRenderThread())
	{
		HCBInput = Context.Graph.RegisterPass( new FRCPassPostProcessBuildHCB() );
		HCBInput->SetInput( ePId_Input0, SceneColorInput );
	}

	{
		FRenderingCompositePass* TracePass = Context.Graph.RegisterPass( new FRCPassPostProcessScreenSpaceReflections( bPrevFrame ) );
		TracePass->SetInput( ePId_Input0, SceneColorInput );
		TracePass->SetInput( ePId_Input1, HZBInput );
		TracePass->SetInput( ePId_Input2, HCBInput );
		TracePass->SetInput( ePId_Input3, VelocityInput );

		Context.FinalOutput = FRenderingCompositeOutputRef( TracePass );
	}

	const bool bTemporalFilter = IsSSRTemporalPassRequired(View, false);

	if( ViewState && bTemporalFilter )
	{
		{
			FRenderingCompositeOutputRef HistoryInput;
			if( ViewState->SSRHistoryRT && !Context.View.bCameraCut )
			{
				HistoryInput = Context.Graph.RegisterPass( new FRCPassPostProcessInput( ViewState->SSRHistoryRT ) );
			}
			else
			{
				// No history, use black
				HistoryInput = Context.Graph.RegisterPass(new FRCPassPostProcessInput(GSystemTextures.BlackDummy));
			}

			FRenderingCompositePass* TemporalAAPass = Context.Graph.RegisterPass( new FRCPassPostProcessSSRTemporalAA );
			TemporalAAPass->SetInput( ePId_Input0, Context.FinalOutput );
			TemporalAAPass->SetInput( ePId_Input1, HistoryInput );
			TemporalAAPass->SetInput( ePId_Input2, HistoryInput );
			TemporalAAPass->SetInput( ePId_Input3, VelocityInput );

			Context.FinalOutput = FRenderingCompositeOutputRef( TemporalAAPass );
		}

		FRenderingCompositePass* HistoryOutput = Context.Graph.RegisterPass( new FRCPassPostProcessOutput( &ViewState->SSRHistoryRT ) );
		HistoryOutput->SetInput( ePId_Input0, Context.FinalOutput );

		Context.FinalOutput = FRenderingCompositeOutputRef( HistoryOutput );
	}

	{
		FRenderingCompositePass* ReflectionOutput = Context.Graph.RegisterPass( new FRCPassPostProcessOutput( &SSROutput ) );
		ReflectionOutput->SetInput( ePId_Input0, Context.FinalOutput );

		Context.FinalOutput = FRenderingCompositeOutputRef( ReflectionOutput );
	}

	CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("ReflectionEnvironments"));
}
