// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Viewport.cpp: D3D viewport RHI implementation.
	=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "RenderCore.h"

namespace D3D12RHI
{
	/**
	 * RHI console variables used by viewports.
	 */
	namespace RHIConsoleVariables
	{
		int32 bSyncWithDWM = 0;
		static FAutoConsoleVariableRef CVarSyncWithDWM(
			TEXT("D3D12.SyncWithDWM"),
			bSyncWithDWM,
			TEXT("If true, synchronize with the desktop window manager for vblank."),
			ECVF_RenderThreadSafe
			);

		float RefreshPercentageBeforePresent = 1.0f;
		static FAutoConsoleVariableRef CVarRefreshPercentageBeforePresent(
			TEXT("D3D12.RefreshPercentageBeforePresent"),
			RefreshPercentageBeforePresent,
			TEXT("The percentage of the refresh period to wait before presenting."),
			ECVF_RenderThreadSafe
			);

		int32 bForceThirtyHz = 1;
		static FAutoConsoleVariableRef CVarForceThirtyHz(
			TEXT("D3D12.ForceThirtyHz"),
			bForceThirtyHz,
			TEXT("If true, the display will never update more often than 30Hz."),
			ECVF_RenderThreadSafe
			);

		int32 SyncInterval = 1;
		static FAutoConsoleVariableRef CVarSyncInterval(
			TEXT("D3D12.SyncInterval"),
			SyncInterval,
			TEXT("When synchronizing with D3D, specifies the interval at which to refresh."),
			ECVF_RenderThreadSafe
			);

		float SyncRefreshThreshold = 1.05f;
		static FAutoConsoleVariableRef CVarSyncRefreshThreshold(
			TEXT("D3D12.SyncRefreshThreshold"),
			SyncRefreshThreshold,
			TEXT("Threshold for time above which vsync will be disabled as a percentage of the refresh rate."),
			ECVF_RenderThreadSafe
			);

		int32 MaxSyncCounter = 8;
		static FAutoConsoleVariableRef CVarMaxSyncCounter(
			TEXT("D3D12.MaxSyncCounter"),
			MaxSyncCounter,
			TEXT("Maximum sync counter to smooth out vsync transitions."),
			ECVF_RenderThreadSafe
			);

		int32 SyncThreshold = 7;
		static FAutoConsoleVariableRef CVarSyncThreshold(
			TEXT("D3D12.SyncThreshold"),
			SyncThreshold,
			TEXT("Number of consecutive 'fast' frames before vsync is enabled."),
			ECVF_RenderThreadSafe
			);

		int32 MaximumFrameLatency = 3;
		static FAutoConsoleVariableRef CVarMaximumFrameLatency(
			TEXT("D3D12.MaximumFrameLatency"),
			MaximumFrameLatency,
			TEXT("Number of frames that can be queued for render."),
			ECVF_RenderThreadSafe
			);

		int32 AFRUseFramePacing = 0;
		static FAutoConsoleVariableRef CVarUseFramePacing(
			TEXT("D3D12.AFRUseFramePacing"),
			AFRUseFramePacing,
			TEXT("Control when frames are presented when using mGPU and Alternate Frame Rendering."),
			ECVF_RenderThreadSafe
			);

#if UE_BUILD_DEBUG
		int32 DumpStatsEveryNFrames = 0;
		static FAutoConsoleVariableRef CVarDumpStatsNFrames(
			TEXT("D3D12.DumpStatsEveryNFrames"),
			DumpStatsEveryNFrames,
			TEXT("Dumps D3D12 stats every N frames on Present; 0 means no information (default)."),
			ECVF_RenderThreadSafe
			);
#endif
	};
}
using namespace D3D12RHI;

#if PLATFORM_SUPPORTS_MGPU
FD3D12FramePacing::FD3D12FramePacing(FD3D12Adapter* Parent)
	: bKeepRunning(true)
	, AvgFrameTimeMs(0.0f)
	, LastFrameTimeMs(0)
	, Thread(nullptr)
	, FD3D12AdapterChild(Parent)
{
	VERIFYD3D12RESULT(Parent->GetD3DDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(Fence.GetInitReference())));
	FMemory::Memset(SleepTimes, 0);

	Thread = FRunnableThread::Create(this, TEXT("FramePacer"), 0, TPri_AboveNormal);
}

FD3D12FramePacing::~FD3D12FramePacing()
{
	delete Thread;
	Thread = nullptr;
}

bool FD3D12FramePacing::Init()
{
	Semaphore = CreateSemaphore(nullptr, 0, MaxFrames, nullptr);
	return Semaphore != INVALID_HANDLE_VALUE;
}

void FD3D12FramePacing::Stop()
{
	bKeepRunning = false;
	FMemory::Memset(SleepTimes, 0);

	ReleaseSemaphore(Semaphore, 1, nullptr);
	VERIFYD3D12RESULT(Fence->Signal(UINT64_MAX));
}

void FD3D12FramePacing::Exit()
{
	CloseHandle(Semaphore);
}

uint32 FD3D12FramePacing::Run()
{
	while (bKeepRunning)
	{
		// Wait for the present to be submitted so we know which GPU to wait on
		WaitForSingleObjectEx(Semaphore, INFINITE, false);
		check(CurIndex <= NextIndex || !bKeepRunning);

		// Wait for the present to be completed so we can start timing to the next one
		const uint32 ReadIndex = CurIndex % MaxFrames;

		// Wait for the right amount of time to pass
		const uint32 SleepTime = SleepTimes[ReadIndex];
		Sleep(SleepTime);

		VERIFYD3D12RESULT(Fence->Signal(++CurIndex));
	}
	return 0;
}

void FD3D12FramePacing::PrePresentQueued(ID3D12CommandQueue* Queue)
{
	const uint64 CurrTimeMs = GetTickCount64();
	check(CurrTimeMs >= LastFrameTimeMs);

	const float Delta = float(CurrTimeMs - LastFrameTimeMs);
	const float Alpha = FMath::Clamp(Delta / 1000.0f / FramePacingAvgTimePeriod, 0.0f, 1.0f);

	/** Number of milliseconds the GPU was busy last frame. */
	const uint32 GPUCycles = RHIGetGPUFrameCycles();
	const float GPUMsForFrame = FPlatformTime::ToMilliseconds(GPUCycles);

	AvgFrameTimeMs = (Alpha * GPUMsForFrame) + ((1.0f - Alpha) * AvgFrameTimeMs);
	LastFrameTimeMs = CurrTimeMs;

	const float TargetFrameTime = AvgFrameTimeMs * FramePacingPercentage / GetParentAdapter()->GetNumGPUNodes();

	const uint32 WriteIndex = NextIndex % MaxFrames;
	SleepTimes[WriteIndex] = (uint32)TargetFrameTime;
	VERIFYD3D12RESULT(Queue->Wait(Fence, ++NextIndex));
	ReleaseSemaphore(Semaphore, 1, nullptr);
}
#endif //PLATFORM_SUPPORTS_MGPU

/**
 * Creates a FD3D12Surface to represent a swap chain's back buffer.
 */
FD3D12Texture2D* GetSwapChainSurface(FD3D12Device* Parent, EPixelFormat PixelFormat, IDXGISwapChain* SwapChain, const uint32 &backBufferIndex)
{
	const GPUNodeMask Node = Parent->GetNodeMask();

	// Grab the back buffer
	TRefCountPtr<ID3D12Resource> BackBufferResource;
	VERIFYD3D12RESULT_EX(SwapChain->GetBuffer(backBufferIndex, IID_PPV_ARGS(BackBufferResource.GetInitReference())), Parent->GetDevice());

	D3D12_RESOURCE_DESC BackBufferDesc = BackBufferResource->GetDesc();

	FD3D12Texture2D* NewTexture = new FD3D12Texture2D(Parent,
		(uint32)BackBufferDesc.Width,
		BackBufferDesc.Height,
		1,
		1,
		1,
		PixelFormat,
		false,
		false,
		FClearValueBinding());

	const D3D12_RESOURCE_STATES State = D3D12_RESOURCE_STATE_COMMON;

	FD3D12Resource* NewResourceWrapper = new FD3D12Resource(Parent, Node, BackBufferResource, State, BackBufferDesc);
	NewResourceWrapper->AddRef();
	NewTexture->ResourceLocation.AsStandAlone(NewResourceWrapper);

	// create the render target view
	D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
	RTVDesc.Format = BackBufferDesc.Format;
	RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	RTVDesc.Texture2D.MipSlice = 0;

	FD3D12RenderTargetView* BackBufferRenderTargetView = new FD3D12RenderTargetView(Parent, &RTVDesc, &NewTexture->ResourceLocation);
	NewTexture->SetRenderTargetView(BackBufferRenderTargetView);

	// create a shader resource view to allow using the backbuffer as a texture
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.Format = BackBufferDesc.Format;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = 1;

	FD3D12ShaderResourceView* WrappedShaderResourceView = new FD3D12ShaderResourceView(Parent, &SRVDesc, &NewTexture->ResourceLocation);
	NewTexture->SetShaderResourceView(WrappedShaderResourceView);

	FD3D12TextureStats::D3D12TextureAllocated2D(*NewTexture);

	NewTexture->DoNoDeferDelete();
	BackBufferRenderTargetView->DoNoDeferDelete();
	WrappedShaderResourceView->DoNoDeferDelete();

	return NewTexture;
}

FD3D12Viewport::~FD3D12Viewport()
{
	check(IsInRenderingThread());

	GetParentAdapter()->GetViewports().Remove(this);

#if PLATFORM_SUPPORTS_MGPU
	if (FramePacerRunnable)
	{
		delete FramePacerRunnable;
		FramePacerRunnable = nullptr;
	}
#endif //PLATFORM_SUPPORTS_MGPU
}

DXGI_MODE_DESC FD3D12Viewport::SetupDXGI_MODE_DESC() const
{
	DXGI_MODE_DESC Ret;

	Ret.Width = SizeX;
	Ret.Height = SizeY;
	Ret.RefreshRate.Numerator = 0;	// illamas: use 0 to avoid a potential mismatch with hw
	Ret.RefreshRate.Denominator = 0;	// illamas: ditto
	Ret.Format = GetRenderTargetFormat(PixelFormat);
	Ret.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	Ret.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	return Ret;
}

void FD3D12Viewport::CalculateSwapChainDepth()
{
	FD3D12Adapter* Adapter = GetParentAdapter();
	NumBackBuffers = (Adapter->AlternateFrameRenderingEnabled()) ? (AFRNumBackBuffersPerNode * Adapter->GetNumGPUNodes()) : DefaultNumBackBuffers;

	BackBuffers.Empty();
	BackBuffers.AddZeroed(NumBackBuffers);

	SDRBackBuffers.Empty();
	SDRBackBuffers.AddZeroed(NumBackBuffers);
}

void FD3D12Viewport::Resize(uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen, EPixelFormat PreferredPixelFormat)
{
	FD3D12Adapter* Adapter = GetParentAdapter();
	const uint32 NumGPUs = Adapter->GetNumGPUNodes();

#if LOG_VIEWPORT_EVENTS
	const FString ThreadName(FThreadManager::Get().GetThreadName(FPlatformTLS::GetCurrentThreadId()));
	UE_LOG(LogD3D12RHI, Log, TEXT("Thread %s: Resize Viewport %#016llx (%ux%u)"), ThreadName.GetCharArray().GetData(), this, InSizeX, InSizeY);
#endif

	// Flush the outstanding GPU work and wait for it to complete.
	FlushRenderingCommands();
	FRHICommandListExecutor::CheckNoOutstandingCmdLists();
	Adapter->BlockUntilIdle();

	// Unbind any dangling references to resources.
	for (size_t i = 0; i < NumGPUs; i++)
	{
		FD3D12Device* Device = Adapter->GetDevice(1 << i);
		Device->GetDefaultCommandContext().ClearState();
		Device->GetDefaultAsyncComputeContext().ClearState();
	}

	if (IsValidRef(CustomPresent))
	{
		CustomPresent->OnBackBufferResize();
	}

	// Release our backbuffer reference, as required by DXGI before calling ResizeBuffers.
	for (uint32 i = 0; i < NumBackBuffers; ++i)
	{
		if (IsValidRef(BackBuffers[i]))
		{
			check(BackBuffers[i]->GetRefCount() == 1);

			// Tell the back buffer to delete immediately so that we can call resize.
			BackBuffers[i]->GetResource()->DoNotDeferDelete();
		}
		
		BackBuffers[i].SafeRelease();
		check(BackBuffers[i] == nullptr);

		if (IsValidRef(SDRBackBuffers[i]))
		{
			check(SDRBackBuffers[i]->GetRefCount() == 1);
			SDRBackBuffers[i]->GetResource()->DoNotDeferDelete();
		}

		SDRBackBuffers[i].SafeRelease();
		check(SDRBackBuffers[i] == nullptr);
	}

	// Keep the current pixel format if one wasn't specified.
	if (PreferredPixelFormat == PF_Unknown)
	{
		PreferredPixelFormat = PixelFormat;
	}

	if (SizeX != InSizeX || SizeY != InSizeY || PixelFormat != PreferredPixelFormat)
	{
		SizeX = InSizeX;
		SizeY = InSizeY;
		PixelFormat = PreferredPixelFormat;

		check(SizeX > 0);
		check(SizeY > 0);

		if (bInIsFullscreen)
		{
			const DXGI_MODE_DESC BufferDesc = SetupDXGI_MODE_DESC();
			if (FAILED(SwapChain1->ResizeTarget(&BufferDesc)))
			{
				ConditionalResetSwapChain(true);
			}
		}
	}

	if (bIsFullscreen != bInIsFullscreen)
	{
		bIsFullscreen = bInIsFullscreen;
		bIsValid = false;

		// Use ConditionalResetSwapChain to call SetFullscreenState, to handle the failure case.
		// Ignore the viewport's focus state; since Resize is called as the result of a user action we assume authority without waiting for Focus.
		ConditionalResetSwapChain(true);
	}

	ResizeInternal();

	// Enable HDR if desired.
	if (CheckHDRSupport())
	{
		EnableHDR();
	}
	else
	{
		ShutdownHDR();
	}
}

/** Returns true if desktop composition is enabled. */
static bool IsCompositionEnabled()
{
	BOOL bDwmEnabled = false;
#if defined(D3D12_WITH_DWMAPI) && D3D12_WITH_DWMAPI
	DwmIsCompositionEnabled(&bDwmEnabled);
#endif	//D3D12_WITH_DWMAPI
	return !!bDwmEnabled;
}

/** Presents the swap chain checking the return result. */
bool FD3D12Viewport::PresentChecked(int32 SyncInterval)
{
	HRESULT Result = S_OK;
	bool bNeedNativePresent = true;

	if (IsValidRef(CustomPresent))
	{
		bNeedNativePresent = CustomPresent->Present(SyncInterval);
	}
	if (bNeedNativePresent)
	{
		// Present the back buffer to the viewport window.
		Result = PresentInternal(SyncInterval);

		if (IsValidRef(CustomPresent))
		{
			CustomPresent->PostPresent();
		}

#if LOG_PRESENT
		const FString ThreadName(FThreadManager::Get().GetThreadName(FPlatformTLS::GetCurrentThreadId()));
		UE_LOG(LogD3D12RHI, Log, TEXT("*** PRESENT: Thread %s: Viewport %#016llx: BackBuffer %#016llx (SyncInterval %u) ***"), ThreadName.GetCharArray().GetData(), this, GetBackBuffer_RHIThread(), SyncInterval);
#endif

	}

	// Detect a lost device.
	if (Result == DXGI_ERROR_DEVICE_REMOVED || Result == DXGI_ERROR_DEVICE_RESET || Result == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
	{
		// This variable is checked periodically by the main thread.
		GetParentAdapter()->SetDeviceRemoved(true);
	}
	else
	{
		VERIFYD3D12RESULT(Result);
	}

	return bNeedNativePresent;
}

/** Blocks the CPU to synchronize with vblank by communicating with DWM. */
void FD3D12Viewport::PresentWithVsyncDWM()
{
#if defined(D3D12_WITH_DWMAPI) && D3D12_WITH_DWMAPI
	LARGE_INTEGER Cycles;
	DWM_TIMING_INFO TimingInfo;

	// Find out how long since we last flipped and query DWM for timing information.
	QueryPerformanceCounter(&Cycles);
	FMemory::Memzero(TimingInfo);
	TimingInfo.cbSize = sizeof(DWM_TIMING_INFO);
	DwmGetCompositionTimingInfo(WindowHandle, &TimingInfo);

	uint64 QpcAtFlip = Cycles.QuadPart;
	uint64 CyclesSinceLastFlip = Cycles.QuadPart - LastFlipTime;
	float CPUTime = FPlatformTime::ToMilliseconds(CyclesSinceLastFlip);
	float GPUTime = FPlatformTime::ToMilliseconds(TimingInfo.qpcFrameComplete - LastCompleteTime);
	float DisplayRefreshPeriod = FPlatformTime::ToMilliseconds(TimingInfo.qpcRefreshPeriod);

	// Find the smallest multiple of the refresh rate that is >= 33ms, our target frame rate.
	float RefreshPeriod = DisplayRefreshPeriod;
	if (RHIConsoleVariables::bForceThirtyHz && RefreshPeriod > 1.0f)
	{
		while (RefreshPeriod - (1000.0f / 30.0f) < -1.0f)
		{
			RefreshPeriod *= 2.0f;
		}
	}

	// If the last frame hasn't completed yet, we don't know how long the GPU took.
	bool bValidGPUTime = (TimingInfo.cFrameComplete > LastFrameComplete);
	if (bValidGPUTime)
	{
		GPUTime /= (float)(TimingInfo.cFrameComplete - LastFrameComplete);
	}

	// Update the sync counter depending on how much time it took to complete the previous frame.
	float FrameTime = FMath::Max<float>(CPUTime, GPUTime);
	if (FrameTime >= RHIConsoleVariables::SyncRefreshThreshold * RefreshPeriod)
	{
		SyncCounter--;
	}
	else if (bValidGPUTime)
	{
		SyncCounter++;
	}
	SyncCounter = FMath::Clamp<int32>(SyncCounter, 0, RHIConsoleVariables::MaxSyncCounter);

	// If frames are being completed quickly enough, block for vsync.
	bool bSync = (SyncCounter >= RHIConsoleVariables::SyncThreshold);
	if (bSync)
	{
		// This flushes the previous present call and blocks until it is made available to DWM.
		GetParentDevice()->GetDefaultCommandContext().FlushCommands();
		// MS: Might need to wait for the previous command list to finish

		DwmFlush();

		// We sleep a percentage of the remaining time. The trick is to get the
		// present call in after the vblank we just synced for but with time to
		// spare for the next vblank.
		float MinFrameTime = RefreshPeriod * RHIConsoleVariables::RefreshPercentageBeforePresent;
		float TimeToSleep;
		do
		{
			QueryPerformanceCounter(&Cycles);
			float TimeSinceFlip = FPlatformTime::ToMilliseconds(Cycles.QuadPart - LastFlipTime);
			TimeToSleep = (MinFrameTime - TimeSinceFlip);
			if (TimeToSleep > 0.0f)
			{
				FPlatformProcess::Sleep(TimeToSleep * 0.001f);
			}
		} while (TimeToSleep > 0.0f);
	}

	// Present.
	PresentChecked(/*SyncInterval=*/ 0);

	// If we are forcing <= 30Hz, block the CPU an additional amount of time if needed.
	// This second block is only needed when RefreshPercentageBeforePresent < 1.0.
	if (bSync)
	{
		LARGE_INTEGER LocalCycles;
		float TimeToSleep;
		bool bSaveCycles = false;
		do
		{
			QueryPerformanceCounter(&LocalCycles);
			float TimeSinceFlip = FPlatformTime::ToMilliseconds(LocalCycles.QuadPart - LastFlipTime);
			TimeToSleep = (RefreshPeriod - TimeSinceFlip);
			if (TimeToSleep > 0.0f)
			{
				bSaveCycles = true;
				FPlatformProcess::Sleep(TimeToSleep * 0.001f);
			}
		} while (TimeToSleep > 0.0f);

		if (bSaveCycles)
		{
			Cycles = LocalCycles;
		}
	}

	// If we are dropping vsync reset the counter. This provides a debounce time
	// before which we try to vsync again.
	if (!bSync && bSyncedLastFrame)
	{
		SyncCounter = 0;
	}

	if (bSync != bSyncedLastFrame || UE_LOG_ACTIVE(LogRHI, VeryVerbose))
	{
		UE_LOG(LogRHI, Verbose, TEXT("BlockForVsync[%d]: CPUTime:%.2fms GPUTime[%d]:%.2fms Blocked:%.2fms Pending/Complete:%d/%d"),
			bSync,
			CPUTime,
			bValidGPUTime,
			GPUTime,
			FPlatformTime::ToMilliseconds(Cycles.QuadPart - QpcAtFlip),
			TimingInfo.cFramePending,
			TimingInfo.cFrameComplete);
	}

	// Remember if we synced, when the frame completed, etc.
	bSyncedLastFrame = bSync;
	LastFlipTime = Cycles.QuadPart;
	LastFrameComplete = TimingInfo.cFrameComplete;
	LastCompleteTime = TimingInfo.qpcFrameComplete;
#endif	//D3D12_WITH_DWMAPI
}

bool FD3D12Viewport::Present(bool bLockToVsync)
{
	FD3D12Adapter* Adapter = GetParentAdapter();
	FD3D12Device* Device = Adapter->GetCurrentDevice();

	FD3D12CommandContext& DefaultContext = Device->GetDefaultCommandContext();

	FD3D12DynamicRHI::TransitionResource(DefaultContext.CommandListHandle, GetBackBuffer_RHIThread()->GetShaderResourceView(), D3D12_RESOURCE_STATE_PRESENT);
	if (SDRBackBuffer_RHIThread != nullptr)
	{
		FD3D12DynamicRHI::TransitionResource(DefaultContext.CommandListHandle, GetSDRBackBuffer_RHIThread()->GetShaderResourceView(), D3D12_RESOURCE_STATE_PRESENT);
	}
	DefaultContext.CommandListHandle.FlushResourceBarriers();

	// Stop Timing at the very last moment
	Adapter->GetGPUProfiler().EndFrame(Device->GetOwningRHI());

	// Execute the current command lists, and then open a new command list with a new command allocator.
	DefaultContext.ReleaseCommandAllocator();
	DefaultContext.FlushCommands();

	// Reset the default context state
	DefaultContext.ClearState();

	if (GEnableAsyncCompute)
	{
		FD3D12CommandContext& DefaultAsyncComputeContext = Device->GetDefaultAsyncComputeContext();
		DefaultAsyncComputeContext.ReleaseCommandAllocator();
		DefaultAsyncComputeContext.ClearState();
	}

#if PLATFORM_SUPPORTS_MGPU
	// When using AFR the GPUs must be synchronized so make the current GPU wait for the N-1 GPU's frame to finish
	if (Adapter->GetMultiGPUMode() == EMultiGPUMode::MGPU_AFR)
	{
		FD3D12Fence& FrameFence = Adapter->GetFrameFence();
		const uint64 CurrentValue = FrameFence.GetCurrentFence();
		const uint64 FenceToWait = (CurrentValue == 0) ? 0 :
			CurrentValue - 1;
	
		// Broadcast to all queues on the device.
		// TODO: if we do more than texture uploading on the Copy Queue that will have to wait too.
		FrameFence.GpuWait(Device->GetAsyncCommandListManager().GetD3DCommandQueue(), FenceToWait);
		FrameFence.GpuWait(Device->GetCommandListManager().GetD3DCommandQueue(), FenceToWait);
	}

	// When using an alternating frame rendering technique with multiple GPUs the time of frame
	// delivery must be paced in order to provide a nice experience.
	if (Adapter->GetMultiGPUMode() == MGPU_AFR && RHIConsoleVariables::AFRUseFramePacing && !bLockToVsync)
	{
		if (FramePacerRunnable == nullptr)
		{
			FramePacerRunnable = new FD3D12FramePacing(Adapter);
		}
	
		FramePacerRunnable->PrePresentQueued(Device->GetCommandListManager().GetD3DCommandQueue());
	}
	else if (FramePacerRunnable)
	{
		delete FramePacerRunnable;
		FramePacerRunnable = nullptr;
	}
#endif //PLATFORM_SUPPORTS_MGPU

	const int32 SyncInterval = bLockToVsync ? RHIConsoleVariables::SyncInterval : 0;
	const bool bNativelyPresented = PresentChecked(SyncInterval);
	if (bNativelyPresented)
	{
		// Increment back buffer
		CurrentBackBufferIndex_RHIThread++;
		CurrentBackBufferIndex_RHIThread = CurrentBackBufferIndex_RHIThread % NumBackBuffers;
		BackBuffer_RHIThread = BackBuffers[CurrentBackBufferIndex_RHIThread].GetReference();
		SDRBackBuffer_RHIThread = SDRBackBuffers[CurrentBackBufferIndex_RHIThread].GetReference();
	}

	return bNativelyPresented;
}

void FD3D12Viewport::WaitForFrameEventCompletion()
{
	// Wait for the last signaled fence value.
	Fence.WaitForFence(LastSignaledValue);
}

void FD3D12Viewport::IssueFrameEvent()
{
	// Signal the fence.
	LastSignaledValue = Fence.Signal(pCommandQueue);
}

bool FD3D12Viewport::CheckHDRSupport()
{
	return GRHISupportsHDROutput && IsHDREnabled();
}

void FD3D12Viewport::AdvanceBackBufferFrame_RenderThread()
{
	bool bNeedsNativePresent = IsValidRef(CustomPresent) ? CustomPresent->NeedsNativePresent() : true;

	if (bNeedsNativePresent)
	{
		CurrentBackBufferIndex_RenderThread++;
		CurrentBackBufferIndex_RenderThread = CurrentBackBufferIndex_RenderThread % NumBackBuffers;
		BackBuffer_RenderThread = BackBuffers[CurrentBackBufferIndex_RenderThread].GetReference();
		SDRBackBuffer_RenderThread = SDRBackBuffers[CurrentBackBufferIndex_RenderThread].GetReference();
	}
}


/*=============================================================================
 *	The following RHI functions must be called from the main thread.
 *=============================================================================*/
FViewportRHIRef FD3D12DynamicRHI::RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
{
	check(IsInGameThread());

	// Use a default pixel format if none was specified	
	if (PreferredPixelFormat == EPixelFormat::PF_Unknown)
	{
		PreferredPixelFormat = EPixelFormat::PF_A2B10G10R10;
	}

	FD3D12Viewport* RenderingViewport = new FD3D12Viewport(&GetAdapter(), (HWND)WindowHandle, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat);
	RenderingViewport->Init();
	return RenderingViewport;
}

void FD3D12DynamicRHI::RHIResizeViewport(FViewportRHIParamRef ViewportRHI, uint32 SizeX, uint32 SizeY, bool bIsFullscreen)
{
	check(IsInGameThread());

	FD3D12Viewport* Viewport = FD3D12DynamicRHI::ResourceCast(ViewportRHI);
	Viewport->Resize(SizeX, SizeY, bIsFullscreen, PF_Unknown);
}

void FD3D12DynamicRHI::RHIResizeViewport(FViewportRHIParamRef ViewportRHI, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
{
	check(IsInGameThread());

	// Use a default pixel format if none was specified	
	if (PreferredPixelFormat == EPixelFormat::PF_Unknown)
	{
		PreferredPixelFormat = EPixelFormat::PF_A2B10G10R10;
	}

	FD3D12Viewport* Viewport = FD3D12DynamicRHI::ResourceCast(ViewportRHI);
	Viewport->Resize(SizeX, SizeY, bIsFullscreen, PreferredPixelFormat);
}

void FD3D12DynamicRHI::RHITick(float DeltaTime)
{
	check(IsInGameThread());

	// Check if any swap chains have been invalidated.
	auto& Viewports = GetAdapter().GetViewports();
	for (int32 ViewportIndex = 0; ViewportIndex < Viewports.Num(); ViewportIndex++)
	{
		Viewports[ViewportIndex]->ConditionalResetSwapChain(false);
	}
}

/*=============================================================================
 *	Viewport functions.
 *=============================================================================*/

void FD3D12CommandContext::RHIBeginDrawingViewport(FViewportRHIParamRef ViewportRHI, FTextureRHIParamRef RenderTargetRHI)
{
	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();
	FD3D12Viewport* Viewport = FD3D12DynamicRHI::ResourceCast(ViewportRHI);

	SCOPE_CYCLE_COUNTER(STAT_D3D12PresentTime);

	// Set the viewport.
	check(IsDefaultContext());
	check(!Adapter->GetDrawingViewport());
	Adapter->SetDrawingViewport(Viewport);

	if (RenderTargetRHI == nullptr)
	{
		RenderTargetRHI = Viewport->GetBackBuffer_RHIThread();
	}

#if LOG_VIEWPORT_EVENTS
	const FString ThreadName(FThreadManager::Get().GetThreadName(FPlatformTLS::GetCurrentThreadId()));
	UE_LOG(LogD3D12RHI, Log, TEXT("Thread %s: RHIBeginDrawingViewport (Viewport %#016llx: BackBuffer %#016llx: CmdList: %016llx)"), ThreadName.GetCharArray().GetData(), Viewport, RenderTargetRHI, CommandListHandle.CommandList());
#endif

	// Set the render target.
	const FRHIRenderTargetView RTView(RenderTargetRHI, ERenderTargetLoadAction::ELoad);
	RHISetRenderTargets(1, &RTView, nullptr, 0, nullptr);
}

void FD3D12CommandContext::RHIEndDrawingViewport(FViewportRHIParamRef ViewportRHI, bool bPresent, bool bLockToVsync)
{
	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();
	FD3D12DynamicRHI& RHI = *Device->GetOwningRHI();
	FD3D12Viewport* Viewport = FD3D12DynamicRHI::ResourceCast(ViewportRHI);

#if LOG_VIEWPORT_EVENTS
	const FString ThreadName(FThreadManager::Get().GetThreadName(FPlatformTLS::GetCurrentThreadId()));
	UE_LOG(LogD3D12RHI, Log, TEXT("Thread %s: RHIEndDrawingViewport (Viewport %#016llx: BackBuffer %#016llx: CmdList: %016llx)"), ThreadName.GetCharArray().GetData(), Viewport, Viewport->GetBackBuffer_RHIThread(), CommandListHandle.CommandList());

#endif

	SCOPE_CYCLE_COUNTER(STAT_D3D12PresentTime);

	check(Adapter->GetDrawingViewport() == Viewport);
	Adapter->SetDrawingViewport(nullptr);

	const bool bNativelyPresented = Viewport->Present(bLockToVsync);

	// Don't wait on the GPU when using SLI, let the driver determine how many frames behind the GPU should be allowed to get
	if (Adapter->GetMultiGPUMode() == MGPU_Disabled)
	{
		if (bNativelyPresented)
		{
			static const auto CFinishFrameVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.FinishCurrentFrame"));
			if (!CFinishFrameVar->GetValueOnRenderThread())
			{
				// Wait for the GPU to finish rendering the previous frame before finishing this frame.
				Viewport->WaitForFrameEventCompletion();
				Viewport->IssueFrameEvent();
			}
			else
			{
				// Finish current frame immediately to reduce latency
				Viewport->IssueFrameEvent();
				Viewport->WaitForFrameEventCompletion();
			}
		}

		// If the input latency timer has been triggered, block until the GPU is completely
		// finished displaying this frame and calculate the delta time.
		if (GInputLatencyTimer.RenderThreadTrigger)
		{
			Viewport->WaitForFrameEventCompletion();
			uint32 EndTime = FPlatformTime::Cycles();
			GInputLatencyTimer.DeltaTime = EndTime - GInputLatencyTimer.StartTime;
			GInputLatencyTimer.RenderThreadTrigger = false;
		}
	}
	else
	{
		// When using AFR we signal at RHIEndFrame due to the slate thread
	}
}

void FD3D12DynamicRHI::RHIAdvanceFrameForGetViewportBackBuffer(FViewportRHIParamRef ViewportRHI)
{
	check(IsInRenderingThread());

#if LOG_VIEWPORT_EVENTS
	const FString ThreadName(FThreadManager::Get().GetThreadName(FPlatformTLS::GetCurrentThreadId()));
	UE_LOG(LogD3D12RHI, Log, TEXT("Thread %s: RHIAdvanceFrameForGetViewportBackBuffer"), ThreadName.GetCharArray().GetData());
#endif

	// Queue a command to signal the current frame is a complete on the GPU.
	// Note: No need to handle multiple adapters yet, eventually this function will take a viewport as input.
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	auto& Adapter = ChosenAdapters[0];
	{
		check(Adapter != nullptr);
		Adapter->SignalFrameFence_RenderThread(RHICmdList);
	}

	// Advance frame so the next call to RHIGetViewportBackBuffer returns the next buffer in the swap chain.
	FD3D12Viewport* Viewport = FD3D12DynamicRHI::ResourceCast(ViewportRHI);
	Viewport->AdvanceBackBufferFrame_RenderThread();
}

FTexture2DRHIRef FD3D12DynamicRHI::RHIGetViewportBackBuffer(FViewportRHIParamRef ViewportRHI)
{
	check(IsInRenderingThread());

	const FD3D12Viewport* const Viewport = FD3D12DynamicRHI::ResourceCast(ViewportRHI);
	FRHITexture2D* const BackBuffer = Viewport->GetBackBuffer_RenderThread();
	
#if LOG_VIEWPORT_EVENTS
	const FString ThreadName(FThreadManager::Get().GetThreadName(FPlatformTLS::GetCurrentThreadId()));
	UE_LOG(LogD3D12RHI, Log, TEXT("Thread %s: RHIGetViewportBackBuffer (Viewport %#016llx: BackBuffer %#016llx)"), ThreadName.GetCharArray().GetData(), Viewport, BackBuffer);
#endif

	return BackBuffer;
}

#if defined(D3D12_WITH_DWMAPI) && D3D12_WITH_DWMAPI
#include "HideWindowsPlatformTypes.h"
#endif	//D3D12_WITH_DWMAPI
