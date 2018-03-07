// NvFlow begin

/*=============================================================================
D3D12NvFlow.cpp: D3D device RHI NvFlow interop implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"

#include "GameWorks/RHINvFlowD3D12.h"

void FD3D12CommandContext::NvFlowGetDeviceDesc(FRHINvFlowDeviceDesc* desc)
{
	FRHINvFlowDeviceDescD3D12* descD3D12 = static_cast<FRHINvFlowDeviceDescD3D12*>(desc);

	descD3D12->device = GetParentDevice()->GetDevice();
	descD3D12->commandQueue = GetCommandListManager().GetD3DCommandQueue();
	descD3D12->commandQueueFence = GetCommandListManager().GetFence().GetFenceCore()->GetFence();
	descD3D12->commandList = CommandListHandle.GraphicsCommandList();
	descD3D12->lastFenceCompleted = GetCommandListManager().GetFence().GetLastCompletedFence();
	descD3D12->nextFenceValue = GetCommandListManager().GetFence().GetCurrentFence();
}

class FRHINvFlowStateCacheAccessD3D12
{
	FD3D12StateCacheBase* StateCache = nullptr;
public:
	FRHINvFlowStateCacheAccessD3D12(FD3D12CommandContext* cmdctx)
	{
		StateCache = &cmdctx->StateCache;
	}
	FD3D12DescriptorCache& DescriptorCache()
	{
		return StateCache->DescriptorCache;
	}
};

void FD3D12CommandContext::NvFlowReserveDescriptors(FRHINvFlowDescriptorReserveHandle* dstHandle, uint32 numDescriptors, uint64 lastFenceCompleted, uint64 nextFenceValue)
{
	FRHINvFlowStateCacheAccessD3D12 StateCacheAccess(this);
	auto& DescriptorCache = StateCacheAccess.DescriptorCache();
	uint32 ViewHeapSlot = 0u;
	for (uint32 iTries = 0; iTries < 2; ++iTries)
	{
		const uint32 NumViews = numDescriptors;
		if (!DescriptorCache.GetCurrentViewHeap()->CanReserveSlots(NumViews))
		{
			DescriptorCache.GetCurrentViewHeap()->RollOver();
			continue;
		}
		ViewHeapSlot = DescriptorCache.GetCurrentViewHeap()->ReserveSlots(NumViews);
		break;
	}
	if (dstHandle)
	{
		auto handle = static_cast<FRHINvFlowDescriptorReserveHandleD3D12*>(dstHandle);
		handle->heap = DescriptorCache.GetCurrentViewHeap()->GetHeap();
		handle->descriptorSize = DescriptorCache.GetCurrentViewHeap()->GetDescriptorSize();
		handle->cpuHandle = DescriptorCache.GetCurrentViewHeap()->GetCPUSlotHandle(ViewHeapSlot);
		handle->gpuHandle = DescriptorCache.GetCurrentViewHeap()->GetGPUSlotHandle(ViewHeapSlot);
	}
}

void FD3D12CommandContext::NvFlowGetDepthStencilViewDesc(FTexture2DRHIParamRef depthSurface, FTexture2DRHIParamRef depthTexture, FRHINvFlowDepthStencilViewDesc* desc)
{
	check(depthSurface != nullptr);
	check(depthTexture != nullptr);
	FRHINvFlowDepthStencilViewDescD3D12* descD3D12 = static_cast<FRHINvFlowDepthStencilViewDescD3D12*>(desc);

	auto DSV = RetrieveTextureBase(depthSurface)->GetDepthStencilView(CurrentDSVAccessType);
	auto SRV = RetrieveTextureBase(depthTexture)->GetShaderResourceView();

	descD3D12->dsvHandle = DSV->GetView();
	descD3D12->dsvDesc   = DSV->GetDesc();
	descD3D12->dsvResource = DSV->GetResource()->GetResource();
	descD3D12->dsvCurrentState = DSV->GetResource()->GetResourceState().GetSubresourceState(0);

	descD3D12->srvHandle = SRV->GetView();
	descD3D12->srvDesc   = SRV->GetDesc();
	descD3D12->srvResource = SRV->GetResource()->GetResource();
	descD3D12->srvCurrentState = SRV->GetResource()->GetResourceState().GetSubresourceState(0);

	StateCache.GetViewport(&descD3D12->viewport);
}

void FD3D12CommandContext::NvFlowGetRenderTargetViewDesc(FRHINvFlowRenderTargetViewDesc* desc)
{
	FRHINvFlowRenderTargetViewDescD3D12* descD3D12 = static_cast<FRHINvFlowRenderTargetViewDescD3D12*>(desc);

	descD3D12->rtvHandle = CurrentRenderTargets[0]->GetView();
	descD3D12->rtvDesc   = CurrentRenderTargets[0]->GetDesc();
	descD3D12->resource  = CurrentRenderTargets[0]->GetResource()->GetResource();
	descD3D12->currentState = CurrentRenderTargets[0]->GetResource()->GetResourceState().GetSubresourceState(0);

	StateCache.GetViewport(&descD3D12->viewport);
	StateCache.GetScissorRect(&descD3D12->scissor);
}


class FD3D12NvFlowResourceRW : public FRHINvFlowResourceRW
{
public:
	FD3D12Resource Resource;
	FD3D12ResourceLocation ResourceLocation;
	D3D12_RESOURCE_STATES* ResourceState;

	FD3D12NvFlowResourceRW(
		FD3D12Device* InParent,
		GPUNodeMask VisibleNodes,
		ID3D12Resource* InResource,
		D3D12_RESOURCE_DESC const& InDesc,
		D3D12_RESOURCE_STATES* InResourceState
	)
		: Resource(InParent, VisibleNodes, InResource, *InResourceState, InDesc)
		, ResourceLocation(InParent)
		, ResourceState(InResourceState)
	{
		ResourceLocation.SetResource(&Resource);
	}
};

class FD3D12ShaderResourceViewNvFlow : public FD3D12ShaderResourceView
{
	TRefCountPtr<FD3D12NvFlowResourceRW> NvFlowResourceRWRef;

public:
	FD3D12ShaderResourceViewNvFlow(FD3D12Device* InParent, D3D12_SHADER_RESOURCE_VIEW_DESC* InSRVDesc, FD3D12NvFlowResourceRW* InNvFlowResourceRW)
		: FD3D12ShaderResourceView(InParent, InSRVDesc, &InNvFlowResourceRW->ResourceLocation)
		, NvFlowResourceRWRef(InNvFlowResourceRW)
	{
	}
};

FShaderResourceViewRHIRef FD3D12CommandContext::NvFlowCreateSRV(const FRHINvFlowResourceViewDesc* desc)
{
	const FRHINvFlowResourceViewDescD3D12* descD3D12 = static_cast<const FRHINvFlowResourceViewDescD3D12*>(desc);

	FD3D12NvFlowResourceRW* NvFlowResource = new FD3D12NvFlowResourceRW(
		GetParentDevice(), GetParentDevice()->GetNodeMask(), descD3D12->resource,
		descD3D12->resource->GetDesc(), descD3D12->currentState);

	// initialize CommandList resource state to avoid getting to PendingBarrierResources CommandList
	CResourceState& ResourceState = CommandListHandle.GetResourceState(&NvFlowResource->Resource);
	check(ResourceState.CheckResourceState(D3D12_RESOURCE_STATE_TBD));
	ResourceState.SetResourceState(*descD3D12->currentState);

	auto localSrvDesc = descD3D12->srvDesc;
	FD3D12ShaderResourceViewNvFlow* pSRV = new FD3D12ShaderResourceViewNvFlow(GetParentDevice(), &localSrvDesc, NvFlowResource);

	// make resource transition to SRV state
	const D3D12_RESOURCE_STATES TargetState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	FD3D12DynamicRHI::TransitionResource(CommandListHandle, pSRV, TargetState);
	*descD3D12->currentState = TargetState;

	return pSRV;
}


class FD3D12UnorderedAccessViewNvFlow : public FD3D12UnorderedAccessView
{
	TRefCountPtr<FD3D12NvFlowResourceRW> NvFlowResourceRWRef;

public:
	FD3D12UnorderedAccessViewNvFlow(FD3D12Device* InParent, D3D12_UNORDERED_ACCESS_VIEW_DESC* InUAVDesc, FD3D12NvFlowResourceRW* InNvFlowResourceRW)
		: FD3D12UnorderedAccessView(InParent, InUAVDesc, &InNvFlowResourceRW->ResourceLocation)
		, NvFlowResourceRWRef(InNvFlowResourceRW)
	{
	}
};

FRHINvFlowResourceRW* FD3D12CommandContext::NvFlowCreateResourceRW(const FRHINvFlowResourceRWViewDesc* desc, FShaderResourceViewRHIRef* pRHIRefSRV, FUnorderedAccessViewRHIRef* pRHIRefUAV)
{
	const FRHINvFlowResourceRWViewDescD3D12* descD3D12 = static_cast<const FRHINvFlowResourceRWViewDescD3D12*>(desc);

	FD3D12NvFlowResourceRW* NvFlowResourceRW = new FD3D12NvFlowResourceRW(
		GetParentDevice(), GetParentDevice()->GetNodeMask(), descD3D12->resourceView.resource,
		descD3D12->resourceView.resource->GetDesc(), descD3D12->resourceView.currentState);

	// initialize CommandList resource state to avoid getting to PendingBarrierResources CommandList
	CResourceState& ResourceState = CommandListHandle.GetResourceState(&NvFlowResourceRW->Resource);
	check(ResourceState.CheckResourceState(D3D12_RESOURCE_STATE_TBD));
	ResourceState.SetResourceState(*descD3D12->resourceView.currentState);

	if (pRHIRefSRV)
	{
		auto localSrvDesc = descD3D12->resourceView.srvDesc;
		FD3D12ShaderResourceViewNvFlow* pSRV = new FD3D12ShaderResourceViewNvFlow(GetParentDevice(), &localSrvDesc, NvFlowResourceRW);
		check(pSRV->GetViewSubresourceSubset().IsWholeResource());
		*pRHIRefSRV = pSRV;
	}
	if (pRHIRefUAV)
	{
		auto localUavDesc = descD3D12->uavDesc;
		FD3D12UnorderedAccessViewNvFlow* pUAV = new FD3D12UnorderedAccessViewNvFlow(GetParentDevice(), &localUavDesc, NvFlowResourceRW);
		check(pUAV->GetViewSubresourceSubset().IsWholeResource());
		*pRHIRefUAV = pUAV;
	}

	NvFlowResourceRW->AddRef();
	return NvFlowResourceRW;
}

void FD3D12CommandContext::NvFlowReleaseResourceRW(FRHINvFlowResourceRW* InNvFlowResourceRW)
{
	FD3D12NvFlowResourceRW* NvFlowResourceRW = static_cast<FD3D12NvFlowResourceRW*>(InNvFlowResourceRW);

	// pass updated resource state back to NvFlow!
	CResourceState& LastResourceState = CommandListHandle.GetResourceState(&NvFlowResourceRW->Resource);
	check(!LastResourceState.CheckResourceState(D3D12_RESOURCE_STATE_TBD));
	check(LastResourceState.AreAllSubresourcesSame());
	*NvFlowResourceRW->ResourceState = LastResourceState.GetSubresourceState(0);

	NvFlowResourceRW->Release();
}

void FD3D12CommandContext::NvFlowRestoreState()
{
	StateCache.GetDescriptorCache()->NotifyCurrentCommandList(CommandListHandle);

	StateCache.DirtyState();
}

// NvFlow end