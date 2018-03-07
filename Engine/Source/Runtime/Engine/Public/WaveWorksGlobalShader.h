#pragma once
#include "GlobalShader.h"
#include "ShaderParameters.h"
#include "ShaderParameterUtils.h"

BEGIN_UNIFORM_BUFFER_STRUCT(FWaveWorksShorelineDFUniformParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, MaxPixelsToShoreline)
END_UNIFORM_BUFFER_STRUCT(FWaveWorksShorelineDFUniformParameters)

typedef TUniformBufferRef<FWaveWorksShorelineDFUniformParameters> FWaveWorksShorelineDFUniformBufferRef;

// Preprocess distance field texture CS
class FPreprocessShorelineDistanceFieldTexCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPreprocessShorelineDistanceFieldTexCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FPreprocessShorelineDistanceFieldTexCS()
	{
	}

	/** Initialization constructor. */
	explicit FPreprocessShorelineDistanceFieldTexCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		OriginShorelineDFTexture.Bind(Initializer.ParameterMap, TEXT("OriginDistanceFieldTexture"));
		OutShorelineDFUAV.Bind(Initializer.ParameterMap, TEXT("OutputDistanceFieldTexture"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << OriginShorelineDFTexture;
		Ar << OutShorelineDFUAV;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,FUnorderedAccessViewRHIParamRef OutShorelineDFUAVRef)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutShorelineDFUAV.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutShorelineDFUAV.GetBaseIndex(), OutShorelineDFUAVRef);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(FRHICommandList& RHICmdList, FTexture2DRHIParamRef OriginShorelineDFTextureRHI)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OriginShorelineDFTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, OriginShorelineDFTexture.GetBaseIndex(), OriginShorelineDFTextureRHI);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutShorelineDFUAV.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutShorelineDFUAV.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	/** The origin shoreline DistanceField texture. */
	FShaderResourceParameter OriginShorelineDFTexture;

	/** The output shoreline DistanceField texture. */
	FShaderResourceParameter OutShorelineDFUAV;
};

// Get Nearest Pixel to shoreline CS
class FGetNearestPixelToShorelineCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGetNearestPixelToShorelineCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FGetNearestPixelToShorelineCS()
	{
	}

	/** Initialization constructor. */
	explicit FGetNearestPixelToShorelineCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		OriginShorelineDFTexture.Bind(Initializer.ParameterMap, TEXT("OriginDistanceFieldTexture"));
		OutShorelineDFUAV.Bind(Initializer.ParameterMap, TEXT("OutputDistanceFieldTexture"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << OriginShorelineDFTexture;
		Ar << OutShorelineDFUAV;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutShorelineDFUAVRef)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutShorelineDFUAV.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutShorelineDFUAV.GetBaseIndex(), OutShorelineDFUAVRef);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture2DRHIParamRef OriginShorelineDFTextureRHI,
		FUniformBufferRHIParamRef WaveWorksShorelineDFUniformBuffer)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FWaveWorksShorelineDFUniformParameters>(), WaveWorksShorelineDFUniformBuffer);

		if (OriginShorelineDFTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, OriginShorelineDFTexture.GetBaseIndex(), OriginShorelineDFTextureRHI);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutShorelineDFUAV.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutShorelineDFUAV.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	/** The origin shoreline DistanceField texture. */
	FShaderResourceParameter OriginShorelineDFTexture;

	/** The output shoreline DistanceField texture. */
	FShaderResourceParameter OutShorelineDFUAV;
};

// Blur distance field texture CS
class FBlurShorelineDistanceFieldCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FBlurShorelineDistanceFieldCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FBlurShorelineDistanceFieldCS()
	{
	}

	/** Initialization constructor. */
	explicit FBlurShorelineDistanceFieldCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		OriginShorelineDFTexture.Bind(Initializer.ParameterMap, TEXT("OriginDistanceFieldTexture"));
		OutShorelineDFUAV.Bind(Initializer.ParameterMap, TEXT("OutputDistanceFieldTexture"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << OriginShorelineDFTexture;
		Ar << OutShorelineDFUAV;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutShorelineDFUAVRef)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutShorelineDFUAV.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutShorelineDFUAV.GetBaseIndex(), OutShorelineDFUAVRef);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture2DRHIParamRef OriginShorelineDFTextureRHI)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OriginShorelineDFTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, OriginShorelineDFTexture.GetBaseIndex(), OriginShorelineDFTextureRHI);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutShorelineDFUAV.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutShorelineDFUAV.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	/** The origin shoreline DistanceField texture. */
	FShaderResourceParameter OriginShorelineDFTexture;

	/** The output shoreline DistanceField texture. */
	FShaderResourceParameter OutShorelineDFUAV;
};

// Get Gradient to DF texture CS
class FGetGradientShorelineDistanceFieldCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGetGradientShorelineDistanceFieldCS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FGetGradientShorelineDistanceFieldCS()
	{
	}

	/** Initialization constructor. */
	explicit FGetGradientShorelineDistanceFieldCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		OriginShorelineDFTexture.Bind(Initializer.ParameterMap, TEXT("OriginDistanceFieldTexture"));
		OutShorelineDFUAV.Bind(Initializer.ParameterMap, TEXT("OutputDistanceFieldTexture"));
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << OriginShorelineDFTexture;
		Ar << OutShorelineDFUAV;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Set output buffers for this shader.
	*/
	void SetOutput(FRHICommandList& RHICmdList,
		FUnorderedAccessViewRHIParamRef OutShorelineDFUAVRef)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutShorelineDFUAV.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutShorelineDFUAV.GetBaseIndex(), OutShorelineDFUAVRef);
		}
	}

	/**
	* Set input parameters.
	*/
	void SetParameters(
		FRHICommandList& RHICmdList,
		FTexture2DRHIParamRef OriginShorelineDFTextureRHI)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OriginShorelineDFTexture.IsBound())
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, OriginShorelineDFTexture.GetBaseIndex(), OriginShorelineDFTextureRHI);
		}
	}

	/**
	* Unbinds any buffers that have been bound.
	*/
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();

		if (OutShorelineDFUAV.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutShorelineDFUAV.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:
	/** The origin shoreline DistanceField texture. */
	FShaderResourceParameter OriginShorelineDFTexture;

	/** The output shoreline DistanceField texture. */
	FShaderResourceParameter OutShorelineDFUAV;
};