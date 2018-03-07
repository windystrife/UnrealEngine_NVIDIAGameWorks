// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"

class FWideCustomResolveVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FWideCustomResolveVS,Global);
public:
	FWideCustomResolveVS() {}
	FWideCustomResolveVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader( Initializer )
	{
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) || (Platform == SP_PCD3D_ES2);
	}
};

template <unsigned MSAASampleCount, unsigned Width>
class FWideCustomResolvePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FWideCustomResolvePS,Global);
public:
	FWideCustomResolvePS() {}
	FWideCustomResolvePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader( Initializer )
	{
		static_assert(Width >= 0 && Width <= 3, "Invalid width");
		static_assert(MSAASampleCount == 0 || MSAASampleCount == 2 || MSAASampleCount == 4, "Invalid sample count");

		Tex.Bind(Initializer.ParameterMap, TEXT("Tex"), SPF_Mandatory);
		ResolveOrigin.Bind(Initializer.ParameterMap, TEXT("ResolveOrigin"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << Tex;
		Ar << ResolveOrigin;
		return bShaderHasOutdatedParameters;
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	void SetParameters(FRHICommandList& RHICmdList, FTextureRHIParamRef Texture2DMS, FIntPoint Origin)
	{
		FPixelShaderRHIParamRef PixelShaderRHI = GetPixelShader();
		SetTextureParameter(RHICmdList, PixelShaderRHI, Tex, Texture2DMS);
		SetShaderValue(RHICmdList, PixelShaderRHI, ResolveOrigin, FVector2D(Origin));
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("WIDE_RESOLVE_WIDTH"), Width);
		OutEnvironment.SetDefine(TEXT("MSAA_SAMPLE_COUNT"), MSAASampleCount);
	}

protected:
	FShaderResourceParameter Tex;
	FShaderParameter ResolveOrigin;
};

extern void ResolveFilterWide(
	FRHICommandList& RHICmdList,
	FGraphicsPipelineStateInitializer& GraphicsPSOInit,
	const ERHIFeatureLevel::Type CurrentFeatureLevel,
	const FTextureRHIRef& SrcTexture,
	const FIntPoint& SrcOrigin,
	int32 NumSamples,
	int32 WideFilterWidth);
