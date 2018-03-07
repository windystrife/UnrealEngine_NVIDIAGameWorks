// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "RenderResource.h"

class FOculusVertexShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOculusVertexShader, Global, UTILITYSHADERS_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FOculusVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{}
	FOculusVertexShader() {}
};

class FOculusWhiteShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOculusWhiteShader, Global, UTILITYSHADERS_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FOculusWhiteShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
	}
	
	FOculusWhiteShader() {}
};

class FOculusBlackShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOculusBlackShader, Global, UTILITYSHADERS_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FOculusBlackShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
	}

	FOculusBlackShader() {}
};

class FOculusAlphaInverseShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOculusAlphaInverseShader, Global, UTILITYSHADERS_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FOculusAlphaInverseShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		InTexture.Bind(Initializer.ParameterMap, TEXT("InTexture"), SPF_Mandatory);
		InTextureSampler.Bind(Initializer.ParameterMap, TEXT("InTextureSampler"));
	}
	FOculusAlphaInverseShader() {}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture)
	{
		SetTextureParameter(RHICmdList, GetPixelShader(), InTexture, InTextureSampler, Texture);
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, FSamplerStateRHIParamRef SamplerStateRHI, FTextureRHIParamRef TextureRHI)
	{
		SetTextureParameter(RHICmdList, GetPixelShader(), InTexture, InTextureSampler, SamplerStateRHI, TextureRHI);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InTexture;
		Ar << InTextureSampler;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter InTexture;
	FShaderResourceParameter InTextureSampler;
};


/**
* A pixel shader for rendering a textured screen element.
*/
class FOculusCubemapPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOculusCubemapPS, Global, UTILITYSHADERS_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FOculusCubemapPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		InTexture.Bind(Initializer.ParameterMap, TEXT("InTextureCube"), SPF_Mandatory);
		InTextureSampler.Bind(Initializer.ParameterMap, TEXT("InTextureSampler"));
		InFaceIndexParameter.Bind(Initializer.ParameterMap, TEXT("CubeFaceIndex"));
	}
	FOculusCubemapPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture, int FaceIndex)
	{
		SetTextureParameter(RHICmdList, GetPixelShader(), InTexture, InTextureSampler, Texture);
		SetShaderValue(RHICmdList, GetPixelShader(), InFaceIndexParameter, FaceIndex);
	}

	void SetParameters(FRHICommandList& RHICmdList, FSamplerStateRHIParamRef SamplerStateRHI, FTextureRHIParamRef TextureRHI, int FaceIndex)
	{
		SetTextureParameter(RHICmdList, GetPixelShader(), InTexture, InTextureSampler, SamplerStateRHI, TextureRHI);
		SetShaderValue(RHICmdList, GetPixelShader(), InFaceIndexParameter, FaceIndex);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InTexture;
		Ar << InTextureSampler;
		Ar << InFaceIndexParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter InTexture;
	FShaderResourceParameter InTextureSampler;
	FShaderParameter		 InFaceIndexParameter;
};