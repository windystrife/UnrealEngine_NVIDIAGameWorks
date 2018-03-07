// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanTexture.cpp: Vulkan texture RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanMemory.h"
#include "VulkanContext.h"
#include "VulkanPendingState.h"
#include "Containers/ResourceArray.h"

static FCriticalSection GTextureMapLock;

struct FTextureLock
{
	FRHIResource* Texture;
	uint32 MipIndex;
	uint32 LayerIndex;

	FTextureLock(FRHIResource* InTexture, uint32 InMipIndex, uint32 InLayerIndex = 0)
		: Texture(InTexture)
		, MipIndex(InMipIndex)
		, LayerIndex(InLayerIndex)
	{
	}
};

inline bool operator == (const FTextureLock& A, const FTextureLock& B)
{
	return A.Texture == B.Texture && A.MipIndex == B.MipIndex && A.LayerIndex == B.LayerIndex;
}

inline uint32 GetTypeHash(const FTextureLock& Lock)
{
	return GetTypeHash(Lock.Texture) ^ (Lock.MipIndex << 16) ^ (Lock.LayerIndex << 8);
}

static TMap<FTextureLock, VulkanRHI::FStagingBuffer*> GPendingLockedBuffers;

static const VkImageTiling GVulkanViewTypeTilingMode[VK_IMAGE_VIEW_TYPE_RANGE_SIZE] =
{
	VK_IMAGE_TILING_LINEAR,		// VK_IMAGE_VIEW_TYPE_1D
	VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_2D
	VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_3D
	VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_CUBE
	VK_IMAGE_TILING_LINEAR,		// VK_IMAGE_VIEW_TYPE_1D_ARRAY
	VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_2D_ARRAY
	VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
};

static TStatId GetVulkanStatEnum(bool bIsCube, bool bIs3D, bool bIsRT)
{
#if STATS
	if (bIsRT == false)
	{
		// normal texture
		if (bIsCube)
		{
			return GET_STATID(STAT_TextureMemoryCube);
		}
		else if (bIs3D)
		{
			return GET_STATID(STAT_TextureMemory3D);
		}
		else
		{
			return GET_STATID(STAT_TextureMemory2D);
		}
	}
	else
	{
		// render target
		if (bIsCube)
		{
			return GET_STATID(STAT_RenderTargetMemoryCube);
		}
		else if (bIs3D)
		{
			return GET_STATID(STAT_RenderTargetMemory3D);
		}
		else
		{
			return GET_STATID(STAT_RenderTargetMemory2D);
		}
	}
#endif
	return TStatId();
}

static void UpdateVulkanTextureStats(int64 TextureSize, bool bIsCube, bool bIs3D, bool bIsRT)
{
	const int64 AlignedSize = (TextureSize > 0) ? Align(TextureSize, 1024) / 1024 : -(Align(-TextureSize, 1024) / 1024);
	if (bIsRT == false)
	{
		FPlatformAtomics::InterlockedAdd(&GCurrentTextureMemorySize, AlignedSize);
	}
	else
	{
		FPlatformAtomics::InterlockedAdd(&GCurrentRendertargetMemorySize, AlignedSize);
	}

	INC_MEMORY_STAT_BY_FName(GetVulkanStatEnum(bIsCube, bIs3D, bIsRT).GetName(), TextureSize);
}

static void VulkanTextureAllocated(uint64 Size, VkImageViewType ImageType, bool bIsRT)
{
	bool bIsCube = ImageType == VK_IMAGE_VIEW_TYPE_CUBE || ImageType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	bool bIs3D = ImageType == VK_IMAGE_VIEW_TYPE_3D ;

	UpdateVulkanTextureStats(Size, bIsCube, bIs3D, bIsRT);
}

static void VulkanTextureDestroyed(uint64 Size, VkImageViewType ImageTupe, bool bIsRT)
{
	bool bIsCube = ImageTupe == VK_IMAGE_VIEW_TYPE_CUBE || ImageTupe == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	bool bIs3D = ImageTupe == VK_IMAGE_VIEW_TYPE_3D;

	UpdateVulkanTextureStats(-(int64)Size, bIsCube, bIs3D, bIsRT);
}

inline void FVulkanSurface::InternalLockWrite(FVulkanCommandListContext& Context, FVulkanSurface* Surface, const VkImageSubresourceRange& SubresourceRange, const VkBufferImageCopy& Region, VulkanRHI::FStagingBuffer* StagingBuffer)
{
	FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetUploadCmdBuffer();
	ensure(CmdBuffer->IsOutsideRenderPass());
	VkCommandBuffer StagingCommandBuffer = CmdBuffer->GetHandle();

	VulkanRHI::ImagePipelineBarrier(StagingCommandBuffer, Surface->Image, EImageLayoutBarrier::Undefined, EImageLayoutBarrier::TransferDest, SubresourceRange);

	VulkanRHI::vkCmdCopyBufferToImage(StagingCommandBuffer, StagingBuffer->GetHandle(), Surface->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

	VulkanRHI::ImagePipelineBarrier(StagingCommandBuffer, Surface->Image, EImageLayoutBarrier::TransferDest, EImageLayoutBarrier::PixelShaderRead, SubresourceRange);

	Surface->Device->GetStagingManager().ReleaseBuffer(CmdBuffer, StagingBuffer);

	Context.GetCommandBufferManager()->SubmitUploadCmdBuffer(false);
}

struct FRHICommandLockWriteTexture : public FRHICommand<FRHICommandLockWriteTexture>
{
	FVulkanSurface* Surface;
	VkImageSubresourceRange SubresourceRange;
	VkBufferImageCopy Region;
	VulkanRHI::FStagingBuffer* StagingBuffer;

	FRHICommandLockWriteTexture(FVulkanSurface* InSurface, const VkImageSubresourceRange& InSubresourceRange, const VkBufferImageCopy& InRegion, VulkanRHI::FStagingBuffer* InStagingBuffer)
		: Surface(InSurface)
		, SubresourceRange(InSubresourceRange)
		, Region(InRegion)
		, StagingBuffer(InStagingBuffer)
	{
	}

	void Execute(FRHICommandListBase& RHICmdList)
	{
		FVulkanSurface::InternalLockWrite((FVulkanCommandListContext&)RHICmdList.GetContext(), Surface, SubresourceRange, Region, StagingBuffer);
	}
};

VkImage FVulkanSurface::CreateImage(
	FVulkanDevice& InDevice,
	VkImageViewType ResourceType,
	EPixelFormat InFormat,
	uint32 SizeX, uint32 SizeY, uint32 SizeZ,
	bool bArray, uint32 ArraySize,
	uint32 NumMips,
	uint32 NumSamples,
	uint32 UEFlags,
	VkMemoryRequirements& OutMemoryRequirements,
	VkFormat* OutStorageFormat,
	VkFormat* OutViewFormat,
	VkImageCreateInfo* OutInfo,
	bool bForceLinearTexture)
{
	const VkPhysicalDeviceProperties& DeviceProperties = InDevice.GetDeviceProperties();

	checkf(GPixelFormats[InFormat].Supported, TEXT("Format %d"), (int32)InFormat);

	VkImageCreateInfo TmpCreateInfo;
	VkImageCreateInfo* ImageCreateInfoPtr = OutInfo ? OutInfo : &TmpCreateInfo;
	VkImageCreateInfo& ImageCreateInfo = *ImageCreateInfoPtr;
	FMemory::Memzero(ImageCreateInfo);
	ImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

	switch(ResourceType)
	{
	case VK_IMAGE_VIEW_TYPE_1D:
		ImageCreateInfo.imageType = VK_IMAGE_TYPE_1D;
		check(SizeX <= DeviceProperties.limits.maxImageDimension1D);
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE:
	case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		check(SizeX == SizeY);
		check(SizeX <= DeviceProperties.limits.maxImageDimensionCube);
		check(SizeY <= DeviceProperties.limits.maxImageDimensionCube);
		ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		break;
	case VK_IMAGE_VIEW_TYPE_2D:
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		check(SizeX <= DeviceProperties.limits.maxImageDimension2D);
		check(SizeY <= DeviceProperties.limits.maxImageDimension2D);
		ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		break;
	case VK_IMAGE_VIEW_TYPE_3D:
		check(SizeY <= DeviceProperties.limits.maxImageDimension3D);
		ImageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
		break;
	default:
		checkf(false, TEXT("Unhandled image type %d"), (int32)ResourceType);
		break;
	}

	ImageCreateInfo.format = UEToVkFormat(InFormat, false);

	checkf(ImageCreateInfo.format != VK_FORMAT_UNDEFINED, TEXT("Pixel Format %d not defined!"), (int32)InFormat);
	if (OutStorageFormat)
	{
		*OutStorageFormat = ImageCreateInfo.format;
	}

	if (OutViewFormat)
	{
		VkFormat ViewFormat = UEToVkFormat(InFormat, (UEFlags & TexCreate_SRGB) == TexCreate_SRGB);
		*OutViewFormat = ViewFormat;
		ImageCreateInfo.format = ViewFormat;
	}

	ImageCreateInfo.extent.width = SizeX;
	ImageCreateInfo.extent.height = SizeY;
	ImageCreateInfo.extent.depth = ResourceType == VK_IMAGE_VIEW_TYPE_3D ? SizeZ : 1;
	ImageCreateInfo.mipLevels = NumMips;
	uint32 LayerCount = (ResourceType == VK_IMAGE_VIEW_TYPE_CUBE || ResourceType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) ? 6 : 1;
	ImageCreateInfo.arrayLayers = (bArray ? ArraySize : 1) * LayerCount;
	check(ImageCreateInfo.arrayLayers <= DeviceProperties.limits.maxImageArrayLayers);

	ImageCreateInfo.flags = (ResourceType == VK_IMAGE_VIEW_TYPE_CUBE || ResourceType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	if ((UEFlags & TexCreate_SRGB) == TexCreate_SRGB)
	{
		ImageCreateInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	}

#if SUPPORTS_MAINTENANCE_LAYER
	if (InDevice.GetOptionalExtensions().HasKHRMaintenance1 && ImageCreateInfo.imageType == VK_IMAGE_TYPE_3D)
	{
		ImageCreateInfo.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR;
	}
#endif

	ImageCreateInfo.tiling = bForceLinearTexture ? VK_IMAGE_TILING_LINEAR : GVulkanViewTypeTilingMode[ResourceType];

	ImageCreateInfo.usage = 0;
	ImageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	//@TODO: should everything be created with the source bit?
	ImageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	ImageCreateInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

	if (UEFlags & TexCreate_Presentable)
	{
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;		
	}
	else if (UEFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable))
	{
		ImageCreateInfo.usage |= (UEFlags & TexCreate_RenderTargetable) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	}
	else if (UEFlags & (TexCreate_DepthStencilResolveTarget))
	{
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	}
	else if (UEFlags & TexCreate_ResolveTargetable)
	{
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	}
	
	if (UEFlags & TexCreate_UAV)
	{
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	}

	ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ImageCreateInfo.queueFamilyIndexCount = 0;
	ImageCreateInfo.pQueueFamilyIndices = nullptr;

	if (ImageCreateInfo.tiling == VK_IMAGE_TILING_LINEAR && NumSamples > 1)
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Not allowed to create Linear textures with %d samples, reverting to 1 sample"), NumSamples);
		NumSamples = 1;
	}

	switch (NumSamples)
	{
	case 1:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		break;
	case 2:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_2_BIT;
		break;
	case 4:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_4_BIT;
		break;
	case 8:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_8_BIT;
		break;
	case 16:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_16_BIT;
		break;
	case 32:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_32_BIT;
		break;
	case 64:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_64_BIT;
		break;
	default:
		checkf(0, TEXT("Unsupported number of samples %d"), NumSamples);
		break;
	}

//#todo-rco: Verify flags work on newer Android drivers
#if !PLATFORM_ANDROID
	if (ImageCreateInfo.tiling == VK_IMAGE_TILING_LINEAR)
	{
		VkFormatFeatureFlags FormatFlags = InDevice.GetFormatProperties()[ImageCreateInfo.format].linearTilingFeatures;
		if ((FormatFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_SAMPLED_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_STORAGE_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_STORAGE_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
	}
	else if (ImageCreateInfo.tiling == VK_IMAGE_TILING_OPTIMAL)
	{
		VkFormatFeatureFlags FormatFlags = InDevice.GetFormatProperties()[ImageCreateInfo.format].optimalTilingFeatures;
		if ((FormatFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_SAMPLED_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_STORAGE_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_STORAGE_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
	}
#endif

	VkImage Image;
	VERIFYVULKANRESULT(VulkanRHI::vkCreateImage(InDevice.GetInstanceHandle(), &ImageCreateInfo, nullptr, &Image));

	// Fetch image size
	VulkanRHI::vkGetImageMemoryRequirements(InDevice.GetInstanceHandle(), Image, &OutMemoryRequirements);

	return Image;
}


struct FRHICommandInitialClearTexture : public FRHICommand<FRHICommandInitialClearTexture>
{
	FVulkanSurface* Surface;
	FClearValueBinding ClearValueBinding;
	bool bTransitionToPresentable;

	FRHICommandInitialClearTexture(FVulkanSurface* InSurface, const FClearValueBinding& InClearValueBinding, bool bInTransitionToPresentable)
		: Surface(InSurface)
		, ClearValueBinding(InClearValueBinding)
		, bTransitionToPresentable(bInTransitionToPresentable)
	{
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		Surface->InitialClear((FVulkanCommandListContext&)CmdList.GetContext(), ClearValueBinding, bTransitionToPresentable);
	}
};


FVulkanSurface::FVulkanSurface(FVulkanDevice& InDevice, VkImageViewType ResourceType, EPixelFormat InFormat,
								uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 InNumMips,
								uint32 InNumSamples, uint32 InUEFlags, const FRHIResourceCreateInfo& CreateInfo)
	: Device(&InDevice)
	, Image(VK_NULL_HANDLE)
	, StorageFormat(VK_FORMAT_UNDEFINED)
	, ViewFormat(VK_FORMAT_UNDEFINED)
	, Width(SizeX)
	, Height(SizeY)
	, Depth(SizeZ)
	, PixelFormat(InFormat)
	, UEFlags(InUEFlags)
	, MemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	, Tiling(VK_IMAGE_TILING_MAX_ENUM)	// Can be expanded to a per-platform definition
	, ViewType(ResourceType)
	, bIsImageOwner(true)
	, Allocation(nullptr)
	, NumMips(InNumMips)
	, NumSamples(InNumSamples)
	, FullAspectMask(0)
	, PartialAspectMask(0)
{
	VkImageCreateInfo ImageCreateInfo;	// Zeroed inside CreateImage
	Image = FVulkanSurface::CreateImage(InDevice, ResourceType,
		InFormat, SizeX, SizeY, SizeZ,
		bArray, ArraySize, NumMips, NumSamples, UEFlags, MemoryRequirements,
		&StorageFormat, &ViewFormat,
		&ImageCreateInfo);

	FullAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(PixelFormat, true, true);
	PartialAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(PixelFormat, false, true);

	// If VK_IMAGE_TILING_OPTIMAL is specified,
	// memoryTypeBits in vkGetImageMemoryRequirements will become 1
	// which does not support VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT.
	if (ImageCreateInfo.tiling != VK_IMAGE_TILING_OPTIMAL)
	{
		MemProps |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	}

	const bool bRenderTarget = (UEFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable | TexCreate_ResolveTargetable)) != 0;
	const bool bCPUReadback = (UEFlags & TexCreate_CPUReadback) != 0;
	const bool bDynamic = (UEFlags & TexCreate_Dynamic) != 0;

	if (!bDynamic && !bCPUReadback)
	{
		ResourceAllocation = InDevice.GetResourceHeapManager().AllocateImageMemory(MemoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, __FILE__, __LINE__);
		ResourceAllocation->BindImage(Device, Image);
	}
	else
	{
		Allocation = InDevice.GetMemoryManager().Alloc(MemoryRequirements.size, MemoryRequirements.memoryTypeBits, MemProps, __FILE__, __LINE__);
		//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("** vkBindImageMemory Buf %p MemHandle %p MemOffset %d Size %u\n"), (void*)Image, (void*)Allocation->GetHandle(), (uint32)0, (uint32)MemoryRequirements.size);
		VERIFYVULKANRESULT(VulkanRHI::vkBindImageMemory(Device->GetInstanceHandle(), Image, Allocation->GetHandle(), 0));
	}

	// update rhi stats
	VulkanTextureAllocated(MemoryRequirements.size, ResourceType, bRenderTarget);

	Tiling = ImageCreateInfo.tiling;
	check(Tiling == VK_IMAGE_TILING_LINEAR || Tiling == VK_IMAGE_TILING_OPTIMAL);

	if ((ImageCreateInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) && (UEFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable)))
	{
		const bool bTransitionToPresentable = ((UEFlags & TexCreate_Presentable) == TexCreate_Presentable);

		FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		if (!IsInRenderingThread() || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
		{
			InitialClear(Device->GetImmediateContext(), CreateInfo.ClearValueBinding, bTransitionToPresentable);
		}
		else
		{
			check(IsInRenderingThread());
			new (RHICmdList.AllocCommand<FRHICommandInitialClearTexture>()) FRHICommandInitialClearTexture(this, CreateInfo.ClearValueBinding, bTransitionToPresentable);
		}
	}
}

// This is usually used for the framebuffer image
FVulkanSurface::FVulkanSurface(FVulkanDevice& InDevice, VkImageViewType ResourceType, EPixelFormat InFormat,
								uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 InNumMips, uint32 InNumSamples,
								VkImage InImage, uint32 InUEFlags, const FRHIResourceCreateInfo& CreateInfo)
	: Device(&InDevice)
	, Image(InImage)
	, StorageFormat(VK_FORMAT_UNDEFINED)
	, ViewFormat(VK_FORMAT_UNDEFINED)
	, Width(SizeX)
	, Height(SizeY)
	, Depth(SizeZ)
	, PixelFormat(InFormat)
	, UEFlags(InUEFlags)
	, MemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	, Tiling(VK_IMAGE_TILING_MAX_ENUM)	// Can be expanded to a per-platform definition
	, ViewType(ResourceType)
	, bIsImageOwner(false)
	, Allocation(nullptr)
	, NumMips(InNumMips)
	, NumSamples(InNumSamples)
	, FullAspectMask(0)
	, PartialAspectMask(0)
{
	StorageFormat = (VkFormat)GPixelFormats[PixelFormat].PlatformFormat;
	check((UEFlags & TexCreate_SRGB) == 0);
	ViewFormat = StorageFormat;
	FullAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(PixelFormat, true, true);
	PartialAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(PixelFormat, false, true);

	// Purely informative patching, we know that "TexCreate_Presentable" uses optimal tiling
	if ((UEFlags & TexCreate_Presentable) == TexCreate_Presentable && GetTiling() == VK_IMAGE_TILING_MAX_ENUM)
	{
		Tiling = VK_IMAGE_TILING_OPTIMAL;
	}
	
	if (Image != VK_NULL_HANDLE)
	{
		if (UEFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable))
		{
			const bool bTransitionToPresentable = ((UEFlags & TexCreate_Presentable) == TexCreate_Presentable);
			InitialClear(InDevice.GetImmediateContext(), CreateInfo.ClearValueBinding, bTransitionToPresentable);
		}
	}
}

FVulkanSurface::~FVulkanSurface()
{
	Destroy();
}

void FVulkanSurface::Destroy()
{
	// An image can be instances.
	// - Instances VkImage has "bIsImageOwner" set to "false".
	// - Owner of VkImage has "bIsImageOwner" set to "true".
	if (bIsImageOwner)
	{
		Device->NotifyDeletedImage(Image);
		bIsImageOwner = false;

		uint64 Size = 0;

		if (Image != VK_NULL_HANDLE)
		{
			Size = GetMemorySize();
			Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::Image, Image);
			Image = VK_NULL_HANDLE;
		}

		if (Allocation)
		{
			Device->GetMemoryManager().Free(Allocation);
			Allocation = nullptr;
		}

		const bool bRenderTarget = (UEFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable | TexCreate_ResolveTargetable)) != 0;
		VulkanTextureDestroyed(Size, ViewType, bRenderTarget);
	}
}

#if 0
void* FVulkanSurface::Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride)
{
	DestStride = 0;

	check((MemProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0 ? Tiling == VK_IMAGE_TILING_OPTIMAL : Tiling == VK_IMAGE_TILING_LINEAR);

	// Verify all buffers are unmapped
	auto& Data = MipMapMapping.FindOrAdd(MipIndex);
	checkf(Data == nullptr, TEXT("The buffer needs to be unmapped, before it can be mapped"));

	// Get the layout of the subresource
	VkImageSubresource ImageSubResource;
	FMemory::Memzero(ImageSubResource);

	ImageSubResource.aspectMask = GetAspectMask();
	ImageSubResource.mipLevel = MipIndex;
	ImageSubResource.arrayLayer = ArrayIndex;

	// Get buffer size
	// Pitch can be only retrieved from linear textures.
	VkSubresourceLayout SubResourceLayout;
	VulkanRHI::vkGetImageSubresourceLayout(Device->GetInstanceHandle(), Image, &ImageSubResource, &SubResourceLayout);

	// Set linear row-pitch
	GetMipStride(MipIndex, DestStride);

	if(Tiling == VK_IMAGE_TILING_LINEAR)
	{
		// Verify pitch if linear
		check(DestStride == SubResourceLayout.rowPitch);

		// Map buffer to a pointer
		Data = Allocation->Map(SubResourceLayout.size, SubResourceLayout.offset);
		return Data;
	}

	// From here on, the code is dedicated to optimal textures

	// Verify all buffers are unmapped
	TRefCountPtr<FVulkanBuffer>& LinearBuffer = MipMapBuffer.FindOrAdd(MipIndex);
	checkf(LinearBuffer == nullptr, TEXT("The buffer needs to be unmapped, before it can be mapped"));

	// Create intermediate buffer which is going to be used to perform buffer to image copy
	// The copy buffer is always one face and single mip level.
	const uint32 Layers = 1;
	const uint32 Bytes = SubResourceLayout.size * Layers;

	VkBufferUsageFlags Usage = 0;
	Usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VkMemoryPropertyFlags Flags = 0;
	Flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT;

	LinearBuffer = new FVulkanBuffer(*Device, Bytes, Usage, Flags, false, __FILE__, __LINE__);

	void* DataPtr = LinearBuffer->Lock(Bytes);
	check(DataPtr);

	return DataPtr;
}

void FVulkanSurface::Unlock(uint32 MipIndex, uint32 ArrayIndex)
{
	check((MemProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0 ? Tiling == VK_IMAGE_TILING_OPTIMAL : Tiling == VK_IMAGE_TILING_LINEAR);

	if(Tiling == VK_IMAGE_TILING_LINEAR)
	{
		void*& Data = MipMapMapping.FindOrAdd(MipIndex);
		checkf(Data != nullptr, TEXT("The buffer needs to be mapped, before it can be unmapped"));

		Allocation->Unmap();
		Data = nullptr;
		return;
	}

	TRefCountPtr<FVulkanBuffer>& LinearBuffer = MipMapBuffer.FindOrAdd(MipIndex);
	checkf(LinearBuffer != nullptr, TEXT("The buffer needs to be mapped, before it can be unmapped"));
	LinearBuffer->Unlock();

	VkImageSubresource ImageSubResource;
	FMemory::Memzero(ImageSubResource);
	ImageSubResource.aspectMask = GetAspectMask();
	ImageSubResource.mipLevel = MipIndex;
	ImageSubResource.arrayLayer = ArrayIndex;

	VkSubresourceLayout SubResourceLayout;
	
	VulkanRHI::vkGetImageSubresourceLayout(Device->GetInstanceHandle(), Image, &ImageSubResource, &SubResourceLayout);

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	Region.bufferOffset = 0;
	Region.bufferRowLength = (Width >> MipIndex);
	Region.bufferImageHeight = (Height >> MipIndex);

	// The data/image is always parsed per one face.
	// Meaning that a cubemap will have atleast 6 locks/unlocks
	Region.imageSubresource.baseArrayLayer = ArrayIndex;	// Layer/face copy destination
	Region.imageSubresource.layerCount = 1;	// Indicates number of arrays in the buffer, this is also the amount of "faces/layers" to be copied
	Region.imageSubresource.aspectMask = GetAspectMask();
	Region.imageSubresource.mipLevel = MipIndex;

	Region.imageExtent.width = Region.bufferRowLength;
	Region.imageExtent.height = Region.bufferImageHeight;
	Region.imageExtent.depth = 1;

	LinearBuffer->CopyTo(*this, Region, nullptr);

	// Release buffer
	LinearBuffer = nullptr;
}
#endif

void FVulkanSurface::GetMipStride(uint32 MipIndex, uint32& Stride)
{
	// Calculate the width of the MipMap.
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 MipSizeX = FMath::Max(Width >> MipIndex, BlockSizeX);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;

	if (PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4)
	{
		// PVRTC has minimum 2 blocks width
		NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
	}

	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;

	Stride = NumBlocksX * BlockBytes;
}

void FVulkanSurface::GetMipOffset(uint32 MipIndex, uint32& Offset)
{
	uint32 offset = Offset = 0;
	for(uint32 i = 0; i < MipIndex; i++)
	{
		GetMipSize(i, offset);
		Offset += offset;
	}
}

void FVulkanSurface::GetMipSize(uint32 MipIndex, uint32& MipBytes)
{
	// Calculate the dimensions of mip-map level.
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	const uint32 MipSizeX = FMath::Max(Width >> MipIndex, BlockSizeX);
	const uint32 MipSizeY = FMath::Max(Height >> MipIndex, BlockSizeY);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;

	if (PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4)
	{
		// PVRTC has minimum 2 blocks width and height
		NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
		NumBlocksY = FMath::Max<uint32>(NumBlocksY, 2);
	}

	// Size in bytes
	MipBytes = NumBlocksX * NumBlocksY * BlockBytes;
/*
#if VULKAN_HAS_DEBUGGING_ENABLED
	VkImageSubresource SubResource;
	FMemory::Memzero(SubResource);
	SubResource.aspectMask = FullAspectMask;
	SubResource.mipLevel = MipIndex;
	//SubResource.arrayLayer = 0;
	VkSubresourceLayout OutLayout;
	VulkanRHI::vkGetImageSubresourceLayout(Device->GetInstanceHandle(), Image, &SubResource, &OutLayout);
	ensure(MipBytes >= OutLayout.size);
#endif
*/
}

void FVulkanSurface::InitialClear(FVulkanCommandListContext& Context,const FClearValueBinding& ClearValueBinding, bool bTransitionToPresentable)
{
	// Can't use TransferQueue as Vulkan requires that queue to also have Gfx or Compute capabilities...
	//#todo-rco: This function is only used during loading currently, if used for regular RHIClear then use the ActiveCmdBuffer
	FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetUploadCmdBuffer();
	ensure(CmdBuffer->IsOutsideRenderPass());

	VkPipelineStageFlags SourceStage = 0;
	VkPipelineStageFlags DestStage = 0;

	// Undefined -> Dest Optimal
	VkImageMemoryBarrier ImageBarrier = VulkanRHI::SetupImageMemoryBarrier(Image, FullAspectMask, NumMips);
	ImageBarrier.subresourceRange.layerCount = (ViewType == VK_IMAGE_VIEW_TYPE_CUBE ? 6 : 1);
	VulkanRHI::SetImageBarrierInfo(EImageLayoutBarrier::Undefined, EImageLayoutBarrier::TransferDest, ImageBarrier, SourceStage, DestStage);

	VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), SourceStage, DestStage, 0, 0, nullptr, 0, nullptr, 1, &ImageBarrier);

	if (FullAspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
	{
		VkClearColorValue Color;
		FMemory::Memzero(Color);
		Color.float32[0] = ClearValueBinding.Value.Color[0];
		Color.float32[1] = ClearValueBinding.Value.Color[1];
		Color.float32[2] = ClearValueBinding.Value.Color[2];
		Color.float32[3] = ClearValueBinding.Value.Color[3];

		// Clear
		VulkanRHI::vkCmdClearColorImage(CmdBuffer->GetHandle(), Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &Color, 1, &ImageBarrier.subresourceRange);

		// General to Present or Color
		VulkanRHI::SetImageBarrierInfo(EImageLayoutBarrier::TransferDest, bTransitionToPresentable ? EImageLayoutBarrier::Present : EImageLayoutBarrier::ColorAttachment, ImageBarrier, SourceStage, DestStage);
		VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), SourceStage, DestStage, 0, 0, nullptr, 0, nullptr, 1, &ImageBarrier);
	}
	else
	{
		check(FullAspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));
		ensure(!bTransitionToPresentable);
		VkClearDepthStencilValue Value;
		FMemory::Memzero(Value);
		Value.depth = ClearValueBinding.Value.DSValue.Depth;
		Value.stencil = ClearValueBinding.Value.DSValue.Stencil;

		// Clear
		VulkanRHI::vkCmdClearDepthStencilImage(CmdBuffer->GetHandle(), Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &Value, 1, &ImageBarrier.subresourceRange);

		// General -> DepthStencil
		VulkanRHI::SetImageBarrierInfo(EImageLayoutBarrier::TransferDest, EImageLayoutBarrier::DepthStencilAttachment, ImageBarrier, SourceStage, DestStage);
		VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), SourceStage, DestStage, 0, 0, nullptr, 0, nullptr, 1, &ImageBarrier);
	}
}

/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/

void FVulkanDynamicRHI::RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{
	check(Device);
	const uint64 TotalGPUMemory = Device->GetMemoryManager().GetTotalMemory(true);
	const uint64 TotalCPUMemory = Device->GetMemoryManager().GetTotalMemory(false);

	OutStats.DedicatedVideoMemory = TotalGPUMemory;
	OutStats.DedicatedSystemMemory = TotalCPUMemory;
	OutStats.SharedSystemMemory = -1;
	OutStats.TotalGraphicsMemory = TotalGPUMemory ? TotalGPUMemory : -1;

	OutStats.AllocatedMemorySize = int64(GCurrentTextureMemorySize) * 1024;
	OutStats.LargestContiguousAllocation = OutStats.AllocatedMemorySize;
	OutStats.TexturePoolSize = GTexturePoolSize;
	OutStats.PendingMemoryAdjustment = 0;
}

bool FVulkanDynamicRHI::RHIGetTextureMemoryVisualizeData( FColor* /*TextureData*/, int32 /*SizeX*/, int32 /*SizeY*/, int32 /*Pitch*/, int32 /*PixelSize*/ )
{
	VULKAN_SIGNAL_UNIMPLEMENTED();

	return false;
}

uint32 FVulkanDynamicRHI::RHIComputeMemorySize(FTextureRHIParamRef TextureRHI)
{
	if(!TextureRHI)
	{
		return 0;
	}

	return FVulkanTextureBase::Cast(TextureRHI)->Surface.GetMemorySize();
}

/*-----------------------------------------------------------------------------
	2D texture support.
-----------------------------------------------------------------------------*/

FTexture2DRHIRef FVulkanDynamicRHI::RHICreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FVulkanTexture2D(*Device, (EPixelFormat)Format, SizeX, SizeY, NumMips, NumSamples, Flags, CreateInfo);
}

FTexture2DRHIRef FVulkanDynamicRHI::RHIAsyncCreateTexture2D(uint32 SizeX,uint32 SizeY,uint8 Format,uint32 NumMips,uint32 Flags,void** InitialMipData,uint32 NumInitialMips)
{
	UE_LOG(LogVulkan, Fatal, TEXT("RHIAsyncCreateTexture2D is not supported"));
	VULKAN_SIGNAL_UNIMPLEMENTED(); // Unsupported atm
	return FTexture2DRHIRef();
}

void FVulkanDynamicRHI::RHICopySharedMips(FTexture2DRHIParamRef DestTexture2D,FTexture2DRHIParamRef SrcTexture2D)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

FTexture2DArrayRHIRef FVulkanDynamicRHI::RHICreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FVulkanTexture2DArray(*Device, (EPixelFormat)Format, SizeX, SizeY, SizeZ, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

FTexture3DRHIRef FVulkanDynamicRHI::RHICreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	FVulkanTexture3D* Tex3d = new FVulkanTexture3D(*Device, (EPixelFormat)Format, SizeX, SizeY, SizeZ, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);

	return Tex3d;
}

void FVulkanDynamicRHI::RHIGetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo)
{
	FVulkanTextureBase* Base = (FVulkanTextureBase*)Ref->GetTextureBaseRHI();
	OutInfo.VRamAllocation.AllocationSize = Base->Surface.GetMemorySize();
}

void FVulkanDynamicRHI::RHIGenerateMips(FTextureRHIParamRef TextureRHI)
{
	Device->GetImmediateContext().RHIGenerateMips(TextureRHI, -1);
}

void FVulkanCommandListContext::RHIGenerateMips(FTextureRHIParamRef TextureRHI, int32 NumMips)
{
	FVulkanTextureBase* VulkanTexture = (FVulkanTextureBase*)TextureRHI->GetTextureBaseRHI();
	const bool bIs2D = VulkanTexture->Surface.GetViewType() == VK_IMAGE_VIEW_TYPE_2D;
	const bool bIsCube = VulkanTexture->Surface.GetViewType() == VK_IMAGE_VIEW_TYPE_CUBE;
	check(bIs2D || bIsCube);

	FVulkanCmdBuffer* CmdBuffer = GetCommandBufferManager()->GetActiveCmdBuffer();

	if (CmdBuffer->IsInsideRenderPass())
	{
		TransitionState.EndRenderPass(CmdBuffer);
	}

	const uint32 NumLayers = (bIsCube ? 6u : 1u);

	if (NumMips == -1)
	{
		NumMips = (int32)VulkanTexture->Surface.GetNumMips();
	}

	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		VkImageMemoryBarrier SrcBarrier;
		FMemory::Memzero(SrcBarrier);

		// Transition Base Mip to Transfer Src
		VulkanRHI::SetupImageBarrierOLD(SrcBarrier, VulkanTexture->Surface, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1);
		SrcBarrier.subresourceRange.baseMipLevel = 0;
		SrcBarrier.subresourceRange.levelCount = 1;
		SrcBarrier.subresourceRange.baseArrayLayer = LayerIndex;
		SrcBarrier.subresourceRange.layerCount = 1;
		VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &SrcBarrier);

		for (uint32 MipIndex = 1; MipIndex < (uint32)NumMips; ++MipIndex)
		{
			// Transition target mip to Transfer
			VkImageMemoryBarrier DestBarrier;
			FMemory::Memzero(DestBarrier);
			VulkanRHI::SetupImageBarrierOLD(DestBarrier, VulkanTexture->Surface, 0, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
			DestBarrier.subresourceRange.baseArrayLayer = LayerIndex;
			DestBarrier.subresourceRange.layerCount = 1;
			DestBarrier.subresourceRange.baseMipLevel = MipIndex;
			DestBarrier.subresourceRange.levelCount = 1;
			VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
					0, 0, nullptr, 0, nullptr, 1, &DestBarrier);

			VkImageBlit Region;
			FMemory::Memzero(Region);

			Region.srcSubresource.aspectMask = VulkanTexture->Surface.GetFullAspectMask();
			Region.srcSubresource.baseArrayLayer = LayerIndex;
			Region.srcSubresource.layerCount = 1;
			Region.srcSubresource.mipLevel = MipIndex - 1;
			Region.srcOffsets[1].x = FMath::Max(1u, VulkanTexture->Surface.Width >> (MipIndex - 1));
			Region.srcOffsets[1].y = FMath::Max(1u, VulkanTexture->Surface.Height >> (MipIndex - 1));
			Region.srcOffsets[1].z = 1;

			Region.dstSubresource.aspectMask = VulkanTexture->Surface.GetFullAspectMask();
			Region.dstSubresource.baseArrayLayer = LayerIndex;
			Region.dstSubresource.layerCount = 1;
			Region.dstSubresource.mipLevel = MipIndex;
			Region.dstOffsets[1].x = FMath::Max(1u, VulkanTexture->Surface.Width >> MipIndex);
			Region.dstOffsets[1].y = FMath::Max(1u, VulkanTexture->Surface.Height >> MipIndex);
			Region.dstOffsets[1].z = 1;

			VulkanRHI::vkCmdBlitImage(CmdBuffer->GetHandle(), VulkanTexture->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VulkanTexture->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region, VK_FILTER_LINEAR);

			// Now transition this mip to Transfer Src
			VulkanRHI::SetupImageBarrierOLD(DestBarrier, VulkanTexture->Surface, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1);
			DestBarrier.subresourceRange.baseArrayLayer = LayerIndex;
			DestBarrier.subresourceRange.layerCount = 1;
			DestBarrier.subresourceRange.baseMipLevel = MipIndex;
			DestBarrier.subresourceRange.levelCount = 1;
			VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &DestBarrier);
		}
	}

	// Finally transition the entire texture to readable
	VkImageMemoryBarrier Barrier;
	FMemory::Memzero(Barrier);

	VulkanRHI::SetupImageBarrierOLD(Barrier, VulkanTexture->Surface, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
	Barrier.subresourceRange.baseMipLevel = 0;
	Barrier.subresourceRange.levelCount = VulkanTexture->Surface.GetNumMips();
	Barrier.subresourceRange.baseArrayLayer = 0;
	Barrier.subresourceRange.layerCount = NumLayers;
	VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
		0, 0, nullptr, 0, nullptr, 1, &Barrier);

	TransitionState.CurrentLayout.FindOrAdd(VulkanTexture->Surface.Image) = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

static void DoAsyncReallocateTexture2D(FVulkanCommandListContext& Context, FVulkanTexture2D* OldTexture, FVulkanTexture2D* NewTexture, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	//QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandGnmAsyncReallocateTexture2D_Execute);
	check(Context.IsImmediate());

	// figure out what mips to copy from/to
	const uint32 NumSharedMips = FMath::Min(OldTexture->GetNumMips(), NewTexture->GetNumMips());
	const uint32 SourceFirstMip = OldTexture->GetNumMips() - NumSharedMips;
	const uint32 DestFirstMip = NewTexture->GetNumMips() - NumSharedMips;

	FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetUploadCmdBuffer();
	ensure(CmdBuffer->IsOutsideRenderPass());

	VkCommandBuffer StagingCommandBuffer = CmdBuffer->GetHandle();

	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	SubresourceRange.baseMipLevel = DestFirstMip;
	SubresourceRange.levelCount = NumSharedMips;
	SubresourceRange.layerCount = 1;
	VulkanSetImageLayout(StagingCommandBuffer, NewTexture->Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);

	VkImageCopy Regions[MAX_TEXTURE_MIP_COUNT];
	FMemory::Memzero(&Regions[0], sizeof(VkImageCopy) * NumSharedMips);
	for (uint32 Index = 0; Index < NumSharedMips; ++Index)
	{
		uint32 MipWidth = FMath::Max<uint32>(NewSizeX >> (DestFirstMip + Index), 1u);
		uint32 MipHeight = FMath::Max<uint32>(NewSizeY >> (DestFirstMip + Index), 1u);

		VkImageCopy& Region = Regions[Index];
		Region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Region.srcSubresource.mipLevel = SourceFirstMip + Index;
		Region.srcSubresource.baseArrayLayer = 0;
		Region.srcSubresource.layerCount = 1;
		//Region.srcOffset
		Region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Region.dstSubresource.mipLevel = DestFirstMip + Index;
		Region.dstSubresource.baseArrayLayer = 0;
		Region.dstSubresource.layerCount = 1;
		//Region.dstOffset
		Region.extent.width = MipWidth;
		Region.extent.height = MipHeight;
		Region.extent.depth = 1;
	}
	VulkanRHI::vkCmdCopyImage(StagingCommandBuffer, OldTexture->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, NewTexture->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, NumSharedMips, Regions);

	// request is now complete
	RequestStatus->Decrement();

	// the next unlock for this texture can't block the GPU (it's during runtime)
	//NewTexture->Surface.bSkipBlockOnUnlock = true;
}

struct FRHICommandVulkanAsyncReallocateTexture2D : public FRHICommand<FRHICommandVulkanAsyncReallocateTexture2D>
{
	FVulkanCommandListContext& Context;
	FVulkanTexture2D* OldTexture;
	FVulkanTexture2D* NewTexture;
	int32 NewMipCount;
	int32 NewSizeX;
	int32 NewSizeY;
	FThreadSafeCounter* RequestStatus;

	FORCEINLINE_DEBUGGABLE FRHICommandVulkanAsyncReallocateTexture2D(FVulkanCommandListContext& InContext, FVulkanTexture2D* InOldTexture, FVulkanTexture2D* InNewTexture, int32 InNewMipCount, int32 InNewSizeX, int32 InNewSizeY, FThreadSafeCounter* InRequestStatus)
		: Context(InContext)
		, OldTexture(InOldTexture)
		, NewTexture(InNewTexture)
		, NewMipCount(InNewMipCount)
		, NewSizeX(InNewSizeX)
		, NewSizeY(InNewSizeY)
		, RequestStatus(InRequestStatus)
	{
	}

	void Execute(FRHICommandListBase& RHICmdList)
	{
		ensure(&((FVulkanCommandListContext&)RHICmdList.GetContext()) == &Context);
		DoAsyncReallocateTexture2D(Context, OldTexture, NewTexture, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
	}
};

FTexture2DRHIRef FVulkanDynamicRHI::AsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef OldTextureRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	if (RHICmdList.Bypass())
	{
		return FDynamicRHI::AsyncReallocateTexture2D_RenderThread(RHICmdList, OldTextureRHI, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
	}

	FVulkanTexture2D* OldTexture = ResourceCast(OldTextureRHI);

	FRHIResourceCreateInfo CreateInfo;
	FVulkanTexture2D* NewTexture = new FVulkanTexture2D(*Device, OldTexture->GetFormat(), NewSizeX, NewSizeY, NewMipCount, OldTexture->GetNumSamples(), OldTexture->GetFlags(), CreateInfo);

	new (RHICmdList.AllocCommand<FRHICommandVulkanAsyncReallocateTexture2D>()) FRHICommandVulkanAsyncReallocateTexture2D(Device->GetImmediateContext(), OldTexture, NewTexture, NewMipCount, NewSizeX, NewSizeY, RequestStatus);

	return NewTexture;
}

FTexture2DRHIRef FVulkanDynamicRHI::RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef OldTextureRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	FVulkanTexture2D* OldTexture = ResourceCast(OldTextureRHI);

	FRHIResourceCreateInfo CreateInfo;
	FVulkanTexture2D* NewTexture = new FVulkanTexture2D(*Device, OldTexture->GetFormat(), NewSizeX, NewSizeY, NewMipCount, OldTexture->GetNumSamples(), OldTexture->GetFlags(), CreateInfo);

	DoAsyncReallocateTexture2D(Device->GetImmediateContext(), OldTexture, NewTexture, NewMipCount, NewSizeX, NewSizeY, RequestStatus);

	return NewTexture;
}

ETextureReallocationStatus FVulkanDynamicRHI::RHIFinalizeAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
{
	return TexRealloc_Succeeded;
}

ETextureReallocationStatus FVulkanDynamicRHI::RHICancelAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
{
	return TexRealloc_Succeeded;
}

void* FVulkanDynamicRHI::RHILockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FVulkanTexture2D* Texture = ResourceCast(TextureRHI);
	check(Texture);

	VulkanRHI::FStagingBuffer** StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		StagingBuffer = &GPendingLockedBuffers.FindOrAdd(FTextureLock(TextureRHI, MipIndex));
		checkf(!*StagingBuffer, TEXT("Can't lock the same texture twice!"));
	}

	uint32 BufferSize = 0;
	DestStride = 0;
	Texture->Surface.GetMipSize(MipIndex, BufferSize);
	Texture->Surface.GetMipStride(MipIndex, DestStride);
	*StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);

	void* Data = (*StagingBuffer)->GetMappedPointer();
	return Data;
}

void FVulkanDynamicRHI::InternalUnlockTexture2D(bool bFromRenderingThread, FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,bool bLockWithinMiptail)
{
	FVulkanTexture2D* Texture = ResourceCast(TextureRHI);
	check(Texture);

	VkDevice LogicalDevice = Device->GetInstanceHandle();

	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		bool bFound = GPendingLockedBuffers.RemoveAndCopyValue(FTextureLock(TextureRHI, MipIndex), StagingBuffer);
		checkf(bFound, TEXT("Texture was not locked!"));
	}

	EPixelFormat Format = Texture->Surface.PixelFormat;
	uint32 MipWidth = FMath::Max<uint32>(Texture->Surface.Width >> MipIndex, GPixelFormats[Format].BlockSizeX);
	uint32 MipHeight = FMath::Max<uint32>(Texture->Surface.Height >> MipIndex, GPixelFormats[Format].BlockSizeY);

	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	SubresourceRange.baseMipLevel = MipIndex;
	SubresourceRange.levelCount = 1;
	//SubresourceRange.baseArrayLayer = 0;
	SubresourceRange.layerCount = 1;

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//#todo-rco: Might need an offset here?
	//Region.bufferOffset = 0;
	//Region.bufferRowLength = 0;
	//Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.imageSubresource.mipLevel = MipIndex;
	//Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = 1;
	Region.imageExtent.width = MipWidth;
	Region.imageExtent.height = MipHeight;
	Region.imageExtent.depth = 1;

	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (!bFromRenderingThread || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
	{
		FVulkanSurface::InternalLockWrite(Device->GetImmediateContext(), &Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
	else
	{
		check(IsInRenderingThread());
		new (RHICmdList.AllocCommand<FRHICommandLockWriteTexture>()) FRHICommandLockWriteTexture(&Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
}

void* FVulkanDynamicRHI::RHILockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FVulkanTexture2DArray* Texture = ResourceCast(TextureRHI);
	check(Texture);

	VulkanRHI::FStagingBuffer** StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		StagingBuffer = &GPendingLockedBuffers.FindOrAdd(FTextureLock(TextureRHI, MipIndex, TextureIndex));
		checkf(!*StagingBuffer, TEXT("Can't lock the same texture twice!"));
	}

	uint32 BufferSize = 0;
	DestStride = 0;
	Texture->Surface.GetMipSize(MipIndex, BufferSize);
	Texture->Surface.GetMipStride(MipIndex, DestStride);
	*StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);

	void* Data = (*StagingBuffer)->GetMappedPointer();
	return Data;
}

void FVulkanDynamicRHI::RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	FVulkanTexture2DArray* Texture = ResourceCast(TextureRHI);
	check(Texture);

	VkDevice LogicalDevice = Device->GetInstanceHandle();

	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		bool bFound = GPendingLockedBuffers.RemoveAndCopyValue(FTextureLock(TextureRHI, MipIndex, TextureIndex), StagingBuffer);
		checkf(bFound, TEXT("Texture was not locked!"));
	}

	EPixelFormat Format = Texture->Surface.PixelFormat;
	uint32 MipWidth = FMath::Max<uint32>(Texture->Surface.Width >> MipIndex, GPixelFormats[Format].BlockSizeX);
	uint32 MipHeight = FMath::Max<uint32>(Texture->Surface.Height >> MipIndex, GPixelFormats[Format].BlockSizeY);

	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = Texture->Surface.GetPartialAspectMask();
	SubresourceRange.baseMipLevel = MipIndex;
	SubresourceRange.levelCount = 1;
	SubresourceRange.baseArrayLayer = TextureIndex;
	SubresourceRange.layerCount = 1;

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//#todo-rco: Might need an offset here?
	//Region.bufferOffset = 0;
	//Region.bufferRowLength = 0;
	//Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = Texture->Surface.GetPartialAspectMask();
	Region.imageSubresource.mipLevel = MipIndex;
	Region.imageSubresource.baseArrayLayer = TextureIndex;
	Region.imageSubresource.layerCount = 1;
	Region.imageExtent.width = MipWidth;
	Region.imageExtent.height = MipHeight;
	Region.imageExtent.depth = 1;

	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		FVulkanSurface::InternalLockWrite(Device->GetImmediateContext(), &Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
	else
	{
		check(IsInRenderingThread());
		new (RHICmdList.AllocCommand<FRHICommandLockWriteTexture>()) FRHICommandLockWriteTexture(&Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
}

void FVulkanDynamicRHI::InternalUpdateTexture2D(bool bFromRenderingThread, FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	FVulkanTexture2D* Texture = ResourceCast(TextureRHI);

	EPixelFormat PixelFormat = Texture->GetFormat();
	check(GPixelFormats[PixelFormat].BlockSizeX == 1);
	check(GPixelFormats[PixelFormat].BlockSizeY == 1);

	VkFormat Format = UEToVkFormat(PixelFormat, false);

	FVulkanCommandListContext& Context = Device->GetImmediateContext();
	const VkPhysicalDeviceLimits& Limits = Device->GetLimits();
	const uint32 AlignedSourcePitch = Align(SourcePitch, Limits.optimalBufferCopyRowPitchAlignment);
	const uint32 BufferSize = Align(UpdateRegion.Height * AlignedSourcePitch, Limits.minMemoryMapAlignment);
	const uint32 AlignedSourceWidth = AlignedSourcePitch / GPixelFormats[PixelFormat].BlockBytes;

	VulkanRHI::FStagingBuffer* StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);
	void* Memory = StagingBuffer->GetMappedPointer();
	
	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = Texture->Surface.GetFullAspectMask();
	SubresourceRange.baseMipLevel = MipIndex;
	SubresourceRange.levelCount = 1;
	//SubresourceRange.baseArrayLayer = 0;
	SubresourceRange.layerCount = 1;

	uint8* RowData = (uint8*)Memory;
	uint8* SourceRowData = (uint8*)SourceData;
	const uint32 CopyPitch = UpdateRegion.Width * GPixelFormats[PixelFormat].BlockBytes;
	check(CopyPitch <= SourcePitch);
	for (uint32 i = 0; i < UpdateRegion.Height; i++)
	{
		FMemory::Memcpy(RowData, SourceRowData, CopyPitch);
		SourceRowData += SourcePitch;
		RowData += AlignedSourcePitch;
	}
	
	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	Region.bufferRowLength = AlignedSourceWidth;
	Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.imageSubresource.mipLevel = MipIndex;
	Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = 1;
	Region.imageOffset.x = UpdateRegion.DestX;
	Region.imageOffset.y = UpdateRegion.DestY;
	Region.imageOffset.z = 0;
	Region.imageExtent.width = UpdateRegion.Width;
	Region.imageExtent.height = UpdateRegion.Height;
	Region.imageExtent.depth = 1;

	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (!bFromRenderingThread || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
	{
		FVulkanSurface::InternalLockWrite(Device->GetImmediateContext(), &Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
	else
	{
		check(IsInRenderingThread());
		new (RHICmdList.AllocCommand<FRHICommandLockWriteTexture>()) FRHICommandLockWriteTexture(&Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
}

void FVulkanDynamicRHI::InternalUpdateTexture3D(bool bFromRenderingThread, FTexture3DRHIParamRef TextureRHI, uint32 MipIndex, const FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
{
	FVulkanTexture3D* Texture = ResourceCast(TextureRHI);

	EPixelFormat PixelFormat = Texture->GetFormat();
	check(GPixelFormats[PixelFormat].BlockSizeX == 1);
	check(GPixelFormats[PixelFormat].BlockSizeY == 1);

	VkFormat Format = UEToVkFormat(PixelFormat, false);

	FVulkanCommandListContext& Context = Device->GetImmediateContext();
	const VkPhysicalDeviceLimits& Limits = Device->GetLimits();

	const uint32 AlignedSourcePitch = Align(SourceRowPitch, Limits.optimalBufferCopyRowPitchAlignment);	
	const int32 SlicePitch = AlignedSourcePitch * UpdateRegion.Height;
	const uint32 BufferSize = Align(UpdateRegion.Depth * SlicePitch, Limits.minMemoryMapAlignment);

	VulkanRHI::FStagingBuffer* StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);
	void* Memory = StagingBuffer->GetMappedPointer();

	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = Texture->Surface.GetFullAspectMask();
	SubresourceRange.baseMipLevel = MipIndex;
	SubresourceRange.levelCount = 1;
	//SubresourceRange.baseArrayLayer = 0;
	SubresourceRange.layerCount = 1;

	uint32 CopyPitch = UpdateRegion.Width * GPixelFormats[Texture->GetFormat()].BlockBytes;
	check(CopyPitch <= SourceRowPitch);
	uint8* RowData = (uint8*)Memory;
	for (uint32 i = 0; i < UpdateRegion.Depth; i++)
	{
		uint8* DestRowData = RowData + SlicePitch * i;
		const uint8* SourceRowData = SourceData + SourceDepthPitch * i;
		for (uint32 j = 0; j < UpdateRegion.Height; j++)
		{
			FMemory::Memcpy(DestRowData, SourceRowData, CopyPitch);
			SourceRowData += SourceRowPitch;
			DestRowData += AlignedSourcePitch;
		}
	}

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//Region.bufferOffset = 0;
	Region.bufferRowLength = UpdateRegion.Width;
	Region.bufferImageHeight = UpdateRegion.Height;
	Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.imageSubresource.mipLevel = MipIndex;
	Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = 1;
	Region.imageOffset.x = UpdateRegion.DestX;
	Region.imageOffset.y = UpdateRegion.DestY;
	Region.imageOffset.z = UpdateRegion.DestZ;
	Region.imageExtent.width = UpdateRegion.Width;
	Region.imageExtent.height = UpdateRegion.Height;
	Region.imageExtent.depth = UpdateRegion.Depth;

	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (!bFromRenderingThread || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
	{
		FVulkanSurface::InternalLockWrite(Device->GetImmediateContext(), &Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
	else
	{
		check(IsInRenderingThread());
		new (RHICmdList.AllocCommand<FRHICommandLockWriteTexture>()) FRHICommandLockWriteTexture(&Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
}


VkImageView FVulkanTextureView::StaticCreate(FVulkanDevice& Device, VkImage Image, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, uint32 FirstMip, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices, bool bUseIdentitySwizzle)
{
	VkImageView View = VK_NULL_HANDLE;

	VkImageViewCreateInfo ViewInfo;
	FMemory::Memzero(ViewInfo);
	ViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ViewInfo.pNext = nullptr;
	ViewInfo.image = Image;
	ViewInfo.viewType = ViewType;
	ViewInfo.format = Format;
	if (bUseIdentitySwizzle)
	{
		// VK_COMPONENT_SWIZZLE_IDENTITY == 0 and this was memzero'd already
	}
	else
	{
		ViewInfo.components = Device.GetFormatComponentMapping(UEFormat);
	}

	ViewInfo.subresourceRange.aspectMask = AspectFlags;
	ViewInfo.subresourceRange.baseMipLevel = FirstMip;
	ensure(NumMips != 0xFFFFFFFF);
	ViewInfo.subresourceRange.levelCount = NumMips;
	ensure(ArraySliceIndex != 0xFFFFFFFF);
	ViewInfo.subresourceRange.baseArrayLayer = ArraySliceIndex;
	ensure(NumArraySlices != 0xFFFFFFFF);
	switch (ViewType)
	{
	case VK_IMAGE_VIEW_TYPE_3D:
		ViewInfo.subresourceRange.layerCount = 1;
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE:
		ensure(NumArraySlices == 1);
		ViewInfo.subresourceRange.layerCount = 6;
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		ViewInfo.subresourceRange.layerCount = 6 * NumArraySlices;
		break;
	case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		ViewInfo.subresourceRange.layerCount = NumArraySlices;
		break;
	default:
		ViewInfo.subresourceRange.layerCount = 1;
		break;
	}

	//HACK.  DX11 on PC currently uses a D24S8 depthbuffer and so needs an X24_G8 SRV to visualize stencil.
	//So take that as our cue to visualize stencil.  In the future, the platform independent code will have a real format
	//instead of PF_DepthStencil, so the cross-platform code could figure out the proper format to pass in for this.
	if (UEFormat == PF_X24_G8)
	{
		ensure(ViewInfo.format == VK_FORMAT_UNDEFINED);
		ViewInfo.format = (VkFormat)GPixelFormats[PF_DepthStencil].PlatformFormat;
		ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	INC_DWORD_STAT(STAT_VulkanNumImageViews);
	VERIFYVULKANRESULT(VulkanRHI::vkCreateImageView(Device.GetInstanceHandle(), &ViewInfo, nullptr, &View));

	return View;
}

void FVulkanTextureView::Create(FVulkanDevice& Device, VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, uint32 FirstMip, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices)
{
	View = StaticCreate(Device, InImage, ViewType, AspectFlags, UEFormat, Format, FirstMip, NumMips, ArraySliceIndex, NumArraySlices);
	Image = InImage;
/*
	switch (AspectFlags)
	{
	case VK_IMAGE_ASPECT_DEPTH_BIT:
	case VK_IMAGE_ASPECT_STENCIL_BIT:
	case VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT:
		Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		break;
	default:
		ensure(0);
	case VK_IMAGE_ASPECT_COLOR_BIT:
		Layout = VK_IMAGE_LAYOUT_GENERAL;
		break;
	}
*/
}

void FVulkanTextureView::Destroy(FVulkanDevice& Device)
{
	if (View)
	{
		DEC_DWORD_STAT(STAT_VulkanNumImageViews);
		Device.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::ImageView, View);
		Image = VK_NULL_HANDLE;
		View = VK_NULL_HANDLE;
	}
}

FVulkanTextureBase::FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat InFormat, uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo)
	#if !VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	: Surface(Device, ResourceType, InFormat, SizeX, SizeY, SizeZ, bArray, ArraySize, NumMips, NumSamples, UEFlags, CreateInfo)
	, PartialView(nullptr)
	#else
	: Surface(Device, ResourceType, InFormat, SizeX, SizeY, SizeZ, bArray, ArraySize, NumMips, (UEFlags & TexCreate_DepthStencilTargetable) ? NumSamples : 1, UEFlags, CreateInfo)
	, PartialView(nullptr)
	, MSAASurface(nullptr)
	#endif
{
	if (Surface.ViewFormat == VK_FORMAT_UNDEFINED)
	{
		Surface.StorageFormat = UEToVkFormat(InFormat, false);
		Surface.ViewFormat = UEToVkFormat(InFormat, (UEFlags & TexCreate_SRGB) == TexCreate_SRGB);
		checkf(Surface.StorageFormat != VK_FORMAT_UNDEFINED, TEXT("Pixel Format %d not defined!"), (int32)InFormat);
	}

	if (ResourceType != VK_IMAGE_VIEW_TYPE_MAX_ENUM)
	{
		DefaultView.Create(Device, Surface.Image, ResourceType, Surface.GetFullAspectMask(), Surface.PixelFormat, Surface.ViewFormat, 0, FMath::Max(NumMips, 1u), 0, bArray ? FMath::Max(1u, ArraySize) : FMath::Max(1u, SizeZ));
	}

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	// Create MSAA surface. The surface above is the resolve target
	if (NumSamples > 1 && (UEFlags & TexCreate_RenderTargetable) && !(UEFlags & TexCreate_DepthStencilTargetable))
	{
		MSAASurface = new FVulkanSurface(Device, ResourceType, InFormat, SizeX, SizeY, SizeZ, /*bArray=*/ false, 1, NumMips, NumSamples, UEFlags, CreateInfo);
		if (ResourceType != VK_IMAGE_VIEW_TYPE_MAX_ENUM)
		{
			MSAAView.Create(Device, MSAASurface->Image, ResourceType, MSAASurface->GetFullAspectMask(), MSAASurface->PixelFormat, MSAASurface->ViewFormat, 0, FMath::Max(NumMips, 1u), 0, bArray ? FMath::Max(1u, ArraySize) : FMath::Max(1u, SizeZ));
		}
	}
#endif

	if (Surface.FullAspectMask == Surface.PartialAspectMask)
	{
		PartialView = &DefaultView;
	}
	else
	{
		PartialView = new FVulkanTextureView;
		PartialView->Create(Device, Surface.Image, Surface.ViewType, Surface.PartialAspectMask, Surface.PixelFormat, Surface.ViewFormat, 0, FMath::Max(NumMips, 1u), 0, bArray ? FMath::Max(1u, ArraySize) : FMath::Max(1u, SizeZ));
	}

	if (!CreateInfo.BulkData)
	{
		return;
	}

	// Transfer bulk data
	VulkanRHI::FStagingBuffer* StagingBuffer = Device.GetStagingManager().AcquireBuffer(CreateInfo.BulkData->GetResourceBulkDataSize());
	void* Data = StagingBuffer->GetMappedPointer();

	// Do copy
	FMemory::Memcpy(Data, CreateInfo.BulkData->GetResourceBulkData(), CreateInfo.BulkData->GetResourceBulkDataSize());
	CreateInfo.BulkData->Discard();

	uint32 LayersPerArrayIndex = (ResourceType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY || ResourceType == VK_IMAGE_VIEW_TYPE_CUBE) ? 6 : 1;

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//#todo-rco: Use real Buffer offset when switching to suballocations!
	Region.bufferOffset = 0;
	Region.bufferRowLength = Surface.Width;
	Region.bufferImageHeight = Surface.Height;
	
	Region.imageSubresource.mipLevel = 0;
	Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = ArraySize * LayersPerArrayIndex;
	Region.imageSubresource.aspectMask = Surface.GetFullAspectMask();

	Region.imageExtent.width = Region.bufferRowLength;
	Region.imageExtent.height = Region.bufferImageHeight;
	Region.imageExtent.depth = Surface.Depth;

	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//SubresourceRange.baseMipLevel = 0;
	SubresourceRange.levelCount = Surface.GetNumMips();
	//SubresourceRange.baseArrayLayer = 0;
	SubresourceRange.layerCount = ArraySize * LayersPerArrayIndex;

	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		FVulkanSurface::InternalLockWrite(Device.GetImmediateContext(), &Surface, SubresourceRange, Region, StagingBuffer);
	}
	else
	{
		check(IsInRenderingThread());
		new (RHICmdList.AllocCommand<FRHICommandLockWriteTexture>()) FRHICommandLockWriteTexture(&Surface, SubresourceRange, Region, StagingBuffer);
	}
}

FVulkanTextureBase::FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 InNumMips, uint32 InNumSamples, VkImage InImage, VkDeviceMemory InMem, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo):
	 Surface(Device, ResourceType, Format, SizeX, SizeY, SizeZ, InNumMips, InNumSamples, InImage, UEFlags, CreateInfo)
	#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	, MSAASurface(nullptr)
	#endif
{
	check(InMem == VK_NULL_HANDLE);
	if (ResourceType != VK_IMAGE_VIEW_TYPE_MAX_ENUM && Surface.Image != VK_NULL_HANDLE)
	{
		DefaultView.Create(Device, Surface.Image, ResourceType, Surface.GetFullAspectMask(), Format, Surface.ViewFormat, 0, FMath::Max(Surface.NumMips, 1u), 0, 1u);
	}

	if (Surface.FullAspectMask == Surface.PartialAspectMask)
	{
		PartialView = &DefaultView;
	}
	else
	{
		PartialView = new FVulkanTextureView;
		check(SizeZ == 1 && ResourceType == VK_IMAGE_VIEW_TYPE_2D);
		PartialView->Create(Device, Surface.Image, Surface.ViewType, Surface.PartialAspectMask, Surface.PixelFormat, Surface.ViewFormat, 0, FMath::Max(Surface.NumMips, 1u), 0, 1u);
	}
}

FVulkanTextureBase::~FVulkanTextureBase()
{
	DestroyViews();

	if (PartialView != &DefaultView)
	{
		delete PartialView;
	}

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	if (MSAASurface)
	{
		delete MSAASurface;
		MSAASurface = nullptr;
	}
#endif
}

VkImageView FVulkanTextureBase::CreateRenderTargetView(uint32 MipIndex, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices)
{
	return FVulkanTextureView::StaticCreate(*Surface.Device, Surface.Image, Surface.GetViewType(), Surface.GetFullAspectMask(), Surface.PixelFormat, Surface.ViewFormat, MipIndex, NumMips, ArraySliceIndex, NumArraySlices, true);
}

void FVulkanTextureBase::AliasTextureResources(const FVulkanTextureBase* SrcTexture)
{
	DestroyViews();

	check(!Surface.bIsImageOwner);
	Surface.Image = SrcTexture->Surface.Image;
	DefaultView.View = SrcTexture->DefaultView.View;

	if (PartialView != &DefaultView)
	{
		PartialView->View = SrcTexture->PartialView->View;
	}

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	if (MSAASurface)
	{
		check(!MSAASurface->bIsImageOwner);
		MSAASurface->Image = SrcTexture->MSAASurface->Image;
		MSAAView.View = SrcTexture->MSAAView.View;
	}
#endif

	bIsAliased = true;
}

void FVulkanTextureBase::DestroyViews()
{
	if (!bIsAliased)
	{
		DefaultView.Destroy(*Surface.Device);

		if (PartialView != &DefaultView)
		{
			PartialView->Destroy(*Surface.Device);
		}

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
		MSAAView.Destroy(*Surface.Device);
#endif
	}
}


FVulkanTexture2D::FVulkanTexture2D(FVulkanDevice& Device, EPixelFormat InFormat, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo)
:	FRHITexture2D(SizeX, SizeY, FMath::Max(NumMips, 1u), NumSamples, InFormat, UEFlags, CreateInfo.ClearValueBinding)
,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_2D, InFormat, SizeX, SizeY, 1, /*bArray=*/ false, 1, FMath::Max(NumMips, 1u), NumSamples, UEFlags, CreateInfo)
{
}

FVulkanTexture2D::FVulkanTexture2D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, VkImage Image, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo)
:	FRHITexture2D(SizeX, SizeY, NumMips, NumSamples, Format, UEFlags, CreateInfo.ClearValueBinding)
,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_2D, Format, SizeX, SizeY, 1, NumMips, NumSamples, Image, VK_NULL_HANDLE, UEFlags)
{
}

FVulkanTexture2D::~FVulkanTexture2D()
{
	if ((Surface.UEFlags & (TexCreate_DepthStencilTargetable | TexCreate_RenderTargetable)) != 0)
	{
		Surface.Device->NotifyDeletedRenderTarget(Surface.Image);
	}
}


FVulkanBackBuffer::FVulkanBackBuffer(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 UEFlags)
	: FVulkanTexture2D(Device, Format, SizeX, SizeY, 1, 1, UEFlags, FRHIResourceCreateInfo())
{
}

FVulkanBackBuffer::FVulkanBackBuffer(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, VkImage Image, uint32 UEFlags)
	: FVulkanTexture2D(Device, Format, SizeX, SizeY, 1, 1, Image, UEFlags, FRHIResourceCreateInfo())
{
}

FVulkanBackBuffer::~FVulkanBackBuffer()
{
	if (Surface.IsImageOwner() == false)
	{
		Surface.Device->NotifyDeletedRenderTarget(Surface.Image);
		// Clear flags so ~FVulkanTexture2D() doesn't try to re-destroy it
		Surface.UEFlags = 0;
		DefaultView.View = VK_NULL_HANDLE;
		Surface.Image = VK_NULL_HANDLE;
	}
}


FVulkanTexture2DArray::FVulkanTexture2DArray(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
	:	FRHITexture2DArray(SizeX, SizeY, ArraySize, NumMips, Format, Flags, InClearValue)
	,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_2D_ARRAY, Format, SizeX, SizeY, 1, /*bArray=*/ true, ArraySize, NumMips, /*NumSamples=*/ 1, Flags, BulkData)
{
}

FVulkanTexture2DArray::FVulkanTexture2DArray(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, VkImage Image, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
	:	FRHITexture2DArray(SizeX, SizeY, ArraySize, NumMips, Format, Flags, InClearValue)
	,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_2D_ARRAY, Format, SizeX, SizeY, 1, NumMips, /*NumSamples=*/ 1, Image, VK_NULL_HANDLE, Flags, BulkData)
{
}


void FVulkanTextureReference::SetReferencedTexture(FRHITexture* InTexture)
{
	FRHITextureReference::SetReferencedTexture(InTexture);
}


FVulkanTextureCube::FVulkanTextureCube(FVulkanDevice& Device, EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
:	 FRHITextureCube(Size, NumMips, Format, Flags, InClearValue)
	//#todo-rco: Array/slices count
,	FVulkanTextureBase(Device, bArray ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE, Format, Size, Size, 1, bArray, ArraySize, NumMips, /*NumSamples=*/ 1, Flags, BulkData)
{
}

FVulkanTextureCube::FVulkanTextureCube(FVulkanDevice& Device, EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, VkImage Image, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
	:	 FRHITextureCube(Size, NumMips, Format, Flags, InClearValue)
	//#todo-rco: Array/slices count
	,	FVulkanTextureBase(Device, bArray ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE, Format, Size, Size, 1, NumMips, /*NumSamples=*/ 1, Image, VK_NULL_HANDLE, Flags, BulkData)
{
}

FVulkanTextureCube::~FVulkanTextureCube()
{
	if ((GetFlags() & (TexCreate_DepthStencilTargetable | TexCreate_RenderTargetable)) != 0)
	{
		Surface.Device->NotifyDeletedRenderTarget(Surface.Image);
	}
}


FVulkanTexture3D::FVulkanTexture3D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
	: FRHITexture3D(SizeX, SizeY, SizeZ, NumMips, Format, Flags, InClearValue)
	, FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_3D, Format, SizeX, SizeY, SizeZ, /*bArray=*/ false, 1, NumMips, /*NumSamples=*/ 1, Flags, BulkData)
{
}

FVulkanTexture3D::~FVulkanTexture3D()
{
	if ((GetFlags() & (TexCreate_DepthStencilTargetable | TexCreate_RenderTargetable)) != 0)
	{
		Surface.Device->NotifyDeletedRenderTarget(Surface.Image);
	}
}

/*-----------------------------------------------------------------------------
	Cubemap texture support.
-----------------------------------------------------------------------------*/
FTextureCubeRHIRef FVulkanDynamicRHI::RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FVulkanTextureCube(*Device, (EPixelFormat)Format, Size, false, 1, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

FTextureCubeRHIRef FVulkanDynamicRHI::RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FVulkanTextureCube(*Device, (EPixelFormat)Format, Size, true, ArraySize, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

void* FVulkanDynamicRHI::RHILockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	FVulkanTextureCube* Texture = ResourceCast(TextureCubeRHI);
	check(Texture);

	VulkanRHI::FStagingBuffer** StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		StagingBuffer = &GPendingLockedBuffers.FindOrAdd(FTextureLock(TextureCubeRHI, MipIndex));
		checkf(!*StagingBuffer, TEXT("Can't lock the same texture twice!"));
	}

	uint32 BufferSize = 0;
	DestStride = 0;
	Texture->Surface.GetMipSize(MipIndex, BufferSize);
	Texture->Surface.GetMipStride(MipIndex, DestStride);
	*StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);

	void* Data = (*StagingBuffer)->GetMappedPointer();
	return Data;
}

void FVulkanDynamicRHI::RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	FVulkanTextureCube* Texture = ResourceCast(TextureCubeRHI);
	check(Texture);

	VkDevice LogicalDevice = Device->GetInstanceHandle();

	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		bool bFound = GPendingLockedBuffers.RemoveAndCopyValue(FTextureLock(TextureCubeRHI, MipIndex), StagingBuffer);
		checkf(bFound, TEXT("Texture was not locked!"));
	}

	EPixelFormat Format = Texture->Surface.PixelFormat;
	uint32 MipWidth = FMath::Max<uint32>(Texture->Surface.Width >> MipIndex, GPixelFormats[Format].BlockSizeX);
	uint32 MipHeight = FMath::Max<uint32>(Texture->Surface.Height >> MipIndex, GPixelFormats[Format].BlockSizeY);

	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = Texture->Surface.GetPartialAspectMask();
	SubresourceRange.baseMipLevel = MipIndex;
	SubresourceRange.levelCount = 1;
	SubresourceRange.baseArrayLayer = ArrayIndex * 6 + FaceIndex;
	SubresourceRange.layerCount = 1;

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//#todo-rco: Might need an offset here?
	//Region.bufferOffset = 0;
	//Region.bufferRowLength = 0;
	//Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = Texture->Surface.GetPartialAspectMask();
	Region.imageSubresource.mipLevel = MipIndex;
	Region.imageSubresource.baseArrayLayer = ArrayIndex * 6 + FaceIndex;
	Region.imageSubresource.layerCount = 1;
	Region.imageExtent.width = MipWidth;
	Region.imageExtent.height = MipHeight;
	Region.imageExtent.depth = 1;

	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		FVulkanSurface::InternalLockWrite(Device->GetImmediateContext(), &Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
	else
	{
		check(IsInRenderingThread());
		new (RHICmdList.AllocCommand<FRHICommandLockWriteTexture>()) FRHICommandLockWriteTexture(&Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
}

void FVulkanDynamicRHI::RHIBindDebugLabelName(FTextureRHIParamRef TextureRHI, const TCHAR* Name)
{
#if VULKAN_ENABLE_DUMP_LAYER || VULKAN_ENABLE_API_DUMP
	{
// TODO: this dies in the printf on android. Needs investigation.
#if !PLATFORM_ANDROID
		FVulkanTextureBase* Base = (FVulkanTextureBase*)TextureRHI->GetTextureBaseRHI();
#if VULKAN_ENABLE_DUMP_LAYER
		VulkanRHI::PrintfBegin
#elif VULKAN_ENABLE_API_DUMP
		FPlatformMisc::LowLevelOutputDebugStringf
#endif
			(*FString::Printf(TEXT("vkDebugMarkerSetObjectNameEXT(%p=%s)\n"), Base->Surface.Image, Name));
#endif
	}
#endif

#if VULKAN_ENABLE_DRAW_MARKERS
	if (Device->GetDebugMarkerSetObjectName())
	{
		// Lambda so the char* pointer is valid
		auto DoCall = [](PFN_vkDebugMarkerSetObjectNameEXT DebugMarkerSetObjectName, VkDevice VulkanDevice, VkImage Image, const char* ObjectName)
		{
			VkDebugMarkerObjectNameInfoEXT Info;
			FMemory::Memzero(Info);
			Info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
			Info.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT;
			Info.object = (uint64)Image;
			Info.pObjectName = ObjectName;
			DebugMarkerSetObjectName(VulkanDevice, &Info);
		};

		FVulkanTextureBase* Base = (FVulkanTextureBase*)TextureRHI->GetTextureBaseRHI();
		DoCall(Device->GetDebugMarkerSetObjectName(), Device->GetInstanceHandle(), Base->Surface.Image, TCHAR_TO_ANSI(Name));
	}
#endif
	FName DebugName(Name);
	TextureRHI->SetName(DebugName);
}

void FVulkanDynamicRHI::RHIBindDebugLabelName(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const TCHAR* Name)
{
#if VULKAN_ENABLE_DUMP_LAYER || VULKAN_ENABLE_API_DUMP
	//if (Device->SupportsDebugMarkers())
	{
		//if (FRHITexture2D* Tex2d = UnorderedAccessViewRHI->GetTexture2D())
		//{
		//	FVulkanTexture2D* VulkanTexture = (FVulkanTexture2D*)Tex2d;
		//	VkDebugMarkerObjectTagInfoEXT Info;
		//	FMemory::Memzero(Info);
		//	Info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		//	Info.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT;
		//	Info.object = VulkanTexture->Surface.Image;
		//	vkDebugMarkerSetObjectNameEXT(Device->GetInstanceHandle(), &Info);
		//}
	}
#endif
}


void FVulkanDynamicRHI::RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanDynamicRHI::RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

static VkMemoryRequirements FindOrCalculateTexturePlatformSize(FVulkanDevice* Device, VkImageViewType ViewType, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags)
{
	// Adjust number of mips as UTexture can request non-valid # of mips
	NumMips = FMath::Min(FMath::FloorLog2(FMath::Max(SizeX, FMath::Max(SizeY, SizeZ))) + 1, NumMips);

	struct FTexturePlatformSizeKey
	{
		VkImageViewType ViewType;
		uint32 SizeX;
		uint32 SizeY;
		uint32 SizeZ;
		uint32 Format;
		uint32 NumMips;
		uint32 NumSamples;
		uint32 Flags;
	};

	static TMap<uint32, VkMemoryRequirements> TextureSizes;
	static FCriticalSection TextureSizesLock;

	const FTexturePlatformSizeKey Key = { ViewType, SizeX, SizeY, SizeZ, Format, NumMips, NumSamples, Flags};
	const uint32 Hash = FCrc::MemCrc32(&Key, sizeof(FTexturePlatformSizeKey));

	VkMemoryRequirements* Found = nullptr;
	{
		FScopeLock Lock(&TextureSizesLock);
		TextureSizes.Find(Hash);
		if (Found)
		{
			return *Found;
		}
	}

	VkFormat InternalStorageFormat, InternalViewFormat;
	VkImageCreateInfo CreateInfo;
	VkMemoryRequirements MemReq;
	EPixelFormat PixelFormat = (EPixelFormat)Format;

	// Create temporary image to measure the memory requirements
	VkImage TmpImage = FVulkanSurface::CreateImage(*Device, ViewType,
		PixelFormat, SizeX, SizeY, SizeZ, false, 0, NumMips, NumSamples,
		Flags, MemReq, &InternalStorageFormat, &InternalViewFormat, &CreateInfo, false);

	VulkanRHI::vkDestroyImage(Device->GetInstanceHandle(), TmpImage, nullptr);

	{
		FScopeLock Lock(&TextureSizesLock);
		TextureSizes.Add(Hash, MemReq);
	}
	
	return MemReq;
}



uint64 FVulkanDynamicRHI::RHICalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign)
{
	const VkMemoryRequirements MemReq = FindOrCalculateTexturePlatformSize(Device, VK_IMAGE_VIEW_TYPE_2D, SizeX, SizeY, 1, Format, NumMips, NumSamples, Flags);
	OutAlign = MemReq.alignment;
	return MemReq.size;
}

uint64 FVulkanDynamicRHI::RHICalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	const VkMemoryRequirements MemReq = FindOrCalculateTexturePlatformSize(Device, VK_IMAGE_VIEW_TYPE_3D, SizeX, SizeY, SizeZ, Format, NumMips, 1, Flags);
	OutAlign = MemReq.alignment;
	return MemReq.size;
}

uint64 FVulkanDynamicRHI::RHICalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	const VkMemoryRequirements MemReq = FindOrCalculateTexturePlatformSize(Device, VK_IMAGE_VIEW_TYPE_CUBE, Size, Size, 1, Format, NumMips, 1, Flags);
	OutAlign = MemReq.alignment;
	return MemReq.size;
}

FTextureReferenceRHIRef FVulkanDynamicRHI::RHICreateTextureReference(FLastRenderTimeContainer* LastRenderTime)
{
	return new FVulkanTextureReference(*Device, LastRenderTime);
}

void FVulkanCommandListContext::RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture)
{
	//#todo-rco: Implementation needs to be verified
	FVulkanTextureReference* VulkanTextureRef = (FVulkanTextureReference*)TextureRef;
	if (VulkanTextureRef)
	{
		VulkanTextureRef->SetReferencedTexture(NewTexture);
	}
}
