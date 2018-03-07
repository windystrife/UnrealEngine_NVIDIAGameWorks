// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AtmosphereTextureParameters.h: Shader base classes
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "SceneView.h"

class FShaderParameterMap;

/** Shader parameters needed for atmosphere passes. */
class FAtmosphereShaderTextureParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap);

	template< typename ShaderRHIParamRef >
	FORCEINLINE_DEBUGGABLE void Set( FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FSceneView& View ) const
	{
		if (TransmittanceTexture.IsBound() || IrradianceTexture.IsBound() || InscatterTexture.IsBound())
		{
			SetTextureParameter(RHICmdList, ShaderRHI, TransmittanceTexture, TransmittanceTextureSampler,
				TStaticSamplerState<SF_Bilinear>::GetRHI(), View.AtmosphereTransmittanceTexture);
			SetTextureParameter(RHICmdList, ShaderRHI, IrradianceTexture, IrradianceTextureSampler,
				TStaticSamplerState<SF_Bilinear>::GetRHI(), View.AtmosphereIrradianceTexture);
			SetTextureParameter(RHICmdList, ShaderRHI, InscatterTexture, InscatterTextureSampler,
				TStaticSamplerState<SF_Bilinear>::GetRHI(), View.AtmosphereInscatterTexture);
		}
	}

	friend FArchive& operator<<(FArchive& Ar,FAtmosphereShaderTextureParameters& P);

private:
	FShaderResourceParameter TransmittanceTexture;
	FShaderResourceParameter TransmittanceTextureSampler;
	FShaderResourceParameter IrradianceTexture;
	FShaderResourceParameter IrradianceTextureSampler;
	FShaderResourceParameter InscatterTexture;
	FShaderResourceParameter InscatterTextureSampler;
};
