// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialShader.h: Shader base classes
=============================================================================*/

#pragma once

//#include "HAL/IConsoleManager.h"
//#include "RHI.h"
#include "ShaderParameters.h"
#include "SceneView.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "NiagaraShared.h"
#include "NiagaraShaderType.h"
#include "SceneRenderTargetParameters.h"



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
/*
class FDebugUniformExpressionSet
{
public:
	// The number of each type of expression contained in the set. 
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

	// Initialize from a uniform expression set. 
	void InitFromExpressionSet(const FUniformExpressionSet& InUniformExpressionSet)
	{
		NumVectorExpressions = InUniformExpressionSet.UniformVectorExpressions.Num();
		NumScalarExpressions = InUniformExpressionSet.UniformScalarExpressions.Num();
		Num2DTextureExpressions = InUniformExpressionSet.Uniform2DTextureExpressions.Num();
		NumCubeTextureExpressions = InUniformExpressionSet.UniformCubeTextureExpressions.Num();
		NumPerFrameScalarExpressions = InUniformExpressionSet.PerFrameUniformScalarExpressions.Num();
		NumPerFrameVectorExpressions = InUniformExpressionSet.PerFrameUniformVectorExpressions.Num();
	}

	// Returns true if the number of uniform expressions matches those with which the debug set was initialized.
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
*/

/** Serialization for debug uniform expression sets. */
/*
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
*/

/** Base class of all shaders that need material parameters. */
class RENDERER_API FNiagaraShader : public FShader
{
public:
	DECLARE_SHADER_TYPE(FNiagaraShader, Niagara);

	static FName UniformBufferLayoutName;

	FNiagaraShader() //: DebugUniformExpressionUBLayout(FRHIUniformBufferLayout::Zero)
	{
	}

	static bool ShouldCache(EShaderPlatform Platform, const FNiagaraScript* Script)
	{
		//@todo - lit materials only 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}


	FNiagaraShader(const FNiagaraShaderType::CompiledShaderInitializerType& Initializer);

	typedef void (*ModifyCompilationEnvironmentType)(EShaderPlatform, const FNiagaraScript*, FShaderCompilerEnvironment&);

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FNiagaraScript* Script, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	void SetDatainterfaceBufferDescriptors(const TArray< TArray<DIGPUBufferParamDescriptor>> &InBufferDescriptors)
	{
		DIBufferDescriptors = InBufferDescriptors;
	}

//	FUniformBufferRHIParamRef GetParameterCollectionBuffer(const FGuid& Id, const FSceneInterface* SceneInterface) const;
	/*
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
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	// Bind parameters
	void BindParams(const FShaderParameterMap &ParameterMap)
	{
		FloatInputBufferParam.Bind(ParameterMap, TEXT("InputFloat"));
		IntInputBufferParam.Bind(ParameterMap, TEXT("InputInt"));
		FloatOutputBufferParam.Bind(ParameterMap, TEXT("OutputFloat"));
		IntOutputBufferParam.Bind(ParameterMap, TEXT("OutputInt"));
		OutputIndexBufferParam.Bind(ParameterMap, TEXT("DataSetIndices"));
		EmitterTickCounterParam.Bind(ParameterMap, TEXT("EmitterTickCounter"));
		NumInstancesPerThreadParam.Bind(ParameterMap, TEXT("NumInstancesPerThread"));		
		NumEventsPerParticleParam.Bind(ParameterMap, TEXT("NumEventsPerParticle"));
		NumParticlesPerEventParam.Bind(ParameterMap, TEXT("NumParticlesPerEvent"));
		CopyInstancesBeforeStartParam.Bind(ParameterMap, TEXT("CopyInstancesBeforeStart"));
		NumInstancesParam.Bind(ParameterMap, TEXT("NumInstances"));
		StartInstanceParam.Bind(ParameterMap, TEXT("StartInstance"));
		SimulateStartInstanceParam.Bind(ParameterMap, TEXT("SimulateStartInstance"));
		GroupStartInstanceParam.Bind(ParameterMap, TEXT("GroupStartInstance"));
		ComponentBufferSizeReadParam.Bind(ParameterMap, TEXT("ComponentBufferSizeRead"));
		ComponentBufferSizeWriteParam.Bind(ParameterMap, TEXT("ComponentBufferSizeWrite"));
		NumThreadGroupsParam.Bind(ParameterMap, TEXT("NumThreadGroups"));
		EmitterConstantBufferParam.Bind(ParameterMap, TEXT("FEmitterParameters"));

		// params for event buffers
		// this is horrendous; need to do this in a uniform buffer instead.
		//
		for (uint32 i = 0; i < MAX_CONCURRENT_EVENT_DATASETS; i++)
		{
			FString VarName = TEXT("WriteDataSetFloat") + FString::FromInt(i+1);
			EventFloatUAVParams[i].Bind(ParameterMap, *VarName);
			VarName = TEXT("WriteDataSetInt") + FString::FromInt(i+1);
			EventIntUAVParams[i].Bind(ParameterMap, *VarName);

			VarName = TEXT("ReadDataSetFloat") + FString::FromInt(i+1);
			EventFloatSRVParams[i].Bind(ParameterMap, *VarName);
			VarName = TEXT("ReadDataSetInt") + FString::FromInt(i+1);
			EventIntSRVParams[i].Bind(ParameterMap, *VarName);

			VarName = TEXT("DSComponentBufferSizeReadFloat") + FString::FromInt(i + 1);
			EventReadFloatStrideParams[i].Bind(ParameterMap, *VarName);
			VarName = TEXT("DSComponentBufferSizeWriteFloat") + FString::FromInt(i + 1);
			EventWriteFloatStrideParams[i].Bind(ParameterMap, *VarName);
			VarName = TEXT("DSComponentBufferSizeReadInt") + FString::FromInt(i + 1);
			EventReadIntStrideParams[i].Bind(ParameterMap, *VarName);
			VarName = TEXT("DSComponentBufferSizeWriteInt") + FString::FromInt(i + 1);
			EventWriteIntStrideParams[i].Bind(ParameterMap, *VarName);
		}

		// params for data interface buffers
		BuildDIBufferParamMap(ParameterMap);

		ensure(FloatOutputBufferParam.IsBound() || IntOutputBufferParam.IsBound());	// we should have at least one output buffer we're writing to
		ensure(OutputIndexBufferParam.IsBound());		
		ensure(NumInstancesPerThreadParam.IsBound());	
		ensure(ComponentBufferSizeWriteParam.IsBound());
		ensure(NumInstancesPerThreadParam.IsBound());
		ensure(StartInstanceParam.IsBound());
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override;
	virtual uint32 GetAllocatedSize() const override;
	
	FShaderResourceParameter FloatInputBufferParam;
	FShaderResourceParameter IntInputBufferParam;
	FRWShaderParameter FloatOutputBufferParam;
	FRWShaderParameter IntOutputBufferParam;
	FRWShaderParameter OutputIndexBufferParam;
	FShaderUniformBufferParameter EmitterConstantBufferParam;
	FShaderUniformBufferParameter DataInterfaceUniformBufferParam;
	FShaderParameter EmitterTickCounterParam;
	FShaderParameter NumInstancesPerThreadParam;
	FShaderParameter NumInstancesParam;
	FShaderParameter NumEventsPerParticleParam;
	FShaderParameter NumParticlesPerEventParam;
	FShaderParameter CopyInstancesBeforeStartParam;
	FShaderParameter StartInstanceParam;
	FShaderParameter SimulateStartInstanceParam;
	FShaderParameter GroupStartInstanceParam;
	FShaderParameter NumThreadGroupsParam;
	FShaderParameter ComponentBufferSizeReadParam;
	FShaderParameter ComponentBufferSizeWriteParam;
	FRWShaderParameter EventIntUAVParams[MAX_CONCURRENT_EVENT_DATASETS];
	FRWShaderParameter EventFloatUAVParams[MAX_CONCURRENT_EVENT_DATASETS];
	FShaderResourceParameter EventIntSRVParams[MAX_CONCURRENT_EVENT_DATASETS];
	FShaderResourceParameter EventFloatSRVParams[MAX_CONCURRENT_EVENT_DATASETS];
	FShaderParameter EventWriteFloatStrideParams[MAX_CONCURRENT_EVENT_DATASETS];
	FShaderParameter EventWriteIntStrideParams[MAX_CONCURRENT_EVENT_DATASETS];
	FShaderParameter EventReadFloatStrideParams[MAX_CONCURRENT_EVENT_DATASETS];
	FShaderParameter EventReadIntStrideParams[MAX_CONCURRENT_EVENT_DATASETS];

	FShaderResourceParameter *FindDIBufferParam(int32 DataInterfaceIndex, FName InName)
	{
		if (NameToDIBufferParamMap.Num() > DataInterfaceIndex)
		{
			return NameToDIBufferParamMap[DataInterfaceIndex].Find(InName);
		}

		return nullptr;
	}

	void BuildDIBufferParamMap(const FShaderParameterMap &ParameterMap)
	{
		for (TArray<DIGPUBufferParamDescriptor> &InterfaceDescs : DIBufferDescriptors)
		{
			int32 Idx = NameToDIBufferParamMap.AddDefaulted(1);
			for (DIGPUBufferParamDescriptor &Desc : InterfaceDescs)
			{
				FShaderResourceParameter &Param = NameToDIBufferParamMap[Idx].Add(*Desc.BufferParamName);
				Param.Bind(ParameterMap, *Desc.BufferParamName);
				ensure(Param.IsBound());
			}
		}
	}

private:
	FShaderUniformBufferParameter NiagaraUniformBuffer;

	// buffer descriptors for data interfaces holding names and params for binding
	TArray< TArray<DIGPUBufferParamDescriptor> > DIBufferDescriptors;

	// buffer descriptors for event data sets holding names and params for binding
	TArray< TArray<DIGPUBufferParamDescriptor> > EventBufferDescriptors;

	// one map per data interface, mapping buffer names to their params
	TArray< TMap<FName, FShaderResourceParameter> > NameToDIBufferParamMap;

	/*
	FDebugUniformExpressionSet	DebugUniformExpressionSet;
	FRHIUniformBufferLayout		DebugUniformExpressionUBLayout;
	*/
	FString						DebugDescription;

	/* OPTODO: ? */
	/*
	// If true, cached uniform expressions are allowed.
	static int32 bAllowCachedUniformExpressions;
	// Console variable ref to toggle cached uniform expressions.
	static FAutoConsoleVariableRef CVarAllowCachedUniformExpressions;
	*/


	/*
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING || !WITH_EDITOR)
	void VerifyExpressionAndShaderMaps(const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial& Material, const FUniformExpressionCache* UniformExpressionCache);
#endif
	*/
};


class FNiagaraEmitterInstanceShader : public FNiagaraShader
{

};
