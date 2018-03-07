// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"

class FClearReplacementVS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FClearReplacementVS, Global, UTILITYSHADERS_API);
public:
	FClearReplacementVS() {}
	FClearReplacementVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader( Initializer )
	{
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
	
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}
};

class FClearReplacementPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FClearReplacementPS, Global, UTILITYSHADERS_API);
public:
	FClearReplacementPS() {}
	FClearReplacementPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader( Initializer )
	{
		ClearColor.Bind(Initializer.ParameterMap, TEXT("ClearColor"), SPF_Mandatory);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ClearColor;
		return bShaderHasOutdatedParameters;
	}
	
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_A32B32G32R32F);
	}
	
protected:
	FShaderParameter ClearColor;
};

template< typename T >
class FClearTexture2DReplacementCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FClearTexture2DReplacementCS, Global, UTILITYSHADERS_API);
public:
	FClearTexture2DReplacementCS() {}
	FClearTexture2DReplacementCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader( Initializer )
	{
		ClearColor.Bind(Initializer.ParameterMap, TEXT("ClearColor"), SPF_Mandatory);
		ClearTextureRW.Bind(Initializer.ParameterMap, TEXT("ClearTextureRW"), SPF_Mandatory);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ClearColor << ClearTextureRW;
		return bShaderHasOutdatedParameters;
	}
	
	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW, const T(&Values)[4]);
	UTILITYSHADERS_API void FinalizeParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW);
	
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine( TEXT("Type"), TIsSame< T, float >::Value ? TEXT("float4") : TEXT("uint4") );
	}
	
	const FShaderParameter& GetClearColorParameter()
	{
		return ClearColor;
	}
	
	const FShaderResourceParameter& GetClearTextureRWParameter()
	{
		return ClearTextureRW;
	}
	
protected:
	FShaderParameter ClearColor;
	FShaderResourceParameter ClearTextureRW;
};

template< typename T >
class FClearTexture2DArrayReplacementCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FClearTexture2DArrayReplacementCS, Global, UTILITYSHADERS_API);
public:
	FClearTexture2DArrayReplacementCS() {}
	FClearTexture2DArrayReplacementCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ClearColor.Bind(Initializer.ParameterMap, TEXT("ClearColor"), SPF_Mandatory);
		ClearTextureArrayRW.Bind(Initializer.ParameterMap, TEXT("ClearTextureArrayRW"), SPF_Mandatory);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ClearColor << ClearTextureArrayRW;
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW, const T(&Values)[4]);
	UTILITYSHADERS_API void FinalizeParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine( TEXT("Type"), TIsSame< T, float >::Value ? TEXT("float4") : TEXT("uint4") );
	}

	const FShaderParameter& GetClearColorParameter()
	{
		return ClearColor;
	}

	const FShaderResourceParameter& GetClearTextureRWParameter()
	{
		return ClearTextureArrayRW;
	}

protected:
	FShaderParameter ClearColor;
	FShaderResourceParameter ClearTextureArrayRW;
};

template< typename T >
class FClearVolumeReplacementCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FClearVolumeReplacementCS, Global, UTILITYSHADERS_API);
public:
	FClearVolumeReplacementCS() {}
	FClearVolumeReplacementCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ClearColor.Bind(Initializer.ParameterMap, TEXT("ClearColor"), SPF_Mandatory);
		ClearVolumeRW.Bind(Initializer.ParameterMap, TEXT("ClearVolumeRW"), SPF_Mandatory);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ClearColor << ClearVolumeRW;
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW, const T(&Values)[4]);
	UTILITYSHADERS_API void FinalizeParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine( TEXT("Type"), TIsSame< T, float >::Value ? TEXT("float4") : TEXT("uint4") );
	}

	const FShaderParameter& GetClearColorParameter()
	{
		return ClearColor;
	}

	const FShaderResourceParameter& GetClearTextureRWParameter()
	{
		return ClearVolumeRW;
	}

protected:
	FShaderParameter ClearColor;
	FShaderResourceParameter ClearVolumeRW;
};

class FClearTexture2DReplacementScissorCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FClearTexture2DReplacementScissorCS, Global, UTILITYSHADERS_API);
public:
	FClearTexture2DReplacementScissorCS() {}
	FClearTexture2DReplacementScissorCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ClearColor.Bind(Initializer.ParameterMap, TEXT("ClearColor"), SPF_Mandatory);
		TargetBounds.Bind(Initializer.ParameterMap, TEXT("TargetBounds"), SPF_Mandatory);
		ClearTextureRW.Bind(Initializer.ParameterMap, TEXT("ClearTextureRW"), SPF_Mandatory);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ClearColor << TargetBounds << ClearTextureRW;
		return bShaderHasOutdatedParameters;
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW, FLinearColor ClearColor, const FVector4& InTargetBounds);
	UTILITYSHADERS_API void FinalizeParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	const FShaderParameter& GetClearColorParameter()
	{
		return ClearColor;
	}

	const FShaderParameter& GetTargetBoundsParameter()
	{
		return TargetBounds;
	}

	const FShaderResourceParameter& GetClearTextureRWParameter()
	{
		return ClearTextureRW;
	}

protected:
	FShaderParameter ClearColor;
	FShaderParameter TargetBounds;
	FShaderResourceParameter ClearTextureRW;
};

class FClearBufferReplacementCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FClearBufferReplacementCS, Global, UTILITYSHADERS_API);
public:
	FClearBufferReplacementCS() {}
	FClearBufferReplacementCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader( Initializer )
	{
		ClearBufferCSParams.Bind(Initializer.ParameterMap, TEXT("ClearBufferCSParams"), SPF_Mandatory);
		ClearBufferRW.Bind(Initializer.ParameterMap, TEXT("ClearBufferRW"), SPF_Mandatory);
	}
	
	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef BufferRW, uint32 NumDWordsToClear, uint32 ClearValue);
	UTILITYSHADERS_API void FinalizeParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef BufferRW);
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ClearBufferCSParams << ClearBufferRW;
		return bShaderHasOutdatedParameters;
	}
	
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}
	
	//protected:
	FShaderParameter ClearBufferCSParams;
	FShaderResourceParameter ClearBufferRW;
};
