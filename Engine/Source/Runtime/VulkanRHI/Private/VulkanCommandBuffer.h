// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanCommandBuffer.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "VulkanConfiguration.h"

class FVulkanDevice;
class FVulkanCommandBufferPool;
class FVulkanCommandBufferManager;
class FVulkanRenderTargetLayout;
class FVulkanQueue;

namespace VulkanRHI
{
	class FFence;
}

class FVulkanCmdBuffer
{
protected:
	friend class FVulkanCommandBufferManager;
	friend class FVulkanCommandBufferPool;
	friend class FVulkanQueue;

	FVulkanCmdBuffer(FVulkanDevice* InDevice, FVulkanCommandBufferPool* InCommandBufferPool);
	~FVulkanCmdBuffer();

public:
	FVulkanCommandBufferPool* GetOwner()
	{
		return CommandBufferPool;
	}

	inline bool IsInsideRenderPass() const
	{
		return State == EState::IsInsideRenderPass;
	}

	inline bool IsOutsideRenderPass() const
	{
		return State == EState::IsInsideBegin;
	}

	inline bool HasBegun() const
	{
		return State == EState::IsInsideBegin || State == EState::IsInsideRenderPass;
	}

	inline bool HasEnded() const
	{
		return State == EState::HasEnded;
	}

	inline bool IsSubmitted() const
	{
		return State == EState::Submitted;
	}

	inline VkCommandBuffer GetHandle()
	{
		return CommandBufferHandle;
	}

	void BeginRenderPass(const FVulkanRenderTargetLayout& Layout, class FVulkanRenderPass* RenderPass, class FVulkanFramebuffer* Framebuffer, const VkClearValue* AttachmentClearValues);

	void EndRenderPass()
	{
		check(IsInsideRenderPass());
		VulkanRHI::vkCmdEndRenderPass(CommandBufferHandle);
		State = EState::IsInsideBegin;
	}

	void End();

	inline volatile uint64 GetFenceSignaledCounter() const
	{
		return FenceSignaledCounter;
	}

	inline bool HasValidTiming() const
	{
		return (Timing != nullptr) && (FMath::Abs((int64)FenceSignaledCounter - (int64)LastValidTiming) < 3);
	}
	
	void Begin();

	enum class EState
	{
		ReadyForBegin,
		IsInsideBegin,
		IsInsideRenderPass,
		HasEnded,
		Submitted,
	};

	bool bNeedsDynamicStateSet;
	bool bHasPipeline;
	bool bHasViewport;
	bool bHasScissor;
	bool bHasStencilRef;

	VkViewport CurrentViewport;
	VkRect2D CurrentScissor;
	uint32 CurrentStencilRef;

private:
	FVulkanDevice* Device;
	VkCommandBuffer CommandBufferHandle;
	EState State;

	// Do not cache this pointer as it might change depending on VULKAN_REUSE_FENCES
	VulkanRHI::FFence* Fence;

	volatile uint64 FenceSignaledCounter;

	void RefreshFenceStatus();
	void InitializeTimings(FVulkanCommandListContext* InContext);

	FVulkanCommandBufferPool* CommandBufferPool;

	FVulkanGPUTiming* Timing;
	uint64 LastValidTiming;

	friend class FVulkanDynamicRHI;
};

class FVulkanCommandBufferPool
{
public:
	FVulkanCommandBufferPool(FVulkanDevice* InDevice);

	~FVulkanCommandBufferPool();

/*
	inline FVulkanCmdBuffer* GetActiveCmdBuffer()
	{
		if (UploadCmdBuffer)
		{
			SubmitUploadCmdBuffer(false);
		}

		return ActiveCmdBuffer;
	}

	inline bool HasPendingUploadCmdBuffer() const
	{
		return UploadCmdBuffer != nullptr;
	}

	inline bool HasPendingActiveCmdBuffer() const
	{
		return ActiveCmdBuffer != nullptr;
	}

	FVulkanCmdBuffer* GetUploadCmdBuffer();

	void SubmitUploadCmdBuffer(bool bWaitForFence);
	void SubmitActiveCmdBuffer(bool bWaitForFence);

	void WaitForCmdBuffer(FVulkanCmdBuffer* CmdBuffer, float TimeInSecondsToWait = 1.0f);
	*/
	void RefreshFenceStatus();
	/*
	void PrepareForNewActiveCommandBuffer();
*/

	inline VkCommandPool GetHandle() const
	{
		check(Handle != VK_NULL_HANDLE);
		return Handle;
	}

private:
	FVulkanDevice* Device;
	VkCommandPool Handle;
	//FVulkanCmdBuffer* ActiveCmdBuffer;

	FVulkanCmdBuffer* Create();

	TArray<FVulkanCmdBuffer*> CmdBuffers;

	void Create(uint32 QueueFamilyIndex);
	friend class FVulkanCommandBufferManager;
};

class FVulkanCommandBufferManager
{
public:
	FVulkanCommandBufferManager(FVulkanDevice* InDevice, FVulkanCommandListContext* InContext);
	~FVulkanCommandBufferManager();

	inline FVulkanCmdBuffer* GetActiveCmdBuffer()
	{
		if (UploadCmdBuffer)
		{
			SubmitUploadCmdBuffer(false);
		}

		return ActiveCmdBuffer;
	}

	inline bool HasPendingUploadCmdBuffer() const
	{
		return UploadCmdBuffer != nullptr;
	}

	inline bool HasPendingActiveCmdBuffer() const
	{
		return ActiveCmdBuffer != nullptr;
	}

	VULKANRHI_API FVulkanCmdBuffer* GetUploadCmdBuffer();

	VULKANRHI_API void SubmitUploadCmdBuffer(bool bWaitForFence);
	void SubmitActiveCmdBuffer(bool bWaitForFence);

	void WaitForCmdBuffer(FVulkanCmdBuffer* CmdBuffer, float TimeInSecondsToWait = 1.0f);

	void RefreshFenceStatus()
	{
		Pool.RefreshFenceStatus();
	}

	void PrepareForNewActiveCommandBuffer();

	inline VkCommandPool GetHandle() const
	{
		return Pool.GetHandle();
	}

	uint32 CalculateGPUTime();

private:
	FVulkanDevice* Device;
	FVulkanCommandBufferPool Pool;
	FVulkanQueue* Queue;
	FVulkanCmdBuffer* ActiveCmdBuffer;
	FVulkanCmdBuffer* UploadCmdBuffer;
};
