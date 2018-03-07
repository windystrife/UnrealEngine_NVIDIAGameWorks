// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderParameters.h: Shader parameter inline definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "ShaderParameters.h"
#include "ShaderCore.h"
#include "Misc/App.h"

template<typename TBufferStruct> class TUniformBuffer;
template<typename TBufferStruct> class TUniformBufferRef;

/**
 * Sets the value of a  shader parameter.  Template'd on shader type
 * A template parameter specified the type of the parameter value.
 * NOTE: Shader should be the param ref type, NOT the param type, since Shader is passed by value. 
 * Otherwise AddRef/ReleaseRef will be called many times.
 */
template<typename ShaderRHIParamRef, class ParameterType, typename TRHICmdList>
void SetShaderValue(
	TRHICmdList& RHICmdList,
	ShaderRHIParamRef Shader,
	const FShaderParameter& Parameter,
	const ParameterType& Value,
	uint32 ElementIndex = 0
	)
{
	static_assert(!TIsPointer<ParameterType>::Value, "Passing by value is not valid.");

	const uint32 AlignedTypeSize = Align(sizeof(ParameterType),ShaderArrayElementAlignBytes);
	const int32 NumBytesToSet = FMath::Min<int32>(sizeof(ParameterType),Parameter.GetNumBytes() - ElementIndex * AlignedTypeSize);

	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if(NumBytesToSet > 0)
	{
		RHICmdList.SetShaderParameter(
			Shader,
			Parameter.GetBufferIndex(),
			Parameter.GetBaseIndex() + ElementIndex * AlignedTypeSize,
			(uint32)NumBytesToSet,
			&Value
			);
	}
}

template<typename ShaderRHIParamRef, class ParameterType>
void SetShaderValueOnContext(
	IRHICommandContext& RHICmdListContext,
	ShaderRHIParamRef Shader,
	const FShaderParameter& Parameter,
	const ParameterType& Value,
	uint32 ElementIndex = 0
	)
{
	static_assert(!TIsPointer<ParameterType>::Value, "Passing by value is not valid.");

	const uint32 AlignedTypeSize = Align(sizeof(ParameterType), ShaderArrayElementAlignBytes);
	const int32 NumBytesToSet = FMath::Min<int32>(sizeof(ParameterType), Parameter.GetNumBytes() - ElementIndex * AlignedTypeSize);

	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if (NumBytesToSet > 0)
	{
		RHICmdListContext.RHISetShaderParameter(
			Shader,
			Parameter.GetBufferIndex(),
			Parameter.GetBaseIndex() + ElementIndex * AlignedTypeSize,
			(uint32)NumBytesToSet,
			&Value
			);
	}
}



/** Specialization of the above for C++ bool type. */
template<typename ShaderRHIParamRef>
void SetShaderValue(
	FRHICommandList& RHICmdList, 
	ShaderRHIParamRef Shader,
	const FShaderParameter& Parameter,
	const bool& Value,
	uint32 ElementIndex = 0
	)
{
	const uint32 BoolValue = Value;
	SetShaderValue(RHICmdList, Shader, Parameter, BoolValue, ElementIndex);
}

/** Specialization of the above for C++ bool type. */
template<typename ShaderRHIParamRef>
void SetShaderValue(
	FRHIAsyncComputeCommandList& RHICmdList,
	ShaderRHIParamRef Shader,
	const FShaderParameter& Parameter,
	const bool& Value,
	uint32 ElementIndex = 0
	)
{
	const uint32 BoolValue = Value;
	SetShaderValue(RHICmdList, Shader, Parameter, BoolValue, ElementIndex);
}

/**
 * Sets the value of a shader parameter array.  Template'd on shader type
 * A template parameter specified the type of the parameter value.
 * NOTE: Shader should be the param ref type, NOT the param type, since Shader is passed by value. 
 * Otherwise AddRef/ReleaseRef will be called many times.
 */
template<typename ShaderRHIParamRef,class ParameterType, typename TRHICmdList>
void SetShaderValueArray(
	TRHICmdList& RHICmdList,
	ShaderRHIParamRef Shader,
	const FShaderParameter& Parameter,
	const ParameterType* Values,
	uint32 NumElements,
	uint32 BaseElementIndex = 0
	)
{
	const uint32 AlignedTypeSize = Align(sizeof(ParameterType),ShaderArrayElementAlignBytes);
	const int32 NumBytesToSet = FMath::Min<int32>(NumElements * AlignedTypeSize,Parameter.GetNumBytes() - BaseElementIndex * AlignedTypeSize);

	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if(NumBytesToSet > 0)
	{
		RHICmdList.SetShaderParameter(
			Shader,
			Parameter.GetBufferIndex(),
			Parameter.GetBaseIndex() + BaseElementIndex * AlignedTypeSize,
			(uint32)NumBytesToSet,
			Values
			);
	}
}

/** Specialization of the above for C++ bool type. */
template<typename ShaderRHIParamRef, typename TRHICmdList>
void SetShaderValueArray(
	TRHICmdList& RHICmdList,
	ShaderRHIParamRef Shader,
	const FShaderParameter& Parameter,
	const bool* Values,
	uint32 NumElements,
	uint32 BaseElementIndex = 0
	)
{
	UE_LOG(LogShaders, Fatal, TEXT("SetShaderValueArray does not support bool arrays."));
}

/**
 * Sets the value of a pixel shader bool parameter.
 */
inline void SetPixelShaderBool(
	FRHICommandList& RHICmdList, 
	FPixelShaderRHIParamRef PixelShader,
	const FShaderParameter& Parameter,
	bool Value
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if (Parameter.GetNumBytes() > 0)
	{
		// Convert to uint32 before passing to RHI
		uint32 BoolValue = Value;
		RHICmdList.SetShaderParameter(
			PixelShader,
			Parameter.GetBufferIndex(),
			Parameter.GetBaseIndex(),
			sizeof(BoolValue),
			&BoolValue
			);
	}
}

/**
 * Sets the value of a shader texture parameter.  Template'd on shader type
 */
template<typename ShaderTypeRHIParamRef, typename TRHICmdList>
FORCEINLINE void SetTextureParameter(
	TRHICmdList& RHICmdList,
	ShaderTypeRHIParamRef Shader,
	const FShaderResourceParameter& TextureParameter,
	const FShaderResourceParameter& SamplerParameter,
	const FTexture* Texture,
	uint32 ElementIndex = 0
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(TextureParameter.IsInitialized());
	checkSlow(SamplerParameter.IsInitialized());
	if(TextureParameter.IsBound())
	{
		Texture->LastRenderTime = FApp::GetCurrentTime();

		if (ElementIndex < TextureParameter.GetNumResources())
		{
			RHICmdList.SetShaderTexture( Shader, TextureParameter.GetBaseIndex() + ElementIndex, Texture->TextureRHI);
		}
	}
	
	// @todo ue4 samplerstate Should we maybe pass in two separate values? SamplerElement and TextureElement? Or never allow an array of samplers? Unsure best
	// if there is a matching sampler for this texture array index (ElementIndex), then set it. This will help with this case:
	//			Texture2D LightMapTextures[NUM_LIGHTMAP_COEFFICIENTS];
	//			SamplerState LightMapTexturesSampler;
	// In this case, we only set LightMapTexturesSampler when ElementIndex is 0, we don't set the sampler state for all 4 textures
	// This assumes that the all textures want to use the same sampler state
	if(SamplerParameter.IsBound())
	{
		if (ElementIndex < SamplerParameter.GetNumResources())
		{
			RHICmdList.SetShaderSampler( Shader, SamplerParameter.GetBaseIndex() + ElementIndex, Texture->SamplerStateRHI);
		}
	}
}

/**
 * Sets the value of a shader texture parameter. Template'd on shader type.
 */
template<typename ShaderTypeRHIParamRef, typename TRHICmdList>
FORCEINLINE void SetTextureParameter(
	TRHICmdList& RHICmdList,
	ShaderTypeRHIParamRef Shader,
	const FShaderResourceParameter& TextureParameter,
	const FShaderResourceParameter& SamplerParameter,
	FSamplerStateRHIParamRef SamplerStateRHI,
	FTextureRHIParamRef TextureRHI,
	uint32 ElementIndex = 0
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(TextureParameter.IsInitialized());
	checkSlow(SamplerParameter.IsInitialized());
	if(TextureParameter.IsBound())
	{
		if (ElementIndex < TextureParameter.GetNumResources())
		{
			RHICmdList.SetShaderTexture( Shader, TextureParameter.GetBaseIndex() + ElementIndex, TextureRHI);
		}
	}
	// @todo ue4 samplerstate Should we maybe pass in two separate values? SamplerElement and TextureElement? Or never allow an array of samplers? Unsure best
	// if there is a matching sampler for this texture array index (ElementIndex), then set it. This will help with this case:
	//			Texture2D LightMapTextures[NUM_LIGHTMAP_COEFFICIENTS];
	//			SamplerState LightMapTexturesSampler;
	// In this case, we only set LightMapTexturesSampler when ElementIndex is 0, we don't set the sampler state for all 4 textures
	// This assumes that the all textures want to use the same sampler state
	if(SamplerParameter.IsBound())
	{
		if (ElementIndex < SamplerParameter.GetNumResources())
		{
			RHICmdList.SetShaderSampler( Shader, SamplerParameter.GetBaseIndex() + ElementIndex, SamplerStateRHI);
		}
	}
}

/**
 * Sets the value of a shader surface parameter (e.g. to access MSAA samples).
 * Template'd on shader type (e.g. pixel shader or compute shader).
 */
template<typename ShaderTypeRHIParamRef, typename TRHICmdList>
FORCEINLINE void SetTextureParameter(
	TRHICmdList& RHICmdList,
	ShaderTypeRHIParamRef Shader,
	const FShaderResourceParameter& Parameter,
	FTextureRHIParamRef NewTextureRHI
	)
{
	if(Parameter.IsBound())
	{
		RHICmdList.SetShaderTexture(
			Shader,
			Parameter.GetBaseIndex(),
			NewTextureRHI
			);
	}
}

/**
 * Sets the value of a shader sampler parameter. Template'd on shader type.
 */
template<typename ShaderTypeRHIParamRef, typename TRHICmdList>
FORCEINLINE void SetSamplerParameter(
	TRHICmdList& RHICmdList,
	ShaderTypeRHIParamRef Shader,
	const FShaderResourceParameter& Parameter,
	FSamplerStateRHIParamRef SamplerStateRHI
	)
{
	if(Parameter.IsBound())
	{
		RHICmdList.SetShaderSampler(
			Shader,
			Parameter.GetBaseIndex(),
			SamplerStateRHI
			);
	}
}

/**
 * Sets the value of a shader resource view parameter
 * Template'd on shader type (e.g. pixel shader or compute shader).
 */
template<typename ShaderTypeRHIParamRef, typename TRHICmdList>
FORCEINLINE void SetSRVParameter(
	TRHICmdList& RHICmdList,
	ShaderTypeRHIParamRef Shader,
	const FShaderResourceParameter& Parameter,
	FShaderResourceViewRHIParamRef NewShaderResourceViewRHI
	)
{
	if(Parameter.IsBound())
	{
		RHICmdList.SetShaderResourceViewParameter(
			Shader,
			Parameter.GetBaseIndex(),
			NewShaderResourceViewRHI
			);
	}
}

/**
 * Sets the value of a unordered access view parameter
 */
template<typename TRHICmdList>
FORCEINLINE void SetUAVParameter(
TRHICmdList& RHICmdList,
	FComputeShaderRHIParamRef ComputeShader,
	const FShaderResourceParameter& Parameter,
	FUnorderedAccessViewRHIParamRef NewUnorderedAccessViewRHI
	)
{
	if(Parameter.IsBound())
	{
		RHICmdList.SetUAVParameter(
			ComputeShader,
			Parameter.GetBaseIndex(),
			NewUnorderedAccessViewRHI
			);
	}
}


template<typename TRHICmdList>
inline bool SetUAVParameterIfCS(TRHICmdList& RHICmdList, const FVertexShaderRHIParamRef Shader, const FShaderResourceParameter& UAVParameter, FUnorderedAccessViewRHIParamRef UAV)
{
	return false;
}

template<typename TRHICmdList>
inline bool SetUAVParameterIfCS(TRHICmdList& RHICmdList, const FPixelShaderRHIParamRef Shader, const FShaderResourceParameter& UAVParameter, FUnorderedAccessViewRHIParamRef UAV)
{
	return false;
}

template<typename TRHICmdList>
inline bool SetUAVParameterIfCS(TRHICmdList& RHICmdList, const FHullShaderRHIParamRef Shader, const FShaderResourceParameter& UAVParameter, FUnorderedAccessViewRHIParamRef UAV)
{
	return false;
}

template<typename TRHICmdList>
inline bool SetUAVParameterIfCS(TRHICmdList& RHICmdList, const FDomainShaderRHIParamRef Shader, const FShaderResourceParameter& UAVParameter, FUnorderedAccessViewRHIParamRef UAV)
{
	return false;
}

template<typename TRHICmdList>
inline bool SetUAVParameterIfCS(TRHICmdList& RHICmdList, const FGeometryShaderRHIParamRef Shader, const FShaderResourceParameter& UAVParameter, FUnorderedAccessViewRHIParamRef UAV)
{
	return false;
}

template<typename TRHICmdList>
inline bool SetUAVParameterIfCS(TRHICmdList& RHICmdList, const FComputeShaderRHIParamRef Shader, const FShaderResourceParameter& UAVParameter, FUnorderedAccessViewRHIParamRef UAV)
{
	SetUAVParameter(RHICmdList, Shader, UAVParameter, UAV);
	return UAVParameter.IsBound();
}

template<typename TShaderRHIRef, typename TRHICmdList>
inline void FRWShaderParameter::SetBuffer(TRHICmdList& RHICmdList, TShaderRHIRef Shader, const FRWBuffer& RWBuffer) const
{
	if (!SetUAVParameterIfCS(RHICmdList, Shader, UAVParameter, RWBuffer.UAV))
	{
		SetSRVParameter(RHICmdList, Shader, SRVParameter, RWBuffer.SRV);
	}
}

template<typename TShaderRHIRef, typename TRHICmdList>
inline void FRWShaderParameter::SetBuffer(TRHICmdList& RHICmdList, TShaderRHIRef Shader, const FRWBufferStructured& RWBuffer) const
{
	if (!SetUAVParameterIfCS(RHICmdList, Shader, UAVParameter, RWBuffer.UAV))
	{
		SetSRVParameter(RHICmdList, Shader, SRVParameter, RWBuffer.SRV);
	}
}

template<typename TShaderRHIRef, typename TRHICmdList>
inline void FRWShaderParameter::SetTexture(TRHICmdList& RHICmdList, TShaderRHIRef Shader, const FTextureRHIParamRef Texture, FUnorderedAccessViewRHIParamRef UAV) const
{
	if (!SetUAVParameterIfCS(RHICmdList, Shader, UAVParameter, UAV))
	{
		SetTextureParameter(RHICmdList, Shader, SRVParameter, Texture);
	}
}

template<typename TRHICmdList>
inline void FRWShaderParameter::UnsetUAV(TRHICmdList& RHICmdList, FComputeShaderRHIParamRef ComputeShader) const
{
	SetUAVParameter(RHICmdList, ComputeShader,UAVParameter,FUnorderedAccessViewRHIRef());
}


/** Sets the value of a shader uniform buffer parameter to a uniform buffer containing the struct. */
template<typename TShaderRHIRef>
inline void SetLocalUniformBufferParameter(
	FRHICommandList& RHICmdList,
	TShaderRHIRef Shader,
	const FShaderUniformBufferParameter& Parameter,
	const FLocalUniformBuffer& LocalUniformBuffer
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());
	if(Parameter.IsBound())
	{
		RHICmdList.SetLocalShaderUniformBuffer(Shader, Parameter.GetBaseIndex(), LocalUniformBuffer);
	}
}

/** Sets the value of a shader uniform buffer parameter to a uniform buffer containing the struct. */
template<typename TShaderRHIRef, typename TRHICmdList>
inline void SetUniformBufferParameter(
	TRHICmdList& RHICmdList,
	TShaderRHIRef Shader,
	const FShaderUniformBufferParameter& Parameter,
	FUniformBufferRHIParamRef UniformBufferRHI
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());
	if(Parameter.IsBound())
	{
		RHICmdList.SetShaderUniformBuffer( Shader, Parameter.GetBaseIndex(), UniformBufferRHI);
	}
}

/** Sets the value of a shader uniform buffer parameter to a uniform buffer containing the struct. */
template<typename TShaderRHIRef, typename TBufferStruct, typename TRHICmdList>
inline void SetUniformBufferParameter(
	TRHICmdList& RHICmdList,
	TShaderRHIRef Shader,
	const TShaderUniformBufferParameter<TBufferStruct>& Parameter,
	const TUniformBufferRef<TBufferStruct>& UniformBufferRef
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());
	if(Parameter.IsBound())
	{
		RHICmdList.SetShaderUniformBuffer( Shader, Parameter.GetBaseIndex(), UniformBufferRef);
	}
}

/** Sets the value of a shader uniform buffer parameter to a uniform buffer containing the struct. */
template<typename TShaderRHIRef, typename TBufferStruct, typename TRHICmdList>
inline void SetUniformBufferParameter(
	TRHICmdList& RHICmdList,
	TShaderRHIRef Shader,
	const TShaderUniformBufferParameter<TBufferStruct>& Parameter,
	const TUniformBuffer<TBufferStruct>& UniformBuffer
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());
	if (Parameter.IsBound())
	{
		RHICmdList.SetShaderUniformBuffer( Shader, Parameter.GetBaseIndex(), UniformBuffer.GetUniformBufferRHI());
	}
}

/** Sets the value of a shader uniform buffer parameter to a value of the struct. */
template<typename TShaderRHIRef,typename TBufferStruct>
inline void SetUniformBufferParameterImmediate(
	FRHICommandList& RHICmdList,
	TShaderRHIRef Shader,
	const TShaderUniformBufferParameter<TBufferStruct>& Parameter,
	const TBufferStruct& UniformBufferValue
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());
	if(Parameter.IsBound())
	{
		RHICmdList.SetShaderUniformBuffer(
			Shader,
			Parameter.GetBaseIndex(),
			RHICreateUniformBuffer(&UniformBufferValue,TBufferStruct::StaticStruct.GetLayout(),UniformBuffer_SingleDraw)
			);
	}
}

/** Sets the value of a shader uniform buffer parameter to a value of the struct. */
template<typename TShaderRHIRef,typename TBufferStruct, typename TRHICmdList>
inline void SetUniformBufferParameterImmediate(
	TRHICmdList& RHICmdList,
	TShaderRHIRef Shader,
	const TShaderUniformBufferParameter<TBufferStruct>& Parameter,
	const TBufferStruct& UniformBufferValue
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());
	if(Parameter.IsBound())
	{
		RHICmdList.SetShaderUniformBuffer(
			Shader,
			Parameter.GetBaseIndex(),
			RHICreateUniformBuffer(&UniformBufferValue,TBufferStruct::StaticStruct.GetLayout(),UniformBuffer_SingleDraw)
			);
	}
}
