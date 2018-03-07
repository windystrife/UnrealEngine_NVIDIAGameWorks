// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessDeferredDecals.cpp: Deferred Decals implementation.
=============================================================================*/

#include "CompositionLighting/PostProcessDeferredDecals.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "DecalRenderingShared.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"


static TAutoConsoleVariable<int32> CVarGenerateDecalRTWriteMaskTexture(
	TEXT("r.Decal.GenerateRTWriteMaskTexture"),
	1,
	TEXT("Turn on or off generation of the RT write mask texture for decals\n"),
	ECVF_Default);




class FRTWriteMaskDecodeCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRTWriteMaskDecodeCS, Global);

public:
	static const uint32 ThreadGroupSizeX = 8;
	static const uint32 ThreadGroupSizeY = 8;

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadGroupSizeY);

		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FRTWriteMaskDecodeCS() {}

public:
	FShaderParameter OutCombinedRTWriteMask;		// UAV
	FShaderResourceParameter RTWriteMaskInput0;				// SRV 
	FShaderResourceParameter RTWriteMaskInput1;				// SRV 
	FShaderResourceParameter RTWriteMaskInput2;				// SRV 
	FShaderParameter UtilizeMask;				// SRV 

	FRTWriteMaskDecodeCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		RTWriteMaskDimensions.Bind(Initializer.ParameterMap, TEXT("RTWriteMaskDimensions"));
		OutCombinedRTWriteMask.Bind(Initializer.ParameterMap, TEXT("OutCombinedRTWriteMask"));
		RTWriteMaskInput0.Bind(Initializer.ParameterMap, TEXT("RTWriteMaskInput0"));
		RTWriteMaskInput1.Bind(Initializer.ParameterMap, TEXT("RTWriteMaskInput1"));
		RTWriteMaskInput2.Bind(Initializer.ParameterMap, TEXT("RTWriteMaskInput2"));
		UtilizeMask.Bind(Initializer.ParameterMap, TEXT("UtilizeMask"));
	}

	void SetCS(FRHICommandList& RHICmdList, const FRenderingCompositePassContext& Context, FIntPoint WriteMaskDimensions)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		//PostprocessParameter.SetCS(ShaderRHI, Context, Context.RHICmdList, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		SetShaderValue(Context.RHICmdList, ShaderRHI, RTWriteMaskDimensions, WriteMaskDimensions);
		SetSRVParameter(Context.RHICmdList, ShaderRHI, RTWriteMaskInput0, SceneContext.DBufferA->GetRenderTargetItem().RTWriteMaskBufferRHI_SRV);
		SetSRVParameter(Context.RHICmdList, ShaderRHI, RTWriteMaskInput1, SceneContext.DBufferB->GetRenderTargetItem().RTWriteMaskBufferRHI_SRV);
		SetSRVParameter(Context.RHICmdList, ShaderRHI, RTWriteMaskInput2, SceneContext.DBufferC->GetRenderTargetItem().RTWriteMaskBufferRHI_SRV);
		int32 UseMask = CVarGenerateDecalRTWriteMaskTexture.GetValueOnRenderThread();
		SetShaderValue(Context.RHICmdList, ShaderRHI, UtilizeMask, UseMask);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << RTWriteMaskDimensions;
		Ar << OutCombinedRTWriteMask;
		Ar << RTWriteMaskInput0;
		Ar << RTWriteMaskInput1;
		Ar << RTWriteMaskInput2;
		Ar << UtilizeMask;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter			RTWriteMaskDimensions;
};

IMPLEMENT_SHADER_TYPE(, FRTWriteMaskDecodeCS, TEXT("/Engine/Private/RTWriteMaskDecode.usf"), TEXT("RTWriteMaskCombineMain"), SF_Compute);

static TAutoConsoleVariable<float> CVarStencilSizeThreshold(
	TEXT("r.Decal.StencilSizeThreshold"),
	0.1f,
	TEXT("Control a per decal stencil pass that allows to large (screen space) decals faster. It adds more overhead per decals so this\n")
	TEXT("  <0: optimization is disabled\n")
	TEXT("   0: optimization is enabled no matter how small (screen space) the decal is\n")
	TEXT("0..1: optimization is enabled, value defines the minimum size (screen space) to trigger the optimization (default 0.1)")
);

enum EDecalDepthInputState
{
	DDS_Undefined,
	DDS_Always,
	DDS_DepthTest,
	DDS_DepthAlways_StencilEqual1,
	DDS_DepthAlways_StencilEqual1_IgnoreMask,
	DDS_DepthAlways_StencilEqual0,
	DDS_DepthTest_StencilEqual1,
	DDS_DepthTest_StencilEqual1_IgnoreMask,
	DDS_DepthTest_StencilEqual0,
};

struct FDecalDepthState
{
	EDecalDepthInputState DepthTest;
	bool bDepthOutput;

	FDecalDepthState()
		: DepthTest(DDS_Undefined)
		, bDepthOutput(false)
	{
	}

	bool operator !=(const FDecalDepthState &rhs) const
	{
		return DepthTest != rhs.DepthTest || bDepthOutput != rhs.bDepthOutput;
	}
};

enum EDecalRasterizerState
{
	DRS_Undefined,
	DRS_CCW,
	DRS_CW,
};

// @param RenderState 0:before BasePass, 1:before lighting, (later we could add "after lighting" and multiply)
FBlendStateRHIParamRef GetDecalBlendState(const ERHIFeatureLevel::Type SMFeatureLevel, EDecalRenderStage InDecalRenderStage, EDecalBlendMode DecalBlendMode, bool bHasNormal)
{
	if (InDecalRenderStage == DRS_BeforeBasePass)
	{
		// before base pass (for DBuffer decals)

		if (SMFeatureLevel == ERHIFeatureLevel::SM4)
		{
			// DX10 doesn't support masking/using different blend modes per MRT.
			// We set the opacity in the shader to 0 so we can use the same frame buffer blend.

			return TStaticBlendState<
				CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha,
				CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha,
				CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha >::GetRHI();
		}
		else
		{
			// see DX10 comment above
			// As we set the opacity in the shader we don't need to set different frame buffer blend modes but we like to hint to the driver that we
			// don't need to output there. We also could replace this with many SetRenderTarget calls but it might be slower (needs to be tested).

			switch (DecalBlendMode)
			{
			case DBM_DBuffer_ColorNormalRoughness:
				return TStaticBlendState<
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha,
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha,
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha >::GetRHI();

			case DBM_DBuffer_Color:
				// we can optimize using less MRT later
				return TStaticBlendState<
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha,
					CW_RGBA, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
					CW_RGBA, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI();

			case DBM_DBuffer_ColorNormal:
				// we can optimize using less MRT later
				return TStaticBlendState<
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha,
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha,
					CW_RGBA, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One >::GetRHI();

			case DBM_DBuffer_ColorRoughness:
				// we can optimize using less MRT later
				return TStaticBlendState<
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha,
					CW_RGBA, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha >::GetRHI();

			case DBM_DBuffer_Normal:
				// we can optimize using less MRT later
				return TStaticBlendState<
					CW_RGBA, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha,
					CW_RGBA, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI();

			case DBM_DBuffer_NormalRoughness:
				// we can optimize using less MRT later
				return TStaticBlendState<
					CW_RGBA, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha,
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha >::GetRHI();

			case DBM_DBuffer_Roughness:
				// we can optimize using less MRT later
				return TStaticBlendState<
					CW_RGBA, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
					CW_RGBA, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha >::GetRHI();

			default:
				// the decal type should not be rendered in this pass - internal error
				check(0);
				return nullptr;
			}
		}
	}
	else if (InDecalRenderStage == DRS_AfterBasePass)
	{
		ensure(DecalBlendMode == DBM_Volumetric_DistanceFunction);

		return TStaticBlendState<>::GetRHI();
	}
	else
	{
		// before lighting (for non DBuffer decals)

		switch (DecalBlendMode)
		{
		case DBM_Translucent:
			// @todo: Feature Level 10 does not support separate blends modes for each render target. This could result in the
			// translucent and stain blend modes looking incorrect when running in this mode.
			if (GSupportsSeparateRenderTargetBlendState)
			{
				if (bHasNormal)
				{
					return TStaticBlendState<
						CW_RGB, BO_Add, BF_SourceAlpha, BF_One, BO_Add, BF_Zero, BF_One,	// Emissive
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Normal
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One		// BaseColor
					>::GetRHI();
				}
				else
				{
					return TStaticBlendState<
						CW_RGB, BO_Add, BF_SourceAlpha, BF_One, BO_Add, BF_Zero, BF_One,	// Emissive
						CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,	// Normal
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One		// BaseColor
					>::GetRHI();
				}
			}
			else if (SMFeatureLevel == ERHIFeatureLevel::SM4)
			{
				return TStaticBlendState<
					CW_RGB, BO_Add, BF_SourceAlpha, BF_One, BO_Add, BF_Zero, BF_One,	// Emissive
					CW_RGB, BO_Add, BF_SourceAlpha, BF_One, BO_Add, BF_Zero, BF_One,	// Normal
					CW_RGB, BO_Add, BF_SourceAlpha, BF_One, BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
					CW_RGB, BO_Add, BF_SourceAlpha, BF_One, BO_Add, BF_Zero, BF_One		// BaseColor
				>::GetRHI();
			}

		case DBM_Stain:
			if (GSupportsSeparateRenderTargetBlendState)
			{
				if (bHasNormal)
				{
					return TStaticBlendState<
						CW_RGB, BO_Add, BF_SourceAlpha, BF_One, BO_Add, BF_Zero, BF_One,	// Emissive
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Normal
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
						CW_RGB, BO_Add, BF_DestColor, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One		// BaseColor
					>::GetRHI();
				}
				else
				{
					return TStaticBlendState<
						CW_RGB, BO_Add, BF_SourceAlpha, BF_One, BO_Add, BF_Zero, BF_One,	// Emissive
						CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,	// Normal
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
						CW_RGB, BO_Add, BF_DestColor, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One		// BaseColor
					>::GetRHI();
				}
			}
			else if (SMFeatureLevel == ERHIFeatureLevel::SM4)
			{
				return TStaticBlendState<
					CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Emissive
					CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Normal
					CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
					CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One		// BaseColor
				>::GetRHI();
			}

		case DBM_Normal:
			return TStaticBlendState< CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha >::GetRHI();


		case DBM_Emissive:
			return TStaticBlendState< CW_RGB, BO_Add, BF_SourceAlpha, BF_One >::GetRHI();

		default:
			// the decal type should not be rendered in this pass - internal error
			check(0);
			return nullptr;
		}
	}
}

bool RenderPreStencil(FRenderingCompositePassContext& Context, const FMatrix& ComponentToWorldMatrix, const FMatrix& FrustumComponentToClip)
{
	const FViewInfo& View = Context.View;

	float Distance = (View.ViewMatrices.GetViewOrigin() - ComponentToWorldMatrix.GetOrigin()).Size();
	float Radius = ComponentToWorldMatrix.GetMaximumAxisScale();

	// if not inside
	if (Distance > Radius)
	{
		float EstimatedDecalSize = Radius / Distance;

		float StencilSizeThreshold = CVarStencilSizeThreshold.GetValueOnRenderThread();

		// Check if it's large enough on screen
		if (EstimatedDecalSize < StencilSizeThreshold)
		{
			return false;
		}
	}

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// Set states, the state cache helps us avoiding redundant sets
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();

	// all the same to have DX10 working
	GraphicsPSOInit.BlendState = TStaticBlendState<
		CW_NONE, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Emissive
		CW_NONE, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Normal
		CW_NONE, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
		CW_NONE, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One		// BaseColor
	>::GetRHI();

	// Carmack's reverse the sandbox stencil bit on the bounds
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
		false, CF_LessEqual,
		true, CF_Always, SO_Keep, SO_Keep, SO_Invert,
		true, CF_Always, SO_Keep, SO_Keep, SO_Invert,
		STENCIL_SANDBOX_MASK, STENCIL_SANDBOX_MASK
	>::GetRHI();

	FDecalRendering::SetVertexShaderOnly(Context.RHICmdList, GraphicsPSOInit, View, FrustumComponentToClip);
	Context.RHICmdList.SetStencilRef(0);

	// Set stream source after updating cached strides
	Context.RHICmdList.SetStreamSource(0, GetUnitCubeVertexBuffer(), 0);

	// Render decal mask
	Context.RHICmdList.DrawIndexedPrimitive(GetUnitCubeIndexBuffer(), PT_TriangleList, 0, 0, 8, 0, ARRAY_COUNT(GCubeIndices) / 3, 1);

	return true;
}

bool IsDBufferEnabled()
{
	static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DBuffer"));

	return CVar->GetValueOnRenderThread() > 0;
}


static EDecalRasterizerState ComputeDecalRasterizerState(bool bInsideDecal, bool bIsInverted, const FViewInfo& View)
{
	bool bClockwise = bInsideDecal;

	if (View.bReverseCulling)
	{
		bClockwise = !bClockwise;
	}

	if (bIsInverted)
	{
		bClockwise = !bClockwise;
	}
	return bClockwise ? DRS_CW : DRS_CCW;
}

FRCPassPostProcessDeferredDecals::FRCPassPostProcessDeferredDecals(EDecalRenderStage InDecalRenderStage)
	: CurrentStage(InDecalRenderStage)
{
}

static FDecalDepthState ComputeDecalDepthState(EDecalRenderStage LocalDecalStage, bool bInsideDecal, bool bThisDecalUsesStencil)
{
	FDecalDepthState Ret;

	Ret.bDepthOutput = (LocalDecalStage == DRS_AfterBasePass);

	if (Ret.bDepthOutput)
	{
		// can be made one enum
		Ret.DepthTest = DDS_DepthTest;
		return Ret;
	}

	const bool bGBufferDecal = LocalDecalStage == DRS_BeforeLighting;

	if (bInsideDecal)
	{
		if (bThisDecalUsesStencil)
		{
			Ret.DepthTest = bGBufferDecal ? DDS_DepthAlways_StencilEqual1 : DDS_DepthAlways_StencilEqual1_IgnoreMask;
		}
		else
		{
			Ret.DepthTest = bGBufferDecal ? DDS_DepthAlways_StencilEqual0 : DDS_Always;
		}
	}
	else
	{
		if (bThisDecalUsesStencil)
		{
			Ret.DepthTest = bGBufferDecal ? DDS_DepthTest_StencilEqual1 : DDS_DepthTest_StencilEqual1_IgnoreMask;
		}
		else
		{
			Ret.DepthTest = bGBufferDecal ? DDS_DepthTest_StencilEqual0 : DDS_DepthTest;
		}
	}

	return Ret;
}

static FDepthStencilStateRHIParamRef GetDecalDepthState(uint32& StencilRef, FDecalDepthState DecalDepthState)
{
	switch (DecalDepthState.DepthTest)
	{
	case DDS_DepthAlways_StencilEqual1:
		check(!DecalDepthState.bDepthOutput);			// todo
		StencilRef = STENCIL_SANDBOX_MASK | GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1);
		return TStaticDepthStencilState<
			false, CF_Always,
			true, CF_Equal, SO_Zero, SO_Zero, SO_Zero,
			true, CF_Equal, SO_Zero, SO_Zero, SO_Zero,
			STENCIL_SANDBOX_MASK | GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1), STENCIL_SANDBOX_MASK>::GetRHI();

	case DDS_DepthAlways_StencilEqual1_IgnoreMask:
		check(!DecalDepthState.bDepthOutput);			// todo
		StencilRef = STENCIL_SANDBOX_MASK;
		return TStaticDepthStencilState<
			false, CF_Always,
			true, CF_Equal, SO_Zero, SO_Zero, SO_Zero,
			true, CF_Equal, SO_Zero, SO_Zero, SO_Zero,
			STENCIL_SANDBOX_MASK, STENCIL_SANDBOX_MASK>::GetRHI();

	case DDS_DepthAlways_StencilEqual0:
		check(!DecalDepthState.bDepthOutput);			// todo
		StencilRef = GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1);
		return TStaticDepthStencilState<
			false, CF_Always,
			true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
			false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
			STENCIL_SANDBOX_MASK | GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1), 0x00>::GetRHI();

	case DDS_Always:
		check(!DecalDepthState.bDepthOutput);			// todo 
		StencilRef = 0;
		return TStaticDepthStencilState<false, CF_Always>::GetRHI();

	case DDS_DepthTest_StencilEqual1:
		check(!DecalDepthState.bDepthOutput);			// todo
		StencilRef = STENCIL_SANDBOX_MASK | GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1);
		return TStaticDepthStencilState<
			false, CF_DepthNearOrEqual,
			true, CF_Equal, SO_Zero, SO_Zero, SO_Zero,
			true, CF_Equal, SO_Zero, SO_Zero, SO_Zero,
			STENCIL_SANDBOX_MASK | GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1), STENCIL_SANDBOX_MASK>::GetRHI();

	case DDS_DepthTest_StencilEqual1_IgnoreMask:
		check(!DecalDepthState.bDepthOutput);			// todo
		StencilRef = STENCIL_SANDBOX_MASK;
		return TStaticDepthStencilState<
			false, CF_DepthNearOrEqual,
			true, CF_Equal, SO_Zero, SO_Zero, SO_Zero,
			true, CF_Equal, SO_Zero, SO_Zero, SO_Zero,
			STENCIL_SANDBOX_MASK, STENCIL_SANDBOX_MASK>::GetRHI();

	case DDS_DepthTest_StencilEqual0:
		check(!DecalDepthState.bDepthOutput);			// todo
		StencilRef = GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1);
		return TStaticDepthStencilState<
			false, CF_DepthNearOrEqual,
			true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
			false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
			STENCIL_SANDBOX_MASK | GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1), 0x00>::GetRHI();

	case DDS_DepthTest:
		if (DecalDepthState.bDepthOutput)
		{
			StencilRef = 0;
			return TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI();
		}
		else
		{
			StencilRef = 0;
			return TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
		}

	default:
		check(0);
		return nullptr;
	}
}

static FRasterizerStateRHIParamRef GetDecalRasterizerState(EDecalRasterizerState DecalRasterizerState)
{
	switch (DecalRasterizerState)
	{
	case DRS_CW: return TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
	case DRS_CCW: return TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI();
	default: check(0); return nullptr;
	}
}

static inline bool IsStencilOptimizationAvailable(EDecalRenderStage RenderStage)
{
	return RenderStage == DRS_BeforeLighting || RenderStage == DRS_BeforeBasePass;
}

const TCHAR* GetStageName(EDecalRenderStage Stage)
{
	// could be implemented with enum reflections as well

	switch (Stage)
	{
	case DRS_BeforeBasePass: return TEXT("DRS_BeforeBasePass");
	case DRS_AfterBasePass: return TEXT("DRS_AfterBasePass");
	case DRS_BeforeLighting: return TEXT("DRS_BeforeLighting");
	case DRS_Mobile: return TEXT("DRS_Mobile");
	}
	return TEXT("<UNKNOWN>");
}


void FRCPassPostProcessDeferredDecals::DecodeRTWriteMask(FRenderingCompositePassContext& Context)
{
	// @todo: get these values from the RHI?
	const uint32 MaskTileSizeX = 8;
	const uint32 MaskTileSizeY = 8;

	check(GSupportsRenderTargetWriteMask);

	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FTextureRHIRef DBufferTex = SceneContext.DBufferA->GetRenderTargetItem().TargetableTexture;

	FIntPoint RTWriteMaskDims(
		FMath::DivideAndRoundUp(DBufferTex->GetTexture2D()->GetSizeX(), MaskTileSizeX),
		FMath::DivideAndRoundUp(DBufferTex->GetTexture2D()->GetSizeY(), MaskTileSizeY));

	// allocate the DBufferMask from the render target pool.
	FPooledRenderTargetDesc MaskDesc(FPooledRenderTargetDesc::Create2DDesc(RTWriteMaskDims,
		PF_R8_UINT,
		FClearValueBinding::White,
		TexCreate_None | GFastVRamConfig.DBufferMask,
		TexCreate_UAV | TexCreate_RenderTargetable,
		false));

	GRenderTargetPool.FindFreeElement(Context.RHICmdList, MaskDesc, SceneContext.DBufferMask, TEXT("DBufferMask"));

	FIntRect ViewRect(0, 0, DBufferTex->GetTexture2D()->GetSizeX(), DBufferTex->GetTexture2D()->GetSizeY());

	TShaderMapRef< FRTWriteMaskDecodeCS > ComputeShader(Context.GetShaderMap());

	SetRenderTarget(Context.RHICmdList, FTextureRHIRef(), FTextureRHIRef());
	Context.SetViewportAndCallRHI(ViewRect);
	Context.RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

	// set destination
	Context.RHICmdList.SetUAVParameter(ComputeShader->GetComputeShader(), ComputeShader->OutCombinedRTWriteMask.GetBaseIndex(), SceneContext.DBufferMask->GetRenderTargetItem().UAV);
	ComputeShader->SetCS(Context.RHICmdList, Context, RTWriteMaskDims);

	RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, SceneContext.DBufferMask->GetRenderTargetItem().UAV);
	{
		SCOPED_DRAW_EVENTF(Context.RHICmdList, DeferredDecals, TEXT("Combine DBuffer RTWriteMasks"));

		FIntPoint ThreadGroupCountValue(
			FMath::DivideAndRoundUp((uint32)RTWriteMaskDims.X, FRTWriteMaskDecodeCS::ThreadGroupSizeX),
			FMath::DivideAndRoundUp((uint32)RTWriteMaskDims.Y, FRTWriteMaskDecodeCS::ThreadGroupSizeY));

		DispatchComputeShader(Context.RHICmdList, *ComputeShader, ThreadGroupCountValue.X, ThreadGroupCountValue.Y, 1);
	}

	//	void FD3D11DynamicRHI::RHIGraphicsWaitOnAsyncComputeJob( uint32 FenceIndex )
	Context.RHICmdList.FlushComputeShaderCache();

	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, SceneContext.DBufferMask->GetRenderTargetItem().UAV);
	
    FTextureRHIParamRef Textures[3] =
	{
		SceneContext.DBufferA->GetRenderTargetItem().TargetableTexture,
		SceneContext.DBufferB->GetRenderTargetItem().TargetableTexture,
		SceneContext.DBufferC->GetRenderTargetItem().TargetableTexture
	};
	RHICmdList.TransitionResources(EResourceTransitionAccess::EMetaData, Textures, 3);
	
	// un-set destination
	Context.RHICmdList.SetUAVParameter(ComputeShader->GetComputeShader(), ComputeShader->OutCombinedRTWriteMask.GetBaseIndex(), NULL);
}


void FRCPassPostProcessDeferredDecals::Process(FRenderingCompositePassContext& Context)
{
	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	const bool bShaderComplexity = Context.View.Family->EngineShowFlags.ShaderComplexity;
	const bool bDBuffer = IsDBufferEnabled();
	const bool bStencilSizeThreshold = CVarStencilSizeThreshold.GetValueOnRenderThread() >= 0;

	SCOPED_DRAW_EVENTF(RHICmdList, DeferredDecals, TEXT("DeferredDecals %s"), GetStageName(CurrentStage));

	// this cast is safe as only the dedicated server implements this differently and this pass should not be executed on the dedicated server
	const FViewInfo& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	bool bNeedsDBufferTargets = false;

	if (CurrentStage == DRS_BeforeBasePass)
	{
		// before BasePass, only if DBuffer is enabled
		check(bDBuffer);

		// If we're rendering dbuffer decals but there are no decals in the scene, we avoid the 
		// clears/decompresses and set the targets to NULL		
		// The DBufferA-C will be replaced with dummy textures in FDeferredPixelShaderParameters
		if (ViewFamily.EngineShowFlags.Decals)
		{
			FScene& Scene = *(FScene*)ViewFamily.Scene;
			if (Scene.Decals.Num() > 0 || Context.View.MeshDecalPrimSet.NumPrims() > 0)
			{
				bNeedsDBufferTargets = true;
			}
		}

		// If we need dbuffer targets, initialize them
		if (bNeedsDBufferTargets)
		{
			FPooledRenderTargetDesc GBufferADesc;
			SceneContext.GetGBufferADesc(GBufferADesc);

			// DBuffer: Decal buffer
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(GBufferADesc.Extent,
				PF_B8G8R8A8,
				FClearValueBinding::None,
				TexCreate_None | GFastVRamConfig.DBufferA,
				TexCreate_ShaderResource | TexCreate_RenderTargetable,
				false,
				1,
				true,
				true));
			
			if (!SceneContext.DBufferA)
			{
				Desc.ClearValue = FClearValueBinding::Black;
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SceneContext.DBufferA, TEXT("DBufferA"));
			}

			if (!SceneContext.DBufferB)
			{
				Desc.Flags = TexCreate_None | GFastVRamConfig.DBufferB;
				Desc.ClearValue = FClearValueBinding(FLinearColor(128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SceneContext.DBufferB, TEXT("DBufferB"));
			}

			Desc.Format = PF_R8G8;

			if (!SceneContext.DBufferC)
			{
				Desc.Flags = TexCreate_None | GFastVRamConfig.DBufferC;
				Desc.ClearValue = FClearValueBinding(FLinearColor(0, 1, 0, 1));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SceneContext.DBufferC, TEXT("DBufferC"));
			}

			// we assume views are non overlapping, then we need to clear only once in the beginning, otherwise we would need to set scissor rects
			// and don't get FastClear any more.
			bool bFirstView = Context.View.Family->Views[0] == &Context.View;

			if (bFirstView)
			{
				SCOPED_DRAW_EVENT(RHICmdList, DBufferClear);

				FRHIRenderTargetView RenderTargets[3];
				RenderTargets[0] = FRHIRenderTargetView(SceneContext.DBufferA->GetRenderTargetItem().TargetableTexture, 0, -1, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore);
				RenderTargets[1] = FRHIRenderTargetView(SceneContext.DBufferB->GetRenderTargetItem().TargetableTexture, 0, -1, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore);
				RenderTargets[2] = FRHIRenderTargetView(SceneContext.DBufferC->GetRenderTargetItem().TargetableTexture, 0, -1, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore);

				FRHIDepthRenderTargetView DepthView(SceneContext.GetSceneDepthTexture(), ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::ENoAction, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::ENoAction, FExclusiveDepthStencil(FExclusiveDepthStencil::DepthRead_StencilWrite));

				FRHISetRenderTargetsInfo Info(3, RenderTargets, DepthView);
				RHICmdList.SetRenderTargetsAndClear(Info);
			}
		} // if ( bNeedsDBufferTargets )
	}

	bool bHasValidDBufferMask = false;

	if (ViewFamily.EngineShowFlags.Decals)
	{
		if (CurrentStage == DRS_BeforeBasePass || CurrentStage == DRS_BeforeLighting)
		{
			if (Context.View.MeshDecalPrimSet.NumPrims() > 0)
			{
				check(bNeedsDBufferTargets || CurrentStage != DRS_BeforeBasePass);
				RenderMeshDecals(Context, CurrentStage);
			}
		}

		FScene& Scene = *(FScene*)ViewFamily.Scene;

		//don't early return. Resolves must be run for fast clears to work.
		if (Scene.Decals.Num())
		{
			check(bNeedsDBufferTargets || CurrentStage != DRS_BeforeBasePass);
			FDecalRenderTargetManager RenderTargetManager(RHICmdList, Context.GetShaderPlatform(), CurrentStage);

			// Build a list of decals that need to be rendered for this view
			FTransientDecalRenderDataList SortedDecals;
			FDecalRendering::BuildVisibleDecalList(Scene, View, CurrentStage, SortedDecals);

			if (SortedDecals.Num() > 0)
			{
				SCOPED_DRAW_EVENTF(RHICmdList, DeferredDecalsInner, TEXT("DeferredDecalsInner %d/%d"), SortedDecals.Num(), Scene.Decals.Num());

				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

				// optimization to have less state changes
				EDecalRasterizerState LastDecalRasterizerState = DRS_Undefined;
				FDecalDepthState LastDecalDepthState;
				int32 LastDecalBlendMode = -1;
				int32 LastDecalHasNormal = -1; // Decal state can change based on its normal property.(SM5)
				uint32 StencilRef = 0;

				FDecalRenderingCommon::ERenderTargetMode LastRenderTargetMode = FDecalRenderingCommon::RTM_Unknown;
				const ERHIFeatureLevel::Type SMFeatureLevel = Context.GetFeatureLevel();

				SCOPED_DRAW_EVENT(RHICmdList, Decals);
				INC_DWORD_STAT_BY(STAT_Decals, SortedDecals.Num());

				for (int32 DecalIndex = 0, DecalCount = SortedDecals.Num(); DecalIndex < DecalCount; DecalIndex++)
				{
					const FTransientDecalRenderData& DecalData = SortedDecals[DecalIndex];
					const FDeferredDecalProxy& DecalProxy = *DecalData.DecalProxy;
					const FMatrix ComponentToWorldMatrix = DecalProxy.ComponentTrans.ToMatrixWithScale();
					const FMatrix FrustumComponentToClip = FDecalRendering::ComputeComponentToClipMatrix(View, ComponentToWorldMatrix);

					EDecalBlendMode DecalBlendMode = DecalData.DecalBlendMode;
					EDecalRenderStage LocalDecalStage = FDecalRenderingCommon::ComputeRenderStage(View.GetShaderPlatform(), DecalBlendMode);
					bool bStencilThisDecal = IsStencilOptimizationAvailable(LocalDecalStage);

					FDecalRenderingCommon::ERenderTargetMode CurrentRenderTargetMode = FDecalRenderingCommon::ComputeRenderTargetMode(View.GetShaderPlatform(), DecalBlendMode, DecalData.bHasNormal);

					if (bShaderComplexity)
					{
						CurrentRenderTargetMode = FDecalRenderingCommon::RTM_SceneColor;
						// we want additive blending for the ShaderComplexity mode
						DecalBlendMode = DBM_Emissive;
					}

					// Here we assume that GBuffer can only be WorldNormal since it is the only GBufferTarget handled correctly.
					if (RenderTargetManager.bGufferADirty && DecalData.MaterialResource->NeedsGBuffer())
					{
						RHICmdList.CopyToResolveTarget(SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture, SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture, true, FResolveParams());
						RenderTargetManager.TargetsToResolve[FDecalRenderTargetManager::GBufferAIndex] = nullptr;
						RenderTargetManager.bGufferADirty = false;
					}

					// fewer rendertarget switches if possible
					if (CurrentRenderTargetMode != LastRenderTargetMode)
					{
						LastRenderTargetMode = CurrentRenderTargetMode;

						RenderTargetManager.SetRenderTargetMode(CurrentRenderTargetMode, DecalData.bHasNormal);
						Context.SetViewportAndCallRHI(Context.View.ViewRect);
						RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
					}

					bool bThisDecalUsesStencil = false;

					if (bStencilThisDecal && bStencilSizeThreshold)
					{
						// note this is after a SetStreamSource (in if CurrentRenderTargetMode != LastRenderTargetMode) call as it needs to get the VB input
						bThisDecalUsesStencil = RenderPreStencil(Context, ComponentToWorldMatrix, FrustumComponentToClip);

						LastDecalRasterizerState = DRS_Undefined;
						LastDecalDepthState = FDecalDepthState();
						LastDecalBlendMode = -1;
					}

					const bool bBlendStateChange = DecalBlendMode != LastDecalBlendMode;// Has decal mode changed.
					const bool bDecalNormalChanged = GSupportsSeparateRenderTargetBlendState && // has normal changed for SM5 stain/translucent decals?
						(DecalBlendMode == DBM_Translucent || DecalBlendMode == DBM_Stain) &&
						(int32)DecalData.bHasNormal != LastDecalHasNormal;

					// fewer blend state changes if possible
					if (bBlendStateChange || bDecalNormalChanged)
					{
						LastDecalBlendMode = DecalBlendMode;
						LastDecalHasNormal = (int32)DecalData.bHasNormal;

						GraphicsPSOInit.BlendState = GetDecalBlendState(SMFeatureLevel, CurrentStage, (EDecalBlendMode)LastDecalBlendMode, DecalData.bHasNormal);
					}

					// todo
					const float ConservativeRadius = DecalData.ConservativeRadius;
					//			const int32 IsInsideDecal = ((FVector)View.ViewMatrices.ViewOrigin - ComponentToWorldMatrix.GetOrigin()).SizeSquared() < FMath::Square(ConservativeRadius * 1.05f + View.NearClippingDistance * 2.0f) + ( bThisDecalUsesStencil ) ? 2 : 0;
					const bool bInsideDecal = ((FVector)View.ViewMatrices.GetViewOrigin() - ComponentToWorldMatrix.GetOrigin()).SizeSquared() < FMath::Square(ConservativeRadius * 1.05f + View.NearClippingDistance * 2.0f);
					//			const bool bInsideDecal =  !(IsInsideDecal & 1);

					// update rasterizer state if needed
					{
						bool bReverseHanded = false;
						{
							// Account for the reversal of handedness caused by negative scale on the decal
							const auto& Scale3d = DecalProxy.ComponentTrans.GetScale3D();
							bReverseHanded = Scale3d[0] * Scale3d[1] * Scale3d[2] < 0.f;
						}
						EDecalRasterizerState DecalRasterizerState = ComputeDecalRasterizerState(bInsideDecal, bReverseHanded, View);

						if (LastDecalRasterizerState != DecalRasterizerState)
						{
							LastDecalRasterizerState = DecalRasterizerState;
							GraphicsPSOInit.RasterizerState = GetDecalRasterizerState(DecalRasterizerState);
						}
					}

					// update DepthStencil state if needed
					{
						FDecalDepthState DecalDepthState = ComputeDecalDepthState(LocalDecalStage, bInsideDecal, bThisDecalUsesStencil);

						if (LastDecalDepthState != DecalDepthState)
						{
							LastDecalDepthState = DecalDepthState;
							GraphicsPSOInit.DepthStencilState = GetDecalDepthState(StencilRef, DecalDepthState);
						}
					}

					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					FDecalRendering::SetShader(RHICmdList, GraphicsPSOInit, View, DecalData, FrustumComponentToClip);
					RHICmdList.SetStencilRef(StencilRef);

					RHICmdList.DrawIndexedPrimitive(GetUnitCubeIndexBuffer(), PT_TriangleList, 0, 0, 8, 0, ARRAY_COUNT(GCubeIndices) / 3, 1);
					RenderTargetManager.bGufferADirty |= (RenderTargetManager.TargetsToResolve[FDecalRenderTargetManager::GBufferAIndex] != nullptr);
				}

				// we don't modify stencil but if out input was having stencil for us (after base pass - we need to clear)
				// Clear stencil to 0, which is the assumed default by other passes
				DrawClearQuad(RHICmdList, false, FLinearColor(), false, 0, true, 0, SceneContext.GetSceneDepthSurface()->GetSizeXY(), FIntRect());
			}

			// This stops the targets from being resolved and decoded until the last view is rendered.
			// This is done so as to not run eliminate fast clear on the views before the end.
			bool bLastView = Context.View.Family->Views.Last() == &Context.View;
			if (CurrentStage == DRS_BeforeBasePass)
			{
				// combine DBuffer RTWriteMasks; will end up in one texture we can load from in the base pass PS and decide whether to do the actual work or not
				FTextureRHIParamRef Textures[3] =
				{
					SceneContext.DBufferA->GetRenderTargetItem().TargetableTexture,
					SceneContext.DBufferB->GetRenderTargetItem().TargetableTexture,
					SceneContext.DBufferC->GetRenderTargetItem().TargetableTexture
				};
				RenderTargetManager.FlushMetaData(Textures, 3);

				if (GSupportsRenderTargetWriteMask && bLastView)
				{
					DecodeRTWriteMask(Context);
					GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.DBufferMask);
					bHasValidDBufferMask = true;
				}
			}

			if (bLastView || !GSupportsRenderTargetWriteMask)
			{
				RenderTargetManager.ResolveTargets();
			}
		}

		if (CurrentStage == DRS_BeforeBasePass && bNeedsDBufferTargets)
		{
			// before BasePass
			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.DBufferA);
			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.DBufferB);
			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.DBufferC);
		}
	}

	if (CurrentStage == DRS_BeforeBasePass && !bHasValidDBufferMask)
	{
		// Return the DBufferMask to the render target pool.
		// FDeferredPixelShaderParameters will fall back to setting a white dummy mask texture.
		// This allows us to ignore the DBufferMask on frames without decals, without having to explicitly clear the texture.
		SceneContext.DBufferMask = nullptr;
	}
}

FPooledRenderTargetDesc FRCPassPostProcessDeferredDecals::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	// This pass creates it's own output so the compositing graph output isn't needed.
	FPooledRenderTargetDesc Ret;

	Ret.DebugName = TEXT("DeferredDecals");

	return Ret;
}

//
// FDecalRenderTargetManager class
//
void FDecalRenderTargetManager::ResolveTargets()
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	// If GBuffer A is dirty, mark it as needing resolve since the content of TargetsToResolve[GBufferAIndex] could have been nullified by modes like RTM_SceneColorAndGBufferNoNormal
	if (bGufferADirty)
	{
		TargetsToResolve[FDecalRenderTargetManager::GBufferAIndex] = SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture;
	}

	//those have been cleared or rendered to and need to be resolved
	TargetsToResolve[FDecalRenderTargetManager::DBufferAIndex] = SceneContext.DBufferA ? SceneContext.DBufferA->GetRenderTargetItem().TargetableTexture : nullptr;
	TargetsToResolve[FDecalRenderTargetManager::DBufferBIndex] = SceneContext.DBufferB ? SceneContext.DBufferB->GetRenderTargetItem().TargetableTexture : nullptr;
	TargetsToResolve[FDecalRenderTargetManager::DBufferCIndex] = SceneContext.DBufferC ? SceneContext.DBufferC->GetRenderTargetItem().TargetableTexture : nullptr;

	// resolve the targets we wrote to.
	FResolveParams ResolveParams;
	for (int32 i = 0; i < ResolveBufferMax; ++i)
	{
		if (TargetsToResolve[i])
		{
			RHICmdList.CopyToResolveTarget(TargetsToResolve[i], TargetsToResolve[i], true, ResolveParams);
		}
	}
}


FDecalRenderTargetManager::FDecalRenderTargetManager(FRHICommandList& InRHICmdList, EShaderPlatform ShaderPlatform, EDecalRenderStage CurrentStage)
	: RHICmdList(InRHICmdList)
	, bGufferADirty(false)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	for (uint32 i = 0; i < ResolveBufferMax; ++i)
	{
		TargetsToTransitionWritable[i] = true;
		TargetsToResolve[i] = nullptr;
	}

	if (SceneContext.DBufferA)
	{
		TargetsToResolve[DBufferAIndex] = SceneContext.DBufferA->GetRenderTargetItem().TargetableTexture;
	}
	if (SceneContext.DBufferB)
	{
		TargetsToResolve[DBufferBIndex] = SceneContext.DBufferB->GetRenderTargetItem().TargetableTexture;
	}

	if (SceneContext.DBufferC)
	{
		TargetsToResolve[DBufferCIndex] = SceneContext.DBufferC->GetRenderTargetItem().TargetableTexture;
	}

	if (!IsAnyForwardShadingEnabled(ShaderPlatform))
	{
		// Normal buffer is already dirty at this point and needs resolve before being read from (irrelevant for DBuffer).
		bGufferADirty = CurrentStage == DRS_AfterBasePass;
	}
}

void FDecalRenderTargetManager::SetRenderTargetMode(FDecalRenderingCommon::ERenderTargetMode CurrentRenderTargetMode, bool bHasNormal)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	// If GBufferA was resolved for read, and we want to write to it again.
	if (!bGufferADirty && IsWritingToGBufferA(CurrentRenderTargetMode))
	{
		// This is required to be compliant with RHISetRenderTargets resource transition code : const bool bAccessValid = !bReadable || LastFrameWritten != CurrentFrame;
		// If the normal buffer was resolved as a texture before, then bReadable && LastFrameWritten == CurrentFrame, and an error msg will be triggered. 
		// Which is not needed here since no more read will be done at this point (at least not before any other CopyToResolvedTarget).
		RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture);
	}

	// @todo Workaround Vulkan (always) or Mac with NV/Intel graphics driver bug requires we pointlessly bind into RT1 even though we don't write to it,
	// otherwise the writes to RT2 and RT3 go haywire. This isn't really possible to fix lower down the stack.
	const bool bRequiresDummyRenderTarget = PLATFORM_MAC || IsVulkanPlatform(GMaxRHIShaderPlatform);

	switch (CurrentRenderTargetMode)
	{
	case FDecalRenderingCommon::RTM_SceneColorAndGBufferWithNormal:
	case FDecalRenderingCommon::RTM_SceneColorAndGBufferNoNormal:
		TargetsToResolve[SceneColorIndex] = SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture;
		TargetsToResolve[GBufferAIndex] = bHasNormal ? SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture : (bRequiresDummyRenderTarget ? SceneContext.GBufferB->GetRenderTargetItem().TargetableTexture : nullptr);
		TargetsToResolve[GBufferBIndex] = SceneContext.GBufferB->GetRenderTargetItem().TargetableTexture;
		TargetsToResolve[GBufferCIndex] = SceneContext.GBufferC->GetRenderTargetItem().TargetableTexture;
		SetRenderTargets(RHICmdList, 4, TargetsToResolve, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, TargetsToTransitionWritable[CurrentRenderTargetMode]);
		break;

	case FDecalRenderingCommon::RTM_SceneColorAndGBufferDepthWriteWithNormal:
	case FDecalRenderingCommon::RTM_SceneColorAndGBufferDepthWriteNoNormal:
		TargetsToResolve[SceneColorIndex] = SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture;
		TargetsToResolve[GBufferAIndex] = bHasNormal ? SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture : (bRequiresDummyRenderTarget ? SceneContext.GBufferB->GetRenderTargetItem().TargetableTexture : nullptr);
		TargetsToResolve[GBufferBIndex] = SceneContext.GBufferB->GetRenderTargetItem().TargetableTexture;
		TargetsToResolve[GBufferCIndex] = SceneContext.GBufferC->GetRenderTargetItem().TargetableTexture;
		TargetsToResolve[GBufferEIndex] = SceneContext.GBufferE->GetRenderTargetItem().TargetableTexture;
		SetRenderTargets(RHICmdList, 5, TargetsToResolve, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthWrite_StencilWrite, TargetsToTransitionWritable[CurrentRenderTargetMode]);
		break;

	case FDecalRenderingCommon::RTM_GBufferNormal:
		TargetsToResolve[GBufferAIndex] = SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture;
		SetRenderTarget(RHICmdList, TargetsToResolve[GBufferAIndex], SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, TargetsToTransitionWritable[CurrentRenderTargetMode]);
		break;

	case FDecalRenderingCommon::RTM_SceneColor:
		TargetsToResolve[SceneColorIndex] = SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture;
		SetRenderTarget(RHICmdList, TargetsToResolve[SceneColorIndex], SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, TargetsToTransitionWritable[CurrentRenderTargetMode]);
		break;

	case FDecalRenderingCommon::RTM_DBuffer:
		TargetsToResolve[DBufferAIndex] = SceneContext.DBufferA->GetRenderTargetItem().TargetableTexture;
		TargetsToResolve[DBufferBIndex] = SceneContext.DBufferB->GetRenderTargetItem().TargetableTexture;
		TargetsToResolve[DBufferCIndex] = SceneContext.DBufferC->GetRenderTargetItem().TargetableTexture;
		SetRenderTargets(RHICmdList, 3, &TargetsToResolve[DBufferAIndex], SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, TargetsToTransitionWritable[CurrentRenderTargetMode]);
		break;

	default:
		check(0);
		break;
	}
	TargetsToTransitionWritable[CurrentRenderTargetMode] = false;
}



void FDecalRenderTargetManager::FlushMetaData(FTextureRHIParamRef* Textures, uint32 NumTextures)
{
	RHICmdList.TransitionResources(EResourceTransitionAccess::EMetaData, Textures, NumTextures);
}