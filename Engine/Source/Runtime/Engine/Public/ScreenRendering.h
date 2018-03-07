// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenRendering.h: Screen rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "SceneView.h"

struct FScreenVertex
{
	FVector2D Position;
	FVector2D UV;
};

/** The filter vertex declaration resource type. */
class FScreenVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Destructor.
	virtual ~FScreenVertexDeclaration() {}

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		uint32 Stride = sizeof(FScreenVertex);
		Elements.Add(FVertexElement(0,STRUCT_OFFSET(FScreenVertex,Position),VET_Float2,0,Stride));
		Elements.Add(FVertexElement(0,STRUCT_OFFSET(FScreenVertex,UV),VET_Float2,1,Stride));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

extern ENGINE_API TGlobalResource<FScreenVertexDeclaration> GScreenVertexDeclaration;

/**
 * A pixel shader for rendering a textured screen element.
 */
class FScreenPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FScreenPS,Global,ENGINE_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FScreenPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		InTexture.Bind(Initializer.ParameterMap,TEXT("InTexture"), SPF_Mandatory);
		InTextureSampler.Bind(Initializer.ParameterMap,TEXT("InTextureSampler"));
	}
	FScreenPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture)
	{
		SetTextureParameter(RHICmdList, GetPixelShader(),InTexture,InTextureSampler,Texture);
	}

	void SetParameters(FRHICommandList& RHICmdList, FSamplerStateRHIParamRef SamplerStateRHI, FTextureRHIParamRef TextureRHI)
	{
		SetTextureParameter(RHICmdList, GetPixelShader(),InTexture,InTextureSampler,SamplerStateRHI,TextureRHI);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InTexture;
		Ar << InTextureSampler;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter InTexture;
	FShaderResourceParameter InTextureSampler;
};

class FScreenPS_OSE : public FGlobalShader
{
    DECLARE_EXPORTED_SHADER_TYPE(FScreenPS_OSE,Global,ENGINE_API);
public:

    static bool ShouldCache(EShaderPlatform Platform) { return true; }

    FScreenPS_OSE(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
        FGlobalShader(Initializer)
    {
        InTexture.Bind(Initializer.ParameterMap,TEXT("InTexture"), SPF_Mandatory);
        InTextureSampler.Bind(Initializer.ParameterMap,TEXT("InTextureSampler"));
    }

    FScreenPS_OSE() {}

    void SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture)
    {
        SetTextureParameter(RHICmdList, GetPixelShader(),InTexture,InTextureSampler,Texture);
    }

    void SetParameters(FRHICommandList& RHICmdList, FSamplerStateRHIParamRef SamplerStateRHI, FTextureRHIParamRef TextureRHI)
    {
        SetTextureParameter(RHICmdList, GetPixelShader(),InTexture,InTextureSampler,SamplerStateRHI,TextureRHI);
    }

    virtual bool Serialize(FArchive& Ar) override
    {
        bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
        Ar << InTexture;
        Ar << InTextureSampler;
        return bShaderHasOutdatedParameters;
    }

private:
    FShaderResourceParameter InTexture;
    FShaderResourceParameter InTextureSampler;
};

/**
 * A vertex shader for rendering a textured screen element.
 */
class FScreenVS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FScreenVS,Global,ENGINE_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FScreenVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
	}
	FScreenVS() {}


	void SetParameters(FRHICommandList& RHICmdList, const FUniformBufferRHIParamRef ViewUniformBuffer)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(), ViewUniformBuffer);
	}


	virtual bool Serialize(FArchive& Ar) override
	{
		return FGlobalShader::Serialize(Ar);
	}
};

template<bool bUsingVertexLayers=false>
class TScreenVSForGS : public FScreenVS
{
	DECLARE_EXPORTED_SHADER_TYPE(TScreenVSForGS,Global,ENGINE_API);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && (!bUsingVertexLayers || RHISupportsVertexShaderLayer(Platform));
	}

	TScreenVSForGS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	  FScreenVS(Initializer)
	  {
	  }
	TScreenVSForGS() {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FScreenVS::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USING_LAYERS"), (uint32)(bUsingVertexLayers ? 1 : 0));
		if (!bUsingVertexLayers)
		{
			OutEnvironment.CompilerFlags.Add( CFLAG_VertexToGeometryShader );
		}
	}
};

