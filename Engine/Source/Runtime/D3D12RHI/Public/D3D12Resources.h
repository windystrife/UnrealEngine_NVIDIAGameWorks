// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Resources.h: D3D resource RHI definitions.
	=============================================================================*/

#pragma once

#include "BoundShaderStateCache.h"
#include "D3D12ShaderResources.h"

// Forward Decls
class FD3D12Resource;
class FD3D12StateCacheBase;
class FD3D12CommandListManager;
class FD3D12CommandContext;
class FD3D12CommandListHandle;
struct FD3D12GraphicsPipelineState;
typedef FD3D12StateCacheBase FD3D12StateCache;

class FD3D12PendingResourceBarrier
{
public:

	FD3D12Resource*              Resource;
	D3D12_RESOURCE_STATES        State;
	uint32                       SubResource;
};

class FD3D12RefCount
{
public:
	FD3D12RefCount()
	{
	}
	virtual ~FD3D12RefCount()
	{
		check(NumRefs.GetValue() == 0);
	}
	uint32 AddRef() const
	{
		int32 NewValue = NumRefs.Increment();
		check(NewValue > 0);
		return uint32(NewValue);
	}
	uint32 Release() const
	{
		int32 NewValue = NumRefs.Decrement();
		if (NewValue == 0)
		{
			delete this;
		}
		check(NewValue >= 0);
		return uint32(NewValue);
	}
	uint32 GetRefCount() const
	{
		int32 CurrentValue = NumRefs.GetValue();
		check(CurrentValue >= 0);
		return uint32(CurrentValue);
	}
private:
	mutable FThreadSafeCounter NumRefs;
};

class FD3D12Heap : public FD3D12RefCount, public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
public:
	FD3D12Heap(FD3D12Device* Parent, GPUNodeMask VisibleNodes);
	~FD3D12Heap();

	inline ID3D12Heap* GetHeap() { return Heap.GetReference(); }
	inline void SetHeap(ID3D12Heap* HeapIn) { Heap = HeapIn; }

	void UpdateResidency(FD3D12CommandListHandle& CommandList);

	void BeginTrackingResidency(uint64 Size);

	void Destroy();

	inline FD3D12ResidencyHandle* GetResidencyHandle() { return &ResidencyHandle; }

private:
	TRefCountPtr<ID3D12Heap> Heap;
	FD3D12ResidencyHandle ResidencyHandle;
};

class FD3D12Resource : public FD3D12RefCount, public FD3D12DeviceChild, public FD3D12MultiNodeGPUObject
{
private:
	TRefCountPtr<ID3D12Resource> Resource;
	TRefCountPtr<FD3D12Heap> Heap;

	FD3D12ResidencyHandle ResidencyHandle;

	D3D12_RESOURCE_DESC Desc;
	uint8 PlaneCount;
	uint16 SubresourceCount;
	CResourceState ResourceState;
	D3D12_RESOURCE_STATES DefaultResourceState;
	D3D12_RESOURCE_STATES ReadableState;
	D3D12_RESOURCE_STATES WritableState;
	bool bRequiresResourceStateTracking;
	bool bDepthStencil;
	bool bDeferDelete;
	D3D12_HEAP_TYPE HeapType;
	D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddress;
	void* ResourceBaseAddress;
	FName DebugName;

	// NVCHANGE_BEGIN: Add VXGI
	bool bEnableUAVBarriers;
	bool bFirstUAVBarrierPlaced;
	// NVCHANGE_END: Add VXGI

#if UE_BUILD_DEBUG
	static int64 TotalResourceCount;
	static int64 NoStateTrackingResourceCount;
#endif

public:
	explicit FD3D12Resource(FD3D12Device* ParentDevice,
		GPUNodeMask VisibleNodes,
		ID3D12Resource* InResource,
		D3D12_RESOURCE_STATES InitialState,
		D3D12_RESOURCE_DESC const& InDesc,
		FD3D12Heap* InHeap = nullptr,
		D3D12_HEAP_TYPE InHeapType = D3D12_HEAP_TYPE_DEFAULT);

	virtual ~FD3D12Resource();

	operator ID3D12Resource&() { return *Resource; }
	ID3D12Resource* GetResource() const { return Resource.GetReference(); }

	inline void* Map()
	{
		check(Resource);
		VERIFYD3D12RESULT(Resource->Map(0, nullptr, &ResourceBaseAddress));

		return ResourceBaseAddress;
	}

	inline void Unmap()
	{
		check(Resource);
		check(ResourceBaseAddress);
		Resource->Unmap(0, nullptr);

		ResourceBaseAddress = nullptr;
	}

	D3D12_RESOURCE_DESC const& GetDesc() const { return Desc; }
	D3D12_HEAP_TYPE GetHeapType() const { return HeapType; }
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return GPUVirtualAddress; }
	void* GetResourceBaseAddress() const { check(ResourceBaseAddress); return ResourceBaseAddress; }
	uint16 GetMipLevels() const { return Desc.MipLevels; }
	uint16 GetArraySize() const { return (Desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) ? 1 : Desc.DepthOrArraySize; }
	uint8 GetPlaneCount() const { return PlaneCount; }
	uint16 GetSubresourceCount() const { return SubresourceCount; }
	CResourceState& GetResourceState()
	{
		check(bRequiresResourceStateTracking);
		// This state is used as the resource's "global" state between command lists. It's only needed for resources that
		// require state tracking.
		return ResourceState;
	}
	D3D12_RESOURCE_STATES GetDefaultResourceState() const { check(!bRequiresResourceStateTracking); return DefaultResourceState; }
	D3D12_RESOURCE_STATES GetWritableState() const { return WritableState; }
	D3D12_RESOURCE_STATES GetReadableState() const { return ReadableState; }
	bool RequiresResourceStateTracking() const { return bRequiresResourceStateTracking; }

	void SetName(const TCHAR* Name)
	{
		DebugName = FName(Name);
		::SetName(Resource, Name);
	}

	FName GetName() const
	{
		return DebugName;
	}
	
	void DoNotDeferDelete()
	{
		bDeferDelete = false;
	}

	inline bool ShouldDeferDelete() const { return bDeferDelete; }
	inline bool IsPlacedResource() const { return Heap.GetReference() != nullptr; }
	inline FD3D12Heap* GetHeap() const { return Heap; };
	inline bool IsDepthStencilResource() const { return bDepthStencil; }

	void StartTrackingForResidency();

	void UpdateResidency(FD3D12CommandListHandle& CommandList);

	inline FD3D12ResidencyHandle* GetResidencyHandle() 
	{
		return (IsPlacedResource()) ? Heap->GetResidencyHandle() : &ResidencyHandle; 
	}

	struct FD3D12ResourceTypeHelper
	{
		FD3D12ResourceTypeHelper(D3D12_RESOURCE_DESC& Desc, D3D12_HEAP_TYPE HeapType) :
			bSRV((Desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0),
			bDSV((Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0),
			bRTV((Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0),
			bUAV((Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0),
			bWritable(bDSV || bRTV || bUAV),
			bSRVOnly(bSRV && !bWritable),
			bBuffer(Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER),
			bReadBackResource(HeapType == D3D12_HEAP_TYPE_READBACK)
		{}

		const D3D12_RESOURCE_STATES GetOptimalInitialState(bool bAccurateWriteableStates) const
		{
			if (bSRVOnly)
			{
				return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			}
			else if (bBuffer && !bUAV)
			{
				return (bReadBackResource) ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_GENERIC_READ;
			}
			else if (bWritable)
			{
				if (bAccurateWriteableStates)
				{
					if (bDSV)
					{
						return D3D12_RESOURCE_STATE_DEPTH_WRITE;
					}
					else if (bRTV)
					{
						return D3D12_RESOURCE_STATE_RENDER_TARGET;
					}
					else if (bUAV)
					{
						return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
					}
				}
				else
				{
					// This things require tracking anyway
					return D3D12_RESOURCE_STATE_COMMON;
				}
			}

			return D3D12_RESOURCE_STATE_COMMON;
		}

		const uint32 bSRV : 1;
		const uint32 bDSV : 1;
		const uint32 bRTV : 1;
		const uint32 bUAV : 1;
		const uint32 bWritable : 1;
		const uint32 bSRVOnly : 1;
		const uint32 bBuffer : 1;
		const uint32 bReadBackResource : 1;
	};

	// NVCHANGE_BEGIN: Add VXGI
	void SetEnableUAVBarriers(bool Enable)
	{
		bEnableUAVBarriers = Enable;
		bFirstUAVBarrierPlaced = false;
	}

	bool RequestUAVBarrier()
	{
		if (bEnableUAVBarriers)
			return true;

		if (!bFirstUAVBarrierPlaced)
		{
			bFirstUAVBarrierPlaced = true;
			return true;
		}

		return false;
	}
	// NVCHANGE_END: Add VXGI

private:
	void InitalizeResourceState(D3D12_RESOURCE_STATES InitialState)
	{
		SubresourceCount = GetMipLevels() * GetArraySize() * GetPlaneCount();
		DetermineResourceStates();

		if (bRequiresResourceStateTracking)
		{
			// Only a few resources (~1%) actually need resource state tracking
			ResourceState.Initialize(SubresourceCount);
			ResourceState.SetResourceState(InitialState);
		}
	}

	void DetermineResourceStates()
	{
		const FD3D12ResourceTypeHelper Type(Desc, HeapType);

		bDepthStencil = Type.bDSV;

		if (Type.bWritable)
		{
			// Determine the resource's write/read states.
			if (Type.bRTV)
			{
				// Note: The resource could also be used as a UAV however we don't store that writable state. UAV's are handled in a separate RHITransitionResources() specially for UAVs so we know the writeable state in that case should be UAV.
				check(!Type.bDSV && !Type.bBuffer);
				WritableState = D3D12_RESOURCE_STATE_RENDER_TARGET;
				ReadableState = Type.bSRV ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_CORRUPT;
			}
			else if (Type.bDSV)
			{
				check(!Type.bRTV && !Type.bUAV && !Type.bBuffer);
				WritableState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
				ReadableState = Type.bSRV ? D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_DEPTH_READ;
			}
			else
			{
				check(Type.bUAV && !Type.bRTV && !Type.bDSV);
				WritableState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				ReadableState = Type.bSRV ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_CORRUPT;
			}
		}

		if (Type.bBuffer)
		{
			if (!Type.bWritable)
			{
				// Buffer used for input, like Vertex/Index buffer.
				// Don't bother tracking state for this resource.
#if UE_BUILD_DEBUG
				FPlatformAtomics::InterlockedIncrement(&NoStateTrackingResourceCount);
#endif
				DefaultResourceState = (HeapType == D3D12_HEAP_TYPE_READBACK) ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_GENERIC_READ;
				bRequiresResourceStateTracking = false;
				return;
			}
		}
		else
		{
			if (Type.bSRVOnly)
			{
				// Texture used only as a SRV.
				// Don't bother tracking state for this resource.
#if UE_BUILD_DEBUG
				FPlatformAtomics::InterlockedIncrement(&NoStateTrackingResourceCount);
#endif
				DefaultResourceState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				bRequiresResourceStateTracking = false;
				return;
			}
		}
	}
};

#ifdef USE_BUCKET_ALLOCATOR
typedef class FD3D12BucketAllocator FD3D12BaseAllocatorType;
#else
typedef class FD3D12BuddyAllocator FD3D12BaseAllocatorType;
#endif

struct FD3D12BuddyAllocatorPrivateData
{
	uint32 Offset;
	uint32 Order;

	void Init()
	{
		Offset = 0;
		Order = 0;
	}
};

struct FD3D12BlockAllocatorPrivateData
{
	uint64 FrameFence;
	uint32 BucketIndex;
	uint32 Offset;
	FD3D12Resource* ResourceHeap;

	void Init()
	{
		FrameFence = 0;
		BucketIndex = 0;
		Offset = 0;
		ResourceHeap = nullptr;
	}
};

class FD3D12ResourceAllocator;
// A very light-weight and cache friendly way of accessing a GPU resource
class FD3D12ResourceLocation : public FD3D12DeviceChild
{
public:

	enum class ResourceLocationType
	{
		eUndefined,
		eStandAlone,
		eSubAllocation,
		eFastAllocation,
		eAliased, // Oculus is the only API that uses this
		eHeapAliased, 
	};

	FD3D12ResourceLocation(FD3D12Device* Parent);
	~FD3D12ResourceLocation();

	void Clear();

	// Transfers the contents of 1 resource location to another, destroying the original but preserving the underlying resource 
	static void TransferOwnership(FD3D12ResourceLocation& Destination, FD3D12ResourceLocation& Source);

	// Setters
	void SetResource(FD3D12Resource* Value);
	inline void SetType(ResourceLocationType Value) { Type = Value;}

	inline void SetAllocator(FD3D12BaseAllocatorType* Value) { Allocator = Value; }
	inline void SetMappedBaseAddress(void* Value) { MappedBaseAddress = Value; }
	inline void SetGPUVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS Value) { GPUVirtualAddress = Value; }
	inline void SetOffsetFromBaseOfResource(uint64 Value) { OffsetFromBaseOfResource = Value; }
	inline void SetSize(uint64 Value) { Size = Value; }

	// Getters
	inline ResourceLocationType GetType() const { return Type; }
	inline FD3D12BaseAllocatorType* GetAllocator() { return Allocator; }
	inline FD3D12Resource* GetResource() const { return UnderlyingResource; }
	inline void* GetMappedBaseAddress() const { return MappedBaseAddress; }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return GPUVirtualAddress; }
	inline uint64 GetOffsetFromBaseOfResource() const { return OffsetFromBaseOfResource; }
	inline uint64 GetSize() const { return Size; }
	inline FD3D12ResidencyHandle* GetResidencyHandle() { return ResidencyHandle; }
	inline FD3D12BuddyAllocatorPrivateData& GetBuddyAllocatorPrivateData() { return AllocatorData.BuddyAllocatorPrivateData; }
	inline FD3D12BlockAllocatorPrivateData& GetBlockAllocatorPrivateData() { return AllocatorData.BlockAllocatorPrivateData; }

	const inline bool IsValid() const { return Type != ResourceLocationType::eUndefined; }

	inline void AsStandAlone(FD3D12Resource* Resource, uint32 BufferSize = 0, bool bInIsTransient = false )
	{
		SetType(FD3D12ResourceLocation::ResourceLocationType::eStandAlone);
		SetResource(Resource);
		SetSize(BufferSize);

		if (!IsCPUInaccessible(Resource->GetHeapType()))
		{
			SetMappedBaseAddress(Resource->Map());
		}
		SetGPUVirtualAddress(Resource->GetGPUVirtualAddress());
		SetTransient(bInIsTransient);
	}

	inline void AsHeapAliased(FD3D12Resource* Resource)
	{
		SetType(FD3D12ResourceLocation::ResourceLocationType::eHeapAliased);
		SetResource(Resource);
		SetSize(0);

		if (IsCPUWritable(Resource->GetHeapType()))
		{
			SetMappedBaseAddress(Resource->Map());
		}
		SetGPUVirtualAddress(Resource->GetGPUVirtualAddress());
	}


	inline void AsFastAllocation(FD3D12Resource* Resource, uint32 BufferSize, D3D12_GPU_VIRTUAL_ADDRESS GPUBase, void* CPUBase, uint64 Offset)
	{
		SetType(FD3D12ResourceLocation::ResourceLocationType::eFastAllocation);
		SetResource(Resource);
		SetSize(BufferSize);
		SetOffsetFromBaseOfResource(Offset);

		if (CPUBase != nullptr)
		{
			SetMappedBaseAddress((uint8*)CPUBase + Offset);
		}
		SetGPUVirtualAddress(GPUBase + Offset);
	}

	// Oculus API Aliases textures so this allows 2+ resource locations to reference the same underlying
	// resource. We should avoid this as much as possible as it requires expensive reference counting and
	// it complicates the resource ownership model.
	static void Alias(FD3D12ResourceLocation& Destination, FD3D12ResourceLocation& Source);

	void SetTransient(bool bInTransient)
	{
		bTransient = bInTransient;
	}
	bool IsTransient() const
	{
		return bTransient;
	}

private:

	template<bool bReleaseResource>
	void InternalClear();

	void ReleaseResource();

	ResourceLocationType Type;

	FD3D12Resource* UnderlyingResource;
	FD3D12ResidencyHandle* ResidencyHandle;

	// Which allocator this belongs to
	FD3D12BaseAllocatorType* Allocator;

	// Union to save memory
	union PrivateAllocatorData
	{
		FD3D12BuddyAllocatorPrivateData BuddyAllocatorPrivateData;
		FD3D12BlockAllocatorPrivateData BlockAllocatorPrivateData;
	} AllocatorData;

	// Note: These values refer to the start of this location including any padding *NOT* the start of the underlying resource
	void* MappedBaseAddress;
	D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddress;
	uint64 OffsetFromBaseOfResource;

	// The size the application asked for
	uint64 Size;

	bool bTransient;
};

class FD3D12DeferredDeletionQueue : public FD3D12AdapterChild
{
	typedef TPair<FD3D12Resource*, uint64> FencedObjectType;
	FThreadsafeQueue<FencedObjectType> DeferredReleaseQueue;

public:

	inline const uint32 QueueSize() const { return DeferredReleaseQueue.GetSize(); }

	void EnqueueResource(FD3D12Resource* pResource);

	bool ReleaseResources(bool DeleteImmediately = false);

	void Clear()
	{
		ReleaseResources(true);
	}

	FD3D12DeferredDeletionQueue(FD3D12Adapter* InParent);
	~FD3D12DeferredDeletionQueue();

	class FD3D12AsyncDeletionWorker : public FD3D12AdapterChild, public FNonAbandonableTask
	{
	public:
		FD3D12AsyncDeletionWorker(FD3D12Adapter* Adapter, FThreadsafeQueue<FencedObjectType>* DeletionQueue);

		void DoWork();

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FD3D12AsyncDeletionWorker, STATGROUP_ThreadPoolAsyncTasks);
		}

	private:
		TQueue<FencedObjectType> Queue;
	};

private:
	TQueue<FAsyncTask<FD3D12AsyncDeletionWorker>*> DeleteTasks;

};

struct FD3D12LockedResource : public FD3D12DeviceChild
{
	FD3D12LockedResource(FD3D12Device* Device) 
		: LockedOffset(0)
		, LockedPitch(0)
		, ResourceLocation(Device)
		, bLocked(false)
		, bLockedForReadOnly(false)
		, FD3D12DeviceChild(Device)
	{}

	inline void Reset()
	{
		ResourceLocation.Clear();
		bLocked = false;
		bLockedForReadOnly = false;
		LockedOffset = 0;
		LockedPitch = 0;
	}

	FD3D12ResourceLocation ResourceLocation;
	uint32 LockedOffset;
	uint32 LockedPitch;
	uint32 bLocked : 1;
	uint32 bLockedForReadOnly : 1;
};

/** The base class of resources that may be bound as shader resources. */
class FD3D12BaseShaderResource : public FD3D12DeviceChild, public IRefCountedObject
{
public:
	FD3D12Resource* GetResource() const { return ResourceLocation.GetResource(); }

	FD3D12ResourceLocation ResourceLocation;
	uint32 BufferAlignment;

public:
	FD3D12BaseShaderResource(FD3D12Device* InParent)
		: ResourceLocation(InParent)
		, BufferAlignment(0)
		, FD3D12DeviceChild(InParent)
	{
	}
};

/** Updates tracked stats for a buffer. */
#define D3D12_BUFFER_TYPE_CONSTANT   1
#define D3D12_BUFFER_TYPE_INDEX      2
#define D3D12_BUFFER_TYPE_VERTEX     3
#define D3D12_BUFFER_TYPE_STRUCTURED 4
extern void UpdateBufferStats(FD3D12ResourceLocation* ResourceLocation, bool bAllocating, uint32 BufferType);

/** Uniform buffer resource class. */
class FD3D12UniformBuffer : public FRHIUniformBuffer, public FD3D12DeviceChild, public FD3D12LinkedAdapterObject<FD3D12UniformBuffer>
{
public:
#if USE_STATIC_ROOT_SIGNATURE
	class FD3D12ConstantBufferView* View;
#endif

	/** The D3D12 constant buffer resource */
	FD3D12ResourceLocation ResourceLocation;

	/** Resource table containing RHI references. */
	TArray<TRefCountPtr<FRHIResource> > ResourceTable;

	/** Initialization constructor. */
	FD3D12UniformBuffer(class FD3D12Device* InParent, const FRHIUniformBufferLayout& InLayout)
		: ResourceLocation(InParent)
		, FRHIUniformBuffer(InLayout)
		, FD3D12DeviceChild(InParent)
#if USE_STATIC_ROOT_SIGNATURE
		, View(nullptr)
#endif
	{
	}

	virtual ~FD3D12UniformBuffer();

private:
	class FD3D12DynamicRHI* D3D12RHI;
};

#if PLATFORM_WINDOWS
class FD3D12TransientResource
{
	// Nothing special for fast ram
};
class FD3D12FastClearResource {};
#endif

/** Index buffer resource class that stores stride information. */
class FD3D12IndexBuffer : public FRHIIndexBuffer, public FD3D12BaseShaderResource, public FD3D12TransientResource, public FD3D12LinkedAdapterObject<FD3D12IndexBuffer>
{
public:

	FD3D12IndexBuffer(FD3D12Device* InParent, uint32 InStride, uint32 InSize, uint32 InUsage)
		: FRHIIndexBuffer(InStride, InSize, InUsage)
		, LockedData(InParent)
		, FD3D12BaseShaderResource(InParent)
	{}

	virtual ~FD3D12IndexBuffer();

	void Rename(FD3D12ResourceLocation& NewResource);

	// IRefCountedObject interface.
	virtual uint32 AddRef() const
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const
	{
		return FRHIResource::GetRefCount();
	}

	FD3D12LockedResource LockedData;
};

/** Structured buffer resource class. */
class FD3D12StructuredBuffer : public FRHIStructuredBuffer, public FD3D12BaseShaderResource, public FD3D12TransientResource, public FD3D12LinkedAdapterObject<FD3D12StructuredBuffer>
{
public:

	FD3D12StructuredBuffer(FD3D12Device* InParent, uint32 InStride, uint32 InSize, uint32 InUsage)
		: FRHIStructuredBuffer(InStride, InSize, InUsage)
		, LockedData(InParent)
		, FD3D12BaseShaderResource(InParent)
	{
	}

	void Rename(FD3D12ResourceLocation& NewResource);

	virtual ~FD3D12StructuredBuffer();

	// IRefCountedObject interface.
	virtual uint32 AddRef() const
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const
	{
		return FRHIResource::GetRefCount();
	}

	FD3D12LockedResource LockedData;
};

class FD3D12ShaderResourceView;

/** Vertex buffer resource class. */
class FD3D12VertexBuffer : public FRHIVertexBuffer, public FD3D12BaseShaderResource, public FD3D12TransientResource, public FD3D12LinkedAdapterObject<FD3D12VertexBuffer>
{
public:
	// Current SRV
	FD3D12ShaderResourceView* DynamicSRV;

	FD3D12VertexBuffer(FD3D12Device* InParent, uint32 InStride, uint32 InSize, uint32 InUsage)
		: FRHIVertexBuffer(InSize, InUsage)
		, LockedData(InParent)
		, FD3D12BaseShaderResource(InParent)
		, DynamicSRV(nullptr)
	{
		UNREFERENCED_PARAMETER(InStride);
	}

	virtual ~FD3D12VertexBuffer();

	void Rename(FD3D12ResourceLocation& NewResource);

	void SetDynamicSRV(FD3D12ShaderResourceView* InSRV)
	{
		DynamicSRV = InSRV;
	}

	FD3D12ShaderResourceView* GetDynamicSRV() const
	{
		return DynamicSRV;
	}

	// IRefCountedObject interface.
	virtual uint32 AddRef() const
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const
	{
		return FRHIResource::GetRefCount();
	}

	FD3D12LockedResource LockedData;
};

class FD3D12ResourceBarrierBatcher : public FNoncopyable
{
public:
	explicit FD3D12ResourceBarrierBatcher()
	{};

	// Add a UAV barrier to the batch. Ignoring the actual resource for now.
	void AddUAV()
	{
		Barriers.AddUninitialized();
		D3D12_RESOURCE_BARRIER& Barrier = Barriers.Last();
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		Barrier.UAV.pResource = nullptr;	// Ignore the resource ptr for now. HW doesn't do anything with it.
	}

	// Add a transition resource barrier to the batch.
	void AddTransition(ID3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32 Subresource)
	{
		check(Before != After);
		Barriers.AddUninitialized();
		D3D12_RESOURCE_BARRIER& Barrier = Barriers.Last();
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		Barrier.Transition.StateBefore = Before;
		Barrier.Transition.StateAfter = After;
		Barrier.Transition.Subresource = Subresource;
		Barrier.Transition.pResource = pResource;
	}

	void AddAliasingBarrier(ID3D12Resource* pResource)
	{
		Barriers.AddUninitialized();
		D3D12_RESOURCE_BARRIER& Barrier = Barriers.Last();
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
		Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		Barrier.Aliasing.pResourceBefore = NULL;
		Barrier.Aliasing.pResourceAfter = pResource;
	}

	// Flush the batch to the specified command list then reset.
	void Flush(ID3D12GraphicsCommandList* pCommandList)
	{
		if (Barriers.Num())
		{
			check(pCommandList);
			pCommandList->ResourceBarrier(Barriers.Num(), Barriers.GetData());
			Reset();
		}
	}

	// Clears the batch.
	void Reset()
	{
		Barriers.SetNumUnsafeInternal(0);	// Reset the array without shrinking (Does not destruct items, does not de-allocate memory).
		check(Barriers.Num() == 0);
	}

	const TArray<D3D12_RESOURCE_BARRIER>& GetBarriers() const
	{
		return Barriers;
	}

private:
	TArray<D3D12_RESOURCE_BARRIER> Barriers;
};

/**
* Class for managing dynamic buffers (Used for DrawUp).
*/
class FD3D12DynamicBuffer : public FD3D12DeviceChild
{
public:
	/** Initialization constructor. */
	FD3D12DynamicBuffer(FD3D12Device* InParent);
	/** Destructor. */
	~FD3D12DynamicBuffer();

	/** Locks the buffer returning at least Size bytes. */
	void* Lock(uint32 Size);
	/** Unlocks the buffer returning the underlying D3D12 buffer to use as a resource. */
	FD3D12ResourceLocation* Unlock();

	void ReleaseResourceLocation() { ResourceLocation.Clear(); }

private:
	FD3D12ResourceLocation ResourceLocation;
};

template<class T>
struct TD3D12ResourceTraits
{
};
template<>
struct TD3D12ResourceTraits<FRHIUniformBuffer>
{
	typedef FD3D12UniformBuffer TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIIndexBuffer>
{
	typedef FD3D12IndexBuffer TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIStructuredBuffer>
{
	typedef FD3D12StructuredBuffer TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIVertexBuffer>
{
	typedef FD3D12VertexBuffer TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHISamplerState>
{
	typedef FD3D12SamplerState TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIRasterizerState>
{
	typedef FD3D12RasterizerState TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIDepthStencilState>
{
	typedef FD3D12DepthStencilState TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIBlendState>
{
	typedef FD3D12BlendState TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIComputeFence>
{
	typedef FD3D12Fence TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIGraphicsPipelineState>
{
	typedef FD3D12GraphicsPipelineState TConcreteType;
};
