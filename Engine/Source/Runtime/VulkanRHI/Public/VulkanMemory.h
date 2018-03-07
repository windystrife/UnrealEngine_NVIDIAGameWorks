// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanMemory.h: Vulkan Memory RHI definitions.
=============================================================================*/

#pragma once 

// Enable to store file & line of every mem & resource allocation
#define VULKAN_MEMORY_TRACK_FILE_LINE	(UE_BUILD_DEBUG)

// Enable to save the callstack for every mem and resource allocation
#define VULKAN_MEMORY_TRACK_CALLSTACK	0


class FVulkanQueue;
class FVulkanCmdBuffer;

namespace VulkanRHI
{
	class FFenceManager;

	enum
	{
#if PLATFORM_ANDROID
		NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS = 3,
		GPU_ONLY_HEAP_PAGE_SIZE = 64 * 1024 * 1024,
		STAGING_HEAP_PAGE_SIZE = 16 * 1024 * 1024,
#else
		NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS = 20,
		GPU_ONLY_HEAP_PAGE_SIZE = 256 * 1024 * 1024,
		STAGING_HEAP_PAGE_SIZE = 64 * 1024 * 1024,
#endif
	};

	// Custom ref counting
	class FRefCount
	{
	public:
		FRefCount() {}
		virtual ~FRefCount()
		{
			check(NumRefs.GetValue() == 0);
		}

		inline uint32 AddRef()
		{
			int32 NewValue = NumRefs.Increment();
			check(NewValue > 0);
			return (uint32)NewValue;
		}

		inline uint32 Release()
		{
			int32 NewValue = NumRefs.Decrement();
			if (NewValue == 0)
			{
				delete this;
			}
			check(NewValue >= 0);
			return (uint32)NewValue;
		}

		inline uint32 GetRefCount() const
		{
			int32 Value = NumRefs.GetValue();
			check(Value >= 0);
			return (uint32)Value;
		}

	private:
		FThreadSafeCounter NumRefs;
	};

	class FDeviceChild
	{
	public:
		FDeviceChild(FVulkanDevice* InDevice = nullptr) :
			Device(InDevice)
		{
		}

		inline FVulkanDevice* GetParent() const
		{
			// Has to have one if we are asking for it...
			check(Device);
			return Device;
		}

		inline void SetParent(FVulkanDevice* InDevice)
		{
			check(!Device);
			Device = InDevice;
		}

	protected:
		FVulkanDevice* Device;
	};

	// An Allocation off a Device Heap. Lowest level of allocations and bounded by VkPhysicalDeviceLimits::maxMemoryAllocationCount.
	class FDeviceMemoryAllocation
	{
	public:
		FDeviceMemoryAllocation()
			: Size(0)
			, DeviceHandle(VK_NULL_HANDLE)
			, Handle(VK_NULL_HANDLE)
			, MappedPointer(nullptr)
			, MemoryTypeIndex(0)
			, bCanBeMapped(0)
			, bIsCoherent(0)
			, bIsCached(0)
			, bFreedBySystem(false)
#if VULKAN_MEMORY_TRACK_FILE_LINE
			, File(nullptr)
			, Line(0)
			, UID(0)
#endif
		{
		}

		void* Map(VkDeviceSize Size, VkDeviceSize Offset);
		void Unmap();

		inline bool CanBeMapped() const
		{
			return bCanBeMapped != 0;
		}

		inline bool IsMapped() const
		{
			return !!MappedPointer;
		}

		inline void* GetMappedPointer()
		{
			check(IsMapped());
			return MappedPointer;
		}

		inline bool IsCoherent() const
		{
			return bIsCoherent != 0;
		}

		void FlushMappedMemory(VkDeviceSize InOffset, VkDeviceSize InSize);
		void InvalidateMappedMemory();

		inline VkDeviceMemory GetHandle() const
		{
			return Handle;
		}

		inline VkDeviceSize GetSize() const
		{
			return Size;
		}

		inline uint32 GetMemoryTypeIndex() const
		{
			return MemoryTypeIndex;
		}

	protected:
		VkDeviceSize Size;
		VkDevice DeviceHandle;
		VkDeviceMemory Handle;
		void* MappedPointer;
		uint32 MemoryTypeIndex : 8;
		uint32 bCanBeMapped : 1;
		uint32 bIsCoherent : 1;
		uint32 bIsCached : 1;
		uint32 bFreedBySystem : 1;
		uint32 : 0;

#if VULKAN_MEMORY_TRACK_FILE_LINE
		const char* File;
		uint32 Line;
		uint32 UID;
#endif
#if VULKAN_MEMORY_TRACK_CALLSTACK
		FString Callstack;
#endif
		// Only owner can delete!
		~FDeviceMemoryAllocation();

		friend class FDeviceMemoryManager;
	};

	// Manager of Device Heap Allocations. Calling Alloc/Free is expensive!
	class FDeviceMemoryManager
	{
	public:
		FDeviceMemoryManager();
		~FDeviceMemoryManager();

		void Init(FVulkanDevice* InDevice);

		void Deinit();

		inline bool HasUnifiedMemory() const
		{
			return bHasUnifiedMemory;
		}

		inline uint32 GetNumMemoryTypes() const
		{
			return MemoryProperties.memoryTypeCount;
		}

		//#todo-rco: Might need to revisit based on https://gitlab.khronos.org/vulkan/vulkan/merge_requests/1165
		inline VkResult GetMemoryTypeFromProperties(uint32 TypeBits, VkMemoryPropertyFlags Properties, uint32* OutTypeIndex)
		{
			// Search memtypes to find first index with those properties
			for (uint32 i = 0; i < MemoryProperties.memoryTypeCount && TypeBits; i++)
			{
				if ((TypeBits & 1) == 1)
				{
					// Type is available, does it match user properties?
					if ((MemoryProperties.memoryTypes[i].propertyFlags & Properties) == Properties)
					{
						*OutTypeIndex = i;
						return VK_SUCCESS;
					}
				}
				TypeBits >>= 1;
			}

			// No memory types matched, return failure
			return VK_ERROR_FEATURE_NOT_PRESENT;
		}

		inline VkResult GetMemoryTypeFromPropertiesExcluding(uint32 TypeBits, VkMemoryPropertyFlags Properties, uint32 ExcludeTypeIndex, uint32* OutTypeIndex)
		{
			// Search memtypes to find first index with those properties
			for (uint32 i = 0; i < MemoryProperties.memoryTypeCount && TypeBits; i++)
			{
				if ((TypeBits & 1) == 1)
				{
					// Type is available, does it match user properties?
					if ((MemoryProperties.memoryTypes[i].propertyFlags & Properties) == Properties && ExcludeTypeIndex != i)
					{
						*OutTypeIndex = i;
						return VK_SUCCESS;
					}
				}
				TypeBits >>= 1;
			}

			// No memory types matched, return failure
			return VK_ERROR_FEATURE_NOT_PRESENT;
		}

		inline const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const
		{
			return MemoryProperties;
		}

		FDeviceMemoryAllocation* Alloc(VkDeviceSize AllocationSize, uint32 MemoryTypeIndex, const char* File, uint32 Line);

		inline FDeviceMemoryAllocation* Alloc(VkDeviceSize AllocationSize, uint32 MemoryTypeBits, VkMemoryPropertyFlags MemoryPropertyFlags, const char* File, uint32 Line)
		{
			uint32 MemoryTypeIndex = ~0;
			VERIFYVULKANRESULT(this->GetMemoryTypeFromProperties(MemoryTypeBits, MemoryPropertyFlags, &MemoryTypeIndex));
			return Alloc(AllocationSize, MemoryTypeIndex, File, Line);
		}

		// Sets the Allocation to nullptr
		void Free(FDeviceMemoryAllocation*& Allocation);

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		void DumpMemory();
#endif

		uint64 GetTotalMemory(bool bGPU) const;

	protected:
		VkPhysicalDeviceMemoryProperties MemoryProperties;
		VkDevice DeviceHandle;
		bool bHasUnifiedMemory;
		FVulkanDevice* Device;
		uint32 NumAllocations;
		uint32 PeakNumAllocations;

		struct FHeapInfo
		{
			VkDeviceSize TotalSize;
			VkDeviceSize UsedSize;
			VkDeviceSize PeakSize;
			TArray<FDeviceMemoryAllocation*> Allocations;

			FHeapInfo() :
				TotalSize(0),
				UsedSize(0),
				PeakSize(0)
			{
			}
		};

		TArray<FHeapInfo> HeapInfos;
		void PrintMemInfo();
	};

	class FOldResourceHeap;
	class FOldResourceHeapPage;
	class FResourceHeapManager;

	// A sub allocation for a specific memory type
	class FOldResourceAllocation : public FRefCount
	{
	public:
		FOldResourceAllocation(FOldResourceHeapPage* InOwner, FDeviceMemoryAllocation* InDeviceMemoryAllocation,
			uint32 InRequestedSize, uint32 InAlignedOffset,
			uint32 InAllocationSize, uint32 InAllocationOffset, const char* InFile, uint32 InLine);

		virtual ~FOldResourceAllocation();

		inline uint32 GetSize() const
		{
			return RequestedSize;
		}

		inline uint32 GetAllocationSize()
		{
			return AllocationSize;
		}

		inline uint32 GetOffset() const
		{
			return AlignedOffset;
		}

		inline VkDeviceMemory GetHandle() const
		{
			return DeviceMemoryAllocation->GetHandle();
		}

		inline void* GetMappedPointer()
		{
			check(DeviceMemoryAllocation->CanBeMapped());
			check(DeviceMemoryAllocation->IsMapped());
			return (uint8*)DeviceMemoryAllocation->GetMappedPointer() + AlignedOffset;
		}

		inline uint32 GetMemoryTypeIndex() const
		{
			return DeviceMemoryAllocation->GetMemoryTypeIndex();
		}

		inline void FlushMappedMemory()
		{
			DeviceMemoryAllocation->FlushMappedMemory(AllocationOffset, AllocationSize);
		}

		void BindBuffer(FVulkanDevice* Device, VkBuffer Buffer);
		void BindImage(FVulkanDevice* Device, VkImage Image);

	private:
		FOldResourceHeapPage* Owner;

		// Total size of allocation
		uint32 AllocationSize;

		// Original offset of allocation
		uint32 AllocationOffset;

		// Requested size
		uint32 RequestedSize;

		// Requested alignment offset
		uint32 AlignedOffset;

		FDeviceMemoryAllocation* DeviceMemoryAllocation;

#if VULKAN_MEMORY_TRACK_FILE_LINE
		const char* File;
		uint32 Line;
#endif
#if VULKAN_MEMORY_TRACK_CALLSTACK
		FString Callstack;
#endif

		friend class FOldResourceHeapPage;
	};

	struct FRange
	{
		uint32 Offset;
		uint32 Size;

		inline bool operator<(const FRange& In) const
		{
			return Offset < In.Offset;
		}

		static void JoinConsecutiveRanges(TArray<FRange>& Ranges);
	};

	// One device allocation that is shared amongst different resources
	class FOldResourceHeapPage
	{
	public:
		FOldResourceHeapPage(FOldResourceHeap* InOwner, FDeviceMemoryAllocation* InDeviceMemoryAllocation, uint32 InID);
		~FOldResourceHeapPage();

		FOldResourceAllocation* TryAllocate(uint32 Size, uint32 Alignment, const char* File, uint32 Line);

		FOldResourceAllocation* Allocate(uint32 Size, uint32 Alignment, const char* File, uint32 Line)
		{
			FOldResourceAllocation* ResourceAllocation = TryAllocate(Size, Alignment, File, Line);
			check(ResourceAllocation);
			return ResourceAllocation;
		}

		void ReleaseAllocation(FOldResourceAllocation* Allocation);

		inline FOldResourceHeap* GetOwner()
		{
			return Owner;
		}

		inline uint32 GetID() const
		{
			return ID;
		}

	protected:
		FOldResourceHeap* Owner;
		FDeviceMemoryAllocation* DeviceMemoryAllocation;
		TArray<FOldResourceAllocation*>  ResourceAllocations;
		uint32 MaxSize;
		uint32 UsedSize;
		int32 PeakNumAllocations;
		uint32 FrameFreed;
		uint32 ID;

		bool JoinFreeBlocks();

		TArray<FRange> FreeList;
		friend class FOldResourceHeap;
	};

	class FBufferAllocation;

	// This holds the information for a SubAllocation (a range); does NOT hold any information about what the object type is
	class FResourceSuballocation : public FRefCount
	{
	public:
		FResourceSuballocation(uint32 InRequestedSize, uint32 InAlignedOffset,
			uint32 InAllocationSize, uint32 InAllocationOffset)
			: RequestedSize(InRequestedSize)
			, AlignedOffset(InAlignedOffset)
			, AllocationSize(InAllocationSize)
			, AllocationOffset(InAllocationOffset)
		{
		}

		inline uint32 GetOffset() const
		{
			return AlignedOffset;
		}

		inline uint32 GetSize() const
		{
			return RequestedSize;
		}

	protected:
		uint32 RequestedSize;
		uint32 AlignedOffset;
		uint32 AllocationSize;
		uint32 AllocationOffset;
#if VULKAN_MEMORY_TRACK_FILE_LINE
		const char* File;
		uint32 Line;
#endif
#if VULKAN_MEMORY_TRACK_CALLSTACK
		FString Callstack;
#endif
#if VULKAN_MEMORY_TRACK_CALLSTACK || VULKAN_MEMORY_TRACK_FILE_LINE
		friend class FSubresourceAllocator;
#endif
	};

	// Suballocation of a VkBuffer
	class FBufferSuballocation : public FResourceSuballocation
	{
	public:
		FBufferSuballocation(FBufferAllocation* InOwner, VkBuffer InHandle,
			uint32 InRequestedSize, uint32 InAlignedOffset,
			uint32 InAllocationSize, uint32 InAllocationOffset)
			: FResourceSuballocation(InRequestedSize, InAlignedOffset, InAllocationSize, InAllocationOffset)
			, Owner(InOwner)
			, Handle(InHandle)
		{
		}

		virtual ~FBufferSuballocation();

		inline VkBuffer GetHandle() const
		{
			return Handle;
		}

		inline FBufferAllocation* GetBufferAllocation()
		{
			return Owner;
		}

		// Returns the pointer to the mapped data for this SubAllocation, not the full buffer!
		void* GetMappedPointer();

	protected:
		friend class FBufferAllocation;
		FBufferAllocation* Owner;
		VkBuffer Handle;
	};

	// Generically mantains/manages sub-allocations; doesn't know what the object type is
	class FSubresourceAllocator
	{
	public:
		FSubresourceAllocator(FResourceHeapManager* InOwner, FDeviceMemoryAllocation* InDeviceMemoryAllocation,
			uint32 InMemoryTypeIndex, VkMemoryPropertyFlags InMemoryPropertyFlags,
			uint32 InAlignment)
			: Owner(InOwner)
			, MemoryTypeIndex(InMemoryTypeIndex)
			, MemoryPropertyFlags(InMemoryPropertyFlags)
			, MemoryAllocation(InDeviceMemoryAllocation)
			, Alignment(InAlignment)
			, FrameFreed(0)
			, UsedSize(0)
		{
			MaxSize = InDeviceMemoryAllocation->GetSize();
			FRange FullRange;
			FullRange.Offset = 0;
			FullRange.Size = MaxSize;
			FreeList.Add(FullRange);
		}

		virtual ~FSubresourceAllocator() {}

		virtual FResourceSuballocation* CreateSubAllocation(uint32 Size, uint32 AlignedOffset, uint32 AllocatedSize, uint32 AllocatedOffset) = 0;
		virtual void Destroy(FVulkanDevice* Device) = 0;

		FResourceSuballocation* TryAllocateNoLocking(uint32 InSize, uint32 InAlignment, const char* File, uint32 Line);

		inline FResourceSuballocation* TryAllocateLocking(uint32 InSize, uint32 InAlignment, const char* File, uint32 Line)
		{
			FScopeLock ScopeLock(&CS);
			return TryAllocateNoLocking(InSize, InAlignment, File, Line);
		}

		inline uint32 GetAlignment() const
		{
			return Alignment;
		}

		inline void* GetMappedPointer()
		{
			return MemoryAllocation->GetMappedPointer();
		}

	protected:
		FResourceHeapManager* Owner;
		uint32 MemoryTypeIndex;
		VkMemoryPropertyFlags MemoryPropertyFlags;
		FDeviceMemoryAllocation* MemoryAllocation;
		uint32 MaxSize;
		uint32 Alignment;
		uint32 FrameFreed;
		int64 UsedSize;
		static FCriticalSection CS;

		// List of free ranges
		TArray<FRange> FreeList;

		// Active sub-allocations
		TArray<FResourceSuballocation*> Suballocations;
		bool JoinFreeBlocks();
	};

	// Manages/maintains sub-allocations of a VkBuffer; assumes it was created elsewhere, but it does destroy it
	class FBufferAllocation : public FSubresourceAllocator
	{
	public:
		FBufferAllocation(FResourceHeapManager* InOwner, FDeviceMemoryAllocation* InDeviceMemoryAllocation,
			uint32 InMemoryTypeIndex, VkMemoryPropertyFlags InMemoryPropertyFlags,
			uint32 InAlignment,
			VkBuffer InBuffer, VkBufferUsageFlags InBufferUsageFlags)
			: FSubresourceAllocator(InOwner, InDeviceMemoryAllocation, InMemoryTypeIndex, InMemoryPropertyFlags, InAlignment)
			, BufferUsageFlags(InBufferUsageFlags)
			, Buffer(InBuffer)
		{
		}

		virtual ~FBufferAllocation()
		{
			check(Buffer == VK_NULL_HANDLE);
		}

		virtual FResourceSuballocation* CreateSubAllocation(uint32 Size, uint32 AlignedOffset, uint32 AllocatedSize, uint32 AllocatedOffset) override
		{
			return new FBufferSuballocation(this, Buffer, Size, AlignedOffset, AllocatedSize, AllocatedOffset);// , File, Line);
		}
		virtual void Destroy(FVulkanDevice* Device) override;

		void Release(FBufferSuballocation* Suballocation);

	protected:
		VkBufferUsageFlags BufferUsageFlags;
		VkBuffer Buffer;
		friend class FResourceHeapManager;
	};

#if 0
	class FImageAllocation;

	class FImageSuballocation : public FResourceSuballocation
	{
	public:
		FImageSuballocation(FImageAllocation* InOwner, VkImage InHandle,
			uint32 InRequestedSize, uint32 InAlignedOffset,
			uint32 InAllocationSize, uint32 InAllocationOffset)
			: FResourceSuballocation(InRequestedSize, InAlignedOffset, InAllocationSize, InAllocationOffset)
			, Owner(InOwner)
			, Handle(InHandle)
		{
		}

		virtual ~FImageSuballocation();

		inline VkImage GetHandle() const
		{
			return Handle;
		}

	protected:
		friend class FImageAllocation;
		FImageAllocation* Owner;
		VkImage Handle;
	};

	class FImageAllocation : public FSubresourceAllocator
	{
	public:
		FImageAllocation(FResourceHeapManager* InOwner, FDeviceMemoryAllocation* InDeviceMemoryAllocation,
			uint32 InMemoryTypeIndex, VkMemoryPropertyFlags InMemoryPropertyFlags,
			uint32 InAlignment,
			VkImage InImage, VkImageUsageFlags InImageUsageFlags)
			: FSubresourceAllocator(InOwner, InDeviceMemoryAllocation, InMemoryTypeIndex, InMemoryPropertyFlags, InAlignment)
			, ImageUsageFlags(InImageUsageFlags)
			, Image(InImage)
		{
		}

		virtual ~FImageAllocation()
		{
			check(Image == VK_NULL_HANDLE);
		}

		virtual FResourceSuballocation* CreateSubAllocation(uint32 Size, uint32 AlignedOffset, uint32 AllocatedSize, uint32 AllocatedOffset) override
		{
			return new FImageSuballocation(this, Image, Size, AlignedOffset, AllocatedSize, AllocatedOffset);// , File, Line);
		}
		virtual void Destroy(FVulkanDevice* Device) override;

		void Release(FImageSuballocation* Suballocation);

	protected:
		VkImageUsageFlags ImageUsageFlags;
		VkImage Image;
		friend class FResourceHeapManager;
	};
#endif

	// A set of Device Allocations (Heap Pages) for a specific memory type. This handles pooling allocations inside memory pages to avoid
	// doing allocations directly off the device's heaps
	class FOldResourceHeap
	{
	public:
		FOldResourceHeap(FResourceHeapManager* InOwner, uint32 InMemoryTypeIndex, uint32 InPageSize);
		~FOldResourceHeap();

		void FreePage(FOldResourceHeapPage* InPage);

		void ReleaseFreedPages(bool bImmediately);

		inline FResourceHeapManager* GetOwner()
		{
			return Owner;
		}

		inline bool IsHostCachedSupported() const
		{
			return bIsHostCachedSupported;
		}

		inline bool IsLazilyAllocatedSupported() const
		{
			return bIsLazilyAllocatedSupported;
		}
		
		inline uint32 GetMemoryTypeIndex() const
		{
			return MemoryTypeIndex;
		}

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		void DumpMemory();
#endif

	protected:
		FResourceHeapManager* Owner;
		uint32 MemoryTypeIndex;

		bool bIsHostCachedSupported;
		bool bIsLazilyAllocatedSupported;

		uint32 DefaultPageSize;
		uint32 PeakPageSize;
		uint64 UsedMemory;
		uint32 PageIDCounter;

		TArray<FOldResourceHeapPage*> UsedBufferPages;
		TArray<FOldResourceHeapPage*> UsedImagePages;
		TArray<FOldResourceHeapPage*> FreePages;

		FOldResourceAllocation* AllocateResource(uint32 Size, uint32 Alignment, bool bIsImage, bool bMapAllocation, const char* File, uint32 Line);

		FCriticalSection CriticalSection;

		friend class FResourceHeapManager;
	};

	// Manages heaps and their interactions
	class FResourceHeapManager : public FDeviceChild
	{
	public:
		FResourceHeapManager(FVulkanDevice* InDevice);
		~FResourceHeapManager();

		void Init();
		void Deinit();

		// Returns a sub-allocation, as there can be space inside a previously allocated VkBuffer to be reused; to release a sub allocation, just delete the pointer
		FBufferSuballocation* AllocateBuffer(uint32 Size, VkBufferUsageFlags BufferUsageFlags, VkMemoryPropertyFlags MemoryPropertyFlags, const char* File, uint32 Line);

		// Release a whole allocation; this is only called from within a FBufferAllocation
		void ReleaseBuffer(FBufferAllocation* BufferAllocation);
#if 0
		FImageSuballocation* AllocateImage(uint32 Size, VkImageUsageFlags ImageUsageFlags, VkMemoryPropertyFlags MemoryPropertyFlags, const char* File, uint32 Line);
		void ReleaseImage(FImageAllocation* ImageAllocation);
#endif
		inline FOldResourceAllocation* AllocateImageMemory(const VkMemoryRequirements& MemoryReqs, VkMemoryPropertyFlags MemoryPropertyFlags, const char* File, uint32 Line)
		{
			uint32 TypeIndex = 0;
			VERIFYVULKANRESULT(DeviceMemoryManager->GetMemoryTypeFromProperties(MemoryReqs.memoryTypeBits, MemoryPropertyFlags, &TypeIndex));
			bool bMapped = (MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			if (!ResourceTypeHeaps[TypeIndex])
			{
				UE_LOG(LogVulkanRHI, Fatal, TEXT("Missing memory type index %d, MemSize %d, MemPropTypeBits %u, MemPropertyFlags %u, %s(%d)"), TypeIndex, (uint32)MemoryReqs.size, (uint32)MemoryReqs.memoryTypeBits, (uint32)MemoryPropertyFlags, ANSI_TO_TCHAR(File), Line);
			}
			FOldResourceAllocation* Allocation = ResourceTypeHeaps[TypeIndex]->AllocateResource(MemoryReqs.size, MemoryReqs.alignment, true, bMapped, File, Line);
			if (!Allocation)
			{
				VERIFYVULKANRESULT(DeviceMemoryManager->GetMemoryTypeFromPropertiesExcluding(MemoryReqs.memoryTypeBits, MemoryPropertyFlags, TypeIndex, &TypeIndex));
				bMapped = (MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
				Allocation = ResourceTypeHeaps[TypeIndex]->AllocateResource(MemoryReqs.size, MemoryReqs.alignment, true, bMapped, File, Line);
			}
			return Allocation;
		}

		inline FOldResourceAllocation* AllocateBufferMemory(const VkMemoryRequirements& MemoryReqs, VkMemoryPropertyFlags MemoryPropertyFlags, const char* File, uint32 Line)
		{
			uint32 TypeIndex = 0;
			VERIFYVULKANRESULT(DeviceMemoryManager->GetMemoryTypeFromProperties(MemoryReqs.memoryTypeBits, MemoryPropertyFlags, &TypeIndex));
			bool bMapped = (MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			if (!ResourceTypeHeaps[TypeIndex])
			{
				// Try another heap type
				uint32 OriginalTypeIndex = TypeIndex;
				if (DeviceMemoryManager->GetMemoryTypeFromPropertiesExcluding(MemoryReqs.memoryTypeBits, MemoryPropertyFlags, TypeIndex, &TypeIndex) != VK_SUCCESS)
				{
					UE_LOG(LogVulkanRHI, Fatal, TEXT("Unable to find alternate type for index %d, MemSize %d, MemPropTypeBits %u, MemPropertyFlags %u, %s(%d)"), OriginalTypeIndex, (uint32)MemoryReqs.size, (uint32)MemoryReqs.memoryTypeBits, (uint32)MemoryPropertyFlags, ANSI_TO_TCHAR(File), Line);
				}

				if (!ResourceTypeHeaps[TypeIndex])
				{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
					DumpMemory();
#endif
					UE_LOG(LogVulkanRHI, Fatal, TEXT("Missing memory type index %d (originally requested %d), MemSize %d, MemPropTypeBits %u, MemPropertyFlags %u, %s(%d)"), TypeIndex, OriginalTypeIndex, (uint32)MemoryReqs.size, (uint32)MemoryReqs.memoryTypeBits, (uint32)MemoryPropertyFlags, ANSI_TO_TCHAR(File), Line);
				}
			}

			if (!ResourceTypeHeaps[TypeIndex]->IsHostCachedSupported())
			{
				//remove host cached bit if device does not support it
				//it should only affect perf
				MemoryPropertyFlags = MemoryPropertyFlags & ~VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
			}
			if (!ResourceTypeHeaps[TypeIndex]->IsLazilyAllocatedSupported())
			{
				//remove lazily bit if device does not support it
				//it should only affect perf
				MemoryPropertyFlags = MemoryPropertyFlags & ~VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
			}

			FOldResourceAllocation* Allocation = ResourceTypeHeaps[TypeIndex]->AllocateResource(MemoryReqs.size, MemoryReqs.alignment, false, bMapped, File, Line); //-V595
			if (!Allocation)
			{
				// Try another memory type if the allocation failed
				VERIFYVULKANRESULT(DeviceMemoryManager->GetMemoryTypeFromPropertiesExcluding(MemoryReqs.memoryTypeBits, MemoryPropertyFlags, TypeIndex, &TypeIndex));
				bMapped = (MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
				if (!ResourceTypeHeaps[TypeIndex])
				{
					UE_LOG(LogVulkanRHI, Fatal, TEXT("Missing memory type index %d, MemSize %d, MemPropTypeBits %u, MemPropertyFlags %u, %s(%d)"), TypeIndex, (uint32)MemoryReqs.size, (uint32)MemoryReqs.memoryTypeBits, (uint32)MemoryPropertyFlags, ANSI_TO_TCHAR(File), Line);
				}
				Allocation = ResourceTypeHeaps[TypeIndex]->AllocateResource(MemoryReqs.size, MemoryReqs.alignment, false, bMapped, File, Line);
			}
			return Allocation;
		}

		void ReleaseFreedPages();

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		void DumpMemory();
#endif

	protected:
		static FCriticalSection CS;
		FDeviceMemoryManager* DeviceMemoryManager;
		TArray<FOldResourceHeap*> ResourceTypeHeaps;

		FOldResourceHeap* GPUHeap;
		FOldResourceHeap* UploadToGPUHeap;
		FOldResourceHeap* DownloadToCPUHeap;

		enum
		{
			BufferAllocationSize = 1 * 1024 * 1024,
			ImageAllocationSize = 2 * 1024 * 1024,
		};

		TArray<FBufferAllocation*> UsedBufferAllocations;
		TArray<FBufferAllocation*> FreeBufferAllocations;
#if 0
		TArray<FImageAllocation*> UsedImageAllocations;
		TArray<FImageAllocation*> FreeImageAllocations;
#endif
		void ReleaseFreedResources(bool bImmediately);
		void DestroyResourceAllocations();
	};

	class FStagingBuffer : public FRefCount
	{
	public:
		FStagingBuffer()
			: ResourceAllocation(nullptr)
			, Buffer(VK_NULL_HANDLE)
			, bCPURead(false)
			, BufferSize(0)
		{
		}

		VkBuffer GetHandle() const
		{
			return Buffer;
		}

		inline void* GetMappedPointer()
		{
			return ResourceAllocation->GetMappedPointer();
		}

		inline uint32 GetAllocationOffset() const
		{
			return ResourceAllocation->GetOffset();
		}

		inline uint32 GetSize() const
		{
			return BufferSize;
		}

		inline VkDeviceMemory GetDeviceMemoryHandle() const
		{
			return ResourceAllocation->GetHandle();
		}

		inline void FlushMappedMemory()
		{
			ResourceAllocation->FlushMappedMemory();
		}

	protected:
		TRefCountPtr<FOldResourceAllocation> ResourceAllocation;
		VkBuffer Buffer;
		bool bCPURead;
		uint32 BufferSize;

		// Owner maintains lifetime
		virtual ~FStagingBuffer();

		void Destroy(FVulkanDevice* Device);

		friend class FStagingManager;
	};

	class FStagingManager
	{
	public:
		FStagingManager() :
			PeakUsedMemory(0),
			UsedMemory(0),
			Device(nullptr)
		{
		}
		~FStagingManager();

		void Init(FVulkanDevice* InDevice)
		{
			Device = InDevice;
		}

		void Deinit();

		FStagingBuffer* AcquireBuffer(uint32 Size, VkBufferUsageFlags InUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, bool bCPURead = false);

		// Sets pointer to nullptr
		void ReleaseBuffer(FVulkanCmdBuffer* CmdBuffer, FStagingBuffer*& StagingBuffer);

		void ProcessPendingFree(bool bImmediately, bool bFreeToOS);

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		void DumpMemory();
#endif

	protected:
		struct FPendingItemsPerCmdBuffer
		{
			FVulkanCmdBuffer* CmdBuffer;
			struct FPendingItems
			{
				uint64 FenceCounter;
				TArray<FStagingBuffer*> Resources;
			};


			inline FPendingItems* FindOrAddItemsForFence(uint64 Fence);

			TArray<FPendingItems> PendingItems;
		};

		TArray<FStagingBuffer*> UsedStagingBuffers;
		TArray<FPendingItemsPerCmdBuffer> PendingFreeStagingBuffers;
		struct FFreeEntry
		{
			FStagingBuffer* Buffer;
			uint32 FrameNumber;
		};
		TArray<FFreeEntry> FreeStagingBuffers;

		uint64 PeakUsedMemory;
		uint64 UsedMemory;

		FPendingItemsPerCmdBuffer* FindOrAdd(FVulkanCmdBuffer* CmdBuffer);

		void ProcessPendingFreeNoLock(bool bImmediately, bool bFreeToOS);

		FVulkanDevice* Device;
	};

	class FFence
	{
	public:
		FFence(FVulkanDevice* InDevice, FFenceManager* InOwner, bool bCreateSignaled);

		inline VkFence GetHandle() const
		{
			return Handle;
		}

		inline bool IsSignaled() const
		{
			return State == EState::Signaled;
		}

		FFenceManager* GetOwner()
		{
			return Owner;
		}

	protected:
		VkFence Handle;

		enum class EState
		{
			// Initial state
			NotReady,

			// After GPU processed it
			Signaled,
		};

		EState State;

		FFenceManager* Owner;

		// Only owner can delete!
		~FFence();
		friend class FFenceManager;
	};

	class FFenceManager
	{
	public:
		FFenceManager()
			: Device(nullptr)
		{
		}
		~FFenceManager();

		void Init(FVulkanDevice* InDevice);
		void Deinit();

		FFence* AllocateFence(bool bCreateSignaled = false);

		inline bool IsFenceSignaled(FFence* Fence)
		{
			if (Fence->IsSignaled())
			{
				return true;
			}

			return CheckFenceState(Fence);
		}

		// Returns false if it timed out
		bool WaitForFence(FFence* Fence, uint64 TimeInNanoseconds);

		void ResetFence(FFence* Fence);

		// Sets it to nullptr
		void ReleaseFence(FFence*& Fence);

		// Sets it to nullptr
		void WaitAndReleaseFence(FFence*& Fence, uint64 TimeInNanoseconds);

	protected:
		FVulkanDevice* Device;
		TArray<FFence*> FreeFences;
		TArray<FFence*> UsedFences;

		// Returns true if signaled
		bool CheckFenceState(FFence* Fence);

		void DestroyFence(FFence* Fence);
	};

	class FGPUEvent : public FDeviceChild, public FRefCount
	{
	public:
		FGPUEvent(FVulkanDevice* InDevice);
		virtual ~FGPUEvent();

		inline VkEvent GetHandle() const
		{
			return Handle;
		}

	protected:
		VkEvent Handle;
	};

	class FDeferredDeletionQueue : public FDeviceChild
	{
		//typedef TPair<FRefCountedObject*, uint64> FFencedObject;
		//FThreadsafeQueue<FFencedObject> DeferredReleaseQueue;

	public:
		FDeferredDeletionQueue(FVulkanDevice* InDevice);
		~FDeferredDeletionQueue();

		enum class EType
		{
			RenderPass,
			Buffer,
			BufferView,
			Image,
			ImageView,
			Pipeline,
			PipelineLayout,
			Framebuffer,
			DescriptorSetLayout,
			Sampler,
			Semaphore,
			ShaderModule,
			Event,
		};

		template <typename T>
		inline void EnqueueResource(EType Type, T Handle)
		{
			static_assert(sizeof(T) <= sizeof(uint64), "Vulkan resource handle type size too large.");
			EnqueueGenericResource(Type, (uint64)Handle);
		}

		void ReleaseResources(bool bDeleteImmediately = false);

		inline void Clear()
		{
			ReleaseResources(true);
		}
/*
		class FVulkanAsyncDeletionWorker : public FVulkanDeviceChild, FNonAbandonableTask
		{
		public:
			FVulkanAsyncDeletionWorker(FVulkanDevice* InDevice, FThreadsafeQueue<FFencedObject>* DeletionQueue);

			void DoWork();

		private:
			TQueue<FFencedObject> Queue;
		};
*/
	private:
		//TQueue<FAsyncTask<FVulkanAsyncDeletionWorker>*> DeleteTasks;

		void EnqueueGenericResource(EType Type, uint64 Handle);

		struct FEntry
		{
			uint64 FenceCounter;
			FVulkanCmdBuffer* CmdBuffer;
			uint64 Handle;
			EType StructureType;

			//FRefCount* Resource;
		};
		FCriticalSection CS;
		TArray<FEntry> Entries;
	};

	// Simple tape allocation per frame for a VkBuffer, used for Volatile allocations
	class FTempFrameAllocationBuffer : public FDeviceChild
	{
		enum
		{
			ALLOCATION_SIZE = (2 * 1024 * 1024),
		};

	public:
		FTempFrameAllocationBuffer(FVulkanDevice* InDevice);
		virtual ~FTempFrameAllocationBuffer();
		void Destroy();

		struct FTempAllocInfo
		{
			void* Data;

			FBufferSuballocation* BufferSuballocation;

			// Offset into the locked area
			uint32 CurrentOffset;

			// Simple counter used for the SRVs to know a new one is required
			uint32 LockCounter;

			FTempAllocInfo()
				: Data(nullptr)
				, BufferSuballocation(nullptr)
				, CurrentOffset(0)
				, LockCounter(0)
			{
			}

			inline uint32 GetBindOffset() const
			{
				return BufferSuballocation->GetOffset() + CurrentOffset;
			}

			inline VkBuffer GetHandle() const
			{
				return BufferSuballocation->GetHandle();
			}
		};

		void Alloc(uint32 InSize, uint32 InAlignment, FTempAllocInfo& OutInfo);

		void Reset();

	protected:
		uint32 BufferIndex;

		struct FFrameEntry
		{
			TRefCountPtr<FBufferSuballocation> BufferSuballocation;
			TArray<TRefCountPtr<FBufferSuballocation>> PendingDeletionList;
			uint8* MappedData = nullptr;
			uint8* CurrentData = nullptr;
			uint32 Size = 0;
			uint32 PeakUsed = 0;

			void InitBuffer(FVulkanDevice* InDevice, uint32 InSize);
			void Reset();
			bool TryAlloc(uint32 InSize, uint32 InAlignment, FTempAllocInfo& OutInfo);
		};
		FFrameEntry Entries[NUM_RENDER_BUFFERS];

		friend class FVulkanCommandListContext;
	};

	inline void* FBufferSuballocation::GetMappedPointer()
	{
		return (uint8*)Owner->GetMappedPointer() + AlignedOffset;
	}

	enum class EImageLayoutBarrier
	{
		Undefined,
		TransferDest,
		ColorAttachment,
		DepthStencilAttachment,
		TransferSource,
		Present,
		PixelShaderRead,
		PixelDepthStencilRead,
		ComputeGeneralRW,
	};

	inline EImageLayoutBarrier GetImageLayoutFromVulkanLayout(VkImageLayout Layout)
	{
		switch (Layout)
		{
		case VK_IMAGE_LAYOUT_UNDEFINED:
			return EImageLayoutBarrier::Undefined;

		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return EImageLayoutBarrier::TransferDest;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return EImageLayoutBarrier::ColorAttachment;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			return EImageLayoutBarrier::DepthStencilAttachment;

		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return EImageLayoutBarrier::TransferSource;

		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			return EImageLayoutBarrier::Present;

		//case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		//	return EImageLayoutBarrier::PixelShaderRead;

		//case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
		//	return EImageLayoutBarrier::PixelDepthStencilRead;

		//case VK_IMAGE_LAYOUT_GENERAL:
		//	return EImageLayoutBarrier::ComputeGeneral;

		default:
			checkf(0, TEXT("Unknown VkImageLayout %d"), (int32)Layout);
			break;
		}

		return EImageLayoutBarrier::Undefined;
	}

	inline VkPipelineStageFlags GetImageBarrierFlags(EImageLayoutBarrier Target, VkAccessFlags& AccessFlags, VkImageLayout& Layout)
	{
		VkPipelineStageFlags StageFlags = (VkPipelineStageFlags)0;
		switch (Target)
		{
		case EImageLayoutBarrier::Undefined:
			AccessFlags = 0;
			StageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			Layout = VK_IMAGE_LAYOUT_UNDEFINED;
			break;

		case EImageLayoutBarrier::TransferDest:
			AccessFlags = VK_ACCESS_TRANSFER_WRITE_BIT;
			StageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			break;

		case EImageLayoutBarrier::ColorAttachment:
			AccessFlags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			StageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			break;

		case EImageLayoutBarrier::DepthStencilAttachment:
			AccessFlags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			StageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			break;

		case EImageLayoutBarrier::TransferSource:
			AccessFlags = VK_ACCESS_TRANSFER_READ_BIT;
			StageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			break;

		case EImageLayoutBarrier::Present:
			AccessFlags = 0;
			StageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			Layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			break;

		case EImageLayoutBarrier::PixelShaderRead:
			AccessFlags = VK_ACCESS_SHADER_READ_BIT;
			StageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			break;

		case EImageLayoutBarrier::PixelDepthStencilRead:
			AccessFlags = VK_ACCESS_SHADER_READ_BIT;
			StageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			break;

		case EImageLayoutBarrier::ComputeGeneralRW:
			AccessFlags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			StageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			Layout = VK_IMAGE_LAYOUT_GENERAL;
			break;

		default:
			checkf(0, TEXT("Unknown ImageLayoutBarrier %d"), (int32)Target);
			break;
		}

		return StageFlags;
	}

	inline VkImageLayout GetImageLayout(EImageLayoutBarrier Target)
	{
		VkAccessFlags Flags;
		VkImageLayout Layout;
		GetImageBarrierFlags(Target, Flags, Layout);
		return Layout;
	}

	inline void SetImageBarrierInfo(EImageLayoutBarrier Source, EImageLayoutBarrier Dest, VkImageMemoryBarrier& InOutBarrier, VkPipelineStageFlags& InOutSourceStage, VkPipelineStageFlags& InOutDestStage)
	{
		InOutSourceStage |= GetImageBarrierFlags(Source, InOutBarrier.srcAccessMask, InOutBarrier.oldLayout);
		InOutDestStage |= GetImageBarrierFlags(Dest, InOutBarrier.dstAccessMask, InOutBarrier.newLayout);
	}

	void ImagePipelineBarrier(VkCommandBuffer CmdBuffer, VkImage Image, EImageLayoutBarrier SourceTransition, EImageLayoutBarrier DestTransition, const VkImageSubresourceRange& SubresourceRange);

	inline VkImageSubresourceRange SetupImageSubresourceRange(VkImageAspectFlags Aspect = VK_IMAGE_ASPECT_COLOR_BIT, uint32 StartMip = 0)
	{
		VkImageSubresourceRange Range;
		FMemory::Memzero(Range);
		Range.aspectMask = Aspect;
		Range.baseMipLevel = StartMip;
		Range.levelCount = 1;
		Range.baseArrayLayer = 0;
		Range.layerCount = 1;
		return Range;
	}

	inline VkImageMemoryBarrier SetupImageMemoryBarrier(VkImage Image, VkImageAspectFlags Aspect, uint32 NumMips = 1)
	{
		VkImageMemoryBarrier Barrier;
		FMemory::Memzero(Barrier);
		Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		Barrier.image = Image;
		Barrier.subresourceRange.aspectMask = Aspect;
		Barrier.subresourceRange.levelCount = NumMips;
		Barrier.subresourceRange.layerCount = 1;
		Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		return Barrier;
	}
}
