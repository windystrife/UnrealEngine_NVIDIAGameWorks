// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenRendering.h: Screen rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"

/**
 * A vertex shader for rendering a transformed textured element.
 */
class FStereoLayerVS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FStereoLayerVS,Global,ENGINE_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FStereoLayerVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		InQuadAdjust.Bind(Initializer.ParameterMap, TEXT("InQuadAdjust"));
		InUVAdjust.Bind(Initializer.ParameterMap, TEXT("InUVAdjust"));
		InViewProjection.Bind(Initializer.ParameterMap, TEXT("InViewProjection"));
		InWorld.Bind(Initializer.ParameterMap, TEXT("InWorld"));
	}
	FStereoLayerVS() {}

	void SetParameters(FRHICommandList& RHICmdList, FVector2D QuadSize, FBox2D UVRect, const FMatrix& ViewProjection, const FMatrix& World)
	{
		FVertexShaderRHIParamRef VS = GetVertexShader();

		if (InQuadAdjust.IsBound())
		{
			SetShaderValue(RHICmdList, VS, InQuadAdjust, QuadSize);
		}

		if (InUVAdjust.IsBound())
		{
			FVector4 UVAdjust;
			UVAdjust.X = UVRect.Min.X;
			UVAdjust.Y = UVRect.Min.Y;
			UVAdjust.Z = UVRect.Max.X - UVRect.Min.X;
			UVAdjust.W = UVRect.Max.Y - UVRect.Min.Y;
			SetShaderValue(RHICmdList, VS, InUVAdjust, UVAdjust);
		}

		if (InViewProjection.IsBound())
		{
			SetShaderValue(RHICmdList, VS, InViewProjection, ViewProjection);
		}

		if (InWorld.IsBound())
		{
			SetShaderValue(RHICmdList, VS, InWorld, World);
		}
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InQuadAdjust;
		Ar << InUVAdjust;
		Ar << InViewProjection;
		Ar << InWorld;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter InQuadAdjust;
	FShaderParameter InUVAdjust;
	FShaderParameter InViewProjection;
	FShaderParameter InWorld;
};

/**
 * A pixel shader for rendering a transformed textured element.
 */
class FStereoLayerPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FStereoLayerPS,Global,ENGINE_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FStereoLayerPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		InTexture.Bind(Initializer.ParameterMap,TEXT("InTexture"), SPF_Mandatory);
		InTextureSampler.Bind(Initializer.ParameterMap,TEXT("InTextureSampler"));
	}
	FStereoLayerPS() {}

	void SetParameters(FRHICommandList& RHICmdList, FSamplerStateRHIParamRef SamplerStateRHI, FTextureRHIParamRef TextureRHI)
	{
		FPixelShaderRHIParamRef PS = GetPixelShader();

		SetTextureParameter(RHICmdList, PS,InTexture,InTextureSampler,SamplerStateRHI,TextureRHI);
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
