// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VolumetricFog.h
=============================================================================*/

#pragma once

#include "RHIDefinitions.h"
#include "SceneView.h"
#include "SceneRendering.h"

inline bool DoesPlatformSupportVolumetricFog(EShaderPlatform Platform)
{
	return Platform == SP_PCD3D_SM5 || Platform == SP_PS4 || Platform == SP_XBOXONE_D3D12 || Platform == SP_METAL_SM5;
}

inline bool DoesPlatformSupportVolumetricFogVoxelization(EShaderPlatform Platform)
{
	return Platform == SP_PCD3D_SM5 || Platform == SP_PS4 || Platform == SP_XBOXONE_D3D12 || Platform == SP_METAL_SM5;
}

extern bool ShouldRenderVolumetricFog(const FScene* Scene, const FSceneViewFamily& ViewFamily);

extern bool LightNeedsSeparateInjectionIntoVolumetricFog(const FLightSceneInfo* LightSceneInfo, FVisibleLightInfo& VisibleLightInfo);

/**  */
class FVolumetricFogParameters
{
public:

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		
	}

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		ApplyVolumetricFog.Bind(ParameterMap, TEXT("ApplyVolumetricFog"));
		IntegratedLightScattering.Bind(ParameterMap, TEXT("IntegratedLightScattering"));
		IntegratedLightScatteringSampler.Bind(ParameterMap, TEXT("IntegratedLightScatteringSampler"));
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef& ShaderRHI, const FViewInfo& View) const
	{
		const bool bApplyVolumetricFog = View.VolumetricFogResources.IntegratedLightScattering != NULL;
		SetShaderValue(RHICmdList, ShaderRHI, ApplyVolumetricFog, bApplyVolumetricFog ? 1.0f : 0.0f);

		if (IntegratedLightScattering.IsBound())
		{
			FTextureRHIParamRef IntegratedLightScatteringTexture = GBlackVolumeTexture->TextureRHI;

			if (View.VolumetricFogResources.IntegratedLightScattering)
			{
				IntegratedLightScatteringTexture = View.VolumetricFogResources.IntegratedLightScattering->GetRenderTargetItem().ShaderResourceTexture;
			}

			SetTextureParameter(RHICmdList, ShaderRHI, IntegratedLightScattering, IntegratedLightScatteringSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), IntegratedLightScatteringTexture);
		}
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FVolumetricFogParameters& P)
	{
		Ar << P.ApplyVolumetricFog;
		Ar << P.IntegratedLightScattering;
		Ar << P.IntegratedLightScatteringSampler;
		return Ar;
	}

private:

	FShaderParameter ApplyVolumetricFog;
	FShaderResourceParameter IntegratedLightScattering;
	FShaderResourceParameter IntegratedLightScatteringSampler;
};
