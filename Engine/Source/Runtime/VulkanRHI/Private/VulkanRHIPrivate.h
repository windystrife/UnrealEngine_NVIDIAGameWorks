// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanRHIPrivate.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

// Dependencies
#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "RHI.h"
#include "RenderUtils.h"
#include "VulkanConfiguration.h"

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#endif

#ifndef VK_PROTOTYPES
#define VK_PROTOTYPES	1
#endif

#if PLATFORM_WINDOWS
	#define VK_USE_PLATFORM_WIN32_KHR 1
	#define VK_USE_PLATFORM_WIN32_KHX 1
#endif
#if PLATFORM_ANDROID
	#define VK_USE_PLATFORM_ANDROID_KHR 1
#endif

#if PLATFORM_ANDROID
	#define VULKAN_COMMANDWRAPPERS_ENABLE VULKAN_ENABLE_DUMP_LAYER
	#define VULKAN_DYNAMICALLYLOADED 1
#elif PLATFORM_LINUX
	#define VULKAN_COMMANDWRAPPERS_ENABLE 1
	#define VULKAN_DYNAMICALLYLOADED 1
#else
	#define VULKAN_COMMANDWRAPPERS_ENABLE 1
	#define VULKAN_DYNAMICALLYLOADED 0
#endif

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif

#if VULKAN_DYNAMICALLYLOADED
	#include "VulkanLoader.h"
#else
	#include <vulkan.h>
#endif

#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif

#if VULKAN_COMMANDWRAPPERS_ENABLE
	#if VULKAN_DYNAMICALLYLOADED
		// Vulkan API is defined in VulkanDynamicAPI namespace.
		#define VULKANAPINAMESPACE VulkanDynamicAPI
	#else
		// Vulkan API is in the global namespace.
		#define VULKANAPINAMESPACE
	#endif
	#include "VulkanCommandWrappers.h"
#else
	#if VULKAN_DYNAMICALLYLOADED
		#include "VulkanCommandsDirect.h"
	#else
		#error "Statically linked vulkan api must be wrapped!"
	#endif
#endif


#include "VulkanRHI.h"
#include "VulkanGlobalUniformBuffer.h"
#include "RHI.h"
#include "VulkanDevice.h"
#include "VulkanQueue.h"
#include "VulkanCommandBuffer.h"
#include "Stats2.h"

using namespace VulkanRHI;

class FVulkanQueue;
class FVulkanCmdBuffer;
class FVulkanShader;
class FVulkanDescriptorSetsLayout;
class FVulkanGfxPipeline;
class FVulkanRenderPass;
class FVulkanCommandBufferManager;

inline VkShaderStageFlagBits UEFrequencyToVKStageBit(EShaderFrequency InStage)
{
	VkShaderStageFlagBits OutStage = VK_SHADER_STAGE_ALL;

	switch (InStage)
	{
	case SF_Vertex:		OutStage = VK_SHADER_STAGE_VERTEX_BIT;						break;
	case SF_Hull:		OutStage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;		break;
	case SF_Domain:		OutStage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;		break;
	case SF_Pixel:		OutStage = VK_SHADER_STAGE_FRAGMENT_BIT;					break;
	case SF_Geometry:	OutStage = VK_SHADER_STAGE_GEOMETRY_BIT;					break;
	case SF_Compute:	OutStage = VK_SHADER_STAGE_COMPUTE_BIT;						break;
	default:
		checkf(false, TEXT("Undefined shader stage %d"), (int32)InStage);
		break;
	}

	check(OutStage != VK_SHADER_STAGE_ALL);
	return OutStage;
}

class FVulkanRenderTargetLayout
{
public:
	FVulkanRenderTargetLayout(const FGraphicsPipelineStateInitializer& Initializer);
	FVulkanRenderTargetLayout(const FRHISetRenderTargetsInfo& RTInfo);

	inline uint32 GetHash() const { return OldHash; }
	inline uint32 GetRenderPassHash() const { return RenderPassHash; }
	inline const VkExtent2D& GetExtent2D() const { return Extent.Extent2D; }
	inline const VkExtent3D& GetExtent3D() const { return Extent.Extent3D; }
	inline const VkAttachmentDescription* GetAttachmentDescriptions() const { return Desc; }
	inline uint32 GetNumColorAttachments() const { return NumColorAttachments; }
	inline bool GetHasDepthStencil() const { return bHasDepthStencil != 0; }
	inline bool GetHasResolveAttachments() const { return bHasResolveAttachments != 0; }
	inline uint32 GetNumAttachmentDescriptions() const { return NumAttachmentDescriptions; }
	inline uint32 GetNumSamples() const { return NumSamples; }
	inline uint32 GetNumUsedClearValues() const
	{
		return NumUsedClearValues;
	}

	inline const VkAttachmentReference* GetColorAttachmentReferences() const { return NumColorAttachments > 0 ? ColorReferences : nullptr; }
	inline const VkAttachmentReference* GetResolveAttachmentReferences() const { return bHasResolveAttachments ? ResolveReferences : nullptr; }
	inline const VkAttachmentReference* GetDepthStencilAttachmentReference() const { return bHasDepthStencil ? &DepthStencilReference : nullptr; }

protected:
	VkAttachmentReference ColorReferences[MaxSimultaneousRenderTargets];
	uint16 MipLevels[MaxSimultaneousRenderTargets];
	uint16 ArraySlices[MaxSimultaneousRenderTargets];
	VkAttachmentReference ResolveReferences[MaxSimultaneousRenderTargets];
	VkAttachmentReference DepthStencilReference;

	VkAttachmentDescription Desc[MaxSimultaneousRenderTargets * 2 + 1];

	uint32 NumAttachmentDescriptions;
	uint32 NumColorAttachments;
	uint8 bHasDepthStencil;
	uint8 bHasResolveAttachments;
	uint8 NumSamples;
	uint8 NumUsedClearValues;

	uint32 OldHash;
	uint32 RenderPassHash;

	union
	{
		VkExtent3D	Extent3D;
		VkExtent2D	Extent2D;
	} Extent;

	FVulkanRenderTargetLayout()
	{
		FMemory::Memzero(ColorReferences);
		FMemory::Memzero(MipLevels);
		FMemory::Memzero(ArraySlices);
		FMemory::Memzero(ResolveReferences);
		FMemory::Memzero(DepthStencilReference);
		FMemory::Memzero(Desc);
		NumAttachmentDescriptions = 0;
		NumColorAttachments = 0;
		bHasDepthStencil = 0;
		bHasResolveAttachments = 0;
		OldHash = 0;
		RenderPassHash = 0;
		Extent.Extent3D.width = 0;
		Extent.Extent3D.height = 0;
		Extent.Extent3D.depth = 0;
	}

	friend class FVulkanPipelineStateCache;

private:
	void CreateRenderPassHash();
};

struct FVulkanSemaphore
{
	FVulkanSemaphore(FVulkanDevice& InDevice):
		Device(InDevice),
		SemaphoreHandle(VK_NULL_HANDLE)
	{
		// Create semaphore
		VkSemaphoreCreateInfo PresentCompleteSemaphoreCreateInfo;
		FMemory::Memzero(PresentCompleteSemaphoreCreateInfo);
		PresentCompleteSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		PresentCompleteSemaphoreCreateInfo.pNext = nullptr;
		PresentCompleteSemaphoreCreateInfo.flags = 0;
		VERIFYVULKANRESULT(VulkanRHI::vkCreateSemaphore(Device.GetInstanceHandle(), &PresentCompleteSemaphoreCreateInfo, nullptr, &SemaphoreHandle));
	}

	~FVulkanSemaphore()
	{
		check(SemaphoreHandle != VK_NULL_HANDLE);
		Device.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::Semaphore, SemaphoreHandle);
		SemaphoreHandle = VK_NULL_HANDLE;
	}

	VkSemaphore GetHandle() const
	{
		check(SemaphoreHandle != VK_NULL_HANDLE);
		return SemaphoreHandle;
	}
	
private:
	FVulkanDevice& Device;
	VkSemaphore SemaphoreHandle;
};

class FVulkanFramebuffer
{
public:
	FVulkanFramebuffer(FVulkanDevice& Device, const FRHISetRenderTargetsInfo& InRTInfo, const FVulkanRenderTargetLayout& RTLayout, const FVulkanRenderPass& RenderPass);
	~FVulkanFramebuffer();

	bool Matches(const FRHISetRenderTargetsInfo& RTInfo) const;

	inline uint32 GetNumColorAttachments() const { return NumColorAttachments; }

	void Destroy(FVulkanDevice& Device);

	VkFramebuffer GetHandle()
	{
		check(Framebuffer != VK_NULL_HANDLE);
		return Framebuffer;
	}

	TArray<VkImageView> AttachmentViews;
	TArray<VkImageView> AttachmentViewsToDelete;

	inline bool ContainsRenderTarget(FRHITexture* Texture) const
	{
		ensure(Texture);
		for (int32 Index = 0; Index < FMath::Min((int32)NumColorAttachments, RTInfo.NumColorRenderTargets); ++Index)
		{
			if (RTInfo.ColorRenderTarget[Index].Texture == Texture)
			{
				return true;
			}
		}

		if (RTInfo.DepthStencilRenderTarget.Texture == Texture)
		{
			return true;
		}

		return false;
	}

	inline bool ContainsRenderTarget(VkImage Image) const
	{
		ensure(Image != VK_NULL_HANDLE);
		for (int32 Index = 0; Index < FMath::Min((int32)NumColorAttachments, RTInfo.NumColorRenderTargets); ++Index)
		{
			FRHITexture* RHITexture = RTInfo.ColorRenderTarget[Index].Texture;
			if (RHITexture)
			{
				FVulkanTextureBase* Base = (FVulkanTextureBase*)RHITexture->GetTextureBaseRHI();
				if (Image == Base->Surface.Image)
				{
					return true;
				}
			}
		}

		if (RTInfo.DepthStencilRenderTarget.Texture)
		{
			FVulkanTextureBase* Depth = (FVulkanTextureBase*)RTInfo.DepthStencilRenderTarget.Texture->GetTextureBaseRHI();
			check(Depth);
			return Depth->Surface.Image == Image;
		}

		return false;
	}

	inline uint32 GetWidth() const
	{
		return Extents.width;
	}

	inline uint32 GetHeight() const
	{
		return Extents.height;
	}

private:
	VkFramebuffer Framebuffer;
	VkExtent2D Extents;

	// We do not adjust RTInfo, since it used for hashing and is what the UE provides,
	// it's up to VulkanRHI to handle this correctly.
	const FRHISetRenderTargetsInfo RTInfo;
	uint32 NumColorAttachments;

	// Predefined set of barriers, when executes ensuring all writes are finished
	TArray<VkImageMemoryBarrier> WriteBarriers;

#if VULKAN_KEEP_CREATE_INFO
	VkFramebufferCreateInfo CreateInfo;
#endif

	friend class FVulkanCommandListContext;
};

class FVulkanRenderPass
{
public:
	const FVulkanRenderTargetLayout& GetLayout() const { return Layout; }
	VkRenderPass GetHandle() const { check(RenderPass != VK_NULL_HANDLE); return RenderPass; }
	uint32 GetNumUsedClearValues() const
	{
		return NumUsedClearValues;
	}

private:
	friend class FVulkanCommandListContext;
	friend class FVulkanPipelineStateCache;

	FVulkanRenderPass(FVulkanDevice& Device, const FVulkanRenderTargetLayout& RTLayout);
	~FVulkanRenderPass();

private:
	FVulkanRenderTargetLayout Layout;
	VkRenderPass RenderPass;
	uint32 NumUsedClearValues;
	FVulkanDevice& Device;

#if VULKAN_KEEP_CREATE_INFO
	const FVulkanRenderTargetLayout& RTLayout;
	VkSubpassDescription SubpassDesc;
	VkRenderPassCreateInfo CreateInfo;
#endif
};

namespace VulkanRHI
{
	inline void SetupImageBarrierOLD(VkImageMemoryBarrier& Barrier, const FVulkanSurface& Surface, VkAccessFlags SrcMask, VkImageLayout SrcLayout, VkAccessFlags DstMask, VkImageLayout DstLayout, uint32 NumLayers = 1)
	{
		Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		Barrier.srcAccessMask = SrcMask;
		Barrier.dstAccessMask = DstMask;
		Barrier.oldLayout = SrcLayout;
		Barrier.newLayout = DstLayout;
		Barrier.image = Surface.Image;
		Barrier.subresourceRange.aspectMask = Surface.GetFullAspectMask();
		Barrier.subresourceRange.levelCount = Surface.GetNumMips();
		//#todo-rco: Cubemaps?
		//Barriers[Index].subresourceRange.baseArrayLayer = 0;
		Barrier.subresourceRange.layerCount = NumLayers;
		Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}

	inline void SetupBufferBarrier(VkBufferMemoryBarrier& Barrier, VkAccessFlags SrcAccess, VkAccessFlags DstAccess, VkBuffer Buffer, uint32 Offset, VkDeviceSize Size)
	{
		Barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		Barrier.srcAccessMask = SrcAccess;
		Barrier.dstAccessMask = DstAccess;
		Barrier.buffer = Buffer;
		Barrier.offset = Offset;
		Barrier.size = Size;
	}

	inline void SetupAndZeroImageBarrierOLD(VkImageMemoryBarrier& Barrier, const FVulkanSurface& Surface, VkAccessFlags SrcMask, VkImageLayout SrcLayout, VkAccessFlags DstMask, VkImageLayout DstLayout)
	{
		FMemory::Memzero(Barrier);
		SetupImageBarrierOLD(Barrier, Surface, SrcMask, SrcLayout, DstMask, DstLayout);
	}

	inline void SetupAndZeroBufferBarrier(VkBufferMemoryBarrier& Barrier, VkAccessFlags SrcAccess, VkAccessFlags DstAccess, VkBuffer Buffer, uint32 Offset, VkDeviceSize Size)
	{
		FMemory::Memzero(Barrier);
		SetupBufferBarrier(Barrier, SrcAccess, DstAccess, Buffer, Offset, Size);
	}
}

void VulkanSetImageLayout(VkCommandBuffer CmdBuffer, VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange& SubresourceRange);

// Transitions Color Images's first mip/layer/face
inline void VulkanSetImageLayoutSimple(VkCommandBuffer CmdBuffer, VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, VkImageAspectFlags Aspect = VK_IMAGE_ASPECT_COLOR_BIT)
{
	VkImageSubresourceRange SubresourceRange = { Aspect, 0, 1, 0, 1 };
	VulkanSetImageLayout(CmdBuffer, Image, OldLayout, NewLayout, SubresourceRange);
}

void VulkanResolveImage(VkCommandBuffer Cmd, FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI);

// Stats
DECLARE_STATS_GROUP(TEXT("Vulkan RHI"), STATGROUP_VulkanRHI, STATCAT_Advanced);
//DECLARE_STATS_GROUP(TEXT("Vulkan RHI Verbose"), STATGROUP_VulkanRHIVERBOSE, STATCAT_Advanced);
//DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("DrawPrimitive calls"), STAT_VulkanDrawPrimitiveCalls, STATGROUP_VulkanRHI, );
//DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Triangles drawn"), STAT_VulkanTriangles, STATGROUP_VulkanRHI, );
//DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Lines drawn"), STAT_VulkanLines, STATGROUP_VulkanRHI, );
//DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Textures Allocated"), STAT_VulkanTexturesAllocated, STATGROUP_VulkanRHI, );
//DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Textures Released"), STAT_VulkanTexturesReleased, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Draw call time"), STAT_VulkanDrawCallTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Dispatch call time"), STAT_VulkanDispatchCallTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Draw call prep time"), STAT_VulkanDrawCallPrepareTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Dispatch call prep time"), STAT_VulkanDispatchCallPrepareTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Create uniform buffer time"), STAT_VulkanCreateUniformBufferTime, STATGROUP_VulkanRHI, );
//DECLARE_CYCLE_STAT_EXTERN(TEXT("Update uniform buffer"), STAT_VulkanUpdateUniformBufferTime, STATGROUP_VulkanRHI, );
//DECLARE_CYCLE_STAT_EXTERN(TEXT("GPU Flip Wait Time"), STAT_VulkanGPUFlipWaitTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Get Or Create Pipeline"), STAT_VulkanGetOrCreatePipeline, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Get DescriptorSet"), STAT_VulkanGetDescriptorSet, STATGROUP_VulkanRHI, );
//DECLARE_CYCLE_STAT_EXTERN(TEXT("Create Pipeline"), STAT_VulkanCreatePipeline, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Pipeline Bind"), STAT_VulkanPipelineBind, STATGROUP_VulkanRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Bound Shader States"), STAT_VulkanNumBoundShaderState, STATGROUP_VulkanRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Render Passes"), STAT_VulkanNumRenderPasses, STATGROUP_VulkanRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Frame Buffers"), STAT_VulkanNumFrameBuffers, STATGROUP_VulkanRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Buffer Views"), STAT_VulkanNumBufferViews, STATGROUP_VulkanRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Image Views"), STAT_VulkanNumImageViews, STATGROUP_VulkanRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Physical Mem Allocations"), STAT_VulkanNumPhysicalMemAllocations, STATGROUP_VulkanRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Dynamic VB Size"), STAT_VulkanDynamicVBSize, STATGROUP_VulkanRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Dynamic IB Size"), STAT_VulkanDynamicIBSize, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Dynamic VB Lock/Unlock time"), STAT_VulkanDynamicVBLockTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Dynamic IB Lock/Unlock time"), STAT_VulkanDynamicIBLockTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("DrawPrim UP Prep Time"), STAT_VulkanUPPrepTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Uniform Buffer Creation Time"), STAT_VulkanUniformBufferCreateTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Apply DS Uniform Buffers"), STAT_VulkanApplyDSUniformBuffers, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("SRV Update Time"), STAT_VulkanSRVUpdateTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("UAV Update Time"), STAT_VulkanUAVUpdateTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Deletion Queue"), STAT_VulkanDeletionQueue, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Queue Submit"), STAT_VulkanQueueSubmit, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Queue Present"), STAT_VulkanQueuePresent, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Wait For Query"), STAT_VulkanWaitQuery, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Reset Queries"), STAT_VulkanResetQuery, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Wait For Swapchain"), STAT_VulkanWaitSwapchain, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Acquire Backbuffer"), STAT_VulkanAcquireBackBuffer, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Staging Buffer Mgmt"), STAT_VulkanStagingBuffer, STATGROUP_VulkanRHI, );
#if VULKAN_ENABLE_AGGRESSIVE_STATS
DECLARE_CYCLE_STAT_EXTERN(TEXT("Apply DS Shader Resources"), STAT_VulkanApplyDSResources, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update DescriptorSets"), STAT_VulkanUpdateDescriptorSets, STATGROUP_VulkanRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Desc Sets Updated"), STAT_VulkanNumDescSets, STATGROUP_VulkanRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num WriteDescriptors Cmd"), STAT_VulkanNumUpdateDescriptors, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Set Shader Param"), STAT_VulkanSetShaderParamTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Set unif Buffer"), STAT_VulkanSetUniformBufferTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("VkUpdate DS"), STAT_VulkanVkUpdateDS, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Clear Dirty DS State"), STAT_VulkanClearDirtyDSState, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Bind Vertex Streams"), STAT_VulkanBindVertexStreamsTime, STATGROUP_VulkanRHI, );
#endif

namespace VulkanRHI
{
	struct FPendingBufferLock
	{
		FStagingBuffer* StagingBuffer;
		uint32 Offset;
		uint32 Size;
		EResourceLockMode LockMode;
	};

	static VkImageAspectFlags GetAspectMaskFromUEFormat(EPixelFormat Format, bool bIncludeStencil, bool bIncludeDepth = true)
	{
		switch (Format)
		{
		case PF_X24_G8:
			return VK_IMAGE_ASPECT_STENCIL_BIT;
		case PF_DepthStencil:
			return (bIncludeDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) | (bIncludeStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
		case PF_ShadowDepth:
		case PF_D24:
			return VK_IMAGE_ASPECT_DEPTH_BIT;
		default:
			return VK_IMAGE_ASPECT_COLOR_BIT;
		}
	}

	inline VkAccessFlags GetAccessMask(VkImageLayout Layout)
	{
		VkAccessFlags Flags = 0;
		switch (Layout)
		{
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			Flags = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			Flags = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			Flags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			Flags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			Flags = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			Flags = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			Flags = VK_ACCESS_MEMORY_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_GENERAL:
		case VK_IMAGE_LAYOUT_UNDEFINED:
			Flags = 0;
			break;
			break;
		default:
			check(0);
			break;
		}
		return Flags;
	};

	inline VkPipelineStageFlags GetStageFlags(VkImageLayout Layout)
	{
		VkAccessFlags Flags = 0;
		switch (Layout)
		{
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			Flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			Flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			Flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			Flags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			Flags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			Flags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			Flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			break;
		case VK_IMAGE_LAYOUT_GENERAL:
		case VK_IMAGE_LAYOUT_UNDEFINED:
			Flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			break;
			break;
		default:
			check(0);
			break;
		}
		return Flags;
	};
}

#if VULKAN_HAS_DEBUGGING_ENABLED
extern TAutoConsoleVariable<int32> GValidationCvar;
#endif

static inline VkAttachmentLoadOp RenderTargetLoadActionToVulkan(ERenderTargetLoadAction InLoadAction)
{
	VkAttachmentLoadOp OutLoadAction = VK_ATTACHMENT_LOAD_OP_MAX_ENUM;

	switch (InLoadAction)
	{
	case ERenderTargetLoadAction::ELoad:		OutLoadAction = VK_ATTACHMENT_LOAD_OP_LOAD;			break;
	case ERenderTargetLoadAction::EClear:		OutLoadAction = VK_ATTACHMENT_LOAD_OP_CLEAR;		break;
	case ERenderTargetLoadAction::ENoAction:	OutLoadAction = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	break;
	default:																						break;
	}

	// Check for missing translation
	check(OutLoadAction != VK_ATTACHMENT_LOAD_OP_MAX_ENUM);
	return OutLoadAction;
}

static inline VkAttachmentStoreOp RenderTargetStoreActionToVulkan(ERenderTargetStoreAction InStoreAction)
{
	VkAttachmentStoreOp OutStoreAction = VK_ATTACHMENT_STORE_OP_MAX_ENUM;

	switch (InStoreAction)
	{
	case ERenderTargetStoreAction::EStore:		OutStoreAction = VK_ATTACHMENT_STORE_OP_STORE;		break;
	//#todo-rco: Temp until we have a better RenderPass system
	case ERenderTargetStoreAction::ENoAction:	OutStoreAction = VK_ATTACHMENT_STORE_OP_STORE/*DONT_CARE*/;	break;
	default:																						break;
	}

	// Check for missing translation
	check(OutStoreAction != VK_ATTACHMENT_STORE_OP_MAX_ENUM);
	return OutStoreAction;
}

inline VkFormat UEToVkFormat(EPixelFormat UEFormat, const bool bIsSRGB)
{
	VkFormat Format = (VkFormat)GPixelFormats[UEFormat].PlatformFormat;
	if (bIsSRGB && GMaxRHIFeatureLevel > ERHIFeatureLevel::ES2)
	{
		switch (Format)
		{
		case VK_FORMAT_B8G8R8A8_UNORM:				Format = VK_FORMAT_B8G8R8A8_SRGB; break;
		case VK_FORMAT_A8B8G8R8_UNORM_PACK32:		Format = VK_FORMAT_A8B8G8R8_SRGB_PACK32; break;
		case VK_FORMAT_R8_UNORM:					Format = ((GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1) ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8_SRGB); break;
		case VK_FORMAT_R8G8_UNORM:					Format = VK_FORMAT_R8G8_SRGB; break;
		case VK_FORMAT_R8G8B8_UNORM:				Format = VK_FORMAT_R8G8B8_SRGB; break;
		case VK_FORMAT_R8G8B8A8_UNORM:				Format = VK_FORMAT_R8G8B8A8_SRGB; break;
		case VK_FORMAT_BC1_RGB_UNORM_BLOCK:			Format = VK_FORMAT_BC1_RGB_SRGB_BLOCK; break;
		case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:		Format = VK_FORMAT_BC1_RGBA_SRGB_BLOCK; break;
		case VK_FORMAT_BC2_UNORM_BLOCK:				Format = VK_FORMAT_BC2_SRGB_BLOCK; break;
		case VK_FORMAT_BC3_UNORM_BLOCK:				Format = VK_FORMAT_BC3_SRGB_BLOCK; break;
		case VK_FORMAT_BC7_UNORM_BLOCK:				Format = VK_FORMAT_BC7_SRGB_BLOCK; break;
		case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:		Format = VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK; break;
		case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:	Format = VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK; break;
		case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:	Format = VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK; break;
		case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:		Format = VK_FORMAT_ASTC_4x4_SRGB_BLOCK; break;
		case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:		Format = VK_FORMAT_ASTC_5x4_SRGB_BLOCK; break;
		case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:		Format = VK_FORMAT_ASTC_5x5_SRGB_BLOCK; break;
		case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:		Format = VK_FORMAT_ASTC_6x5_SRGB_BLOCK; break;
		case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:		Format = VK_FORMAT_ASTC_6x6_SRGB_BLOCK; break;
		case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:		Format = VK_FORMAT_ASTC_8x5_SRGB_BLOCK; break;
		case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:		Format = VK_FORMAT_ASTC_8x6_SRGB_BLOCK; break;
		case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:		Format = VK_FORMAT_ASTC_8x8_SRGB_BLOCK; break;
		case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:		Format = VK_FORMAT_ASTC_10x5_SRGB_BLOCK; break;
		case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:		Format = VK_FORMAT_ASTC_10x6_SRGB_BLOCK; break;
		case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:		Format = VK_FORMAT_ASTC_10x8_SRGB_BLOCK; break;
		case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:		Format = VK_FORMAT_ASTC_10x10_SRGB_BLOCK; break;
		case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:		Format = VK_FORMAT_ASTC_12x10_SRGB_BLOCK; break;
		case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:		Format = VK_FORMAT_ASTC_12x12_SRGB_BLOCK; break;
//		case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:	Format = VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG; break;
//		case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:	Format = VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG; break;
//		case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:	Format = VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG; break;
//		case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:	Format = VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG; break;
		default:	break;
		}
	}

	return Format;
}

static inline VkFormat UEToVkFormat(EVertexElementType Type)
{
	switch (Type)
	{
	case VET_Float1:
		return VK_FORMAT_R32_SFLOAT;
	case VET_Float2:
		return VK_FORMAT_R32G32_SFLOAT;
	case VET_Float3:
		return VK_FORMAT_R32G32B32_SFLOAT;
	case VET_PackedNormal:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case VET_UByte4:
		return VK_FORMAT_R8G8B8A8_UINT;
	case VET_UByte4N:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case VET_Color:
		return VK_FORMAT_B8G8R8A8_UNORM;
	case VET_Short2:
		return VK_FORMAT_R16G16_SINT;
	case VET_Short4:
		return VK_FORMAT_R16G16B16A16_SINT;
	case VET_Short2N:
		return VK_FORMAT_R16G16_SNORM;
	case VET_Half2:
		return VK_FORMAT_R16G16_SFLOAT;
	case VET_Half4:
		return VK_FORMAT_R16G16B16A16_SFLOAT;
	case VET_Short4N:		// 4 X 16 bit word: normalized 
		return VK_FORMAT_R16G16B16A16_SNORM;
	case VET_UShort2:
		return VK_FORMAT_R16G16_UINT;
	case VET_UShort4:
		return VK_FORMAT_R16G16B16A16_UINT;
	case VET_UShort2N:		// 16 bit word normalized to (value/65535.0:value/65535.0:0:0:1)
		return VK_FORMAT_R16G16_UNORM;
	case VET_UShort4N:		// 4 X 16 bit word unsigned: normalized 
		return VK_FORMAT_R16G16B16A16_UNORM;
	case VET_Float4:
		return VK_FORMAT_R32G32B32A32_SFLOAT;
	case VET_URGB10A2N:
		return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
	default:
		break;
	}

	check(!"Undefined vertex-element format conversion");
	return VK_FORMAT_UNDEFINED;
}

static inline VkPrimitiveTopology UEToVulkanType(EPrimitiveType PrimitiveType)
{
	switch (PrimitiveType)
	{
	case PT_PointList:			return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	case PT_LineList:			return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case PT_TriangleList:		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	case PT_TriangleStrip:		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	default:
		break;
	}

	checkf(false, TEXT("Unsupported primitive type"));
	return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

namespace VulkanRHI
{
	static inline FString GetPipelineCacheFilename()
	{
		return FPaths::ProjectSavedDir() / TEXT("VulkanPSO.cache");
	}
}

#if 0
namespace FRCLog
{
	void Printf(const FString& S);
}
#endif


#ifndef VK_KHR_maintenance1
#define VK_KHR_maintenance1	0
#endif

#define SUPPORTS_MAINTENANCE_LAYER							VK_KHR_maintenance1

