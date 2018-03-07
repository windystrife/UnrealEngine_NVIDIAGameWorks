// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Implementation of Memory Allocation Strategies

//-----------------------------------------------------------------------------
//	Include Files
//-----------------------------------------------------------------------------
#include "D3D12RHIPrivate.h"
#include "D3D12Allocation.h"
#include "Misc/BufferedOutputDevice.h"

#if PLATFORM_XBOXONE
#define PIX_MEMORY_PROFILING XBOXONE_PROFILING_ENABLED
#else
#define PIX_MEMORY_PROFILING 0
#endif

#if PIX_MEMORY_PROFILING
#include "AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <pixmemory.h>
#endif

namespace ED3D12AllocatorID
{
	enum Type
	{
		DefaultBufferAllocator,
		DynamicHeapAllocator,
		TextureAllocator,
		DefaultBufferAllocatorFullResources
	};
};

//-----------------------------------------------------------------------------
//	Allocator Base
//-----------------------------------------------------------------------------
FD3D12ResourceAllocator::FD3D12ResourceAllocator(FD3D12Device* ParentDevice,
	const GPUNodeMask& VisibleNodes,
	const FString& Name,
	D3D12_HEAP_TYPE InHeapType,
	D3D12_RESOURCE_FLAGS Flags,
	uint32 MaxSizeForPooling)
	: DebugName(Name)
	, HeapType(InHeapType)
	, ResourceFlags(Flags)
	, Initialized(false)
	, MaximumAllocationSizeForPooling(MaxSizeForPooling)
#if defined(UE_BUILD_DEBUG)
	, PeakUsage(0)
	, SpaceUsed(0)
	, InternalFragmentation(0)
	, NumBlocksInDeferredDeletionQueue(0)
	, FailedAllocationSpace(0)
#endif
	, FD3D12DeviceChild(ParentDevice)
	, FD3D12MultiNodeGPUObject(ParentDevice->GetNodeMask(), VisibleNodes)
{
}

FD3D12ResourceAllocator::~FD3D12ResourceAllocator()
{
}

//-----------------------------------------------------------------------------
//	Buddy Allocator
//-----------------------------------------------------------------------------

FD3D12BuddyAllocator::FD3D12BuddyAllocator(FD3D12Device* ParentDevice, 
	const GPUNodeMask& VisibleNodes,
	const FString& Name,
	eBuddyAllocationStrategy InAllocationStrategy,
	D3D12_HEAP_TYPE HeapType,
	D3D12_HEAP_FLAGS HeapFlags,
	D3D12_RESOURCE_FLAGS Flags,
	uint32 MaxSizeForPooling,
	uint32 InAllocatorID,
	uint32 InMaxBlockSize,
	uint32 InMinBlockSize)
	: AllocationStrategy(InAllocationStrategy)
	, MaxBlockSize(InMaxBlockSize)
	, MinBlockSize(InMinBlockSize)
	, HeapFlags(HeapFlags)
	, BackingHeap(nullptr)
	, HeapFullMessageDisplayed(false)
	, TotalSizeUsed(0)
	, AllocatorID(InAllocatorID)
	, FD3D12ResourceAllocator(ParentDevice, VisibleNodes, Name, HeapType, Flags, MaxSizeForPooling)
{
	// maxBlockSize should be evenly dividable by MinBlockSize and  
	// maxBlockSize / MinBlockSize should be a power of two  
	check((MaxBlockSize / MinBlockSize) * MinBlockSize == MaxBlockSize); // Evenly dividable  
	check(0 == ((MaxBlockSize / MinBlockSize) & ((MaxBlockSize / MinBlockSize) - 1))); // Power of two  

	MaxOrder = UnitSizeToOrder(SizeToUnitSize(MaxBlockSize));

	Reset();
}

void FD3D12BuddyAllocator::Initialize()
{
	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();

	if (AllocationStrategy == eBuddyAllocationStrategy::kPlacedResourceStrategy)
	{
		D3D12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(HeapType);
		HeapProps.CreationNodeMask = GetNodeMask();
		HeapProps.VisibleNodeMask = GetVisibilityMask();

		D3D12_HEAP_DESC Desc = {};
		Desc.SizeInBytes = MaxBlockSize;
		Desc.Properties = HeapProps;
		Desc.Alignment = 0;
		Desc.Flags = HeapFlags;

		ID3D12Heap* Heap = nullptr;
		{
			LLM_PLATFORM_SCOPE(ELLMTag::GraphicsPlatform);

			// we are tracking allocations ourselves, so don't let XMemAlloc track these as well
			LLM_SCOPED_PAUSE_TRACKING_FOR_TRACKER(ELLMTracker::Default);
			VERIFYD3D12RESULT(Adapter->GetD3DDevice()->CreateHeap(&Desc, IID_PPV_ARGS(&Heap)));
		}
		SetName(Heap, L"Placed Resource Allocator Backing Heap");

		BackingHeap = new FD3D12Heap(GetParentDevice(), GetVisibilityMask());
		BackingHeap->SetHeap(Heap);

		// Only track resources that cannot be accessed on the CPU.
		if (IsCPUInaccessible(HeapType))
		{
			BackingHeap->BeginTrackingResidency(Desc.SizeInBytes);
		}
	}
	else
	{
		{
			LLM_SCOPED_PAUSE_TRACKING_FOR_TRACKER(ELLMTracker::Default);
			VERIFYD3D12RESULT(Adapter->CreateBuffer(HeapType, GetNodeMask(), GetVisibilityMask(), MaxBlockSize, BackingResource.GetInitReference(), ResourceFlags));
		}
		SetName(BackingResource, L"Resource Allocator Underlying Buffer");

		if (IsCPUWritable(HeapType))
		{
			BackingResource->Map();
		}
	}
}

void FD3D12BuddyAllocator::Destroy()
{
	ReleaseAllResources();
}

uint32 FD3D12BuddyAllocator::AllocateBlock(uint32 order)
{
	uint32 offset;

	if (order > MaxOrder)
	{
		check(false); // Can't allocate a block that large  
	}

	if (FreeBlocks[order].Num() == 0)
	{
		// No free nodes in the requested pool.  Try to find a higher-order block and split it.  
		uint32 left = AllocateBlock(order + 1);

		uint32 size = OrderToUnitSize(order);

		uint32 right = left + size;

		FreeBlocks[order].Add(right); // Add the right block to the free pool  

		offset = left; // Return the left block  
	}

	else
	{
		TSet<uint32>::TConstIterator it(FreeBlocks[order]);
		offset = *it;

		// Remove the block from the free list
		FreeBlocks[order].Remove(*it);
	}

	return offset;
}

void FD3D12BuddyAllocator::DeallocateBlock(uint32 offset, uint32 order)
{
	// See if the buddy block is free  
	uint32 size = OrderToUnitSize(order);

	uint32 buddy = GetBuddyOffset(offset, size);

	uint32* it = FreeBlocks[order].Find(buddy);

	if (it != nullptr)
	{
		// Deallocate merged blocks
		DeallocateBlock(FMath::Min(offset, buddy), order + 1);
		// Remove the buddy from the free list  
		FreeBlocks[order].Remove(*it);
	}
	else
	{
		// Add the block to the free list
		FreeBlocks[order].Add(offset);
	}
}

void FD3D12BuddyAllocator::Allocate(uint32 SizeInBytes, uint32 Alignment, FD3D12ResourceLocation& ResourceLocation)
{
	FScopeLock Lock(&CS);

	if (Initialized == false)
	{
		Initialize();
		Initialized = true;
	}

	uint32 SizeToAllocate = SizeInBytes;

	// If the alignment doesn't match the block size
	if (Alignment != 0 && MinBlockSize % Alignment != 0)
	{
		SizeToAllocate = SizeInBytes + Alignment;
	}

	// Work out what size block is needed and allocate one
	const uint32 UnitSize = SizeToUnitSize(SizeToAllocate);
	const uint32 Order = UnitSizeToOrder(UnitSize);
	const uint32 Offset = AllocateBlock(Order); // This is the offset in MinBlockSize units

	const uint32 AllocSize = uint32(OrderToUnitSize(Order) * MinBlockSize);
	const uint32 AllocationBlockOffset = uint32(Offset * MinBlockSize);
	uint32 Padding = 0;

	if (Alignment != 0 && AllocationBlockOffset % Alignment != 0)
	{
		uint32 AlignedBlockOffset = AlignArbitrary(AllocationBlockOffset, Alignment);
		Padding = AlignedBlockOffset - AllocationBlockOffset;

		check((Padding + SizeInBytes) <= AllocSize)
	}

	INCREASE_ALLOC_COUNTER(SpaceUsed, AllocSize);
	INCREASE_ALLOC_COUNTER(InternalFragmentation, Padding);

	TotalSizeUsed += AllocSize;

#if defined(UE_BUILD_DEBUG)
	if (SpaceUsed > PeakUsage)
	{
		PeakUsage = SpaceUsed;
	}
#endif
	const uint32 AlignedOffsetFromResourceBase = AllocationBlockOffset + Padding;

	// Setup the info that this allocator
	FD3D12BuddyAllocatorPrivateData& PrivateData = ResourceLocation.GetBuddyAllocatorPrivateData();
	PrivateData.Order = Order;
	PrivateData.Offset = Offset;

	ResourceLocation.SetType(FD3D12ResourceLocation::ResourceLocationType::eSubAllocation);
	ResourceLocation.SetAllocator((FD3D12BaseAllocatorType*)this);
	ResourceLocation.SetSize(SizeInBytes);
	ResourceLocation.SetOffsetFromBaseOfResource(AlignedOffsetFromResourceBase);

	if (AllocationStrategy == eBuddyAllocationStrategy::kManualSubAllocationStrategy)
	{
		ResourceLocation.SetResource(BackingResource);
		ResourceLocation.SetGPUVirtualAddress(BackingResource->GetGPUVirtualAddress() + AlignedOffsetFromResourceBase);

		if (IsCPUWritable(HeapType))
		{
			ResourceLocation.SetMappedBaseAddress((uint8*)BackingResource->GetResourceBaseAddress() + AlignedOffsetFromResourceBase);
		}
	}
	else
	{
		// Place resources are intialized elsewhere
	}

	if (Alignment != 0)
	{
		check(uint64(ResourceLocation.GetMappedBaseAddress()) % Alignment == 0);
		check(uint64(ResourceLocation.GetGPUVirtualAddress()) % Alignment == 0);
	}

#if PIX_MEMORY_PROFILING
	uint64 Addr = (ResourceLocation.GetGPUVirtualAddress() != 0ull) ? (uint64)ResourceLocation.GetGPUVirtualAddress() : AlignedOffsetFromResourceBase;
	PIXRecordMemoryAllocationEvent(AllocatorID, (void*)(Addr), SizeInBytes, MaximumAllocationSizeForPooling);
#endif

	// track the allocation
	LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default, (void*)ResourceLocation.GetGPUVirtualAddress(), SizeInBytes));
}

bool FD3D12BuddyAllocator::TryAllocate(uint32 SizeInBytes, uint32 Alignment, FD3D12ResourceLocation& ResourceLocation)
{
	FScopeLock Lock(&CS);

	if (CanAllocate(SizeInBytes, Alignment))
	{
		Allocate(SizeInBytes, Alignment, ResourceLocation);
		return true;
	}
	else
	{
		INCREASE_ALLOC_COUNTER(FailedAllocationSpace, SizeInBytes);
		return false;
	}
}

void FD3D12BuddyAllocator::Deallocate(FD3D12ResourceLocation& ResourceLocation)
{
	check(IsOwner(ResourceLocation));
	// Blocks are cleaned up async so need a lock
	FScopeLock Lock(&CS);

	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	FD3D12Fence& FrameFence = Adapter->GetFrameFence();

	DeferredDeletionQueue.AddUninitialized();
	RetiredBlock& Block = DeferredDeletionQueue.Last();
	Block.FrameFence = FrameFence.GetCurrentFence();
	FD3D12BuddyAllocatorPrivateData& PrivateData = ResourceLocation.GetBuddyAllocatorPrivateData();
	Block.Data.Order = PrivateData.Order;
	Block.Data.Offset = PrivateData.Offset;

#if defined(UE_BUILD_DEBUG)
	Block.Padding = uint32(OrderToUnitSize(Block.Data.Order) * MinBlockSize) - ResourceLocation.GetSize();
#endif

	if (ResourceLocation.GetResource()->IsPlacedResource())
	{
		Block.PlacedResource = ResourceLocation.GetResource();
	}

	INCREASE_ALLOC_COUNTER(NumBlocksInDeferredDeletionQueue, 1);

#if PIX_MEMORY_PROFILING
	uint64 Addr = (ResourceLocation.GetGPUVirtualAddress() != 0ull) ? (uint64)ResourceLocation.GetGPUVirtualAddress() : ResourceLocation.GetOffsetFromBaseOfResource();
	PIXRecordMemoryFreeEvent(AllocatorID, (void*)Addr, 0, MaximumAllocationSizeForPooling);
#endif

	// track the allocation
	LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Default, (void*)ResourceLocation.GetGPUVirtualAddress(), 0));
}

void FD3D12BuddyAllocator::DeallocateInternal(RetiredBlock& Block)
{
	DeallocateBlock(Block.Data.Offset, Block.Data.Order);

	const uint32 Size = uint32(OrderToUnitSize(Block.Data.Order) * MinBlockSize);
	DECREASE_ALLOC_COUNTER(SpaceUsed, Size);
	DECREASE_ALLOC_COUNTER(InternalFragmentation, Block.Padding);

	TotalSizeUsed -= Size;

	if (AllocationStrategy == eBuddyAllocationStrategy::kPlacedResourceStrategy)
	{
		// Release the resource
		check(Block.PlacedResource != nullptr);
		Block.PlacedResource->Release();
		Block.PlacedResource = nullptr;
	}
};

void FD3D12BuddyAllocator::CleanUpAllocations()
{
	FScopeLock Lock(&CS);

	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	FD3D12Fence& FrameFence = Adapter->GetFrameFence();

	uint32 PopCount = 0;
	for (int32 i = 0; i < DeferredDeletionQueue.Num(); i++)
	{
		RetiredBlock& Block = DeferredDeletionQueue[i];

		if (FrameFence.IsFenceComplete(Block.FrameFence))
		{
			DeallocateInternal(Block);
			DECREASE_ALLOC_COUNTER(NumBlocksInDeferredDeletionQueue, 1);
			PopCount = i + 1;
		}
		else
		{
			break;
		}
	}

	if (PopCount)
	{
		// clear out all of the released blocks, don't allow the array to shrink
		DeferredDeletionQueue.RemoveAt(0, PopCount, false);
	}
}

void FD3D12BuddyAllocator::ReleaseAllResources()
{
	LLM_SCOPED_PAUSE_TRACKING_FOR_TRACKER(ELLMTracker::Default);

	for (RetiredBlock& Block : DeferredDeletionQueue)
	{
		DeallocateInternal(Block);
		DECREASE_ALLOC_COUNTER(NumBlocksInDeferredDeletionQueue, 1);
	}

	DeferredDeletionQueue.Empty();

	if (BackingResource)
	{
		check(BackingResource->GetRefCount() == 1);
		BackingResource = nullptr;
	}

	if (BackingHeap)
	{
		BackingHeap->Destroy();
	}
}

void FD3D12BuddyAllocator::DumpAllocatorStats(class FOutputDevice& Ar)
{
#if defined(UE_BUILD_DEBUG)
	FBufferedOutputDevice BufferedOutput;
	{
		// This is the memory tracked inside individual allocation pools
		FD3D12DynamicRHI* D3DRHI = FD3D12DynamicRHI::GetD3DRHI();
		FName categoryName(&DebugName.GetCharArray()[0]);

		BufferedOutput.CategorizedLogf(categoryName, ELogVerbosity::Log, TEXT(""));
		BufferedOutput.CategorizedLogf(categoryName, ELogVerbosity::Log, TEXT("Heap Size | MinBlock Size | Space Used | Peak Usage | Unpooled Allocations | Internal Fragmentation | Blocks in Deferred Delete Queue "));
		BufferedOutput.CategorizedLogf(categoryName, ELogVerbosity::Log, TEXT("----------"));

		BufferedOutput.CategorizedLogf(categoryName, ELogVerbosity::Log, TEXT("% 10i % 10i % 16i % 12i % 13i % 8i % 10I"),
			MaxBlockSize,
			MinBlockSize,
			SpaceUsed,
			PeakUsage,
			FailedAllocationSpace,
			InternalFragmentation,
			NumBlocksInDeferredDeletionQueue);
	}

	BufferedOutput.RedirectTo(Ar);
#endif
}

bool FD3D12BuddyAllocator::CanAllocate(uint32 size, uint32 alignment)
{
	if (TotalSizeUsed == MaxBlockSize)
	{
		return false;
	}

	uint32 sizeToAllocate = size;
	// If the alignment doesn't match the block size
	if (alignment != 0 && MinBlockSize % alignment != 0)
	{
		sizeToAllocate = size + alignment;
	}

	uint32 blockSize = MaxBlockSize;

	for (int32 i = FreeBlocks.Num() - 1; i >= 0; i--)
	{
		if (FreeBlocks[i].Num() && blockSize >= sizeToAllocate)
		{
			return true;
		}

		// Halve the block size;
		blockSize = blockSize >> 1;

		if (blockSize < sizeToAllocate) return false;
	}
	return false;
}

void FD3D12BuddyAllocator::Reset()
{
	// Clear the free blocks collection
	FreeBlocks.Empty();

	// Initialize the pool with a free inner block of max inner block size
	FreeBlocks.SetNum(MaxOrder + 1);
	FreeBlocks[MaxOrder].Add((uint32)0);
}

//-----------------------------------------------------------------------------
//	Multi-Buddy Allocator
//-----------------------------------------------------------------------------

FD3D12MultiBuddyAllocator::FD3D12MultiBuddyAllocator(FD3D12Device* ParentDevice,
	const GPUNodeMask& VisibleNodes,
	const FString& Name,
	eBuddyAllocationStrategy InAllocationStrategy,
	D3D12_HEAP_TYPE HeapType,
	D3D12_HEAP_FLAGS InHeapFlags,
	D3D12_RESOURCE_FLAGS Flags,
	uint32 MaxSizeForPooling,
	uint32 InAllocatorID,
	uint32 InMaxBlockSize,
	uint32 InMinBlockSize) :
	AllocationStrategy(InAllocationStrategy)
	, HeapFlags(InHeapFlags)
	, MaxBlockSize(InMaxBlockSize)
	, MinBlockSize(InMinBlockSize)
	, AllocatorID(InAllocatorID)
	, FD3D12ResourceAllocator(ParentDevice, VisibleNodes, Name, HeapType, Flags, MaxSizeForPooling)
{}


bool FD3D12MultiBuddyAllocator::TryAllocate(uint32 SizeInBytes, uint32 Alignment, FD3D12ResourceLocation& ResourceLocation)
{
	FScopeLock Lock(&CS);

	for (int32 i = 0; i < Allocators.Num(); i++)
	{
		if (Allocators[i]->TryAllocate(SizeInBytes, Alignment, ResourceLocation))
		{
			return true;
		}
	}

	Allocators.Add(CreateNewAllocator());
	return Allocators.Last()->TryAllocate(SizeInBytes, Alignment, ResourceLocation);
}

void FD3D12MultiBuddyAllocator::Deallocate(FD3D12ResourceLocation& ResourceLocation)
{
	//The sub-allocators should handle the deallocation
	check(false);
}

FD3D12BuddyAllocator* FD3D12MultiBuddyAllocator::CreateNewAllocator()
{
	return new FD3D12BuddyAllocator(GetParentDevice(),
		GetVisibilityMask(),
		DebugName,
		AllocationStrategy,
		HeapType,
		HeapFlags,
		ResourceFlags,
		MaximumAllocationSizeForPooling,
		AllocatorID,
		MaxBlockSize,
		MinBlockSize);
}

void FD3D12MultiBuddyAllocator::Initialize()
{
	Allocators.Add(CreateNewAllocator());
}

void FD3D12MultiBuddyAllocator::Destroy()
{
	ReleaseAllResources();
}

void FD3D12MultiBuddyAllocator::CleanUpAllocations()
{
	FScopeLock Lock(&CS);

	for (auto*& Allocator : Allocators)
	{
		Allocator->CleanUpAllocations();
	}

	// Trim empty allocators
	for (int32 i = (Allocators.Num() - 1); i >= 0; i--)
	{
		if (Allocators[i]->IsEmpty())
		{
			Allocators[i]->Destroy();
			delete(Allocators[i]);
			Allocators.RemoveAt(i);
		}
	}
}

void FD3D12MultiBuddyAllocator::DumpAllocatorStats(class FOutputDevice& Ar)
{
	//TODO
}

void FD3D12MultiBuddyAllocator::ReleaseAllResources()
{
	for (int32 i = (Allocators.Num() - 1); i >= 0; i--)
	{
		Allocators[i]->Destroy();
		delete(Allocators[i]);
	}

	Allocators.Empty();
}

void FD3D12MultiBuddyAllocator::Reset()
{

}

//-----------------------------------------------------------------------------
//	Bucket Allocator
//-----------------------------------------------------------------------------
FD3D12BucketAllocator::FD3D12BucketAllocator(FD3D12Device* ParentDevice,
	const GPUNodeMask& VisibleNodes,
	const FString& Name,
	D3D12_HEAP_TYPE HeapType,
	D3D12_RESOURCE_FLAGS Flags,
	uint64 InBlockRetentionFrameCount) :
	BlockRetentionFrameCount(InBlockRetentionFrameCount),
	FD3D12ResourceAllocator(ParentDevice, VisibleNodes, Name, HeapType, Flags, 32 * 1024 * 1024)
{}

bool FD3D12BucketAllocator::TryAllocate(uint32 SizeInBytes, uint32 Alignment, FD3D12ResourceLocation& ResourceLocation)
{
	FScopeLock Lock(&CS);

	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();

	// Size cannot be smaller than the requested alignment
	SizeInBytes = FMath::Max(SizeInBytes, Alignment);

	uint32 Bucket = BucketFromSize(SizeInBytes, BucketShift);
	check(Bucket < NumBuckets);

	uint32 BlockSize = BlockSizeFromBufferSize(SizeInBytes, BucketShift);

	// If some odd alignment is requested, make sure the block can fulfill it.
	if (BlockSize % Alignment != 0)
	{
		const uint32 AlignedSizeInBytes = SizeInBytes + Alignment;
		Bucket = BucketFromSize(AlignedSizeInBytes, BucketShift);
		BlockSize = BlockSizeFromBufferSize(AlignedSizeInBytes, BucketShift);
	}

	FD3D12BlockAllocatorPrivateData& Block = ResourceLocation.GetBlockAllocatorPrivateData();

	// See if a block is already available in the bucket
	if (AvailableBlocks[Bucket].Dequeue(Block))
	{
		check(Block.ResourceHeap);
	}
	else
	{
		// No blocks of the requested size are available so make one
		FD3D12Resource* Resource = nullptr;
		void* BaseAddress = nullptr;

		// Allocate a block
		check(BlockSize >= SizeInBytes);

		if (FAILED(Adapter->CreateBuffer(HeapType, GetNodeMask(), GetVisibilityMask(), SizeInBytes < MIN_HEAP_SIZE ? MIN_HEAP_SIZE : SizeInBytes, &Resource, ResourceFlags)))
		{
			return false;
		}

		// Track the resource so we know when to delete it
		SubAllocatedResources.Add(Resource);

		if (IsCPUWritable(HeapType))
		{
			BaseAddress = Resource->Map();
			check(BaseAddress);
			check(BaseAddress == (uint8*)(((uint64)BaseAddress + Alignment - 1) & ~((uint64)Alignment - 1)));
		}

		// Init the block we will return
		Block.BucketIndex = Bucket;
		Block.Offset = 0;
		Block.ResourceHeap = Resource;
		Block.ResourceHeap->AddRef();

		// Chop up the rest of the resource into reusable blocks
		if (BlockSize < MIN_HEAP_SIZE)
		{
			// Create additional available blocks that can be sub-allocated from the same resource
			for (uint32 Offset = BlockSize; Offset <= MIN_HEAP_SIZE - BlockSize; Offset += BlockSize)
			{
				FD3D12BlockAllocatorPrivateData NewBlock = {};
				NewBlock.BucketIndex = Bucket;
				NewBlock.Offset = Offset;
				NewBlock.ResourceHeap = Resource;
				NewBlock.ResourceHeap->AddRef();

				// Add the bucket to the available list
				AvailableBlocks[Bucket].Enqueue(NewBlock);
			}
		}
	}

	uint64 AlignedBlockOffset = Block.Offset;
	if (Alignment != 0 && AlignedBlockOffset % Alignment != 0)
	{
		AlignedBlockOffset = AlignArbitrary(AlignedBlockOffset, Alignment);
	}

	ResourceLocation.SetType(FD3D12ResourceLocation::ResourceLocationType::eSubAllocation);
	ResourceLocation.SetAllocator((FD3D12BaseAllocatorType*)this);
	ResourceLocation.SetResource(Block.ResourceHeap);
	ResourceLocation.SetSize(SizeInBytes);
	ResourceLocation.SetOffsetFromBaseOfResource(AlignedBlockOffset);
	ResourceLocation.SetGPUVirtualAddress(Block.ResourceHeap->GetGPUVirtualAddress() + AlignedBlockOffset);

	if (IsCPUWritable(HeapType))
	{
		ResourceLocation.SetMappedBaseAddress((void*)((uint64)Block.ResourceHeap->GetResourceBaseAddress() + AlignedBlockOffset));
	}

	// Check that when the offset is aligned that it doesn't go passed the end of the block
	check(ResourceLocation.GetOffsetFromBaseOfResource() - Block.Offset + SizeInBytes <= BlockSize);

	return true;
}

void FD3D12BucketAllocator::Deallocate(FD3D12ResourceLocation& ResourceLocation)
{
	FScopeLock Lock(&CS);

	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	FD3D12Fence& FrameFence = Adapter->GetFrameFence();

	FD3D12BlockAllocatorPrivateData& Block = ResourceLocation.GetBlockAllocatorPrivateData();
	Block.FrameFence = FrameFence.GetCurrentFence();

	ExpiredBlocks.Enqueue(Block);
}

void FD3D12BucketAllocator::Initialize()
{

}

void FD3D12BucketAllocator::Destroy()
{
	ReleaseAllResources();
}
void FD3D12BucketAllocator::CleanUpAllocations()
{
	FScopeLock Lock(&CS);

	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	FD3D12Fence& FrameFence = Adapter->GetFrameFence();

#if SUB_ALLOCATED_DEFAULT_ALLOCATIONS
	const static uint32 MinCleanupBucket = FMath::Max<uint32>(0, BucketFromSize(MIN_HEAP_SIZE, BucketShift) - 4);
#else
	const static uint32 MinCleanupBucket = 0;
#endif

	// Start at bucket 8 since smaller buckets are sub-allocated resources
	// and would be fragmented by deleting blocks
	for (uint32 bucket = MinCleanupBucket; bucket < NumBuckets; bucket++)
	{
		FD3D12BlockAllocatorPrivateData BlockInQueue = {};
		const uint32 RetentionCount = BlockRetentionFrameCount;

		const auto& Functor = [&FrameFence, RetentionCount](const FD3D12BlockAllocatorPrivateData& Block) { return FrameFence.IsFenceComplete(Block.FrameFence + RetentionCount); };
		while (AvailableBlocks[bucket].Dequeue(BlockInQueue, Functor))
		{
			SAFE_RELEASE(BlockInQueue.ResourceHeap);
		}
	}

	FD3D12BlockAllocatorPrivateData BlockInQueue = {};

	const auto& Functor = [&FrameFence](const FD3D12BlockAllocatorPrivateData& Block) { return FrameFence.IsFenceComplete(Block.FrameFence); };
	while (ExpiredBlocks.Dequeue(BlockInQueue, Functor))
	{
		// Add the bucket to the available list
		AvailableBlocks[BlockInQueue.BucketIndex].Enqueue(BlockInQueue);
	}
}

void FD3D12BucketAllocator::DumpAllocatorStats(class FOutputDevice& Ar)
{
	//TODO:
}
void FD3D12BucketAllocator::ReleaseAllResources()
{
	const static uint32 MinCleanupBucket = 0;

	// Start at bucket 8 since smaller buckets are sub-allocated resources
	// and would be fragmented by deleting blocks
	for (uint32 bucket = MinCleanupBucket; bucket < NumBuckets; bucket++)
	{
		FD3D12BlockAllocatorPrivateData Block = {};
		while (AvailableBlocks[bucket].Dequeue(Block))
		{
			SAFE_RELEASE(Block.ResourceHeap);
		}
	}

	FD3D12BlockAllocatorPrivateData Block = {};

	while (ExpiredBlocks.Dequeue(Block))
	{
		if (Block.BucketIndex >= MinCleanupBucket) //-V547
		{
			SAFE_RELEASE(Block.ResourceHeap);
		}
	}

	for (FD3D12Resource*& Resource : SubAllocatedResources)
	{
		Resource->Release();
		delete(Resource);
	}
}

void FD3D12BucketAllocator::Reset()
{

}

//-----------------------------------------------------------------------------
//	Dynamic Buffer Allocator
//-----------------------------------------------------------------------------

FD3D12DynamicHeapAllocator::FD3D12DynamicHeapAllocator(FD3D12Adapter* InParent, FD3D12Device* InParentDevice, const FString& InName, eBuddyAllocationStrategy InAllocationStrategy,
	uint32 InMaxSizeForPooling,
	uint32 InMaxBlockSize,
	uint32 InMinBlockSize)
	: 
#ifdef USE_BUCKET_ALLOCATOR
	Allocator(InParentDevice,
		GetVisibilityMask(),
		InName,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_FLAG_NONE,
		5)
#else
	Allocator(InParentDevice,
		GetVisibilityMask(),
		InName,
		InAllocationStrategy,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
		D3D12_RESOURCE_FLAG_NONE,
		InMaxSizeForPooling,
		ED3D12AllocatorID::DynamicHeapAllocator,
		InMaxBlockSize,
		InMinBlockSize)
#endif
	, FD3D12AdapterChild(InParent)
	, FD3D12MultiNodeGPUObject(InParentDevice->GetNodeMask(), InParent->ActiveGPUMask()) // Dynamic heaps are upload memory, thus they can be trivially visibile to all GPUs
{
}

void FD3D12DynamicHeapAllocator::Init()
{

}

void* FD3D12DynamicHeapAllocator::AllocUploadResource(uint32 Size, uint32 Alignment, FD3D12ResourceLocation& ResourceLocation)
{
	FD3D12Adapter* Adapter = GetParentAdapter();

	ResourceLocation.Clear();

	//TODO: For some reason 0 sized buffers are being created and then expected to have a resource
	if (Size == 0)
	{
		Size = 16;
	}
	
	//Work loads like infiltrator create enourmous amounts of buffer space in setup
	//clean up as we go as it can even run out of memory before the first frame.
	if (Adapter->GetDeferredDeletionQueue().QueueSize() > 128)
	{
		Adapter->GetDeferredDeletionQueue().ReleaseResources(true);
		Allocator.CleanUpAllocations();
	}
	
	if (Size <= Allocator.MaximumAllocationSizeForPooling)
	{
		if (Allocator.TryAllocate(Size, Alignment, ResourceLocation))
		{
			return ResourceLocation.GetMappedBaseAddress();
		}
	}

	FD3D12Resource* NewResource = nullptr;

	//Allocate Standalone
	VERIFYD3D12RESULT(Adapter->CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, GetNodeMask(), GetVisibilityMask(), Size, &NewResource));
	SetName(NewResource, L"Stand Alone Upload Buffer");

	ResourceLocation.AsStandAlone(NewResource, Size);

	return ResourceLocation.GetMappedBaseAddress();
}

void FD3D12DynamicHeapAllocator::CleanUpAllocations()
{
	Allocator.CleanUpAllocations();
}

void FD3D12DynamicHeapAllocator::Destroy()
{
	Allocator.Destroy();
}

//-----------------------------------------------------------------------------
//	Default Buffer Allocator
//-----------------------------------------------------------------------------

FD3D12DefaultBufferPool::FD3D12DefaultBufferPool(FD3D12Device* InParent, FD3D12AllocatorType* InAllocator) :
	Allocator(InAllocator),
	FD3D12DeviceChild(InParent),
	FD3D12MultiNodeGPUObject(InAllocator->GetNodeMask(), InAllocator->GetVisibilityMask())
{
}

void FD3D12DefaultBufferPool::CleanUpAllocations()
{
	Allocator->CleanUpAllocations();
}


// Grab a buffer from the available buffers or create a new buffer if none are available

void FD3D12DefaultBufferPool::AllocDefaultResource(const D3D12_RESOURCE_DESC& Desc, FD3D12ResourceLocation& ResourceLocation, uint32 Alignment)
{
	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();

	// If the resource location owns a block, this will deallocate it.
	ResourceLocation.Clear();

	if (Desc.Width == 0) return;

	const bool PoolResource = Desc.Width < Allocator->MaximumAllocationSizeForPooling &&
		((Desc.Width % (1024 * 64)) != 0);
	
	if (PoolResource)
	{
		// Ensure we're allocating from the correct pool
		check(Desc.Flags == Allocator->ResourceFlags);
	
		if (Allocator->TryAllocate(Desc.Width, Alignment, ResourceLocation))
		{
			// Successfully sub-allocated
			return;
		}
	}

	FD3D12Resource* NewResource = nullptr;

	//Allocate Standalone
	VERIFYD3D12RESULT(Adapter->CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, GetNodeMask(), GetVisibilityMask(), Desc.Width, &NewResource, Allocator->ResourceFlags));
	SetName(NewResource, L"Stand Alone Default Buffer");

#if PIX_MEMORY_PROFILING && 0
	// Track absolute memory usage
	{
		D3D12_RESOURCE_ALLOCATION_INFO Info = Device->GetDevice()->GetResourceAllocationInfo(0, 1, &Desc);
		PIXRecordMemoryAllocationEvent(ED3D12AllocatorID::DefaultBufferAllocatorFullResources, (void*)(NewResource->GetGPUVirtualAddress()), Info.SizeInBytes, 0);
	}
#endif

	ResourceLocation.AsStandAlone(NewResource, Desc.Width);
}

FD3D12DefaultBufferAllocator::FD3D12DefaultBufferAllocator(FD3D12Device* InParent, const GPUNodeMask& VisibleNodes)
	: FD3D12DeviceChild(InParent)
	, FD3D12MultiNodeGPUObject(InParent->GetNodeMask(), VisibleNodes)
{
	FMemory::Memset(DefaultBufferPools, 0);
}

// Grab a buffer from the available buffers or create a new buffer if none are available
HRESULT FD3D12DefaultBufferAllocator::AllocDefaultResource(const D3D12_RESOURCE_DESC& Desc, FD3D12ResourceLocation& ResourceLocation, uint32 Alignment)
{
	FD3D12Device* Device = GetParentDevice();

	if (BufferIsWriteable(Desc))
	{
		FD3D12Adapter* Adapter = Device->GetParentAdapter();
		FD3D12Resource* NewResource = nullptr;

		//Allocate Standalone
		VERIFYD3D12RESULT(Adapter->CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, GetNodeMask(), GetVisibilityMask(), Desc.Width, &NewResource, Desc.Flags));
		SetName(NewResource, L"Stand Alone Default Buffer");

		ResourceLocation.AsStandAlone(NewResource, Desc.Width);
	}
	else
	{
		//NOTE: Indexing based on the resource flags looks weird but is necessary e.g. the flags dictate if the resource
		//      can be used as a UAV. So each type of buffer has to come from a separate pool.

		check((uint32)Desc.Flags < MAX_DEFAULT_POOLS);
		if (DefaultBufferPools[Desc.Flags] == nullptr)
		{
			FD3D12AllocatorType* Allocator = nullptr;

#ifdef USE_BUCKET_ALLOCATOR
			const FString Name(L"Default Buffer Bucket Allocator");
			Allocator = new FD3D12BucketAllocator(Device,
				GetVisibilityMask(),
				Name,
				D3D12_HEAP_TYPE_DEFAULT,
				Desc.Flags,
				5);
#else
			const FString Name(L"Default Buffer Multi Buddy Allocator");
			Allocator = new FD3D12MultiBuddyAllocator(Device,
				GetVisibilityMask(),
				Name,
				kManualSubAllocationStrategy,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
				Desc.Flags,
				DEFAULT_BUFFER_POOL_MAX_ALLOC_SIZE,
				ED3D12AllocatorID::DefaultBufferAllocator,
				DEFAULT_BUFFER_POOL_SIZE,
				16);
#endif

			DefaultBufferPools[Desc.Flags] = new FD3D12DefaultBufferPool(Device, Allocator);
		}

		DefaultBufferPools[Desc.Flags]->AllocDefaultResource(Desc, ResourceLocation, Alignment);
	}
	return S_OK;
}

void FD3D12DefaultBufferAllocator::FreeDefaultBufferPools()
{
	for (uint32 i = 0; i < MAX_DEFAULT_POOLS; ++i)
	{
		if (DefaultBufferPools[i])
		{
			DefaultBufferPools[i]->CleanUpAllocations();
		}
		delete DefaultBufferPools[i];
		DefaultBufferPools[i] = nullptr;
	}
}

void FD3D12DefaultBufferAllocator::CleanupFreeBlocks()
{
	for (uint32 i = 0; i < MAX_DEFAULT_POOLS; ++i)
	{
		if (DefaultBufferPools[i])
		{
			DefaultBufferPools[i]->CleanUpAllocations();
		}
	}
}

//-----------------------------------------------------------------------------
//	Texture Allocator
//-----------------------------------------------------------------------------

FD3D12TextureAllocator::FD3D12TextureAllocator(FD3D12Device* Device,
	const GPUNodeMask& VisibleNodes,
	const FString& Name,
	uint32 HeapSize,
	D3D12_HEAP_FLAGS Flags) :
	FD3D12MultiBuddyAllocator(Device,
		VisibleNodes,
		Name,
		kPlacedResourceStrategy,
		D3D12_HEAP_TYPE_DEFAULT,
		Flags | D3D12_HEAP_FLAG_DENY_BUFFERS,
		D3D12_RESOURCE_FLAG_NONE,
		D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		ED3D12AllocatorID::TextureAllocator,
		HeapSize,
		D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT)
{
	// Inform the texture streaming system of this heap so that it correctly accounts for placed textures
	FD3D12DynamicRHI::GetD3DRHI()->UpdataTextureMemorySize((MaxBlockSize / 1024));
}

FD3D12TextureAllocator::~FD3D12TextureAllocator()
{
	FD3D12DynamicRHI::GetD3DRHI()->UpdataTextureMemorySize(-int32(MaxBlockSize / 1024));
}

HRESULT FD3D12TextureAllocator::AllocateTexture(D3D12_RESOURCE_DESC Desc, const D3D12_CLEAR_VALUE* ClearValue, FD3D12ResourceLocation& TextureLocation, const D3D12_RESOURCE_STATES InitialState, bool bForcePlacementCreation)
{
	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();

	HRESULT hr = S_OK;
	FD3D12Resource* NewResource = nullptr;

	TextureLocation.Clear();

	D3D12_RESOURCE_ALLOCATION_INFO Info = Device->GetDevice()->GetResourceAllocationInfo(0, 1, &Desc);
	
	if (Info.SizeInBytes < D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT)
	{
		if (TryAllocate(Info.SizeInBytes, Info.Alignment, TextureLocation))
		{
			FD3D12Heap* BackingHeap = ((FD3D12BuddyAllocator*)TextureLocation.GetAllocator())->GetBackingHeap();
	
			hr = Adapter->CreatePlacedResource(Desc, BackingHeap, TextureLocation.GetOffsetFromBaseOfResource(), InitialState, ClearValue, &NewResource);
	
			TextureLocation.SetType(FD3D12ResourceLocation::ResourceLocationType::eSubAllocation);
			TextureLocation.SetResource(NewResource);
	
			return hr;
		}
	}

	// Request default alignment for stand alone textures
	Desc.Alignment = 0;
	const D3D12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT, GetNodeMask(), GetVisibilityMask());

	if (bForcePlacementCreation)
	{
		hr = Adapter->CreatePlacedResourceWithHeap(Desc, HeapProps, InitialState, ClearValue, &NewResource);
	}
	else
	{
		hr = Adapter->CreateCommittedResource(Desc, HeapProps, InitialState, ClearValue, &NewResource);
	}

	TextureLocation.SetType(FD3D12ResourceLocation::ResourceLocationType::eStandAlone);
	TextureLocation.SetResource(NewResource);

	return hr;
}

FD3D12TextureAllocatorPool::FD3D12TextureAllocatorPool(FD3D12Device* Device, const GPUNodeMask& VisibilityNode) :
	ReadOnlyTexturePool(Device, VisibilityNode, FString(L"Small Read-Only Texture allocator"), TEXTURE_POOL_SIZE, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES),
	FD3D12DeviceChild(Device),
	FD3D12MultiNodeGPUObject(Device->GetNodeMask(), VisibilityNode)
{};

HRESULT FD3D12TextureAllocatorPool::AllocateTexture(D3D12_RESOURCE_DESC Desc, const D3D12_CLEAR_VALUE* ClearValue, uint8 UEFormat, FD3D12ResourceLocation& TextureLocation, const D3D12_RESOURCE_STATES InitialState, bool bForcePlacementCreation)
{
	// 4KB alignment is only available for read only textures
	if ((Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET ||
		Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL ||
		Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == false &&
		Desc.SampleDesc.Count == 1)// Multi-Sample texures have much larger alignment requirements (4MB vs 64KB)
	{
		// The top mip level must be less than 64k
		if (TextureCanBe4KAligned(Desc, UEFormat))
		{
			Desc.Alignment = D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT; // request 4k alignment
			return ReadOnlyTexturePool.AllocateTexture(Desc, ClearValue, TextureLocation, InitialState);
		}
	}

	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	FD3D12Resource* Resource = nullptr;

	const D3D12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT, GetNodeMask(), GetVisibilityMask());
	HRESULT hr;
	if (bForcePlacementCreation)
	{
		hr = Adapter->CreatePlacedResourceWithHeap(Desc, HeapProps, InitialState, ClearValue, &Resource);
	}
	else
	{
		hr = Adapter->CreateCommittedResource(Desc, HeapProps, InitialState, ClearValue, &Resource);
	}

	TextureLocation.SetType(FD3D12ResourceLocation::ResourceLocationType::eStandAlone);
	TextureLocation.SetResource(Resource);

	return hr;
}

//-----------------------------------------------------------------------------
//	Fast Allocation
//-----------------------------------------------------------------------------

template void* FD3D12FastAllocator::Allocate<FD3D12ScopeLock>(uint32 Size, uint32 Alignment, class FD3D12ResourceLocation* ResourceLocation);
template void* FD3D12FastAllocator::Allocate<FD3D12ScopeNoLock>(uint32 Size, uint32 Alignment, class FD3D12ResourceLocation* ResourceLocation);

template void FD3D12FastAllocator::CleanupPages<FD3D12ScopeLock>(uint64 FrameLag);
template void FD3D12FastAllocator::CleanupPages<FD3D12ScopeNoLock>(uint64 FrameLag);

template void FD3D12FastAllocator::Destroy<FD3D12ScopeLock>();
template void FD3D12FastAllocator::Destroy<FD3D12ScopeNoLock>();

FD3D12FastAllocator::FD3D12FastAllocator(FD3D12Device* Parent, const GPUNodeMask& VisibiltyMask, D3D12_HEAP_TYPE InHeapType, uint32 PageSize)
	: PagePool(Parent, VisibiltyMask, InHeapType, PageSize)
	, CurrentAllocatorPage(nullptr)
	, FD3D12DeviceChild(Parent)
	, FD3D12MultiNodeGPUObject(Parent->GetNodeMask(), VisibiltyMask)
{}

FD3D12FastAllocator::FD3D12FastAllocator(FD3D12Device* Parent, const GPUNodeMask& VisibiltyMask, const D3D12_HEAP_PROPERTIES& InHeapProperties, uint32 PageSize)
	: PagePool(Parent, VisibiltyMask, InHeapProperties, PageSize)
	, CurrentAllocatorPage(nullptr)
	, FD3D12DeviceChild(Parent)
	, FD3D12MultiNodeGPUObject(Parent->GetNodeMask(), VisibiltyMask)
{}

template<typename LockType>
void* FD3D12FastAllocator::Allocate(uint32 Size, uint32 Alignment, class FD3D12ResourceLocation* ResourceLocation)
{
	LockType Lock(&CS);

	// Check to make sure our assumption that we don't need a ResourceLocation->Clear() here is valid.
	checkf(!ResourceLocation->IsValid(), TEXT("The supplied resource location already has a valid resource. You should Clear() it first or it may leak."));

	if (Size > PagePool.GetPageSize())
	{
		FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();

		//Allocations are 64k aligned
		if (Alignment)
		{
			Alignment = (D3D_BUFFER_ALIGNMENT % Alignment) == 0 ? 0 : Alignment;
		}

		FD3D12Resource* Resource = nullptr;
		VERIFYD3D12RESULT(Adapter->CreateBuffer(PagePool.GetHeapType(), GetNodeMask(), GetVisibilityMask(), Size + Alignment, &Resource));
		SetName(Resource, L"Stand Alone Fast Allocation");

		void* Data = nullptr;
		if (PagePool.IsCPUWritable())
		{
			Data = Resource->Map();
		}
		ResourceLocation->AsStandAlone(Resource, Size + Alignment);

		return Data;
	}
	else
	{
		const uint32 Offset = (CurrentAllocatorPage) ? CurrentAllocatorPage->NextFastAllocOffset : 0;
		uint32 CurrentOffset = AlignArbitrary(Offset, Alignment);

		// See if there is room in the current pool
		if (CurrentAllocatorPage == nullptr || PagePool.GetPageSize() < CurrentOffset + Size)
		{
			if (CurrentAllocatorPage)
			{
				PagePool.ReturnFastAllocatorPage(CurrentAllocatorPage);
			}
			CurrentAllocatorPage = PagePool.RequestFastAllocatorPage();

			CurrentOffset = AlignArbitrary(CurrentAllocatorPage->NextFastAllocOffset, Alignment);
		}

		check(PagePool.GetPageSize() - Size >= CurrentOffset);

		// Create a FD3D12ResourceLocation representing a sub-section of the pool resource
		ResourceLocation->AsFastAllocation(CurrentAllocatorPage->FastAllocBuffer.GetReference(),
			Size,
			CurrentAllocatorPage->FastAllocBuffer->GetGPUVirtualAddress(),
			CurrentAllocatorPage->FastAllocData,
			CurrentOffset);

		CurrentAllocatorPage->NextFastAllocOffset = CurrentOffset + Size;

		check(ResourceLocation->GetMappedBaseAddress());
		return ResourceLocation->GetMappedBaseAddress();
	}
}

template<typename LockType>
void FD3D12FastAllocator::CleanupPages(uint64 FrameLag)
{
	LockType Lock(&CS);
	PagePool.CleanupPages(FrameLag);
}

template<typename LockType>
void FD3D12FastAllocator::Destroy()
{
	LockType Lock(&CS);

	if (CurrentAllocatorPage)
	{
		PagePool.ReturnFastAllocatorPage(CurrentAllocatorPage);
		CurrentAllocatorPage = nullptr;
	}

	PagePool.Destroy();
}

FD3D12FastAllocatorPagePool::FD3D12FastAllocatorPagePool(FD3D12Device* Parent, const GPUNodeMask& VisibiltyMask, D3D12_HEAP_TYPE InHeapType, uint32 Size)
	: PageSize(Size)
	, HeapProperties(CD3DX12_HEAP_PROPERTIES(InHeapType, Parent->GetNodeMask(), VisibiltyMask))
	, FD3D12DeviceChild(Parent)
	, FD3D12MultiNodeGPUObject(Parent->GetNodeMask(), VisibiltyMask)
{};

FD3D12FastAllocatorPagePool::FD3D12FastAllocatorPagePool(FD3D12Device* Parent, const GPUNodeMask& VisibiltyMask, const D3D12_HEAP_PROPERTIES& InHeapProperties, uint32 Size)
	: PageSize(Size)
	, HeapProperties(InHeapProperties)
	, FD3D12DeviceChild(Parent)
	, FD3D12MultiNodeGPUObject(Parent->GetNodeMask(), VisibiltyMask)
{};

FD3D12FastAllocatorPage* FD3D12FastAllocatorPagePool::RequestFastAllocatorPage()
{
	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();
	FD3D12Fence& Fence = Adapter->GetFrameFence();

	FD3D12FastAllocatorPage* Page = nullptr;

	const uint64 CompletedFence = Fence.GetLastCompletedFence();

	for (int32 Index = 0; Index < Pool.Num(); Index++)
	{
		//If the GPU is done with it and no-one has a lock on it
		if (Pool[Index]->FastAllocBuffer->GetRefCount() == 1 &&
			Pool[Index]->FrameFence <= CompletedFence)
		{
			Page = Pool[Index];
			Page->Reset();
			Pool.RemoveAt(Index);
			return Page;
		}
	}

	check(Page == nullptr);
	Page = new FD3D12FastAllocatorPage(PageSize);

	VERIFYD3D12RESULT(Adapter->CreateBuffer(HeapProperties, PageSize, Page->FastAllocBuffer.GetInitReference()));
	SetName(Page->FastAllocBuffer, L"Fast Allocator Page");

	Page->FastAllocData = Page->FastAllocBuffer->Map();
	return Page;
}

void FD3D12FastAllocatorPagePool::ReturnFastAllocatorPage(FD3D12FastAllocatorPage* Page)
{
	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	FD3D12Fence& FrameFence = Adapter->GetFrameFence();

	// Extend the lifetime of these resources when in AFR as other nodes might be relying on this
	Page->FrameFence = FrameFence.GetCurrentFence();
	Pool.Add(Page);
}

void FD3D12FastAllocatorPagePool::CleanupPages(uint64 FrameLag)
{
	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	FD3D12Fence& FrameFence = Adapter->GetFrameFence();

	const uint64 CompletedFence = FrameFence.GetLastCompletedFence();

	FD3D12FastAllocatorPage* Page = nullptr;

	int32 Index = Pool.Num() - 1;
	while (Index >= 0)
	{
		//If the GPU is done with it and no-one has a lock on it
		if (Pool[Index]->FastAllocBuffer->GetRefCount() == 1 &&
			Pool[Index]->FrameFence + FrameLag <= CompletedFence)
		{
			Page = Pool[Index];
			Pool.RemoveAt(Index);
			delete(Page);
			Page = nullptr;
		}

		Index--;
	}
}

void FD3D12FastAllocatorPagePool::Destroy()
{
	for (int32 i = 0; i < Pool.Num(); i++)
	{
		//check(Pool[i]->FastAllocBuffer->GetRefCount() == 1);
		{
			FD3D12FastAllocatorPage *Page = Pool[i];
			delete(Page);
			Page = nullptr;
		}
	}

	Pool.Empty();
}

FD3D12FastConstantAllocator::FD3D12FastConstantAllocator(FD3D12Device* Parent, const GPUNodeMask& VisibiltyMask, uint32 InPageSize)
	: RingBuffer(PageSize / D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
	, PageSize(InPageSize)
	, UnderlyingResource(Parent)
	, FD3D12DeviceChild(Parent)
	, FD3D12MultiNodeGPUObject(Parent->GetNodeMask(), VisibiltyMask)
{
	check(PageSize % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0);
}

void FD3D12FastConstantAllocator::Init()
{
	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();
	ReallocBuffer();

	RingBuffer.SetFence(&Adapter->GetFrameFence());
}

void FD3D12FastConstantAllocator::ReallocBuffer()
{
	check(PageSize % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0);

	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();

	UnderlyingResource.Clear();
	
	FD3D12Resource* NewBuffer = nullptr;
	VERIFYD3D12RESULT(Adapter->CreateBuffer(D3D12_HEAP_TYPE_UPLOAD,
		GetNodeMask(),
		GetVisibilityMask(),
		PageSize, &NewBuffer));

	UnderlyingResource.AsStandAlone(NewBuffer, PageSize);
}

#if USE_STATIC_ROOT_SIGNATURE
void* FD3D12FastConstantAllocator::Allocate(uint32 Bytes, FD3D12ResourceLocation& OutLocation, FD3D12ConstantBufferView* OutCBView)
#else
void* FD3D12FastConstantAllocator::Allocate(uint32 Bytes, FD3D12ResourceLocation& OutLocation)
#endif
{
	check(Bytes <= PageSize);

	// Check to make sure our assumption that we don't need a OutLocation.Clear() here is valid.
	checkf(!OutLocation.IsValid(), TEXT("The supplied resource location already has a valid resource. You should Clear() it first or it may leak."));

	// Align to a constant buffer block size
	const uint32 AlignedSize = Align(Bytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	const uint64 Location = RingBuffer.Allocate(AlignedSize / D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	if (Location == FD3D12AbstractRingBuffer::FailedReturnValue)
	{
		PageSize = Align(PageSize + (PageSize / 2), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		ReallocBuffer();
		RingBuffer.Reset(PageSize / D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		UE_LOG(LogD3D12RHI, Warning, TEXT("Constant Allocator had to grow! Consider making it larger to begin with. New size: %d bytes"), PageSize);

#if USE_STATIC_ROOT_SIGNATURE
		return Allocate(Bytes, OutLocation, OutCBView);
#else
		return Allocate(Bytes, OutLocation);
#endif
	}

	// Useful when trying to tweak initial size
	//UE_LOG(LogD3D12RHI, Warning, TEXT("Space Left. %d"), RingBuffer.GetSpaceLeft() * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	const uint64 Offset = Location * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

	OutLocation.AsFastAllocation(UnderlyingResource.GetResource(),
		AlignedSize,
		UnderlyingResource.GetGPUVirtualAddress(),
		UnderlyingResource.GetMappedBaseAddress(),
		Offset);

#if USE_STATIC_ROOT_SIGNATURE
	if (OutCBView)
	{
		OutCBView->Create(UnderlyingResource.GetGPUVirtualAddress() + Offset, AlignedSize);
	}
#endif
	return OutLocation.GetMappedBaseAddress();
}

#if PIX_MEMORY_PROFILING
#include "HideWindowsPlatformTypes.h"
#endif
