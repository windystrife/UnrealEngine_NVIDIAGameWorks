// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanViewport.h: Vulkan viewport RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanResources.h"

class FVulkanDynamicRHI;
class FVulkanSwapChain;
class FVulkanQueue;
struct FVulkanSemaphore;

class FVulkanViewport : public FRHIViewport, public VulkanRHI::FDeviceChild
{
public:
	enum { NUM_BUFFERS = 3 };

	FVulkanViewport(FVulkanDynamicRHI* InRHI, FVulkanDevice* InDevice, void* InWindowHandle, uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen, EPixelFormat InPreferredPixelFormat);
	~FVulkanViewport();

	FVulkanTexture2D* GetBackBuffer(FRHICommandList& RHICmdList);

	void WaitForFrameEventCompletion();

	//#todo-rco
	void IssueFrameEvent() {}

	inline FIntPoint GetSizeXY() const
	{
		return FIntPoint(SizeX, SizeY);
	}

	inline FVulkanSwapChain* GetSwapChain()
	{
		return SwapChain;
	}

	virtual void SetCustomPresent(FRHICustomPresent* InCustomPresent) override final
	{
		CustomPresent = InCustomPresent;
	}

	virtual FRHICustomPresent* GetCustomPresent() const override final
	{
		return CustomPresent;
	}

	void AdvanceBackBufferFrame();

	bool Present(FVulkanCmdBuffer* CmdBuffer, FVulkanQueue* Queue, FVulkanQueue* PresentQueue, bool bLockToVsync);

	inline uint32 GetPresentCount() const
	{
		return PresentCount;
	}

protected:
	VkImage BackBufferImages[NUM_BUFFERS];
	FVulkanSemaphore* RenderingDoneSemaphores[NUM_BUFFERS];
	FVulkanTextureView TextureViews[NUM_BUFFERS];

	// 'Dummy' back buffer
	TRefCountPtr<FVulkanBackBuffer> RenderingBackBuffer;
	TRefCountPtr<FVulkanBackBuffer> RHIBackBuffer;

	FVulkanDynamicRHI* RHI;
	uint32 SizeX;
	uint32 SizeY;
	bool bIsFullscreen;
	EPixelFormat PixelFormat;
	int32 AcquiredImageIndex;
	FVulkanSwapChain* SwapChain;
	void* WindowHandle;
	uint32 PresentCount;

	// Just a pointer, not owned by this class
	FVulkanSemaphore* AcquiredSemaphore;

	FCustomPresentRHIRef CustomPresent;

	void CreateSwapchain();
	void AcquireBackBuffer(FRHICommandListBase& CmdList, FVulkanBackBuffer* NewBackBuffer);

	void RecreateSwapchain(void* NewNativeWindow, bool bForce = false);
	void Resize(uint32 InSizeX, uint32 InSizeY, bool bIsFullscreen);

	static int32 DoAcquireImageIndex(FVulkanViewport* Viewport);
	bool DoCheckedSwapChainJob(TFunction<int32(FVulkanViewport*)> SwapChainJob);

	friend class FVulkanDynamicRHI;
	friend class FVulkanCommandListContext;
	friend struct FRHICommandAcquireBackBuffer;
};

template<>
struct TVulkanResourceTraits<FRHIViewport>
{
	typedef FVulkanViewport TConcreteType;
};
