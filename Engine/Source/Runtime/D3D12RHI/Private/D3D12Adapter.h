// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Adapter.h: D3D12 Adapter Interfaces

The D3D12 RHI is layed out in the following stucture. 

	[Engine]--
			|
			|-[RHI]--
					|
					|-[Adapter]-- (LDA)
					|			|
					|			|- [Device]
					|			|
					|			|- [Device]
					|
					|-[Adapter]--
								|
								|- [Device]--
											|
											|-[CommandContext]
											|
											|-[CommandContext]---
																|
																|-[StateCache]

Under this scheme an FD3D12Device represents 1 node belonging to 1 physical adapter.

This structure allows a single RHI to control several different hardware setups. Some example arrangements:
	- Single-GPU systems (the common case)
	- Multi-GPU systems i.e. LDA (Crossfire/SLI)
	- Asymmetric Multi-GPU systems i.e. Discrete/Integrated GPU cooperation
=============================================================================*/

#pragma once

class FD3D12DynamicRHI;

enum EMultiGPUMode
{
	MGPU_Disabled,
	MGPU_AFR
};

static const bool GRedirectDefaultContextForAFR = true;

/// @cond DOXYGEN_WARNINGS

void* FD3D12ThreadLocalObject<FD3D12FastConstantAllocator>::ThisThreadObject = nullptr;

/// @endcond

struct FD3D12AdapterDesc
{
	FD3D12AdapterDesc()
		: AdapterIndex(-1)
		, MaxSupportedFeatureLevel((D3D_FEATURE_LEVEL)0)
		, NumDeviceNodes(0)
	{
	}

	FD3D12AdapterDesc(DXGI_ADAPTER_DESC& DescIn , int32 InAdapterIndex, D3D_FEATURE_LEVEL InMaxSupportedFeatureLevel, uint32 NumNodes)
		: AdapterIndex(InAdapterIndex)
		, MaxSupportedFeatureLevel(InMaxSupportedFeatureLevel)
		, Desc(DescIn)
		, NumDeviceNodes(NumNodes)
	{
	}

	bool IsValid() const { return MaxSupportedFeatureLevel != (D3D_FEATURE_LEVEL)0 && AdapterIndex >= 0; }

	/** -1 if not supported or FindAdpater() wasn't called. Ideally we would store a pointer to IDXGIAdapter but it's unlikely the adpaters change during engine init. */
	int32 AdapterIndex;
	/** The maximum D3D12 feature level supported. 0 if not supported or FindAdpater() wasn't called */
	D3D_FEATURE_LEVEL MaxSupportedFeatureLevel;

	DXGI_ADAPTER_DESC Desc;

	uint32 NumDeviceNodes;
};

// Represents a set of linked D3D12 device nodes (LDA i.e 1 or more identical GPUs). In most cases there will be only 1 node, however if the system supports
// SLI/Crossfire and the app enables it an Adapter will have 2 or more nodes. This class will own anything that can be shared
// across LDA including: System Pool Memory,.Pipeline State Objects, Root Signatures etc.
class FD3D12Adapter : public FNoncopyable
{
public:

	FD3D12Adapter(FD3D12AdapterDesc& DescIn);
	virtual ~FD3D12Adapter() { }

	void Initialize(FD3D12DynamicRHI* RHI);
	void InitializeDevices();

	// Getters
	inline const uint32 GetAdapterIndex() const { return Desc.AdapterIndex; }
	inline const D3D_FEATURE_LEVEL GetFeatureLevel() const { return Desc.MaxSupportedFeatureLevel; }
	inline ID3D12Device* GetD3DDevice() { return RootDevice.GetReference(); }
	inline ID3D12Device1* GetD3DDevice1() { return RootDevice1.GetReference(); }
	inline void SetDeviceRemoved(bool value) { bDeviceRemoved = value; }
	inline const bool IsDeviceRemoved() const { return bDeviceRemoved; }
	inline FD3D12DynamicRHI* GetOwningRHI() { return OwningRHI; }
	inline const D3D12_RESOURCE_HEAP_TIER GetResourceHeapTier() const { return ResourceHeapTier; }
	inline const D3D12_RESOURCE_BINDING_TIER GetResourceBindingTier() const { return ResourceBindingTier; }
	inline const D3D_ROOT_SIGNATURE_VERSION GetRootSignatureVersion() const { return RootSignatureVersion; }
	inline const DXGI_ADAPTER_DESC& GetD3DAdapterDesc() const { return Desc.Desc; }
	inline IDXGIAdapter* GetAdapter() { return DxgiAdapter; }
	inline const FD3D12AdapterDesc& GetDesc() const { return Desc; }
	inline TArray<FD3D12Viewport*>& GetViewports() { return Viewports; }
	inline FD3D12Viewport* GetDrawingViewport() { return DrawingViewport; }
	inline void SetDrawingViewport(FD3D12Viewport* InViewport) { DrawingViewport = InViewport; }

	inline ID3D12CommandSignature* GetDrawIndirectCommandSignature() { return DrawIndirectCommandSignature; }
	inline ID3D12CommandSignature* GetDrawIndexedIndirectCommandSignature() { return DrawIndexedIndirectCommandSignature; }
	inline ID3D12CommandSignature* GetDispatchIndirectCommandSignature() { return DispatchIndirectCommandSignature; }

	inline FD3D12PipelineStateCache& GetPSOCache() { return PipelineStateCache; }

	inline FD3D12FenceCorePool& GetFenceCorePool() { return FenceCorePool; }

#if USE_STATIC_ROOT_SIGNATURE
	inline const FD3D12RootSignature* GetStaticGraphicsRootSignature()
	{
		static const FD3D12RootSignature StaticGraphicsRootSignature(this, FD3D12RootSignatureDesc::GetStaticGraphicsRootSignatureDesc());
		return &StaticGraphicsRootSignature;
	}
	inline const FD3D12RootSignature* GetStaticComputeRootSignature()
	{
		static const FD3D12RootSignature StaticComputeRootSignature(this, FD3D12RootSignatureDesc::GetStaticComputeRootSignatureDesc());
		return &StaticComputeRootSignature;
	}
#else
	inline const FD3D12RootSignature* GetStaticGraphicsRootSignature(){ return nullptr; }
	inline const FD3D12RootSignature* GetStaticComputeRootSignature() { return nullptr; }

	inline FD3D12RootSignature* GetRootSignature(const FD3D12QuantizedBoundShaderState& QBSS) 
	{
		return RootSignatureManager.GetRootSignature(QBSS);
	}
#endif
	inline FD3D12RootSignatureManager* GetRootSignatureManager()
	{
		return &RootSignatureManager;
	}

	inline FD3D12DeferredDeletionQueue& GetDeferredDeletionQueue() { return DeferredDeletionQueue; }

	inline const GPUNodeMask ActiveGPUMask() const { return ActiveGPUNodes; }
	inline const uint32 GetNumGPUNodes() const { return (!GIsEditor && GEnableMGPU) ? Desc.NumDeviceNodes : 1; }
	inline const bool AlternateFrameRenderingEnabled() const { return MultiGPUMode == MGPU_AFR; }
	
	inline FD3D12ManualFence& GetFrameFence()  { return FrameFence; }

	void SwitchToNextGPU();

	inline FD3D12Device* GetDevice(GPUNodeMask Node = GDefaultGPUMask)
	{
		return Devices[FD3D12SingleNodeGPUObject::DetermineGPUIndex(Node)];
	}

	inline FD3D12Device* GetCurrentDevice()
	{
		return GetDevice(CurrentGPUNode);
	}

	inline const GPUNodeMask GetCurrentNodeMask() const { return CurrentGPUNode; }
	inline const GPUNodeMask GetPreviousNodeMask() const 
	{
		GPUNodeMask Node = CurrentGPUNode >> 1;
		return  (Node == 0) ? (1 << (GetNumGPUNodes() - 1)) : Node;
	}

	inline FD3D12Device* GetDeviceByIndex(uint32 Index)
	{
		check(Index < GetNumGPUNodes());
		return Devices[Index];
	}

	inline const EMultiGPUMode GetMultiGPUMode() const { return MultiGPUMode; }
	inline void SetAFRMode() { MultiGPUMode = MGPU_AFR; }

	inline void CreateDXGIFactory()
	{
#if PLATFORM_WINDOWS
		VERIFYD3D12RESULT(::CreateDXGIFactory(IID_PPV_ARGS(DxgiFactory.GetInitReference())));
		VERIFYD3D12RESULT(DxgiFactory->QueryInterface(IID_PPV_ARGS(DxgiFactory2.GetInitReference())));
#endif
	}
	inline IDXGIFactory* GetDXGIFactory() const { return DxgiFactory; }
	inline IDXGIFactory2* GetDXGIFactory2() const { return DxgiFactory2; }

	inline FD3D12DynamicHeapAllocator& GetUploadHeapAllocator() { return *UploadHeapAllocator; }

	inline FD3DGPUProfiler& GetGPUProfiler() { return GPUProfilingData; }

	inline uint32 GetDebugFlags() const { return DebugFlags; }

	void Cleanup();

	void EndFrame();

	// Resource Creation
	HRESULT CreateCommittedResource(const D3D12_RESOURCE_DESC& Desc,
		const D3D12_HEAP_PROPERTIES& HeapProps,
		const D3D12_RESOURCE_STATES& InitialUsage,
		const D3D12_CLEAR_VALUE* ClearValue,
		FD3D12Resource** ppOutResource);

	HRESULT CreatePlacedResourceWithHeap(const D3D12_RESOURCE_DESC& Desc,
		const D3D12_HEAP_PROPERTIES& HeapProps,
		const D3D12_RESOURCE_STATES& InitialUsage,
		const D3D12_CLEAR_VALUE* ClearValue,
		FD3D12Resource** ppOutResource);

	HRESULT CreatePlacedResource(const D3D12_RESOURCE_DESC& Desc,
		FD3D12Heap* BackingHeap,
		uint64 HeapOffset,
		const D3D12_RESOURCE_STATES& InitialUsage,
		const D3D12_CLEAR_VALUE* ClearValue,
		FD3D12Resource** ppOutResource);

	HRESULT CreateBuffer(D3D12_HEAP_TYPE HeapType,
		GPUNodeMask CreationNode,
		GPUNodeMask VisibleNodes,
		uint64 HeapSize,
		FD3D12Resource** ppOutResource,
		D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE);

	HRESULT CreateBuffer(const D3D12_HEAP_PROPERTIES& HeapProps,
		uint64 HeapSize,
		FD3D12Resource** ppOutResource,
		D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE);

	template <typename BufferType> 
	BufferType* CreateRHIBuffer(FRHICommandListImmediate* RHICmdList,
		const D3D12_RESOURCE_DESC& Desc,
		uint32 Alignment, uint32 Stride, uint32 Size, uint32 InUsage,
		FRHIResourceCreateInfo& CreateInfo,
		bool SkipCreate);

	// Queue up a command to signal the frame fence on the command list. This should only be called from the rendering thread.
	void SignalFrameFence_RenderThread(FRHICommandListImmediate& RHICmdList);

	template<typename ObjectType, typename CreationCoreFunction>
	inline ObjectType* CreateLinkedObject(const CreationCoreFunction& pfnCreationCore)
	{
		// TODO: this should be a value specified by the Engine to tell which GPU node to create on
		const uint32 NumGPUs = GetNumGPUNodes();

		ObjectType* ObjectOut = nullptr;
		ObjectType* Previous = nullptr;

		for (size_t i = 0; i < NumGPUs; i++)
		{
			ObjectType* NewObject = pfnCreationCore(GetDevice(1 << i));

			// For AFR link up the resources so they can be implicitly destroyed
			if (i > 0)
			{
				Previous->SetNextObject(NewObject);
			}
			else
			{
				ObjectOut = NewObject;
			}

			Previous = NewObject;
		}

		return ObjectOut;
	}

	template<typename ResourceType, typename ViewType, typename CreationCoreFunction>
	inline ViewType* CreateLinkedViews(ResourceType* Resource, const CreationCoreFunction& pfnCreationCore)
	{
		ViewType* ViewOut = nullptr;
		ViewType* Previous = nullptr;

		uint32 i = 0;
		while (Resource)
		{
			ViewType* NewView = pfnCreationCore(Resource);
			// For AFR link up the resources so they can be implicitly destroyed
			if (i > 0)
			{
				Previous->SetNextObject(NewView);
			}
			else
			{
				ViewOut = NewView;
			}
			i++;
			Previous = NewView;
			Resource = (ResourceType*)Resource->GetNextObject();
		}

		return ViewOut;
	}

	inline FD3D12CommandContextRedirector& GetDefaultContextRedirector() { return DefaultContextRedirector; }
	inline FD3D12CommandContextRedirector& GetDefaultAsyncComputeContextRedirector() { return DefaultAsyncComputeContextRedirector; }

	FD3D12TemporalEffect* GetTemporalEffect(const FName& EffectName);

	FD3D12FastConstantAllocator& GetTransientUniformBufferAllocator() 
	{
		return *TransientUniformBufferAllocator.GetObjectForThisThread([this]() -> FD3D12FastConstantAllocator*
		{
			FD3D12FastConstantAllocator* Alloc = new FD3D12FastConstantAllocator(Devices[0], ActiveGPUMask(), 1024*1024*2);
			Alloc->Init();
			return Alloc;
		});
	}

	void BlockUntilIdle();

protected:

	virtual void CreateRootDevice(bool bWithDebug);

	virtual void AllocateBuffer(FD3D12Device* Device,
		const D3D12_RESOURCE_DESC& Desc,
		uint32 Size,
		uint32 InUsage,
		FRHIResourceCreateInfo& CreateInfo,
		uint32 Alignment,
		FD3D12TransientResource& TransientResource,
		FD3D12ResourceLocation& ResourceLocation);

	// Creates default root and execute indirect signatures
	void CreateSignatures();

	GPUNodeMask CurrentGPUNode;
	GPUNodeMask ActiveGPUNodes;
	EMultiGPUMode MultiGPUMode;

	FD3D12DynamicRHI* OwningRHI;

	// LDA setups have one ID3D12Device
	TRefCountPtr<ID3D12Device> RootDevice;
	TRefCountPtr<ID3D12Device1> RootDevice1;
	D3D12_RESOURCE_HEAP_TIER ResourceHeapTier;
	D3D12_RESOURCE_BINDING_TIER ResourceBindingTier;
	D3D_ROOT_SIGNATURE_VERSION RootSignatureVersion;

	/** True if the device being used has been removed. */
	bool bDeviceRemoved;

	FD3D12AdapterDesc Desc;
	TRefCountPtr<IDXGIAdapter> DxgiAdapter;

	FD3D12RootSignatureManager RootSignatureManager;

	FD3D12PipelineStateCache PipelineStateCache;

	TRefCountPtr<ID3D12CommandSignature> DrawIndirectCommandSignature;
	TRefCountPtr<ID3D12CommandSignature> DrawIndexedIndirectCommandSignature;
	TRefCountPtr<ID3D12CommandSignature> DispatchIndirectCommandSignature;

	FD3D12FenceCorePool FenceCorePool;

	FD3D12DynamicHeapAllocator*	UploadHeapAllocator;

	/** A list of all viewport RHIs that have been created. */
	TArray<FD3D12Viewport*> Viewports;

	/** The viewport which is currently being drawn. */
	TRefCountPtr<FD3D12Viewport> DrawingViewport;
	TRefCountPtr<IDXGIFactory> DxgiFactory;
	TRefCountPtr<IDXGIFactory2> DxgiFactory2;

	/** A Fence whos value increases every frame*/
	FD3D12ManualFence FrameFence;

	FD3D12DeferredDeletionQueue DeferredDeletionQueue;

	FD3D12CommandContextRedirector DefaultContextRedirector;
	FD3D12CommandContextRedirector DefaultAsyncComputeContextRedirector;

	FD3DGPUProfiler GPUProfilingData;

	TMap<FName, FD3D12TemporalEffect> TemporalEffectMap;

	FD3D12ThreadLocalObject<FD3D12FastConstantAllocator> TransientUniformBufferAllocator;

	// Each of these devices represents a physical GPU 'Node'
	FD3D12Device* Devices[MAX_NUM_LDA_NODES];

	uint32 DebugFlags;
};
