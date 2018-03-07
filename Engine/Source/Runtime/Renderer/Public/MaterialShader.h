// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialShader.h: Shader base classes
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "ShaderParameters.h"
#include "SceneView.h"
#include "Shader.h"
#include "MaterialShared.h"
#include "GlobalShader.h"
#include "MaterialShaderType.h"
#include "SceneRenderTargetParameters.h"

// WaveWorks Start
#include "WaveWorksShaderParameters.h"
// WaveWorks End
template<typename TBufferStruct> class TUniformBufferRef;

template<typename ParameterType> 
struct TUniformParameter
{
	int32 Index;
	ParameterType ShaderParameter;
	friend FArchive& operator<<(FArchive& Ar,TUniformParameter<ParameterType>& P)
	{
		return Ar << P.Index << P.ShaderParameter;
	}
};


/**
 * Debug information related to uniform expression sets.
 */
class FDebugUniformExpressionSet
{
public:
	/** The number of each type of expression contained in the set. */
	int32 NumVectorExpressions;
	int32 NumScalarExpressions;
	int32 Num2DTextureExpressions;
	int32 NumCubeTextureExpressions;
	int32 NumPerFrameScalarExpressions;
	int32 NumPerFrameVectorExpressions;

	FDebugUniformExpressionSet()
		: NumVectorExpressions(0)
		, NumScalarExpressions(0)
		, Num2DTextureExpressions(0)
		, NumCubeTextureExpressions(0)
		, NumPerFrameScalarExpressions(0)
		, NumPerFrameVectorExpressions(0)
	{
	}

	explicit FDebugUniformExpressionSet(const FUniformExpressionSet& InUniformExpressionSet)
	{
		InitFromExpressionSet(InUniformExpressionSet);
	}

	/** Initialize from a uniform expression set. */
	void InitFromExpressionSet(const FUniformExpressionSet& InUniformExpressionSet)
	{
		NumVectorExpressions = InUniformExpressionSet.UniformVectorExpressions.Num();
		NumScalarExpressions = InUniformExpressionSet.UniformScalarExpressions.Num();
		Num2DTextureExpressions = InUniformExpressionSet.Uniform2DTextureExpressions.Num();
		NumCubeTextureExpressions = InUniformExpressionSet.UniformCubeTextureExpressions.Num();
		NumPerFrameScalarExpressions = InUniformExpressionSet.PerFrameUniformScalarExpressions.Num();
		NumPerFrameVectorExpressions = InUniformExpressionSet.PerFrameUniformVectorExpressions.Num();
	}

	/** Returns true if the number of uniform expressions matches those with which the debug set was initialized. */
	bool Matches(const FUniformExpressionSet& InUniformExpressionSet) const
	{
		return NumVectorExpressions == InUniformExpressionSet.UniformVectorExpressions.Num()
			&& NumScalarExpressions == InUniformExpressionSet.UniformScalarExpressions.Num()
			&& Num2DTextureExpressions == InUniformExpressionSet.Uniform2DTextureExpressions.Num()
			&& NumCubeTextureExpressions == InUniformExpressionSet.UniformCubeTextureExpressions.Num()
			&& NumPerFrameScalarExpressions == InUniformExpressionSet.PerFrameUniformScalarExpressions.Num()
			&& NumPerFrameVectorExpressions == InUniformExpressionSet.PerFrameUniformVectorExpressions.Num();
	}
};

/** Serialization for debug uniform expression sets. */
inline FArchive& operator<<(FArchive& Ar, FDebugUniformExpressionSet& DebugExpressionSet)
{
	Ar << DebugExpressionSet.NumVectorExpressions;
	Ar << DebugExpressionSet.NumScalarExpressions;
	Ar << DebugExpressionSet.NumPerFrameScalarExpressions;
	Ar << DebugExpressionSet.NumPerFrameVectorExpressions;
	Ar << DebugExpressionSet.Num2DTextureExpressions;
	Ar << DebugExpressionSet.NumCubeTextureExpressions;
	return Ar;
}


/** Base class of all shaders that need material parameters. */
class RENDERER_API FMaterialShader : public FShader
{
public:
	static FName UniformBufferLayoutName;

	FMaterialShader() : DebugUniformExpressionUBLayout(FRHIUniformBufferLayout::Zero)
	{
	}

	FMaterialShader(const FMaterialShaderType::CompiledShaderInitializerType& Initializer);

	typedef void (*ModifyCompilationEnvironmentType)(EShaderPlatform, const FMaterial*, FShaderCompilerEnvironment&);

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	FUniformBufferRHIParamRef GetParameterCollectionBuffer(const FGuid& Id, const FSceneInterface* SceneInterface) const;

	template<typename ShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void SetViewParameters(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FSceneView& View, const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer)
	{
		const auto& ViewUniformBufferParameter = GetUniformBufferParameter<FViewUniformShaderParameters>();
		const auto& BuiltinSamplersUBParameter = GetUniformBufferParameter<FBuiltinSamplersParameters>();
		CheckShaderIsValid();
		SetUniformBufferParameter(RHICmdList, ShaderRHI, ViewUniformBufferParameter, ViewUniformBuffer);
#if USE_GBuiltinSamplersUniformBuffer
		SetUniformBufferParameter(RHICmdList, ShaderRHI, BuiltinSamplersUBParameter, GBuiltinSamplersUniformBuffer.GetUniformBufferRHI());
#endif

		if (View.bShouldBindInstancedViewUB && View.Family->Views.Num() > 0)
		{
			// When drawing the left eye in a stereo scene, copy the right eye view values into the instanced view uniform buffer.
			const EStereoscopicPass StereoPassIndex = (View.StereoPass != eSSP_FULL) ? eSSP_RIGHT_EYE : eSSP_FULL;

			const FSceneView& InstancedView = View.Family->GetStereoEyeView(StereoPassIndex);
			const auto& InstancedViewUniformBufferParameter = GetUniformBufferParameter<FInstancedViewUniformShaderParameters>();
			SetUniformBufferParameter(RHICmdList, ShaderRHI, InstancedViewUniformBufferParameter, InstancedView.ViewUniformBuffer);
		}
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	/** Sets pixel parameters that are material specific but not FMeshBatch specific. */
	template< typename ShaderRHIParamRef >
	void SetParameters(
		FRHICommandList& RHICmdList,
		const ShaderRHIParamRef ShaderRHI, 
		const FMaterialRenderProxy* MaterialRenderProxy, 
		const FMaterial& Material,
		const FSceneView& View, 
		const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
		bool bDeferredPass, 
		ESceneRenderTargetsMode::Type TextureMode);

// WaveWorks Start
	/** Sets pixel parameters that are material specific but not FMeshBatch specific. */
	template< typename ShaderRHIParamRef >
	void SetWaveWorksParameters(
		FRHICommandList& RHICmdList,
		const ShaderRHIParamRef ShaderRHI,
		const FSceneView& View,
		class FWaveWorksResource* WaveWorksResource
		);
// WaveWorks End

	FTextureRHIRef& GetEyeAdaptation(FRHICommandList& RHICmdList, const FSceneView& View);

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override;
	virtual uint32 GetAllocatedSize() const override;

	// WaveWorks Start
	FWaveWorksShaderParameters* GetWaveWorksShaderParameters() { return &WaveWorksParameters; }
	// WaveWorks End
private:

	FShaderUniformBufferParameter MaterialUniformBuffer;
	TArray<FShaderUniformBufferParameter> ParameterCollectionUniformBuffers;
	TArray<FShaderParameter> PerFrameScalarExpressions;
	TArray<FShaderParameter> PerFrameVectorExpressions;
	TArray<FShaderParameter> PerFramePrevScalarExpressions;
	TArray<FShaderParameter> PerFramePrevVectorExpressions;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter SceneColorCopyTexture;
	FShaderResourceParameter SceneColorCopyTextureSampler;

	//Use of the eye adaptation texture here is experimental and potentially dangerous as it can introduce a feedback loop. May be removed.
	FShaderResourceParameter EyeAdaptation;

	// WaveWorks Start
	FWaveWorksShaderParameters WaveWorksParameters;
	// WaveWorks End

	FDebugUniformExpressionSet	DebugUniformExpressionSet;
	FRHIUniformBufferLayout		DebugUniformExpressionUBLayout;
	FString						DebugDescription;

	/** If true, cached uniform expressions are allowed. */
	static int32 bAllowCachedUniformExpressions;
	/** Console variable ref to toggle cached uniform expressions. */
	static FAutoConsoleVariableRef CVarAllowCachedUniformExpressions;

#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING || !WITH_EDITOR)
	void VerifyExpressionAndShaderMaps(const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial& Material, const FUniformExpressionCache* UniformExpressionCache);
#endif
};
