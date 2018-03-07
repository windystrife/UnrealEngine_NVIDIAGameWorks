// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "VulkanRHIPrivate.h"
#include "Containers/ResourceArray.h"

FVulkanStructuredBuffer::FVulkanStructuredBuffer(FVulkanDevice* InDevice, uint32 InStride, uint32 InSize, FRHIResourceCreateInfo& CreateInfo, uint32 InUsage)
	: FRHIStructuredBuffer(InStride, InSize, InUsage)
	, FVulkanResourceMultiBuffer(InDevice, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, InSize, InUsage, CreateInfo)
{
}

FVulkanStructuredBuffer::~FVulkanStructuredBuffer()
{
}


FStructuredBufferRHIRef FVulkanDynamicRHI::RHICreateStructuredBuffer(uint32 InStride, uint32 InSize, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	return new FVulkanStructuredBuffer(Device, InStride, InSize, CreateInfo, InUsage);
}

void* FVulkanDynamicRHI::RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI,uint32 Offset,uint32 Size,EResourceLockMode LockMode)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
	FVulkanStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	return nullptr;
}

void FVulkanDynamicRHI::RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}
