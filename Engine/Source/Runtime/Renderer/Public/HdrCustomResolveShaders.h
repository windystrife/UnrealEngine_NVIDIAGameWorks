// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"

class FHdrCustomResolveVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHdrCustomResolveVS,Global);
public:
	FHdrCustomResolveVS() {}
	FHdrCustomResolveVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
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
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}
};

class FHdrCustomResolve2xPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHdrCustomResolve2xPS,Global);
public:
	FHdrCustomResolve2xPS() {}
	FHdrCustomResolve2xPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader( Initializer )
	{
		Tex.Bind(Initializer.ParameterMap, TEXT("Tex"), SPF_Mandatory);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << Tex;
		return bShaderHasOutdatedParameters;
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}

	void SetParameters(FRHICommandList& RHICmdList, FTextureRHIParamRef Texture2DMS)
	{
		FPixelShaderRHIParamRef PixelShaderRHI = GetPixelShader();
		SetTextureParameter(RHICmdList, PixelShaderRHI, Tex, Texture2DMS);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("HDR_CUSTOM_RESOLVE_2X"), 1);
	}

protected:
	FShaderResourceParameter Tex;
};

class FHdrCustomResolve4xPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHdrCustomResolve4xPS,Global);
public:
	FHdrCustomResolve4xPS() {}
	FHdrCustomResolve4xPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader( Initializer )
	{
		Tex.Bind(Initializer.ParameterMap, TEXT("Tex"), SPF_Mandatory);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << Tex;
		return bShaderHasOutdatedParameters;
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}

	void SetParameters(FRHICommandList& RHICmdList, FTextureRHIParamRef Texture2DMS)
	{
		FPixelShaderRHIParamRef PixelShaderRHI = GetPixelShader();
		SetTextureParameter(RHICmdList, PixelShaderRHI, Tex, Texture2DMS);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("HDR_CUSTOM_RESOLVE_4X"), 1);
	}

protected:
	FShaderResourceParameter Tex;
};


class FHdrCustomResolve8xPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHdrCustomResolve8xPS,Global);
public:
	FHdrCustomResolve8xPS() {}
	FHdrCustomResolve8xPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader( Initializer )
	{
		Tex.Bind(Initializer.ParameterMap, TEXT("Tex"), SPF_Mandatory);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << Tex;
		return bShaderHasOutdatedParameters;
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}

	void SetParameters(FRHICommandList& RHICmdList, FTextureRHIParamRef Texture2DMS)
	{
		FPixelShaderRHIParamRef PixelShaderRHI = GetPixelShader();
		SetTextureParameter(RHICmdList, PixelShaderRHI, Tex, Texture2DMS);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("HDR_CUSTOM_RESOLVE_8X"), 1);
	}

protected:
	FShaderResourceParameter Tex;
};




