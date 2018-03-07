// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Implementation of Device Context State Caching to improve draw
//	thread performance by removing redundant device context calls.

//-----------------------------------------------------------------------------
//	Include Files
//-----------------------------------------------------------------------------
#include "D3D12RHIPrivate.h"
#include <emmintrin.h>

extern bool D3D12RHI_ShouldCreateWithD3DDebug();

inline bool operator!=(D3D12_CPU_DESCRIPTOR_HANDLE lhs, D3D12_CPU_DESCRIPTOR_HANDLE rhs)
{
	return lhs.ptr != rhs.ptr;
}

// Define template functions that are only declared in the header.
template void FD3D12StateCacheBase::SetShaderResourceView<SF_Vertex>(FD3D12ShaderResourceView* SRV, uint32 ResourceIndex);
template void FD3D12StateCacheBase::SetShaderResourceView<SF_Hull>(FD3D12ShaderResourceView* SRV, uint32 ResourceIndex);
template void FD3D12StateCacheBase::SetShaderResourceView<SF_Domain>(FD3D12ShaderResourceView* SRV, uint32 ResourceIndex);
template void FD3D12StateCacheBase::SetShaderResourceView<SF_Geometry>(FD3D12ShaderResourceView* SRV, uint32 ResourceIndex);
template void FD3D12StateCacheBase::SetShaderResourceView<SF_Pixel>(FD3D12ShaderResourceView* SRV, uint32 ResourceIndex);
template void FD3D12StateCacheBase::SetShaderResourceView<SF_Compute>(FD3D12ShaderResourceView* SRV, uint32 ResourceIndex);

template void FD3D12StateCacheBase::SetUAVs<SF_Pixel>(uint32 UAVStartSlot, uint32 NumSimultaneousUAVs, FD3D12UnorderedAccessView** UAVArray, uint32* UAVInitialCountArray);
template void FD3D12StateCacheBase::SetUAVs<SF_Compute>(uint32 UAVStartSlot, uint32 NumSimultaneousUAVs, FD3D12UnorderedAccessView** UAVArray, uint32* UAVInitialCountArray);

template void FD3D12StateCacheBase::ApplyState<false>();
template void FD3D12StateCacheBase::ApplyState<true>();

#if D3D12_STATE_CACHE_RUNTIME_TOGGLE

// Default the state caching system to on.
bool GD3D12SkipStateCaching = false;

// A self registering exec helper to check for the TOGGLESTATECACHE command.
class FD3D12ToggleStateCacheExecHelper : public FSelfRegisteringExec
{
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
	{
		if (FParse::Command(&Cmd, TEXT("TOGGLESTATECACHE")))
		{
			GD3D12SkipStateCaching = !GD3D12SkipStateCaching;
			Ar.Log(FString::Printf(TEXT("D3D12 State Caching: %s"), GD3D12SkipStateCaching ? TEXT("OFF") : TEXT("ON")));
			return true;
		}
		return false;
	}
};
static FD3D12ToggleStateCacheExecHelper GD3D12ToggleStateCacheExecHelper;

#endif	// D3D12_STATE_CACHE_RUNTIME_TOGGLE

FD3D12StateCacheBase::FD3D12StateCacheBase(GPUNodeMask Node)
	: DescriptorCache(Node)
	, FD3D12SingleNodeGPUObject(Node)
{}

void FD3D12StateCacheBase::Init(FD3D12Device* InParent, FD3D12CommandContext* InCmdContext, const FD3D12StateCacheBase* AncestralState, FD3D12SubAllocatedOnlineHeap::SubAllocationDesc& SubHeapDesc)
{
	Parent = InParent;
	CmdContext = InCmdContext;

	// Cache the resource binding tier
	ResourceBindingTier = GetParentDevice()->GetParentAdapter()->GetResourceBindingTier();

	// Init the descriptor heaps
	const uint32 MaxDescriptorsForTier = (ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_1) ? NUM_VIEW_DESCRIPTORS_TIER_1 :
		NUM_VIEW_DESCRIPTORS_TIER_2;

	check(LOCAL_VIEW_HEAP_SIZE <= MaxDescriptorsForTier);
	check(GLOBAL_VIEW_HEAP_SIZE <= MaxDescriptorsForTier);

	const uint32 NumSamplerDescriptors = NUM_SAMPLER_DESCRIPTORS;
	DescriptorCache.Init(InParent, InCmdContext, LOCAL_VIEW_HEAP_SIZE, NumSamplerDescriptors, SubHeapDesc);

	if (AncestralState)
	{
		InheritState(*AncestralState);
	}
	else
	{
		ClearState();
	}
}

void FD3D12StateCacheBase::Clear()
{
	ClearState();

	// Release references to cached objects
	DescriptorCache.Clear();
}

void FD3D12StateCacheBase::ClearSRVs()
{
	if (bSRVSCleared)
	{
		return;
	}

	PipelineState.Common.SRVCache.Clear();

	bSRVSCleared = true;
}

void FD3D12StateCacheBase::FlushComputeShaderCache(bool bForce)
{
	if (bAutoFlushComputeShaderCache || bForce)
	{
		FD3D12CommandListHandle& CommandList = CmdContext->CommandListHandle;
		CommandList.AddUAVBarrier();
	}
}

void FD3D12StateCacheBase::ClearState()
{
	// Shader Resource View State Cache
	bSRVSCleared = false;
	ClearSRVs();

	PipelineState.Common.CBVCache.Clear();
	PipelineState.Common.UAVCache.Clear();
	PipelineState.Common.SamplerCache.Clear();

	FMemory::Memzero(PipelineState.Common.CurrentShaderSamplerCounts, sizeof(PipelineState.Common.CurrentShaderSamplerCounts));
	FMemory::Memzero(PipelineState.Common.CurrentShaderSRVCounts, sizeof(PipelineState.Common.CurrentShaderSRVCounts));
	FMemory::Memzero(PipelineState.Common.CurrentShaderCBCounts, sizeof(PipelineState.Common.CurrentShaderCBCounts));
	FMemory::Memzero(PipelineState.Common.CurrentShaderUAVCounts, sizeof(PipelineState.Common.CurrentShaderUAVCounts));
	
	PipelineState.Graphics.HighLevelDesc.NumRenderTargets = 0;
	PipelineState.Graphics.CurrentNumberOfStreamOutTargets = 0;
	PipelineState.Graphics.CurrentNumberOfScissorRects = 0;

	// Rasterizer State Cache
	PipelineState.Graphics.HighLevelDesc.RasterizerState = nullptr;

	// Zero the RTV array - this is necessary to prevent uninitialized memory affecting the PSO cache hash generation
	FMemory::Memzero(&PipelineState.Graphics.HighLevelDesc.RTVFormats[0], sizeof(PipelineState.Graphics.HighLevelDesc.RTVFormats[0]) * PipelineState.Graphics.HighLevelDesc.RTVFormats.Num());

	// Depth Stencil State Cache
	PipelineState.Graphics.CurrentReferenceStencil = 0;
	PipelineState.Graphics.HighLevelDesc.DepthStencilState = nullptr;
	PipelineState.Graphics.CurrentDepthStencilTarget = nullptr;

	// Shader Cache
	PipelineState.Graphics.HighLevelDesc.BoundShaderState = nullptr;
	PipelineState.Compute.CurrentComputeShader = nullptr;

	// Blend State Cache
	PipelineState.Graphics.CurrentBlendFactor[0] = 1.0f;
	PipelineState.Graphics.CurrentBlendFactor[1] = 1.0f;
	PipelineState.Graphics.CurrentBlendFactor[2] = 1.0f;
	PipelineState.Graphics.CurrentBlendFactor[3] = 1.0f;

	FMemory::Memzero(PipelineState.Graphics.CurrentViewport, sizeof(PipelineState.Graphics.CurrentViewport));
	PipelineState.Graphics.CurrentNumberOfViewports = 0;

	PipelineState.Graphics.bNeedRebuildPSO = true;
	PipelineState.Compute.bNeedRebuildPSO = true;
	PipelineState.Compute.ComputeBudget = EAsyncComputeBudget::EAll_4;
	PipelineState.Graphics.CurrentPipelineStateObject = nullptr;
	PipelineState.Compute.CurrentPipelineStateObject = nullptr;
	PipelineState.Common.CurrentPipelineStateObject = nullptr;
	FMemory::Memzero(PipelineState.Graphics.CurrentStreamOutTargets, sizeof(PipelineState.Graphics.CurrentStreamOutTargets));
	FMemory::Memzero(PipelineState.Graphics.CurrentSOOffsets, sizeof(PipelineState.Graphics.CurrentSOOffsets));

	const CD3DX12_RECT ScissorRect(0, 0, GetMax2DTextureDimension(), GetMax2DTextureDimension());
	SetScissorRect(ScissorRect);

	PipelineState.Graphics.HighLevelDesc.SampleMask = 0xffffffff;
	PipelineState.Graphics.HighLevelDesc.BlendState = nullptr;

	PipelineState.Graphics.VBCache.Clear();
	PipelineState.Graphics.IBCache.Clear();

	FMemory::Memzero(PipelineState.Graphics.RenderTargetArray, sizeof(PipelineState.Graphics.RenderTargetArray));

	PipelineState.Graphics.CurrentPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

	PipelineState.Graphics.MinDepth = 0.0f;
	PipelineState.Graphics.MaxDepth = 1.0f;

	bAutoFlushComputeShaderCache = false;

	DirtyState();
}

void FD3D12StateCacheBase::DirtyState()
{
	// Mark bits dirty so the next call to ApplyState will set all this state again
	PipelineState.Common.bNeedSetPSO = true;
	PipelineState.Compute.bNeedSetRootSignature = true;
	PipelineState.Graphics.bNeedSetRootSignature = true;
	bNeedSetVB = true;
	bNeedSetIB = true;
	bNeedSetSOs = true;
	bNeedSetRTs = true;
	bNeedSetViewports = true;
	bNeedSetScissorRects = true;
	bNeedSetPrimitiveTopology = true;
	bNeedSetBlendFactor = true;
	bNeedSetStencilRef = true;
	bNeedSetDepthBounds = true;
	PipelineState.Common.SRVCache.DirtyAll();
	PipelineState.Common.UAVCache.DirtyAll();
	PipelineState.Common.CBVCache.DirtyAll();
	PipelineState.Common.SamplerCache.DirtyAll();
}

void FD3D12StateCacheBase::DirtyViewDescriptorTables()
{
	// Mark the CBV/SRV/UAV descriptor tables dirty for the current root signature.
	// Note: Descriptor table state is undefined at the beginning of a command list and after descriptor heaps are changed on a command list.
	// This will cause the next call to ApplyState to copy and set these descriptors again.
	PipelineState.Common.SRVCache.DirtyAll();
	PipelineState.Common.UAVCache.DirtyAll();
	PipelineState.Common.CBVCache.DirtyAll(GDescriptorTableCBVSlotMask);	// Only mark descriptor table slots as dirty.
}

void FD3D12StateCacheBase::DirtySamplerDescriptorTables()
{
	// Mark the sampler descriptor tables dirty for the current root signature.
	// Note: Descriptor table state is undefined at the beginning of a command list and after descriptor heaps are changed on a command list.
	// This will cause the next call to ApplyState to copy and set these descriptors again.
	PipelineState.Common.SamplerCache.DirtyAll();
}

void FD3D12StateCacheBase::SetViewport(const D3D12_VIEWPORT& Viewport)
{
	if ((PipelineState.Graphics.CurrentNumberOfViewports != 1 || FMemory::Memcmp(&PipelineState.Graphics.CurrentViewport[0], &Viewport, sizeof(D3D12_VIEWPORT))) || GD3D12SkipStateCaching)
	{
		FMemory::Memcpy(&PipelineState.Graphics.CurrentViewport[0], &Viewport, sizeof(D3D12_VIEWPORT));
		PipelineState.Graphics.CurrentNumberOfViewports = 1;
		bNeedSetViewports = true;
		UpdateViewportScissorRects();
	}
}

void FD3D12StateCacheBase::SetViewports(uint32 Count, const D3D12_VIEWPORT* const Viewports)
{
	check(Count < _countof(PipelineState.Graphics.CurrentViewport));
	if ((PipelineState.Graphics.CurrentNumberOfViewports != Count || FMemory::Memcmp(&PipelineState.Graphics.CurrentViewport[0], Viewports, sizeof(D3D12_VIEWPORT) * Count)) || GD3D12SkipStateCaching)
	{
		FMemory::Memcpy(&PipelineState.Graphics.CurrentViewport[0], Viewports, sizeof(D3D12_VIEWPORT) * Count);
		PipelineState.Graphics.CurrentNumberOfViewports = Count;
		bNeedSetViewports = true;
		UpdateViewportScissorRects();
	}
}

void FD3D12StateCacheBase::UpdateViewportScissorRects()
{
	for (uint32 i = 0; i < PipelineState.Graphics.CurrentNumberOfScissorRects; ++i)
	{
		const D3D12_VIEWPORT& Viewport = PipelineState.Graphics.CurrentViewport[FMath::Min(i, PipelineState.Graphics.CurrentNumberOfViewports)];
		const D3D12_RECT& ScissorRect = PipelineState.Graphics.CurrentScissorRects[i];
		D3D12_RECT& ViewportScissorRect = PipelineState.Graphics.CurrentViewportScissorRects[i];

		ViewportScissorRect.top = FMath::Max(ScissorRect.top, (LONG)Viewport.TopLeftY);
		ViewportScissorRect.left = FMath::Max(ScissorRect.left, (LONG)Viewport.TopLeftX);
		ViewportScissorRect.bottom = FMath::Min(ScissorRect.bottom, (LONG)Viewport.TopLeftY + (LONG)Viewport.Height);
		ViewportScissorRect.right = FMath::Min(ScissorRect.right, (LONG)Viewport.TopLeftX + (LONG)Viewport.Width);

		const bool ViewportEmpty = Viewport.Width <= 0 || Viewport.Height <= 0;
		const bool ScissorEmpty = ViewportScissorRect.right <= ViewportScissorRect.left || ViewportScissorRect.bottom <= ViewportScissorRect.top;
		check(!ViewportEmpty || ScissorEmpty);
	}

	bNeedSetScissorRects = true;
}

void FD3D12StateCacheBase::SetScissorRect(const D3D12_RECT& ScissorRect)
{
	if ((PipelineState.Graphics.CurrentNumberOfScissorRects != 1 || FMemory::Memcmp(&PipelineState.Graphics.CurrentScissorRects[0], &ScissorRect, sizeof(D3D12_RECT))) || GD3D12SkipStateCaching)
	{
		FMemory::Memcpy(&PipelineState.Graphics.CurrentScissorRects[0], &ScissorRect, sizeof(D3D12_RECT));
		PipelineState.Graphics.CurrentNumberOfScissorRects = 1;
		UpdateViewportScissorRects();
	}
}

void FD3D12StateCacheBase::SetScissorRects(uint32 Count, const D3D12_RECT* const ScissorRects)
{
	check(Count < _countof(PipelineState.Graphics.CurrentScissorRects));
	if ((PipelineState.Graphics.CurrentNumberOfScissorRects != Count || FMemory::Memcmp(&PipelineState.Graphics.CurrentScissorRects[0], ScissorRects, sizeof(D3D12_RECT) * Count)) || GD3D12SkipStateCaching)
	{
		FMemory::Memcpy(&PipelineState.Graphics.CurrentScissorRects[0], ScissorRects, sizeof(D3D12_RECT) * Count);
		PipelineState.Graphics.CurrentNumberOfScissorRects = Count;
		UpdateViewportScissorRects();
	}
}

template <bool IsCompute>
void FD3D12StateCacheBase::ApplyState()
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12ApplyStateTime);
	const bool bForceState = false;
	if (bForceState)
	{
		// Mark all state as dirty.
		DirtyState();
	}

#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	CmdContext->FlushTextureCacheIfNeeded();
#endif

	FD3D12CommandListHandle& CommandList = CmdContext->CommandListHandle;
	const FD3D12RootSignature* const pRootSignature = IsCompute ?
		PipelineState.Compute.CurrentComputeShader->pRootSignature : PipelineState.Graphics.HighLevelDesc.BoundShaderState->pRootSignature;

	// PSO
	FD3D12PipelineState* Pso = nullptr;
	if (IsCompute)
	{
		if (PipelineState.Compute.bNeedRebuildPSO)
		{
			Pso = CommitPendingComputePipelineState();
		}

		// See if we need to set a compute root signature
		if (PipelineState.Compute.bNeedSetRootSignature)
		{
			CommandList->SetComputeRootSignature(pRootSignature->GetRootSignature());
			PipelineState.Compute.bNeedSetRootSignature = false;

			// After setting a root signature, all root parameters are undefined and must be set again.
			PipelineState.Common.SRVCache.DirtyCompute();
			PipelineState.Common.UAVCache.DirtyCompute();
			PipelineState.Common.SamplerCache.DirtyCompute();
			PipelineState.Common.CBVCache.DirtyCompute();
		}
	}
	else
	{
		if (PipelineState.Graphics.bNeedRebuildPSO)
		{
			// The desc is mostly initialized, just need to copy the RTV/DSV formats and sample properties in
			FD3D12HighLevelGraphicsPipelineStateDesc& PsoDesc = PipelineState.Graphics.HighLevelDesc;

			FMemory::Memzero(&PsoDesc.RTVFormats[0], sizeof(PsoDesc.RTVFormats[0]) * PsoDesc.RTVFormats.Num());
			PsoDesc.SampleDesc.Count = 0;
			PsoDesc.SampleDesc.Quality = 0;

			for (uint32 i = 0; i < PsoDesc.NumRenderTargets; i++)
			{
				if (PipelineState.Graphics.RenderTargetArray[i] != nullptr)
				{
					const D3D12_RENDER_TARGET_VIEW_DESC &desc = PipelineState.Graphics.RenderTargetArray[i]->GetDesc();
					D3D12_RESOURCE_DESC const& resDesc = PipelineState.Graphics.RenderTargetArray[i]->GetResource()->GetDesc();

					if (desc.Format == DXGI_FORMAT_UNKNOWN)
					{
						PsoDesc.RTVFormats[i] = resDesc.Format;
					}
					else
					{
						PsoDesc.RTVFormats[i] = desc.Format;
					}
					check(PsoDesc.RTVFormats[i] != DXGI_FORMAT_UNKNOWN);

					if (PsoDesc.SampleDesc.Count == 0)
					{
						PsoDesc.SampleDesc.Count = resDesc.SampleDesc.Count;
						PsoDesc.SampleDesc.Quality = resDesc.SampleDesc.Quality;
					}
				}
			}

			PsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
			if (PipelineState.Graphics.CurrentDepthStencilTarget)
			{
				const D3D12_DEPTH_STENCIL_VIEW_DESC &dsvDesc = PipelineState.Graphics.CurrentDepthStencilTarget->GetDesc();
				D3D12_RESOURCE_DESC const& resDesc = PipelineState.Graphics.CurrentDepthStencilTarget->GetResource()->GetDesc();

				PsoDesc.DSVFormat = dsvDesc.Format;
				if (PsoDesc.NumRenderTargets == 0 || PsoDesc.SampleDesc.Count == 0)
				{
					PsoDesc.SampleDesc.Count = resDesc.SampleDesc.Count;
					PsoDesc.SampleDesc.Quality = resDesc.SampleDesc.Quality;
				}
			}

			Pso = CommitPendingGraphicsPipelineState();
		}

		// See if we need to set a graphics root signature
		if (PipelineState.Graphics.bNeedSetRootSignature)
		{
			CommandList->SetGraphicsRootSignature(pRootSignature->GetRootSignature());
			PipelineState.Graphics.bNeedSetRootSignature = false;

			// After setting a root signature, all root parameters are undefined and must be set again.
			PipelineState.Common.SRVCache.DirtyGraphics();
			PipelineState.Common.UAVCache.DirtyGraphics();
			PipelineState.Common.SamplerCache.DirtyGraphics();
			PipelineState.Common.CBVCache.DirtyGraphics();
		}
	}

	SetPipelineState<IsCompute>(Pso);

	// Need to cache compute budget, as we need to reset after PSO changes
	if (IsCompute && CommandList->GetType() == D3D12_COMMAND_LIST_TYPE_COMPUTE)
	{
		CmdContext->SetAsyncComputeBudgetInternal(PipelineState.Compute.ComputeBudget);
	}

	if (!IsCompute)
	{
		// Setup non-heap bindings
		if (bNeedSetVB)
		{
			//SCOPE_CYCLE_COUNTER(STAT_D3D12ApplyStateSetVertexBufferTime);
			DescriptorCache.SetVertexBuffers(PipelineState.Graphics.VBCache);
			bNeedSetVB = false;
		}
		if (bNeedSetIB)
		{
			if (PipelineState.Graphics.IBCache.CurrentIndexBufferLocation != nullptr)
			{
				DescriptorCache.SetIndexBuffer(PipelineState.Graphics.IBCache);
			}
			bNeedSetIB = false;
		}
		if (bNeedSetSOs)
		{
			DescriptorCache.SetStreamOutTargets(PipelineState.Graphics.CurrentStreamOutTargets, PipelineState.Graphics.CurrentNumberOfStreamOutTargets, PipelineState.Graphics.CurrentSOOffsets);
			bNeedSetSOs = false;
		}
		if (bNeedSetViewports)
		{
			CommandList->RSSetViewports(PipelineState.Graphics.CurrentNumberOfViewports, PipelineState.Graphics.CurrentViewport);
			bNeedSetViewports = false;
		}
		if (bNeedSetScissorRects)
		{
			CommandList->RSSetScissorRects(PipelineState.Graphics.CurrentNumberOfScissorRects, PipelineState.Graphics.CurrentViewportScissorRects);
			bNeedSetScissorRects = false;
		}
		if (bNeedSetPrimitiveTopology)
		{
			CommandList->IASetPrimitiveTopology(PipelineState.Graphics.CurrentPrimitiveTopology);
			bNeedSetPrimitiveTopology = false;
		}
		if (bNeedSetBlendFactor)
		{
			CommandList->OMSetBlendFactor(PipelineState.Graphics.CurrentBlendFactor);
			bNeedSetBlendFactor = false;
		}
		if (bNeedSetStencilRef)
		{
			CommandList->OMSetStencilRef(PipelineState.Graphics.CurrentReferenceStencil);
			bNeedSetStencilRef = false;
		}
		if (bNeedSetRTs)
		{
			DescriptorCache.SetRenderTargets(PipelineState.Graphics.RenderTargetArray, PipelineState.Graphics.HighLevelDesc.NumRenderTargets, PipelineState.Graphics.CurrentDepthStencilTarget);
			bNeedSetRTs = false;
		}
		if (bNeedSetDepthBounds)
		{
			CmdContext->SetDepthBounds(PipelineState.Graphics.MinDepth, PipelineState.Graphics.MaxDepth);
			bNeedSetDepthBounds = false;
		}
	}

	const uint32 StartStage = IsCompute ? SF_Compute : 0;
	const uint32 EndStage = IsCompute ? SF_NumFrequencies : SF_Compute;
	const EShaderFrequency UAVStage = IsCompute ? SF_Compute : SF_Pixel;

	//
	// Reserve space in descriptor heaps
	// Since this can cause heap rollover (which causes old bindings to become invalid), the reserve must be done atomically
	//

	// Samplers
	ApplySamplers(pRootSignature, StartStage, EndStage);

	// Determine what resource bind slots are dirty for the current shaders and how many descriptor table slots we need.
	// We only set dirty resources that can be used for the upcoming Draw/Dispatch.
	SRVSlotMask CurrentShaderDirtySRVSlots[SF_NumFrequencies] = {};
	CBVSlotMask CurrentShaderDirtyCBVSlots[SF_NumFrequencies] = {};
	UAVSlotMask CurrentShaderDirtyUAVSlots = 0;
	uint32 NumUAVs = 0;
	uint32 NumSRVs[SF_NumFrequencies] = {};
#if USE_STATIC_ROOT_SIGNATURE
	uint32 NumCBVs[SF_NumFrequencies] ={};
#endif
	uint32 NumViews = 0;
	for (uint32 iTries = 0; iTries < 2; ++iTries)
	{
		const UAVSlotMask CurrentShaderUAVRegisterMask = (1 << PipelineState.Common.CurrentShaderUAVCounts[UAVStage]) - 1;
		CurrentShaderDirtyUAVSlots = CurrentShaderUAVRegisterMask & PipelineState.Common.UAVCache.DirtySlotMask[UAVStage];
		if (CurrentShaderDirtyUAVSlots)
		{
			if (ResourceBindingTier <= D3D12_RESOURCE_BINDING_TIER_2)
			{
				// Tier 1 and 2 HW requires the full number of UAV descriptors defined in the root signature's descriptor table.
				NumUAVs = pRootSignature->MaxUAVCount(UAVStage);
			}
			else
			{
				NumUAVs = PipelineState.Common.CurrentShaderUAVCounts[UAVStage];
			}

			check(NumUAVs > 0 && NumUAVs <= MAX_UAVS);
			NumViews += NumUAVs;
		}

		for (uint32 Stage = StartStage; Stage < EndStage; ++Stage)
		{
			// Note this code assumes the starting register is index 0.
			const SRVSlotMask CurrentShaderSRVRegisterMask = (1 << PipelineState.Common.CurrentShaderSRVCounts[Stage]) - 1;
			CurrentShaderDirtySRVSlots[Stage] = CurrentShaderSRVRegisterMask & PipelineState.Common.SRVCache.DirtySlotMask[Stage];
			if (CurrentShaderDirtySRVSlots[Stage])
			{
				if (ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_1)
				{
					// Tier 1 HW requires the full number of SRV descriptors defined in the root signature's descriptor table.
					NumSRVs[Stage] = pRootSignature->MaxSRVCount(Stage);
				}
				else
				{
					NumSRVs[Stage] = PipelineState.Common.CurrentShaderSRVCounts[Stage];
				}

				check(NumSRVs[Stage] > 0 && NumSRVs[Stage] <= MAX_SRVS);
				NumViews += NumSRVs[Stage];
			}

			const CBVSlotMask CurrentShaderCBVRegisterMask = (1 << PipelineState.Common.CurrentShaderCBCounts[Stage]) - 1;
			CurrentShaderDirtyCBVSlots[Stage] = CurrentShaderCBVRegisterMask & PipelineState.Common.CBVCache.DirtySlotMask[Stage];
#if USE_STATIC_ROOT_SIGNATURE
			if (CurrentShaderDirtyCBVSlots[Stage])
			{
				if (ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_1)
				{
					// Tier 1 HW requires the full number of SRV descriptors defined in the root signature's descriptor table.
					NumCBVs[Stage] = pRootSignature->MaxCBVCount(Stage);
				}
				else
				{
					NumCBVs[Stage] = PipelineState.Common.CurrentShaderCBCounts[Stage];
				}

				check(NumCBVs[Stage] > 0 && NumCBVs[Stage] <= MAX_SRVS);
				NumViews += NumCBVs[Stage];
			}
#endif
			// Note: CBVs don't currently use descriptor tables but we still need to know what resource point slots are dirty.
		}

		// See if the descriptor slots will fit
		if (!DescriptorCache.GetCurrentViewHeap()->CanReserveSlots(NumViews))
		{
			const bool bDescriptorHeapsChanged = DescriptorCache.GetCurrentViewHeap()->RollOver();
			if (bDescriptorHeapsChanged)
			{
				// If descriptor heaps changed, then all our tables are dirty again and we need to recalculate the number of slots we need.
				NumViews = 0;
				continue;
			}
		}

		// We can reserve slots in the descriptor heap, no need to loop again.
		break;
	}

	uint32 ViewHeapSlot = DescriptorCache.GetCurrentViewHeap()->ReserveSlots(NumViews);

	// Unordered access views
	if (CurrentShaderDirtyUAVSlots)
	{
		SCOPE_CYCLE_COUNTER(STAT_D3D12ApplyStateSetUAVTime);
		DescriptorCache.SetUAVs<UAVStage>(PipelineState.Common.UAVCache, CurrentShaderDirtyUAVSlots, NumUAVs, ViewHeapSlot);
	}

	// Shader resource views
	{
		//SCOPE_CYCLE_COUNTER(STAT_D3D12ApplyStateSetSRVTime);
		FD3D12ShaderResourceViewCache& SRVCache = PipelineState.Common.SRVCache;

#define CONDITIONAL_SET_SRVS(Shader) \
		if (CurrentShaderDirtySRVSlots[##Shader]) \
		{ \
			DescriptorCache.SetSRVs<##Shader>(SRVCache, CurrentShaderDirtySRVSlots[##Shader], NumSRVs[##Shader], ViewHeapSlot); \
		}

		if (IsCompute)
		{
			CONDITIONAL_SET_SRVS(SF_Compute);
		}
		else
		{
			CONDITIONAL_SET_SRVS(SF_Vertex);
			CONDITIONAL_SET_SRVS(SF_Hull);
			CONDITIONAL_SET_SRVS(SF_Domain);
			CONDITIONAL_SET_SRVS(SF_Geometry);
			CONDITIONAL_SET_SRVS(SF_Pixel);
		}
#undef CONDITIONAL_SET_SRVS
	}

	// Constant buffers
	{
		//SCOPE_CYCLE_COUNTER(STAT_D3D12ApplyStateSetConstantBufferTime);
		FD3D12ConstantBufferCache& CBVCache = PipelineState.Common.CBVCache;

#if USE_STATIC_ROOT_SIGNATURE
	#define CONDITIONAL_SET_CBVS(Shader) \
		if (CurrentShaderDirtyCBVSlots[##Shader]) \
		{ \
			DescriptorCache.SetConstantBuffers<##Shader>(CBVCache, CurrentShaderDirtyCBVSlots[##Shader], NumCBVs[##Shader], ViewHeapSlot); \
		}
#else
	#define CONDITIONAL_SET_CBVS(Shader) \
		if (CurrentShaderDirtyCBVSlots[##Shader]) \
		{ \
			DescriptorCache.SetConstantBuffers<##Shader>(CBVCache, CurrentShaderDirtyCBVSlots[##Shader]); \
		}
#endif

		if (IsCompute)
		{
			CONDITIONAL_SET_CBVS(SF_Compute);
		}
		else
		{
			CONDITIONAL_SET_CBVS(SF_Vertex);
			CONDITIONAL_SET_CBVS(SF_Hull);
			CONDITIONAL_SET_CBVS(SF_Domain);
			CONDITIONAL_SET_CBVS(SF_Geometry);
			CONDITIONAL_SET_CBVS(SF_Pixel);
		}
#undef CONDITIONAL_SET_CBVS
	}

	// Flush any needed resource barriers
	CommandList.FlushResourceBarriers();

#if ASSERT_RESOURCE_STATES
	bool bSucceeded = AssertResourceStates(IsCompute);
	check(bSucceeded);
#endif
}

void FD3D12StateCacheBase::ApplySamplers(const FD3D12RootSignature* const pRootSignature, uint32 StartStage, uint32 EndStage)
{
	bool HighLevelCacheMiss = false;

	FD3D12SamplerStateCache& Cache = PipelineState.Common.SamplerCache;
	SamplerSlotMask CurrentShaderDirtySamplerSlots[SF_NumFrequencies] = {};
	uint32 NumSamplers[SF_NumFrequencies + 1] = {};

	const auto& pfnCalcSamplersNeeded = [&]()
	{
		NumSamplers[SF_NumFrequencies] = 0;

		for (uint32 Stage = StartStage; Stage < EndStage; ++Stage)
		{
			// Note this code assumes the starting register is index 0.
			const SamplerSlotMask CurrentShaderSamplerRegisterMask = (1 << PipelineState.Common.CurrentShaderSamplerCounts[Stage]) - 1;
			CurrentShaderDirtySamplerSlots[Stage] = CurrentShaderSamplerRegisterMask & Cache.DirtySlotMask[Stage];
			if (CurrentShaderDirtySamplerSlots[Stage])
			{
				if (ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_1)
				{
					// Tier 1 HW requires the full number of sampler descriptors defined in the root signature.
					NumSamplers[Stage] = pRootSignature->MaxSamplerCount(Stage);
				}
				else
				{
					NumSamplers[Stage] = PipelineState.Common.CurrentShaderSamplerCounts[Stage];
				}

				check(NumSamplers[Stage] > 0 && NumSamplers[Stage] <= MAX_SAMPLERS);
				NumSamplers[SF_NumFrequencies] += NumSamplers[Stage];
			}
		}
	};

	pfnCalcSamplersNeeded();

	if (DescriptorCache.UsingGlobalSamplerHeap())
	{
		auto& GlobalSamplerSet = DescriptorCache.GetLocalSamplerSet();
		FD3D12CommandListHandle& CommandList = CmdContext->CommandListHandle;

		for (uint32 Stage = StartStage; Stage < EndStage; Stage++)
		{
			if ((CurrentShaderDirtySamplerSlots[Stage] && NumSamplers[Stage]))
			{
				SamplerSlotMask& CurrentDirtySlotMask = Cache.DirtySlotMask[Stage];
				FD3D12SamplerState** Samplers = Cache.States[Stage];

				FD3D12UniqueSamplerTable Table;
				Table.Key.Count = NumSamplers[Stage];

				for (uint32 i = 0; i < NumSamplers[Stage]; i++)
				{
					Table.Key.SamplerID[i] = Samplers[i] ? Samplers[i]->ID : 0;
					FD3D12SamplerStateCache::CleanSlot(CurrentDirtySlotMask, i);
				}

				FD3D12UniqueSamplerTable* CachedTable = GlobalSamplerSet.Find(Table);
				if (CachedTable)
				{
					// Make sure the global sampler heap is really set on the command list before we try to find a cached descriptor table for it.
					check(DescriptorCache.IsHeapSet(GetParentDevice()->GetGlobalSamplerHeap().GetHeap()));
					check(CachedTable->GPUHandle.ptr);
					if (Stage == SF_Compute)
					{
						const uint32 RDTIndex = CmdContext->StateCache.GetComputeRootSignature()->SamplerRDTBindSlot(EShaderFrequency(Stage));
						CommandList->SetComputeRootDescriptorTable(RDTIndex, CachedTable->GPUHandle);
					}
					else
					{
						const uint32 RDTIndex = CmdContext->StateCache.GetGraphicsRootSignature()->SamplerRDTBindSlot(EShaderFrequency(Stage));
						CommandList->SetGraphicsRootDescriptorTable(RDTIndex, CachedTable->GPUHandle);
					}

					// We changed the descriptor table, so all resources bound to slots outside of the table's range are now dirty.
					// If a shader needs to use resources bound to these slots later, we need to set the descriptor table again to ensure those
					// descriptors are valid.
					const SamplerSlotMask OutsideCurrentTableRegisterMask = ~((1 << Table.Key.Count) - 1);
					Cache.Dirty(static_cast<EShaderFrequency>(Stage), OutsideCurrentTableRegisterMask);
				}
				else
				{
					HighLevelCacheMiss = true;
					break;
				}
			}
		}

		if (!HighLevelCacheMiss)
		{
			// Success, all the tables were found in the high level heap
			return;
		}
	}

	if (HighLevelCacheMiss)
	{
		// Move to per context heap strategy
		const bool bDescriptorHeapsChanged = DescriptorCache.SwitchToContextLocalSamplerHeap();
		if (bDescriptorHeapsChanged)
		{
			// If descriptor heaps changed, then all our tables are dirty again and we need to recalculate the number of slots we need.
			pfnCalcSamplersNeeded();
		}
	}

	FD3D12OnlineHeap* const SamplerHeap = DescriptorCache.GetCurrentSamplerHeap();
	check(DescriptorCache.UsingGlobalSamplerHeap() == false);
	check(SamplerHeap != &GetParentDevice()->GetGlobalSamplerHeap());
	check(DescriptorCache.IsHeapSet(SamplerHeap->GetHeap()));
	check(!DescriptorCache.IsHeapSet(GetParentDevice()->GetGlobalSamplerHeap().GetHeap()));

	if (!SamplerHeap->CanReserveSlots(NumSamplers[SF_NumFrequencies]))
	{
		const bool bDescriptorHeapsChanged = SamplerHeap->RollOver();
		if (bDescriptorHeapsChanged)
		{
			// If descriptor heaps changed, then all our tables are dirty again and we need to recalculate the number of slots we need.
			pfnCalcSamplersNeeded();
		}
	}
	uint32 SamplerHeapSlot = SamplerHeap->ReserveSlots(NumSamplers[SF_NumFrequencies]);

#define CONDITIONAL_SET_SAMPLERS(Shader) \
	if (CurrentShaderDirtySamplerSlots[##Shader]) \
	{ \
		DescriptorCache.SetSamplers<##Shader>(Cache, CurrentShaderDirtySamplerSlots[##Shader], NumSamplers[##Shader], SamplerHeapSlot); \
	}

	if (StartStage == SF_Compute)
	{
		CONDITIONAL_SET_SAMPLERS(SF_Compute);
	}
	else
	{
		CONDITIONAL_SET_SAMPLERS(SF_Vertex);
		CONDITIONAL_SET_SAMPLERS(SF_Hull);
		CONDITIONAL_SET_SAMPLERS(SF_Domain);
		CONDITIONAL_SET_SAMPLERS(SF_Geometry);
		CONDITIONAL_SET_SAMPLERS(SF_Pixel);
	}
#undef CONDITIONAL_SET_SAMPLERS

	SamplerHeap->SetNextSlot(SamplerHeapSlot);
}

bool FD3D12StateCacheBase::AssertResourceStates(const bool IsCompute)
{
// This requires the debug layer and isn't an option for Xbox. 
#if PLATFORM_XBOXONE
	UE_LOG(LogD3D12RHI, Log, TEXT("*** VerifyResourceStates requires the debug layer ***"), this);
	return true;
#else
	// Can only verify resource states if the debug layer is used
	static const bool bWithD3DDebug = D3D12RHI_ShouldCreateWithD3DDebug();
	if (!bWithD3DDebug)
	{
		UE_LOG(LogD3D12RHI, Fatal, TEXT("*** AssertResourceStates requires the debug layer ***"));
		return false;
	}

	// Get the debug command queue
	ID3D12CommandList* pCommandList = CmdContext->CommandListHandle.CommandList();
	TRefCountPtr<ID3D12DebugCommandList> pDebugCommandList;
	VERIFYD3D12RESULT(pCommandList->QueryInterface(pDebugCommandList.GetInitReference()));

	// Note: There is nothing special to check when IsCompute = true
	if (!IsCompute)
	{
		//
		// Verify graphics pipeline state
		//

		// DSV
		{
			FD3D12DepthStencilView* pCurrentView = PipelineState.Graphics.CurrentDepthStencilTarget;

			if (pCurrentView)
			{
				// Check if the depth/stencil resource has an SRV bound
				const bool bSRVBound = PipelineState.Common.SRVCache.NumViewsIntersectWithDepthCount > 0;
				uint32 SanityCheckCount = 0;
				const uint32 StartStage = 0;
				const uint32 EndStage = SF_Compute;
				for (uint32 Stage = StartStage; Stage < EndStage; Stage++)
				{
					for (uint32 i = 0; i < MAX_SRVS; i++)
					{
						if (PipelineState.Common.SRVCache.ViewsIntersectWithDepthRT[Stage][i])
						{
							SanityCheckCount++;
						}
					}
				}
				check(SanityCheckCount == PipelineState.Common.SRVCache.NumViewsIntersectWithDepthCount);

				const D3D12_DEPTH_STENCIL_VIEW_DESC& desc = pCurrentView->GetDesc();
				const bool bDepthIsReadOnly = !!(desc.Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH);
				const bool bStencilIsReadOnly = !!(desc.Flags & D3D12_DSV_FLAG_READ_ONLY_STENCIL);

				// Decompose the view into the subresources (depth and stencil are on different planes)
				FD3D12Resource* pResource = pCurrentView->GetResource();
				const CViewSubresourceSubset subresourceSubset = pCurrentView->GetViewSubresourceSubset();
				for (CViewSubresourceSubset::CViewSubresourceIterator it = subresourceSubset.begin(); it != subresourceSubset.end(); ++it)
				{
					for (uint32 SubresourceIndex = it.StartSubresource(); SubresourceIndex < it.EndSubresource(); SubresourceIndex++)
					{
						uint16 MipSlice;
						uint16 ArraySlice;
						uint8 PlaneSlice;
						D3D12DecomposeSubresource(SubresourceIndex,
							pResource->GetMipLevels(),
							pResource->GetArraySize(),
							MipSlice, ArraySlice, PlaneSlice);

						D3D12_RESOURCE_STATES expectedState;
						if (PlaneSlice == 0)
						{
							// Depth plane
							expectedState = bDepthIsReadOnly ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;
							if (bSRVBound)
							{
								// Depth SRVs just contain the depth plane
								check(bDepthIsReadOnly);
								expectedState |=
									D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
									D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
							}
						}
						else
						{
							// Stencil plane
							expectedState = bStencilIsReadOnly ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;
						}

						bool bGoodState = !!pDebugCommandList->AssertResourceState(pResource->GetResource(), SubresourceIndex, expectedState);
						if (!bGoodState)
						{
							return false;
						}
					}
				}

			}
		}

		// RTV
		{
			const uint32 numRTVs = _countof(PipelineState.Graphics.RenderTargetArray);
			for (uint32 i = 0; i < numRTVs; i++)
			{
				FD3D12RenderTargetView* pCurrentView = PipelineState.Graphics.RenderTargetArray[i];
				if (!AssertResourceState(pCommandList, pCurrentView, D3D12_RESOURCE_STATE_RENDER_TARGET))
				{
					return false;
				}
			}
		}

		// TODO: Verify vertex buffer, index buffer, and constant buffer state.
	}

	//
	// Verify common pipeline state
	//

	const uint32 StartStage = IsCompute ? SF_Compute : 0;
	const uint32 EndStage = IsCompute ? SF_NumFrequencies : SF_Compute;
	for (uint32 Stage = StartStage; Stage < EndStage; Stage++)
	{
		// UAVs
		{
			const uint32 numUAVs = PipelineState.Common.CurrentShaderUAVCounts[Stage];
			for (uint32 i = 0; i < numUAVs; i++)
			{
				FD3D12UnorderedAccessView *pCurrentView = PipelineState.Common.UAVCache.Views[Stage][i];
				if (!AssertResourceState(pCommandList, pCurrentView, D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
				{
					return false;
				}
			}
		}

		// SRVs
		{
			const uint32 numSRVs = PipelineState.Common.CurrentShaderSRVCounts[Stage];
			for (uint32 i = 0; i < numSRVs; i++)
			{
				FD3D12ShaderResourceView* pCurrentView = PipelineState.Common.SRVCache.Views[Stage][i];
				if (!AssertResourceState(pCommandList, pCurrentView, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))
				{
					return false;
				}
			}
		}
	}

	return true;
#endif
}

template <EShaderFrequency ShaderStage>
void FD3D12StateCacheBase::SetUAVs(uint32 UAVStartSlot, uint32 NumSimultaneousUAVs, FD3D12UnorderedAccessView** UAVArray, uint32* UAVInitialCountArray)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12SetUnorderedAccessViewTime);
	check(NumSimultaneousUAVs > 0);

	FD3D12UnorderedAccessViewCache& Cache = PipelineState.Common.UAVCache;

	// When setting UAV's for Graphics, it wipes out all existing bound resources.
	const bool bIsCompute = ShaderStage == SF_Compute;
	Cache.StartSlot[ShaderStage] = bIsCompute ? FMath::Min(UAVStartSlot, Cache.StartSlot[ShaderStage]) : UAVStartSlot;

	for (uint32 i = 0; i < NumSimultaneousUAVs; ++i)
	{
		FD3D12UnorderedAccessView* UAV = UAVArray[i];

		Cache.Views[ShaderStage][UAVStartSlot + i] = UAV;
		FD3D12UnorderedAccessViewCache::DirtySlot(Cache.DirtySlotMask[ShaderStage], UAVStartSlot + i);

		if (UAV)
		{
			Cache.ResidencyHandles[ShaderStage][i] = UAV->GetResidencyHandle();

			if (UAV->CounterResource && (!UAV->CounterResourceInitialized || UAVInitialCountArray[i] != -1))
			{
				FD3D12ResourceLocation UploadBufferLocation(GetParentDevice());
#if USE_STATIC_ROOT_SIGNATURE
				uint32* CounterUploadHeapData = static_cast<uint32*>(CmdContext->ConstantsAllocator.Allocate(sizeof(uint32), UploadBufferLocation, nullptr));
#else
				uint32* CounterUploadHeapData = static_cast<uint32*>(CmdContext->ConstantsAllocator.Allocate(sizeof(uint32), UploadBufferLocation));
#endif

				// Initialize the counter to 0 if it's not been previously initialized and the UAVInitialCount is -1, if not use the value that was passed.
				*CounterUploadHeapData = (!UAV->CounterResourceInitialized && UAVInitialCountArray[i] == -1) ? 0 : UAVInitialCountArray[i];

				CmdContext->CommandListHandle->CopyBufferRegion(
					UAV->CounterResource->GetResource(),
					0,
					UploadBufferLocation.GetResource()->GetResource(),
					UploadBufferLocation.GetOffsetFromBaseOfResource(),
					4);

				CmdContext->CommandListHandle.UpdateResidency(UAV->CounterResource);

				UAV->CounterResourceInitialized = true;
			}
		}
		else
		{
			Cache.ResidencyHandles[ShaderStage][i] = nullptr;
		}
	}
}

void FD3D12StateCacheBase::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType)
{
	if (PipelineState.Graphics.HighLevelDesc.PrimitiveTopologyType != PrimitiveTopologyType || GD3D12SkipStateCaching)
	{
		PipelineState.Graphics.HighLevelDesc.PrimitiveTopologyType = PrimitiveTopologyType;
		PipelineState.Graphics.bNeedRebuildPSO = true;

		PipelineState.Graphics.CurrentPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
		bNeedSetPrimitiveTopology = true;
	}
}

void FD3D12StateCacheBase::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
	if ((PipelineState.Graphics.CurrentPrimitiveTopology != PrimitiveTopology) || GD3D12SkipStateCaching)
	{
		PipelineState.Graphics.CurrentPrimitiveTopology = PrimitiveTopology;
		bNeedSetPrimitiveTopology = true;

		D3D12_PRIMITIVE_TOPOLOGY_TYPE CurrentPrimitiveTopologyType = D3D12PrimitiveTypeToTopologyType(PipelineState.Graphics.CurrentPrimitiveTopology);

		if (PipelineState.Graphics.HighLevelDesc.PrimitiveTopologyType != CurrentPrimitiveTopologyType || GD3D12SkipStateCaching)
		{
			PipelineState.Graphics.HighLevelDesc.PrimitiveTopologyType = CurrentPrimitiveTopologyType;
			PipelineState.Graphics.bNeedRebuildPSO = true;
		}
	}
}

void FD3D12StateCacheBase::SetBlendState(D3D12_BLEND_DESC* State, const float BlendFactor[4], uint32 SampleMask)
{
	if (PipelineState.Graphics.HighLevelDesc.BlendState != State || PipelineState.Graphics.HighLevelDesc.SampleMask != SampleMask || GD3D12SkipStateCaching)
	{
		PipelineState.Graphics.HighLevelDesc.BlendState = State;
		PipelineState.Graphics.HighLevelDesc.SampleMask = SampleMask;
		PipelineState.Graphics.bNeedRebuildPSO = true;
	}

	SetBlendFactor(BlendFactor);
}

void FD3D12StateCacheBase::SetBlendFactor(const float BlendFactor[4])
{
	if (FMemory::Memcmp(PipelineState.Graphics.CurrentBlendFactor, BlendFactor, sizeof(PipelineState.Graphics.CurrentBlendFactor)))
	{
		FMemory::Memcpy(PipelineState.Graphics.CurrentBlendFactor, BlendFactor, sizeof(PipelineState.Graphics.CurrentBlendFactor));
		bNeedSetBlendFactor = true;
	}
}

void FD3D12StateCacheBase::SetDepthStencilState(D3D12_DEPTH_STENCIL_DESC* State, uint32 RefStencil)
{
	if (PipelineState.Graphics.HighLevelDesc.DepthStencilState != State || GD3D12SkipStateCaching)
	{
		PipelineState.Graphics.HighLevelDesc.DepthStencilState = State;
		PipelineState.Graphics.bNeedRebuildPSO = true;
	}

	SetStencilRef(RefStencil);
}

void FD3D12StateCacheBase::SetStencilRef(uint32 StencilRef)
{
	if (PipelineState.Graphics.CurrentReferenceStencil != StencilRef)
	{
		PipelineState.Graphics.CurrentReferenceStencil = StencilRef;
		bNeedSetStencilRef = true;
	}
}

void FD3D12StateCacheBase::SetComputeShader(FD3D12ComputeShader* Shader)
{
	if (PipelineState.Compute.CurrentComputeShader != Shader)
	{
		// See if we need to change the root signature
		const FD3D12RootSignature* const pCurrentRootSignature = PipelineState.Compute.CurrentComputeShader ? PipelineState.Compute.CurrentComputeShader->pRootSignature : nullptr;
		const FD3D12RootSignature* const pNewRootSignature = Shader ? Shader->pRootSignature : nullptr;
		if (pCurrentRootSignature != pNewRootSignature)
		{
			PipelineState.Compute.bNeedSetRootSignature = true;
		}

		PipelineState.Compute.CurrentComputeShader = Shader;
		PipelineState.Compute.bNeedRebuildPSO = true;
		PipelineState.Common.CurrentShaderSamplerCounts[SF_Compute] = (Shader) ? Shader->ResourceCounts.NumSamplers : 0;
		PipelineState.Common.CurrentShaderSRVCounts[SF_Compute] = (Shader) ? Shader->ResourceCounts.NumSRVs : 0;
		PipelineState.Common.CurrentShaderCBCounts[SF_Compute] = (Shader) ? Shader->ResourceCounts.NumCBs : 0;
		PipelineState.Common.CurrentShaderUAVCounts[SF_Compute] = (Shader) ? Shader->ResourceCounts.NumUAVs : 0;

		// Shader changed so its resource table is dirty
		CmdContext->DirtyUniformBuffers[SF_Compute] = 0xffff;
	}
}

void FD3D12StateCacheBase::InternalSetIndexBuffer(FD3D12ResourceLocation* IndexBufferLocation, DXGI_FORMAT Format, uint32 Offset)
{
	__declspec(align(16)) D3D12_INDEX_BUFFER_VIEW NewView;
	NewView.BufferLocation = (IndexBufferLocation) ? IndexBufferLocation->GetGPUVirtualAddress() + Offset : 0;
	NewView.SizeInBytes = (IndexBufferLocation) ? IndexBufferLocation->GetSize() - Offset : 0;
	NewView.Format = Format;

	D3D12_INDEX_BUFFER_VIEW& CurrentView = PipelineState.Graphics.IBCache.CurrentIndexBufferView;

	if (NewView.BufferLocation != CurrentView.BufferLocation ||
		NewView.SizeInBytes != CurrentView.SizeInBytes ||
		NewView.Format != CurrentView.Format ||
		GD3D12SkipStateCaching)
	{
		bNeedSetIB = true;
		PipelineState.Graphics.IBCache.CurrentIndexBufferLocation = IndexBufferLocation;

		if (IndexBufferLocation != nullptr)
		{
			PipelineState.Graphics.IBCache.ResidencyHandle = IndexBufferLocation->GetResource()->GetResidencyHandle();
			FMemory::Memcpy(CurrentView, NewView);
		}
		else
		{
			FMemory::Memzero(&CurrentView, sizeof(CurrentView));
			PipelineState.Graphics.IBCache.CurrentIndexBufferLocation = nullptr;
			PipelineState.Graphics.IBCache.ResidencyHandle = nullptr;
		}
	}
	if (IndexBufferLocation)
	{
		FD3D12Resource* const pResource = IndexBufferLocation->GetResource();
		if (pResource->RequiresResourceStateTracking())
		{
			check(pResource->GetSubresourceCount() == 1);
			FD3D12DynamicRHI::TransitionResource(CmdContext->CommandListHandle, pResource, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		}
	}

}

void FD3D12StateCacheBase::InternalSetStreamSource(FD3D12ResourceLocation* VertexBufferLocation, uint32 StreamIndex, uint32 Stride, uint32 Offset)
{
	check(StreamIndex < ARRAYSIZE(PipelineState.Graphics.VBCache.CurrentVertexBufferResources));

	__declspec(align(16)) D3D12_VERTEX_BUFFER_VIEW NewView;
	NewView.BufferLocation = (VertexBufferLocation) ? VertexBufferLocation->GetGPUVirtualAddress() + Offset : 0;
	NewView.StrideInBytes = Stride;
	NewView.SizeInBytes = (VertexBufferLocation) ? VertexBufferLocation->GetSize() - Offset : 0; // Make sure we account for how much we offset into the VB

	D3D12_VERTEX_BUFFER_VIEW& CurrentView = PipelineState.Graphics.VBCache.CurrentVertexBufferViews[StreamIndex];

	if (NewView.BufferLocation != CurrentView.BufferLocation ||
		NewView.StrideInBytes != CurrentView.StrideInBytes ||
		NewView.SizeInBytes != CurrentView.SizeInBytes ||
		GD3D12SkipStateCaching)
	{
		bNeedSetVB = true;
		PipelineState.Graphics.VBCache.CurrentVertexBufferResources[StreamIndex] = VertexBufferLocation;

		if (VertexBufferLocation != nullptr)
		{
			PipelineState.Graphics.VBCache.ResidencyHandles[StreamIndex] = VertexBufferLocation->GetResource()->GetResidencyHandle();
			FMemory::Memcpy(CurrentView, NewView);
			PipelineState.Graphics.VBCache.BoundVBMask |= (1 << StreamIndex);
		}
		else
		{
			FMemory::Memzero(&CurrentView, sizeof(CurrentView));
			PipelineState.Graphics.VBCache.CurrentVertexBufferResources[StreamIndex] = nullptr;
			PipelineState.Graphics.VBCache.ResidencyHandles[StreamIndex] = nullptr;

			PipelineState.Graphics.VBCache.BoundVBMask &= ~(1 << StreamIndex);
		}

		if (PipelineState.Graphics.VBCache.BoundVBMask)
		{
			PipelineState.Graphics.VBCache.MaxBoundVertexBufferIndex = FMath::FloorLog2(PipelineState.Graphics.VBCache.BoundVBMask);
		}
		else
		{
			PipelineState.Graphics.VBCache.MaxBoundVertexBufferIndex = INDEX_NONE;
		}
	}

	if (VertexBufferLocation)
	{
		FD3D12Resource* const pResource = VertexBufferLocation->GetResource();
		if (pResource->RequiresResourceStateTracking())
		{
			check(pResource->GetSubresourceCount() == 1);
			FD3D12DynamicRHI::TransitionResource(CmdContext->CommandListHandle, pResource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		}
	}

}

template <EShaderFrequency ShaderFrequency>
void FD3D12StateCacheBase::SetShaderResourceView(FD3D12ShaderResourceView* SRV, uint32 ResourceIndex)
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12SetShaderResourceViewTime);

	check(ResourceIndex < MAX_SRVS);
	FD3D12ShaderResourceViewCache& Cache = PipelineState.Common.SRVCache;
	auto& CurrentShaderResourceViews = Cache.Views[ShaderFrequency];

	if ((CurrentShaderResourceViews[ResourceIndex] != SRV) || GD3D12SkipStateCaching)
	{
		if (SRV != nullptr)
		{
			// Mark the SRVs as not cleared
			bSRVSCleared = false;

			Cache.BoundMask[ShaderFrequency] |= (1 << ResourceIndex);
			Cache.ResidencyHandles[ShaderFrequency][ResourceIndex] = SRV->GetResidencyHandle();
		}
		else
		{
			Cache.BoundMask[ShaderFrequency] &= ~(1 << ResourceIndex);
			Cache.ResidencyHandles[ShaderFrequency][ResourceIndex] = nullptr;
		}

		// Find the highest set SRV
		(Cache.BoundMask[ShaderFrequency] == 0) ? Cache.MaxBoundIndex[ShaderFrequency] = INDEX_NONE :
			Cache.MaxBoundIndex[ShaderFrequency] = FMath::FloorLog2(Cache.BoundMask[ShaderFrequency]);

		CurrentShaderResourceViews[ResourceIndex] = SRV;
		FD3D12ShaderResourceViewCache::DirtySlot(Cache.DirtySlotMask[ShaderFrequency], ResourceIndex);

		if (SRV && SRV->IsDepthStencilResource())
		{
			if (FD3D12DynamicRHI::ResourceViewsIntersect(PipelineState.Graphics.CurrentDepthStencilTarget, SRV))
			{
				const D3D12_DEPTH_STENCIL_VIEW_DESC &DSVDesc = PipelineState.Graphics.CurrentDepthStencilTarget->GetDesc();
				const bool bHasDepth = PipelineState.Graphics.CurrentDepthStencilTarget->HasDepth();
				const bool bHasStencil = PipelineState.Graphics.CurrentDepthStencilTarget->HasStencil();
				const bool bWritableDepth = bHasDepth && (DSVDesc.Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH) == 0;
				const bool bWritableStencil = bHasStencil && (DSVDesc.Flags & D3D12_DSV_FLAG_READ_ONLY_STENCIL) == 0;
				const bool bUnbindDepthStencil = (bWritableDepth && SRV->IsDepthPlaneResource()) || (bWritableStencil && SRV->IsStencilPlaneResource());
				if (!bUnbindDepthStencil)
				{
					// If the DSV isn't writing to the same subresource as the SRV then we can leave the depth stencil bound.
					if (!Cache.ViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex])
					{
						Cache.ViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex] = true;
						Cache.NumViewsIntersectWithDepthCount++;
					}
				}
				else
				{
					// Unbind the DSV because it's being used for depth write
					check(bWritableDepth || bWritableStencil);
					PipelineState.Graphics.CurrentDepthStencilTarget = nullptr;
					PipelineState.Graphics.bNeedRebuildPSO = true;
					bNeedSetRTs = true;
					if (Cache.ViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex])
					{
						Cache.ViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex] = false;
						Cache.NumViewsIntersectWithDepthCount--;
					}
				}
			}
			else
			{
				if (Cache.ViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex])
				{
					Cache.ViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex] = false;
					Cache.NumViewsIntersectWithDepthCount--;
				}
			}
		}
	}
}

void FD3D12StateCacheBase::SetRenderTargets(uint32 NumSimultaneousRenderTargets, FD3D12RenderTargetView** RTArray, FD3D12DepthStencilView* DSTarget)
{
	// Note: We assume that the have been checks to make sure this function is only called when there really are changes being made.
	// We always update the PSO and set descriptors after calling this function.
	PipelineState.Graphics.bNeedRebuildPSO = true;
	bNeedSetRTs = true;

	// Update the depth stencil
	PipelineState.Graphics.CurrentDepthStencilTarget = DSTarget;

	// Update the render targets
	FMemory::Memzero(PipelineState.Graphics.RenderTargetArray, sizeof(PipelineState.Graphics.RenderTargetArray));
	FMemory::Memcpy(PipelineState.Graphics.RenderTargetArray, RTArray, sizeof(FD3D12RenderTargetView*)* NumSimultaneousRenderTargets);

	// In D3D11, the NumSimultaneousRenderTargets count was used even when setting RTV slots to null (to unbind them)
	// In D3D12, we don't do this. So we need change the count to match the non null views used.
	uint32 ActiveNumSimultaneousRenderTargets = 0;
	for (uint32 i = 0; i < NumSimultaneousRenderTargets; i++)
	{
		if (RTArray[i] != nullptr)
		{
			ActiveNumSimultaneousRenderTargets = i + 1;
		}
	}
	PipelineState.Graphics.HighLevelDesc.NumRenderTargets = ActiveNumSimultaneousRenderTargets;
}

void FD3D12StateCacheBase::SetRenderDepthStencilTargetFormats(
	uint32 NumRenderTargets,
	const TRenderTargetFormatsArray& RenderTargetFormats,
	DXGI_FORMAT DepthStencilFormat,
	uint32 NumSamples
)
{
	auto& PsoDesc = PipelineState.Graphics.HighLevelDesc;

	if (FMemory::Memcmp(&PsoDesc.RTVFormats[0], &RenderTargetFormats[0], sizeof(RenderTargetFormats[0]) * D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT))
	{
		PsoDesc.NumRenderTargets = NumRenderTargets;

		uint32 RTIdx = 0;
		for (; RTIdx < PsoDesc.NumRenderTargets; ++RTIdx)
		{
			PsoDesc.RTVFormats[RTIdx] = RenderTargetFormats[RTIdx];
		}

		for (; RTIdx < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++RTIdx)
		{
			PsoDesc.RTVFormats[RTIdx] = DXGI_FORMAT_UNKNOWN;
		}

		PipelineState.Graphics.bNeedRebuildPSO = true;
	}

	if (PsoDesc.DSVFormat != DepthStencilFormat)
	{
		PsoDesc.DSVFormat = DepthStencilFormat;
		PipelineState.Graphics.bNeedRebuildPSO = true;
	}

	if (PsoDesc.SampleDesc.Count != NumSamples || PsoDesc.SampleDesc.Quality != GetMaxMSAAQuality(NumSamples))
	{
		PsoDesc.SampleDesc.Count = NumSamples;
		PsoDesc.SampleDesc.Quality = GetMaxMSAAQuality(NumSamples);
		PipelineState.Graphics.bNeedRebuildPSO = true;
	}
}

FD3D12PipelineState* FD3D12StateCacheBase::CommitPendingGraphicsPipelineState()
{
	FD3D12PipelineStateCache& PSOCache = GetParentDevice()->GetParentAdapter()->GetPSOCache();
	FD3D12PipelineState* const PSO = PSOCache.FindGraphics(&PipelineState.Graphics.HighLevelDesc);
	PipelineState.Graphics.bNeedRebuildPSO = false;

	// Indicate we need to set the PSO on the command list
	PipelineState.Common.bNeedSetPSO = true;

	check(PSO != nullptr);
	return PSO;
}

FD3D12PipelineState* FD3D12StateCacheBase::CommitPendingComputePipelineState()
{
	FD3D12ComputePipelineStateDesc PSODesc;
	FMemory::Memzero(&PSODesc, sizeof(PSODesc));
	PSODesc.pRootSignature = PipelineState.Compute.CurrentComputeShader->pRootSignature;
	PSODesc.Desc.pRootSignature = PSODesc.pRootSignature->GetRootSignature();
	PSODesc.Desc.CS = PipelineState.Compute.CurrentComputeShader->ShaderBytecode.GetShaderBytecode();
	PSODesc.CSHash = PipelineState.Compute.CurrentComputeShader->ShaderBytecode.GetHash();

	FD3D12PipelineStateCache& PSOCache = GetParentDevice()->GetParentAdapter()->GetPSOCache();
	FD3D12PipelineState* const PSO = PSOCache.FindCompute(&PSODesc);
	PipelineState.Compute.bNeedRebuildPSO = false;

	// Indicate we need to set the PSO on the command list
	PipelineState.Common.bNeedSetPSO = true;

	check(PSO != nullptr);
	return PSO;
}

void FD3D12StateCacheBase::SetStreamOutTargets(uint32 NumSimultaneousStreamOutTargets, FD3D12Resource** SOArray, const uint32* SOOffsets)
{
	PipelineState.Graphics.CurrentNumberOfStreamOutTargets = NumSimultaneousStreamOutTargets;
	if (PipelineState.Graphics.CurrentNumberOfStreamOutTargets > 0)
	{
		FMemory::Memcpy(PipelineState.Graphics.CurrentStreamOutTargets, SOArray, sizeof(FD3D12Resource*)* NumSimultaneousStreamOutTargets);
		FMemory::Memcpy(PipelineState.Graphics.CurrentSOOffsets, SOOffsets, sizeof(uint32*)* NumSimultaneousStreamOutTargets);

		bNeedSetSOs = true;
	}
}
