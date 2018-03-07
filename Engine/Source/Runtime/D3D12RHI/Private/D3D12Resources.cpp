// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Resources.cpp: D3D RHI utility implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "EngineModule.h"
#include "D3D12LLM.h"

/////////////////////////////////////////////////////////////////////
//	FD3D12 Deferred Deletion Queue
/////////////////////////////////////////////////////////////////////

FD3D12DeferredDeletionQueue::FD3D12DeferredDeletionQueue(FD3D12Adapter* InParent) :
	FD3D12AdapterChild(InParent) {}

FD3D12DeferredDeletionQueue::~FD3D12DeferredDeletionQueue()
{
	FAsyncTask<FD3D12AsyncDeletionWorker>* DeleteTask = nullptr;
	while (DeleteTasks.Peek(DeleteTask))
	{
		DeleteTasks.Dequeue(DeleteTask);
		DeleteTask->EnsureCompletion(true);
		delete(DeleteTask);
	}
}

void FD3D12DeferredDeletionQueue::EnqueueResource(FD3D12Resource* pResource)
{
	check(pResource->ShouldDeferDelete());
	const uint64 CurrentFrameFence = GetParentAdapter()->GetFrameFence().GetCurrentFence();

	// Useful message for identifying when resources are released on the rendering thread.
	//UE_CLOG(IsInActualRenderingThread(), LogD3D12RHI, Display, TEXT("Rendering Thread: Deleting %#016llx when done with frame fence %llu"), pResource, CurrentFrameFence);

	const FencedObjectType FencedObject(pResource, CurrentFrameFence);
	DeferredReleaseQueue.Enqueue(FencedObject);
}

bool FD3D12DeferredDeletionQueue::ReleaseResources(bool DeleteImmediately)
{
	FD3D12Adapter* Adapter = GetParentAdapter();

#if ASYNC_DEFERRED_DELETION
	if (DeleteImmediately)
	{
		FAsyncTask<FD3D12AsyncDeletionWorker>* DeleteTask = nullptr;
		// Call back all threads
		while (DeleteTasks.Peek(DeleteTask))
		{
			DeleteTasks.Dequeue(DeleteTask);
			DeleteTask->EnsureCompletion(true);
			delete(DeleteTask);
		}
#endif

		struct FDequeueFenceObject
		{
			FDequeueFenceObject(FD3D12Fence& InFence)
				: FrameFence(InFence)
			{
			}

			bool operator() (FencedObjectType FenceObject) const
			{
				return FrameFence.IsFenceComplete(FenceObject.Value);
			}

		private:
			FD3D12Fence& FrameFence;
		};

		FencedObjectType FenceObject;
		const FDequeueFenceObject DequeueFenceObject(Adapter->GetFrameFence());

		while (DeferredReleaseQueue.Dequeue(FenceObject, DequeueFenceObject))
		{
			FenceObject.Key->Release();
		}

		return DeferredReleaseQueue.IsEmpty();

#if ASYNC_DEFERRED_DELETION
	}
	else
	{
		FAsyncTask<FD3D12AsyncDeletionWorker>* DeleteTask = nullptr;
		while (DeleteTasks.Peek(DeleteTask) && DeleteTask->IsDone())
		{
			DeleteTasks.Dequeue(DeleteTask);
			delete(DeleteTask);
		}

		DeleteTask = new FAsyncTask<FD3D12AsyncDeletionWorker>(Adapter, &DeferredReleaseQueue);

		DeleteTask->StartBackgroundTask();
		DeleteTasks.Enqueue(DeleteTask);

		return false;
	}
#endif
}

FD3D12DeferredDeletionQueue::FD3D12AsyncDeletionWorker::FD3D12AsyncDeletionWorker(FD3D12Adapter* Adapter, FThreadsafeQueue<FencedObjectType>* DeletionQueue) :
	FD3D12AdapterChild(Adapter)
{
	struct FDequeueFenceObject
	{
		FDequeueFenceObject(FD3D12Fence& InFence)
			: FrameFence(InFence)
		{
		}

		bool operator() (FencedObjectType FenceObject) const
		{
			return FrameFence.IsFenceComplete(FenceObject.Value);
		}

	private:
		FD3D12Fence& FrameFence;
	};

	const FDequeueFenceObject DequeueFenceObject(Adapter->GetFrameFence());
	DeletionQueue->BatchDequeue(&Queue, DequeueFenceObject, 4096);
}

void FD3D12DeferredDeletionQueue::FD3D12AsyncDeletionWorker::DoWork()
{
	FencedObjectType ResourceToDelete;

	while (Queue.Dequeue(ResourceToDelete))
	{
		// TEMP: Disable check until memory cleanup issues are resolved. This should be a final release.
		//check(ResourceToDelete.Key->GetRefCount() == 1);
		ResourceToDelete.Key->Release();
	}
}

/////////////////////////////////////////////////////////////////////
//	FD3D12 Resource
/////////////////////////////////////////////////////////////////////

#if UE_BUILD_DEBUG
int64 FD3D12Resource::TotalResourceCount = 0;
int64 FD3D12Resource::NoStateTrackingResourceCount = 0;
#endif

FD3D12Resource::FD3D12Resource(FD3D12Device* ParentDevice,
	GPUNodeMask VisibleNodes,
	ID3D12Resource* InResource,
	D3D12_RESOURCE_STATES InitialState,
	D3D12_RESOURCE_DESC const& InDesc,
	FD3D12Heap* InHeap,
	D3D12_HEAP_TYPE InHeapType)
	: Resource(InResource)
	, Heap(InHeap)
	, Desc(InDesc)
	, PlaneCount(::GetPlaneCount(Desc.Format))
	, SubresourceCount(0)
	, DefaultResourceState(D3D12_RESOURCE_STATE_TBD)
	, bRequiresResourceStateTracking(true)
	, bDepthStencil(false)
	, bDeferDelete(true)
	, HeapType(InHeapType)
	, GPUVirtualAddress(0)
	, ResourceBaseAddress(nullptr)
	, ResidencyHandle()
	, FD3D12DeviceChild(ParentDevice)
	, FD3D12MultiNodeGPUObject(ParentDevice->GetNodeMask(), VisibleNodes)
	// NVCHANGE_BEGIN: Add VXGI
	, bEnableUAVBarriers(true)
	, bFirstUAVBarrierPlaced(false)
	// NVCHANGE_END: Add VXGI
{
#if UE_BUILD_DEBUG
	FPlatformAtomics::InterlockedIncrement(&TotalResourceCount);
#endif

	if (Resource && Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		GPUVirtualAddress = Resource->GetGPUVirtualAddress();
	}

	InitalizeResourceState(InitialState);
}

FD3D12Resource::~FD3D12Resource()
{
	if (D3DX12Residency::IsInitialized(ResidencyHandle))
	{
		D3DX12Residency::EndTrackingObject(GetParentDevice()->GetResidencyManager(), ResidencyHandle);
	}
}

void FD3D12Resource::StartTrackingForResidency()
{
#if ENABLE_RESIDENCY_MANAGEMENT
	check(IsCPUInaccessible(HeapType));	// This is checked at a higher level before calling this function.
	check(D3DX12Residency::IsInitialized(ResidencyHandle) == false);
	const D3D12_RESOURCE_DESC ResourceDesc = Resource->GetDesc();
	const D3D12_RESOURCE_ALLOCATION_INFO Info = GetParentDevice()->GetDevice()->GetResourceAllocationInfo(0, 1, &ResourceDesc);

	D3DX12Residency::Initialize(ResidencyHandle, Resource.GetReference(), Info.SizeInBytes);
	D3DX12Residency::BeginTrackingObject(GetParentDevice()->GetResidencyManager(), ResidencyHandle);
#endif
}

void FD3D12Resource::UpdateResidency(FD3D12CommandListHandle& CommandList)
{
#if ENABLE_RESIDENCY_MANAGEMENT
	if (IsPlacedResource())
	{
		Heap->UpdateResidency(CommandList);
	}
	else if (D3DX12Residency::IsInitialized(ResidencyHandle))
	{
		check(Heap == nullptr);
		D3DX12Residency::Insert(CommandList.GetResidencySet(), ResidencyHandle);
	}
#endif
}

/////////////////////////////////////////////////////////////////////
//	FD3D12 Heap
/////////////////////////////////////////////////////////////////////

FD3D12Heap::FD3D12Heap(FD3D12Device* Parent, GPUNodeMask VisibleNodes) :
	ResidencyHandle(),
	FD3D12DeviceChild(Parent),
	FD3D12MultiNodeGPUObject(Parent->GetNodeMask(), VisibleNodes)
{
}

FD3D12Heap::~FD3D12Heap()
{
	Destroy();
}

void FD3D12Heap::UpdateResidency(FD3D12CommandListHandle& CommandList)
{
#if ENABLE_RESIDENCY_MANAGEMENT
	if (D3DX12Residency::IsInitialized(ResidencyHandle))
	{
		D3DX12Residency::Insert(CommandList.GetResidencySet(), ResidencyHandle);
	}
#endif
}

void FD3D12Heap::Destroy()
{
	//TODO: Check ref counts?
	if (D3DX12Residency::IsInitialized(ResidencyHandle))
	{
		D3DX12Residency::EndTrackingObject(GetParentDevice()->GetResidencyManager(), ResidencyHandle);
		ResidencyHandle = {};
	}
}

void FD3D12Heap::BeginTrackingResidency(uint64 Size)
{
#if ENABLE_RESIDENCY_MANAGEMENT
	D3DX12Residency::Initialize(ResidencyHandle, Heap.GetReference(), Size);
	D3DX12Residency::BeginTrackingObject(GetParentDevice()->GetResidencyManager(), ResidencyHandle);
#endif
}

/////////////////////////////////////////////////////////////////////
//	FD3D12 Adapter
/////////////////////////////////////////////////////////////////////

HRESULT FD3D12Adapter::CreateCommittedResource(const D3D12_RESOURCE_DESC& InDesc, const D3D12_HEAP_PROPERTIES& HeapProps, const D3D12_RESOURCE_STATES& InitialUsage, const D3D12_CLEAR_VALUE* ClearValue, FD3D12Resource** ppOutResource)
{
	if (!ppOutResource)
	{
		return E_POINTER;
	}

	LLM_PLATFORM_SCOPE(ELLMTag::GraphicsPlatform);

	TRefCountPtr<ID3D12Resource> pResource;
	const HRESULT hr = RootDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &InDesc, InitialUsage, ClearValue, IID_PPV_ARGS(pResource.GetInitReference()));
	check(SUCCEEDED(hr));

	if (SUCCEEDED(hr))
	{
		// Set the output pointer
		*ppOutResource = new FD3D12Resource(GetDevice(HeapProps.CreationNodeMask), HeapProps.VisibleNodeMask, pResource, InitialUsage, InDesc, nullptr, HeapProps.Type);
		(*ppOutResource)->AddRef();

		// Only track resources that cannot be accessed on the CPU.
		if (IsCPUInaccessible(HeapProps.Type))
		{
			(*ppOutResource)->StartTrackingForResidency();
		}
	}

	return hr;
}

HRESULT FD3D12Adapter::CreatePlacedResourceWithHeap(const D3D12_RESOURCE_DESC& InDesc, const D3D12_HEAP_PROPERTIES& HeapProps, const D3D12_RESOURCE_STATES& InitialUsage, const D3D12_CLEAR_VALUE* ClearValue, FD3D12Resource** ppOutResource)
{
	if (!ppOutResource)
	{
		return E_POINTER;
	}

	LLM_PLATFORM_SCOPE_D3D12(ELLMTagD3D12::CommittedResources);
	TRefCountPtr<ID3D12Resource> pResource;
	FD3D12Heap* Heap = nullptr;
	HRESULT hr;
	TRefCountPtr<ID3D12Heap> D3DHeap;
	D3D12_RESOURCE_ALLOCATION_INFO ResInfo = RootDevice->GetResourceAllocationInfo(HeapProps.VisibleNodeMask, 1, &InDesc);
	D3D12_HEAP_DESC HeapDesc = {};
	HeapDesc.Properties = HeapProps;
	HeapDesc.SizeInBytes = ResInfo.SizeInBytes;
	HeapDesc.Alignment = 0;
	HeapDesc.Flags = D3D12_HEAP_FLAG_NONE;
	
#if PLATFORM_XBOXONE
	if (InDesc.Flags & D3D12RHI_RESOURCE_FLAG_ALLOW_INDIRECT_BUFFER)
	{
		HeapDesc.Flags |= D3D12RHI_HEAP_FLAG_ALLOW_INDIRECT_BUFFERS;
	}
#endif
	hr = RootDevice->CreateHeap(&HeapDesc, IID_PPV_ARGS(D3DHeap.GetInitReference()));
	check(SUCCEEDED(hr));
	if (SUCCEEDED(hr))
	{
		hr = RootDevice->CreatePlacedResource(D3DHeap, 0, &InDesc, InitialUsage, ClearValue, IID_PPV_ARGS(pResource.GetInitReference()));
		Heap = new FD3D12Heap(GetDevice(), HeapProps.VisibleNodeMask);
		Heap->SetHeap(D3DHeap);
		check(SUCCEEDED(hr));
	}

	if (SUCCEEDED(hr))
	{
		// Set the output pointer
		*ppOutResource = new FD3D12Resource(GetDevice(HeapProps.CreationNodeMask), HeapProps.VisibleNodeMask, pResource, InitialUsage, InDesc, Heap, HeapProps.Type);
		(*ppOutResource)->AddRef();

		// Only track resources that cannot be accessed on the CPU.
		if (IsCPUInaccessible(HeapProps.Type))
		{
			(*ppOutResource)->StartTrackingForResidency();
		}
	}

	return hr;
}

HRESULT FD3D12Adapter::CreatePlacedResource(const D3D12_RESOURCE_DESC& InDesc, FD3D12Heap* BackingHeap, uint64 HeapOffset, const D3D12_RESOURCE_STATES& InitialUsage, const D3D12_CLEAR_VALUE* ClearValue, FD3D12Resource** ppOutResource)
{
	if (!ppOutResource)
	{
		return E_POINTER;
	}

	ID3D12Heap* Heap = BackingHeap->GetHeap();

	TRefCountPtr<ID3D12Resource> pResource;
	const HRESULT hr = RootDevice->CreatePlacedResource(Heap, HeapOffset, &InDesc, InitialUsage, nullptr, IID_PPV_ARGS(pResource.GetInitReference()));
	check(SUCCEEDED(hr));

	if (SUCCEEDED(hr))
	{
		FD3D12Device* Device = BackingHeap->GetParentDevice();
		const D3D12_HEAP_DESC HeapDesc = Heap->GetDesc();

		// Set the output pointer
		*ppOutResource = new FD3D12Resource(Device,
			Device->GetVisibilityMask(),
			pResource,
			InitialUsage,
			InDesc,
			BackingHeap,
			HeapDesc.Properties.Type);

		(*ppOutResource)->AddRef();
	}

	return hr;
}

HRESULT FD3D12Adapter::CreateBuffer(D3D12_HEAP_TYPE HeapType, GPUNodeMask CreationNode, GPUNodeMask VisibleNodes, uint64 HeapSize, FD3D12Resource** ppOutResource, D3D12_RESOURCE_FLAGS Flags)
{
	const D3D12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(HeapType, CreationNode, VisibleNodes);
	return CreateBuffer(HeapProps, HeapSize, ppOutResource, Flags);
}

HRESULT FD3D12Adapter::CreateBuffer(const D3D12_HEAP_PROPERTIES& HeapProps, uint64 HeapSize, FD3D12Resource** ppOutResource, D3D12_RESOURCE_FLAGS Flags)
{
	if (!ppOutResource)
	{
		return E_POINTER;
	}

	const D3D12_RESOURCE_DESC BufDesc = CD3DX12_RESOURCE_DESC::Buffer(HeapSize, Flags);
	const D3D12_RESOURCE_STATES InitialState = DetermineInitialResourceState(HeapProps.Type, &HeapProps);
	TRefCountPtr<ID3D12Resource> pResource;
	return CreateCommittedResource(BufDesc,
		HeapProps,
		InitialState,
		nullptr,
		ppOutResource);
}

/////////////////////////////////////////////////////////////////////
//	FD3D12 Resource Location
/////////////////////////////////////////////////////////////////////

FD3D12ResourceLocation::FD3D12ResourceLocation(FD3D12Device* Parent)
	: Type(ResourceLocationType::eUndefined)
	, UnderlyingResource(nullptr)
	, MappedBaseAddress(nullptr)
	, GPUVirtualAddress(0)
	, ResidencyHandle(nullptr)
	, Size(0)
	, OffsetFromBaseOfResource(0)
	, Allocator(nullptr)
	, FD3D12DeviceChild(Parent)
	, bTransient(false)
{
	FMemory::Memzero(AllocatorData);
}

FD3D12ResourceLocation::~FD3D12ResourceLocation()
{
	ReleaseResource();
}

void FD3D12ResourceLocation::Clear()
{
	InternalClear<true>();
}

template void FD3D12ResourceLocation::InternalClear<false>();
template void FD3D12ResourceLocation::InternalClear<true>();

template<bool bReleaseResource>
void FD3D12ResourceLocation::InternalClear()
{
	if (bReleaseResource)
	{
		ReleaseResource();
	}

	// Reset members
	Type = ResourceLocationType::eUndefined;
	UnderlyingResource = nullptr;
	MappedBaseAddress = nullptr;
	GPUVirtualAddress = 0;
	ResidencyHandle = nullptr;
	Size = 0;
	OffsetFromBaseOfResource = 0;
	FMemory::Memzero(AllocatorData);

	Allocator = nullptr;
}

void FD3D12ResourceLocation::TransferOwnership(FD3D12ResourceLocation& Destination, FD3D12ResourceLocation& Source)
{
	// Clear out the destination
	Destination.Clear();

	FMemory::Memmove(&Destination, &Source, sizeof(FD3D12ResourceLocation));

	// Destroy the source but don't invoke any resource destruction
	Source.InternalClear<false>();
}


void FD3D12ResourceLocation::Alias(FD3D12ResourceLocation & Destination, FD3D12ResourceLocation & Source)
{
	check(Source.GetResource() != nullptr);
	Destination.Clear();

	FMemory::Memmove(&Destination, &Source, sizeof(FD3D12ResourceLocation));
	Destination.SetType(ResourceLocationType::eAliased);
	Source.SetType(ResourceLocationType::eAliased);

	// Addref the source as another resource location references it
	Source.GetResource()->AddRef();
}

void FD3D12ResourceLocation::ReleaseResource()
{
	switch (Type)
	{
	case ResourceLocationType::eStandAlone:
	{
		check(UnderlyingResource->GetRefCount() == 1);
		
		if (UnderlyingResource->ShouldDeferDelete())
		{
			GetParentDevice()->GetParentAdapter()->GetDeferredDeletionQueue().EnqueueResource(UnderlyingResource);
		}
		else
		{
			UnderlyingResource->Release();
		}
		break;
	}
	case ResourceLocationType::eSubAllocation:
	{
		check(Allocator != nullptr);
		Allocator->Deallocate(*this);
		break;
	}
	case ResourceLocationType::eAliased:
	{
		if (UnderlyingResource->ShouldDeferDelete() && UnderlyingResource->GetRefCount() == 1)
		{
			GetParentDevice()->GetParentAdapter()->GetDeferredDeletionQueue().EnqueueResource(UnderlyingResource);
		}
		else
		{
			UnderlyingResource->Release();
		}
		break;
	}
	case ResourceLocationType::eHeapAliased:
	{
		check(UnderlyingResource->GetRefCount() == 1);
		if (UnderlyingResource->ShouldDeferDelete())
		{
			GetParentDevice()->GetParentAdapter()->GetDeferredDeletionQueue().EnqueueResource(UnderlyingResource);
		}
		else
		{
			UnderlyingResource->Release();
		}
		break;
	}
	case ResourceLocationType::eFastAllocation:
	case ResourceLocationType::eUndefined:
	default:
		// Fast allocations are volatile by default so no work needs to be done.
		break;
	}
}

void FD3D12ResourceLocation::SetResource(FD3D12Resource* Value)
{
	check(UnderlyingResource == nullptr);
	check(ResidencyHandle == nullptr);

	UnderlyingResource = Value;
	ResidencyHandle = UnderlyingResource->GetResidencyHandle();
}

/////////////////////////////////////////////////////////////////////
//	FD3D12 Dynamic Buffer
/////////////////////////////////////////////////////////////////////

FD3D12DynamicBuffer::FD3D12DynamicBuffer(FD3D12Device* InParent)
	: ResourceLocation(InParent)
	, FD3D12DeviceChild(InParent)
{
}

FD3D12DynamicBuffer::~FD3D12DynamicBuffer()
{
}

void* FD3D12DynamicBuffer::Lock(uint32 Size)
{
	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();

	return 	Adapter->GetUploadHeapAllocator().AllocUploadResource(Size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, ResourceLocation);
}

FD3D12ResourceLocation* FD3D12DynamicBuffer::Unlock()
{
	return &ResourceLocation;
}
