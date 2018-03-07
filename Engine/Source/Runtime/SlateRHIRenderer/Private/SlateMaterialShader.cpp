// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SlateMaterialShader.h"
#include "Materials/Material.h"
#include "ShaderParameterUtils.h"


FSlateMaterialShaderVS::FSlateMaterialShaderVS(const FMaterialShaderType::CompiledShaderInitializerType& Initializer)
	: FMaterialShader(Initializer)
{
	ViewProjection.Bind(Initializer.ParameterMap, TEXT("ViewProjection"));
	SwitchVerticalAxisMultiplier.Bind( Initializer.ParameterMap, TEXT("SwitchVerticalAxisMultiplier"));
}


void FSlateMaterialShaderVS::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	// Set defines based on what this shader will be used for
	OutEnvironment.SetDefine( TEXT("USE_MATERIALS"), 1 );
	OutEnvironment.SetDefine( TEXT("NUM_CUSTOMIZED_UVS"), Material->GetNumCustomizedUVs() );
	OutEnvironment.SetDefine( TEXT("HAS_SCREEN_POSITION"), Material->HasVertexPositionOffsetConnected() );

	FMaterialShader::ModifyCompilationEnvironment( Platform, Material, OutEnvironment );
}

bool FSlateMaterialShaderVS::ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
{
	return Material->GetMaterialDomain() == MD_UI;
}

void FSlateMaterialShaderVS::SetViewProjection(FRHICommandList& RHICmdList, const FMatrix& InViewProjection )
{
	SetShaderValue(RHICmdList, GetVertexShader(), ViewProjection, InViewProjection );
}

void FSlateMaterialShaderVS::SetMaterialShaderParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial* Material)
{
	const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

	const bool bDeferredPass = false;
	FMaterialShader::SetParameters<FVertexShaderRHIParamRef>(RHICmdList, ShaderRHI, MaterialRenderProxy, *Material, View, View.ViewUniformBuffer, bDeferredPass, ESceneRenderTargetsMode::DontSet);
}

void FSlateMaterialShaderVS::SetVerticalAxisMultiplier(FRHICommandList& RHICmdList, float InMultiplier )
{
	SetShaderValue(RHICmdList, GetVertexShader(), SwitchVerticalAxisMultiplier, InMultiplier );
}


bool FSlateMaterialShaderVS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);

	Ar << ViewProjection;
	Ar << SwitchVerticalAxisMultiplier;

	return bShaderHasOutdatedParameters;
}


bool FSlateMaterialShaderPS::ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
{
	return Material->GetMaterialDomain() == MD_UI;
}


void FSlateMaterialShaderPS::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	// Set defines based on what this shader will be used for
	OutEnvironment.SetDefine( TEXT("USE_MATERIALS"), 1 );
	OutEnvironment.SetDefine( TEXT("NUM_CUSTOMIZED_UVS"), Material->GetNumCustomizedUVs() );

	FMaterialShader::ModifyCompilationEnvironment( Platform, Material, OutEnvironment );
}

FSlateMaterialShaderPS::FSlateMaterialShaderPS(const FMaterialShaderType::CompiledShaderInitializerType& Initializer)
	: FMaterialShader(Initializer)
{
	ShaderParams.Bind(Initializer.ParameterMap, TEXT("ShaderParams"));
	GammaValues.Bind(Initializer.ParameterMap, TEXT("GammaValues"));
	AdditionalTextureParameter.Bind(Initializer.ParameterMap, TEXT("ElementTexture"));
	TextureParameterSampler.Bind(Initializer.ParameterMap, TEXT("ElementTextureSampler"));
}

void FSlateMaterialShaderPS::SetBlendState(FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FMaterial* Material)
{
	EBlendMode BlendMode = Material->GetBlendMode();

	switch (BlendMode)
	{
	default:
	case BLEND_Opaque:
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		break;
	case BLEND_Masked:
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		break;
	case BLEND_Translucent:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_InverseDestAlpha, BF_One>::GetRHI();
		break;
	case BLEND_Additive:
		// Add to the existing scene color
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
		break;
	case BLEND_Modulate:
		// Modulate with the existing scene color
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_Zero, BF_SourceColor>::GetRHI();
		break;
	case BLEND_AlphaComposite:
		// Blend with existing scene color. New color is already pre-multiplied by alpha.
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
		break;
	};
}

void FSlateMaterialShaderPS::SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial* Material, const FVector4& InShaderParams)
{
	const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

	SetShaderValue( RHICmdList, ShaderRHI, ShaderParams, InShaderParams );

	const bool bDeferredPass = false;
	FMaterialShader::SetParameters<FPixelShaderRHIParamRef>(RHICmdList, ShaderRHI, MaterialRenderProxy, *Material, View, View.ViewUniformBuffer, bDeferredPass, ESceneRenderTargetsMode::SetTextures);
}

void FSlateMaterialShaderPS::SetAdditionalTexture( FRHICommandList& RHICmdList, const FTextureRHIParamRef InTexture, const FSamplerStateRHIRef SamplerState )
{
	SetTextureParameter(RHICmdList, GetPixelShader(), AdditionalTextureParameter, TextureParameterSampler, SamplerState, InTexture );
}

void FSlateMaterialShaderPS::SetDisplayGamma(FRHICommandList& RHICmdList, float InDisplayGamma)
{
	FVector2D InGammaValues(2.2f / InDisplayGamma, 1.0f/InDisplayGamma);

	SetShaderValue(RHICmdList, GetPixelShader(), GammaValues, InGammaValues);
}

bool FSlateMaterialShaderPS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
	Ar << GammaValues;
	Ar << ShaderParams;
	Ar << TextureParameterSampler;
	Ar << AdditionalTextureParameter;
	return bShaderHasOutdatedParameters;
}


#define IMPLEMENT_SLATE_VERTEXMATERIALSHADER_TYPE(bUseInstancing) \
	typedef TSlateMaterialShaderVS<bUseInstancing> TSlateMaterialShaderVS##bUseInstancing; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TSlateMaterialShaderVS##bUseInstancing, TEXT("/Engine/Private/SlateVertexShader.usf"), TEXT("Main"), SF_Vertex);

/** Instancing vertex shader */
IMPLEMENT_SLATE_VERTEXMATERIALSHADER_TYPE(true);
/** Non instancing vertex shader */
IMPLEMENT_SLATE_VERTEXMATERIALSHADER_TYPE(false);

#define IMPLEMENT_SLATE_MATERIALSHADER_TYPE(ShaderType, bDrawDisabledEffect) \
	typedef TSlateMaterialShaderPS<ESlateShader::ShaderType, bDrawDisabledEffect> TSlateMaterialShaderPS##ShaderType##bDrawDisabledEffect; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TSlateMaterialShaderPS##ShaderType##bDrawDisabledEffect, TEXT("/Engine/Private/SlateElementPixelShader.usf"), TEXT("Main"), SF_Pixel);

IMPLEMENT_SLATE_MATERIALSHADER_TYPE(Custom, false)

IMPLEMENT_SLATE_MATERIALSHADER_TYPE(Default, true);
IMPLEMENT_SLATE_MATERIALSHADER_TYPE(Default, false);
IMPLEMENT_SLATE_MATERIALSHADER_TYPE(Border, true);
IMPLEMENT_SLATE_MATERIALSHADER_TYPE(Border, false);
IMPLEMENT_SLATE_MATERIALSHADER_TYPE(Font, true);
IMPLEMENT_SLATE_MATERIALSHADER_TYPE(Font, false);
