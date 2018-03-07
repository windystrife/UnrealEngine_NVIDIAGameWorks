// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTemporalAA.cpp: Post process MotionBlur implementation.
=============================================================================*/

#include "PostProcess/PostProcessTemporalAA.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneRenderTargetParameters.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "CompositionLighting/PostProcessAmbientOcclusion.h"
#include "PostProcess/PostProcessTonemap.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"

const int32 GTemporalAATileSizeX = 8;
const int32 GTemporalAATileSizeY = 8;

static TAutoConsoleVariable<float> CVarTemporalAAFilterSize(
	TEXT("r.TemporalAAFilterSize"),
	1.0f,
	TEXT("Size of the filter kernel. (1.0 = smoother, 0.0 = sharper but aliased)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarTemporalAACatmullRom(
	TEXT("r.TemporalAACatmullRom"),
	0,
	TEXT("Whether to use a Catmull-Rom filter kernel. Should be a bit sharper than Gaussian."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarTemporalAAPauseCorrect(
	TEXT("r.TemporalAAPauseCorrect"),
	1,
	TEXT("Correct temporal AA in pause. This holds onto render targets longer preventing reuse and consumes more memory."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarTemporalAACurrentFrameWeight(
	TEXT("r.TemporalAACurrentFrameWeight"),
	.04f,
	TEXT("Weight of current frame's contribution to the history.  Low values cause blurriness and ghosting, high values fail to hide jittering."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static float CatmullRom( float x )
{
	float ax = FMath::Abs(x);
	if( ax > 1.0f )
		return ( ( -0.5f * ax + 2.5f ) * ax - 4.0f ) *ax + 2.0f;
	else
		return ( 1.5f * ax - 2.5f ) * ax*ax + 1.0f;
}

/** Encapsulates a TemporalAA pixel shader. */
template< uint32 Type, uint32 Responsive >
class FPostProcessTemporalAAPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessTemporalAAPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine( TEXT("RESPONSIVE"), Responsive );
	}

	/** Default constructor. */
	FPostProcessTemporalAAPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter SampleWeights;
	FShaderParameter PlusWeights;
	FShaderParameter DitherScale;
	FShaderParameter VelocityScaling;
	FShaderParameter CurrentFrameWeight;

	/** Initialization constructor. */
	FPostProcessTemporalAAPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		SampleWeights.Bind(Initializer.ParameterMap, TEXT("SampleWeights"));
		PlusWeights.Bind(Initializer.ParameterMap, TEXT("PlusWeights"));
		DitherScale.Bind(Initializer.ParameterMap, TEXT("DitherScale"));
		VelocityScaling.Bind(Initializer.ParameterMap, TEXT("VelocityScaling"));
		CurrentFrameWeight.Bind(Initializer.ParameterMap, TEXT("CurrentFrameWeight"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << SampleWeights << PlusWeights << DitherScale << VelocityScaling << CurrentFrameWeight;
		return bShaderHasOutdatedParameters;
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, bool bUseDither)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		
		FSamplerStateRHIParamRef FilterTable[4];
		FilterTable[0] = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		FilterTable[1] = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		FilterTable[2] = FilterTable[0];
		FilterTable[3] = FilterTable[0];

		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, 0, eFC_0000, FilterTable);

		DeferredParameters.Set(RHICmdList, ShaderRHI, Context.View, MD_PostProcess);

		FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;

		float JitterX = Context.View.TemporalJitterPixelsX;
		float JitterY = Context.View.TemporalJitterPixelsY;

		{
			const FPooledRenderTargetDesc* InputDesc = Context.Pass->GetInputDesc(ePId_Input0);

			static const float SampleOffsets[9][2] =
			{
				{ -1.0f, -1.0f },
				{  0.0f, -1.0f },
				{  1.0f, -1.0f },
				{ -1.0f,  0.0f },
				{  0.0f,  0.0f },
				{  1.0f,  0.0f },
				{ -1.0f,  1.0f },
				{  0.0f,  1.0f },
				{  1.0f,  1.0f },
			};
		
			float FilterSize = CVarTemporalAAFilterSize.GetValueOnRenderThread();
			int32 bCatmullRom = CVarTemporalAACatmullRom.GetValueOnRenderThread();

			float Weights[9];
			float WeightsPlus[5];
			float TotalWeight = 0.0f;
			float TotalWeightLow = 0.0f;
			float TotalWeightPlus = 0.0f;
			for( int32 i = 0; i < 9; i++ )
			{
				float PixelOffsetX = SampleOffsets[i][0] - JitterX;
				float PixelOffsetY = SampleOffsets[i][1] - JitterY;

				PixelOffsetX /= FilterSize;
				PixelOffsetY /= FilterSize;

				if( bCatmullRom )
				{
					Weights[i] = CatmullRom( PixelOffsetX ) * CatmullRom( PixelOffsetY );
					TotalWeight += Weights[i];
				}
				else
				{
					// Normal distribution, Sigma = 0.47
					Weights[i] = FMath::Exp( -2.29f * ( PixelOffsetX * PixelOffsetX + PixelOffsetY * PixelOffsetY ) );
					TotalWeight += Weights[i];
				}
			}

			WeightsPlus[0] = Weights[1];
			WeightsPlus[1] = Weights[3];
			WeightsPlus[2] = Weights[4];
			WeightsPlus[3] = Weights[5];
			WeightsPlus[4] = Weights[7];
			TotalWeightPlus = Weights[1] + Weights[3] + Weights[4] + Weights[5] + Weights[7];
			
			for( int32 i = 0; i < 9; i++ )
			{
				SetShaderValue(RHICmdList, ShaderRHI, SampleWeights, Weights[i] / TotalWeight, i );
			}

			for( int32 i = 0; i < 5; i++ )
			{
				SetShaderValue(RHICmdList, ShaderRHI, PlusWeights, WeightsPlus[i] / TotalWeightPlus, i );
			}
		}

		SetShaderValue(RHICmdList, ShaderRHI, DitherScale, bUseDither ? 1.0f : 0.0f);

		const bool bIgnoreVelocity = (ViewState && ViewState->bSequencerIsPaused);
		SetShaderValue(RHICmdList, ShaderRHI, VelocityScaling, bIgnoreVelocity ? 0.0f : 1.0f);

		SetShaderValue(RHICmdList, ShaderRHI, CurrentFrameWeight, CVarTemporalAACurrentFrameWeight.GetValueOnRenderThread());

		SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<FCameraMotionParameters>(), CreateCameraMotionParametersUniformBuffer(Context.View));
	}
};

// Typedef is necessary because the C preprocessor thinks the comma in the template parameter list is a comma in the macro parameter list.
#define IMPLEMENT_TEMPORALAA_PIXELSHADER_TYPE(A, B, EntryName) \
	typedef FPostProcessTemporalAAPS<A,B> FPostProcessTemporalAAPS##A##B; \
	IMPLEMENT_SHADER_TYPE(template<>,FPostProcessTemporalAAPS##A##B,TEXT("/Engine/Private/PostProcessTemporalAA.usf"),EntryName,SF_Pixel);

IMPLEMENT_TEMPORALAA_PIXELSHADER_TYPE(0, 0, TEXT("DOFTemporalAAPS"));
IMPLEMENT_TEMPORALAA_PIXELSHADER_TYPE(1, 0, TEXT("MainTemporalAAPS"));
IMPLEMENT_TEMPORALAA_PIXELSHADER_TYPE(1, 1, TEXT("MainTemporalAAPS"));
IMPLEMENT_TEMPORALAA_PIXELSHADER_TYPE(2, 0, TEXT("SSRTemporalAAPS"));
IMPLEMENT_TEMPORALAA_PIXELSHADER_TYPE(3, 0, TEXT("LightShaftTemporalAAPS"));
IMPLEMENT_TEMPORALAA_PIXELSHADER_TYPE(4, 0, TEXT("MainFastTemporalAAPS"));
IMPLEMENT_TEMPORALAA_PIXELSHADER_TYPE(4, 1, TEXT("MainFastTemporalAAPS"));

/** Encapsulates the post processing depth of field setup pixel shader. */
template<uint32 Type>
class FPostProcessTemporalAACS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessTemporalAACS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GTemporalAATileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GTemporalAATileSizeY);
	}

	/** Default constructor. */
	FPostProcessTemporalAACS() {}

public:
	// CS params
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter TemporalAAComputeParams;
	FShaderParameter OutComputeTex;

	// VS params
	FShaderResourceParameter EyeAdaptation;

	// PS params
	FShaderParameter SampleWeights;
	FShaderParameter PlusWeights;
	FShaderParameter DitherScale;
	FShaderParameter VelocityScaling;
	FShaderParameter CurrentFrameWeight;

	/** Initialization constructor. */
	FPostProcessTemporalAACS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		// CS params
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		TemporalAAComputeParams.Bind(Initializer.ParameterMap,TEXT("TemporalAAComputeParams"));
		OutComputeTex.Bind(Initializer.ParameterMap, TEXT("OutComputeTex"));

		// VS params
		EyeAdaptation.Bind(Initializer.ParameterMap, TEXT("EyeAdaptation"));

		// PS params
		SampleWeights.Bind(Initializer.ParameterMap, TEXT("SampleWeights"));
		PlusWeights.Bind(Initializer.ParameterMap, TEXT("PlusWeights"));
		DitherScale.Bind(Initializer.ParameterMap, TEXT("DitherScale"));
		VelocityScaling.Bind(Initializer.ParameterMap, TEXT("VelocityScaling"));
		CurrentFrameWeight.Bind(Initializer.ParameterMap, TEXT("CurrentFrameWeight"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		// CS params
		Ar << PostprocessParameter << DeferredParameters << TemporalAAComputeParams << OutComputeTex;
		// VS params
		Ar << EyeAdaptation;
		// PS params
		Ar << SampleWeights << PlusWeights << DitherScale << VelocityScaling << CurrentFrameWeight;
		return bShaderHasOutdatedParameters;
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, const FIntPoint& DestSize, FUnorderedAccessViewRHIParamRef DestUAV, bool bUseDither, FTextureRHIParamRef EyeAdaptationTex)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		// CS params
		FSamplerStateRHIParamRef FilterTable[4];
		{
			FilterTable[0] = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			FilterTable[1] = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			FilterTable[2] = FilterTable[0];
			FilterTable[3] = FilterTable[0];
		}
		PostprocessParameter.SetCS(ShaderRHI, Context, RHICmdList, 0, eFC_0000, FilterTable);

		DeferredParameters.Set(RHICmdList, ShaderRHI, Context.View, MD_PostProcess);
		RHICmdList.SetUAVParameter(ShaderRHI, OutComputeTex.GetBaseIndex(), DestUAV);

		const float ForceResponsiveFrame = Context.View.bCameraCut ? 1.f : 0.f;
		FVector4 TemporalAAComputeValues(ForceResponsiveFrame, 0, 1.f / (float)DestSize.X, 1.f / (float)DestSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, TemporalAAComputeParams, TemporalAAComputeValues);

		// VS params		
		SetTextureParameter(RHICmdList, ShaderRHI, EyeAdaptation, EyeAdaptationTex);

		// PS params
		{
			const float JitterX = Context.View.TemporalJitterPixelsX;
			const float JitterY = Context.View.TemporalJitterPixelsY;

			static const float SampleOffsets[9][2] =
			{
				{ -1.0f, -1.0f },
				{  0.0f, -1.0f },
				{  1.0f, -1.0f },
				{ -1.0f,  0.0f },
				{  0.0f,  0.0f },
				{  1.0f,  0.0f },
				{ -1.0f,  1.0f },
				{  0.0f,  1.0f },
				{  1.0f,  1.0f },
			};
		
			float FilterSize = CVarTemporalAAFilterSize.GetValueOnRenderThread();
			int32 bCatmullRom = CVarTemporalAACatmullRom.GetValueOnRenderThread();

			float Weights[9];
			float WeightsPlus[5];
			float TotalWeight = 0.0f;
			float TotalWeightLow = 0.0f;
			float TotalWeightPlus = 0.0f;
			for( int32 i = 0; i < 9; i++ )
			{
				float PixelOffsetX = SampleOffsets[i][0] - JitterX;
				float PixelOffsetY = SampleOffsets[i][1] - JitterY;

				PixelOffsetX /= FilterSize;
				PixelOffsetY /= FilterSize;

				if( bCatmullRom )
				{
					Weights[i] = CatmullRom( PixelOffsetX ) * CatmullRom( PixelOffsetY );
					TotalWeight += Weights[i];
				}
				else
				{
					// Normal distribution, Sigma = 0.47
					Weights[i] = FMath::Exp( -2.29f * ( PixelOffsetX * PixelOffsetX + PixelOffsetY * PixelOffsetY ) );
					TotalWeight += Weights[i];
				}
			}

			WeightsPlus[0] = Weights[1];
			WeightsPlus[1] = Weights[3];
			WeightsPlus[2] = Weights[4];
			WeightsPlus[3] = Weights[5];
			WeightsPlus[4] = Weights[7];
			TotalWeightPlus = Weights[1] + Weights[3] + Weights[4] + Weights[5] + Weights[7];
			
			for( int32 i = 0; i < 9; i++ )
			{
				SetShaderValue(RHICmdList, ShaderRHI, SampleWeights, Weights[i] / TotalWeight, i );
			}

			for( int32 i = 0; i < 5; i++ )
			{
				SetShaderValue(RHICmdList, ShaderRHI, PlusWeights, WeightsPlus[i] / TotalWeightPlus, i );
			}
		}

		SetShaderValue(RHICmdList, ShaderRHI, DitherScale, bUseDither ? 1.0f : 0.0f);

		const bool bIgnoreVelocity = (ViewState && ViewState->bSequencerIsPaused);
		SetShaderValue(RHICmdList, ShaderRHI, VelocityScaling, bIgnoreVelocity ? 0.0f : 1.0f);

		SetShaderValue(RHICmdList, ShaderRHI, CurrentFrameWeight, CVarTemporalAACurrentFrameWeight.GetValueOnRenderThread());

		SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<FCameraMotionParameters>(), CreateCameraMotionParametersUniformBuffer(Context.View));
	}

	template <typename TRHICmdList>
	void UnsetParameters(TRHICmdList& RHICmdList)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		RHICmdList.SetUAVParameter(ShaderRHI, OutComputeTex.GetBaseIndex(), NULL);
	}
};

#define IMPLEMENT_TEMPORALAA_COMPUTESHADER_TYPE(A, EntryName) \
	typedef FPostProcessTemporalAACS<A> FPostProcessTemporalAACS##A; \
	IMPLEMENT_SHADER_TYPE(template<>,FPostProcessTemporalAACS##A,TEXT("/Engine/Private/PostProcessTemporalAA.usf"),EntryName,SF_Compute);

IMPLEMENT_TEMPORALAA_COMPUTESHADER_TYPE(0, TEXT("DOFTemporalAACS"));
IMPLEMENT_TEMPORALAA_COMPUTESHADER_TYPE(1, TEXT("MainTemporalAACS"));
//IMPLEMENT_TEMPORALAA_COMPUTESHADER_TYPE(2, TEXT("SSRTemporalAACS"));
//IMPLEMENT_TEMPORALAA_COMPUTESHADER_TYPE(3, TEXT("LightShaftTemporalAACS"));
IMPLEMENT_TEMPORALAA_COMPUTESHADER_TYPE(4, TEXT("MainFastTemporalAACS"));

template <int32 Type, typename TRHICmdList>
void DispatchCSTemplate(TRHICmdList& RHICmdList, FRenderingCompositePassContext& Context, const FIntRect& DestRect, FUnorderedAccessViewRHIParamRef DestUAV, const bool bUseDither, FTextureRHIParamRef EyeAdaptationTex)
{
	auto ShaderMap = Context.GetShaderMap();
	TShaderMapRef<FPostProcessTemporalAACS<Type>> ComputeShader(ShaderMap);

	RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

	FIntPoint DestSize(DestRect.Width(), DestRect.Height());
	ComputeShader->SetParameters(RHICmdList, Context, DestSize, DestUAV, bUseDither, EyeAdaptationTex);

	uint32 GroupSizeX = FMath::DivideAndRoundUp(DestSize.X, GTemporalAATileSizeX);
	uint32 GroupSizeY = FMath::DivideAndRoundUp(DestSize.Y, GTemporalAATileSizeY);
	DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

	ComputeShader->UnsetParameters(RHICmdList);
}

void FRCPassPostProcessSSRTemporalAA::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, SSRTemporalAA);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FViewInfo& View = Context.View;

	FIntPoint TexSize = InputDesc->Extent;

	// we assume the input and output is full resolution

	FIntPoint SrcSize = InputDesc->Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	DrawClearQuad(Context.RHICmdList, true, FLinearColor::Black, false, 0, false, 0, PassOutputs[0].RenderTargetDesc.Extent, SrcRect);

	Context.SetViewportAndCallRHI(SrcRect);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef< FPostProcessTonemapVS >			VertexShader(Context.GetShaderMap());
	TShaderMapRef< FPostProcessTemporalAAPS<2, 0> >	PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetVS(Context);
	PixelShader->SetParameters(Context.RHICmdList, Context, false);

	DrawPostProcessPass(
		Context.RHICmdList,
		0, 0,
		SrcRect.Width(), SrcRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		SrcRect.Size(),
		SrcSize,
		*VertexShader,
		View.StereoPass,
		false, // Disabled for correctness
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessSSRTemporalAA::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.DebugName = TEXT("SSRTemporalAA");
	Ret.AutoWritable = false;
	return Ret;
}

void FRCPassPostProcessDOFTemporalAA::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENTF(Context.RHICmdList, DOFTemporalAA, TEXT("DOFTemporalAA%s"), bIsComputePass?TEXT("Compute"):TEXT(""));

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	AsyncEndFence = FComputeFenceRHIRef();

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FViewInfo& View = Context.View;
	FSceneViewState* ViewState = Context.ViewState;

	FIntPoint TexSize = InputDesc->Extent;

	// we assume the input and output is full resolution

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	if (bIsComputePass)
	{
		DestRect = {View.ViewRect.Min, View.ViewRect.Min + DestSize};

		// Common setup
		SetRenderTarget(Context.RHICmdList, nullptr, nullptr);
		Context.SetViewportAndCallRHI(DestRect, 0.0f, 1.0f);
		
		static FName AsyncEndFenceName(TEXT("AsyncDOFTemporalAAEndFence"));
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
				SCOPED_COMPUTE_EVENT(RHICmdListComputeImmediate, AsyncDOFTemporalAA);
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
			Context.RHICmdList.BeginUpdateMultiFrameResource(DestRenderTarget.ShaderResourceTexture);

			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget.UAV);
			DispatchCS(Context.RHICmdList, Context, DestRect, DestRenderTarget.UAV, EyeAdaptationTex);
			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget.UAV, AsyncEndFence);

			Context.RHICmdList.EndUpdateMultiFrameResource(DestRenderTarget.ShaderResourceTexture);
		}
	}
	else
	{
		WaitForInputPassComputeFences(Context.RHICmdList);

		// Inform MultiGPU systems that we're starting to update the texture
		Context.RHICmdList.BeginUpdateMultiFrameResource(DestRenderTarget.ShaderResourceTexture);

		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

		// is optimized away if possible (RT size=view size, )
		DrawClearQuad(Context.RHICmdList, true, FLinearColor::Black, false, 0, false, 0, DestSize, SrcRect);

		Context.SetViewportAndCallRHI(SrcRect);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		TShaderMapRef< FPostProcessTonemapVS >			VertexShader(Context.GetShaderMap());
		TShaderMapRef< FPostProcessTemporalAAPS<0, 0> >	PixelShader(Context.GetShaderMap());

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		VertexShader->SetVS(Context);
		PixelShader->SetParameters(Context.RHICmdList, Context, false);

		DrawPostProcessPass(
			Context.RHICmdList,
			0, 0,
			SrcRect.Width(), SrcRect.Height(),
			SrcRect.Min.X, SrcRect.Min.Y,
			SrcRect.Width(), SrcRect.Height(),
			SrcRect.Size(),
			SrcSize,
			*VertexShader,
			View.StereoPass,
			false, // Disabled for correctness
			EDRF_UseTriangleOptimization);

		Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

		ViewState->DOFHistoryRT = PassOutputs[0].PooledRenderTarget;

		// Inform MultiGPU systems that we've finished with this texture for this frame
		Context.RHICmdList.EndUpdateMultiFrameResource(DestRenderTarget.ShaderResourceTexture);

		check(ViewState->DOFHistoryRT);
	}
}

template <typename TRHICmdList>
void FRCPassPostProcessDOFTemporalAA::DispatchCS(TRHICmdList& RHICmdList, FRenderingCompositePassContext& Context, const FIntRect& DestRect, FUnorderedAccessViewRHIParamRef DestUAV, FTextureRHIParamRef EyeAdaptationTex)
{
	DispatchCSTemplate<0>(RHICmdList, Context, DestRect, DestUAV, false, EyeAdaptationTex);	
}


FPooledRenderTargetDesc FRCPassPostProcessDOFTemporalAA::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	Ret.AutoWritable = false;
	Ret.DebugName = TEXT("BokehDOFTemporalAA");
	Ret.TargetableFlags &= ~(TexCreate_RenderTargetable | TexCreate_UAV);
	Ret.TargetableFlags |= bIsComputePass ? TexCreate_UAV : TexCreate_RenderTargetable;
	return Ret;
}


void FRCPassPostProcessDOFTemporalAANear::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, DOFTemporalAANear);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FViewInfo& View = Context.View;
	FSceneViewState* ViewState = Context.ViewState;

	FIntPoint TexSize = InputDesc->Extent;

	// we assume the input and output is full resolution

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Inform MultiGPU systems that we're starting to update this texture for this frame
	Context.RHICmdList.BeginUpdateMultiFrameResource(DestRenderTarget.ShaderResourceTexture);

	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	DrawClearQuad(Context.RHICmdList, true, FLinearColor::Black, false, 0, false, 0, DestSize, SrcRect);

	Context.SetViewportAndCallRHI(SrcRect);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef< FPostProcessTonemapVS >			VertexShader(Context.GetShaderMap());
	TShaderMapRef< FPostProcessTemporalAAPS<0, 0> >	PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetVS(Context);
	PixelShader->SetParameters(Context.RHICmdList, Context, false);

	DrawPostProcessPass(
		Context.RHICmdList,
		0, 0,
		SrcRect.Width(), SrcRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		SrcRect.Size(),
		SrcSize,
		*VertexShader,
		View.StereoPass,
		false, // Disabled for correctness
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	ViewState->DOFHistoryRT2 = PassOutputs[0].PooledRenderTarget;

	// Inform MultiGPU systems that we've finished with this texture for this frame
	Context.RHICmdList.EndUpdateMultiFrameResource(DestRenderTarget.ShaderResourceTexture);

	check( ViewState->DOFHistoryRT2 );
}

FPooledRenderTargetDesc FRCPassPostProcessDOFTemporalAANear::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.DebugName = TEXT("BokehDOFTemporalAANear");

	return Ret;
}



void FRCPassPostProcessLightShaftTemporalAA::Process(FRenderingCompositePassContext& Context)
{
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FViewInfo& View = Context.View;
	FSceneViewState* ViewState = Context.ViewState;

	FIntPoint TexSize = InputDesc->Extent;

	// we assume the input and output is full resolution

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	DrawClearQuad(Context.RHICmdList, true, FLinearColor::Black, false, 0, false, 0, DestSize, SrcRect);

	Context.SetViewportAndCallRHI(SrcRect);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef< FPostProcessTonemapVS >			VertexShader(Context.GetShaderMap());
	TShaderMapRef< FPostProcessTemporalAAPS<3, 0> >	PixelShader(Context.GetShaderMap());

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	VertexShader->SetVS(Context);
	PixelShader->SetParameters(Context.RHICmdList, Context, false);

	DrawPostProcessPass(
		Context.RHICmdList,
		0, 0,
		SrcRect.Width(), SrcRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		SrcRect.Size(),
		SrcSize,
		*VertexShader,
		View.StereoPass,
		false, // Disabled for correctness
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessLightShaftTemporalAA::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.DebugName = TEXT("LightShaftTemporalAA");

	return Ret;
}


void FRCPassPostProcessTemporalAA::Process(FRenderingCompositePassContext& Context)
{
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	AsyncEndFence = FComputeFenceRHIRef();

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);

	const FViewInfo& View = Context.View;
	FSceneViewState* ViewState = Context.ViewState;

	FIntPoint TexSize = InputDesc->Extent;

	// we assume the input and output is full resolution

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = SceneContext.GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	SCOPED_DRAW_EVENTF(Context.RHICmdList, TemporalAA, TEXT("TemporalAA%s %dx%d"), bIsComputePass?TEXT("Compute"):TEXT(""), SrcRect.Width(), SrcRect.Height());

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessAAQuality")); 
	uint32 Quality = FMath::Clamp(CVar->GetValueOnRenderThread(), 1, 6);
	bool bUseFast = Quality == 3;

	// Only use dithering if we are outputting to a low precision format
	const bool bUseDither = PassOutputs[0].RenderTargetDesc.Format != PF_FloatRGBA;

	if (bIsComputePass)
	{
		// Common setup
		SetRenderTarget(Context.RHICmdList, nullptr, nullptr);
		Context.SetViewportAndCallRHI(DestRect, 0.0f, 1.0f);
		DestRect = {View.ViewRect.Min, View.ViewRect.Min + DestSize};
		
		static FName AsyncEndFenceName(TEXT("AsyncTemporalAAEndFence"));
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
				SCOPED_COMPUTE_EVENT(RHICmdListComputeImmediate, AsyncTemporalAA);
				WaitForInputPassComputeFences(RHICmdListComputeImmediate);
					
				RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget.UAV);
				DispatchCS(RHICmdListComputeImmediate, Context, DestRect, DestRenderTarget.UAV, bUseFast, bUseDither, EyeAdaptationTex);
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
			DispatchCS(Context.RHICmdList, Context, DestRect, DestRenderTarget.UAV, bUseFast, bUseDither, EyeAdaptationTex);
			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget.UAV, AsyncEndFence);

			Context.RHICmdList.EndUpdateMultiFrameResource(DestRenderTarget.ShaderResourceTexture);
		}
	}
	else
	{
		WaitForInputPassComputeFences(Context.RHICmdList);

		// Inform MultiGPU systems that we're starting to update this resource
		Context.RHICmdList.BeginUpdateMultiFrameResource(DestRenderTarget.ShaderResourceTexture);

		//Context.SetRenderTarget(RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, SceneContext.GetSceneDepthTexture(), ESimpleRenderTargetMode::EUninitializedColorExistingDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);

		// is optimized away if possible (RT size=view size, )
		DrawClearQuad(Context.RHICmdList, true, FLinearColor::Black, false, 0, false, 0, DestSize, SrcRect);

		Context.SetViewportAndCallRHI(SrcRect);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();

		if(Context.View.bCameraCut)
		{
			// On camera cut this turns on responsive everywhere.
		
			// Normal temporal feedback
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false,CF_Always>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			TShaderMapRef< FPostProcessTonemapVS >			VertexShader(Context.GetShaderMap());
			if (bUseFast)
			{
				TShaderMapRef< FPostProcessTemporalAAPS<4, 1> >	PixelShader(Context.GetShaderMap());

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

				VertexShader->SetVS(Context);
				PixelShader->SetParameters(Context.RHICmdList, Context, bUseDither);
			}
			else
			{
				TShaderMapRef< FPostProcessTemporalAAPS<1, 1> >	PixelShader(Context.GetShaderMap());

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

				VertexShader->SetVS(Context);
				PixelShader->SetParameters(Context.RHICmdList, Context, bUseDither);
			}
	
			DrawPostProcessPass(
				Context.RHICmdList,
				0, 0,
				SrcRect.Width(), SrcRect.Height(),
				SrcRect.Min.X, SrcRect.Min.Y,
				SrcRect.Width(), SrcRect.Height(),
				SrcRect.Size(),
				SrcSize,
				*VertexShader,
				View.StereoPass,
				false, // Disabled for correctness
				EDRF_UseTriangleOptimization);
		}
		else
		{
			{	
				// Normal temporal feedback
				// Draw to pixels where stencil == 0
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
					false, CF_Always,
					true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					STENCIL_TEMPORAL_RESPONSIVE_AA_MASK, STENCIL_TEMPORAL_RESPONSIVE_AA_MASK
				>::GetRHI();
	
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;

				TShaderMapRef< FPostProcessTonemapVS >			VertexShader(Context.GetShaderMap());
				if (bUseFast)
				{
					TShaderMapRef< FPostProcessTemporalAAPS<4, 0> >	PixelShader(Context.GetShaderMap());
	
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

					VertexShader->SetVS(Context);
					PixelShader->SetParameters(Context.RHICmdList, Context, bUseDither);
				}
				else
				{
					TShaderMapRef< FPostProcessTemporalAAPS<1, 0> >	PixelShader(Context.GetShaderMap());
	
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

					VertexShader->SetVS(Context);
					PixelShader->SetParameters(Context.RHICmdList, Context, bUseDither);
				}
		
				DrawPostProcessPass(
					Context.RHICmdList,
					0, 0,
					SrcRect.Width(), SrcRect.Height(),
					SrcRect.Min.X, SrcRect.Min.Y,
					SrcRect.Width(), SrcRect.Height(),
					SrcRect.Size(),
					SrcSize,
					*VertexShader,
					View.StereoPass,
					false, // Disabled for correctness
					EDRF_UseTriangleOptimization);
			}
	
			{	// Responsive feedback for tagged pixels
				// Draw to pixels where stencil != 0
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
					false, CF_Always,
					true, CF_NotEqual, SO_Keep, SO_Keep, SO_Zero,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					STENCIL_TEMPORAL_RESPONSIVE_AA_MASK, STENCIL_TEMPORAL_RESPONSIVE_AA_MASK
				>::GetRHI();
			
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;

				TShaderMapRef< FPostProcessTonemapVS >			VertexShader(Context.GetShaderMap());
				if(bUseFast)
				{
					TShaderMapRef< FPostProcessTemporalAAPS<4, 1> >	PixelShader(Context.GetShaderMap());
	
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

					VertexShader->SetVS(Context);
					PixelShader->SetParameters(Context.RHICmdList, Context, bUseDither);
				}
				else
				{
					TShaderMapRef< FPostProcessTemporalAAPS<1, 1> >	PixelShader(Context.GetShaderMap());
	
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
					SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

					VertexShader->SetVS(Context);
					PixelShader->SetParameters(Context.RHICmdList, Context, bUseDither);
				}
	
				DrawPostProcessPass(
					Context.RHICmdList,
					0, 0,
					SrcRect.Width(), SrcRect.Height(),
					SrcRect.Min.X, SrcRect.Min.Y,
					SrcRect.Width(), SrcRect.Height(),
					SrcRect.Size(),
					SrcSize,
					*VertexShader,
					View.StereoPass,
					false, // Disabled for correctness
					EDRF_UseTriangleOptimization);
			}
		}

		Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

		// Inform MultiGPU systems that we've finished with this texture for this frame
		Context.RHICmdList.EndUpdateMultiFrameResource(DestRenderTarget.ShaderResourceTexture);
	}

	if( CVarTemporalAAPauseCorrect.GetValueOnRenderThread() )
	{
		ViewState->PendingTemporalAAHistoryRT = PassOutputs[0].PooledRenderTarget;
	}
	else
	{
		ViewState->TemporalAAHistoryRT = PassOutputs[0].PooledRenderTarget;
	}

	if (FSceneRenderer::ShouldCompositeEditorPrimitives(Context.View))
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::WaitForOutstandingTasksOnly);
		// because of the flush it's ok to remove the const, this is not ideal as the flush can cost performance
		FViewInfo& NonConstView = (FViewInfo&)Context.View;

		// Remove jitter
		NonConstView.ViewMatrices.HackRemoveTemporalAAProjectionJitter();

		NonConstView.InitRHIResources();
	}
}

template <typename TRHICmdList>
void FRCPassPostProcessTemporalAA::DispatchCS(TRHICmdList& RHICmdList, FRenderingCompositePassContext& Context, const FIntRect& DestRect, FUnorderedAccessViewRHIParamRef DestUAV, const bool bUseFast, const bool bUseDither, FTextureRHIParamRef EyeAdaptationTex)
{
	if (bUseFast)
	{
		DispatchCSTemplate<4>(RHICmdList, Context, DestRect, DestUAV, bUseDither, EyeAdaptationTex);
	}
	else
	{
		DispatchCSTemplate<1>(RHICmdList, Context, DestRect, DestUAV, bUseDither, EyeAdaptationTex);
	}
}

FPooledRenderTargetDesc FRCPassPostProcessTemporalAA::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	Ret.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	Ret.Reset();
	//regardless of input type, PF_FloatRGBA is required to properly accumulate between frames for a good result.
	Ret.Format = PF_FloatRGBA;
	Ret.DebugName = TEXT("TemporalAA");
	Ret.AutoWritable = false;
	Ret.TargetableFlags &= ~(TexCreate_RenderTargetable | TexCreate_UAV);
	Ret.TargetableFlags |= bIsComputePass ? TexCreate_UAV : TexCreate_RenderTargetable;
	return Ret;
}
