// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanSwapChain.h: Vulkan viewport RHI definitions.
=============================================================================*/

#pragma once

#define USE_IMAGE_ACQUIRE_FENCES	!PLATFORM_ANDROID

namespace VulkanRHI
{
	class FFence;
}

class FVulkanQueue;

class FVulkanSwapChain
{
public:
	FVulkanSwapChain(VkInstance InInstance, FVulkanDevice& InDevice, void* WindowHandle, EPixelFormat& InOutPixelFormat, uint32 Width, uint32 Height,
		uint32* InOutDesiredNumBackBuffers, TArray<VkImage>& OutImages);

	void Destroy();

	// Has to be negative as we use this also on other callbacks as the acquired image index
	enum class EStatus
	{
		Healthy = 0,
		OutOfDate = -1,
		SurfaceLost = -2,
	};
	EStatus Present(FVulkanQueue* GfxQueue, FVulkanQueue* PresentQueue, FVulkanSemaphore* BackBufferRenderingDoneSemaphore);

protected:
	VkSwapchainKHR SwapChain;
	FVulkanDevice& Device;

	VkSurfaceKHR Surface;

	int32 CurrentImageIndex;
	int32 SemaphoreIndex;
	uint32 NumPresentCalls;
	uint32 NumAcquireCalls;
	VkInstance Instance;
	TArray<FVulkanSemaphore*> ImageAcquiredSemaphore;
#if USE_IMAGE_ACQUIRE_FENCES
	TArray<VulkanRHI::FFence*> ImageAcquiredFences;
#endif

	int32 AcquireImageIndex(FVulkanSemaphore** OutSemaphore);

	friend class FVulkanViewport;
	friend class FVulkanQueue;
};
