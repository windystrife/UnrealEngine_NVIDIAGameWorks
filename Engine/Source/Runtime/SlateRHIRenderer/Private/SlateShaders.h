// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "Rendering/RenderingCommon.h"

extern uint32 GSlateShaderColorVisionDeficiencyType;

/**
 * The vertex declaration for the slate vertex shader
 */
class FSlateVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual ~FSlateVertexDeclaration() {}

	/** Initializes the vertex declaration RHI resource */
	virtual void InitRHI() override;

	/** Releases the vertex declaration RHI resource */
	virtual void ReleaseRHI() override;
};

/**
 * The vertex declaration for the slate instanced vertex shader
 */
class FSlateInstancedVertexDeclaration : public FSlateVertexDeclaration
{
public:
	virtual ~FSlateInstancedVertexDeclaration() {}
	
	/** Initializes the vertex declaration RHI resource */
	virtual void InitRHI() override;
};

class FSlateMaskingVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual ~FSlateMaskingVertexDeclaration() {}

	/** Initializes the vertex declaration RHI resource */
	virtual void InitRHI() override;

	/** Releases the vertex declaration RHI resource */
	virtual void ReleaseRHI() override;
};

/** The slate Vertex shader representation */
class FSlateElementVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSlateElementVS, Global);
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCache( EShaderPlatform Platform ) { return true; }

	/** Constructor.  Binds all parameters used by the shader */
	FSlateElementVS( const ShaderMetaType::CompiledShaderInitializerType& Initializer );

	FSlateElementVS() {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);

	/** 
	 * Sets the view projection parameter
	 *
	 * @param InViewProjection	The ViewProjection matrix to use when this shader is bound 
	 */
	void SetViewProjection(FRHICommandList& RHICmdList, const FMatrix& InViewProjection );

	/** 
	 * Sets shader parameters for use in this shader
	 *
	 * @param ShaderParams	The shader params to be used
	 */
	void SetShaderParameters(FRHICommandList& RHICmdList, const FVector4& ShaderParams );

	/**
	 * Sets the vertical axis multiplier to use depending on graphics api
	 */
	void SetVerticalAxisMultiplier(FRHICommandList& RHICmdList, float InMultiplier);

	/** Serializes the shader data */
	virtual bool Serialize( FArchive& Ar ) override;

private:
	/** ViewProjection parameter used by the shader */
	FShaderParameter ViewProjection;
	/** Shader parmeters used by the shader */
	FShaderParameter VertexShaderParams;
	/** Parameter used to determine if we need to swtich the vertical axis for opengl */
	FShaderParameter SwitchVerticalAxisMultiplier;
};

/** 
 * Base class slate pixel shader for all elements
 */
class FSlateElementPS : public FGlobalShader
{	
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCache( EShaderPlatform Platform ) 
	{ 
		return true; 
	}

	FSlateElementPS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	FSlateElementPS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader( Initializer )
	{
		TextureParameter.Bind( Initializer.ParameterMap, TEXT("ElementTexture"));
		TextureParameterSampler.Bind( Initializer.ParameterMap, TEXT("ElementTextureSampler"));
		ShaderParams.Bind( Initializer.ParameterMap, TEXT("ShaderParams"));
		GammaValues.Bind( Initializer.ParameterMap,TEXT("GammaValues"));
		InvertAlpha.Bind(Initializer.ParameterMap, TEXT("InvertAlpha"));
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);

	/**
	 * Sets the texture used by this shader 
	 *
	 * @param Texture	Texture resource to use when this pixel shader is bound
	 * @param SamplerState	Sampler state to use when sampling this texture
	 */
	void SetTexture(FRHICommandList& RHICmdList, const FTextureRHIParamRef InTexture, const FSamplerStateRHIRef SamplerState )
	{
		SetTextureParameter(RHICmdList, GetPixelShader(), TextureParameter, TextureParameterSampler, SamplerState, InTexture );
	}

	/**
	 * Sets shader params used by the shader
	 * 
	 * @param InShaderParams Shader params to use
	 */
	void SetShaderParams(FRHICommandList& RHICmdList, const FVector4& InShaderParams )
	{
		SetShaderValue(RHICmdList, GetPixelShader(), ShaderParams, InShaderParams );
	}

	/**
	 * Sets the display gamma.
	 *
	 * @param DisplayGamma The display gamma to use
	 */
	void SetDisplayGamma(FRHICommandList& RHICmdList, float InDisplayGamma)
	{
		FVector2D InGammaValues( 2.2f / InDisplayGamma, 1.0f/InDisplayGamma );

		SetShaderValue(RHICmdList, GetPixelShader(),GammaValues,InGammaValues);
	}

	/**
	 * Sets if we should invert alpha of the incoming.
	 */
	void SetInvertAlpha(FRHICommandList& RHICmdList, bool bInvertAlpha )
	{
		SetShaderValue(RHICmdList, GetPixelShader(), InvertAlpha, bInvertAlpha ? 1.0f : 0.0f );
	}

	virtual bool Serialize( FArchive& Ar )
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );

		Ar << TextureParameter;
		Ar << TextureParameterSampler;
		Ar << ShaderParams;
		Ar << GammaValues;
		Ar << InvertAlpha;

		return bShaderHasOutdatedParameters;
	}
private:
	/** Texture parameter used by the shader */
	FShaderResourceParameter TextureParameter;
	FShaderResourceParameter TextureParameterSampler;
	FShaderParameter ShaderParams;
	FShaderParameter GammaValues;
	FShaderParameter InvertAlpha;
};

/** 
 * Pixel shader types for all elements
 */
template<ESlateShader::Type ShaderType, bool bDrawDisabledEffect, bool bUseTextureAlpha=true>
class TSlateElementPS : public FSlateElementPS
{
	DECLARE_SHADER_TYPE( TSlateElementPS, Global );
public:

	TSlateElementPS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	TSlateElementPS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FSlateElementPS( Initializer )
	{
	}


	/**
	 * Modifies the compilation of this shader
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Set defines based on what this shader will be used for
		OutEnvironment.SetDefine(TEXT("SHADER_TYPE"), (uint32)ShaderType);
		OutEnvironment.SetDefine(TEXT("DRAW_DISABLED_EFFECT"), (uint32)( bDrawDisabledEffect ? 1 : 0 ));
		OutEnvironment.SetDefine(TEXT("USE_TEXTURE_ALPHA"), (uint32)( bUseTextureAlpha ? 1 : 0 ));
		OutEnvironment.SetDefine(TEXT("COLOR_VISION_DEFICIENCY_TYPE"), GSlateShaderColorVisionDeficiencyType);
		OutEnvironment.SetDefine(TEXT("USE_MATERIALS"), (uint32)0);

		FSlateElementPS::ModifyCompilationEnvironment( Platform, OutEnvironment );
	}

	/** Serializes the shader data */
	virtual bool Serialize( FArchive& Ar ) override
	{
		return FSlateElementPS::Serialize( Ar );
	}
private:

};

/** 
 * Pixel shader for debugging Slate overdraw
 */
class FSlateDebugOverdrawPS : public FSlateElementPS
{	
	DECLARE_SHADER_TYPE( FSlateDebugOverdrawPS, Global );
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCache( EShaderPlatform Platform ) 
	{ 
		return true; 
	}

	FSlateDebugOverdrawPS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	FSlateDebugOverdrawPS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FSlateElementPS( Initializer )
	{
	}


	virtual bool Serialize( FArchive& Ar ) override
	{
		return FSlateElementPS::Serialize( Ar );
	}
private:
};

/** 
 * Pixel shader for debugging Slate overdraw
 */
class FSlateDebugBatchingPS : public FSlateElementPS
{	
	DECLARE_SHADER_TYPE(FSlateDebugBatchingPS, Global );
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCache( EShaderPlatform Platform ) 
	{ 
		return true; 
	}

	FSlateDebugBatchingPS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	FSlateDebugBatchingPS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FSlateElementPS( Initializer )
	{
		BatchColor.Bind(Initializer.ParameterMap, TEXT("BatchColor"));
	}

	/**
	* Sets shader params used by the shader
	*
	* @param InShaderParams Shader params to use
	*/
	void SetBatchColor(FRHICommandList& RHICmdList, const FLinearColor& InBatchColor)
	{
		SetShaderValue(RHICmdList, GetPixelShader(), BatchColor, InBatchColor);
	}

	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FSlateElementPS::Serialize( Ar );

		Ar << BatchColor;

		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter BatchColor;
};

const int32 MAX_BLUR_SAMPLES = 127;

class FSlatePostProcessBlurPS : public FSlateElementPS
{
	DECLARE_SHADER_TYPE(FSlatePostProcessBlurPS, Global);
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	FSlatePostProcessBlurPS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	FSlatePostProcessBlurPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FSlateElementPS(Initializer)
	{
		BufferSizeAndDirection.Bind(Initializer.ParameterMap, TEXT("BufferSizeAndDirection"));
		WeightAndOffsets.Bind(Initializer.ParameterMap, TEXT("WeightAndOffsets"));
		SampleCount.Bind(Initializer.ParameterMap, TEXT("SampleCount"));
		UVBounds.Bind(Initializer.ParameterMap, TEXT("UVBounds"));
	}

	void SetBufferSizeAndDirection(FRHICommandList& RHICmdList, const FVector2D& InBufferSize, const FVector2D& InDir)
	{
		SetShaderValue(RHICmdList, GetPixelShader(), BufferSizeAndDirection, FVector4(InBufferSize, InDir));
	}

	void SetWeightsAndOffsets(FRHICommandList& RHICmdList, const TArray<FVector4>& InWeightsAndOffsets, int32 NumSamples )
	{
		check(InWeightsAndOffsets.Num() <= MAX_BLUR_SAMPLES);
		SetShaderValueArray<FPixelShaderRHIParamRef, FVector4>(RHICmdList, GetPixelShader(), WeightAndOffsets, InWeightsAndOffsets.GetData(), InWeightsAndOffsets.Num() );
		SetShaderValue(RHICmdList, GetPixelShader(), SampleCount, NumSamples);
	}

	void SetUVBounds(FRHICommandList& RHICmdList, const FVector4& InUVBounds)
	{
		SetShaderValue(RHICmdList, GetPixelShader(), UVBounds, InUVBounds);
	}

	
	virtual bool Serialize(FArchive& Ar) override
	{
		Ar << BufferSizeAndDirection;
		Ar << WeightAndOffsets;
		Ar << SampleCount;	
		Ar << UVBounds;
		return FSlateElementPS::Serialize(Ar);
	}
private:
	FShaderParameter BufferSizeAndDirection;
	FShaderParameter WeightAndOffsets;
	FShaderParameter SampleCount;
	FShaderParameter UVBounds;
};


class FSlatePostProcessDownsamplePS : public FSlateElementPS
{
	DECLARE_SHADER_TYPE(FSlatePostProcessDownsamplePS, Global);
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	FSlatePostProcessDownsamplePS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	FSlatePostProcessDownsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FSlateElementPS(Initializer)
	{
		UVBounds.Bind(Initializer.ParameterMap, TEXT("UVBounds"));
	}

	void SetUVBounds(FRHICommandList& RHICmdList, const FVector4& InUVBounds)
	{
		SetShaderValue(RHICmdList, GetPixelShader(), UVBounds, InUVBounds);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		Ar << UVBounds;
		return FSlateElementPS::Serialize(Ar);
	}
private:
	FShaderParameter UVBounds;
};



class FSlateMaskingVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSlateMaskingVS, Global);
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCache( EShaderPlatform Platform )
	{
		return true;
	}

	FSlateMaskingVS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	FSlateMaskingVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	/**
	* Sets the view projection parameter
	*
	* @param InViewProjection	The ViewProjection matrix to use when this shader is bound
	*/
	void SetViewProjection(FRHICommandList& RHICmdList, const FMatrix& InViewProjection);

	/**
	 * Sets the vertical axis multiplier to use depending on graphics api
	 */
	void SetVerticalAxisMultiplier(FRHICommandList& RHICmdList, float InMultiplier);

	virtual bool Serialize(FArchive& Ar) override;

private:

	/** ViewProjection parameter used by the shader */
	FShaderParameter ViewProjection;
	/** Parameter used to determine if we need to swtich the vertical axis for opengl */
	FShaderParameter SwitchVerticalAxisMultiplier;
};

class FSlateMaskingPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSlateMaskingPS, Global);
public:
	/** Indicates that this shader should be cached */
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	FSlateMaskingPS()
	{
	}

	/** Constructor.  Binds all parameters used by the shader */
	FSlateMaskingPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		return FGlobalShader::Serialize(Ar);
	}
};


/** The simple element vertex declaration. */
extern TGlobalResource<FSlateVertexDeclaration> GSlateVertexDeclaration;

/** The instanced simple element vertex declaration. */
extern TGlobalResource<FSlateInstancedVertexDeclaration> GSlateInstancedVertexDeclaration;

/** The vertex declaration for rendering stencil masks. */
extern TGlobalResource<FSlateMaskingVertexDeclaration> GSlateMaskingVertexDeclaration;
