// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	SimpleElementShaders.h: Definitions for simple element shaders.
==============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "SceneTypes.h"
#include "Engine/EngineTypes.h"

class FSceneView;

/**
 * A vertex shader for rendering a texture on a simple element.
 */
class ENGINE_API FSimpleElementVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSimpleElementVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FSimpleElementVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
	FSimpleElementVS() {}

	/*ENGINE_API */void SetParameters(FRHICommandList& RHICmdList, const FMatrix& TransformValue, bool bSwitchVerticalAxis = false);

	virtual bool Serialize(FArchive& Ar) override;

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);

private:
	FShaderParameter Transform;
	FShaderParameter SwitchVerticalAxis;
};

/**
 * Simple pixel shader that just reads from a texture
 * This is used for resolves and needs to be as efficient as possible
 */
class FSimpleElementPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSimpleElementPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FSimpleElementPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
	FSimpleElementPS() {}

	/**
	 * Sets parameters for compositing editor primitives
	 *
	 * @param View			SceneView for view constants when compositing
	 * @param DepthTexture	Depth texture to read from when depth testing for compositing.  If not set no compositing will occur
	 */
	void SetEditorCompositingParameters(FRHICommandList& RHICmdList, const FSceneView* View, FTexture2DRHIRef DepthTexture );

	void SetParameters(FRHICommandList& RHICmdList, const FTexture* TextureValue );

	virtual bool Serialize(FArchive& Ar) override;

private:
	FShaderResourceParameter InTexture;
	FShaderResourceParameter InTextureSampler;
	FShaderParameter TextureComponentReplicate;
	FShaderParameter TextureComponentReplicateAlpha;
	FShaderResourceParameter SceneDepthTextureNonMS;
	FShaderParameter EditorCompositeDepthTestParameter;
	FShaderParameter ScreenToPixel;
};

/**
 * Simple pixel shader that just reads from an alpha-only texture
 */
class FSimpleElementAlphaOnlyPS : public FSimpleElementPS
{
	DECLARE_SHADER_TYPE(FSimpleElementAlphaOnlyPS, Global);
public:

	FSimpleElementAlphaOnlyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
	FSimpleElementAlphaOnlyPS() {}
};

/**
 * A pixel shader for rendering a texture on a simple element.
 */
class FSimpleElementGammaBasePS : public FSimpleElementPS
{
	DECLARE_SHADER_TYPE(FSimpleElementGammaBasePS, Global);
public:

	FSimpleElementGammaBasePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
	FSimpleElementGammaBasePS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture,float GammaValue,ESimpleElementBlendMode BlendMode);

	virtual bool Serialize(FArchive& Ar) override;

private:
	FShaderParameter Gamma;
};

template <bool bSRGBTexture>
class FSimpleElementGammaPS : public FSimpleElementGammaBasePS
{
	DECLARE_SHADER_TYPE(FSimpleElementGammaPS, Global);
public:

	FSimpleElementGammaPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FSimpleElementGammaBasePS(Initializer) {}
	FSimpleElementGammaPS() {}

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SRGB_INPUT_TEXTURE"), bSRGBTexture);
	}
};

/**
 * Simple pixel shader that just reads from an alpha-only texture and gamma corrects the output
 */
class FSimpleElementGammaAlphaOnlyPS : public FSimpleElementGammaBasePS
{
	DECLARE_SHADER_TYPE(FSimpleElementGammaAlphaOnlyPS, Global);
public:

	FSimpleElementGammaAlphaOnlyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FSimpleElementGammaBasePS(Initializer) {}
	FSimpleElementGammaAlphaOnlyPS() {}

	static bool ShouldCache(EShaderPlatform Platform) { return true; }
};

/**
 * A pixel shader for rendering a masked texture on a simple element.
 */
class FSimpleElementMaskedGammaBasePS : public FSimpleElementGammaBasePS
{
	DECLARE_SHADER_TYPE(FSimpleElementMaskedGammaBasePS, Global);
public:

	FSimpleElementMaskedGammaBasePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
	FSimpleElementMaskedGammaBasePS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture,float Gamma,float ClipRefValue,ESimpleElementBlendMode BlendMode);

	virtual bool Serialize(FArchive& Ar) override;

private:
	FShaderParameter ClipRef;
};

template <bool bSRGBTexture>
class FSimpleElementMaskedGammaPS : public FSimpleElementMaskedGammaBasePS
{
	DECLARE_SHADER_TYPE(FSimpleElementMaskedGammaPS, Global);
public:

	FSimpleElementMaskedGammaPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FSimpleElementMaskedGammaBasePS(Initializer) {}
	FSimpleElementMaskedGammaPS() {}

	static bool ShouldCache(EShaderPlatform Platform) { return true; }
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SRGB_INPUT_TEXTURE"), bSRGBTexture);
	}
};

/**
* A pixel shader for rendering a masked texture using signed distance filed for alpha on a simple element.
*/
class FSimpleElementDistanceFieldGammaPS : public FSimpleElementMaskedGammaBasePS
{
	DECLARE_SHADER_TYPE(FSimpleElementDistanceFieldGammaPS,Global);
public:

	/**
	* Determine if this shader should be compiled
	*
	* @param Platform - current shader platform being compiled
	* @return true if this shader should be cached for the given platform
	*/
	static bool ShouldCache(EShaderPlatform Platform) 
	{ 
		return true; 
	}

	/**
	* Constructor
	*
	* @param Initializer - shader initialization container
	*/
	FSimpleElementDistanceFieldGammaPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	/**
	* Default constructor
	*/
	FSimpleElementDistanceFieldGammaPS() {}

	/**
	* Sets all the constant parameters for this shader
	*
	* @param Texture - 2d tile texture
	* @param Gamma - if gamma != 1.0 then a pow(color,Gamma) is applied
	* @param ClipRef - reference value to compare with alpha for killing pixels
	* @param SmoothWidth - The width to smooth the edge the texture
	* @param EnableShadow - Toggles drop shadow rendering
	* @param ShadowDirection - 2D vector specifying the direction of shadow
	* @param ShadowColor - Color of the shadowed pixels
	* @param ShadowSmoothWidth - The width to smooth the edge the shadow of the texture
	* @param BlendMode - current batched element blend mode being rendered
	*/
	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FTexture* Texture,
		float Gamma,
		float ClipRef,
		float SmoothWidthValue,
		bool EnableShadowValue,
		const FVector2D& ShadowDirectionValue,
		const FLinearColor& ShadowColorValue,
		float ShadowSmoothWidthValue,
		const FDepthFieldGlowInfo& GlowInfo,
		ESimpleElementBlendMode BlendMode
		);

	/**
	* Serialize constant paramaters for this shader
	* 
	* @param Ar - archive to serialize to
	* @return true if any of the parameters were outdated
	*/
	virtual bool Serialize(FArchive& Ar) override;
	
private:
	/** The width to smooth the edge the texture */
	FShaderParameter SmoothWidth;
	/** Toggles drop shadow rendering */
	FShaderParameter EnableShadow;
	/** 2D vector specifying the direction of shadow */
	FShaderParameter ShadowDirection;
	/** Color of the shadowed pixels */
	FShaderParameter ShadowColor;	
	/** The width to smooth the edge the shadow of the texture */
	FShaderParameter ShadowSmoothWidth;
	/** whether to turn on the outline glow */
	FShaderParameter EnableGlow;
	/** base color to use for the glow */
	FShaderParameter GlowColor;
	/** outline glow outer radius */
	FShaderParameter GlowOuterRadius;
	/** outline glow inner radius */
	FShaderParameter GlowInnerRadius;
};

/**
 * A pixel shader for rendering a texture on a simple element.
 */
class FSimpleElementHitProxyPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSimpleElementHitProxyPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) 
	{ 
		return IsPCPlatform(Platform); 
	}


	FSimpleElementHitProxyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
	FSimpleElementHitProxyPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FTexture* TextureValue);

	virtual bool Serialize(FArchive& Ar) override;

private:
	FShaderResourceParameter InTexture;
	FShaderResourceParameter InTextureSampler;
};


/**
* A pixel shader for rendering a texture with the ability to weight the colors for each channel.
* The shader also features the ability to view alpha channels and desaturate any red, green or blue channels
* 
*/
class FSimpleElementColorChannelMaskPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSimpleElementColorChannelMaskPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) 
	{ 
		return IsPCPlatform(Platform); 
	}


	FSimpleElementColorChannelMaskPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
	FSimpleElementColorChannelMaskPS() {}

	/**
	* Sets all the constant parameters for this shader
	*
	* @param Texture - 2d tile texture
	* @param ColorWeights - reference value to compare with alpha for killing pixels
	* @param Gamma - if gamma != 1.0 then a pow(color,Gamma) is applied
	*/
	void SetParameters(FRHICommandList& RHICmdList, const FTexture* TextureValue, const FMatrix& ColorWeightsValue, float GammaValue);

	virtual bool Serialize(FArchive& Ar) override;

private:
	FShaderResourceParameter InTexture;
	FShaderResourceParameter InTextureSampler;
	FShaderParameter ColorWeights; 
	FShaderParameter Gamma;
};

template <class TSimpleElementBase, uint32 EncodedBlendMode>
class FEncodedSimpleElement : public TSimpleElementBase
{
	DECLARE_SHADER_TYPE(FEncodedSimpleElement, Global);
public:
	FEncodedSimpleElement(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : TSimpleElementBase(Initializer) {}
	FEncodedSimpleElement() {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		TSimpleElementBase::ModifyCompilationEnvironment(Platform, OutEnvironment); 
		OutEnvironment.SetDefine(TEXT("SE_BLEND_OPAQUE"), (uint32)ESimpleElementBlendMode::SE_BLEND_Opaque);
		OutEnvironment.SetDefine(TEXT("SE_BLEND_MASKED"), (uint32)ESimpleElementBlendMode::SE_BLEND_Masked);
		OutEnvironment.SetDefine(TEXT("SE_BLEND_TRANSLUCENT"), (uint32)ESimpleElementBlendMode::SE_BLEND_Translucent);
		OutEnvironment.SetDefine(TEXT("SE_BLEND_ADDITIVE"), (uint32)ESimpleElementBlendMode::SE_BLEND_Additive);
		OutEnvironment.SetDefine(TEXT("SE_BLEND_MODULATE"), (uint32)ESimpleElementBlendMode::SE_BLEND_Modulate);
		OutEnvironment.SetDefine(TEXT("SE_BLEND_MASKEDDISTANCEFIELD"), (uint32)ESimpleElementBlendMode::SE_BLEND_MaskedDistanceField);
		OutEnvironment.SetDefine(TEXT("SE_BLEND_MASKEDDISTANCEFIELDSHADOWED"), (uint32)ESimpleElementBlendMode::SE_BLEND_MaskedDistanceFieldShadowed);
		OutEnvironment.SetDefine(TEXT("SE_BLEND_ALPHACOMPOSITE"), (uint32)ESimpleElementBlendMode::SE_BLEND_AlphaComposite);
		OutEnvironment.SetDefine(TEXT("SE_BLEND_ALPHABLEND"), (uint32)ESimpleElementBlendMode::SE_BLEND_AlphaBlend);
		OutEnvironment.SetDefine(TEXT("SE_BLEND_TRANSLUCENTALPHAONLY"), (uint32)ESimpleElementBlendMode::SE_BLEND_TranslucentAlphaOnly);
		OutEnvironment.SetDefine(TEXT("SE_BLEND_TRANSLUCENTALPHAONLYWRITEALPHA"), (uint32)ESimpleElementBlendMode::SE_BLEND_TranslucentAlphaOnlyWriteAlpha);
		OutEnvironment.SetDefine(TEXT("SE_BLEND_TRANSLUCENTDISTANCEFIELD"), (uint32)ESimpleElementBlendMode::SE_BLEND_TranslucentDistanceField);
		OutEnvironment.SetDefine(TEXT("SE_BLEND_TRANSLUCENTDISTANCEFIELDSHADOWED"), (uint32)ESimpleElementBlendMode::SE_BLEND_TranslucentDistanceFieldShadowed);
		OutEnvironment.SetDefine(TEXT("SE_BLEND_MODE"), EncodedBlendMode);
		OutEnvironment.SetDefine(TEXT("USE_32BPP_HDR"), 1u);
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return ((IsES2Platform(Platform) && IsPCPlatform(Platform)) || Platform == SP_OPENGL_ES2_ANDROID) && TSimpleElementBase::ShouldCache(Platform);
	}
};

typedef FSimpleElementGammaPS<true> FSimpleElementGammaPS_SRGB;
typedef FSimpleElementGammaPS<false> FSimpleElementGammaPS_Linear;
typedef FSimpleElementMaskedGammaPS<true> FSimpleElementMaskedGammaPS_SRGB;
typedef FSimpleElementMaskedGammaPS<false> FSimpleElementMaskedGammaPS_Linear;
