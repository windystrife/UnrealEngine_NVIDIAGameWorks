//-----------------------------------------------------------------------------
// File:		PostProcessLpvIndirect.cpp
//
// Summary:		Light propagation volume postprocessing
//
// Created:		11/03/2013
//
// Author:		mailto:benwood@microsoft.com
//
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------

#include "CompositionLighting/PostProcessLpvIndirect.h"
#include "StaticBoundShaderState.h"
#include "CanvasTypes.h"
#include "RenderTargetTemp.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneRenderTargetParameters.h"
#include "LightPropagationVolume.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "LightPropagationVolumeSettings.h"
#include "PipelineStateCache.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FLpvReadUniformBufferParameters,TEXT("LpvRead"));
typedef TUniformBufferRef<FLpvReadUniformBufferParameters> FLpvReadUniformBufferRef;


TAutoConsoleVariable<int32> CVarLPVMixing(
	TEXT("r.LPV.Mixing"),
	1,
	TEXT("Reflection environment mixes with indirect shading (Ambient + LPV).\n")
	TEXT(" 0 is off, 1 is on (default)"),
	ECVF_RenderThreadSafe | ECVF_Cheat);


/** Encapsulates the post processing ambient pixel shader. */
class FPostProcessLpvIndirectPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessLpvIndirectPS, Global);

	//@todo-rco: Remove this when reenabling for OpenGL
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && IsLPVSupported(Platform); }

	/** Default constructor. */
	FPostProcessLpvIndirectPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderResourceParameter LpvBufferSRVParameters[7];
	FShaderResourceParameter LpvVolumeTextureSampler;
	FShaderResourceParameter AOVolumeTextureSRVParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter PreIntegratedGF;
	FShaderResourceParameter PreIntegratedGFSampler;

	/** Initialization constructor. */
	FPostProcessLpvIndirectPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		for ( int i=0; i<7; i++ )
		{
			LpvBufferSRVParameters[i].Bind( Initializer.ParameterMap, LpvVolumeTextureSRVNames[i] );
		}
		LpvVolumeTextureSampler.Bind(Initializer.ParameterMap, TEXT("gLpv3DTextureSampler"));

		AOVolumeTextureSRVParameter.Bind( Initializer.ParameterMap, TEXT("gAOVolumeTexture") );

		PreIntegratedGF.Bind(Initializer.ParameterMap, TEXT("PreIntegratedGF"));
		PreIntegratedGFSampler.Bind(Initializer.ParameterMap, TEXT("PreIntegratedGFSampler"));
	}

	template <typename TRHICmdList>
	void SetParameters(	
		TRHICmdList& RHICmdList,
		FTextureRHIParamRef* LpvBufferSRVsIn, 
		FTextureRHIParamRef AOVolumeTextureSRVIn, 
		FLpvReadUniformBufferRef LpvUniformBuffer, 
		const FRenderingCompositePassContext& Context )
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<FLpvReadUniformBufferParameters>(), LpvUniformBuffer);

		for ( int i=0; i<7; i++ )
		{
			if ( LpvBufferSRVParameters[i].IsBound() )
			{
				RHICmdList.SetShaderTexture(ShaderRHI, LpvBufferSRVParameters[i].GetBaseIndex(), LpvBufferSRVsIn[i]);
				SetTextureParameter(RHICmdList, ShaderRHI, LpvBufferSRVParameters[i], LpvVolumeTextureSampler, TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI(), LpvBufferSRVsIn[i]);
			}
		}

		if ( AOVolumeTextureSRVParameter.IsBound() )
		{
			RHICmdList.SetShaderTexture(ShaderRHI, AOVolumeTextureSRVParameter.GetBaseIndex(), AOVolumeTextureSRVIn );
		}
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		DeferredParameters.Set(RHICmdList, ShaderRHI, Context.View, MD_PostProcess);
		SetTextureParameter(RHICmdList, ShaderRHI, PreIntegratedGF, PreIntegratedGFSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), GSystemTextures.PreintegratedGF->GetRenderTargetItem().ShaderResourceTexture);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;

		for ( int i=0; i<7; i++ )
		{
			Ar << LpvBufferSRVParameters[i];
		}
		Ar << LpvVolumeTextureSampler;
		Ar << DeferredParameters;
		Ar << PreIntegratedGF;
		Ar << PreIntegratedGFSampler;
		Ar << AOVolumeTextureSRVParameter;
		return bShaderHasOutdatedParameters;
	}
};

template<bool bApplySeparateSpecularRT>
class TPostProcessLpvIndirectPS : public FPostProcessLpvIndirectPS
{
	DECLARE_SHADER_TYPE(TPostProcessLpvIndirectPS, Global);

	/** Default constructor. */
	TPostProcessLpvIndirectPS() {}

public:

	TPostProcessLpvIndirectPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FPostProcessLpvIndirectPS(Initializer)
	{}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPostProcessLpvIndirectPS::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("APPLY_SEPARATE_SPECULAR_RT"), bApplySeparateSpecularRT);
	}
};

IMPLEMENT_SHADER_TYPE(template<>,TPostProcessLpvIndirectPS<false>,TEXT("/Engine/Private/PostProcessLpvIndirect.usf"),TEXT("MainPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TPostProcessLpvIndirectPS<true>,TEXT("/Engine/Private/PostProcessLpvIndirect.usf"),TEXT("MainPS"),SF_Pixel);

class FPostProcessLpvDirectionalOcclusionPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessLpvDirectionalOcclusionPS, Global);

	//@todo-rco: Remove this when reenabling for OpenGL
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && IsLPVSupported(Platform); }

	/** Default constructor. */
	FPostProcessLpvDirectionalOcclusionPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderResourceParameter LpvVolumeTextureSampler;
	FShaderResourceParameter AOVolumeTextureSRVParameter;

	FDeferredPixelShaderParameters DeferredParameters;

	/** Initialization constructor. */
	FPostProcessLpvDirectionalOcclusionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		LpvVolumeTextureSampler.Bind(Initializer.ParameterMap, TEXT("gLpv3DTextureSampler"));
		AOVolumeTextureSRVParameter.Bind( Initializer.ParameterMap, TEXT("gAOVolumeTexture") );
	}

	void SetParameters(	
		FTextureRHIParamRef AOVolumeTextureSRVIn, 
		FLpvReadUniformBufferRef LpvUniformBuffer, 
		const FRenderingCompositePassContext& Context )
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		SetUniformBufferParameter(Context.RHICmdList, ShaderRHI, GetUniformBufferParameter<FLpvReadUniformBufferParameters>(), LpvUniformBuffer);

		if ( AOVolumeTextureSRVParameter.IsBound() )
		{
			Context.RHICmdList.SetShaderTexture(ShaderRHI, AOVolumeTextureSRVParameter.GetBaseIndex(), AOVolumeTextureSRVIn );
		}
		Context.RHICmdList.SetShaderSampler(ShaderRHI, LpvVolumeTextureSampler.GetBaseIndex(), TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI() );

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View, MD_PostProcess);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << LpvVolumeTextureSampler;
		Ar << DeferredParameters;
		Ar << AOVolumeTextureSRVParameter;
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessLpvDirectionalOcclusionPS,TEXT("/Engine/Private/PostProcessLpvIndirect.usf"),TEXT("DirectionalOcclusionPS"),SF_Pixel);

void FRCPassPostProcessLpvIndirect::Process(FRenderingCompositePassContext& Context)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);

	{
		FRenderingCompositeOutput* OutputOfMyInput = GetInput(ePId_Input0)->GetOutput();
		PassOutputs[0].PooledRenderTarget = OutputOfMyInput->PooledRenderTarget;
		OutputOfMyInput->RenderTargetDesc.DebugName = PassOutputs[0].RenderTargetDesc.DebugName;
		PassOutputs[0].RenderTargetDesc = OutputOfMyInput->RenderTargetDesc;

		check(PassOutputs[0].RenderTargetDesc.Extent.X);
		check(PassOutputs[0].RenderTargetDesc.Extent.Y);
	}

	const FFinalPostProcessSettings& PostprocessSettings = Context.View.FinalPostProcessSettings;
	const FSceneView& View = Context.View;

	FSceneViewState* ViewState = (FSceneViewState*)View.State;

	if(!ViewState)
	{
		return;
	}

	// This check should be inclusive to stereo views
	const bool bIncludeStereoViews = true;
	FLightPropagationVolume* Lpv = ViewState->GetLightPropagationVolume(Context.GetFeatureLevel(), bIncludeStereoViews);

	const FLightPropagationVolumeSettings& LPVSettings = PostprocessSettings.BlendableManager.GetSingleFinalDataConst<FLightPropagationVolumeSettings>();

	if(!Lpv || LPVSettings.LPVIntensity == 0.0f)
	{
		return;
	}

	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntRect SrcRect = View.ViewRect;
	// todo: view size should scale with input texture size so we can do SSAO in half resolution as well
	FIntRect DestRect = View.ViewRect;
	FIntPoint DestSize = DestRect.Size();

	const bool bMixing = CVarLPVMixing.GetValueOnRenderThread() != 0;
	// Apply specular separately if we're mixing reflection environment with indirect lighting
	const bool bApplySeparateSpecularRT = View.Family->EngineShowFlags.ReflectionEnvironment && bMixing;

	const FSceneRenderTargetItem& DestColorRenderTarget = SceneContext.GetSceneColor()->GetRenderTargetItem();
	const FSceneRenderTargetItem& DestSpecularRenderTarget = SceneContext.LightAccumulation->GetRenderTargetItem();

	const FSceneRenderTargetItem& DestDirectionalOcclusionRenderTarget = SceneContext.DirectionalOcclusion->GetRenderTargetItem();

	// Make sure the LPV Update has completed
	Lpv->InsertGPUWaitForAsyncUpdate(Context.RHICmdList);

	if ( LPVSettings.LPVDirectionalOcclusionIntensity > 0.0001f )
	{
		DoDirectionalOcclusionPass(Context);
	}

	FTextureRHIParamRef RenderTargets[2];
	RenderTargets[0] = DestColorRenderTarget.TargetableTexture;
	RenderTargets[1] = DestSpecularRenderTarget.TargetableTexture;

	// Set the view family's render target/viewport.
	// If specular not applied: set only color target
	uint32 NumRenderTargets = 1; 
	if ( bApplySeparateSpecularRT ) 
	{
		NumRenderTargets = 2;
	}

	SetRenderTargets(Context.RHICmdList, NumRenderTargets, RenderTargets, 0, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilNop);
	Context.SetViewportAndCallRHI(View.ViewRect);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	// set the state
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB,BO_Add,BF_One,BF_One,BO_Add,BF_One,BF_One>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false,CF_Always>::GetRHI();

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);

	FPostProcessLpvIndirectPS* PixelShader = NULL;
	if ( bApplySeparateSpecularRT )
	{
		TShaderMapRef< TPostProcessLpvIndirectPS<true> > PixelShaderWithSpecular(Context.GetShaderMap());
		PixelShader = (FPostProcessLpvIndirectPS*)*PixelShaderWithSpecular;
	}
	else
	{
		TShaderMapRef< TPostProcessLpvIndirectPS<false> > PixelShaderNoSpecular(Context.GetShaderMap());
		PixelShader = (FPostProcessLpvIndirectPS*)*PixelShaderNoSpecular;
	}

	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	FLpvReadUniformBufferParameters	LpvReadUniformBufferParams;
	FLpvReadUniformBufferRef LpvReadUniformBuffer;

	LpvReadUniformBufferParams = Lpv->GetReadUniformBufferParams();
	LpvReadUniformBuffer = FLpvReadUniformBufferRef::CreateUniformBufferImmediate( LpvReadUniformBufferParams, UniformBuffer_SingleDraw ); 

	FTextureRHIParamRef LpvBufferSrvs[7];
	for ( int i = 0; i < 7; i++ ) 
	{
		LpvBufferSrvs[i] = Lpv->GetLpvBufferSrv(i);
	}

	PixelShader->SetParameters(Context.RHICmdList, LpvBufferSrvs, Lpv->GetAOVolumeTextureSRV(), LpvReadUniformBuffer, Context );

	{
		SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessLpvIndirect );

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
			Context.HasHmdMesh());

		Context.RHICmdList.CopyToResolveTarget(DestColorRenderTarget.TargetableTexture, DestColorRenderTarget.ShaderResourceTexture, false, FResolveParams());
		if(bApplySeparateSpecularRT)
		{
			Context.RHICmdList.CopyToResolveTarget(DestSpecularRenderTarget.TargetableTexture, DestSpecularRenderTarget.ShaderResourceTexture, false, FResolveParams());
		}
	}

	if ( LPVSettings.LPVDirectionalOcclusionIntensity > 0.0001f )
	{
		GRenderTargetPool.VisualizeTexture.SetCheckPoint(Context.RHICmdList, SceneContext.DirectionalOcclusion);
	}
}

void FRCPassPostProcessLpvIndirect::DoDirectionalOcclusionPass(FRenderingCompositePassContext& Context) const
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);

	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessLpvDirectionalOcclusion );
	const FSceneRenderTargetItem& DestDirectionalOcclusionRenderTarget = SceneContext.DirectionalOcclusion->GetRenderTargetItem();
	const FViewInfo& View = Context.View;
	FSceneViewState* ViewState = (FSceneViewState*)View.State;

	if(!ViewState)
	{
		return;
	}

	const FFinalPostProcessSettings& PostprocessSettings = Context.View.FinalPostProcessSettings;
	const FLightPropagationVolumeSettings& LPVSettings = PostprocessSettings.BlendableManager.GetSingleFinalDataConst<FLightPropagationVolumeSettings>();
	
	FLightPropagationVolume* Lpv = ViewState->GetLightPropagationVolume(Context.GetFeatureLevel());

	if(!Lpv || LPVSettings.LPVIntensity == 0.0f)
	{
		return;
	}

	FTextureRHIParamRef RenderTarget = DestDirectionalOcclusionRenderTarget.TargetableTexture;

	SetRenderTargets(Context.RHICmdList, 1, &RenderTarget, NULL, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilNop);

	Context.SetViewportAndCallRHI(View.ViewRect);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false,CF_Always>::GetRHI();
	TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

	FLpvReadUniformBufferParameters	LpvReadUniformBufferParams;
	FLpvReadUniformBufferRef LpvReadUniformBuffer;

	TShaderMapRef< FPostProcessLpvDirectionalOcclusionPS > PixelShader(View.ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

	LpvReadUniformBufferParams = Lpv->GetReadUniformBufferParams();
	LpvReadUniformBuffer = FLpvReadUniformBufferRef::CreateUniformBufferImmediate( LpvReadUniformBufferParams, UniformBuffer_SingleDraw ); 

	PixelShader->SetParameters( Lpv->GetAOVolumeTextureSRV(), LpvReadUniformBuffer, Context );

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
		Context.HasHmdMesh());
}

FPooledRenderTargetDesc FRCPassPostProcessLpvIndirect::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	// we assume this pass is additively blended with the scene color so this data is not needed
	FPooledRenderTargetDesc Ret;

	Ret.DebugName = TEXT("LpvIndirect");

	return Ret;
}

void FRCPassPostProcessVisualizeLPV::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, VisualizeLPV);

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	
//	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	const TRefCountPtr<IPooledRenderTarget> RenderTarget = GetInput(ePId_Input0)->GetOutput()->PooledRenderTarget;
	const FSceneRenderTargetItem& DestRenderTarget = RenderTarget->GetRenderTargetItem();

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	{
		FRenderTargetTemp TempRenderTarget(View, (const FTexture2DRHIRef&)DestRenderTarget.TargetableTexture);
		FCanvas Canvas(&TempRenderTarget, NULL, ViewFamily.CurrentRealTime, ViewFamily.CurrentWorldTime, ViewFamily.DeltaWorldTime, View.GetFeatureLevel());

		float X = 30;
		float Y = 28;
		const float YStep = 14;
		const float ColumnWidth = 250;

		Canvas.DrawShadowedString( X, Y += YStep, TEXT("VisualizeLightPropagationVolume"), GetStatsFont(), FLinearColor(0.2f, 0.2f, 1));

		Y += YStep;

		const FLightPropagationVolumeSettings& Dest = View.FinalPostProcessSettings.BlendableManager.GetSingleFinalDataConst<FLightPropagationVolumeSettings>();

#define ENTRY(name)\
		Canvas.DrawShadowedString( X, Y += YStep, TEXT(#name) TEXT(":"), GetStatsFont(), FLinearColor(1, 1, 1));\
		Canvas.DrawShadowedString( X + ColumnWidth, Y, *FString::Printf(TEXT("%g"), Dest.name), GetStatsFont(), FLinearColor(1, 1, 1));

		ENTRY(LPVIntensity)
		ENTRY(LPVVplInjectionBias)
		ENTRY(LPVSize)
		ENTRY(LPVSecondaryOcclusionIntensity)
		ENTRY(LPVSecondaryBounceIntensity)
		ENTRY(LPVGeometryVolumeBias)
		ENTRY(LPVEmissiveInjectionIntensity)
		ENTRY(LPVDirectionalOcclusionIntensity)
		ENTRY(LPVDirectionalOcclusionRadius)
		ENTRY(LPVDiffuseOcclusionExponent)
		ENTRY(LPVSpecularOcclusionExponent)
		ENTRY(LPVDiffuseOcclusionIntensity)
		ENTRY(LPVSpecularOcclusionIntensity)
#undef ENTRY

		Canvas.Flush_RenderThread(Context.RHICmdList);
	}

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
	
	// to satify following passws
	FRenderingCompositeOutput* Output = GetOutput(ePId_Output0);
	
	Output->PooledRenderTarget = RenderTarget;
}

FPooledRenderTargetDesc FRCPassPostProcessVisualizeLPV::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();

	// we assume this pass is additively blended with the scene color so this data is not needed
//	FPooledRenderTargetDesc Ret;

	Ret.DebugName = TEXT("VisualizeLPV");

	return Ret;
}
