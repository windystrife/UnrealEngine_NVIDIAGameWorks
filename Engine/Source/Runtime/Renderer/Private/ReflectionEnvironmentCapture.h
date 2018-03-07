// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Functionality for capturing the scene into reflection capture cubemaps, and prefiltering
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Math/SHMath.h"
#include "RHI.h"
#include "GlobalShader.h"

extern void ComputeDiffuseIrradiance(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, FTextureRHIRef LightingSource, int32 LightingSourceMipIndex, FSHVectorRGB3* OutIrradianceEnvironmentMap);

/** Pixel shader used for filtering a mip. */
class FCubeFilterPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCubeFilterPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	FCubeFilterPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		CubeFace.Bind(Initializer.ParameterMap, TEXT("CubeFace"));
		MipIndex.Bind(Initializer.ParameterMap, TEXT("MipIndex"));
		NumMips.Bind(Initializer.ParameterMap, TEXT("NumMips"));
		SourceTexture.Bind(Initializer.ParameterMap, TEXT("SourceTexture"));
		SourceTextureSampler.Bind(Initializer.ParameterMap, TEXT("SourceTextureSampler"));
	}
	FCubeFilterPS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << CubeFace;
		Ar << MipIndex;
		Ar << NumMips;
		Ar << SourceTexture;
		Ar << SourceTextureSampler;
		return bShaderHasOutdatedParameters;
	}

	FShaderParameter CubeFace;
	FShaderParameter MipIndex;
	FShaderParameter NumMips;
	FShaderResourceParameter SourceTexture;
	FShaderResourceParameter SourceTextureSampler;
};

template< uint32 bNormalize >
class TCubeFilterPS : public FCubeFilterPS
{
	DECLARE_SHADER_TYPE(TCubeFilterPS, Global);
public:
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FCubeFilterPS::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NORMALIZE"), bNormalize);
	}

	TCubeFilterPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FCubeFilterPS(Initializer)
	{}

	TCubeFilterPS() {}
};