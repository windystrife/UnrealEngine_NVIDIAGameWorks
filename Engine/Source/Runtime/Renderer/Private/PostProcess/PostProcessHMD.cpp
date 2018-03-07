// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessHMD.cpp: Post processing for HMD devices.
=============================================================================*/

#include "PostProcess/PostProcessHMD.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "SceneUtils.h"
#include "StaticBoundShaderState.h"
#include "SceneRenderTargetParameters.h"
#include "SceneRendering.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"

/** The filter vertex declaration resource type. */
class FDistortionVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/** Destructor. */
	virtual ~FDistortionVertexDeclaration() {}

	virtual void InitRHI() override
	{
		uint16 Stride = sizeof(FDistortionVertex);
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FDistortionVertex, Position),VET_Float2,0, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FDistortionVertex, TexR), VET_Float2, 1, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FDistortionVertex, TexG), VET_Float2, 2, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FDistortionVertex, TexB), VET_Float2, 3, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FDistortionVertex, VignetteFactor), VET_Float1, 4, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FDistortionVertex, TimewarpFactor), VET_Float1, 5, Stride));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** The Distortion vertex declaration. */
TGlobalResource<FDistortionVertexDeclaration> GDistortionVertexDeclaration;

/** Encapsulates the post processing vertex shader. */
class FPostProcessHMDVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessHMDVS, Global);

	// Distortion parameter values
	FShaderParameter EyeToSrcUVScale;
	FShaderParameter EyeToSrcUVOffset;

	// Timewarp-related params
	FShaderParameter EyeRotationStart;
	FShaderParameter EyeRotationEnd;

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	/** Default constructor. */
	FPostProcessHMDVS() {}

public:

	/** Initialization constructor. */
	FPostProcessHMDVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		EyeToSrcUVScale.Bind(Initializer.ParameterMap, TEXT("EyeToSrcUVScale"));
		EyeToSrcUVOffset.Bind(Initializer.ParameterMap, TEXT("EyeToSrcUVOffset"));
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		check(GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice());
		FVector2D EyeToSrcUVScaleValue;
		FVector2D EyeToSrcUVOffsetValue;
		GEngine->XRSystem->GetHMDDevice()->GetEyeRenderParams_RenderThread(Context, EyeToSrcUVScaleValue, EyeToSrcUVOffsetValue);
		SetShaderValue(Context.RHICmdList, ShaderRHI, EyeToSrcUVScale, EyeToSrcUVScaleValue);
		SetShaderValue(Context.RHICmdList, ShaderRHI, EyeToSrcUVOffset, EyeToSrcUVOffsetValue);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << EyeToSrcUVScale << EyeToSrcUVOffset;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(, FPostProcessHMDVS, TEXT("/Engine/Private/PostProcessHMD.usf"), TEXT("MainVS"), SF_Vertex);

/** Encapsulates the post processing HMD distortion and correction pixel shader. */
class FPostProcessHMDPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessHMDPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	/** Default constructor. */
	FPostProcessHMDPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;

	/** Initialization constructor. */
	FPostProcessHMDPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);

	}

	template <typename TRHICmdList>
	void SetPS(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, FIntRect SrcRect, FIntPoint SrcBufferSize, EStereoscopicPass StereoPass, FMatrix& QuadTexTransform)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		DeferredParameters.Set(RHICmdList, ShaderRHI, Context.View, MD_PostProcess);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(, FPostProcessHMDPS, TEXT("/Engine/Private/PostProcessHMD.usf"), TEXT("MainPS"), SF_Pixel);

void FRCPassPostProcessHMD::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessHMD);
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	
	const FIntRect SrcRect = View.ViewRect;
	const FIntRect DestRect = View.UnscaledViewRect;
	const FIntPoint SrcSize = InputDesc->Extent;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport
	if (DestRenderTarget.TargetableTexture->GetClearColor() == FLinearColor::Black)
	{
		FRHIRenderTargetView RtView = FRHIRenderTargetView(DestRenderTarget.TargetableTexture, ERenderTargetLoadAction::EClear);
		FRHISetRenderTargetsInfo Info(1, &RtView, FRHIDepthRenderTargetView());
		Context.RHICmdList.SetRenderTargetsAndClear(Info);
		Context.SetViewportAndCallRHI(DestRect);
	}
	else
	{
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		Context.SetViewportAndCallRHI(DestRect);
		DrawClearQuad(Context.RHICmdList, FLinearColor::Black);
	}

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	FMatrix QuadTexTransform = FMatrix::Identity;
	FMatrix QuadPosTransform = FMatrix::Identity;

	check(GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice());

	{
		TShaderMapRef<FPostProcessHMDVS> VertexShader(Context.GetShaderMap());
		TShaderMapRef<FPostProcessHMDPS> PixelShader(Context.GetShaderMap());

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GDistortionVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		VertexShader->SetVS(Context);
		PixelShader->SetPS(Context.RHICmdList, Context, SrcRect, SrcSize, View.StereoPass, QuadTexTransform);
	}
	GEngine->XRSystem->GetHMDDevice()->DrawDistortionMesh_RenderThread(Context, SrcSize);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessHMD::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassOutputs[0].RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("HMD");

	return Ret;
}
