// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanState.h: Vulkan state definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"

class FVulkanDevice;

class FVulkanSamplerState : public FRHISamplerState
{
public:
	FVulkanSamplerState(const FSamplerStateInitializerRHI& Initializer, FVulkanDevice& InDevice);
	~FVulkanSamplerState();

	VkSampler Sampler;
	FVulkanDevice& Device;

#if VULKAN_KEEP_CREATE_INFO
	VkSamplerCreateInfo SamplerInfo;
#endif
};

class FVulkanRasterizerState : public FRHIRasterizerState
{
public:
	FVulkanRasterizerState(const FRasterizerStateInitializerRHI& Initializer);

	static void ResetCreateInfo(VkPipelineRasterizationStateCreateInfo& OutInfo)
	{
		FMemory::Memzero(OutInfo);
		OutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		OutInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		OutInfo.lineWidth = 1.0f;
	}

	VkPipelineRasterizationStateCreateInfo RasterizerState;
};

class FVulkanDepthStencilState : public FRHIDepthStencilState
{
public:
	FVulkanDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer);

	VkPipelineDepthStencilStateCreateInfo DepthStencilState;
};

class FVulkanBlendState : public FRHIBlendState
{
public:
	FVulkanBlendState(const FBlendStateInitializerRHI& Initializer);

	// array the pipeline state can point right to
	VkPipelineColorBlendAttachmentState BlendStates[MaxSimultaneousRenderTargets];
};
