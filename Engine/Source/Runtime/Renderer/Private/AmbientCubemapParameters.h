// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AmbientCubemapParameters.h: Shared shader parameters for ambient cubemap
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "FinalPostProcessSettings.h"
#include "ShaderParameters.h"

class FShaderParameterMap;

/** Pixel shader parameters needed for deferred passes. */
class FCubemapShaderParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap);

	void SetParameters(FRHICommandList& RHICmdList, const FPixelShaderRHIParamRef ShaderRHI, const FFinalPostProcessSettings::FCubemapEntry& Entry) const;
	void SetParameters(FRHICommandList& RHICmdList, const FComputeShaderRHIParamRef ShaderRHI, const FFinalPostProcessSettings::FCubemapEntry& Entry) const;

	friend FArchive& operator<<(FArchive& Ar, FCubemapShaderParameters& P);

private:
	FShaderParameter AmbientCubemapColor;
	FShaderParameter AmbientCubemapMipAdjust;
	FShaderResourceParameter AmbientCubemap;
	FShaderResourceParameter AmbientCubemapSampler;

	template<typename TShaderRHIRef>
	void SetParametersTemplate(FRHICommandList& RHICmdList, const TShaderRHIRef& ShaderRHI, const FFinalPostProcessSettings::FCubemapEntry& Entry) const;
};

