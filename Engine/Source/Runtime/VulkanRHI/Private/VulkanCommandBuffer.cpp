// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanCommandBuffer.cpp: Vulkan device RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanContext.h"

static int32 GUseSingleQueue = 0;
static FAutoConsoleVariableRef CVarVulkanUseSingleQueue(
	TEXT("r.Vulkan.UseSingleQueue"),
	GUseSingleQueue,
	TEXT("Forces using the same queue for uploads and graphics.\n")
	TEXT(" 0: Uses multiple queues(default)\n")
	TEXT(" 1: Always uses the gfx queue for submissions"),
	ECVF_Default
);

static int32 GVulkanProfileCmdBuffers = 0;
static FAutoConsoleVariableRef CVarVulkanProfileCmdBuffers(
	TEXT("r.Vulkan.ProfileCmdBuffers"),
	GVulkanProfileCmdBuffers,
	TEXT("Insert GPU timing queries in every cmd buffer\n"),
	ECVF_Default
);

FVulkanCmdBuffer::FVulkanCmdBuffer(FVulkanDevice* InDevice, FVulkanCommandBufferPool* InCommandBufferPool)
	: bNeedsDynamicStateSet(true)
	, bHasPipeline(false)
	, bHasViewport(false)
	, bHasScissor(false)
	, bHasStencilRef(false)
	, CurrentStencilRef(0)
	, Device(InDevice)
	, CommandBufferHandle(VK_NULL_HANDLE)
	, State(EState::ReadyForBegin)
	, Fence(nullptr)
	, FenceSignaledCounter(0)
	, CommandBufferPool(InCommandBufferPool)
	, Timing(nullptr)
	, LastValidTiming(0)
{
	FMemory::Memzero(CurrentViewport);
	FMemory::Memzero(CurrentScissor);
	
	VkCommandBufferAllocateInfo CreateCmdBufInfo;
	FMemory::Memzero(CreateCmdBufInfo);
	CreateCmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	CreateCmdBufInfo.pNext = nullptr;
	CreateCmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	CreateCmdBufInfo.commandBufferCount = 1;
	CreateCmdBufInfo.commandPool = CommandBufferPool->GetHandle();

	VERIFYVULKANRESULT(VulkanRHI::vkAllocateCommandBuffers(Device->GetInstanceHandle(), &CreateCmdBufInfo, &CommandBufferHandle));
	Fence = Device->GetFenceManager().AllocateFence();
}

FVulkanCmdBuffer::~FVulkanCmdBuffer()
{
	VulkanRHI::FFenceManager& FenceManager = Device->GetFenceManager();
	if (State == EState::Submitted)
	{
		// Wait 60ms
		uint64 WaitForCmdBufferInNanoSeconds = 60 * 1000 * 1000LL;
		FenceManager.WaitAndReleaseFence(Fence, WaitForCmdBufferInNanoSeconds);
	}
	else
	{
		// Just free the fence, CmdBuffer was not submitted
		FenceManager.ReleaseFence(Fence);
	}

	VulkanRHI::vkFreeCommandBuffers(Device->GetInstanceHandle(), CommandBufferPool->GetHandle(), 1, &CommandBufferHandle);
	CommandBufferHandle = VK_NULL_HANDLE;

	if (Timing)
	{
		Timing->Release();
	}
}

void FVulkanCmdBuffer::BeginRenderPass(const FVulkanRenderTargetLayout& Layout, FVulkanRenderPass* RenderPass, FVulkanFramebuffer* Framebuffer, const VkClearValue* AttachmentClearValues)
{
	check(IsOutsideRenderPass());

	VkRenderPassBeginInfo Info;
	FMemory::Memzero(Info);
	Info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	Info.renderPass = RenderPass->GetHandle();
	Info.framebuffer = Framebuffer->GetHandle();
	Info.renderArea.offset.x = 0;
	Info.renderArea.offset.y = 0;
	Info.renderArea.extent.width = Framebuffer->GetWidth();
	Info.renderArea.extent.height = Framebuffer->GetHeight();
	Info.clearValueCount = Layout.GetNumUsedClearValues();
	Info.pClearValues = AttachmentClearValues;

	VulkanRHI::vkCmdBeginRenderPass(CommandBufferHandle, &Info, VK_SUBPASS_CONTENTS_INLINE);

	State = EState::IsInsideRenderPass;
}

void FVulkanCmdBuffer::End()
{
	check(IsOutsideRenderPass());

	if (GVulkanProfileCmdBuffers)
	{
		if (Timing)
		{
			Timing->EndTiming(this);
			LastValidTiming = FenceSignaledCounter;
		}
	}

	VERIFYVULKANRESULT(VulkanRHI::vkEndCommandBuffer(GetHandle()));
	State = EState::HasEnded;
}

inline void FVulkanCmdBuffer::InitializeTimings(FVulkanCommandListContext* InContext)
{
	if (GVulkanProfileCmdBuffers && !Timing)
	{
		if (InContext)
		{
			Timing = new FVulkanGPUTiming(InContext, Device);
			Timing->Initialize();
		}
	}
}

void FVulkanCmdBuffer::Begin()
{
	check(State == EState::ReadyForBegin);

	VkCommandBufferBeginInfo CmdBufBeginInfo;
	FMemory::Memzero(CmdBufBeginInfo);
	CmdBufBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	CmdBufBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VERIFYVULKANRESULT(VulkanRHI::vkBeginCommandBuffer(CommandBufferHandle, &CmdBufBeginInfo));

	if (GVulkanProfileCmdBuffers)
	{
		InitializeTimings(&Device->GetImmediateContext());
		if (Timing)
		{
			Timing->StartTiming(this);
		}
	}

	bNeedsDynamicStateSet = true;
	State = EState::IsInsideBegin;
}

void FVulkanCmdBuffer::RefreshFenceStatus()
{
	if (State == EState::Submitted)
	{
		VulkanRHI::FFenceManager* FenceMgr = Fence->GetOwner();
		if (FenceMgr->IsFenceSignaled(Fence))
		{
			State = EState::ReadyForBegin;
			bHasPipeline = false;
			bHasViewport = false;
			bHasScissor = false;
			bHasStencilRef = false;

			FMemory::Memzero(CurrentViewport);
			FMemory::Memzero(CurrentScissor);
			CurrentStencilRef = 0;

			VulkanRHI::vkResetCommandBuffer(CommandBufferHandle, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
#if VULKAN_REUSE_FENCES
			Fence->GetOwner()->ResetFence(Fence);
#else
			VulkanRHI::FFence* PrevFence = Fence;
			Fence = FenceMgr->AllocateFence();
			FenceMgr->ReleaseFence(PrevFence);
#endif
			++FenceSignaledCounter;
		}
	}
	else
	{
		check(!Fence->IsSignaled());
	}
}

FVulkanCommandBufferPool::FVulkanCommandBufferPool(FVulkanDevice* InDevice)
	: Device(InDevice)
	, Handle(VK_NULL_HANDLE)
{
}

FVulkanCommandBufferPool::~FVulkanCommandBufferPool()
{
	for (int32 Index = 0; Index < CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = CmdBuffers[Index];
		delete CmdBuffer;
	}

	VulkanRHI::vkDestroyCommandPool(Device->GetInstanceHandle(), Handle, nullptr);
	Handle = VK_NULL_HANDLE;
}

void FVulkanCommandBufferPool::Create(uint32 QueueFamilyIndex)
{
	VkCommandPoolCreateInfo CmdPoolInfo;
	FMemory::Memzero(CmdPoolInfo);
	CmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	CmdPoolInfo.queueFamilyIndex =  QueueFamilyIndex;
	//#todo-rco: Should we use VK_COMMAND_POOL_CREATE_TRANSIENT_BIT?
	CmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VERIFYVULKANRESULT(VulkanRHI::vkCreateCommandPool(Device->GetInstanceHandle(), &CmdPoolInfo, nullptr, &Handle));
}

FVulkanCommandBufferManager::FVulkanCommandBufferManager(FVulkanDevice* InDevice, FVulkanCommandListContext* InContext)
	: Device(InDevice)
	, Pool(InDevice)
	, Queue(InContext->GetQueue())
	, ActiveCmdBuffer(nullptr)
	, UploadCmdBuffer(nullptr)
{
	check(Device);

	Pool.Create(Queue->GetFamilyIndex());

	ActiveCmdBuffer = Pool.Create();
	ActiveCmdBuffer->InitializeTimings(InContext);
	ActiveCmdBuffer->Begin();

	if (InContext->IsImmediate())
	{
		// Insert the Begin frame timestamp query. On EndDrawingViewport() we'll insert the End and immediately after a new Begin()
		InContext->WriteBeginTimestamp(ActiveCmdBuffer);

		// Flush the cmd buffer immediately to ensure a valid
		// 'Last submitted' cmd buffer exists at frame 0.
		SubmitActiveCmdBuffer(false);
		PrepareForNewActiveCommandBuffer();
	}
}

FVulkanCommandBufferManager::~FVulkanCommandBufferManager()
{
}

void FVulkanCommandBufferManager::WaitForCmdBuffer(FVulkanCmdBuffer* CmdBuffer, float TimeInSecondsToWait)
{
	check(CmdBuffer->IsSubmitted());
	bool bSuccess = Device->GetFenceManager().WaitForFence(CmdBuffer->Fence, (uint64)(TimeInSecondsToWait * 1e9));
	check(bSuccess);
	CmdBuffer->RefreshFenceStatus();
}


void FVulkanCommandBufferManager::SubmitUploadCmdBuffer(bool bWaitForFence)
{
	check(UploadCmdBuffer);
	if (!UploadCmdBuffer->IsSubmitted() && UploadCmdBuffer->HasBegun())
	{
		check(UploadCmdBuffer->IsOutsideRenderPass());
		UploadCmdBuffer->End();
		Queue->Submit(UploadCmdBuffer, nullptr, 0, nullptr);
	}

	if (bWaitForFence)
	{
		if (UploadCmdBuffer->IsSubmitted())
		{
			WaitForCmdBuffer(UploadCmdBuffer);
		}
	}

	UploadCmdBuffer = nullptr;
}

void FVulkanCommandBufferManager::SubmitActiveCmdBuffer(bool bWaitForFence)
{
	check(!UploadCmdBuffer);
	check(ActiveCmdBuffer);
	if (!ActiveCmdBuffer->IsSubmitted() && ActiveCmdBuffer->HasBegun())
	{
		if (!ActiveCmdBuffer->IsOutsideRenderPass())
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Forcing EndRenderPass() for submission"));
			ActiveCmdBuffer->EndRenderPass();
		}
		ActiveCmdBuffer->End();
		Queue->Submit(ActiveCmdBuffer, nullptr, 0, nullptr);
	}

	if (bWaitForFence)
	{
		if (ActiveCmdBuffer->IsSubmitted())
		{
			WaitForCmdBuffer(ActiveCmdBuffer);
		}
	}

	ActiveCmdBuffer = nullptr;
}

FVulkanCmdBuffer* FVulkanCommandBufferPool::Create()
{
	check(Device);
	FVulkanCmdBuffer* CmdBuffer = new FVulkanCmdBuffer(Device, this);
	CmdBuffers.Add(CmdBuffer);
	check(CmdBuffer);
	return CmdBuffer;
}

void FVulkanCommandBufferPool::RefreshFenceStatus()
{
	for (int32 Index = 0; Index < CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = CmdBuffers[Index];
		CmdBuffer->RefreshFenceStatus();
	}
}

void FVulkanCommandBufferManager::PrepareForNewActiveCommandBuffer()
{
	check(!UploadCmdBuffer);

	for (int32 Index = 0; Index < Pool.CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = Pool.CmdBuffers[Index];
		CmdBuffer->RefreshFenceStatus();
		if (CmdBuffer->State == FVulkanCmdBuffer::EState::ReadyForBegin)
		{
			ActiveCmdBuffer = CmdBuffer;
			ActiveCmdBuffer->Begin();
			return;
		}
		else
		{
			check(CmdBuffer->State == FVulkanCmdBuffer::EState::Submitted);
		}
	}

	// All cmd buffers are being executed still
	ActiveCmdBuffer = Pool.Create();
	ActiveCmdBuffer->Begin();
}

uint32 FVulkanCommandBufferManager::CalculateGPUTime()
{
	uint32 Time = 0;
	for (int32 Index = 0; Index < Pool.CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = Pool.CmdBuffers[Index];
		if (CmdBuffer->HasValidTiming())
		{
			Time += CmdBuffer->Timing->GetTiming(false);
		}
	}
	return Time;
}

FVulkanCmdBuffer* FVulkanCommandBufferManager::GetUploadCmdBuffer()
{
	if (!UploadCmdBuffer)
	{
		for (int32 Index = 0; Index < Pool.CmdBuffers.Num(); ++Index)
		{
			FVulkanCmdBuffer* CmdBuffer = Pool.CmdBuffers[Index];
			CmdBuffer->RefreshFenceStatus();
			if (CmdBuffer->State == FVulkanCmdBuffer::EState::ReadyForBegin)
			{
				UploadCmdBuffer = CmdBuffer;
				UploadCmdBuffer->Begin();
				return UploadCmdBuffer;
			}
		}

		// All cmd buffers are being executed still
		UploadCmdBuffer = Pool.Create();
		UploadCmdBuffer->Begin();
	}

	return UploadCmdBuffer;
}
