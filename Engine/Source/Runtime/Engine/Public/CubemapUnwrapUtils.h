// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CubemapUnwapUtils.h: Pixel and Vertex shader to render a cube map as 2D texture
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "BatchedElements.h"
#include "GlobalShader.h"

class UTextureCube;
class UTextureRenderTargetCube;

namespace CubemapHelpers
{
	/**
	* Creates an unwrapped 2D image of the cube map ( longitude/latitude )
	* @param	CubeTexture	Source UTextureCube object.
	* @param	BitsOUT	Raw bits of the 2D image bitmap.
	* @param	SizeXOUT	Filled with the X dimension of the output bitmap.
	* @param	SizeYOUT	Filled with the Y dimension of the output bitmap.
	* @param	FormatOUT	Filled with the pixel format of the output bitmap.
	* @return	true on success.
	*/
	ENGINE_API bool GenerateLongLatUnwrap(const UTextureCube* CubeTexture, TArray<uint8>& BitsOUT, FIntPoint& SizeOUT, EPixelFormat& FormatOUT);

	/**
	* Creates an unwrapped 2D image of the cube map ( longitude/latitude )
	* @param	CubeTarget	Source UTextureRenderTargetCube object.
	* @param	BitsOUT	Raw bits of the 2D image bitmap.
	* @param	SizeXOUT	Filled with the X dimension of the output bitmap.
	* @param	SizeYOUT	Filled with the Y dimension of the output bitmap.
	* @param	FormatOUT	Filled with the pixel format of the output bitmap.
	* @return	true on success.
	*/
	ENGINE_API bool GenerateLongLatUnwrap(const UTextureRenderTargetCube* CubeTarget, TArray<uint8>& BitsOUT, FIntPoint& SizeOUT, EPixelFormat& FormatOUT);
}

/**
 * A vertex shader for rendering a texture on a simple element.
 */
class FCubemapTexturePropertiesVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCubemapTexturePropertiesVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return !IsConsolePlatform(Platform); }

	FCubemapTexturePropertiesVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		Transform.Bind(Initializer.ParameterMap,TEXT("Transform"), SPF_Mandatory);
	}
	FCubemapTexturePropertiesVS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FMatrix& TransformValue);

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << Transform;

		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter Transform;
};

/**
 * Simple pixel shader reads from a cube map texture and unwraps it in the LongitudeLatitude form.
 */
template<bool bHDROutput>
class FCubemapTexturePropertiesPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCubemapTexturePropertiesPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return !IsConsolePlatform(Platform); }

	FCubemapTexturePropertiesPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		CubeTexture.Bind(Initializer.ParameterMap,TEXT("CubeTexture"));
		CubeTextureSampler.Bind(Initializer.ParameterMap,TEXT("CubeTextureSampler"));
		ColorWeights.Bind(Initializer.ParameterMap,TEXT("ColorWeights"));
		PackedProperties0.Bind(Initializer.ParameterMap,TEXT("PackedProperties0"));
		Gamma.Bind(Initializer.ParameterMap,TEXT("Gamma"));
	}
	FCubemapTexturePropertiesPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture, const FMatrix& ColorWeightsValue, float MipLevel, float GammaValue);

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << CubeTexture << CubeTextureSampler << PackedProperties0 << ColorWeights << Gamma;
		return bShaderHasOutdatedParameters;
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("HDR_OUTPUT"), bHDROutput ? TEXT("1") : TEXT("0"));
	}
private:
	FShaderResourceParameter CubeTexture;
	FShaderResourceParameter CubeTextureSampler;
	FShaderParameter PackedProperties0;
	FShaderParameter ColorWeights;
	FShaderParameter Gamma;
};


class ENGINE_API FMipLevelBatchedElementParameters : public FBatchedElementParameters
{
public:
	FMipLevelBatchedElementParameters(float InMipLevel, bool bInHDROutput = false)
		: bHDROutput(bInHDROutput)
		, MipLevel(InMipLevel)
	{
	}

	/** Binds vertex and pixel shaders for this element */
	virtual void BindShaders(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ERHIFeatureLevel::Type InFeatureLevel, const FMatrix& InTransform, const float InGamma, const FMatrix& ColorWeights, const FTexture* Texture) override;

private:
	template<typename TPixelShader> void BindShaders(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ERHIFeatureLevel::Type InFeatureLevel, const FMatrix& InTransform, const float InGamma, const FMatrix& ColorWeights, const FTexture* Texture);

	bool bHDROutput;
	/** Parameters that need to be passed to the shader */
	float MipLevel;
};


/**
 * Simple pixel shader that renders a IES light profile for the purposes of visualization.
 */
class FIESLightProfilePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FIESLightProfilePS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && !IsConsolePlatform(Platform);
	}

	FIESLightProfilePS() {}

	FIESLightProfilePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		IESTexture.Bind(Initializer.ParameterMap,TEXT("IESTexture"));
		IESTextureSampler.Bind(Initializer.ParameterMap,TEXT("IESTextureSampler"));
		BrightnessInLumens.Bind(Initializer.ParameterMap,TEXT("BrightnessInLumens"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture, float InBrightnessInLumens);

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << IESTexture;
		Ar << IESTextureSampler;
		Ar << BrightnessInLumens;
		return bShaderHasOutdatedParameters;
	}

private:
	/** The texture to sample. */
	FShaderResourceParameter IESTexture;
	FShaderResourceParameter IESTextureSampler;
	FShaderParameter BrightnessInLumens;
};

class ENGINE_API FIESLightProfileBatchedElementParameters : public FBatchedElementParameters
{
public:
	FIESLightProfileBatchedElementParameters(float InBrightnessInLumens) : BrightnessInLumens(InBrightnessInLumens)
	{
	}

	/** Binds vertex and pixel shaders for this element */
	virtual void BindShaders(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ERHIFeatureLevel::Type InFeatureLevel, const FMatrix& InTransform, const float InGamma, const FMatrix& ColorWeights, const FTexture* Texture) override;

private:
	float BrightnessInLumens;
};
