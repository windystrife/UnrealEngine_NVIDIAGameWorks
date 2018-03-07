// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderResource.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "Modules/ModuleManager.h"

#if defined(HAS_MORPHEUS) && HAS_MORPHEUS

/**
 * Pixel shader to convert RGBA to YUV420.
 *
 * YUV420 has 8 bit intensity values in the top 2/3 of the texture for every pixel and 8 bit each UV coordinates 
 * into the YUV colorspace in the bottom 1/3 for every pixel quad.
 *
 * This is only used by PS4.
 */
class FRGBAToYUV420CS
	: public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRGBAToYUV420CS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		if (Platform == EShaderPlatform::SP_PS4)
		{
			// we must use a run time check for this because the builds the build machines create will have Morpheus defined,
			// but a user will not necessarily have the Morpheus files
			return FModuleManager::Get().ModuleExists(TEXT("Morpheus"));
		}

		return false;
	}

	FRGBAToYUV420CS() { }

	FRGBAToYUV420CS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TargetHeight.Bind(Initializer.ParameterMap, TEXT("TargetHeight"), SPF_Mandatory);
		ScaleFactorX.Bind(Initializer.ParameterMap, TEXT("ScaleFactorX"), SPF_Mandatory);
		ScaleFactorY.Bind(Initializer.ParameterMap, TEXT("ScaleFactorY"), SPF_Mandatory);
		TextureYOffset.Bind(Initializer.ParameterMap, TEXT("TextureYOffset"), SPF_Mandatory);
		SrcTexture.Bind(Initializer.ParameterMap, TEXT("SrcTexture"), SPF_Mandatory);
		OutTextureRW.Bind(Initializer.ParameterMap, TEXT("OutTexture"), SPF_Mandatory);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << TargetHeight << ScaleFactorX << ScaleFactorY << TextureYOffset << SrcTexture << OutTextureRW;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> SrcTex, FUnorderedAccessViewRHIParamRef OutUAV, float TargetHeight, float ScaleFactorX, float ScaleFactorY, float TextureYOffset);
	void UnbindBuffers(FRHICommandList& RHICmdList);

protected:
	FShaderParameter TargetHeight;
	FShaderParameter ScaleFactorX;
	FShaderParameter ScaleFactorY;
	FShaderParameter TextureYOffset;
	FShaderResourceParameter SrcTexture;

	FShaderResourceParameter OutTextureRW;
};

#endif //defined(HAS_MORPHEUS) && HAS_MORPHEUS
