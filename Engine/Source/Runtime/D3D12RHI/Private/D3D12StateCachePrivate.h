// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Implementation of Device Context State Caching to improve draw
//	thread performance by removing redundant device context calls.

#pragma once
#include "Queue.h"
#include "D3D12DirectCommandListManager.h"

//-----------------------------------------------------------------------------
//	Configuration
//-----------------------------------------------------------------------------

// If set, includes a runtime toggle console command for debugging D3D12  state caching.
// ("TOGGLESTATECACHE")
#define D3D12_STATE_CACHE_RUNTIME_TOGGLE 0

// If set, includes a cache state verification check.
// After each state set call, the cached state is compared against the actual state.
// This is *very slow* and should only be enabled to debug the state caching system.
#ifndef D3D12_STATE_CACHE_DEBUG
#define D3D12_STATE_CACHE_DEBUG 0
#endif

// Uncomment only for debugging of the descriptor heap management; this is very noisy
//#define VERBOSE_DESCRIPTOR_HEAP_DEBUG 1

// The number of view descriptors available per (online) descriptor heap, depending on hardware tier
#define NUM_SAMPLER_DESCRIPTORS D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE
#define DESCRIPTOR_HEAP_BLOCK_SIZE 10000

#define NUM_VIEW_DESCRIPTORS_TIER_1 D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1
#define NUM_VIEW_DESCRIPTORS_TIER_2 D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2
// Only some tier 3 hardware can use > 1 million descriptors in a heap, the only way to tell if hardware can
// is to try and create a heap and check for failure. Unless we really want > 1 million Descriptors we'll cap
// out at 1M for now.
#define NUM_VIEW_DESCRIPTORS_TIER_3 D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2

// This value defines how many descriptors will be in the device global view heap which
// is shared across contexts to allow the driver to eliminate redundant descriptor heap sets.
// This should be tweaked for each title as heaps require VRAM. The default value of 512k takes up ~16MB
#if PLATFORM_XBOXONE
  #define GLOBAL_VIEW_HEAP_SIZE ( 500 * 1000 ) // This should be a multiple of DESCRIPTOR_HEAP_BLOCK_SIZE
  #define LOCAL_VIEW_HEAP_SIZE  ( 64 * 1024 )
#else
  #define GLOBAL_VIEW_HEAP_SIZE  ( 500 * 1000 ) // This should be a multiple of DESCRIPTOR_HEAP_BLOCK_SIZE
  #define LOCAL_VIEW_HEAP_SIZE  ( 500 * 1000 )
#endif

// Heap for updating UAV counter values.
#define COUNTER_HEAP_SIZE 1024 * 64

// Keep set state functions inline to reduce call overhead
#define D3D12_STATE_CACHE_INLINE FORCEINLINE

#if D3D12_STATE_CACHE_RUNTIME_TOGGLE
extern bool GD3D12SkipStateCaching;
#else
static const bool GD3D12SkipStateCaching = false;
#endif

struct FD3D12VertexBufferCache
{
	FD3D12VertexBufferCache()
	{
		Clear();
	};

	inline void Clear()
	{
		FMemory::Memzero(CurrentVertexBufferViews, sizeof(CurrentVertexBufferViews));
		FMemory::Memzero(CurrentVertexBufferResources, sizeof(CurrentVertexBufferResources));
		FMemory::Memzero(ResidencyHandles, sizeof(ResidencyHandles));
		MaxBoundVertexBufferIndex = INDEX_NONE;
		BoundVBMask = 0;
	}

	D3D12_VERTEX_BUFFER_VIEW CurrentVertexBufferViews[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	FD3D12ResourceLocation* CurrentVertexBufferResources[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	FD3D12ResidencyHandle* ResidencyHandles[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	int32 MaxBoundVertexBufferIndex;
	uint32 BoundVBMask;
};

struct FD3D12IndexBufferCache
{
	FD3D12IndexBufferCache()
	{
		Clear();
	}

	inline void Clear()
	{
		FMemory::Memzero(&CurrentIndexBufferView, sizeof(CurrentIndexBufferView));
		CurrentIndexBufferLocation = nullptr;
		ResidencyHandle = nullptr;
	}

	D3D12_INDEX_BUFFER_VIEW CurrentIndexBufferView;
	FD3D12ResourceLocation* CurrentIndexBufferLocation;
	FD3D12ResidencyHandle* ResidencyHandle;
};

template<typename ResourceSlotMask>
struct FD3D12ResourceCache
{
	static inline void CleanSlot(ResourceSlotMask& SlotMask, uint32 SlotIndex)
	{
		SlotMask &= ~(1 << SlotIndex);
	}

	static inline void DirtySlot(ResourceSlotMask& SlotMask, uint32 SlotIndex)
	{
		SlotMask |= (1 << SlotIndex);
	}

	static inline bool IsSlotDirty(const ResourceSlotMask& SlotMask, uint32 SlotIndex)
	{
		return (SlotMask & (1 << SlotIndex)) != 0;
	}

	// Mark a specific shader stage as dirty.
	inline void Dirty(EShaderFrequency ShaderFrequency, const ResourceSlotMask& SlotMask = -1)
	{
		DirtySlotMask[ShaderFrequency] |= SlotMask;
	}

	// Mark specified bind slots, on all graphics stages, as dirty.
	inline void DirtyGraphics(const ResourceSlotMask& SlotMask = -1)
	{
		Dirty(SF_Vertex, SlotMask);
		Dirty(SF_Hull, SlotMask);
		Dirty(SF_Domain, SlotMask);
		Dirty(SF_Pixel, SlotMask);
		Dirty(SF_Geometry, SlotMask);
	}

	// Mark specified bind slots on compute as dirty.
	inline void DirtyCompute(const ResourceSlotMask& SlotMask = -1)
	{
		Dirty(SF_Compute, SlotMask);
	}

	// Mark specified bind slots on graphics and compute as dirty.
	inline void DirtyAll(const ResourceSlotMask& SlotMask = -1)
	{
		DirtyGraphics(SlotMask);
		DirtyCompute(SlotMask);
	}

	ResourceSlotMask DirtySlotMask[SF_NumFrequencies];
};

struct FD3D12ConstantBufferCache : public FD3D12ResourceCache<CBVSlotMask>
{
	FD3D12ConstantBufferCache()
	{
		Clear();
	}

	inline void Clear()
	{
		DirtyAll();

		FMemory::Memzero(CurrentGPUVirtualAddress, sizeof(CurrentGPUVirtualAddress));
		FMemory::Memzero(ResidencyHandles, sizeof(ResidencyHandles));
#if USE_STATIC_ROOT_SIGNATURE
		FMemory::Memzero(CBHandles, sizeof(CBHandles));
#endif
	}

#if USE_STATIC_ROOT_SIGNATURE
	D3D12_CPU_DESCRIPTOR_HANDLE CBHandles[SF_NumFrequencies][MAX_CBS];
#endif
	D3D12_GPU_VIRTUAL_ADDRESS CurrentGPUVirtualAddress[SF_NumFrequencies][MAX_CBS];
	FD3D12ResidencyHandle* ResidencyHandles[SF_NumFrequencies][MAX_CBS];
};

struct FD3D12ShaderResourceViewCache : public FD3D12ResourceCache<SRVSlotMask>
{
	FD3D12ShaderResourceViewCache()
	{
		Clear();
	}

	inline void Clear()
	{
		DirtyAll();

		NumViewsIntersectWithDepthCount = 0;
		FMemory::Memzero(ResidencyHandles);
		FMemory::Memzero(ViewsIntersectWithDepthRT);
		FMemory::Memzero(BoundMask);
		
		for (int32& Index : MaxBoundIndex)
		{
			Index = INDEX_NONE;
		}

		for (int32 FrequencyIdx = 0; FrequencyIdx < SF_NumFrequencies; ++FrequencyIdx)
		{
			for (int32 SRVIdx = 0; SRVIdx < MAX_SRVS; ++SRVIdx)
			{
				Views[FrequencyIdx][SRVIdx].SafeRelease();
			}
		}
	}

	TRefCountPtr<FD3D12ShaderResourceView> Views[SF_NumFrequencies][MAX_SRVS];
	FD3D12ResidencyHandle* ResidencyHandles[SF_NumFrequencies][MAX_SRVS];

	bool ViewsIntersectWithDepthRT[SF_NumFrequencies][MAX_SRVS];
	uint32 NumViewsIntersectWithDepthCount;

	uint32 BoundMask[SF_NumFrequencies];
	int32 MaxBoundIndex[SF_NumFrequencies];
};

struct FD3D12UnorderedAccessViewCache : public FD3D12ResourceCache<UAVSlotMask>
{
	FD3D12UnorderedAccessViewCache()
	{
		Clear();
	}

	inline void Clear()
	{
		DirtyAll();

		FMemory::Memzero(Views);
		FMemory::Memzero(ResidencyHandles);

		for (uint32& Index : StartSlot)
		{
			Index = INDEX_NONE;
		}
	}

	FD3D12UnorderedAccessView* Views[SF_NumFrequencies][MAX_UAVS];
	FD3D12ResidencyHandle* ResidencyHandles[SF_NumFrequencies][MAX_UAVS];
	uint32 StartSlot[SF_NumFrequencies];
};

struct FD3D12SamplerStateCache : public FD3D12ResourceCache<SamplerSlotMask>
{
	FD3D12SamplerStateCache()
	{
		Clear();
	}

	inline void Clear()
	{
		DirtyAll();

		FMemory::Memzero(States);
	}

	FD3D12SamplerState* States[SF_NumFrequencies][MAX_SAMPLERS];
};

//-----------------------------------------------------------------------------
//	FD3D12StateCache Class Definition
//-----------------------------------------------------------------------------
class FD3D12StateCacheBase : public FD3D12DeviceChild, public FD3D12SingleNodeGPUObject
{
	friend class FD3D12DynamicRHI;
	// NvFlow begin
	friend class FRHINvFlowStateCacheAccessD3D12;
	// NvFlow end

protected:
	FD3D12CommandContext* CmdContext;

	bool bNeedSetVB;
	bool bNeedSetIB;
	bool bNeedSetRTs;
	bool bNeedSetSOs;
	bool bSRVSCleared;
	bool bNeedSetViewports;
	bool bNeedSetScissorRects;
	bool bNeedSetPrimitiveTopology;
	bool bNeedSetBlendFactor;
	bool bNeedSetStencilRef;
	bool bNeedSetDepthBounds;
	bool bAutoFlushComputeShaderCache;
	D3D12_RESOURCE_BINDING_TIER ResourceBindingTier;

	struct
	{
		struct
		{
			// Cache
			ID3D12PipelineState* CurrentPipelineStateObject;
			bool bNeedRebuildPSO;

			// Note: Current root signature is part of the bound shader state
			bool bNeedSetRootSignature;

			// Full high level PSO desc
			FD3D12HighLevelGraphicsPipelineStateDesc HighLevelDesc;

			// Depth Stencil State Cache
			uint32 CurrentReferenceStencil;

			// Blend State Cache
			float CurrentBlendFactor[4];

			// Viewport
			uint32	CurrentNumberOfViewports;
			D3D12_VIEWPORT CurrentViewport[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];

			// Vertex Buffer State
			FD3D12VertexBufferCache VBCache;

			// SO
			uint32			CurrentNumberOfStreamOutTargets;
			FD3D12Resource* CurrentStreamOutTargets[D3D12_SO_STREAM_COUNT];
			uint32			CurrentSOOffsets[D3D12_SO_STREAM_COUNT];

			// Index Buffer State
			FD3D12IndexBufferCache IBCache;

			// Primitive Topology State
			D3D_PRIMITIVE_TOPOLOGY CurrentPrimitiveTopology;

			// Input Layout State
			D3D12_RECT CurrentScissorRects[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
			D3D12_RECT CurrentViewportScissorRects[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
			uint32 CurrentNumberOfScissorRects;

			uint16 StreamStrides[MaxVertexElementCount];

			FD3D12RenderTargetView* RenderTargetArray[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];

			FD3D12DepthStencilView* CurrentDepthStencilTarget;

			float MinDepth;
			float MaxDepth;
		} Graphics;

		struct
		{
			// Cache
			ID3D12PipelineState* CurrentPipelineStateObject;
			bool bNeedRebuildPSO;

			// Note: Current root signature is part of the bound compute shader
			bool bNeedSetRootSignature;

			// Compute
			FD3D12ComputeShader* CurrentComputeShader;

			// Need to cache compute budget, as we need to reset if after PSO changes
			EAsyncComputeBudget ComputeBudget;
		} Compute;

		struct
		{
			FD3D12ShaderResourceViewCache SRVCache;
			FD3D12ConstantBufferCache CBVCache;
			FD3D12UnorderedAccessViewCache UAVCache;
			FD3D12SamplerStateCache SamplerCache;

			// PSO
			ID3D12PipelineState* CurrentPipelineStateObject;
			bool bNeedSetPSO;

			uint32 CurrentShaderSamplerCounts[SF_NumFrequencies];
			uint32 CurrentShaderSRVCounts[SF_NumFrequencies];
			uint32 CurrentShaderCBCounts[SF_NumFrequencies];
			uint32 CurrentShaderUAVCounts[SF_NumFrequencies];
		} Common;
	} PipelineState;

	FD3D12DescriptorCache DescriptorCache;

	void InternalSetIndexBuffer(FD3D12ResourceLocation *IndexBufferLocation, DXGI_FORMAT Format, uint32 Offset);

	void InternalSetStreamSource(FD3D12ResourceLocation* VertexBufferLocation, uint32 StreamIndex, uint32 Stride, uint32 Offset);

	// Shorthand for typing/reading convenience
	D3D12_STATE_CACHE_INLINE FD3D12BoundShaderState* BSS()
	{
		return PipelineState.Graphics.HighLevelDesc.BoundShaderState;
	}

	template <typename TShader> struct StateCacheShaderTraits;
#define DECLARE_SHADER_TRAITS(Name) \
	template <> struct StateCacheShaderTraits<FD3D12##Name##Shader> \
	{ \
		static const EShaderFrequency Frequency = SF_##Name; \
		static FD3D12##Name##Shader* GetShader(FD3D12BoundShaderState* BSS) { return BSS ? BSS->Get##Name##Shader() : nullptr; } \
	}
	DECLARE_SHADER_TRAITS(Vertex);
	DECLARE_SHADER_TRAITS(Pixel);
	DECLARE_SHADER_TRAITS(Domain);
	DECLARE_SHADER_TRAITS(Hull);
	DECLARE_SHADER_TRAITS(Geometry);
#undef DECLARE_SHADER_TRAITS

	template <typename TShader> D3D12_STATE_CACHE_INLINE void SetShader(TShader* Shader)
	{
		typedef StateCacheShaderTraits<TShader> Traits;
		TShader* OldShader = Traits::GetShader(BSS());
		if (OldShader != Shader)
		{
			PipelineState.Common.CurrentShaderSamplerCounts[Traits::Frequency] = (Shader) ? Shader->ResourceCounts.NumSamplers : 0;
			PipelineState.Common.CurrentShaderSRVCounts[Traits::Frequency]     = (Shader) ? Shader->ResourceCounts.NumSRVs     : 0;
			PipelineState.Common.CurrentShaderCBCounts[Traits::Frequency]      = (Shader) ? Shader->ResourceCounts.NumCBs      : 0;
			PipelineState.Common.CurrentShaderUAVCounts[Traits::Frequency]     = (Shader) ? Shader->ResourceCounts.NumUAVs     : 0;
		
			// Shader changed so its resource table is dirty
			CmdContext->DirtyUniformBuffers[Traits::Frequency] = 0xffff;
		}
	}

	template <typename TShader> D3D12_STATE_CACHE_INLINE void GetShader(TShader** Shader)
	{
		*Shader = StateCacheShaderTraits<TShader>::GetShader(BSS());
	}

public:

	void InheritState(const FD3D12StateCacheBase& AncestralCache)
	{
		FMemory::Memcpy(&PipelineState, &AncestralCache.PipelineState, sizeof(PipelineState));
		DirtyState();
	}

	FD3D12DescriptorCache* GetDescriptorCache()
	{
		return &DescriptorCache;
	}

	ID3D12PipelineState* GetPipelineStateObject()
	{
		return PipelineState.Common.CurrentPipelineStateObject;
	}

	const FD3D12RootSignature* GetGraphicsRootSignature()
	{
		return PipelineState.Graphics.HighLevelDesc.BoundShaderState ?
			PipelineState.Graphics.HighLevelDesc.BoundShaderState->pRootSignature : nullptr;
	}

	const FD3D12RootSignature* GetComputeRootSignature()
	{
		return PipelineState.Compute.CurrentComputeShader ?
			PipelineState.Compute.CurrentComputeShader->pRootSignature : nullptr;
	}

	void ClearSRVs();

	template <EShaderFrequency ShaderFrequency>
	void ClearShaderResourceViews(FD3D12ResourceLocation*& ResourceLocation)
	{
		//SCOPE_CYCLE_COUNTER(STAT_D3D12ClearShaderResourceViewsTime);

		if (PipelineState.Common.SRVCache.MaxBoundIndex[ShaderFrequency] < 0)
		{
			return;
		}

		auto& CurrentShaderResourceViews = PipelineState.Common.SRVCache.Views[ShaderFrequency];
		for (int32 i = 0; i <= PipelineState.Common.SRVCache.MaxBoundIndex[ShaderFrequency]; ++i)
		{
			if (CurrentShaderResourceViews[i] && CurrentShaderResourceViews[i]->GetResourceLocation() == ResourceLocation)
			{
				SetShaderResourceView<ShaderFrequency>(nullptr, i);
			}
		}
	}

	template <EShaderFrequency ShaderFrequency>
	void SetShaderResourceView(FD3D12ShaderResourceView* SRV, uint32 ResourceIndex);

	template <EShaderFrequency ShaderFrequency>
	D3D12_STATE_CACHE_INLINE void GetShaderResourceViews(uint32 StartResourceIndex, uint32& NumResources, FD3D12ShaderResourceView** SRV)
	{
		{
			uint32 NumLoops = D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartResourceIndex;
			NumResources = 0;
			for (uint32 ResourceLoop = 0; ResourceLoop < NumLoops; ResourceLoop++)
			{
				SRV[ResourceLoop] = PipelineState.Common.CurrentShaderResourceViews[ShaderFrequency][ResourceLoop + StartResourceIndex];
				if (SRV[ResourceLoop])
				{
					SRV[ResourceLoop]->AddRef();
					NumResources = ResourceLoop;
				}
			}
		}
	}

	void UpdateViewportScissorRects();
	void SetScissorRects(uint32 Count, const D3D12_RECT* const ScissorRects);
	void SetScissorRect(const D3D12_RECT& ScissorRect);

	D3D12_STATE_CACHE_INLINE void GetScissorRect(D3D12_RECT* ScissorRect) const
	{
		check(ScissorRect);
		FMemory::Memcpy(ScissorRect, &PipelineState.Graphics.CurrentScissorRects, sizeof(D3D12_RECT));
	}

	void SetViewport(const D3D12_VIEWPORT& Viewport);
	void SetViewports(uint32 Count, const D3D12_VIEWPORT* const Viewports);

	D3D12_STATE_CACHE_INLINE uint32 GetNumViewports() const
	{
		return PipelineState.Graphics.CurrentNumberOfViewports;
	}

	D3D12_STATE_CACHE_INLINE void GetViewport(D3D12_VIEWPORT* Viewport) const
	{
		check(Viewport);
		FMemory::Memcpy(Viewport, &PipelineState.Graphics.CurrentViewport, sizeof(D3D12_VIEWPORT));
	}

	D3D12_STATE_CACHE_INLINE void GetViewports(uint32* Count, D3D12_VIEWPORT* Viewports) const
	{
		check(*Count);
		if (Viewports) //NULL is legal if you just want count
		{
			//as per d3d spec
			const int32 StorageSizeCount = (int32)(*Count);
			const int32 CopyCount = FMath::Min(FMath::Min(StorageSizeCount, (int32)PipelineState.Graphics.CurrentNumberOfViewports), D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
			if (CopyCount > 0)
			{
				FMemory::Memcpy(Viewports, &PipelineState.Graphics.CurrentViewport[0], sizeof(D3D12_VIEWPORT) * CopyCount);
			}
			//remaining viewports in supplied array must be set to zero
			if (StorageSizeCount > CopyCount)
			{
				FMemory::Memset(&Viewports[CopyCount], 0, sizeof(D3D12_VIEWPORT) * (StorageSizeCount - CopyCount));
			}
		}
		*Count = PipelineState.Graphics.CurrentNumberOfViewports;
	}

	template <EShaderFrequency ShaderFrequency>
	D3D12_STATE_CACHE_INLINE void SetSamplerState(FD3D12SamplerState* SamplerState, uint32 SamplerIndex)
	{
		check(SamplerIndex < MAX_SAMPLERS);
		auto& Samplers = PipelineState.Common.SamplerCache.States[ShaderFrequency];
		if ((Samplers[SamplerIndex] != SamplerState) || GD3D12SkipStateCaching)
		{
			Samplers[SamplerIndex] = SamplerState;
			FD3D12SamplerStateCache::DirtySlot(PipelineState.Common.SamplerCache.DirtySlotMask[ShaderFrequency], SamplerIndex);
		}
	}

	template <EShaderFrequency ShaderFrequency>
	D3D12_STATE_CACHE_INLINE void GetSamplerState(uint32 StartSamplerIndex, uint32 NumSamplerIndexes, FD3D12SamplerState** SamplerStates) const
	{
		check(StartSamplerIndex + NumSamplerIndexes <= D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);
		for (uint32 StateLoop = 0; StateLoop < NumSamplerIndexes; StateLoop++)
		{
			SamplerStates[StateLoop] = CurrentShaderResourceViews[ShaderFrequency][StateLoop + StartSamplerIndex];
			if (SamplerStates[StateLoop])
			{
				SamplerStates[StateLoop]->AddRef();
			}
		}
	}

	template <EShaderFrequency ShaderFrequency>
	void D3D12_STATE_CACHE_INLINE SetConstantsFromUniformBuffer(uint32 SlotIndex, FD3D12UniformBuffer* UniformBuffer)
	{
		check(SlotIndex < MAX_CBS);
		FD3D12ConstantBufferCache& CBVCache = PipelineState.Common.CBVCache;
		D3D12_GPU_VIRTUAL_ADDRESS& CurrentGPUVirtualAddress = CBVCache.CurrentGPUVirtualAddress[ShaderFrequency][SlotIndex];

		if (UniformBuffer && UniformBuffer->ResourceLocation.GetGPUVirtualAddress())
		{
			const FD3D12ResourceLocation& ResourceLocation = UniformBuffer->ResourceLocation;
			// Only update the constant buffer if it has changed.
			if (ResourceLocation.GetGPUVirtualAddress() != CurrentGPUVirtualAddress)
			{
				CurrentGPUVirtualAddress = ResourceLocation.GetGPUVirtualAddress();
				CBVCache.ResidencyHandles[ShaderFrequency][SlotIndex] = ResourceLocation.GetResource()->GetResidencyHandle();
				FD3D12ConstantBufferCache::DirtySlot(CBVCache.DirtySlotMask[ShaderFrequency], SlotIndex);
			}

#if USE_STATIC_ROOT_SIGNATURE
			CBVCache.CBHandles[ShaderFrequency][SlotIndex] = UniformBuffer->View->OfflineDescriptorHandle;
#endif
		}
		else if (CurrentGPUVirtualAddress != 0)
		{
			CurrentGPUVirtualAddress = 0;
			CBVCache.ResidencyHandles[ShaderFrequency][SlotIndex] = nullptr;
			FD3D12ConstantBufferCache::DirtySlot(CBVCache.DirtySlotMask[ShaderFrequency], SlotIndex);
#if USE_STATIC_ROOT_SIGNATURE
			CBVCache.CBHandles[ShaderFrequency][SlotIndex].ptr = 0;
#endif
		}
		else
		{
#if USE_STATIC_ROOT_SIGNATURE
			CBVCache.CBHandles[ShaderFrequency][SlotIndex].ptr = 0;
#endif
		}
	}

	template <EShaderFrequency ShaderFrequency>
	void D3D12_STATE_CACHE_INLINE SetConstantBuffer(FD3D12ConstantBuffer& Buffer, bool bDiscardSharedConstants)
	{
		FD3D12ResourceLocation Location(GetParentDevice());

		if (Buffer.Version(Location, bDiscardSharedConstants))
		{
			// Note: Code assumes the slot index is always 0.
			const uint32 SlotIndex = 0;

			FD3D12ConstantBufferCache& CBVCache = PipelineState.Common.CBVCache;
			D3D12_GPU_VIRTUAL_ADDRESS& CurrentGPUVirtualAddress = CBVCache.CurrentGPUVirtualAddress[ShaderFrequency][SlotIndex];
			check(Location.GetGPUVirtualAddress() != CurrentGPUVirtualAddress);
			CurrentGPUVirtualAddress = Location.GetGPUVirtualAddress();
			CBVCache.ResidencyHandles[ShaderFrequency][SlotIndex] = Location.GetResource()->GetResidencyHandle();
			FD3D12ConstantBufferCache::DirtySlot(CBVCache.DirtySlotMask[ShaderFrequency], SlotIndex);

#if USE_STATIC_ROOT_SIGNATURE
			CBVCache.CBHandles[ShaderFrequency][SlotIndex] = Buffer.View->OfflineDescriptorHandle;
#endif
		}
	}

	D3D12_STATE_CACHE_INLINE void SetRasterizerState(D3D12_RASTERIZER_DESC* State)
	{
		if (PipelineState.Graphics.HighLevelDesc.RasterizerState != State || GD3D12SkipStateCaching)
		{
			PipelineState.Graphics.HighLevelDesc.RasterizerState = State;
			PipelineState.Graphics.bNeedRebuildPSO = true;
		}
	}

	D3D12_STATE_CACHE_INLINE void GetRasterizerState(D3D12_RASTERIZER_DESC** RasterizerState) const
	{
		*RasterizerState = PipelineState.Graphics.HighLevelDesc.RasterizerState;
	}

	void SetBlendState(D3D12_BLEND_DESC* State, const float BlendFactor[4], uint32 SampleMask);

	D3D12_STATE_CACHE_INLINE void GetBlendState(D3D12_BLEND_DESC** BlendState, float BlendFactor[4], uint32* SampleMask) const
	{
		*BlendState = PipelineState.Graphics.HighLevelDesc.BlendState;
		*SampleMask = PipelineState.Graphics.HighLevelDesc.SampleMask;
		FMemory::Memcpy(BlendFactor, PipelineState.Graphics.CurrentBlendFactor, sizeof(PipelineState.Graphics.CurrentBlendFactor));
	}

	void SetBlendFactor(const float BlendFactor[4]);
	const float* GetBlendFactor() const { return PipelineState.Graphics.CurrentBlendFactor; }
	
	void SetDepthStencilState(D3D12_DEPTH_STENCIL_DESC* State, uint32 RefStencil);

	D3D12_STATE_CACHE_INLINE void GetDepthStencilState(D3D12_DEPTH_STENCIL_DESC** DepthStencilState, uint32* StencilRef) const
	{
		*DepthStencilState = PipelineState.Graphics.HighLevelDesc.DepthStencilState;
		*StencilRef = PipelineState.Graphics.CurrentReferenceStencil;
	}

	void SetStencilRef(uint32 StencilRef);
	uint32 GetStencilRef() const { return PipelineState.Graphics.CurrentReferenceStencil; }

	D3D12_STATE_CACHE_INLINE void GetVertexShader(FD3D12VertexShader** Shader)
	{
		GetShader(Shader);
	}

	D3D12_STATE_CACHE_INLINE void GetHullShader(FD3D12HullShader** Shader)
	{
		GetShader(Shader);
	}

	D3D12_STATE_CACHE_INLINE void GetDomainShader(FD3D12DomainShader** Shader)
	{
		GetShader(Shader);
	}

	D3D12_STATE_CACHE_INLINE void GetGeometryShader(FD3D12GeometryShader** Shader)
	{
		GetShader(Shader);
	}

	D3D12_STATE_CACHE_INLINE void GetPixelShader(FD3D12PixelShader** Shader)
	{
		GetShader(Shader);
	}

	D3D12_STATE_CACHE_INLINE void SetBoundShaderState(FD3D12BoundShaderState* BoundShaderState)
	{
		if (BoundShaderState)
		{
			SetStreamStrides(BoundShaderState->StreamStrides);
			SetShader(BoundShaderState->GetVertexShader());
			SetShader(BoundShaderState->GetPixelShader());
			SetShader(BoundShaderState->GetDomainShader());
			SetShader(BoundShaderState->GetHullShader());
			SetShader(BoundShaderState->GetGeometryShader());
		}
		else
		{
			uint16 NullStrides[MaxVertexElementCount] = {0};
			SetStreamStrides(NullStrides);
			SetShader<FD3D12VertexShader>(nullptr);
			SetShader<FD3D12PixelShader>(nullptr);
			SetShader<FD3D12HullShader>(nullptr);
			SetShader<FD3D12DomainShader>(nullptr);
			SetShader<FD3D12GeometryShader>(nullptr);
		}

		FD3D12BoundShaderState*& CurrentBSS = PipelineState.Graphics.HighLevelDesc.BoundShaderState;
		if (CurrentBSS != BoundShaderState)
		{
			const FD3D12RootSignature* const pCurrentRootSignature = CurrentBSS ? CurrentBSS->pRootSignature : nullptr;
			const FD3D12RootSignature* const pNewRootSignature = BoundShaderState ? BoundShaderState->pRootSignature : nullptr;
			if (pCurrentRootSignature != pNewRootSignature)
			{
				PipelineState.Graphics.bNeedSetRootSignature = true;
			}

			CurrentBSS = BoundShaderState;
			PipelineState.Graphics.bNeedRebuildPSO = true;
		}
	}

	D3D12_STATE_CACHE_INLINE void GetBoundShaderState(FD3D12BoundShaderState** BoundShaderState) const
	{
		*BoundShaderState = PipelineState.Graphics.HighLevelDesc.BoundShaderState;
	}

	template <bool IsCompute = false>
	D3D12_STATE_CACHE_INLINE void SetPipelineState(FD3D12PipelineState* PSO)
	{
		// Save the PSO
		if (PSO)
		{
			if (IsCompute)
			{
				PipelineState.Compute.CurrentPipelineStateObject = PSO->GetPipelineState();
				check(!PipelineState.Compute.bNeedRebuildPSO);
			}
			else
			{
				PipelineState.Graphics.CurrentPipelineStateObject = PSO->GetPipelineState();
				check(!PipelineState.Graphics.bNeedRebuildPSO);
			}
		}

		// See if we need to set our PSO:
		// In D3D11, you could Set dispatch arguments, then set Draw arguments, then call Draw/Dispatch/Draw/Dispatch without setting arguments again.
		// In D3D12, we need to understand when the app switches between Draw/Dispatch and make sure the correct PSO is set.
		bool bNeedSetPSO = PipelineState.Common.bNeedSetPSO;
		auto& CurrentPSO = PipelineState.Common.CurrentPipelineStateObject;
		auto& RequiredPSO = IsCompute ? PipelineState.Compute.CurrentPipelineStateObject : PipelineState.Graphics.CurrentPipelineStateObject;
		if (CurrentPSO != RequiredPSO)
		{
			CurrentPSO = RequiredPSO;
			bNeedSetPSO = true;
		}

		// Set the PSO on the command list if necessary.
		if (bNeedSetPSO)
		{
			CmdContext->CommandListHandle->SetPipelineState(CurrentPSO);
			PipelineState.Common.bNeedSetPSO = false;
		}
	}

	void SetComputeShader(FD3D12ComputeShader* Shader);

	D3D12_STATE_CACHE_INLINE void GetComputeShader(FD3D12ComputeShader** ComputeShader)
	{
		*ComputeShader = PipelineState.Compute.CurrentComputeShader;
	}

	D3D12_STATE_CACHE_INLINE void GetInputLayout(D3D12_INPUT_LAYOUT_DESC* InputLayout) const
	{
		*InputLayout = PipelineState.Graphics.HighLevelDesc.BoundShaderState->InputLayout;
	}

	D3D12_STATE_CACHE_INLINE void SetStreamStrides(const uint16* InStreamStrides)
	{
		FMemory::Memcpy(PipelineState.Graphics.StreamStrides, InStreamStrides, sizeof(PipelineState.Graphics.StreamStrides));
	}

	D3D12_STATE_CACHE_INLINE void SetStreamSource(FD3D12ResourceLocation* VertexBufferLocation, uint32 StreamIndex, uint32 Stride, uint32 Offset)
	{
		ensure(Stride == PipelineState.Graphics.StreamStrides[StreamIndex]);
		InternalSetStreamSource(VertexBufferLocation, StreamIndex, Stride, Offset);
	}

	D3D12_STATE_CACHE_INLINE void SetStreamSource(FD3D12ResourceLocation* VertexBufferLocation, uint32 StreamIndex, uint32 Offset)
	{
		InternalSetStreamSource(VertexBufferLocation, StreamIndex, PipelineState.Graphics.StreamStrides[StreamIndex], Offset);
	}

	D3D12_STATE_CACHE_INLINE bool IsShaderResource(const FD3D12ResourceLocation* VertexBufferLocation) const
	{
		for (int i = 0; i < SF_NumFrequencies; i++)
		{
			if (PipelineState.Common.SRVCache.MaxBoundIndex[i] < 0)
			{
				continue;
			}

			for (int32 j = 0; j < PipelineState.Common.SRVCache.MaxBoundIndex[i]; ++j)
			{
				if (PipelineState.Common.SRVCache.Views[i][j] && PipelineState.Common.SRVCache.Views[i][j]->GetResourceLocation())
				{
					if (PipelineState.Common.SRVCache.Views[i][j]->GetResourceLocation() == VertexBufferLocation)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	D3D12_STATE_CACHE_INLINE bool IsStreamSource(const FD3D12ResourceLocation* VertexBufferLocation) const
	{
		for (int32 index = 0; index <= PipelineState.Graphics.VBCache.MaxBoundVertexBufferIndex; ++index)
		{
			if (PipelineState.Graphics.VBCache.CurrentVertexBufferResources[index] == VertexBufferLocation)
			{
				return true;
			}
		}

		return false;
	}

public:

	D3D12_STATE_CACHE_INLINE void SetIndexBuffer(FD3D12ResourceLocation* IndexBufferLocation, DXGI_FORMAT Format, uint32 Offset)
	{
		InternalSetIndexBuffer(IndexBufferLocation, Format, Offset);
	}

	D3D12_STATE_CACHE_INLINE bool IsIndexBuffer(const FD3D12ResourceLocation *ResourceLocation) const
	{
		return PipelineState.Graphics.IBCache.CurrentIndexBufferLocation == ResourceLocation;
	}

	void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType);
	void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology);
	
	D3D12_STATE_CACHE_INLINE void GetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY* PrimitiveTopology) const
	{
		*PrimitiveTopology = PipelineState.Graphics.CurrentPrimitiveTopology;
	}

	FD3D12StateCacheBase(GPUNodeMask Node);

	void Init(FD3D12Device* InParent, FD3D12CommandContext* InCmdContext, const FD3D12StateCacheBase* AncestralState, FD3D12SubAllocatedOnlineHeap::SubAllocationDesc& SubHeapDesc);

	virtual ~FD3D12StateCacheBase()
	{
	}

	template <bool IsCompute = false> 
	void ApplyState();
	void ApplySamplers(const FD3D12RootSignature* const pRootSignature, uint32 StartStage, uint32 EndStage);
	void DirtyState();
	void DirtyViewDescriptorTables();
	void DirtySamplerDescriptorTables();
	bool AssertResourceStates(const bool IsCompute);

	void SetRenderTargets(uint32 NumSimultaneousRenderTargets, FD3D12RenderTargetView** RTArray, FD3D12DepthStencilView* DSTarget);
	D3D12_STATE_CACHE_INLINE void GetRenderTargets(FD3D12RenderTargetView **RTArray, uint32* NumSimultaneousRTs, FD3D12DepthStencilView** DepthStencilTarget)
	{
		if (RTArray) //NULL is legal
		{
			FMemory::Memcpy(RTArray, PipelineState.Graphics.RenderTargetArray, sizeof(FD3D12RenderTargetView*)* D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
			*NumSimultaneousRTs = PipelineState.Graphics.HighLevelDesc.NumRenderTargets;
		}

		if (DepthStencilTarget)
		{
			*DepthStencilTarget = PipelineState.Graphics.CurrentDepthStencilTarget;
		}
	}

	void SetRenderDepthStencilTargetFormats(
		uint32 NumRenderTargets,
		const TRenderTargetFormatsArray& RenderTargetFormats,
		DXGI_FORMAT DepthStencilFormat,
		uint32 NumSamples
		);

	FD3D12PipelineState* CommitPendingGraphicsPipelineState();
	FD3D12PipelineState* CommitPendingComputePipelineState();

	void SetStreamOutTargets(uint32 NumSimultaneousStreamOutTargets, FD3D12Resource** SOArray, const uint32* SOOffsets);

	template <EShaderFrequency ShaderStage>
	void SetUAVs(uint32 UAVStartSlot, uint32 NumSimultaneousUAVs, FD3D12UnorderedAccessView** UAVArray, uint32* UAVInitialCountArray);

	void SetDepthBounds(float MinDepth, float MaxDepth)
	{
		if (PipelineState.Graphics.MinDepth != MinDepth || PipelineState.Graphics.MaxDepth != MaxDepth)
		{
			PipelineState.Graphics.MinDepth = MinDepth;
			PipelineState.Graphics.MaxDepth = MaxDepth;

			bNeedSetDepthBounds = true;
		}
	}

	void SetComputeBudget(EAsyncComputeBudget ComputeBudget)
	{
		PipelineState.Compute.ComputeBudget = ComputeBudget;
	}

	D3D12_STATE_CACHE_INLINE void AutoFlushComputeShaderCache(bool bEnable)
	{
		bAutoFlushComputeShaderCache = bEnable;
	}

	void FlushComputeShaderCache(bool bForce = false);

	/**
	 * Clears all D3D12 State, setting all input/output resource slots, shaders, input layouts,
	 * predications, scissor rectangles, depth-stencil state, rasterizer state, blend state,
	 * sampler state, and viewports to NULL
	 */
	virtual void ClearState();

	/**
	 * Releases any object references held by the state cache
	 */
	void Clear();

	void ForceRebuildGraphicsPSO() { PipelineState.Graphics.bNeedRebuildPSO = true; }
	void ForceRebuildComputePSO() { PipelineState.Compute.bNeedRebuildPSO = true; }
	void ForceSetGraphicsRootSignature() { PipelineState.Graphics.bNeedSetRootSignature = true; }
	void ForceSetComputeRootSignature() { PipelineState.Compute.bNeedSetRootSignature = true; }
	void ForceSetVB() { bNeedSetVB = true; }
	void ForceSetIB() { bNeedSetIB = true; }
	void ForceSetRTs() { bNeedSetRTs = true; }
	void ForceSetSOs() { bNeedSetSOs = true; }
	void ForceSetSamplersPerShaderStage(uint32 Frequency) { PipelineState.Common.SamplerCache.Dirty((EShaderFrequency)Frequency); }
	void ForceSetSRVsPerShaderStage(uint32 Frequency) { PipelineState.Common.SRVCache.Dirty((EShaderFrequency)Frequency); }
	void ForceSetViewports() { bNeedSetViewports = true; }
	void ForceSetScissorRects() { bNeedSetScissorRects = true; }
	void ForceSetPrimitiveTopology() { bNeedSetPrimitiveTopology = true; }
	void ForceSetBlendFactor() { bNeedSetBlendFactor = true; }
	void ForceSetStencilRef() { bNeedSetStencilRef = true; }

	bool GetForceRebuildGraphicsPSO() const { return PipelineState.Graphics.bNeedRebuildPSO; }
	bool GetForceRebuildComputePSO() const { return PipelineState.Compute.bNeedRebuildPSO; }
	bool GetForceSetVB() const { return bNeedSetVB; }
	bool GetForceSetIB() const { return bNeedSetIB; }
	bool GetForceSetRTs() const { return bNeedSetRTs; }
	bool GetForceSetSOs() const { return bNeedSetSOs; }
	bool GetForceSetSamplersPerShaderStage(uint32 Frequency) const { return PipelineState.Common.SamplerCache.DirtySlotMask[Frequency] != 0; }
	bool GetForceSetSRVsPerShaderStage(uint32 Frequency) const { return PipelineState.Common.SRVCache.DirtySlotMask[Frequency] != 0; }
	bool GetForceSetViewports() const { return bNeedSetViewports; }
	bool GetForceSetScissorRects() const { return bNeedSetScissorRects; }
	bool GetForceSetPrimitiveTopology() const { return bNeedSetPrimitiveTopology; }
	bool GetForceSetBlendFactor() const { return bNeedSetBlendFactor; }
	bool GetForceSetStencilRef() const { return bNeedSetStencilRef; }


#if D3D12_STATE_CACHE_DEBUG
protected:
	// Debug helper methods to verify cached state integrity.
	template <EShaderFrequency ShaderFrequency>
	void VerifySamplerStates();

	template <EShaderFrequency ShaderFrequency>
	void VerifyConstantBuffers();

	template <EShaderFrequency ShaderFrequency>
	void VerifyShaderResourceViews();
#endif
};
