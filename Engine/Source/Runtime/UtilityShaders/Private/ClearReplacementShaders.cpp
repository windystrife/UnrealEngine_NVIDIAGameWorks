// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "ClearReplacementShaders.h"
#include "ShaderParameterUtils.h"
#include "RendererInterface.h"

template< typename T >
void FClearTexture2DReplacementCS<T>::SetParameters( FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW, const T(&Values)[4] )
{
	FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
	SetShaderValue(RHICmdList, ComputeShaderRHI, ClearColor, Values);

	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, TextureRW);
	RHICmdList.SetUAVParameter(ComputeShaderRHI, ClearTextureRW.GetBaseIndex(), TextureRW);
}

template< typename T >
void FClearTexture2DReplacementCS<T>::FinalizeParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW)
{
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, TextureRW);
	FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
	SetUAVParameter(RHICmdList, ComputeShaderRHI, ClearTextureRW, FUnorderedAccessViewRHIRef());
}

template< typename T >
void FClearTexture2DArrayReplacementCS<T>::SetParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW, const T(&Values)[4])
{
	FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
	SetShaderValue(RHICmdList, ComputeShaderRHI, ClearColor, Values);

	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, TextureRW);
	RHICmdList.SetUAVParameter(ComputeShaderRHI, ClearTextureArrayRW.GetBaseIndex(), TextureRW);
}

template< typename T >
void FClearTexture2DArrayReplacementCS<T>::FinalizeParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW)
{
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, TextureRW);
	FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
	SetUAVParameter(RHICmdList, ComputeShaderRHI, ClearTextureArrayRW, FUnorderedAccessViewRHIRef());
}

template< typename T >
void FClearVolumeReplacementCS<T>::SetParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW, const T(&Values)[4])
{
	FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
	SetShaderValue(RHICmdList, ComputeShaderRHI, ClearColor, Values);

	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, TextureRW);
	RHICmdList.SetUAVParameter(ComputeShaderRHI, ClearVolumeRW.GetBaseIndex(), TextureRW);
}

template< typename T >
void FClearVolumeReplacementCS<T>::FinalizeParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW)
{
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, TextureRW);
	FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
	SetUAVParameter(RHICmdList, ComputeShaderRHI, ClearVolumeRW, FUnorderedAccessViewRHIRef());
}

void FClearTexture2DReplacementScissorCS::SetParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW, FLinearColor InClearColor, const FVector4& InTargetBounds)
{
	FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
	SetShaderValue(RHICmdList, ComputeShaderRHI, ClearColor, InClearColor);
	SetShaderValue(RHICmdList, ComputeShaderRHI, TargetBounds, InTargetBounds);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, TextureRW);
	RHICmdList.SetUAVParameter(ComputeShaderRHI, ClearTextureRW.GetBaseIndex(), TextureRW);
}

void FClearTexture2DReplacementScissorCS::FinalizeParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW)
{
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, TextureRW);
	FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
	SetUAVParameter(RHICmdList, ComputeShaderRHI, ClearTextureRW, FUnorderedAccessViewRHIRef());
}

void FClearBufferReplacementCS::SetParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef BufferRW, uint32 NumDWordsToClear,uint32 ClearValue)
{
	FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
	SetShaderValue(RHICmdList, ComputeShaderRHI, ClearBufferCSParams, FUintVector4(ClearValue, NumDWordsToClear, 0, 0));
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, BufferRW);
	RHICmdList.SetUAVParameter(ComputeShaderRHI, ClearBufferRW.GetBaseIndex(), BufferRW);
}

void FClearBufferReplacementCS::FinalizeParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef BufferRW)
{
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, BufferRW);
	FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
	SetUAVParameter(RHICmdList, ComputeShaderRHI, ClearBufferRW, FUnorderedAccessViewRHIRef());
}

template class FClearTexture2DReplacementCS<float>;
template class FClearTexture2DReplacementCS<uint32>;
template class FClearTexture2DArrayReplacementCS<float>;
template class FClearTexture2DArrayReplacementCS<uint32>;
template class FClearVolumeReplacementCS<float>;
template class FClearVolumeReplacementCS<uint32>;

IMPLEMENT_SHADER_TYPE(, FClearReplacementVS, TEXT("/Engine/Private/ClearReplacementShaders.usf"), TEXT("ClearVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FClearReplacementPS, TEXT("/Engine/Private/ClearReplacementShaders.usf"), TEXT("ClearPS"), SF_Pixel);

IMPLEMENT_SHADER_TYPE(template<>, FClearTexture2DReplacementCS<float>, TEXT("/Engine/Private/ClearReplacementShaders.usf"), TEXT("ClearTexture2DCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FClearTexture2DReplacementCS<uint32>, TEXT("/Engine/Private/ClearReplacementShaders.usf"), TEXT("ClearTexture2DCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FClearTexture2DArrayReplacementCS<float>, TEXT("/Engine/Private/ClearReplacementShaders.usf"), TEXT("ClearTexture2DArrayCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FClearTexture2DArrayReplacementCS<uint32>, TEXT("/Engine/Private/ClearReplacementShaders.usf"), TEXT("ClearTexture2DArrayCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FClearVolumeReplacementCS<float>, TEXT("/Engine/Private/ClearReplacementShaders.usf"), TEXT("ClearVolumeCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FClearVolumeReplacementCS<uint32>, TEXT("/Engine/Private/ClearReplacementShaders.usf"), TEXT("ClearVolumeCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FClearTexture2DReplacementScissorCS, TEXT("/Engine/Private/ClearReplacementShaders.usf"), TEXT("ClearTexture2DScissorCS"), SF_Compute);

IMPLEMENT_SHADER_TYPE(, FClearBufferReplacementCS, TEXT("/Engine/Private/ClearReplacementShaders.usf"), TEXT("ClearBufferCS"), SF_Compute);