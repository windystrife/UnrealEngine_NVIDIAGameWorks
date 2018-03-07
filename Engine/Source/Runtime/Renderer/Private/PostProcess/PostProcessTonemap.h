// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTonemap.h: Post processing tone mapping implementation, can add bloom.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RendererInterface.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "RHIStaticStates.h"
#include "GlobalShader.h"
#include "PostProcessParameters.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "PostProcess/PostProcessEyeAdaptation.h"

static float GrainHalton( int32 Index, int32 Base )
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

static void GrainRandomFromFrame(FVector* RESTRICT const Constant, uint32 FrameNumber)
{
	Constant->X = GrainHalton(FrameNumber & 1023, 2);
	Constant->Y = GrainHalton(FrameNumber & 1023, 3);
}


void FilmPostSetConstants(FVector4* RESTRICT const Constants, const uint32 TonemapperType16, const FPostProcessSettings* RESTRICT const FinalPostProcessSettings, bool bMobile);

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: SceneColor
// ePId_Input1: BloomCombined (not needed for bDoGammaOnly)
// ePId_Input2: EyeAdaptation (not needed for bDoGammaOnly)
// ePId_Input3: LUTsCombined (not needed for bDoGammaOnly)
class FRCPassPostProcessTonemap : public TRenderingCompositePassBase<4, 1>
{
public:
	// constructor
	FRCPassPostProcessTonemap(const FViewInfo& InView, bool bInDoGammaOnly, bool bDoEyeAdaptation, bool bHDROutput, bool InIsComputePass);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	virtual FComputeFenceRHIParamRef GetComputePassEndFence() const override { return AsyncEndFence; }

	bool bDoGammaOnly;
	bool bDoScreenPercentageInTonemapper;
private:
	bool bDoEyeAdaptation;
	bool bHDROutput;
	uint32 ConfigIndexPC;

	const FViewInfo& View;

	void SetShader(const FRenderingCompositePassContext& Context);

	template <typename TRHICmdList>
	void DispatchCS(TRHICmdList& RHICmdList, FRenderingCompositePassContext& Context, const FIntRect& DestRect, FUnorderedAccessViewRHIParamRef DestUAV, FTextureRHIParamRef EyeAdaptationTex);

	FComputeFenceRHIRef AsyncEndFence;
};



// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: SceneColor
// ePId_Input1: BloomCombined (not needed for bDoGammaOnly)
// ePId_Input2: Dof (not needed for bDoGammaOnly)
class FRCPassPostProcessTonemapES2 : public TRenderingCompositePassBase<3, 1>
{
public:
	FRCPassPostProcessTonemapES2(const FViewInfo& View, bool bInUsedFramebufferFetch, bool bInSRGBAwareTarget);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	
	bool bDoScreenPercentageInTonemapper;

private:
	const FViewInfo& View;

	bool bUsedFramebufferFetch;
	bool bSRGBAwareTarget;
	// set in constructor
	uint32 ConfigIndexMobile;

	void SetShader(const FRenderingCompositePassContext& Context);
};


/** Encapsulates the post processing tone map vertex shader. */
template< bool bUseAutoExposure>
class TPostProcessTonemapVS : public FGlobalShader
{
	// This class is in the header so that Temporal AA can share this vertex shader.
	DECLARE_SHADER_TYPE(TPostProcessTonemapVS,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	/** Default constructor. */
	TPostProcessTonemapVS(){}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderResourceParameter EyeAdaptation;
	FShaderParameter GrainRandomFull;
	FShaderParameter FringeUVParams;
	FShaderParameter DefaultEyeExposure;

	/** Initialization constructor. */
	TPostProcessTonemapVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		EyeAdaptation.Bind(Initializer.ParameterMap, TEXT("EyeAdaptation"));
		GrainRandomFull.Bind(Initializer.ParameterMap, TEXT("GrainRandomFull"));
		FringeUVParams.Bind(Initializer.ParameterMap, TEXT("FringeUVParams"));
		DefaultEyeExposure.Bind(Initializer.ParameterMap, TEXT("DefaultEyeExposure"));
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		
		// Compile time template-based conditional
		OutEnvironment.SetDefine(TEXT("EYEADAPTATION_EXPOSURE_FIX"), (uint32)bUseAutoExposure);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		FVector GrainRandomFullValue;
		{
			uint8 FrameIndexMod8 = 0;
			if (Context.View.State)
			{
				FrameIndexMod8 = Context.View.State->GetFrameIndexMod8();
			}
			GrainRandomFromFrame(&GrainRandomFullValue, FrameIndexMod8);
		}

		SetShaderValue(Context.RHICmdList, ShaderRHI, GrainRandomFull, GrainRandomFullValue);

		
		if (Context.View.HasValidEyeAdaptation())
		{
			IPooledRenderTarget* EyeAdaptationRT = Context.View.GetEyeAdaptation(Context.RHICmdList);
			FTextureRHIParamRef EyeAdaptationRTRef = EyeAdaptationRT->GetRenderTargetItem().TargetableTexture;
			if (EyeAdaptationRTRef)
			{
				Context.RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, &EyeAdaptationRTRef, 1);
			}
			SetTextureParameter(Context.RHICmdList, ShaderRHI, EyeAdaptation, EyeAdaptationRT->GetRenderTargetItem().TargetableTexture);
		}
		else
		{
			// some views don't have a state, thumbnail rendering?
			SetTextureParameter(Context.RHICmdList, ShaderRHI, EyeAdaptation, GWhiteTexture->TextureRHI);
		}

		// Compile time template-based conditional
		if (!bUseAutoExposure)
		{
			// Compute a CPU-based default.  NB: reverts to "1" if SM5 feature level is not supported
			float DefaultEyeExposureValue = FRCPassPostProcessEyeAdaptation::ComputeExposureScaleValue(Context.View);
			// Load a default value 
			SetShaderValue(Context.RHICmdList, ShaderRHI, DefaultEyeExposure, DefaultEyeExposureValue);
		}

		{
			// for scene color fringe
			// from percent to fraction
			float Offset = Context.View.FinalPostProcessSettings.SceneFringeIntensity * 0.01f;
			//FVector4 Value(1.0f - Offset * 0.5f, 1.0f - Offset, 0.0f, 0.0f);

			// Wavelength of primaries in nm
			const float PrimaryR = 611.3f;
			const float PrimaryG = 549.1f;
			const float PrimaryB = 464.3f;

			// Simple lens chromatic aberration is roughly linear in wavelength
			float ScaleR = 0.007f * ( PrimaryR - PrimaryB );
			float ScaleG = 0.007f * ( PrimaryG - PrimaryB );
			FVector4 Value( 1.0f / ( 1.0f + Offset * ScaleG ), 1.0f / ( 1.0f + Offset * ScaleR ), 0.0f, 0.0f);

			// we only get bigger to not leak in content from outside
			SetShaderValue(Context.RHICmdList, ShaderRHI, FringeUVParams, Value);
		}
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << GrainRandomFull << EyeAdaptation << FringeUVParams << DefaultEyeExposure;

		return bShaderHasOutdatedParameters;
	}
};

// Default uses eye adaptation
typedef TPostProcessTonemapVS<true/*bUseEyeAdaptation*/> FPostProcessTonemapVS;
