// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Adapter.cpp:D3D12 Adapter implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"

struct FRHICommandSignalFrameFence : public FRHICommand<FRHICommandSignalFrameFence>
{
	ID3D12CommandQueue* const pCommandQueue;
	FD3D12ManualFence* const Fence;
	const uint64 Value;
	FORCEINLINE_DEBUGGABLE FRHICommandSignalFrameFence(ID3D12CommandQueue* pInCommandQueue, FD3D12ManualFence* InFence, uint64 InValue)
		: pCommandQueue(pInCommandQueue)
		, Fence(InFence)
		, Value(InValue)
	{
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		Fence->Signal(pCommandQueue, Value);
		check(Fence->GetLastSignaledFence() == Value);
	}
};

FD3D12Adapter::FD3D12Adapter(FD3D12AdapterDesc& DescIn)
	: Desc(DescIn)
	, bDeviceRemoved(false)
	, OwningRHI(nullptr)
	, ActiveGPUNodes(0)
	, RootSignatureManager(this)
	, PipelineStateCache(this)
	, FenceCorePool(this)
	, MultiGPUMode(MGPU_Disabled)
	, UploadHeapAllocator(nullptr)
	, CurrentGPUNode(GDefaultGPUMask)
	, FrameFence(this, L"Adapter Frame Fence")
	, DeferredDeletionQueue(this)
	, DefaultContextRedirector(this)
	, DefaultAsyncComputeContextRedirector(this)
	, GPUProfilingData(this)
	, DebugFlags(0)
{}

void FD3D12Adapter::Initialize(FD3D12DynamicRHI* RHI)
{
	OwningRHI = RHI;

	// Start off disabled as the engine does Initialization we can't do in AFR
	MultiGPUMode = MGPU_Disabled;
}

void FD3D12Adapter::CreateRootDevice(bool bWithDebug)
{
	CreateDXGIFactory();

	// QI for the Adapter
	TRefCountPtr<IDXGIAdapter> TempAdapter;
	DxgiFactory->EnumAdapters(Desc.AdapterIndex, TempAdapter.GetInitReference());
	VERIFYD3D12RESULT(TempAdapter->QueryInterface(IID_PPV_ARGS(DxgiAdapter.GetInitReference())));

	// In Direct3D 11, if you are trying to create a hardware or a software device, set pAdapter != NULL which constrains the other inputs to be:
	//		DriverType must be D3D_DRIVER_TYPE_UNKNOWN 
	//		Software must be NULL. 
	D3D_DRIVER_TYPE DriverType = D3D_DRIVER_TYPE_UNKNOWN;

#if PLATFORM_WINDOWS
	if (bWithDebug)
	{
		TRefCountPtr<ID3D12Debug> DebugController;
		VERIFYD3D12RESULT(D3D12GetDebugInterface(IID_PPV_ARGS(DebugController.GetInitReference())));
		DebugController->EnableDebugLayer();

		// TODO: MSFT: BEGIN TEMPORARY WORKAROUND for a debug layer issue with the Editor creating lots of viewports (swapchains).
		// Without this you could see this error: D3D12 ERROR: ID3D12CommandQueue::ExecuteCommandLists: Up to 8 swapchains can be written to by a single command queue. Present must be called on one of the swapchains to enable a command queue to execute command lists that write to more.  [ STATE_SETTING ERROR #906: COMMAND_QUEUE_TOO_MANY_SWAPCHAIN_REFERENCES]
		if (GIsEditor)
		{
			TRefCountPtr<ID3D12Debug1> DebugController1;
			const HRESULT HrDebugController1 = D3D12GetDebugInterface(IID_PPV_ARGS(DebugController1.GetInitReference()));
			if (DebugController1.GetReference())
			{
				DebugController1->SetEnableSynchronizedCommandQueueValidation(false);
				UE_LOG(LogD3D12RHI, Warning, TEXT("Disabling the debug layer's Synchronized Command Queue Validation. This means many debug layer features won't do anything. This code should be removed as soon as possible with an update debug layer."));
			}
		}
		// END TEMPORARY WORKAROUND

		bool bD3d12gpuvalidation = false;
		if (FParse::Param(FCommandLine::Get(), TEXT("d3d12gpuvalidation")))
		{
			TRefCountPtr<ID3D12Debug1> DebugController1;
			VERIFYD3D12RESULT(DebugController->QueryInterface(IID_PPV_ARGS(DebugController1.GetInitReference())));
			DebugController1->SetEnableGPUBasedValidation(true);
			bD3d12gpuvalidation = true;
		}

		UE_LOG(LogD3D12RHI, Log, TEXT("InitD3DDevice: -D3DDebug = %s -D3D12GPUValidation = %s"), bWithDebug ? TEXT("on") : TEXT("off"), bD3d12gpuvalidation ? TEXT("on") : TEXT("off") );
	}
#endif // PLATFORM_WINDOWS

#if USE_PIX
	UE_LOG(LogD3D12RHI, Log, TEXT("Emitting draw events for PIX profiling."));
	GEmitDrawEvents = true;
#endif
	const bool bIsPerfHUD = !FCString::Stricmp(GetD3DAdapterDesc().Description, TEXT("NVIDIA PerfHUD"));

	if (bIsPerfHUD)
	{
		DriverType = D3D_DRIVER_TYPE_REFERENCE;
	}

	// Creating the Direct3D device.
	VERIFYD3D12RESULT(D3D12CreateDevice(
		GetAdapter(),
		GetFeatureLevel(),
		IID_PPV_ARGS(RootDevice.GetInitReference())
		));

	// See if we can get any newer device interfaces (to use newer D3D12 features).
	if (D3D12RHI_ShouldForceCompatibility())
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("Forcing D3D12 compatibility."));
	}
	else
	{
		if (SUCCEEDED(RootDevice->QueryInterface(IID_PPV_ARGS(RootDevice1.GetInitReference()))))
		{
			UE_LOG(LogD3D12RHI, Log, TEXT("The system supports ID3D12Device1."));
		}
	}

#if UE_BUILD_DEBUG	&& PLATFORM_WINDOWS
	//break on debug
	TRefCountPtr<ID3D12Debug> d3dDebug;
	if (SUCCEEDED(RootDevice->QueryInterface(__uuidof(ID3D12Debug), (void**)d3dDebug.GetInitReference())))
	{
		TRefCountPtr<ID3D12InfoQueue> d3dInfoQueue;
		if (SUCCEEDED(d3dDebug->QueryInterface(__uuidof(ID3D12InfoQueue), (void**)d3dInfoQueue.GetInitReference())))
		{
			d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			//d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		}
	}
#endif

#if !(UE_BUILD_SHIPPING && WITH_EDITOR) && PLATFORM_WINDOWS
	// Add some filter outs for known debug spew messages (that we don't care about)
	if (bWithDebug)
	{
		ID3D12InfoQueue *pd3dInfoQueue = nullptr;
		VERIFYD3D12RESULT(RootDevice->QueryInterface(__uuidof(ID3D12InfoQueue), (void**)&pd3dInfoQueue));
		if (pd3dInfoQueue)
		{
			D3D12_INFO_QUEUE_FILTER NewFilter;
			FMemory::Memzero(&NewFilter, sizeof(NewFilter));

			// Turn off info msgs as these get really spewy
			D3D12_MESSAGE_SEVERITY DenySeverity = D3D12_MESSAGE_SEVERITY_INFO;
			NewFilter.DenyList.NumSeverities = 1;
			NewFilter.DenyList.pSeverityList = &DenySeverity;

			// Be sure to carefully comment the reason for any additions here!  Someone should be able to look at it later and get an idea of whether it is still necessary.
			D3D12_MESSAGE_ID DenyIds[] = {
				// OMSETRENDERTARGETS_INVALIDVIEW - d3d will complain if depth and color targets don't have the exact same dimensions, but actually
				//	if the color target is smaller then things are ok.  So turn off this error.  There is a manual check in FD3D12DynamicRHI::SetRenderTarget
				//	that tests for depth smaller than color and MSAA settings to match.
				D3D12_MESSAGE_ID_OMSETRENDERTARGETS_INVALIDVIEW,

				// QUERY_BEGIN_ABANDONING_PREVIOUS_RESULTS - The RHI exposes the interface to make and issue queries and a separate interface to use that data.
				//		Currently there is a situation where queries are issued and the results may be ignored on purpose.  Filtering out this message so it doesn't
				//		swarm the debug spew and mask other important warnings
				//D3D12_MESSAGE_ID_QUERY_BEGIN_ABANDONING_PREVIOUS_RESULTS,
				//D3D12_MESSAGE_ID_QUERY_END_ABANDONING_PREVIOUS_RESULTS,

				// D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT - This is a warning that gets triggered if you use a null vertex declaration,
				//       which we want to do when the vertex shader is generating vertices based on ID.
				D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT,

				// D3D12_MESSAGE_ID_COMMAND_LIST_DRAW_INDEX_BUFFER_TOO_SMALL - This warning gets triggered by Slate draws which are actually using a valid index range.
				//		The invalid warning seems to only happen when VS 2012 is installed.  Reported to MS.  
				//		There is now an assert in DrawIndexedPrimitive to catch any valid errors reading from the index buffer outside of range.
				D3D12_MESSAGE_ID_COMMAND_LIST_DRAW_INDEX_BUFFER_TOO_SMALL,

				// D3D12_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET - This warning gets triggered by shadow depth rendering because the shader outputs
				//		a color but we don't bind a color render target. That is safe as writes to unbound render targets are discarded.
				//		Also, batched elements triggers it when rendering outside of scene rendering as it outputs to the GBuffer containing normals which is not bound.
				//(D3D12_MESSAGE_ID)3146081, // D3D12_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET,
				// BUGBUG: There is a D3D12_MESSAGE_ID_DEVICE_DRAW_DEPTHSTENCILVIEW_NOT_SET, why not one for RT?

				// D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE/D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE - 
				//      This warning gets triggered by ClearDepthStencilView/ClearRenderTargetView because when the resource was created
				//      it wasn't passed an optimized clear color (see CreateCommitedResource). This shows up a lot and is very noisy.
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,

				// D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_GPU_WRITTEN_READBACK_RESOURCE_MAPPED - This warning gets triggered by ExecuteCommandLists.
				//		if it contains a readback resource that still has mapped subresources when executing a command list that performs a copy operation to the resource.
				//		This may be ok if any data read from the readback resources was flushed by calling Unmap() after the resourcecopy operation completed.
				//		We intentionally keep the readback resources persistently mapped.
				D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_GPU_WRITTEN_READBACK_RESOURCE_MAPPED,

				// Note message ID doesn't exist in the current header (yet, should be available in the RS2 header) for now just mute by the ID number.
				// RESOURCE_BARRIER_DUPLICATE_SUBRESOURCE_TRANSITIONS - This shows up a lot and is very noisy. It would require changes to the resource tracking system
				// but will hopefully be resolved when the RHI switches to use the engine's resource tracking system.
				(D3D12_MESSAGE_ID)1008,

#if ENABLE_RESIDENCY_MANAGEMENT
				// TODO: Remove this when the debug layers work for executions which are guarded by a fence
				D3D12_MESSAGE_ID_INVALID_USE_OF_NON_RESIDENT_RESOURCE
#endif
			};

			NewFilter.DenyList.NumIDs = sizeof(DenyIds) / sizeof(D3D12_MESSAGE_ID);
			NewFilter.DenyList.pIDList = (D3D12_MESSAGE_ID*)&DenyIds;

			pd3dInfoQueue->PushStorageFilter(&NewFilter);

			// Break on D3D debug errors.
			pd3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

			// Enable this to break on a specific id in order to quickly get a callstack
			//pd3dInfoQueue->SetBreakOnID(D3D12_MESSAGE_ID_DEVICE_DRAW_CONSTANT_BUFFER_TOO_SMALL, true);

			if (FParse::Param(FCommandLine::Get(), TEXT("d3dbreakonwarning")))
			{
				pd3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
			}

			pd3dInfoQueue->Release();
		}
	}
#endif
}

void FD3D12Adapter::InitializeDevices()
{
	check(IsInGameThread());

	// Wait for the rendering thread to go idle.
	SCOPED_SUSPEND_RENDERING_THREAD(false);

	// If the device we were using has been removed, release it and the resources we created for it.
	if (bDeviceRemoved)
	{
		check(RootDevice);

		HRESULT hRes = RootDevice->GetDeviceRemovedReason();

		const TCHAR* Reason = TEXT("?");
		switch (hRes)
		{
		case DXGI_ERROR_DEVICE_HUNG:			Reason = TEXT("HUNG"); break;
		case DXGI_ERROR_DEVICE_REMOVED:			Reason = TEXT("REMOVED"); break;
		case DXGI_ERROR_DEVICE_RESET:			Reason = TEXT("RESET"); break;
		case DXGI_ERROR_DRIVER_INTERNAL_ERROR:	Reason = TEXT("INTERNAL_ERROR"); break;
		case DXGI_ERROR_INVALID_CALL:			Reason = TEXT("INVALID_CALL"); break;
		}

		bDeviceRemoved = false;

		Cleanup();

		// We currently don't support removed devices because FTexture2DResource can't recreate its RHI resources from scratch.
		// We would also need to recreate the viewport swap chains from scratch.
		UE_LOG(LogD3D12RHI, Fatal, TEXT("The Direct3D 12 device that was being used has been removed (Error: %d '%s').  Please restart the game."), hRes, Reason);
	}

	// Use a debug device if specified on the command line.
	bool bWithD3DDebug = D3D12RHI_ShouldCreateWithD3DDebug();

	// If we don't have a device yet, either because this is the first viewport, or the old device was removed, create a device.
	if (!RootDevice)
	{
		CreateRootDevice(bWithD3DDebug);

		D3D12_FEATURE_DATA_D3D12_OPTIONS D3D12Caps;
		FMemory::Memzero(&D3D12Caps, sizeof(D3D12Caps));

		VERIFYD3D12RESULT(RootDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &D3D12Caps, sizeof(D3D12Caps)));
		ResourceHeapTier = D3D12Caps.ResourceHeapTier;
		ResourceBindingTier = D3D12Caps.ResourceBindingTier;

		D3D12_FEATURE_DATA_ROOT_SIGNATURE D3D12RootSignatureCaps = {};
		D3D12RootSignatureCaps.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;	// This is the highest version we currently support. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
		if (FAILED(RootDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &D3D12RootSignatureCaps, sizeof(D3D12RootSignatureCaps))))
		{
			D3D12RootSignatureCaps.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}
		RootSignatureVersion = D3D12RootSignatureCaps.HighestVersion;

		FrameFence.CreateFence();

		CreateSignatures();

		const uint32 NumGPUsToInit = GetNumGPUNodes();

		//Create all of the FD3D12Devices
		for (uint32 i = 0; i < NumGPUsToInit; i++)
		{
			GPUNodeMask Node = (1 << i);
			ActiveGPUNodes |= Node;

			Devices[i] = new FD3D12Device(Node, this);
			Devices[i]->Initialize();

			// When using AFR we shim in a proxy between what the upper engine thinks is the 'default' context
			// so that we can switch it out every frame. This points the proxy to each of the actual contexts.
			{
				DefaultContextRedirector.SetPhysicalContext(i, &Devices[i]->GetDefaultCommandContext());

				if (GEnableAsyncCompute)
				{
					DefaultAsyncComputeContextRedirector.SetPhysicalContext(i, &Devices[i]->GetDefaultAsyncComputeContext());
				}
			}
		}

		GPUProfilingData.Init();

		const FString Name(L"Upload Buffer Allocator");
		// Safe to init as we have a device;
		UploadHeapAllocator = new FD3D12DynamicHeapAllocator(this,
			Devices[0],
			Name,
			kManualSubAllocationStrategy,
			DEFAULT_CONTEXT_UPLOAD_POOL_MAX_ALLOC_SIZE,
			DEFAULT_CONTEXT_UPLOAD_POOL_SIZE,
			DEFAULT_CONTEXT_UPLOAD_POOL_ALIGNMENT);

		UploadHeapAllocator->Init();

		FString GraphicsCacheFile = PIPELINE_STATE_FILE_LOCATION / TEXT("D3DGraphics.ushaderprecache");
		FString ComputeCacheFile = PIPELINE_STATE_FILE_LOCATION / TEXT("D3DCompute.ushaderprecache");
		FString DriverBlobFilename = PIPELINE_STATE_FILE_LOCATION / TEXT("D3DDriverByteCodeBlob.ushaderprecache");

		PipelineStateCache.Init(GraphicsCacheFile, ComputeCacheFile, DriverBlobFilename);

		ID3D12RootSignature* StaticGraphicsRS = (GetStaticGraphicsRootSignature()) ? GetStaticGraphicsRootSignature()->GetRootSignature() : nullptr;
		ID3D12RootSignature* StaticComputeRS = (GetStaticComputeRootSignature()) ? GetStaticComputeRootSignature()->GetRootSignature() : nullptr;

		PipelineStateCache.RebuildFromDiskCache(StaticGraphicsRS, StaticComputeRS);
	}
}

void FD3D12Adapter::CreateSignatures()
{
	ID3D12Device* Device = GetD3DDevice();

	// ExecuteIndirect command signatures
	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.NumArgumentDescs = 1;
	commandSignatureDesc.ByteStride = 20;
	commandSignatureDesc.NodeMask = ActiveGPUNodes;

	D3D12_INDIRECT_ARGUMENT_DESC indirectParameterDesc[1] = {};
	indirectParameterDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

	commandSignatureDesc.pArgumentDescs = indirectParameterDesc;

	commandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

	VERIFYD3D12RESULT(Device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(DrawIndirectCommandSignature.GetInitReference())));

	indirectParameterDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	VERIFYD3D12RESULT(Device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(DrawIndexedIndirectCommandSignature.GetInitReference())));

	indirectParameterDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
	commandSignatureDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
	VERIFYD3D12RESULT(Device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(DispatchIndirectCommandSignature.GetInitReference())));
}


void FD3D12Adapter::Cleanup()
{
	// Execute
	FlushRenderingCommands();
	FRHICommandListExecutor::CheckNoOutstandingCmdLists();
	FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);

	// Reset the RHI initialized flag.
	GIsRHIInitialized = false;

	for (auto& Viewport : Viewports)
	{
		Viewport->IssueFrameEvent();
		Viewport->WaitForFrameEventCompletion();
	}

	// Manually destroy the effects as we can't do it in their destructor.
	for (auto& Effect : TemporalEffectMap)
	{
		Effect.Value.Destroy();
	}

	// Ask all initialized FRenderResources to release their RHI resources.
	for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList()); ResourceIt; ResourceIt.Next())
	{
		FRenderResource* Resource = *ResourceIt;
		check(Resource->IsInitialized());
		Resource->ReleaseRHI();
	}

	for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList()); ResourceIt; ResourceIt.Next())
	{
		ResourceIt->ReleaseDynamicRHI();
	}

	TransientUniformBufferAllocator.Destroy();

	FRHIResource::FlushPendingDeletes();

	// Clean up the asnyc texture thread allocators
	for (int32 i = 0; i < GetOwningRHI()->NumThreadDynamicHeapAllocators; i++)
	{
		GetOwningRHI()->ThreadDynamicHeapAllocatorArray[i]->Destroy<FD3D12ScopeLock>();
		delete(GetOwningRHI()->ThreadDynamicHeapAllocatorArray[i]);
	}

	// Cleanup resources
	DeferredDeletionQueue.Clear();

	for (SIZE_T i = 0; i < GetNumGPUNodes(); i++)
	{
		Devices[i]->Cleanup();
		delete(Devices[i]);
		Devices[i] = nullptr;
	}

	// Release buffered timestamp queries
	GPUProfilingData.FrameTiming.ReleaseResource();

	Viewports.Empty();
	DrawingViewport = nullptr;

	UploadHeapAllocator->Destroy();
	delete(UploadHeapAllocator);
	UploadHeapAllocator = nullptr;

	PipelineStateCache.Close();
	FenceCorePool.Destroy();
}

void FD3D12Adapter::EndFrame()
{
	GetUploadHeapAllocator().CleanUpAllocations();
	GetDeferredDeletionQueue().ReleaseResources();
}

void FD3D12Adapter::SwitchToNextGPU()
{
	if (MultiGPUMode == MGPU_AFR)
	{
		CurrentGPUNode <<= 1;
		CurrentGPUNode &= ActiveGPUNodes;
		// Wrap around
		if (CurrentGPUNode == 0)
		{
			CurrentGPUNode = 1;
		}
	}
}

void FD3D12Adapter::SignalFrameFence_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());
	check(RHICmdList.IsImmediate());
	ID3D12CommandQueue* const pCommandQueue = GetCurrentDevice()->GetCommandListManager().GetD3DCommandQueue();

	// Increment the current fence (on render thread timeline).
	const uint64 PreviousFence = FrameFence.IncrementCurrentFence();

	// Queue a command to signal the frame fence is complete on the GPU (on the RHI thread timeline if using an RHI thread).
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		FRHICommandSignalFrameFence Cmd(pCommandQueue, &FrameFence, PreviousFence);
		Cmd.Execute(RHICmdList);
	}
	else
	{
		new (RHICmdList.AllocCommand<FRHICommandSignalFrameFence>()) FRHICommandSignalFrameFence(pCommandQueue, &GetFrameFence(), PreviousFence);
	}
}

FD3D12TemporalEffect* FD3D12Adapter::GetTemporalEffect(const FName& EffectName)
{
	FD3D12TemporalEffect* Effect = TemporalEffectMap.Find(EffectName);

	if (Effect == nullptr)
	{
		Effect = &TemporalEffectMap.Emplace(EffectName, FD3D12TemporalEffect(this, EffectName));
		Effect->Init();
	}

	check(Effect);
	return Effect;
}

void FD3D12Adapter::BlockUntilIdle()
{
	const uint32 NumGPUs = GetNumGPUNodes();
	for (uint32 Index = 0; Index < NumGPUs; ++Index)
	{
		GetDeviceByIndex(Index)->BlockUntilIdle();
	}
}