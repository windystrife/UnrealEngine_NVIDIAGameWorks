// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneRenderTargetParameters.h: Shader base classes
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "MaterialShared.h"

class FSceneView;
class FShaderParameterMap;

namespace ESceneRenderTargetsMode
{
	enum Type
	{
		//scene textures are valid, and materials may bind them.  attempt to bind.
		SetTextures,
		//we know based on the kind of the material that the scenetextures will not attempt to bind.  this is an optimization
		DontSet, 
		DontSetIgnoreBoundByEditorCompositing,
		//we are in a context where the scenetargets are not valid, but materials will want to bind them.  set some defaults.  
		//required for safe FRendererModule::DrawTileMesh rendering of materials which bind scene texture
		InvalidScene,
	};
}

/** Encapsulates scene texture shader parameter bindings. */
class FSceneTextureShaderParameters
{
public:
	/** Binds the parameters using a compiled shader's parameter map. */
	void Bind(const FShaderParameterMap& ParameterMap);

	/** Sets the scene texture parameters for the given view. */
	template< typename ShaderRHIParamRef, typename TRHICmdList >
	void Set(
		TRHICmdList& RHICmdList,
		const ShaderRHIParamRef& ShaderRHI,
		const FSceneView& View,
		const EDeferredParamStrictness ParamStrictness = EDeferredParamStrictness::ELoose,
		ESceneRenderTargetsMode::Type TextureMode = ESceneRenderTargetsMode::SetTextures,
		ESamplerFilter ColorFilter = SF_Point
	) const;

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FSceneTextureShaderParameters& P);

private:
	/** The SceneColorTexture parameter for materials that use SceneColor */
	FShaderResourceParameter SceneColorTextureParameter;
	FShaderResourceParameter SceneColorTextureParameterSampler;
	/** The SceneDepthTexture parameter for materials that use SceneDepth */
	FShaderResourceParameter SceneDepthTextureParameter;
	FShaderResourceParameter SceneDepthTextureParameterSampler;
	/** The SceneAlphaCopyTexture parameter for materials that use SceneAlphaCopy */
	FShaderResourceParameter SceneAlphaCopyTextureParameter;
	FShaderResourceParameter SceneAlphaCopyTextureParameterSampler;

	/** for MSAA access to the scene color */
	FShaderResourceParameter SceneColorSurfaceParameter;
	/** The SceneColorTextureMSAA parameter for materials that use SceneColorTextureMSAA */
	FShaderResourceParameter SceneDepthSurfaceParameter;
	/**  */
	FShaderResourceParameter SceneDepthTextureNonMS;
	FShaderResourceParameter DirectionalOcclusionSampler;
	FShaderResourceParameter DirectionalOcclusionTexture;

	FShaderResourceParameter MobileCustomStencilTexture;
	FShaderResourceParameter MobileCustomStencilTextureSampler;

	FShaderResourceParameter SceneStencilTextureParameter;
};

/** Pixel shader parameters needed for deferred passes. */
class FDeferredPixelShaderParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap);

	template< typename ShaderRHIParamRef, typename TRHICmdList >
	void Set(
		TRHICmdList& RHICmdList,
		const ShaderRHIParamRef ShaderRHI,
		const FSceneView& View,
		EMaterialDomain MaterialDomain,
		ESceneRenderTargetsMode::Type TextureMode = ESceneRenderTargetsMode::SetTextures
	) const;

	friend FArchive& operator<<(FArchive& Ar,FDeferredPixelShaderParameters& P);

private:
	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderUniformBufferParameter GBufferResources;
	FShaderResourceParameter DBufferATextureMS;
	FShaderResourceParameter DBufferBTextureMS;
	FShaderResourceParameter DBufferCTextureMS;
	FShaderResourceParameter ScreenSpaceAOTextureMS;
	FShaderResourceParameter DBufferATextureNonMS;
	FShaderResourceParameter DBufferBTextureNonMS;
	FShaderResourceParameter DBufferCTextureNonMS;
	FShaderResourceParameter ScreenSpaceAOTextureNonMS;
	FShaderResourceParameter CustomDepthTextureNonMS;
	FShaderResourceParameter DBufferATexture;
	FShaderResourceParameter DBufferRenderMask;
	FShaderResourceParameter DBufferATextureSampler;
	FShaderResourceParameter DBufferBTexture;
	FShaderResourceParameter DBufferBTextureSampler;
	FShaderResourceParameter DBufferCTexture;
	FShaderResourceParameter DBufferCTextureSampler;
	FShaderResourceParameter ScreenSpaceAOTexture;
	FShaderResourceParameter ScreenSpaceAOTextureSampler;
	FShaderResourceParameter CustomDepthTexture;
	FShaderResourceParameter CustomDepthTextureSampler;
	FShaderResourceParameter CustomStencilTexture;

	// NVCHANGE_BEGIN: Add VXGI
	FShaderResourceParameter VxgiDiffuseTexture;
	FShaderResourceParameter VxgiDiffuseTextureSampler;
	FShaderResourceParameter VxgiSpecularTexture;
	FShaderResourceParameter VxgiSpecularTextureSampler;
	// NVCHANGE_END: Add VXGI
};