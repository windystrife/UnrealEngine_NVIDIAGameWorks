// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Allocation.h: A Collection of allocators
=============================================================================*/

#pragma once
#include "D3D12Resources.h"

class FD3D12ResourceAllocator : public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:

	FD3D12ResourceAllocator(FD3D12Device* ParentDevice,
		const GPUNodeMask& VisibleNodes,
		const FString& Name,
		D3D12_HEAP_TYPE HeapType,
		D3D12_RESOURCE_FLAGS Flags,
		uint32 MaxSizeForPooling);

	~FD3D12ResourceAllocator();

	// Any allocation larger than this just gets straight up allocated (i.e. not pooled).
	// These large allocations should be infrequent so the CPU overhead should be minimal
	const uint32 MaximumAllocationSizeForPooling;
	D3D12_RESOURCE_FLAGS ResourceFlags;

protected:

	const FString DebugName;

	bool Initialized;

	const D3D12_HEAP_TYPE HeapType;

	FCriticalSection CS;

#if defined(UE_BUILD_DEBUG)
	uint32 SpaceUsed;
	uint32 InternalFragmentation;
	uint32 NumBlocksInDeferredDeletionQueue;
	uint32 PeakUsage;
	uint32 FailedAllocationSpace;
#endif
};

//-----------------------------------------------------------------------------
//	Buddy Allocator
//-----------------------------------------------------------------------------
// Allocates blocks from a fixed range using buddy allocation method.
// Buddy allocation allows reasonably fast allocation of arbitrary size blocks
// with minimal fragmentation and provides efficient reuse of freed ranges.
// When a block is de-allocated an attempt is made to merge it with it's 
// neighbour (buddy) if it is contiguous and free.
// Based on reference implementation by MSFT: billkris

// Unfortunately the api restricts the minimum size of a placed buffer resource to 64k
#define MIN_PLACED_BUFFER_SIZE (64 * 1024)
#define D3D_BUFFER_ALIGNMENT (64 * 1024)

#if defined(UE_BUILD_DEBUG)
#define INCREASE_ALLOC_COUNTER(A, B) (A = A + B);
#define DECREASE_ALLOC_COUNTER(A, B) (A = A - B);
#else
#define INCREASE_ALLOC_COUNTER(A, B)
#define DECREASE_ALLOC_COUNTER(A, B)
#endif

enum eBuddyAllocationStrategy
{
	// This strategy uses Placed Resources to sub-allocate a buffer out of an underlying ID3D12Heap.
	// The benefit of this is that each buffer can have it's own resource state and can be treated
	// as any other buffer. The downside of this strategy is the API limitiation which enforces
	// the minimum buffer size to 64k leading to large internal fragmentation in the allocator
	kPlacedResourceStrategy,
	// The alternative is to manualy sub-allocate out of a single large buffer which allows block
	// allocation granularity down to 1 byte. However, this strategy is only really valid for buffers which
	// will be treated as read-only after their creation (i.e. most Index and Vertex buffers). This 
	// is because the underlying resource can only have one state at a time.
	kManualSubAllocationStrategy
};

class FD3D12BuddyAllocator : public FD3D12ResourceAllocator
{
public:

	FD3D12BuddyAllocator(FD3D12Device* ParentDevice,
		const GPUNodeMask& VisibleNodes,
		const FString& Name,
		eBuddyAllocationStrategy InAllocationStrategy,
		D3D12_HEAP_TYPE HeapType,
		D3D12_HEAP_FLAGS HeapFlags,
		D3D12_RESOURCE_FLAGS Flags,
		uint32 MaxSizeForPooling,
		uint32 InAllocatorID,
		uint32 InMaxBlockSize,
		uint32 InMinBlockSize = MIN_PLACED_BUFFER_SIZE);

	bool TryAllocate(uint32 SizeInBytes, uint32 Alignment, FD3D12ResourceLocation& ResourceLocation);

	void Deallocate(FD3D12ResourceLocation& ResourceLocation);

	void Initialize();

	void Destroy();

	void CleanUpAllocations();

	void DumpAllocatorStats(class FOutputDevice& Ar);

	void ReleaseAllResources();

	void Reset();

	inline bool IsEmpty()
	{
		return FreeBlocks[MaxOrder].Num() == 1;
	}

	inline uint32 GetTotalSizeUsed() const { return TotalSizeUsed; }

	inline FD3D12Heap* GetBackingHeap() { check(AllocationStrategy == kPlacedResourceStrategy); return BackingHeap.GetReference(); }

	inline bool IsOwner(FD3D12ResourceLocation& ResourceLocation)
	{
		return ResourceLocation.GetAllocator() == (FD3D12BaseAllocatorType*)this;
	}

protected:
	const uint32 MaxBlockSize;
	const uint32 MinBlockSize;
	const D3D12_HEAP_FLAGS HeapFlags;
	const eBuddyAllocationStrategy AllocationStrategy;
	const uint32 AllocatorID;

	TRefCountPtr<FD3D12Resource> BackingResource;
	TRefCountPtr<FD3D12Heap> BackingHeap;

private:
	struct RetiredBlock
	{
		FD3D12Resource* PlacedResource;
		uint64 FrameFence;
		FD3D12BuddyAllocatorPrivateData Data;
#if defined(UE_BUILD_DEBUG)
		// Padding is only need in debug builds to keep track of internal fragmentation for stats.
		uint32 Padding;
#endif
	};

	TArray<RetiredBlock> DeferredDeletionQueue;
	TArray<TSet<uint32>> FreeBlocks;
	uint32 MaxOrder;
	uint32 TotalSizeUsed;

	bool HeapFullMessageDisplayed;

	inline uint32 SizeToUnitSize(uint32 size) const
	{
		return (size + (MinBlockSize - 1)) / MinBlockSize;
	}

	inline uint32 UnitSizeToOrder(uint32 size) const
	{
		return uint32(ceil(log2f(float(size))));
	}

	inline uint32 GetBuddyOffset(const uint32 &offset, const uint32 &size)
	{
		return offset ^ size;
	}

	uint32 OrderToUnitSize(uint32 order) const { return ((uint32)1) << order; }
	uint32 AllocateBlock(uint32 order);
	void DeallocateBlock(uint32 offset, uint32 order);

	bool CanAllocate(uint32 size, uint32 alignment);

	void DeallocateInternal(RetiredBlock& Block);

	void Allocate(uint32 SizeInBytes, uint32 Alignment, FD3D12ResourceLocation& ResourceLocation);
};

//-----------------------------------------------------------------------------
//	Multi-Buddy Allocator
//-----------------------------------------------------------------------------
// Builds on top of the Buddy Allocator but covers some of it's deficiencies by
// managing multiple buddy allocator instances to better match memory usage over
// time.

class FD3D12MultiBuddyAllocator : public FD3D12ResourceAllocator
{
public:

	FD3D12MultiBuddyAllocator(FD3D12Device* ParentDevice,
		const GPUNodeMask& VisibleNodes,
		const FString& Name,
		eBuddyAllocationStrategy InAllocationStrategy,
		D3D12_HEAP_TYPE HeapType,
		D3D12_HEAP_FLAGS InHeapFlags,
		D3D12_RESOURCE_FLAGS Flags,
		uint32 MaxSizeForPooling,
		uint32 InAllocatorID,
		uint32 InMaxBlockSize,
		uint32 InMinBlockSize = MIN_PLACED_BUFFER_SIZE);

	bool TryAllocate(uint32 SizeInBytes, uint32 Alignment, FD3D12ResourceLocation& ResourceLocation);

	void Deallocate(FD3D12ResourceLocation& ResourceLocation);

	void Initialize();

	void Destroy();

	void CleanUpAllocations();

	void DumpAllocatorStats(class FOutputDevice& Ar);

	void ReleaseAllResources();

	void Reset();

protected:
	const eBuddyAllocationStrategy AllocationStrategy;
	const D3D12_HEAP_FLAGS HeapFlags;
	const uint32 MaxBlockSize;
	const uint32 MinBlockSize;
	const uint32 AllocatorID;

	FD3D12BuddyAllocator* CreateNewAllocator();

	TArray<FD3D12BuddyAllocator*> Allocators;
};

//-----------------------------------------------------------------------------
//	Bucket Allocator
//-----------------------------------------------------------------------------
// Resources are allocated from buckets, which are just a collection of resources of a particular size.
// Blocks can be an entire resource or a sub allocation from a resource.
class FD3D12BucketAllocator : public FD3D12ResourceAllocator
{
public:

	FD3D12BucketAllocator(FD3D12Device* ParentDevice,
		const GPUNodeMask& VisibleNodes,
		const FString& Name,
		D3D12_HEAP_TYPE HeapType,
		D3D12_RESOURCE_FLAGS Flags,
		uint64 InBlockRetentionFrameCount);

	bool TryAllocate(uint32 SizeInBytes, uint32 Alignment, FD3D12ResourceLocation& ResourceLocation);

	void Deallocate(FD3D12ResourceLocation& ResourceLocation);

	void Initialize();

	void Destroy();

	void CleanUpAllocations();

	void DumpAllocatorStats(class FOutputDevice& Ar);

	void ReleaseAllResources();

	void Reset();

private:

	static uint32 FORCEINLINE BucketFromSize(uint32 size, uint32 bucketShift)
	{
		uint32 bucket = FMath::CeilLogTwo(size);
		bucket = bucket < bucketShift ? 0 : bucket - bucketShift;
		return bucket;
	}

	static uint32 FORCEINLINE BlockSizeFromBufferSize(uint32 bufferSize, uint32 bucketShift)
	{
		const uint32 minSize = 1 << bucketShift;
		return bufferSize > minSize ? FMath::RoundUpToPowerOfTwo(bufferSize) : minSize;
	}

#if SUB_ALLOCATED_DEFAULT_ALLOCATIONS
	static const uint32 MIN_HEAP_SIZE = 256 * 1024;
#else
	static const uint32 MIN_HEAP_SIZE = 64 * 1024;
#endif

	static const uint32 BucketShift = 6;
	static const uint32 NumBuckets = 22; // bucket resource sizes range from 64 to 2^28 

	FThreadsafeQueue<FD3D12BlockAllocatorPrivateData> AvailableBlocks[NumBuckets];
	FThreadsafeQueue<FD3D12BlockAllocatorPrivateData> ExpiredBlocks;
	TArray<FD3D12Resource*> SubAllocatedResources;// keep a list of the sub-allocated resources so that they may be cleaned up

	// This frame count value helps makes sure that we don't delete resources too soon. If resources are deleted too soon,
	// we can get in a loop the heap allocator will be constantly deleting and creating resources every frame which
	// results in CPU stutters. DynamicRetentionFrameCount was tested and set to a value that appears to be adequate for
	// creating a stable state on the Infiltrator demo.
	const uint64 BlockRetentionFrameCount;
};

#ifdef USE_BUCKET_ALLOCATOR
typedef FD3D12BucketAllocator FD3D12AllocatorType;
#else
typedef FD3D12MultiBuddyAllocator FD3D12AllocatorType;
#endif

//-----------------------------------------------------------------------------
//	FD3D12DynamicHeapAllocator
//-----------------------------------------------------------------------------
// This is designed for allocation of scratch memory such as temporary staging buffers
// or shadow buffers for dynamic resources.
class FD3D12DynamicHeapAllocator : public FD3D12AdapterChild, public FD3D12MultiNodeGPUObject
{
public:

	FD3D12DynamicHeapAllocator(FD3D12Adapter* InParent, FD3D12Device* InParentDevice, const FString& InName, eBuddyAllocationStrategy InAllocationStrategy,
		uint32 InMaxSizeForPooling,
		uint32 InMaxBlockSize,
		uint32 InMinBlockSize);

	void Init();

	// Allocates <size> bytes from the end of an available resource heap.
	void* AllocUploadResource(uint32 size, uint32 alignment, FD3D12ResourceLocation& ResourceLocation);

	void CleanUpAllocations();

	void Destroy();

private:

	FD3D12AllocatorType Allocator;
};

//-----------------------------------------------------------------------------
//	FD3D12DefaultBufferPool
//-----------------------------------------------------------------------------
class FD3D12DefaultBufferPool : public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:
	FD3D12DefaultBufferPool(FD3D12Device* InParent, FD3D12AllocatorType* InAllocator);
	~FD3D12DefaultBufferPool() { delete Allocator; }

	// Grab a buffer from the available buffers or create a new buffer if none are available
	void AllocDefaultResource(const D3D12_RESOURCE_DESC& Desc, FD3D12ResourceLocation& ResourceLocation, uint32 Alignment);

	void CleanUpAllocations();

private:
	FD3D12AllocatorType* Allocator;
};

// FD3D12DefaultBufferAllocator
//
class FD3D12DefaultBufferAllocator : public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:
	FD3D12DefaultBufferAllocator(FD3D12Device* InParent, const GPUNodeMask& VisibleNodes);

	// Grab a buffer from the available buffers or create a new buffer if none are available
	HRESULT AllocDefaultResource(const D3D12_RESOURCE_DESC& pDesc, FD3D12ResourceLocation& ResourceLocation, uint32 Alignment);
	void FreeDefaultBufferPools();
	void CleanupFreeBlocks();

private:

	static const uint32 MAX_DEFAULT_POOLS = 16; // Should match the max D3D12_RESOURCE_FLAG_FLAG combinations.
	FD3D12DefaultBufferPool* DefaultBufferPools[MAX_DEFAULT_POOLS];

	bool BufferIsWriteable(const D3D12_RESOURCE_DESC& Desc)
	{
		const bool bDSV = (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0;
		const bool bRTV = (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0;
		const bool bUAV = (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0;

		// Buffer Depth Stencils are invalid
		check(bDSV == false);
		const bool bWriteable = bDSV || bRTV || bUAV;
		return bWriteable;
	}
};

//-----------------------------------------------------------------------------
//	FD3D12TextureAllocator
//-----------------------------------------------------------------------------

class FD3D12TextureAllocator : public FD3D12MultiBuddyAllocator
{
public:
	FD3D12TextureAllocator(FD3D12Device* Device, const GPUNodeMask& VisibleNodes, const FString& Name, uint32 HeapSize, D3D12_HEAP_FLAGS Flags);

	~FD3D12TextureAllocator();

	HRESULT AllocateTexture(D3D12_RESOURCE_DESC Desc, const D3D12_CLEAR_VALUE* ClearValue, FD3D12ResourceLocation& TextureLocation, const D3D12_RESOURCE_STATES InitialState, bool bForcePlacementCreation = false );
};

class FD3D12TextureAllocatorPool : public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:
	FD3D12TextureAllocatorPool(FD3D12Device* Device, const GPUNodeMask& VisibilityNode);

	HRESULT AllocateTexture(D3D12_RESOURCE_DESC Desc, const D3D12_CLEAR_VALUE* ClearValue, uint8 UEFormat, FD3D12ResourceLocation& TextureLocation, const D3D12_RESOURCE_STATES InitialState, bool bForcePlacementCreation = false );

	void CleanUpAllocations() { ReadOnlyTexturePool.CleanUpAllocations(); }

	void Destroy() { ReadOnlyTexturePool.Destroy(); }

private:
	FD3D12TextureAllocator ReadOnlyTexturePool;
};

//-----------------------------------------------------------------------------
//	Fast Allocation
//-----------------------------------------------------------------------------

struct FD3D12FastAllocatorPage
{
	FD3D12FastAllocatorPage() :
		NextFastAllocOffset(0)
		, PageSize(0)
		, FastAllocData(nullptr)
		, FrameFence(0) {};

	FD3D12FastAllocatorPage(uint32 Size) :
		PageSize(Size)
		, NextFastAllocOffset(0)
		, FastAllocData(nullptr)
		, FrameFence(0) {};

	void Reset()
	{
		NextFastAllocOffset = 0;
	}

	const uint32 PageSize;
	TRefCountPtr<FD3D12Resource> FastAllocBuffer;
	uint32 NextFastAllocOffset;
	void* FastAllocData;
	uint64 FrameFence;
};

class FD3D12FastAllocatorPagePool : public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:
	FD3D12FastAllocatorPagePool(FD3D12Device* Parent, const GPUNodeMask& VisibiltyMask, D3D12_HEAP_TYPE InHeapType, uint32 Size);

	FD3D12FastAllocatorPagePool(FD3D12Device* Parent, const GPUNodeMask& VisibiltyMask, const D3D12_HEAP_PROPERTIES& InHeapProperties, uint32 Size);

	FD3D12FastAllocatorPage* RequestFastAllocatorPage();
	void ReturnFastAllocatorPage(FD3D12FastAllocatorPage* Page);
	void CleanupPages(uint64 FrameLag);

	inline uint32 GetPageSize() const { return PageSize; }

	inline D3D12_HEAP_TYPE GetHeapType() const { return HeapProperties.Type; }
	inline bool IsCPUWritable() const { return ::IsCPUWritable(GetHeapType(), &HeapProperties); }

	void Destroy();

protected:

	const uint32 PageSize;
	const D3D12_HEAP_PROPERTIES HeapProperties;

	TArray<FD3D12FastAllocatorPage*> Pool;
};

class FD3D12FastAllocator : public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:
	FD3D12FastAllocator(FD3D12Device* Parent, const GPUNodeMask& VisibiltyMask, D3D12_HEAP_TYPE InHeapType, uint32 PageSize);
	FD3D12FastAllocator(FD3D12Device* Parent, const GPUNodeMask& VisibiltyMask, const D3D12_HEAP_PROPERTIES& InHeapProperties, uint32 PageSize);

	template<typename LockType>
	void* Allocate(uint32 Size, uint32 Alignment, class FD3D12ResourceLocation* ResourceLocation);

	template<typename LockType>
	void Destroy();

	template<typename LockType>
	void CleanupPages(uint64 FrameLag);

protected:
	FD3D12FastAllocatorPagePool PagePool;

	FD3D12FastAllocatorPage* CurrentAllocatorPage;

	FCriticalSection CS;
};

class FD3D12AbstractRingBuffer
{
public:
	FD3D12AbstractRingBuffer(uint64 BufferSize)
		: Size(BufferSize)
		, Head(BufferSize)
		, Tail(0)
		, LastFence(0)
		, OutstandingAllocs(0)
		, Fence(nullptr)
	{}

	static const uint64 FailedReturnValue = uint64(-1);

	inline void Reset(uint64 NewSize)
	{
		Size = NewSize;
		Head = Size;
		Tail = 0;
		LastFence = 0;
		OutstandingAllocs = 0;
	}

	inline void SetFence(FD3D12Fence* InFence)
	{
		Fence = InFence;
		LastFence = 0;
	}

	inline const uint64 GetSpaceLeft() const { return Head - Tail; }

	inline uint64 Allocate(uint64 Count)
	{
		uint64 ReturnValue = FailedReturnValue;
		const uint64 LastCompletedFence = Fence->GetCachedLastCompletedFence();

		uint64 PhysicalTail = Tail % Size;

		if (PhysicalTail + Count > Size)
		{
			// Force the wrap around by simply allocating the difference
			uint64 Padding = Allocate(Size - PhysicalTail);
			if (Padding == FailedReturnValue)
			{
				return FailedReturnValue;
			}

			PhysicalTail = Tail % Size;
		}

		// If progress has been made since we were here last
		if (LastCompletedFence > LastFence)
		{
			LastFence = LastCompletedFence;
			Head += OutstandingAllocs; // Deallocate completed blocks
			OutstandingAllocs = 0;
		}

		if (Tail + Count < Head)
		{
			ReturnValue = PhysicalTail;
			Tail += Count;
			OutstandingAllocs += Count;
		}

		return ReturnValue;
	}

private:
	FD3D12Fence* Fence;
	uint64 Size;
	uint64 Head;
	uint64 Tail;
	uint64 LastFence;
	uint64 OutstandingAllocs;
};

class FD3D12FastConstantAllocator : public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:
	FD3D12FastConstantAllocator(FD3D12Device* Parent, const GPUNodeMask& VisibiltyMask, uint32 InPageSize);

	void Init();

#if USE_STATIC_ROOT_SIGNATURE
	void* Allocate(uint32 Bytes, class FD3D12ResourceLocation& OutLocation, FD3D12ConstantBufferView* OutCBView);
#else
	void* Allocate(uint32 Bytes, class FD3D12ResourceLocation& OutLocation);
#endif


private:
	void ReallocBuffer();

	FD3D12ResourceLocation UnderlyingResource;

	uint32 PageSize;
	FD3D12AbstractRingBuffer RingBuffer;
};