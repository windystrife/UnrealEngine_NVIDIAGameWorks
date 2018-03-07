// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SystemTextures.h: System textures definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "RendererInterface.h"

/**
 * Encapsulates the system textures used for scene rendering.
 */
class FSystemTextures : public FRenderResource
{
public:
	FSystemTextures()
	: FRenderResource()
	, FeatureLevelInitializedTo(ERHIFeatureLevel::ES2)
	, bTexturesInitialized(false)
	{}

	/**
	 * Initialize/allocate textures if not already.
	 */
	inline void InitializeTextures(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type InFeatureLevel)
	{
		if (bTexturesInitialized && FeatureLevelInitializedTo >= InFeatureLevel)
		{
			// Already initialized up to at least the feature level we need, so do nothing
			return;
		}

		InternalInitializeTextures(RHICmdList, InFeatureLevel);
	}

	// FRenderResource interface.
	/**
	 * Release textures when device is lost/destroyed.
	 */
	virtual void ReleaseDynamicRHI();

	// -----------

	// float4(1,1,1,1) can be used in case a light is not shadow casting
	TRefCountPtr<IPooledRenderTarget> WhiteDummy;
	// float4(0,0,0,0) can be used in additive postprocessing to avoid a shader combination
	TRefCountPtr<IPooledRenderTarget> BlackDummy;
	// float4(0,0,0,1)
	TRefCountPtr<IPooledRenderTarget> BlackAlphaOneDummy;
	// used by the material expression Noise
	TRefCountPtr<IPooledRenderTarget> PerlinNoiseGradient;
	// used by the material expression Noise (faster version, should replace old version), todo: move out of SceneRenderTargets
	TRefCountPtr<IPooledRenderTarget> PerlinNoise3D;
	// Sobol sampling texture, the first sample points for four sobol dimensions in RGBA
	TRefCountPtr<IPooledRenderTarget> SobolSampling;
	/** SSAO randomization */
	TRefCountPtr<IPooledRenderTarget> SSAORandomization;
	/** Preintegrated GF for single sample IBL */
	TRefCountPtr<IPooledRenderTarget> PreintegratedGF;
	/** Texture that holds a single value containing the maximum depth that can be stored as FP16. */
	TRefCountPtr<IPooledRenderTarget> MaxFP16Depth;
	/** Depth texture that holds a single depth value */
	TRefCountPtr<IPooledRenderTarget> DepthDummy;
	// float4(0,1,0,1)
	TRefCountPtr<IPooledRenderTarget> GreenDummy;
	// float4(0.5,0.5,0.5,1)
	TRefCountPtr<IPooledRenderTarget> DefaultNormal8Bit;

protected:
	/** Maximum feature level that the textures have been initialized up to */
	ERHIFeatureLevel::Type FeatureLevelInitializedTo;
	bool bTexturesInitialized;

	void InternalInitializeTextures(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type InFeatureLevel);
};

/** The global system textures used for scene rendering. */
extern TGlobalResource<FSystemTextures> GSystemTextures;
