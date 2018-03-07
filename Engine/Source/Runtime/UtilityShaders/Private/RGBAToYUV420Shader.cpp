// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "RGBAToYUV420Shader.h"
#include "ShaderParameterUtils.h"

#if defined(HAS_MORPHEUS) && HAS_MORPHEUS

/* FRGBAToYUV420CS shader
 *****************************************************************************/

IMPLEMENT_SHADER_TYPE(, FRGBAToYUV420CS, TEXT("/Engine/Private/RGBAToYUV420.usf"), TEXT("RGBAToYUV420Main"), SF_Compute);


void FRGBAToYUV420CS::SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> InSrcTexture, FUnorderedAccessViewRHIParamRef InOutUAV, float InTargetHeight, float InScaleFactorX, float InScaleFactorY, float InTextureYOffset)
{
	FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
	SetShaderValue(RHICmdList, ComputeShaderRHI, TargetHeight, InTargetHeight);
	SetShaderValue(RHICmdList, ComputeShaderRHI, ScaleFactorX, InScaleFactorX);
	SetShaderValue(RHICmdList, ComputeShaderRHI, ScaleFactorY, InScaleFactorY);
	SetShaderValue(RHICmdList, ComputeShaderRHI, TextureYOffset, InTextureYOffset);
	SetTextureParameter(RHICmdList, ComputeShaderRHI, SrcTexture, InSrcTexture);
	RHICmdList.SetUAVParameter(ComputeShaderRHI, OutTextureRW.GetBaseIndex(), InOutUAV);
}

/**
* Unbinds any buffers that have been bound.
*/
void FRGBAToYUV420CS::UnbindBuffers(FRHICommandList& RHICmdList)
{
	FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
	RHICmdList.SetUAVParameter(ComputeShaderRHI, OutTextureRW.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
}

#endif //defined(HAS_MORPHEUS) && HAS_MORPHEUS
