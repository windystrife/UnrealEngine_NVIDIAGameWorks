// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLIndexBuffer.cpp: OpenGL Index buffer RHI implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Containers/ResourceArray.h"
#include "OpenGLDrv.h"

FIndexBufferRHIRef FOpenGLDynamicRHI::RHICreateIndexBuffer(uint32 Stride,uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	VERIFY_GL_SCOPE();

	const void *Data = NULL;

	// If a resource array was provided for the resource, create the resource pre-populated
	if(CreateInfo.ResourceArray)
	{
		check(Size == CreateInfo.ResourceArray->GetResourceDataSize());
		Data = CreateInfo.ResourceArray->GetResourceData();
	}

	TRefCountPtr<FOpenGLIndexBuffer> IndexBuffer = new FOpenGLIndexBuffer(Stride, Size, InUsage, Data);
	return IndexBuffer.GetReference();
}

void* FOpenGLDynamicRHI::RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI,uint32 Offset,uint32 Size,EResourceLockMode LockMode)
{
	VERIFY_GL_SCOPE();
	FOpenGLIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	return IndexBuffer->Lock(Offset, Size, LockMode == RLM_ReadOnly, IndexBuffer->IsDynamic());
}

void FOpenGLDynamicRHI::RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI)
{
	VERIFY_GL_SCOPE();
	FOpenGLIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	IndexBuffer->Unlock();
}
