// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"
#include "D3D12CommandList.h"

void FD3D12CommandListHandle::AddTransitionBarrier(FD3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32 Subresource)
{
	check(CommandListData);
	CommandListData->ResourceBarrierBatcher.AddTransition(pResource->GetResource(), Before, After, Subresource);
	CommandListData->CurrentOwningContext->numBarriers++;

	pResource->UpdateResidency(*this);
}

void FD3D12CommandListHandle::AddUAVBarrier()
{
	check(CommandListData);
	CommandListData->ResourceBarrierBatcher.AddUAV();
	CommandListData->CurrentOwningContext->numBarriers++;
}

void FD3D12CommandListHandle::AddAliasingBarrier(FD3D12Resource* pResource)
{
	check(CommandListData);
	CommandListData->ResourceBarrierBatcher.AddAliasingBarrier(pResource->GetResource());
	CommandListData->CurrentOwningContext->numBarriers++;
}

void FD3D12CommandListHandle::Create(FD3D12Device* ParentDevice, D3D12_COMMAND_LIST_TYPE CommandListType, FD3D12CommandAllocator& CommandAllocator, FD3D12CommandListManager* InCommandListManager)
{
	check(!CommandListData);

	CommandListData = new FD3D12CommandListData(ParentDevice, CommandListType, CommandAllocator, InCommandListManager);

	CommandListData->AddRef();
}

FD3D12CommandListHandle::FD3D12CommandListData::FD3D12CommandListData(FD3D12Device* ParentDevice, D3D12_COMMAND_LIST_TYPE InCommandListType, FD3D12CommandAllocator& CommandAllocator, FD3D12CommandListManager* InCommandListManager)
	: CommandListManager(InCommandListManager)
	, CurrentGeneration(1)
	, LastCompleteGeneration(0)
	, IsClosed(false)
	, PendingResourceBarriers()
	, CurrentOwningContext(nullptr)
	, CurrentCommandAllocator(&CommandAllocator)
	, CommandListType(InCommandListType)
	, ResidencySet(nullptr)
	, FD3D12DeviceChild(ParentDevice)
	, FD3D12SingleNodeGPUObject(ParentDevice->GetNodeMask())
{
	VERIFYD3D12RESULT(ParentDevice->GetDevice()->CreateCommandList(GetNodeMask(), CommandListType, CommandAllocator, nullptr, IID_PPV_ARGS(CommandList.GetInitReference())));
	INC_DWORD_STAT(STAT_D3D12NumCommandLists);

	// Initially start with all lists closed.  We'll open them as we allocate them.
	Close();

	PendingResourceBarriers.Reserve(256);

	ResidencySet = D3DX12Residency::CreateResidencySet(ParentDevice->GetResidencyManager());
}

FD3D12CommandListHandle::FD3D12CommandListData::~FD3D12CommandListData()
{
	CommandList.SafeRelease();
	DEC_DWORD_STAT(STAT_D3D12NumCommandLists);

	D3DX12Residency::DestroyResidencySet(GetParentDevice()->GetResidencyManager(), ResidencySet);
}

void inline FD3D12CommandListHandle::FD3D12CommandListData::FCommandListResourceState::ConditionalInitalize(FD3D12Resource* pResource, CResourceState& ResourceState)
{
	// If there is no entry, all subresources should be in the resource's TBD state.
	// This means we need to have pending resource barrier(s).
	if (!ResourceState.CheckResourceStateInitalized())
	{
		ResourceState.Initialize(pResource->GetSubresourceCount());
		check(ResourceState.CheckResourceState(D3D12_RESOURCE_STATE_TBD));
	}

	check(ResourceState.CheckResourceStateInitalized());
}

CResourceState& FD3D12CommandListHandle::FD3D12CommandListData::FCommandListResourceState::GetResourceState(FD3D12Resource* pResource)
{
	// Only certain resources should use this
	check(pResource->RequiresResourceStateTracking());

	CResourceState& ResourceState = ResourceStates.FindOrAdd(pResource);
	ConditionalInitalize(pResource, ResourceState);
	return ResourceState;
}

void FD3D12CommandListHandle::FD3D12CommandListData::FCommandListResourceState::Empty()
{
	ResourceStates.Empty();
}

void FD3D12CommandListHandle::Execute(bool WaitForCompletion)
{
	check(CommandListData);
	CommandListData->CommandListManager->ExecuteCommandList(*this, WaitForCompletion);
}

FD3D12CommandAllocator::FD3D12CommandAllocator(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType)
	: PendingCommandListCount(0)
{
	Init(InDevice, InType);
}

FD3D12CommandAllocator::~FD3D12CommandAllocator()
{
	CommandAllocator.SafeRelease();
	DEC_DWORD_STAT(STAT_D3D12NumCommandAllocators);
}

void FD3D12CommandAllocator::Init(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType)
{
	check(CommandAllocator.GetReference() == nullptr);
	VERIFYD3D12RESULT(InDevice->CreateCommandAllocator(InType, IID_PPV_ARGS(CommandAllocator.GetInitReference())));
	INC_DWORD_STAT(STAT_D3D12NumCommandAllocators);
}
