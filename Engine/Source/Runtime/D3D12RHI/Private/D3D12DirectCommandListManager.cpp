// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"
#include "Windows.h"


extern bool D3D12RHI_ShouldCreateWithD3DDebug();

FComputeFenceRHIRef FD3D12DynamicRHI::RHICreateComputeFence(const FName& Name)
{
	FD3D12Fence* Fence = new FD3D12Fence(&GetAdapter(), Name);
	Fence->CreateFence();

	return Fence;
}

FD3D12FenceCore::FD3D12FenceCore(FD3D12Adapter* Parent, uint64 InitialValue)
	: hFenceCompleteEvent(INVALID_HANDLE_VALUE)
	, FenceValueAvailableAt(0)
	, FD3D12AdapterChild(Parent)
{
	check(Parent);
	hFenceCompleteEvent = CreateEvent(nullptr, false, false, nullptr);
	check(INVALID_HANDLE_VALUE != hFenceCompleteEvent);

	VERIFYD3D12RESULT(Parent->GetD3DDevice()->CreateFence(InitialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(Fence.GetInitReference())));
}

FD3D12FenceCore::~FD3D12FenceCore()
{
	if (hFenceCompleteEvent != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hFenceCompleteEvent);
		hFenceCompleteEvent = INVALID_HANDLE_VALUE;
	}
}

FD3D12Fence::FD3D12Fence(FD3D12Adapter* Parent, const FName& Name)
	: FRHIComputeFence(Name)
	, CurrentFence(0)
	, LastSignaledFence(0)
	, LastCompletedFence(0)
	, FenceCore(nullptr)
	, FD3D12AdapterChild(Parent)
{
}

FD3D12Fence::~FD3D12Fence()
{
	Destroy();
}

void FD3D12Fence::Destroy()
{
	if (FenceCore)
	{
		// Return the underlying fence to the pool, store the last value signaled on this fence
		GetParentAdapter()->GetFenceCorePool().ReleaseFenceCore(FenceCore, LastSignaledFence);
		FenceCore = nullptr;
	}
}

void FD3D12Fence::CreateFence()
{
	check(FenceCore == nullptr);

	// Get a fence from the pool
	FenceCore = GetParentAdapter()->GetFenceCorePool().ObtainFenceCore();
	SetName(FenceCore->GetFence(), *GetName().ToString());

	LastSignaledFence = GetLastCompletedFence();
	CurrentFence = LastSignaledFence + 1;
}

uint64 FD3D12Fence::Signal(ID3D12CommandQueue* pCommandQueue)
{
	check(LastSignaledFence != CurrentFence);
	InternalSignal(pCommandQueue, CurrentFence);

	// Update the cached version of the fence value
	GetLastCompletedFence();

	// Increment the current Fence
	CurrentFence++;

	return LastSignaledFence;
}

void FD3D12Fence::GpuWait(ID3D12CommandQueue* pCommandQueue, uint64 FenceValue)
{
	check(pCommandQueue != nullptr);

#if DEBUG_FENCES
	UE_LOG(LogD3D12RHI, Log, TEXT("*** GPU WAIT (CmdQueue: %016llX) Fence: %016llX (%s), Value: %u ***"), pCommandQueue, FenceCore->GetFence(), *GetName().ToString(), FenceValue);
#endif

	VERIFYD3D12RESULT(pCommandQueue->Wait(FenceCore->GetFence(), FenceValue));
}

bool FD3D12Fence::IsFenceComplete(uint64 FenceValue)
{
	check(FenceCore);

	// Avoid repeatedly calling GetCompletedValue()
	if (FenceValue <= LastCompletedFence)
	{
		checkf(FenceValue <= FenceCore->GetFence()->GetCompletedValue(), TEXT("Fence value (%llu) sanity check failed! Last completed value is really %llu."), FenceValue, FenceCore->GetFence()->GetCompletedValue());
		return true;
	}

	// Refresh the completed fence value
	return FenceValue <= GetLastCompletedFence();
}

uint64 FD3D12Fence::GetLastCompletedFence()
{
	LastCompletedFence = FenceCore->GetFence()->GetCompletedValue();
	return LastCompletedFence;
}

uint64 FD3D12ManualFence::Signal(ID3D12CommandQueue* pCommandQueue, uint64 FenceToSignal)
{
	check(LastSignaledFence != FenceToSignal);
	InternalSignal(pCommandQueue, FenceToSignal);

	// Update the cached version of the fence value
	GetLastCompletedFence();

	check(LastSignaledFence == FenceToSignal);
	return LastSignaledFence;
}

FD3D12CommandAllocatorManager::FD3D12CommandAllocatorManager(FD3D12Device* InParent, const D3D12_COMMAND_LIST_TYPE& InType)
	: Type(InType)
	, FD3D12DeviceChild(InParent)
{}


FD3D12CommandAllocator* FD3D12CommandAllocatorManager::ObtainCommandAllocator()
{
	FScopeLock Lock(&CS);

	// See if the first command allocator in the queue is ready to be reset (will check associated fence)
	FD3D12CommandAllocator* pCommandAllocator = nullptr;
	if (CommandAllocatorQueue.Peek(pCommandAllocator) && pCommandAllocator->IsReady())
	{
		// Reset the allocator and remove it from the queue.
		pCommandAllocator->Reset();
		CommandAllocatorQueue.Dequeue(pCommandAllocator);
	}
	else
	{
		// The queue was empty, or no command allocators were ready, so create a new command allocator.
		pCommandAllocator = new FD3D12CommandAllocator(GetParentDevice()->GetDevice(), Type);
		check(pCommandAllocator);
		CommandAllocators.Add(pCommandAllocator);	// The command allocator's lifetime is managed by this manager

		// Set a valid sync point
		FD3D12Fence& FrameFence = GetParentDevice()->GetParentAdapter()->GetFrameFence();
		const FD3D12SyncPoint SyncPoint(&FrameFence, FrameFence.GetLastCompletedFence());
		pCommandAllocator->SetSyncPoint(SyncPoint);
	}

	check(pCommandAllocator->IsReady());
	return pCommandAllocator;
}

void FD3D12CommandAllocatorManager::ReleaseCommandAllocator(FD3D12CommandAllocator* pCommandAllocator)
{
	FScopeLock Lock(&CS);
	check(pCommandAllocator->HasValidSyncPoint());
	CommandAllocatorQueue.Enqueue(pCommandAllocator);
}

FD3D12CommandListManager::FD3D12CommandListManager(FD3D12Device* InParent, D3D12_COMMAND_LIST_TYPE CommandListType)
	: CommandListType(CommandListType)
	, ResourceBarrierCommandAllocator(nullptr)
	, ResourceBarrierCommandAllocatorManager(InParent, D3D12_COMMAND_LIST_TYPE_DIRECT)
	, CommandListFence(nullptr, L"Command List Fence")
	, FD3D12DeviceChild(InParent)
	, FD3D12SingleNodeGPUObject(InParent->GetNodeMask())
{
}

FD3D12CommandListManager::~FD3D12CommandListManager()
{
	Destroy();
}

void FD3D12CommandListManager::Destroy()
{
	// Wait for the queue to empty
	WaitForCommandQueueFlush();

	D3DCommandQueue.SafeRelease();

	FD3D12CommandListHandle hList;
	while (!ReadyLists.IsEmpty())
	{
		ReadyLists.Dequeue(hList);
	}

	CommandListFence.Destroy();
}

void FD3D12CommandListManager::Create(const TCHAR* Name, uint32 NumCommandLists, uint32 Priority)
{
	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();

	CommandListFence.SetParentAdapter(Adapter);
	CommandListFence.CreateFence();

	check(D3DCommandQueue.GetReference() == nullptr);
	check(ReadyLists.IsEmpty());
	checkf(NumCommandLists <= 0xffff, TEXT("Exceeded maximum supported command lists"));

	D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = {};
	CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	CommandQueueDesc.NodeMask = GetNodeMask();
	CommandQueueDesc.Priority = Priority;
	CommandQueueDesc.Type = CommandListType;
	D3DCommandQueue = Adapter->GetOwningRHI()->CreateCommandQueue(Device, CommandQueueDesc);
	SetName(D3DCommandQueue.GetReference(), Name);

	if (NumCommandLists > 0)
	{
		// Create a temp command allocator for command list creation.
		FD3D12CommandAllocator TempCommandAllocator(Device->GetDevice(), CommandListType);
		for (uint32 i = 0; i < NumCommandLists; ++i)
		{
			FD3D12CommandListHandle hList = CreateCommandListHandle(TempCommandAllocator);
			ReadyLists.Enqueue(hList);
		}
	}
}

FD3D12CommandListHandle FD3D12CommandListManager::ObtainCommandList(FD3D12CommandAllocator& CommandAllocator)
{
	FD3D12CommandListHandle List;
	if (!ReadyLists.Dequeue(List))
	{
		// Create a command list if there are none available.
		List = CreateCommandListHandle(CommandAllocator);
	}

	check(List.GetCommandListType() == CommandListType);
	List.Reset(CommandAllocator);
	return List;
}

void FD3D12CommandListManager::ReleaseCommandList(FD3D12CommandListHandle& hList)
{
	check(hList.IsClosed());
	check(hList.GetCommandListType() == CommandListType);

	// Indicate that a command list using this allocator has either been executed or discarded.
	hList.CurrentCommandAllocator()->DecrementPendingCommandLists();

	ReadyLists.Enqueue(hList);
}

void FD3D12CommandListManager::ExecuteCommandList(FD3D12CommandListHandle& hList, bool WaitForCompletion)
{
	TArray<FD3D12CommandListHandle> Lists;
	Lists.Add(hList);

	ExecuteCommandLists(Lists, WaitForCompletion);
}

uint64 FD3D12CommandListManager::ExecuteAndIncrementFence(FD3D12CommandListPayload& Payload, FD3D12Fence &Fence)
{
	FScopeLock Lock(&FenceCS);

	// Execute, signal, and wait (if requested)
#if UE_BUILD_DEBUG
	if (D3D12RHI_ShouldCreateWithD3DDebug())
	{
		// Debug layer will break when a command list does bad stuff. Helps identify the command list in question.
		for (uint32 i = 0; i < Payload.NumCommandLists; i++)
		{
#if ENABLE_RESIDENCY_MANAGEMENT
			VERIFYD3D12RESULT(GetParentDevice()->GetResidencyManager().ExecuteCommandLists(D3DCommandQueue, &Payload.CommandLists[i], &Payload.ResidencySets[i], 1));
#else
			D3DCommandQueue->ExecuteCommandLists(1, &Payload.CommandLists[i]);
#endif

#if LOG_EXECUTE_COMMAND_LISTS
			LogExecuteCommandLists(1, &(Payload.CommandLists[i]));
#endif
		}
	}
	else
#endif
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		VERIFYD3D12RESULT(GetParentDevice()->GetResidencyManager().ExecuteCommandLists(D3DCommandQueue, Payload.CommandLists, Payload.ResidencySets, Payload.NumCommandLists));
#else
		D3DCommandQueue->ExecuteCommandLists(Payload.NumCommandLists, Payload.CommandLists);
#endif

#if LOG_EXECUTE_COMMAND_LISTS
		LogExecuteCommandLists(Payload.NumCommandLists, Payload.CommandLists);
#endif
	}

	return Fence.Signal(D3DCommandQueue);
}

void FD3D12CommandListManager::ExecuteCommandLists(TArray<FD3D12CommandListHandle>& Lists, bool WaitForCompletion)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12ExecuteCommandListTime);

	bool NeedsResourceBarriers = false;
	for (int32 i = 0; i < Lists.Num(); i++)
	{
		FD3D12CommandListHandle& commandList = Lists[i];
		if (commandList.PendingResourceBarriers().Num() > 0)
		{
			NeedsResourceBarriers = true;
			break;
		}
	}

	uint64 SignaledFenceValue = -1;
	uint64 BarrierFenceValue = -1;
	FD3D12SyncPoint SyncPoint;
	FD3D12SyncPoint BarrierSyncPoint;

	FD3D12CommandListManager& DirectCommandListManager = GetParentDevice()->GetCommandListManager();
	FD3D12Fence& DirectFence = DirectCommandListManager.GetFence();

	int32 commandListIndex = 0;
	int32 barrierCommandListIndex = 0;

	// Close the resource barrier lists, get the raw command list pointers, and enqueue the command list handles
	// Note: All command lists will share the same fence
	FD3D12CommandListPayload CurrentCommandListPayload;
	FD3D12CommandListPayload ComputeBarrierPayload;

	check(Lists.Num() <= FD3D12CommandListPayload::MaxCommandListsPerPayload);
	FD3D12CommandListHandle BarrierCommandList[128];
	if (NeedsResourceBarriers)
	{
#if !USE_D3D12RHI_RESOURCE_STATE_TRACKING
		// If we're using the engine's resource state tracking and barriers, then we should never have pending resource barriers.
		check(false);
#endif // !USE_D3D12RHI_RESOURCE_STATE_TRACKING

#if UE_BUILD_DEBUG	
		if (ResourceStateCS.TryLock())
		{
			ResourceStateCS.Unlock();
		}
		else
		{
			FD3D12DynamicRHI::GetD3DRHI()->SubmissionLockStalls++;
			// We don't think this will get hit but it's possible. If we do see this happen,
			// we should evaluate how often and why this is happening
			check(0);
		}
#endif
		FScopeLock Lock(&ResourceStateCS);

		for (int32 i = 0; i < Lists.Num(); i++)
		{
			FD3D12CommandListHandle& commandList = Lists[i];

			FD3D12CommandListHandle barrierCommandList ={};
			// Async compute cannot perform all resource transitions, and so it uses the direct context
			const uint32 numBarriers = DirectCommandListManager.GetResourceBarrierCommandList(commandList, barrierCommandList);
			if (numBarriers)
			{
				// TODO: Unnecessary assignment here, but fixing this will require refactoring GetResourceBarrierCommandList
				BarrierCommandList[barrierCommandListIndex] = barrierCommandList;
				barrierCommandListIndex++;

				barrierCommandList.Close();

				if (CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
				{
					ComputeBarrierPayload.Reset();
					ComputeBarrierPayload.Append(barrierCommandList.CommandList(), &barrierCommandList.GetResidencySet());
					BarrierFenceValue = DirectCommandListManager.ExecuteAndIncrementFence(ComputeBarrierPayload, DirectFence);
					DirectFence.GpuWait(D3DCommandQueue, BarrierFenceValue);
				}
				else
				{
					CurrentCommandListPayload.Append(barrierCommandList.CommandList(), &barrierCommandList.GetResidencySet());
				}
			}

			CurrentCommandListPayload.Append(commandList.CommandList(), &commandList.GetResidencySet());
			commandList.LogResourceBarriers();
		}
		SignaledFenceValue = ExecuteAndIncrementFence(CurrentCommandListPayload, CommandListFence);
		SyncPoint = FD3D12SyncPoint(&CommandListFence, SignaledFenceValue);
		if (CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
		{
			BarrierSyncPoint = FD3D12SyncPoint(&DirectFence, BarrierFenceValue);
		}
		else
		{
			BarrierSyncPoint = SyncPoint;
		}
	}
	else
	{
		for (int32 i = 0; i < Lists.Num(); i++)
		{
			CurrentCommandListPayload.Append(Lists[i].CommandList(), &Lists[i].GetResidencySet());
			Lists[i].LogResourceBarriers();
		}
		SignaledFenceValue = ExecuteAndIncrementFence(CurrentCommandListPayload, CommandListFence);
		check(CommandListType != D3D12_COMMAND_LIST_TYPE_COMPUTE);
		SyncPoint = FD3D12SyncPoint(&CommandListFence, SignaledFenceValue);
		BarrierSyncPoint = SyncPoint;
	}

	for (int32 i = 0; i < Lists.Num(); i++)
	{
		FD3D12CommandListHandle& commandList = Lists[i];

		// Set a sync point on the command list so we know when it's current generation is complete on the GPU, then release it so it can be reused later.
		// Note this also updates the command list's command allocator
		commandList.SetSyncPoint(SyncPoint);
		ReleaseCommandList(commandList);
	}

	for (int32 i = 0; i < barrierCommandListIndex; i++)
	{
		FD3D12CommandListHandle& commandList = BarrierCommandList[i];

		// Set a sync point on the command list so we know when it's current generation is complete on the GPU, then release it so it can be reused later.
		// Note this also updates the command list's command allocator
		commandList.SetSyncPoint(BarrierSyncPoint);
		DirectCommandListManager.ReleaseCommandList(commandList);
	}

	if (WaitForCompletion)
	{
		CommandListFence.WaitForFence(SignaledFenceValue);
		check(SyncPoint.IsComplete());
	}
}

void FD3D12CommandListManager::ReleaseResourceBarrierCommandListAllocator()
{
	// Release the resource barrier command allocator.
	if (ResourceBarrierCommandAllocator != nullptr)
	{
		ResourceBarrierCommandAllocatorManager.ReleaseCommandAllocator(ResourceBarrierCommandAllocator);
		ResourceBarrierCommandAllocator = nullptr;
	}
}

uint32 FD3D12CommandListManager::GetResourceBarrierCommandList(FD3D12CommandListHandle& hList, FD3D12CommandListHandle& hResourceBarrierList)
{
	TArray<FD3D12PendingResourceBarrier>& PendingResourceBarriers = hList.PendingResourceBarriers();
	const uint32 NumPendingResourceBarriers = PendingResourceBarriers.Num();
	if (NumPendingResourceBarriers)
	{
		// Reserve space for the descs
		TArray<D3D12_RESOURCE_BARRIER> BarrierDescs;
		BarrierDescs.Reserve(NumPendingResourceBarriers);

		// Fill out the descs
		D3D12_RESOURCE_BARRIER Desc = {};
		Desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

		for (uint32 i = 0; i < NumPendingResourceBarriers; ++i)
		{
			const FD3D12PendingResourceBarrier& PRB = PendingResourceBarriers[i];

			// Should only be doing this for the few resources that need state tracking
			check(PRB.Resource->RequiresResourceStateTracking());

			CResourceState& ResourceState = PRB.Resource->GetResourceState();

			Desc.Transition.Subresource = PRB.SubResource;
			const D3D12_RESOURCE_STATES Before = ResourceState.GetSubresourceState(Desc.Transition.Subresource);
			const D3D12_RESOURCE_STATES After = PRB.State;

			check(Before != D3D12_RESOURCE_STATE_TBD && Before != D3D12_RESOURCE_STATE_CORRUPT);
			if (Before != After)
			{
				Desc.Transition.pResource = PRB.Resource->GetResource();
				Desc.Transition.StateBefore = Before;
				Desc.Transition.StateAfter = After;

				// Add the desc
				BarrierDescs.Add(Desc);
			}

			// Update the state to the what it will be after hList executes
			const D3D12_RESOURCE_STATES CommandListState = hList.GetResourceState(PRB.Resource).GetSubresourceState(Desc.Transition.Subresource);
			const D3D12_RESOURCE_STATES LastState = (CommandListState != D3D12_RESOURCE_STATE_TBD) ? CommandListState : After;
			
			if (Before != LastState)
			{
				ResourceState.SetSubresourceState(Desc.Transition.Subresource, LastState);
			}
		}

		if (BarrierDescs.Num() > 0)
		{
			// Get a new resource barrier command allocator if we don't already have one.
			if (ResourceBarrierCommandAllocator == nullptr)
			{
				ResourceBarrierCommandAllocator = ResourceBarrierCommandAllocatorManager.ObtainCommandAllocator();
			}

			hResourceBarrierList = ObtainCommandList(*ResourceBarrierCommandAllocator);

#if ENABLE_RESIDENCY_MANAGEMENT
			//TODO: Update the logic so that this loop can occur above!
			for (uint32 i = 0; i < NumPendingResourceBarriers; ++i)
			{
				const FD3D12PendingResourceBarrier& PRB = PendingResourceBarriers[i];
				hResourceBarrierList.UpdateResidency(PRB.Resource);
			}
#endif
#if DEBUG_RESOURCE_STATES
			LogResourceBarriers(BarrierDescs.Num(), BarrierDescs.GetData(), hResourceBarrierList.CommandList());
#endif

			hResourceBarrierList->ResourceBarrier(BarrierDescs.Num(), BarrierDescs.GetData());
		}

		return BarrierDescs.Num();
	}

	return 0;
}

bool FD3D12CommandListManager::IsComplete(const FD3D12CLSyncPoint& hSyncPoint, uint64 FenceOffset)
{
	if (!hSyncPoint)
	{
		return false;
	}

	checkf(FenceOffset == 0, TEXT("This currently doesn't support offsetting fence values."));
	return hSyncPoint.IsComplete();
}

CommandListState FD3D12CommandListManager::GetCommandListState(const FD3D12CLSyncPoint& hSyncPoint)
{
	check(hSyncPoint);
	if (hSyncPoint.IsComplete())
	{
		return CommandListState::kFinished;
	}
	else if (hSyncPoint.Generation == hSyncPoint.CommandList.CurrentGeneration())
	{
		return CommandListState::kOpen;
	}
	else
	{
		return CommandListState::kQueued;
	}
}

void FD3D12CommandListManager::WaitForCommandQueueFlush()
{
	if (D3DCommandQueue)
	{
		const uint64 SignaledFence = CommandListFence.Signal(D3DCommandQueue);
		CommandListFence.WaitForFence(SignaledFence);
	}
}

FD3D12CommandListHandle FD3D12CommandListManager::CreateCommandListHandle(FD3D12CommandAllocator& CommandAllocator)
{
	FD3D12CommandListHandle List;
	List.Create(GetParentDevice(), CommandListType, CommandAllocator, this);
	return List;
}

FD3D12FenceCore* FD3D12FenceCorePool::ObtainFenceCore()
{
	{
		FScopeLock Lock(&CS);
		FD3D12FenceCore* Fence = nullptr;
		if (AvailableFences.Peek(Fence) && Fence->IsAvailable())
		{
			AvailableFences.Dequeue(Fence);
			return Fence;
		}
	}

	return new FD3D12FenceCore(GetParentAdapter(), 0);
}

void FD3D12FenceCorePool::ReleaseFenceCore(FD3D12FenceCore* Fence, uint64 CurrentFenceValue)
{
	FScopeLock Lock(&CS);
	Fence->FenceValueAvailableAt = CurrentFenceValue;
	AvailableFences.Enqueue(Fence);
}

void FD3D12FenceCorePool::Destroy()
{
	FD3D12FenceCore* Fence = nullptr;
	while (AvailableFences.Dequeue(Fence))
	{
		delete(Fence);
	}
}

void FD3D12CommandListPayload::Reset()
{
	NumCommandLists = 0;
	FMemory::Memzero(CommandLists);
	FMemory::Memzero(ResidencySets);
}

void FD3D12CommandListPayload::Append(ID3D12CommandList* CommandList, FD3D12ResidencySet* Set)
{
	check(NumCommandLists < FD3D12CommandListPayload::MaxCommandListsPerPayload);

	CommandLists[NumCommandLists] = CommandList;
	ResidencySets[NumCommandLists] = Set;
	NumCommandLists++;
}
