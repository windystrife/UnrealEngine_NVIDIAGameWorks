// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanCommandWrappers.h: Wrap all Vulkan API functions so we can add our own 'layers'
=============================================================================*/

#pragma once 

namespace VulkanRHI
{
#if VULKAN_ENABLE_DUMP_LAYER
	VULKANRHI_API void PrintfBeginResult(const FString& String);
	void DevicePrintfBeginResult(VkDevice Device, const FString& String);
	void PrintfBegin(const FString& String);
	void DevicePrintfBegin(VkDevice Device, const FString& String);
	void CmdPrintfBegin(VkCommandBuffer CmdBuffer, const FString& String);
	void CmdPrintfBeginResult(VkCommandBuffer CmdBuffer, const FString& String);
	void PrintResult(VkResult Result);
	void PrintResultAndNamedHandle(VkResult Result, const TCHAR* HandleName, void* Handle);
	void PrintResultAndNamedHandles(VkResult Result, const TCHAR* HandleName, uint32 NumHandles, uint64* Handles);
	VULKANRHI_API void PrintResultAndPointer(VkResult Result, void* Handle);
	void PrintResultAndNamedHandle(VkResult Result, const TCHAR* HandleName, uint64 Handle);
	void PrintResultAndPointer(VkResult Result, uint64 Handle);
	void DumpPhysicalDeviceProperties(VkPhysicalDeviceMemoryProperties* Properties);
	void DumpAllocateMemory(VkDevice Device, const VkMemoryAllocateInfo* AllocateInfo, VkDeviceMemory* Memory);
	void DumpMemoryRequirements(VkMemoryRequirements* MemoryRequirements);
	void DumpCreateBuffer(VkDevice Device, const VkBufferCreateInfo* CreateInfo, VkBuffer* Buffer);
	void DumpCreateBufferView(VkDevice Device, const VkBufferViewCreateInfo* CreateInfo, VkBufferView* BufferView);
	void DumpCreateImage(VkDevice Device, const VkImageCreateInfo* CreateInfo, VkImage* Image);
	void DumpCreateImageResult(VkResult Result, const VkImageCreateInfo* CreateInfo, VkImage Image);
	void DumpDestroyImage(VkDevice Device, VkImage Image);
	void DumpCreateImageView(VkDevice Device, const VkImageViewCreateInfo* CreateInfo, VkImageView* ImageView);
	void DumpFenceCreate(VkDevice Device, const VkFenceCreateInfo* CreateInfo, VkFence* Fence);
	void DumpFenceList(uint32 FenceCount, const VkFence* Fences);
	void DumpSemaphoreCreate(VkDevice Device, const VkSemaphoreCreateInfo* CreateInfo, VkSemaphore* Semaphore);
	void DumpMappedMemoryRanges(uint32 MemoryRangeCount, const VkMappedMemoryRange* pMemoryRanges);
	void DumpResolveImage(VkCommandBuffer CommandBuffer, VkImage SrcImage, VkImageLayout SrcImageLayout, VkImage DstImage, VkImageLayout DstImageLayout, uint32 RegionCount, const VkImageResolve* Regions);
	void DumpFreeDescriptorSets(VkDevice Device, VkDescriptorPool DescriptorPool, uint32 DescriptorSetCount, const VkDescriptorSet* DescriptorSets);
	void DumpCreateInstance(const VkInstanceCreateInfo* CreateInfo, VkInstance* Instance);
	void DumpEnumeratePhysicalDevicesEpilog(uint32* PhysicalDeviceCount, VkPhysicalDevice* PhysicalDevices);
	void DumpCmdPipelineBarrier(VkCommandBuffer CommandBuffer, VkPipelineStageFlags SrcStageMask, VkPipelineStageFlags DstStageMask, VkDependencyFlags DependencyFlags, uint32 MemoryBarrierCount, const VkMemoryBarrier* MemoryBarriers, uint32 BufferMemoryBarrierCount, const VkBufferMemoryBarrier* BufferMemoryBarriers, uint32 ImageMemoryBarrierCount, const VkImageMemoryBarrier* ImageMemoryBarriers);
	void DumpCmdWaitEvents(VkCommandBuffer CommandBuffer, uint32 EventCount, const VkEvent* Events, VkPipelineStageFlags SrcStageMask, VkPipelineStageFlags DstStageMask, uint32 MemoryBarrierCount,
		const VkMemoryBarrier* pMemoryBarriers, uint32 BufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32 ImageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers);
	void DumpCreateDescriptorSetLayout(VkDevice Device, const VkDescriptorSetLayoutCreateInfo* CreateInfo, VkDescriptorSetLayout* SetLayout);
	void DumpAllocateDescriptorSets(VkDevice Device, const VkDescriptorSetAllocateInfo* AllocateInfo, VkDescriptorSet* DescriptorSets);
	void DumpCreateFramebuffer(VkDevice Device, const VkFramebufferCreateInfo* CreateInfo, VkFramebuffer* Framebuffer);
	void DumpCreateFramebufferResult(VkResult Result, const VkFramebufferCreateInfo* CreateInfo, VkFramebuffer Framebuffer);
	void DumpCreateRenderPass(VkDevice Device, const VkRenderPassCreateInfo* CreateInfo, VkRenderPass* RenderPass);
	void DumpCreateRenderPassResult(VkResult Result, const VkRenderPassCreateInfo* CreateInfo, VkRenderPass RenderPass);
	void DumpQueueSubmit(VkQueue Queue, uint32 SubmitCount, const VkSubmitInfo* Submits, VkFence Fence);
	void DumpCreateShaderModule(VkDevice Device, const VkShaderModuleCreateInfo* CreateInfo, VkShaderModule* ShaderModule);
	void DumpCreateDevice(VkPhysicalDevice PhysicalDevice, const VkDeviceCreateInfo* CreateInfo, VkDevice* Device);
	void DumpUpdateDescriptorSets(VkDevice Device, uint32 DescriptorWriteCount, const VkWriteDescriptorSet* DescriptorWrites, uint32 DescriptorCopyCount, const VkCopyDescriptorSet* DescriptorCopies);
	void DumpBeginCommandBuffer(VkCommandBuffer CommandBuffer, const VkCommandBufferBeginInfo* BeginInfo);
	void DumpCmdBeginRenderPass(VkCommandBuffer CommandBuffer, const VkRenderPassBeginInfo* RenderPassBegin, VkSubpassContents Contents);
	void DumpCmdBindVertexBuffers(VkCommandBuffer CommandBuffer, uint32 FirstBinding, uint32 BindingCount, const VkBuffer* Buffers, const VkDeviceSize* pOffsets);
	void DumpGetImageSubresourceLayout(VkDevice Device, VkImage Image, const VkImageSubresource* Subresource, VkSubresourceLayout* Layout);
	void DumpImageSubresourceLayout(VkSubresourceLayout* Layout);
	void DumpCmdCopyBufferToImage(VkCommandBuffer CommandBuffer, VkBuffer SrcBuffer, VkImage DstImage, VkImageLayout DstImageLayout, uint32 RegionCount, const VkBufferImageCopy* Regions);
	void DumpCmdCopyImageToBuffer(VkCommandBuffer CommandBuffer, VkImage SrcImage, VkImageLayout SrcImageLayout, VkBuffer DstBuffer, uint32 RegionCount, const VkBufferImageCopy* Regions);
	void DumpCmdCopyBuffer(VkCommandBuffer CommandBuffer, VkBuffer SrcBuffer, VkBuffer DstBuffer, uint32 RegionCount, const VkBufferCopy* Regions);
	void DumpCmdBlitImage(VkCommandBuffer CommandBuffer, VkImage SrcImage, VkImageLayout SrcImageLayout, VkImage DstImage, VkImageLayout DstImageLayout, uint32 RegionCount, const VkImageBlit* Regions, VkFilter Filter);
	void DumpCreatePipelineCache(VkDevice Device, const VkPipelineCacheCreateInfo* CreateInfo, VkPipelineCache* PipelineCache);
	void DumpCreateCommandPool(VkDevice Device, const VkCommandPoolCreateInfo* CreateInfo, VkCommandPool* CommandPool);
	void DumpCreateQueryPool(VkDevice Device, const VkQueryPoolCreateInfo* CreateInfo, VkQueryPool* QueryPool);
	void DumpCreatePipelineLayout(VkDevice Device, const VkPipelineLayoutCreateInfo* CreateInfo, VkPipelineLayout* PipelineLayout);
	void DumpCreateDescriptorPool(VkDevice Device, const VkDescriptorPoolCreateInfo* CreateInfo, VkDescriptorPool* DescriptorPool);
	void DumpCreateSampler(VkDevice Device, const VkSamplerCreateInfo* CreateInfo, VkSampler* Sampler);
	void DumpGetPhysicalDeviceFeatures(VkPhysicalDevice PhysicalDevice, VkPhysicalDeviceFeatures* Features);
	void DumpPhysicalDeviceFeatures(VkPhysicalDeviceFeatures* Features);
	void DumpBindDescriptorSets(VkCommandBuffer CommandBuffer, VkPipelineBindPoint PipelineBindPoint, VkPipelineLayout Layout, uint32 FirstSet, uint32 DescriptorSetCount, const VkDescriptorSet* DescriptorSets, uint32 DynamicOffsetCount, const uint32* DynamicOffsets);
	void DumpSwapChainImages(VkResult Result, uint32* SwapchainImageCount, VkImage* SwapchainImages);
	void DumpCmdClearAttachments(VkCommandBuffer CommandBuffer, uint32 AttachmentCount, const VkClearAttachment* Attachments, uint32 RectCount, const VkClearRect* Rects);
	void DumpCmdClearColorImage(VkCommandBuffer CommandBuffer, VkImage Image, VkImageLayout ImageLayout, const VkClearColorValue* ColorValue, uint32 RangeCount, const VkImageSubresourceRange* Ranges);
	void DumpCmdClearDepthStencilImage(VkCommandBuffer CommandBuffer, VkImage Image, VkImageLayout ImageLayout, const VkClearDepthStencilValue* DepthStencil, uint32 RangeCount, const VkImageSubresourceRange* Ranges);
	void DumpQueuePresent(VkQueue Queue, const VkPresentInfoKHR* PresentInfo);
	void DumpCreateGraphicsPipelines(VkDevice Device, VkPipelineCache PipelineCache, uint32 CreateInfoCount, const VkGraphicsPipelineCreateInfo* CreateInfos, VkPipeline* Pipelines);
	void TrackImageViewAdd(VkImageView View, const VkImageViewCreateInfo* CreateInfo);
	void TrackImageViewRemove(VkImageView View);
	void TrackBufferViewAdd(VkBufferView View, const VkBufferViewCreateInfo* pCreateInfo);
	void TrackBufferViewRemove(VkBufferView View);
#else
#define FlushDebugWrapperLog()
#define DevicePrintfBeginResult(d, x)
#define PrintfBeginResult(x)
#define DevicePrintfBegin(d, x)
#define CmdPrintfBegin(c, s)
#define CmdPrintfBeginResult(c, s)
#define PrintfBegin(x)
#define PrintResult(x)
#define PrintResultAndNamedHandle(r, n, h)
#define PrintResultAndNamedHandles(r, hn, nh, h)
#define PrintResultAndPointer(r, h)
#define DumpPhysicalDeviceProperties(x)
#define DumpAllocateMemory(d, i, m)
#define DumpMemoryRequirements(x)
#define DumpCreateBuffer(d, i, b)
#define DumpCreateBufferView(d, i, v)
#define DumpCreateImage(d, x, i)
#define DumpCreateImageResult(r, ci, i);
#define DumpDestroyImage(d, i)
#define DumpCreateImageView(d, i, v)
#define DumpFenceCreate(d, x, f)
#define DumpFenceList(c, p)
#define DumpSemaphoreCreate(d, i, s)
#define DumpMappedMemoryRanges(c, p)
#define DumpResolveImage(c, si, sil, di, dil, rc, r)
#define DumpFreeDescriptorSets(d, p, c, s)
#define DumpCreateInstance(c, i)
#define DumpEnumeratePhysicalDevicesEpilog(c, d)
#define DumpCmdPipelineBarrier(c, sm, dm, df, mc, mb, bc, bb, ic, ib)
#define DumpCmdWaitEvents(cb, ec, e, ssm, dsm, mbc, mb, bmbc, bmb, imbc, imb)
#define DumpCreateDescriptorSetLayout(d, i, s)
#define DumpAllocateDescriptorSets(d, i, s)
#define DumpCreateFramebuffer(d, i, f)
#define DumpCreateFramebufferResult(r, i, f)
#define DumpCreateRenderPass(d, i, r)
#define DumpCreateRenderPassResult(r, i, rp)
#define DumpQueueSubmit(q, c, s, f)
#define DumpCreateShaderModule(d, i, s)
#define DumpCreateDevice(p, i, d)
#define DumpUpdateDescriptorSets(d, wc, w, cc, c)
#define DumpBeginCommandBuffer(c, i)
#define DumpCmdBeginRenderPass(c, b, cont)
#define DumpCmdBindVertexBuffers(cb, f, bc, b, o)
#define DumpGetImageSubresourceLayout(d, i, s, l)
#define DumpImageSubresourceLayout(l)
#define DumpCmdCopyBufferToImage(cb, sb, di, dil, rc, r)
#define DumpCmdCopyImageToBuffer(cb, sb, di, dil, rc, r)
#define DumpCmdCopyBuffer(cb, sb, db, rc, r)
#define DumpCmdBlitImage(cb, si, sil, di, dil, rc, r, f)
#define DumpCreatePipelineCache(d, i, pc)
#define DumpCreateCommandPool(d, i, c)
#define DumpCreateQueryPool(d, i, q)
#define DumpCreatePipelineLayout(d, i, p)
#define DumpCreateDescriptorPool(d, i, dp)
#define DumpCreateSampler(d, i, s)
#define DumpGetPhysicalDeviceFeatures(pd, f)
#define DumpPhysicalDeviceFeatures(f)
#define DumpBindDescriptorSets(c, pbp, l, fs, dsc, ds, doc, do)
#define DumpSwapChainImages(r, c, i)
#define DumpCmdClearAttachments(c, ac, a, rc, r)
#define DumpCmdClearColorImage(c, i, il, ds, rc, r)
#define DumpCmdClearDepthStencilImage(c, i, il, ds, rc, r)
#define DumpQueuePresent(q, i)
#define DumpCreateGraphicsPipelines(d, pc, cic, ci, p)
#define TrackImageViewAdd(v, ci)
#define TrackImageViewRemove(v)
#define TrackBufferViewAdd(v, b)
#define TrackBufferViewRemove(v)
#endif

	FORCEINLINE_DEBUGGABLE VkResult  vkCreateInstance(const VkInstanceCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkInstance* Instance)
	{
		DumpCreateInstance(CreateInfo, Instance);

		VkResult Result = VULKANAPINAMESPACE::vkCreateInstance(CreateInfo, Allocator, Instance);

		PrintResultAndNamedHandle(Result, TEXT("Instance"), *Instance);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyInstance(VkInstance Instance, const VkAllocationCallbacks* Allocator)
	{
		PrintfBegin(FString::Printf(TEXT("vkDestroyInstance(Instance=%p)"), Instance));

		VULKANAPINAMESPACE::vkDestroyInstance(Instance, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkEnumeratePhysicalDevices(VkInstance Instance, uint32* PhysicalDeviceCount, VkPhysicalDevice* PhysicalDevices)
	{
		PrintfBegin(FString::Printf(TEXT("vkEnumeratePhysicalDevices(Instance=%p, Count=%p, Devices=%p)"), Instance, PhysicalDeviceCount, PhysicalDevices));

		VkResult Result = VULKANAPINAMESPACE::vkEnumeratePhysicalDevices(Instance, PhysicalDeviceCount, PhysicalDevices);

		DumpEnumeratePhysicalDevicesEpilog(PhysicalDeviceCount, PhysicalDevices);

		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkGetPhysicalDeviceFeatures(VkPhysicalDevice PhysicalDevice, VkPhysicalDeviceFeatures* Features)
	{
		DumpGetPhysicalDeviceFeatures(PhysicalDevice, Features);
		VULKANAPINAMESPACE::vkGetPhysicalDeviceFeatures(PhysicalDevice, Features);
		DumpPhysicalDeviceFeatures(Features);
	}

	static FORCEINLINE_DEBUGGABLE void  vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice PhysicalDevice, VkFormat Format, VkFormatProperties* FormatProperties)
	{
		PrintfBegin(FString::Printf(TEXT("vkGetPhysicalDeviceFormatProperties(PhysicalDevice=%p, Format=%d, FormatProperties=%p)[...]"), PhysicalDevice, (int32)Format, FormatProperties));

		VULKANAPINAMESPACE::vkGetPhysicalDeviceFormatProperties(PhysicalDevice, Format, FormatProperties);
	}

#if 0
	static FORCEINLINE_DEBUGGABLE VkResult  vkGetPhysicalDeviceImageFormatProperties(
		VkPhysicalDevice                            physicalDevice,
		VkFormat                                    format,
		VkImageType                                 type,
		VkImageTiling                               tiling,
		VkImageUsageFlags                           usage,
		VkImageCreateFlags                          flags,
		VkImageFormatProperties*                    pImageFormatProperties);
#endif
	static FORCEINLINE_DEBUGGABLE void  vkGetPhysicalDeviceProperties(VkPhysicalDevice PhysicalDevice, VkPhysicalDeviceProperties* Properties)
	{
		PrintfBegin(FString::Printf(TEXT("vkGetPhysicalDeviceProperties(PhysicalDevice=%p, Properties=%p)[...]"), PhysicalDevice, Properties));

		VULKANAPINAMESPACE::vkGetPhysicalDeviceProperties(PhysicalDevice, Properties);
	}

	static FORCEINLINE_DEBUGGABLE void  vkGetPhysicalDeviceProperties2KHR(VkPhysicalDevice PhysicalDevice, VkPhysicalDeviceProperties2KHR* Properties)
	{
		PrintfBegin(FString::Printf(TEXT("vkGetPhysicalDeviceProperties2KHR(PhysicalDevice=%p, Properties=%p)[...]"), PhysicalDevice, Properties));

		extern PFN_vkGetPhysicalDeviceProperties2KHR GVkGetPhysicalDeviceProperties2KHR;
		
		if (GVkGetPhysicalDeviceProperties2KHR != nullptr)
		{
			GVkGetPhysicalDeviceProperties2KHR(PhysicalDevice, Properties);
		}
	}

	static FORCEINLINE_DEBUGGABLE void  vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice PhysicalDevice, uint32* QueueFamilyPropertyCount, VkQueueFamilyProperties* QueueFamilyProperties)
	{
		PrintfBeginResult(FString::Printf(TEXT("vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice=%p, QueueFamilyPropertyCount=%p, QueueFamilyProperties=%p)[...]"), PhysicalDevice, QueueFamilyPropertyCount, QueueFamilyProperties));

		VULKANAPINAMESPACE::vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, QueueFamilyPropertyCount, QueueFamilyProperties);
	}

	static FORCEINLINE_DEBUGGABLE void  vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice PhysicalDevice, VkPhysicalDeviceMemoryProperties* MemoryProperties)
	{
		PrintfBegin(FString::Printf(TEXT("vkGetPhysicalDeviceMemoryProperties(OutProp=%p)[...]"), MemoryProperties));

		VULKANAPINAMESPACE::vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, MemoryProperties);

		DumpPhysicalDeviceProperties(MemoryProperties);
	}

	static FORCEINLINE_DEBUGGABLE PFN_vkVoidFunction  vkGetInstanceProcAddr(VkInstance Instance, const char* Name)
	{
		PrintfBeginResult(FString::Printf(TEXT("vkGetInstanceProcAddr(Instance=%p, Name=%s)[...]"), Instance, ANSI_TO_TCHAR(Name)));

		PFN_vkVoidFunction Function = VULKANAPINAMESPACE::vkGetInstanceProcAddr(Instance, Name);
		PrintResultAndPointer(VK_SUCCESS, (void*)Function);
		return Function;
	}

	static FORCEINLINE_DEBUGGABLE PFN_vkVoidFunction  vkGetDeviceProcAddr(VkDevice Device, const char* Name)
	{
		PrintfBeginResult(FString::Printf(TEXT("vkGetDeviceProcAddr(Device=%p, Name=%s)[...]"), Device, ANSI_TO_TCHAR(Name)));

		PFN_vkVoidFunction Function = VULKANAPINAMESPACE::vkGetDeviceProcAddr(Device, Name);
		PrintResultAndPointer(VK_SUCCESS, (void*)Function);
		return Function;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateDevice(VkPhysicalDevice PhysicalDevice, const VkDeviceCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkDevice* Device)
	{
		DumpCreateDevice(PhysicalDevice, CreateInfo, Device);

		VkResult Result = VULKANAPINAMESPACE::vkCreateDevice(PhysicalDevice, CreateInfo, Allocator, Device);

		PrintResultAndNamedHandle(Result, TEXT("Device"), *Device);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyDevice(VkDevice Device, const VkAllocationCallbacks* Allocator)
	{
		PrintfBegin(FString::Printf(TEXT("vkDestroyDevice(Device=%p)"), Device));

		VULKANAPINAMESPACE::vkDestroyDevice(Device, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkEnumerateInstanceExtensionProperties(const char* LayerName, uint32* PropertyCount, VkExtensionProperties* Properties)
	{
		PrintfBeginResult(FString::Printf(TEXT("vkEnumerateInstanceExtensionProperties(LayerName=%s, PropertyCount=%p, Properties=%p)[...]"), ANSI_TO_TCHAR(LayerName), PropertyCount, Properties));

		VkResult Result = VULKANAPINAMESPACE::vkEnumerateInstanceExtensionProperties(LayerName, PropertyCount, Properties);
		PrintResultAndPointer(Result, (void*)(uint64)PropertyCount);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkEnumerateDeviceExtensionProperties(VkPhysicalDevice PhysicalDevice, const char* LayerName, uint32* PropertyCount, VkExtensionProperties* Properties)
	{
		PrintfBeginResult(FString::Printf(TEXT("vkEnumerateDeviceExtensionProperties(Device=%p, LayerName=%s, PropertyCount=%p, Properties=%p)[...]"), PhysicalDevice, ANSI_TO_TCHAR(LayerName), PropertyCount, Properties));

		VkResult Result = VULKANAPINAMESPACE::vkEnumerateDeviceExtensionProperties(PhysicalDevice, LayerName, PropertyCount, Properties);
		PrintResultAndPointer(Result, (void*)(uint64)PropertyCount);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkEnumerateInstanceLayerProperties(uint32* PropertyCount, VkLayerProperties* Properties)
	{
		PrintfBeginResult(FString::Printf(TEXT("vkEnumerateInstanceLayerProperties(PropertyCount=%p, Properties=%p)[...]"), PropertyCount, Properties));

		VkResult Result = VULKANAPINAMESPACE::vkEnumerateInstanceLayerProperties(PropertyCount, Properties);
		PrintResultAndPointer(Result, (void*)(uint64)PropertyCount);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkEnumerateDeviceLayerProperties(VkPhysicalDevice PhysicalDevice, uint32* PropertyCount, VkLayerProperties* Properties)
	{
		PrintfBeginResult(FString::Printf(TEXT("vkEnumerateDeviceLayerProperties(Device=%p, PropertyCount=%p, Properties=%p)[...]"), PhysicalDevice, PropertyCount, Properties));

		VkResult Result = VULKANAPINAMESPACE::vkEnumerateDeviceLayerProperties(PhysicalDevice, PropertyCount, Properties);
		PrintResultAndPointer(Result, (void*)(uint64)PropertyCount);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkGetDeviceQueue(VkDevice Device, uint32 QueueFamilyIndex, uint32 QueueIndex, VkQueue* Queue)
	{
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkGetDeviceQueue(QueueFamilyIndex=%d, QueueIndex=%d, OutQueue=%p)"), QueueFamilyIndex, QueueIndex, Queue));

		VULKANAPINAMESPACE::vkGetDeviceQueue(Device, QueueFamilyIndex, QueueIndex, Queue);

		PrintResultAndNamedHandle(VK_SUCCESS, TEXT("Queue"), *Queue);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkQueueSubmit(VkQueue Queue, uint32 SubmitCount, const VkSubmitInfo* Submits, VkFence Fence)
	{
		DumpQueueSubmit(Queue, SubmitCount, Submits, Fence);

		VkResult Result = VULKANAPINAMESPACE::vkQueueSubmit(Queue, SubmitCount, Submits, Fence);

		PrintResult(Result);
		return Result;
	}
	static FORCEINLINE_DEBUGGABLE VkResult  vkQueueWaitIdle(VkQueue Queue)
	{
		PrintfBeginResult(FString::Printf(TEXT("vkQueueWaitIdle(Queue=%p)"), Queue));

		VkResult Result = VULKANAPINAMESPACE::vkQueueWaitIdle(Queue);

		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkDeviceWaitIdle(VkDevice Device)
	{
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkDeviceWaitIdle()")));

		VkResult Result = VULKANAPINAMESPACE::vkDeviceWaitIdle(Device);

		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkAllocateMemory(VkDevice Device, const VkMemoryAllocateInfo* AllocateInfo, const VkAllocationCallbacks* Allocator, VkDeviceMemory* Memory)
	{
		DumpAllocateMemory(Device, AllocateInfo, Memory);

		VkResult Result = VULKANAPINAMESPACE::vkAllocateMemory(Device, AllocateInfo, Allocator, Memory);

		PrintResultAndNamedHandle(Result, TEXT("DevMem"), *Memory);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkFreeMemory(VkDevice Device, VkDeviceMemory Memory, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkFreeMemory(DevMem=%p)"), Memory));

		VULKANAPINAMESPACE::vkFreeMemory(Device, Memory, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkMapMemory(VkDevice Device, VkDeviceMemory Memory, VkDeviceSize Offset, VkDeviceSize Size, VkMemoryMapFlags Flags, void** Data)
	{
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkMapMemory(DevMem=%p, Off=%d, Size=%d, Flags=0x%x, OutData=%p)"), Memory, (uint32)Offset, (uint32)Size, Flags, Data));

		VkResult Result = VULKANAPINAMESPACE::vkMapMemory(Device, Memory, Offset, Size, Flags, Data);

		PrintResultAndPointer(Result, *Data);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkUnmapMemory(VkDevice Device, VkDeviceMemory Memory)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkUnmapMemory(DevMem=%p)"), Memory));
		VULKANAPINAMESPACE::vkUnmapMemory(Device, Memory);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkFlushMappedMemoryRanges(VkDevice Device, uint32 MemoryRangeCount, const VkMappedMemoryRange* MemoryRanges)
	{
		DumpMappedMemoryRanges(MemoryRangeCount, MemoryRanges);
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkFlushMappedMemoryRanges(Count=%d, Ranges=%p)"), MemoryRangeCount, MemoryRanges));

		VkResult Result = VULKANAPINAMESPACE::vkFlushMappedMemoryRanges(Device, MemoryRangeCount, MemoryRanges);

		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkInvalidateMappedMemoryRanges(VkDevice Device, uint32 MemoryRangeCount, const VkMappedMemoryRange* MemoryRanges)
	{
		DumpMappedMemoryRanges(MemoryRangeCount, MemoryRanges);
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkInvalidateMappedMemoryRanges(Count=%d, Ranges=%p)"), MemoryRangeCount, MemoryRanges));

		VkResult Result = VULKANAPINAMESPACE::vkInvalidateMappedMemoryRanges(Device, MemoryRangeCount, MemoryRanges);

		PrintResult(Result);
		return Result;
	}
#if 0
	static FORCEINLINE_DEBUGGABLE void  vkGetDeviceMemoryCommitment(
		VkDevice                                    Device,
		VkDeviceMemory                              Memory,
		VkDeviceSize*                               pCommittedMemoryInBytes);
#endif
	static FORCEINLINE_DEBUGGABLE VkResult  vkBindBufferMemory(VkDevice Device, VkBuffer Buffer, VkDeviceMemory Memory, VkDeviceSize MemoryOffset)
	{
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkBindBufferMemory(Buffer=%p, DevMem=%p, MemOff=%d)"), Buffer, Memory, (uint32)MemoryOffset));

		VkResult Result = VULKANAPINAMESPACE::vkBindBufferMemory(Device, Buffer, Memory, MemoryOffset);

		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkBindImageMemory(VkDevice Device, VkImage Image, VkDeviceMemory Memory, VkDeviceSize MemoryOffset)
	{
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkBindImageMemory(Image=%p, DevMem=%p, MemOff=%d)"), Image, Memory, (uint32)MemoryOffset));

		VkResult Result = VULKANAPINAMESPACE::vkBindImageMemory(Device, Image, Memory, MemoryOffset);

		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkGetBufferMemoryRequirements(VkDevice Device, VkBuffer Buffer, VkMemoryRequirements* MemoryRequirements)
	{
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkGetBufferMemoryRequirements(Buffer=%p, OutReq=%p)"), Buffer, MemoryRequirements));

		VULKANAPINAMESPACE::vkGetBufferMemoryRequirements(Device, Buffer, MemoryRequirements);

		DumpMemoryRequirements(MemoryRequirements);
	}

	static FORCEINLINE_DEBUGGABLE void  vkGetImageMemoryRequirements(VkDevice Device, VkImage Image, VkMemoryRequirements* MemoryRequirements)
	{
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkGetImageMemoryRequirements(Image=%p, OutReq=%p)"), Image, MemoryRequirements));

		VULKANAPINAMESPACE::vkGetImageMemoryRequirements(Device, Image, MemoryRequirements);

		DumpMemoryRequirements(MemoryRequirements);
	}

#if 0
	static FORCEINLINE_DEBUGGABLE void  vkGetImageSparseMemoryRequirements(
		VkDevice                                    Device,
		VkImage                                     Image,
		uint32*                                   pSparseMemoryRequirementCount,
		VkSparseImageMemoryRequirements*            pSparseMemoryRequirements);

	static FORCEINLINE_DEBUGGABLE void  vkGetPhysicalDeviceSparseImageFormatProperties(
		VkPhysicalDevice                            physicalDevice,
		VkFormat                                    format,
		VkImageType                                 type,
		VkSampleCountFlagBits                       samples,
		VkImageUsageFlags                           usage,
		VkImageTiling                               tiling,
		uint32*                                   pPropertyCount,
		VkSparseImageFormatProperties*              pProperties);

	static FORCEINLINE_DEBUGGABLE VkResult  vkQueueBindSparse(
		VkQueue                                     queue,
		uint32                                    bindInfoCount,
		const VkBindSparseInfo*                     pBindInfo,
		VkFence                                     Fence);
#endif
	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateFence(VkDevice Device, const VkFenceCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkFence* Fence)
	{
		DumpFenceCreate(Device, CreateInfo, Fence);

		VkResult Result = VULKANAPINAMESPACE::vkCreateFence(Device, CreateInfo, Allocator, Fence);

		PrintResultAndNamedHandle(Result, TEXT("Fence"), *Fence);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyFence(VkDevice Device, VkFence Fence, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroyFence(Fence=%p)"), Fence));

		VULKANAPINAMESPACE::vkDestroyFence(Device, Fence, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkResetFences(VkDevice Device, uint32 FenceCount, const VkFence* Fences)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkResetFences(Count=%d, Fences=%p)"), FenceCount, Fences));
		DumpFenceList(FenceCount, Fences);

		VkResult Result = VULKANAPINAMESPACE::vkResetFences(Device, FenceCount, Fences);

		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkGetFenceStatus(VkDevice Device, VkFence Fence)
	{
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkGetFenceStatus(Fence=%p)"), Fence));

		VkResult Result = VULKANAPINAMESPACE::vkGetFenceStatus(Device, Fence);

		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkWaitForFences(VkDevice Device, uint32 FenceCount, const VkFence* Fences, VkBool32 bWaitAll, uint64_t Timeout)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkWaitForFences(Count=%p, Fences=%d, WaitAll=%d, Timeout=%p)"), FenceCount, Fences, (uint32)bWaitAll, Timeout));
		DumpFenceList(FenceCount, Fences);

		VkResult Result = VULKANAPINAMESPACE::vkWaitForFences(Device, FenceCount, Fences, bWaitAll, Timeout);

		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateSemaphore(VkDevice Device, const VkSemaphoreCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkSemaphore* Semaphore)
	{
		DumpSemaphoreCreate(Device, CreateInfo, Semaphore);

		VkResult Result = VULKANAPINAMESPACE::vkCreateSemaphore(Device, CreateInfo, Allocator, Semaphore);

		PrintResultAndNamedHandle(Result, TEXT("Semaphore"), *Semaphore);
		return Result;
	}


	static FORCEINLINE_DEBUGGABLE void  vkDestroySemaphore(VkDevice Device, VkSemaphore Semaphore, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroySemaphore(Semaphore=%p)"), Semaphore));

		VULKANAPINAMESPACE::vkDestroySemaphore(Device, Semaphore, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateEvent(VkDevice Device, const VkEventCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkEvent* Event)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkCreateEvent(CreateInfo=0x%016llx%s, OutEvent=0x%016llx)"), CreateInfo, Event));

		VkResult Result = VULKANAPINAMESPACE::vkCreateEvent(Device, CreateInfo, Allocator, Event);

		PrintResultAndNamedHandle(Result, TEXT("Event"), *Event);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyEvent(VkDevice Device, VkEvent Event, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroyEvent(Event=%p)"), Event));

		VULKANAPINAMESPACE::vkDestroyEvent(Device, Event, Allocator);
	}
#if 0
	static FORCEINLINE_DEBUGGABLE VkResult  vkGetEventStatus(
		VkDevice                                    Device,
		VkEvent                                     event);

	static FORCEINLINE_DEBUGGABLE VkResult  vkSetEvent(
		VkDevice                                    Device,
		VkEvent                                     event);

	static FORCEINLINE_DEBUGGABLE VkResult  vkResetEvent(
		VkDevice                                    Device,
		VkEvent                                     event);
#endif
	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateQueryPool(VkDevice Device, const VkQueryPoolCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkQueryPool* QueryPool)
	{
		DumpCreateQueryPool(Device, CreateInfo, QueryPool);

		VkResult Result = VULKANAPINAMESPACE::vkCreateQueryPool(Device, CreateInfo, Allocator, QueryPool);

		PrintResultAndNamedHandle(Result, TEXT("QueryPool"), *QueryPool);

		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyQueryPool(VkDevice Device, VkQueryPool QueryPool, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroyQueryPool(QueryPool=%p)"), QueryPool));

		VULKANAPINAMESPACE::vkDestroyQueryPool(Device, QueryPool, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkGetQueryPoolResults(VkDevice Device, VkQueryPool QueryPool,
		uint32 FirstQuery, uint32 QueryCount, size_t DataSize, void* Data, VkDeviceSize Stride, VkQueryResultFlags Flags)
	{
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkGetQueryPoolResults(QueryPool=%p, FirstQuery=%d, QueryCount=%d, DataSize=%d, Data=%p, Stride=%d, Flags=%d)[...]"), QueryPool, FirstQuery, QueryCount, (int32)DataSize, Data, Stride, Flags));

		VkResult Result = VULKANAPINAMESPACE::vkGetQueryPoolResults(Device, QueryPool, FirstQuery, QueryCount, DataSize, Data, Stride, Flags);

		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateBuffer(VkDevice Device, const VkBufferCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkBuffer* Buffer)
	{
		DumpCreateBuffer(Device, CreateInfo, Buffer);

		VkResult Result = VULKANAPINAMESPACE::vkCreateBuffer(Device, CreateInfo, Allocator, Buffer);

		PrintResultAndNamedHandle(Result, TEXT("Buffer"), *Buffer);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyBuffer(VkDevice Device, VkBuffer Buffer, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroyBuffer(Buffer=%p)"), Buffer));

		VULKANAPINAMESPACE::vkDestroyBuffer(Device, Buffer, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateBufferView(VkDevice Device, const VkBufferViewCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkBufferView* View)
	{
		DumpCreateBufferView(Device, CreateInfo, View);
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkCreateBufferView(Info=%p, OutView=%p)"), CreateInfo, View));

		VkResult Result = VULKANAPINAMESPACE::vkCreateBufferView(Device, CreateInfo, Allocator, View);

		TrackBufferViewAdd(*View, CreateInfo);

		PrintResultAndNamedHandle(Result, TEXT("BufferView"), *View);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyBufferView(VkDevice Device, VkBufferView BufferView, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroyBufferView(BufferView=%p)"), BufferView));

		TrackBufferViewRemove(BufferView);

		VULKANAPINAMESPACE::vkDestroyBufferView(Device, BufferView, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateImage(VkDevice Device, const VkImageCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkImage* Image)
	{
		DumpCreateImage(Device, CreateInfo, Image);

		VkResult Result = VULKANAPINAMESPACE::vkCreateImage(Device, CreateInfo, Allocator, Image);

		DumpCreateImageResult(Result, CreateInfo, *Image);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyImage(VkDevice Device, VkImage Image, const VkAllocationCallbacks* Allocator)
	{
		DumpDestroyImage(Device, Image);
		VULKANAPINAMESPACE::vkDestroyImage(Device, Image, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE void  vkGetImageSubresourceLayout(VkDevice Device, VkImage Image, const VkImageSubresource* Subresource, VkSubresourceLayout* Layout)
	{
		DumpGetImageSubresourceLayout(Device, Image, Subresource, Layout);

		VULKANAPINAMESPACE::vkGetImageSubresourceLayout(Device, Image, Subresource, Layout);

		DumpImageSubresourceLayout(Layout);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateImageView(VkDevice Device, const VkImageViewCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkImageView* View)
	{
		DumpCreateImageView(Device, CreateInfo, View);

		VkResult Result = VULKANAPINAMESPACE::vkCreateImageView(Device, CreateInfo, Allocator, View);
		TrackImageViewAdd(*View, CreateInfo);
		PrintResultAndNamedHandle(Result, TEXT("ImageView"), *View);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyImageView(VkDevice Device, VkImageView ImageView, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroyImageView(ImageView=%p)"), ImageView));
		TrackImageViewRemove(ImageView);
		VULKANAPINAMESPACE::vkDestroyImageView(Device, ImageView, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateShaderModule(VkDevice Device, const VkShaderModuleCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkShaderModule* ShaderModule)
	{
		DumpCreateShaderModule(Device, CreateInfo, ShaderModule);

		VkResult Result = VULKANAPINAMESPACE::vkCreateShaderModule(Device, CreateInfo, Allocator, ShaderModule);

		PrintResultAndNamedHandle(Result, TEXT("ShaderModule"), *ShaderModule);

		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyShaderModule(VkDevice Device, VkShaderModule ShaderModule, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroyShaderModule(ShaderModule=%p)"), ShaderModule));

		VULKANAPINAMESPACE::vkDestroyShaderModule(Device, ShaderModule, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreatePipelineCache(VkDevice Device, const VkPipelineCacheCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkPipelineCache* PipelineCache)
	{
		DumpCreatePipelineCache(Device, CreateInfo, PipelineCache);

		VkResult Result = VULKANAPINAMESPACE::vkCreatePipelineCache(Device, CreateInfo, Allocator, PipelineCache);

		PrintResultAndNamedHandle(Result, TEXT("PipelineCache"), *PipelineCache);

		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyPipelineCache(VkDevice Device, VkPipelineCache PipelineCache, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroyPipelineCache(PipelineCache=%p)"), PipelineCache));

		VULKANAPINAMESPACE::vkDestroyPipelineCache(Device, PipelineCache, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkGetPipelineCacheData(
		VkDevice                                    Device,
		VkPipelineCache                             PipelineCache,
		size_t*                                     DataSize,
		void*                                       Data)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkGetPipelineCacheData(PipelineCache=%p, DataSize=%d, [Data])"), PipelineCache, DataSize));
		VkResult Result = VULKANAPINAMESPACE::vkGetPipelineCacheData(Device, PipelineCache, DataSize, Data);
		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkMergePipelineCaches(
		VkDevice                                    Device,
		VkPipelineCache                             DestCache,
		uint32                                    SourceCacheCount,
		const VkPipelineCache*                      SrcCaches)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkMergePipelineCaches(DestCache=%p, SourceCacheCount=%d, [SrcCaches])"), DestCache, SourceCacheCount));
		VkResult Result = VULKANAPINAMESPACE::vkMergePipelineCaches(Device, DestCache, SourceCacheCount, SrcCaches);
		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateGraphicsPipelines(VkDevice Device, VkPipelineCache PipelineCache, uint32 CreateInfoCount, const VkGraphicsPipelineCreateInfo* CreateInfos, const VkAllocationCallbacks* Allocator, VkPipeline* Pipelines)
	{
		DumpCreateGraphicsPipelines(Device, PipelineCache, CreateInfoCount, CreateInfos, Pipelines);

		VkResult Result = VULKANAPINAMESPACE::vkCreateGraphicsPipelines(Device, PipelineCache, CreateInfoCount, CreateInfos, Allocator, Pipelines);

		//#todo-rco: Multiple pipelines!
		PrintResultAndNamedHandle(Result, TEXT("Pipeline"), Pipelines[0]);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateComputePipelines(VkDevice Device, VkPipelineCache PipelineCache, uint32 CreateInfoCount, const VkComputePipelineCreateInfo* CreateInfos, const VkAllocationCallbacks* Allocator, VkPipeline* Pipelines)
	{
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkCreateComputePipelines(PipelineCache=%p, CreateInfoCount=%d, CreateInfos=%p, OutPipelines=%p)[...]"), PipelineCache, CreateInfoCount, CreateInfos, Pipelines));

		VkResult Result = VULKANAPINAMESPACE::vkCreateComputePipelines(Device, PipelineCache, CreateInfoCount, CreateInfos, Allocator, Pipelines);

		//#todo-rco: Multiple pipelines!
		PrintResultAndNamedHandle(Result, TEXT("Pipeline"), Pipelines[0]);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyPipeline(VkDevice Device, VkPipeline Pipeline, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroyPipeline(Pipeline=%p)"), Pipeline));

		VULKANAPINAMESPACE::vkDestroyPipeline(Device, Pipeline, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreatePipelineLayout(VkDevice Device, const VkPipelineLayoutCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkPipelineLayout* PipelineLayout)
	{
		DumpCreatePipelineLayout(Device, CreateInfo, PipelineLayout);

		VkResult Result = VULKANAPINAMESPACE::vkCreatePipelineLayout(Device, CreateInfo, Allocator, PipelineLayout);

		PrintResultAndNamedHandle(Result, TEXT("PipelineLayout"), *PipelineLayout);

		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyPipelineLayout(VkDevice Device, VkPipelineLayout PipelineLayout, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroyPipelineLayout(PipelineLayout=%p)"), PipelineLayout));

		VULKANAPINAMESPACE::vkDestroyPipelineLayout(Device, PipelineLayout, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateSampler(VkDevice Device, const VkSamplerCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkSampler* Sampler)
	{
		DumpCreateSampler(Device, CreateInfo, Sampler);

		VkResult Result = VULKANAPINAMESPACE::vkCreateSampler(Device, CreateInfo, Allocator, Sampler);

		PrintResultAndNamedHandle(Result, TEXT("Sampler"), *Sampler);

		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroySampler(VkDevice Device, VkSampler Sampler, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroySampler(Sampler=%p)"), Sampler));

		VULKANAPINAMESPACE::vkDestroySampler(Device, Sampler, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateDescriptorSetLayout(VkDevice Device, const VkDescriptorSetLayoutCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkDescriptorSetLayout* SetLayout)
	{
		DumpCreateDescriptorSetLayout(Device, CreateInfo, SetLayout);

		VkResult Result = VULKANAPINAMESPACE::vkCreateDescriptorSetLayout(Device, CreateInfo, Allocator, SetLayout);

		PrintResultAndNamedHandle(Result, TEXT("DescriptorSetLayout"), *SetLayout);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyDescriptorSetLayout(VkDevice Device, VkDescriptorSetLayout DescriptorSetLayout, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroyDescriptorSetLayout(DescriptorSetLayout=%p)"), DescriptorSetLayout));

		VULKANAPINAMESPACE::vkDestroyDescriptorSetLayout(Device, DescriptorSetLayout, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateDescriptorPool(VkDevice Device, const VkDescriptorPoolCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkDescriptorPool* DescriptorPool)
	{
		DumpCreateDescriptorPool(Device, CreateInfo, DescriptorPool);

		VkResult Result = VULKANAPINAMESPACE::vkCreateDescriptorPool(Device, CreateInfo, Allocator, DescriptorPool);

		PrintResultAndNamedHandle(Result, TEXT("DescriptorPool"), *DescriptorPool);

		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyDescriptorPool(VkDevice Device, VkDescriptorPool DescriptorPool, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroyDescriptorPool(DescriptorPool=%p)"), DescriptorPool));

		VULKANAPINAMESPACE::vkDestroyDescriptorPool(Device, DescriptorPool, Allocator);
	}
#if 0
	static FORCEINLINE_DEBUGGABLE VkResult  vkResetDescriptorPool(
		VkDevice                                    Device,
		VkDescriptorPool                            descriptorPool,
		VkDescriptorPoolResetFlags                  flags);
#endif
	static FORCEINLINE_DEBUGGABLE VkResult  vkAllocateDescriptorSets(VkDevice Device, const VkDescriptorSetAllocateInfo* AllocateInfo, VkDescriptorSet* DescriptorSets)
	{
		DumpAllocateDescriptorSets(Device, AllocateInfo, DescriptorSets);

		VkResult Result = VULKANAPINAMESPACE::vkAllocateDescriptorSets(Device, AllocateInfo, DescriptorSets);

		PrintResultAndNamedHandles(Result, TEXT("DescriptorSet"), AllocateInfo->descriptorSetCount, (uint64*)DescriptorSets);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkFreeDescriptorSets(VkDevice Device, VkDescriptorPool DescriptorPool, uint32 DescriptorSetCount, const VkDescriptorSet* DescriptorSets)
	{
		DumpFreeDescriptorSets(Device, DescriptorPool, DescriptorSetCount, DescriptorSets);

		VkResult Result = VULKANAPINAMESPACE::vkFreeDescriptorSets(Device, DescriptorPool, DescriptorSetCount, DescriptorSets);

		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkUpdateDescriptorSets(VkDevice Device, uint32 DescriptorWriteCount, const VkWriteDescriptorSet* DescriptorWrites, uint32 DescriptorCopyCount, const VkCopyDescriptorSet* DescriptorCopies)
	{
		DumpUpdateDescriptorSets(Device, DescriptorWriteCount, DescriptorWrites, DescriptorCopyCount, DescriptorCopies);

		VULKANAPINAMESPACE::vkUpdateDescriptorSets(Device, DescriptorWriteCount, DescriptorWrites, DescriptorCopyCount, DescriptorCopies);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateFramebuffer(VkDevice Device, const VkFramebufferCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkFramebuffer* Framebuffer)
	{
		DumpCreateFramebuffer(Device, CreateInfo, Framebuffer);

		VkResult Result = VULKANAPINAMESPACE::vkCreateFramebuffer(Device, CreateInfo, Allocator, Framebuffer);

		DumpCreateFramebufferResult(Result, CreateInfo, *Framebuffer);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyFramebuffer(VkDevice Device, VkFramebuffer Framebuffer, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroyFramebuffer(Framebuffer=%p)"), Framebuffer));

		VULKANAPINAMESPACE::vkDestroyFramebuffer(Device, Framebuffer, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateRenderPass(VkDevice Device, const VkRenderPassCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkRenderPass* RenderPass)
	{
		DumpCreateRenderPass(Device, CreateInfo, RenderPass);

		VkResult Result = VULKANAPINAMESPACE::vkCreateRenderPass(Device, CreateInfo, Allocator, RenderPass);

		DumpCreateRenderPassResult(Result, CreateInfo, *RenderPass);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyRenderPass(VkDevice Device, VkRenderPass RenderPass, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroyRenderPass(RenderPass=%p)"), RenderPass));

		VULKANAPINAMESPACE::vkDestroyRenderPass(Device, RenderPass, Allocator);
	}
#if 0
	static FORCEINLINE_DEBUGGABLE void  vkGetRenderAreaGranularity(
		VkDevice                                    Device,
		VkRenderPass                                renderPass,
		VkExtent2D*                                 pGranularity);
#endif
	static FORCEINLINE_DEBUGGABLE VkResult  vkCreateCommandPool(VkDevice Device, const VkCommandPoolCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkCommandPool* CommandPool)
	{
		DumpCreateCommandPool(Device, CreateInfo, CommandPool);

		VkResult Result = VULKANAPINAMESPACE::vkCreateCommandPool(Device, CreateInfo, Allocator, CommandPool);

		PrintResultAndNamedHandle(Result, TEXT("CommandPool"), *CommandPool);

		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkDestroyCommandPool(VkDevice Device, VkCommandPool CommandPool, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroyCommandPool(CommandPool=%p)"), CommandPool));

		VULKANAPINAMESPACE::vkDestroyCommandPool(Device, CommandPool, Allocator);
	}
#if 0
	static FORCEINLINE_DEBUGGABLE VkResult  vkResetCommandPool(
		VkDevice                                    Device,
		VkCommandPool                               commandPool,
		VkCommandPoolResetFlags                     flags);
#endif
	static FORCEINLINE_DEBUGGABLE VkResult  vkAllocateCommandBuffers(VkDevice Device, const VkCommandBufferAllocateInfo* AllocateInfo, VkCommandBuffer* CommandBuffers)
	{
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkAllocateCommandBuffers(AllocateInfo=%p, OutCommandBuffers=%p)[...]"), AllocateInfo, CommandBuffers));

		VkResult Result = VULKANAPINAMESPACE::vkAllocateCommandBuffers(Device, AllocateInfo, CommandBuffers);
		PrintResultAndNamedHandle(Result, TEXT("CommandBuffers"), *CommandBuffers);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkFreeCommandBuffers(VkDevice Device, VkCommandPool CommandPool, uint32 CommandBufferCount, const VkCommandBuffer* CommandBuffers)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkFreeCommandBuffers(CommandPool=%p, CommandBufferCount=%d, CommandBuffers=%p)[...]"), CommandPool, CommandBufferCount, CommandBuffers));

		VULKANAPINAMESPACE::vkFreeCommandBuffers(Device, CommandPool, CommandBufferCount, CommandBuffers);
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkBeginCommandBuffer(VkCommandBuffer CommandBuffer, const VkCommandBufferBeginInfo* BeginInfo)
	{
		DumpBeginCommandBuffer(CommandBuffer, BeginInfo);

		VkResult Result = VULKANAPINAMESPACE::vkBeginCommandBuffer(CommandBuffer, BeginInfo);

		PrintResult(Result);

		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkEndCommandBuffer(VkCommandBuffer CommandBuffer)
	{
		CmdPrintfBeginResult(CommandBuffer, FString::Printf(TEXT("vkEndCommandBuffer(Cmd=%p)")));

		VkResult Result = VULKANAPINAMESPACE::vkEndCommandBuffer(CommandBuffer);

		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult  vkResetCommandBuffer(VkCommandBuffer CommandBuffer, VkCommandBufferResetFlags Flags)
	{
		PrintfBeginResult(FString::Printf(TEXT("vkResetCommandBuffer(Cmd=%p, Flags=%d)"), CommandBuffer, Flags));

		VkResult Result = VULKANAPINAMESPACE::vkResetCommandBuffer(CommandBuffer, Flags);

		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdBindPipeline(VkCommandBuffer CommandBuffer, VkPipelineBindPoint PipelineBindPoint, VkPipeline Pipeline)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdBindPipeline(BindPoint=%d, Pipeline=%p)[...]"), (int32)PipelineBindPoint, Pipeline));

		VULKANAPINAMESPACE::vkCmdBindPipeline(CommandBuffer, PipelineBindPoint, Pipeline);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetViewport(VkCommandBuffer CommandBuffer, uint32 FirstViewport, uint32 ViewportCount, const VkViewport* Viewports)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdSetViewport(FirstViewport=%d, ViewportCount=%d, Viewports=%p)[...]"), FirstViewport, ViewportCount, Viewports));

		VULKANAPINAMESPACE::vkCmdSetViewport(CommandBuffer, FirstViewport, ViewportCount, Viewports);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetScissor(VkCommandBuffer CommandBuffer, uint32 FirstScissor, uint32 ScissorCount, const VkRect2D* Scissors)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdSetScissor(FirstScissor=%d, ScissorCount=%d, Scissors=%p)[...]"), FirstScissor, ScissorCount, Scissors));

		VULKANAPINAMESPACE::vkCmdSetScissor(CommandBuffer, FirstScissor, ScissorCount, Scissors);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetLineWidth(VkCommandBuffer CommandBuffer, float LineWidth)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdSetLineWidth(Width=%f)"), LineWidth));

		VULKANAPINAMESPACE::vkCmdSetLineWidth(CommandBuffer, LineWidth);
	}

#if 0
	static FORCEINLINE_DEBUGGABLE void  vkCmdSetDepthBias(
		VkCommandBuffer                             commandBuffer,
		float                                       depthBiasConstantFactor,
		float                                       depthBiasClamp,
		float                                       depthBiasSlopeFactor);

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetBlendConstants(
		VkCommandBuffer                             commandBuffer,
		const float                                 blendConstants[4]);

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetDepthBounds(
		VkCommandBuffer                             commandBuffer,
		float                                       minDepthBounds,
		float                                       maxDepthBounds);

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetStencilCompareMask(
		VkCommandBuffer                             commandBuffer,
		VkStencilFaceFlags                          faceMask,
		uint32                                    compareMask);

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetStencilWriteMask(
		VkCommandBuffer                             commandBuffer,
		VkStencilFaceFlags                          faceMask,
		uint32                                    writeMask);

#endif
	static FORCEINLINE_DEBUGGABLE void  vkCmdSetStencilReference(VkCommandBuffer CommandBuffer, VkStencilFaceFlags FaceMask, uint32 Reference)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdSetStencilReference(FaceMask=%d, Ref=%d)"), (int32)FaceMask, (int32)Reference));

		VULKANAPINAMESPACE::vkCmdSetStencilReference(CommandBuffer, FaceMask, Reference);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdBindDescriptorSets(VkCommandBuffer CommandBuffer, VkPipelineBindPoint PipelineBindPoint, VkPipelineLayout Layout, uint32 FirstSet, uint32 DescriptorSetCount, const VkDescriptorSet* DescriptorSets, uint32 DynamicOffsetCount, const uint32* DynamicOffsets)
	{
		DumpBindDescriptorSets(CommandBuffer, PipelineBindPoint, Layout, FirstSet, DescriptorSetCount, DescriptorSets, DynamicOffsetCount, DynamicOffsets);

		VULKANAPINAMESPACE::vkCmdBindDescriptorSets(CommandBuffer, PipelineBindPoint, Layout, FirstSet, DescriptorSetCount, DescriptorSets, DynamicOffsetCount, DynamicOffsets);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdBindIndexBuffer(VkCommandBuffer CommandBuffer, VkBuffer Buffer, VkDeviceSize Offset, VkIndexType IndexType)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdBindIndexBuffer(Buffer=%p, Offset=%d, IndexType=%d)"), Buffer, (int32)Offset, (int32)IndexType));

		VULKANAPINAMESPACE::vkCmdBindIndexBuffer(CommandBuffer, Buffer, Offset, IndexType);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdBindVertexBuffers(VkCommandBuffer CommandBuffer, uint32 FirstBinding, uint32 BindingCount, const VkBuffer* Buffers, const VkDeviceSize* Offsets)
	{
		DumpCmdBindVertexBuffers(CommandBuffer, FirstBinding, BindingCount, Buffers, Offsets);

		VULKANAPINAMESPACE::vkCmdBindVertexBuffers(CommandBuffer, FirstBinding, BindingCount, Buffers, Offsets);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdDraw(VkCommandBuffer CommandBuffer, uint32 VertexCount, uint32 InstanceCount, uint32 FirstVertex, uint32 FirstInstance)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdDraw(NumVertices=%d, NumInstances=%d, FirstVertex=%d, FirstInstance=%d)"), VertexCount, InstanceCount, FirstVertex, FirstInstance));

		VULKANAPINAMESPACE::vkCmdDraw(CommandBuffer, VertexCount, InstanceCount, FirstVertex, FirstInstance);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdDrawIndexed(VkCommandBuffer CommandBuffer, uint32 IndexCount, uint32 InstanceCount, uint32 FirstIndex, int32_t VertexOffset, uint32 FirstInstance)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdDrawIndexed(IndexCount=%d, NumInstances=%d, FirstIndex=%d, VertexOffset=%d, FirstInstance=%d)"), IndexCount, InstanceCount, FirstIndex, VertexOffset, FirstInstance));

		VULKANAPINAMESPACE::vkCmdDrawIndexed(CommandBuffer, IndexCount, InstanceCount, FirstIndex, VertexOffset, FirstInstance);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdDrawIndirect(VkCommandBuffer CommandBuffer, VkBuffer Buffer, VkDeviceSize Offset, uint32 DrawCount, uint32 Stride)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdDrawIndirect(Buffer=%p, Offset=%d, DrawCount=%d, Stride=%d)"), (void*)Buffer, Offset, DrawCount, Stride));

		VULKANAPINAMESPACE::vkCmdDrawIndirect(CommandBuffer, Buffer, Offset, DrawCount, Stride);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdDrawIndexedIndirect(VkCommandBuffer CommandBuffer, VkBuffer Buffer, VkDeviceSize Offset, uint32 DrawCount, uint32 Stride)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdDrawIndexedIndirect(Buffer=%p, Offset=%d, DrawCount=%d, Stride=%d)"), (void*)Buffer, Offset, DrawCount, Stride));

		VULKANAPINAMESPACE::vkCmdDrawIndexedIndirect(CommandBuffer, Buffer, Offset, DrawCount, Stride);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdDispatch(VkCommandBuffer CommandBuffer, uint32 X, uint32 Y, uint32 Z)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdDispatch(X=%d, Y=%d Z=%d)"), X, Y, Z));

		VULKANAPINAMESPACE::vkCmdDispatch(CommandBuffer, X, Y, Z);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdDispatchIndirect(VkCommandBuffer CommandBuffer, VkBuffer Buffer, VkDeviceSize Offset)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdDispatchIndirect(Buffer=%p, Offset=%d)"), (void*)Buffer, Offset));

		VULKANAPINAMESPACE::vkCmdDispatchIndirect(CommandBuffer, Buffer, Offset);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdCopyBuffer(VkCommandBuffer CommandBuffer, VkBuffer SrcBuffer, VkBuffer DstBuffer, uint32 RegionCount, const VkBufferCopy* Regions)
	{
		DumpCmdCopyBuffer(CommandBuffer, SrcBuffer, DstBuffer, RegionCount, Regions);

		VULKANAPINAMESPACE::vkCmdCopyBuffer(CommandBuffer, SrcBuffer, DstBuffer, RegionCount, Regions);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdCopyImage(VkCommandBuffer CommandBuffer, VkImage SrcImage, VkImageLayout SrcImageLayout, VkImage DstImage, VkImageLayout DstImageLayout, uint32 RegionCount, const VkImageCopy* Regions)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdCopyImage(SrcImage=%p, SrcImageLayout=%d, DstImage=%p, DstImageLayout=%d, RegionCount=%d, Regions=%p)[...]"), SrcImage, (int32)SrcImageLayout, DstImage, (int32)DstImageLayout, RegionCount, Regions));

		VULKANAPINAMESPACE::vkCmdCopyImage(CommandBuffer, SrcImage, SrcImageLayout, DstImage, DstImageLayout, RegionCount, Regions);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdBlitImage(VkCommandBuffer CommandBuffer, VkImage SrcImage, VkImageLayout SrcImageLayout, VkImage DstImage, VkImageLayout DstImageLayout, uint32 RegionCount, const VkImageBlit* Regions, VkFilter Filter)
	{
		DumpCmdBlitImage(CommandBuffer, SrcImage, SrcImageLayout, DstImage, DstImageLayout, RegionCount, Regions, Filter);

		VULKANAPINAMESPACE::vkCmdBlitImage(CommandBuffer, SrcImage, SrcImageLayout, DstImage, DstImageLayout, RegionCount, Regions, Filter);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdCopyBufferToImage(VkCommandBuffer CommandBuffer, VkBuffer SrcBuffer, VkImage DstImage, VkImageLayout DstImageLayout, uint32 RegionCount, const VkBufferImageCopy* Regions)
	{
		DumpCmdCopyBufferToImage(CommandBuffer, SrcBuffer, DstImage, DstImageLayout, RegionCount, Regions);

		VULKANAPINAMESPACE::vkCmdCopyBufferToImage(CommandBuffer, SrcBuffer, DstImage, DstImageLayout, RegionCount, Regions);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdCopyImageToBuffer(VkCommandBuffer CommandBuffer, VkImage SrcImage, VkImageLayout SrcImageLayout, VkBuffer DstBuffer, uint32 RegionCount, const VkBufferImageCopy* Regions)
	{
		DumpCmdCopyImageToBuffer(CommandBuffer, SrcImage, SrcImageLayout, DstBuffer, RegionCount, Regions);

		VULKANAPINAMESPACE::vkCmdCopyImageToBuffer(CommandBuffer, SrcImage, SrcImageLayout, DstBuffer, RegionCount, Regions);
	}

#if 0
	static FORCEINLINE_DEBUGGABLE void  vkCmdUpdateBuffer(
		VkCommandBuffer                             commandBuffer,
		VkBuffer                                    dstBuffer,
		VkDeviceSize                                dstOffset,
		VkDeviceSize                                dataSize,
		const uint32*                             pData);
#endif

	static FORCEINLINE_DEBUGGABLE void  vkCmdFillBuffer(VkCommandBuffer CommandBuffer, VkBuffer DstBuffer, VkDeviceSize DstOffset, VkDeviceSize Size, uint32 Data)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdFillBuffer(DstBuffer=%p, DstOffset=%d, Size=%d, Data=0x%x)"), DstBuffer, (uint32)DstOffset, (uint32)Size, Data));

		VULKANAPINAMESPACE::vkCmdFillBuffer(CommandBuffer, DstBuffer, DstOffset, Size, Data);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdClearColorImage(VkCommandBuffer CommandBuffer, VkImage Image, VkImageLayout ImageLayout, const VkClearColorValue* Color, uint32 RangeCount, const VkImageSubresourceRange* Ranges)
	{
		DumpCmdClearColorImage(CommandBuffer, Image, ImageLayout, Color, RangeCount, Ranges);

		VULKANAPINAMESPACE::vkCmdClearColorImage(CommandBuffer, Image, ImageLayout, Color, RangeCount, Ranges);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdClearDepthStencilImage(VkCommandBuffer CommandBuffer, VkImage Image, VkImageLayout ImageLayout, const VkClearDepthStencilValue* DepthStencil, uint32 RangeCount, const VkImageSubresourceRange* Ranges)
	{
		DumpCmdClearDepthStencilImage(CommandBuffer, Image, ImageLayout, DepthStencil, RangeCount, Ranges);

		VULKANAPINAMESPACE::vkCmdClearDepthStencilImage(CommandBuffer, Image, ImageLayout, DepthStencil, RangeCount, Ranges);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdClearAttachments(VkCommandBuffer CommandBuffer, uint32 AttachmentCount, const VkClearAttachment* Attachments, uint32 RectCount, const VkClearRect* Rects)
	{
		DumpCmdClearAttachments(CommandBuffer, AttachmentCount, Attachments, RectCount, Rects);

		VULKANAPINAMESPACE::vkCmdClearAttachments(CommandBuffer, AttachmentCount, Attachments, RectCount, Rects);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdResolveImage(
		VkCommandBuffer CommandBuffer,
		VkImage SrcImage, VkImageLayout SrcImageLayout,
		VkImage DstImage, VkImageLayout DstImageLayout,
		uint32 RegionCount, const VkImageResolve* Regions)
	{
		DumpResolveImage(CommandBuffer, SrcImage, SrcImageLayout, DstImage, DstImageLayout, RegionCount, Regions);

		VULKANAPINAMESPACE::vkCmdResolveImage(CommandBuffer, SrcImage, SrcImageLayout, DstImage, DstImageLayout, RegionCount, Regions);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdSetEvent(VkCommandBuffer CommandBuffer, VkEvent Event, VkPipelineStageFlags StageMask)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdSetEvent(Event=%p, StageMask=0x%x)"), Event, StageMask));

		VULKANAPINAMESPACE::vkCmdSetEvent(CommandBuffer, Event, StageMask);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdResetEvent(VkCommandBuffer CommandBuffer, VkEvent Event, VkPipelineStageFlags StageMask)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdResetEvent(Event=%p, StageMask=0x%x)"), Event, StageMask));

		VULKANAPINAMESPACE::vkCmdResetEvent(CommandBuffer, Event, StageMask);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdWaitEvents(VkCommandBuffer CommandBuffer, uint32 EventCount, const VkEvent* Events,
		VkPipelineStageFlags SrcStageMask, VkPipelineStageFlags DstStageMask,
		uint32 MemoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers,
		uint32 BufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers,
		uint32 ImageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers)
	{
		DumpCmdWaitEvents(CommandBuffer, EventCount, Events, SrcStageMask, DstStageMask,
			MemoryBarrierCount, pMemoryBarriers, BufferMemoryBarrierCount, pBufferMemoryBarriers, ImageMemoryBarrierCount, pImageMemoryBarriers);

		VULKANAPINAMESPACE::vkCmdWaitEvents(CommandBuffer, EventCount, Events, SrcStageMask, DstStageMask, MemoryBarrierCount, pMemoryBarriers,
			BufferMemoryBarrierCount, pBufferMemoryBarriers, ImageMemoryBarrierCount, pImageMemoryBarriers);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdPipelineBarrier(
		VkCommandBuffer CommandBuffer, VkPipelineStageFlags SrcStageMask, VkPipelineStageFlags DstStageMask, VkDependencyFlags DependencyFlags,
		uint32 MemoryBarrierCount, const VkMemoryBarrier* MemoryBarriers,
		uint32 BufferMemoryBarrierCount, const VkBufferMemoryBarrier* BufferMemoryBarriers,
		uint32 ImageMemoryBarrierCount, const VkImageMemoryBarrier* ImageMemoryBarriers)
	{
		DumpCmdPipelineBarrier(CommandBuffer, SrcStageMask, DstStageMask, DependencyFlags, MemoryBarrierCount, MemoryBarriers, BufferMemoryBarrierCount, BufferMemoryBarriers, ImageMemoryBarrierCount, ImageMemoryBarriers);

		VULKANAPINAMESPACE::vkCmdPipelineBarrier(CommandBuffer, SrcStageMask, DstStageMask, DependencyFlags, MemoryBarrierCount, MemoryBarriers, BufferMemoryBarrierCount, BufferMemoryBarriers, ImageMemoryBarrierCount, ImageMemoryBarriers);
	}
#if 0
	static FORCEINLINE_DEBUGGABLE void  vkCmdBeginQuery(
		VkCommandBuffer                             commandBuffer,
		VkQueryPool                                 queryPool,
		uint32                                    query,
		VkQueryControlFlags                         flags);

	static FORCEINLINE_DEBUGGABLE void  vkCmdEndQuery(
		VkCommandBuffer                             commandBuffer,
		VkQueryPool                                 queryPool,
		uint32                                    query);
#endif
	static FORCEINLINE_DEBUGGABLE void  vkCmdResetQueryPool(VkCommandBuffer CommandBuffer, VkQueryPool QueryPool, uint32 FirstQuery, uint32 QueryCount)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdResetQueryPool(QueryPool=%p, FirstQuery=%d, NumQueries=%d)"), QueryPool, FirstQuery, QueryCount));

		VULKANAPINAMESPACE::vkCmdResetQueryPool(CommandBuffer, QueryPool, FirstQuery, QueryCount);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdWriteTimestamp(VkCommandBuffer CommandBuffer, VkPipelineStageFlagBits PipelineStage, VkQueryPool QueryPool, uint32 Query)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdWriteTimestamp(PipelineStage=%d, QueryPool=%p, Query=%d)"), (int32)PipelineStage, QueryPool, Query));

		VULKANAPINAMESPACE::vkCmdWriteTimestamp(CommandBuffer, PipelineStage, QueryPool, Query);
	}

	static FORCEINLINE_DEBUGGABLE void  vkCmdCopyQueryPoolResults(VkCommandBuffer CommandBuffer, VkQueryPool QueryPool, uint32 FirstQuery, uint32 QueryCount,
		VkBuffer DstBuffer, VkDeviceSize DstOffset, VkDeviceSize Stride, VkQueryResultFlags Flags)
	{
		CmdPrintfBegin(CommandBuffer, FString::Printf(TEXT("vkCmdCopyQueryPoolResults(QueryPool=%p, FirstQuery=%d, QueryCount=%d, DstBuffer=%p, DstOffset=%d, Stride=%d, Flags=0x%x)"), 
			(void*)QueryPool, FirstQuery, QueryCount, (void*)DstBuffer, (uint32)DstOffset, (uint32)Stride, (uint32)Flags));
		VULKANAPINAMESPACE::vkCmdCopyQueryPoolResults(CommandBuffer, QueryPool, FirstQuery, QueryCount, DstBuffer, DstOffset, Stride, Flags);
	}
#if 0
	static FORCEINLINE_DEBUGGABLE void  vkCmdPushConstants(
		VkCommandBuffer                             commandBuffer,
		VkPipelineLayout                            layout,
		VkShaderStageFlags                          stageFlags,
		uint32                                    offset,
		uint32                                    size,
		const void*                                 pValues);
#endif
	static FORCEINLINE_DEBUGGABLE void  vkCmdBeginRenderPass(
		VkCommandBuffer CommandBuffer,
		const VkRenderPassBeginInfo* RenderPassBegin,
		VkSubpassContents Contents)
	{
		DumpCmdBeginRenderPass(CommandBuffer, RenderPassBegin, Contents);

		VULKANAPINAMESPACE::vkCmdBeginRenderPass(CommandBuffer, RenderPassBegin, Contents);
	}
#if 0
	static FORCEINLINE_DEBUGGABLE void  vkCmdNextSubpass(
		VkCommandBuffer                             commandBuffer,
		VkSubpassContents                           contents);
#endif
	static FORCEINLINE_DEBUGGABLE void  vkCmdEndRenderPass(VkCommandBuffer CommandBuffer)
	{
		CmdPrintfBegin(CommandBuffer, TEXT("vkCmdEndRenderPass()"));

		VULKANAPINAMESPACE::vkCmdEndRenderPass(CommandBuffer);
	}
#if 0
	static FORCEINLINE_DEBUGGABLE void  vkCmdExecuteCommands(
		VkCommandBuffer                             commandBuffer,
		uint32                                    commandBufferCount,
		const VkCommandBuffer*                      pCommandBuffers);
#endif

	static FORCEINLINE_DEBUGGABLE VkResult vkCreateSwapchainKHR(VkDevice Device, const VkSwapchainCreateInfoKHR* CreateInfo, const VkAllocationCallbacks* Allocator, VkSwapchainKHR* Swapchain)
	{
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkCreateSwapchainKHR(SwapChainInfo=%p, OutSwapChain=%p)[...]"), CreateInfo, Swapchain));

		VkResult Result = VULKANAPINAMESPACE::vkCreateSwapchainKHR(Device, CreateInfo, nullptr, Swapchain);

		PrintResultAndNamedHandle(Result, TEXT("SwapChain"), *Swapchain);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE void vkDestroySwapchainKHR(VkDevice Device, VkSwapchainKHR Swapchain, const VkAllocationCallbacks* Allocator)
	{
		DevicePrintfBegin(Device, FString::Printf(TEXT("vkDestroySwapchainKHR(SwapChain=%p)[...]"), Swapchain));

		VULKANAPINAMESPACE::vkDestroySwapchainKHR(Device, Swapchain, Allocator);
	}

	static FORCEINLINE_DEBUGGABLE VkResult vkGetSwapchainImagesKHR(VkDevice Device, VkSwapchainKHR Swapchain, uint32_t* SwapchainImageCount, VkImage* SwapchainImages)
	{
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkGetSwapchainImagesKHR(Swapchain=%p, OutSwapchainImageCount=%p, OutSwapchainImages=%p)\n"), Swapchain, SwapchainImageCount, SwapchainImages));

		VkResult Result = VULKANAPINAMESPACE::vkGetSwapchainImagesKHR(Device, Swapchain, SwapchainImageCount, SwapchainImages);

		DumpSwapChainImages(Result, SwapchainImageCount, SwapchainImages);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult vkAcquireNextImageKHR(VkDevice Device, VkSwapchainKHR Swapchain, uint64_t Timeout, VkSemaphore Semaphore, VkFence Fence, uint32_t* ImageIndex)
	{
		DevicePrintfBeginResult(Device, FString::Printf(TEXT("vkAcquireNextImageKHR(Swapchain=%p, Timeout=%p, Semaphore=%p, Fence=%p, OutImageIndex=%p)[...]\n"), Swapchain, (void*)Timeout, Semaphore, Fence, ImageIndex));

		VkResult Result = VULKANAPINAMESPACE::vkAcquireNextImageKHR(Device, Swapchain, Timeout, Semaphore, Fence, ImageIndex);

		PrintResultAndNamedHandle(Result, TEXT("ImageIndex"), *ImageIndex);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult vkQueuePresentKHR(VkQueue Queue, const VkPresentInfoKHR* PresentInfo)
	{
		DumpQueuePresent(Queue, PresentInfo);

		VkResult Result = VULKANAPINAMESPACE::vkQueuePresentKHR(Queue, PresentInfo);
		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, VkSurfaceCapabilitiesKHR* SurfaceCapabilities)
	{
		PrintfBeginResult(FString::Printf(TEXT("vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice=%p, Surface=%p)[...]"), PhysicalDevice, Surface));
		VkResult Result = VULKANAPINAMESPACE::vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, SurfaceCapabilities);
		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, uint32_t* SurfaceFormatCountPtr, VkSurfaceFormatKHR* SurfaceFormats)
	{
		PrintfBeginResult(FString::Printf(TEXT("vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice=%p, Surface=%p)[...]"), PhysicalDevice, Surface));
		VkResult Result = VULKANAPINAMESPACE::vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, SurfaceFormatCountPtr, SurfaceFormats);
		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice PhysicalDevice, uint32_t QueueFamilyIndex,	VkSurfaceKHR Surface, VkBool32* SupportedPtr)
	{
		PrintfBeginResult(FString::Printf(TEXT("vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice=%p, QueueFamilyIndex=%d)[...]"), PhysicalDevice, QueueFamilyIndex));
		VkResult Result = VULKANAPINAMESPACE::vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, QueueFamilyIndex, Surface, SupportedPtr);
		PrintResult(Result);
		return Result;
	}

	static FORCEINLINE_DEBUGGABLE VkResult vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, uint32_t* PresentModeCountPtr, VkPresentModeKHR* PresentModesPtr)
	{
		PrintfBeginResult(FString::Printf(TEXT("vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice=%p, Surface=%p, PresentModeCountPtr=%d, PresentModesPtr=%p)"), PhysicalDevice, Surface, PresentModeCountPtr ? *PresentModeCountPtr:0, PresentModesPtr));
		VkResult Result = VULKANAPINAMESPACE::vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, PresentModeCountPtr, PresentModesPtr);
		PrintResult(Result);
		return Result;
	}

#if PLATFORM_ANDROID
	static FORCEINLINE_DEBUGGABLE VkResult vkCreateAndroidSurfaceKHR(VkInstance Instance, const VkAndroidSurfaceCreateInfoKHR* CreateInfo, const VkAllocationCallbacks* Allocator, VkSurfaceKHR* Surface)
	{
		PrintfBeginResult(FString::Printf(TEXT("vkCreateAndroidSurfaceKHR(Instance=%p, CreateInfo=%p, Allocator=%p, Surface=%p)[...]"), Instance, CreateInfo, Allocator, Surface));

		VkResult Result = VULKANAPINAMESPACE::vkCreateAndroidSurfaceKHR(Instance, CreateInfo, Allocator, Surface);
		PrintResult(Result);
		return Result;
	}
#endif

	static FORCEINLINE_DEBUGGABLE void vkDestroySurfaceKHR(VkInstance Instance, VkSurfaceKHR Surface, const VkAllocationCallbacks* pAllocator)
	{
		PrintfBeginResult(FString::Printf(TEXT("vkDestroySurfaceKHR(PhysicalDevice=%llu, Surface=%llu, pAllocator=%p)"), (uint64)Instance, (uint64)Surface, pAllocator));
		VULKANAPINAMESPACE::vkDestroySurfaceKHR(Instance, Surface, pAllocator);
	}

#if VULKAN_ENABLE_DUMP_LAYER
	void FlushDebugWrapperLog();
#endif
}
