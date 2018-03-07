// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FogRendering.h: 
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Runtime/Renderer/Private/SceneRendering.h"
#include "Runtime/Renderer/Private/VolumetricFog.h"

/** Parameters needed to render exponential height fog. */
class FExponentialHeightFogShaderParameters
{
public:

	/** Binds the parameters. */
	void Bind(const FShaderParameterMap& ParameterMap);

	/** 
	 * Sets exponential height fog shader parameters.
	 */
	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, ShaderRHIParamRef Shader, const FSceneView* View) const
	{
		const FViewInfo* ViewInfo = static_cast<const FViewInfo*>(View);
		SetShaderValue(RHICmdList, Shader, ExponentialFogParameters, ViewInfo->ExponentialFogParameters);
		SetShaderValue(RHICmdList, Shader, ExponentialFogColorParameter, FVector4(ViewInfo->ExponentialFogColor, 1.0f - ViewInfo->FogMaxOpacity));
		SetShaderValue(RHICmdList, Shader, ExponentialFogParameters3, ViewInfo->ExponentialFogParameters3);
		SetShaderValue(RHICmdList, Shader, SinCosInscatteringColorCubemapRotation, ViewInfo->SinCosInscatteringColorCubemapRotation);

		const FTexture* Cubemap = GWhiteTextureCube;

		if (ViewInfo->FogInscatteringColorCubemap)
		{
			Cubemap = ViewInfo->FogInscatteringColorCubemap->Resource;
		}

		SetTextureParameter(
			RHICmdList, 
			Shader, 
			FogInscatteringColorCubemap, 
			FogInscatteringColorSampler, 
			TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(),
			Cubemap->TextureRHI);

		SetShaderValue(RHICmdList, Shader, FogInscatteringTextureParameters, ViewInfo->FogInscatteringTextureParameters);

		SetShaderValue(RHICmdList, Shader, InscatteringLightDirection, FVector4(ViewInfo->InscatteringLightDirection, ViewInfo->bUseDirectionalInscattering ? 1 : 0));
		SetShaderValue(RHICmdList, Shader, DirectionalInscatteringColor, FVector4(FVector(ViewInfo->DirectionalInscatteringColor), FMath::Clamp(ViewInfo->DirectionalInscatteringExponent, 0.000001f, 1000.0f)));
		SetShaderValue(RHICmdList, Shader, DirectionalInscatteringStartDistance, ViewInfo->DirectionalInscatteringStartDistance);

		VolumetricFogParameters.Set(RHICmdList, Shader, *ViewInfo);
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FExponentialHeightFogShaderParameters& P);

	FShaderParameter ExponentialFogParameters;
	FShaderParameter ExponentialFogColorParameter;
	FShaderParameter ExponentialFogParameters3;
	FShaderParameter SinCosInscatteringColorCubemapRotation;
	FShaderResourceParameter FogInscatteringColorCubemap;
	FShaderResourceParameter FogInscatteringColorSampler;
	FShaderParameter FogInscatteringTextureParameters;
	FShaderParameter InscatteringLightDirection;
	FShaderParameter DirectionalInscatteringColor;
	FShaderParameter DirectionalInscatteringStartDistance;
	FVolumetricFogParameters VolumetricFogParameters;
};

/** Encapsulates parameters needed to calculate height fog in a vertex shader. */
class FHeightFogShaderParameters
{
public:

	/** Binds the parameters. */
	void Bind(const FShaderParameterMap& ParameterMap);

	/** 
	 * Sets height fog shader parameters.
	 */
	template<typename TShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, TShaderRHIParamRef Shader, const FSceneView* View) const
	{
		// Set the fog constants.
		ExponentialParameters.Set(RHICmdList, Shader, View);
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FHeightFogShaderParameters& P);

private:

	FExponentialHeightFogShaderParameters ExponentialParameters;
};

extern bool ShouldRenderFog(const FSceneViewFamily& Family);
