// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessVisualizeComplexity.h: PP pass used when visualizing complexity, maps scene color complexity value to colors
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "RendererInterface.h"
#include "PostProcessParameters.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "DebugViewModeHelpers.h"

/**
* The number of shader complexity colors from the engine ini that will be passed to the shader. 
* Changing this requires a recompile of the FShaderComplexityApplyPS.
*/
static const uint32 MaxNumShaderComplexityColors = 11;
static const float NormalizedQuadComplexityValue = 1.f / 16.f;

/**
 * Gets the maximum shader complexity count from the ini settings.
 */
float GetMaxShaderComplexityCount(ERHIFeatureLevel::Type InFeatureType);

/**
* Pixel shader that is used to visualize complexity stored in scene color into color.
*/
class FVisualizeComplexityApplyPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVisualizeComplexityApplyPS,Global);
public:

	enum EColorSampling
	{
		CS_RAMP,
		CS_LINEAR,
		CS_STAIR
	};

	/** 
	* Constructor - binds all shader params
	* @param Initializer - init data from shader compiler
	*/
	FVisualizeComplexityApplyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	FVisualizeComplexityApplyPS() {}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, const TArray<FLinearColor>& Colors, EColorSampling ColorSampling, float ComplexityScale, bool bLegend);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("READ_QUAD_OVERDRAW"), AllowDebugViewPS(DVSM_QuadComplexity, Platform));
		OutEnvironment.SetDefine(TEXT("MAX_NUM_COMPLEXITY_COLORS"), MaxNumShaderComplexityColors);
		// EColorSampling values
		OutEnvironment.SetDefine(TEXT("CS_RAMP"), (uint32)CS_RAMP);
		OutEnvironment.SetDefine(TEXT("CS_LINEAR"), (uint32)CS_LINEAR);
		OutEnvironment.SetDefine(TEXT("CS_STAIR"), (uint32)CS_STAIR);
		// EDebugViewShaderMode values
		OutEnvironment.SetDefine(TEXT("DVSM_None"), (uint32)DVSM_None);
		OutEnvironment.SetDefine(TEXT("DVSM_ShaderComplexity"), (uint32)DVSM_ShaderComplexity);
		OutEnvironment.SetDefine(TEXT("DVSM_ShaderComplexityContainedQuadOverhead"), (uint32)DVSM_ShaderComplexityContainedQuadOverhead);
		OutEnvironment.SetDefine(TEXT("DVSM_ShaderComplexityBleedingQuadOverhead"), (uint32)DVSM_ShaderComplexityBleedingQuadOverhead);
		OutEnvironment.SetDefine(TEXT("DVSM_QuadComplexity"), (uint32)DVSM_QuadComplexity);
	}

	bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << ShaderComplexityColors << MiniFontTexture << ShaderComplexityParams << ShaderComplexityParams2 << QuadOverdrawTexture;
		return bShaderHasOutdatedParameters;
	}

private:

	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter ShaderComplexityColors;
	FShaderResourceParameter MiniFontTexture;
	FShaderParameter ShaderComplexityParams;
	FShaderParameter ShaderComplexityParams2;
	FShaderResourceParameter QuadOverdrawTexture;
};

class FRCPassPostProcessVisualizeComplexity : public TRenderingCompositePassBase<1, 1>
{
public:

	typedef FVisualizeComplexityApplyPS::EColorSampling EColorSampling;

	FRCPassPostProcessVisualizeComplexity(const TArray<FLinearColor>& InColors, EColorSampling InColorSampling, float InComplexityScale, bool bInLegend)
		: Colors(InColors)
		, ColorSampling(InColorSampling)
		, ComplexityScale(InComplexityScale)
		, bLegend(bInLegend)
	{}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private: 

	TArray<FLinearColor> Colors;
	EColorSampling ColorSampling;
	float ComplexityScale;
	bool bLegend;
};
