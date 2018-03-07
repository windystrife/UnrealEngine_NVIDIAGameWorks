// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	NormalMapPreview.h: Implementation for previewing normal maps.
==============================================================================*/

#include "NormalMapPreview.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "SimpleElementShaders.h"
#include "ShaderParameterUtils.h"
#include "PipelineStateCache.h"

/*------------------------------------------------------------------------------
	Batched element shaders for previewing normal maps.
------------------------------------------------------------------------------*/

/**
 * Simple pixel shader that reconstructs a normal for the purposes of visualization.
 */
class FSimpleElementNormalMapPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSimpleElementNormalMapPS,Global);
public:

	/** Should the shader be cached? Always. */
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	/** Default constructor. */
	FSimpleElementNormalMapPS() {}

	/**
	 * Initialization constructor.
	 * @param Initializer - Shader initialization container.
	 */
	FSimpleElementNormalMapPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		Texture.Bind(Initializer.ParameterMap,TEXT("NormalMapTexture"));
		TextureSampler.Bind(Initializer.ParameterMap,TEXT("NormalMapTextureSampler"));
	}

	/**
	 * Set shader parameters.
	 * @param NormalMapTexture - The normal map texture to sample.
	 */
	void SetParameters(FRHICommandList& RHICmdList, const FTexture* NormalMapTexture)
	{
		FPixelShaderRHIParamRef PixelShaderRHI = GetPixelShader();
		SetTextureParameter(RHICmdList, PixelShaderRHI,Texture,TextureSampler,NormalMapTexture);
	}

	/**
	 * Serialization.
	 * @param Ar - The archive with which to serialize.
	 */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << Texture;
		Ar << TextureSampler;
		return bShaderHasOutdatedParameters;
	}

private:
	/** The texture to sample. */
	FShaderResourceParameter Texture;
	FShaderResourceParameter TextureSampler;
};
IMPLEMENT_SHADER_TYPE(,FSimpleElementNormalMapPS,TEXT("/Engine/Private/SimpleElementNormalMapPixelShader.usf"),TEXT("Main"),SF_Pixel);

/** Binds vertex and pixel shaders for this element */
void FNormalMapBatchedElementParameters::BindShaders(
	FRHICommandList& RHICmdList,
	FGraphicsPipelineStateInitializer& GraphicsPSOInit,
	ERHIFeatureLevel::Type InFeatureLevel,
	const FMatrix& InTransform,
	const float InGamma,
	const FMatrix& ColorWeights,
	const FTexture* Texture)
{
	TShaderMapRef<FSimpleElementVS> VertexShader(GetGlobalShaderMap(InFeatureLevel));
	TShaderMapRef<FSimpleElementNormalMapPS> PixelShader(GetGlobalShaderMap(InFeatureLevel));

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GSimpleElementVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, EApplyRendertargetOption::ForceApply);

	VertexShader->SetParameters(RHICmdList, InTransform);
	PixelShader->SetParameters(RHICmdList, Texture);
}

