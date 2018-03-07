// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLStructuredBuffer.cpp: OpenGL Index buffer RHI implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Containers/ResourceArray.h"
#include "OpenGLDrv.h"

FStructuredBufferRHIRef FOpenGLDynamicRHI::RHICreateStructuredBuffer(uint32 Stride,uint32 Size,uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	VERIFY_GL_SCOPE();

	const void *Data = NULL;

	// If a resource array was provided for the resource, create the resource pre-populated
	if(CreateInfo.ResourceArray)
	{
		check(Size == CreateInfo.ResourceArray->GetResourceDataSize());
		Data = CreateInfo.ResourceArray->GetResourceData();
	}

	TRefCountPtr<FOpenGLStructuredBuffer> StructuredBuffer = new FOpenGLStructuredBuffer(Stride, Size, InUsage & BUF_AnyDynamic, Data);
	return StructuredBuffer.GetReference();
}

void* FOpenGLDynamicRHI::RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI,uint32 Offset,uint32 Size,EResourceLockMode LockMode)
{
	VERIFY_GL_SCOPE();
	FOpenGLStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);
	return StructuredBuffer->Lock(Offset, Size, LockMode == RLM_ReadOnly, StructuredBuffer->IsDynamic());
}

void FOpenGLDynamicRHI::RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	VERIFY_GL_SCOPE();
	FOpenGLStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);
	StructuredBuffer->Unlock();
}
