// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanGlobalUniformBuffer.h: Vulkan Global uniform definitions.
=============================================================================*/

#pragma once

#include "VulkanResources.h"

struct FVulkanPooledUniformBuffer : public FRefCountedObject
{
	FVulkanPooledUniformBuffer(FVulkanDevice& InDevice, uint32 InSize);
	~FVulkanPooledUniformBuffer();

	FVulkanBuffer Buffer;
};

typedef TRefCountPtr<FVulkanPooledUniformBuffer> FPooledUniformBufferRef;

class FVulkanGlobalUniformPool
{
public:
	FVulkanGlobalUniformPool();
	~FVulkanGlobalUniformPool();

	void BeginFrame();
	FPooledUniformBufferRef& GetGlobalUniformBufferFromPool(FVulkanDevice& InDevice, uint32 InSize);

private:
	FORCEINLINE uint32 GetPoolBucketIndex(uint32 NumBytes) const
	{
		uint32 Index = FMath::CeilLogTwo(NumBytes);
		check(Index < NumPoolBuckets);
		return Index;
	}

	enum
	{
		NumPoolBuckets	= 17,
		NumFrames		= 4,	// Should be at least the same as the number of command-buffers we run
	};

	TArray<FPooledUniformBufferRef> GlobalUniformBufferPool[NumPoolBuckets];
	TArray<FPooledUniformBufferRef> UsedGlobalUniformBuffers[NumPoolBuckets * NumFrames];
};
