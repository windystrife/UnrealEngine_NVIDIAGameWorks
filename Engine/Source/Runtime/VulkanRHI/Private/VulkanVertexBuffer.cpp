// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanVertexBuffer.cpp: Vulkan vertex buffer RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"


FVulkanVertexBuffer::FVulkanVertexBuffer(FVulkanDevice* InDevice, uint32 InSize, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
	: FRHIVertexBuffer(InSize, InUsage)
	, FVulkanResourceMultiBuffer(InDevice, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, InSize, InUsage, CreateInfo)
{
}

FVertexBufferRHIRef FVulkanDynamicRHI::RHICreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	// make the RHI object, which will allocate memory
	FVulkanVertexBuffer* VertexBuffer = new FVulkanVertexBuffer(Device, Size, InUsage, CreateInfo);
	return VertexBuffer;
}

void* FVulkanDynamicRHI::RHILockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	FVulkanVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	return VertexBuffer->Lock(false, LockMode, Size, Offset);
}
#if 0
void* FVulkanDynamicRHI::LockVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	FVulkanVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	return VertexBuffer->Lock(true, LockMode, SizeRHI, Offset);
}
#endif
void FVulkanDynamicRHI::RHIUnlockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI)
{
	FVulkanVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	VertexBuffer->Unlock(false);
}
#if 0
void FVulkanDynamicRHI::UnlockVertexBuffer_RenderThread(FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBufferRHI)
{
	FVulkanVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	VertexBuffer->Unlock(true);
}
#endif
void FVulkanDynamicRHI::RHICopyVertexBuffer(FVertexBufferRHIParamRef SourceBufferRHI,FVertexBufferRHIParamRef DestBufferRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}
