// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanQueue.cpp: Vulkan Queue implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanQueue.h"
#include "VulkanSwapChain.h"
#include "VulkanMemory.h"

static int32 GWaitForIdleOnSubmit = 0;
static FAutoConsoleVariableRef CVarVulkanWaitForIdleOnSubmit(
	TEXT("r.Vulkan.WaitForIdleOnSubmit"),
	GWaitForIdleOnSubmit,
	TEXT("Waits for the GPU to be idle on every submit. Useful for tracking GPU hangs.\n")
	TEXT(" 0: Do not wait(default)\n")
	TEXT(" 1: Wait"),
	ECVF_Default
	);

FVulkanQueue::FVulkanQueue(FVulkanDevice* InDevice, uint32 InFamilyIndex, uint32 InQueueIndex)
	: Queue(VK_NULL_HANDLE)
	, FamilyIndex(InFamilyIndex)
	, QueueIndex(InQueueIndex)
	, Device(InDevice)
	, LastSubmittedCmdBuffer(nullptr)
	, LastSubmittedCmdBufferFenceCounter(0)
	, SubmitCounter(0)
{
	VulkanRHI::vkGetDeviceQueue(Device->GetInstanceHandle(), FamilyIndex, InQueueIndex, &Queue);
}

FVulkanQueue::~FVulkanQueue()
{
	check(Device);
}

void FVulkanQueue::Submit(FVulkanCmdBuffer* CmdBuffer, FVulkanSemaphore* WaitSemaphore, VkPipelineStageFlags WaitStageFlags, FVulkanSemaphore* SignalSemaphore)
{
	check(CmdBuffer->HasEnded());

	VulkanRHI::FFence* Fence = CmdBuffer->Fence;
	check(!Fence->IsSignaled());

	const VkCommandBuffer CmdBuffers[] = { CmdBuffer->GetHandle() };

	VkSemaphore Semaphores[2];

	VkSubmitInfo SubmitInfo;
	FMemory::Memzero(SubmitInfo);
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = CmdBuffers;
	if (SignalSemaphore)
	{
		SubmitInfo.signalSemaphoreCount = 1;
		Semaphores[0] = SignalSemaphore->GetHandle();
		SubmitInfo.pSignalSemaphores = &Semaphores[0];
	}
	if (WaitSemaphore)
	{
		SubmitInfo.waitSemaphoreCount = 1;
		Semaphores[1] = WaitSemaphore->GetHandle();
		SubmitInfo.pWaitSemaphores = &Semaphores[1];
		SubmitInfo.pWaitDstStageMask = &WaitStageFlags;
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanQueueSubmit)
		VERIFYVULKANRESULT(VulkanRHI::vkQueueSubmit(Queue, 1, &SubmitInfo, Fence->GetHandle()));
	}

	if (GWaitForIdleOnSubmit != 0)
	{
		VERIFYVULKANRESULT(VulkanRHI::vkQueueWaitIdle(Queue));
		CmdBuffer->GetOwner()->RefreshFenceStatus();
		// 60 ms timeout
		Device->GetFenceManager().WaitForFence(CmdBuffer->Fence, 1000 * 60000);
		ensure(Device->GetFenceManager().IsFenceSignaled(CmdBuffer->Fence));
	}

	CmdBuffer->State = FVulkanCmdBuffer::EState::Submitted;

	UpdateLastSubmittedCommandBuffer(CmdBuffer);

	CmdBuffer->GetOwner()->RefreshFenceStatus();

	Device->GetStagingManager().ProcessPendingFree(false, false);
}

void FVulkanQueue::UpdateLastSubmittedCommandBuffer(FVulkanCmdBuffer* CmdBuffer)
{
	FScopeLock ScopeLock(&CS);
	LastSubmittedCmdBuffer = CmdBuffer;
	LastSubmittedCmdBufferFenceCounter = CmdBuffer->GetFenceSignaledCounter();
	++SubmitCounter;
}
