// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11RHI.cpp: Unreal D3D RHI library implementation.
=============================================================================*/

#include "D3D11RHIPrivate.h"
#include "RHIStaticStates.h"
#include "StaticBoundShaderState.h"
#include "Engine/GameViewportClient.h"

#if WITH_DX_PERF
	// For perf events
	#include "AllowWindowsPlatformTypes.h"
		#include "d3d9.h"
	#include "HideWindowsPlatformTypes.h"
#endif	//WITH_DX_PERF
#include "OneColorShader.h"

#if !UE_BUILD_SHIPPING
	#include "STaskGraph.h"
#endif

DEFINE_LOG_CATEGORY(LogD3D11RHI);

extern void UniformBufferBeginFrame();

// http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
// The following line is to favor the high performance NVIDIA GPU if there are multiple GPUs
// Has to be .exe module to be correctly detected.
// extern "C" { _declspec(dllexport) uint32 NvOptimusEnablement = 0x00000001; }

void FD3D11DynamicRHI::RHIBeginFrame()
{
	RHIPrivateBeginFrame();
	UniformBufferBeginFrame();
	GPUProfilingData.BeginFrame(this);
	PSOPrimitiveType = PT_Num;
}

template <int32 Frequency>
void ClearShaderResource(ID3D11DeviceContext* Direct3DDeviceIMContext, uint32 ResourceIndex)
{
	ID3D11ShaderResourceView* NullView = NULL;
	switch(Frequency)
	{
	case SF_Pixel:   Direct3DDeviceIMContext->PSSetShaderResources(ResourceIndex,1,&NullView); break;
	case SF_Compute: Direct3DDeviceIMContext->CSSetShaderResources(ResourceIndex,1,&NullView); break;
	case SF_Geometry:Direct3DDeviceIMContext->GSSetShaderResources(ResourceIndex,1,&NullView); break;
	case SF_Domain:  Direct3DDeviceIMContext->DSSetShaderResources(ResourceIndex,1,&NullView); break;
	case SF_Hull:    Direct3DDeviceIMContext->HSSetShaderResources(ResourceIndex,1,&NullView); break;
	case SF_Vertex:  Direct3DDeviceIMContext->VSSetShaderResources(ResourceIndex,1,&NullView); break;
	};
}

void FD3D11DynamicRHI::ClearState()
{
	StateCache.ClearState();

	FMemory::Memzero(CurrentResourcesBoundAsSRVs, sizeof(CurrentResourcesBoundAsSRVs));
	for (int32 Frequency = 0; Frequency < SF_NumFrequencies; Frequency++)
	{
		MaxBoundShaderResourcesIndex[Frequency] = INDEX_NONE;
	}
}

// WaveWorks Start
void FD3D11DynamicRHI::CacheWaveWorksQuadTreeState(const TArray<uint32>& ShaderInputMappings)
{
	StateCache.CacheWaveWorksShaderInput(ShaderInputMappings, *GDynamicRHI->RHIGetDefaultContext()->RHIGetWaveWorksQuadTreeShaderInput());
}
// WaveWorks End

void GetMipAndSliceInfoFromSRV(ID3D11ShaderResourceView* SRV, int32& MipLevel, int32& NumMips, int32& ArraySlice, int32& NumSlices)
{
	MipLevel = -1;
	NumMips = -1;
	ArraySlice = -1;
	NumSlices = -1;

	if (SRV)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		SRV->GetDesc(&SRVDesc);
		switch (SRVDesc.ViewDimension)
		{			
			case D3D11_SRV_DIMENSION_TEXTURE1D:
				MipLevel	= SRVDesc.Texture1D.MostDetailedMip;
				NumMips		= SRVDesc.Texture1D.MipLevels;
				break;
			case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
				MipLevel	= SRVDesc.Texture1DArray.MostDetailedMip;
				NumMips		= SRVDesc.Texture1DArray.MipLevels;
				ArraySlice	= SRVDesc.Texture1DArray.FirstArraySlice;
				NumSlices	= SRVDesc.Texture1DArray.ArraySize;
				break;
			case D3D11_SRV_DIMENSION_TEXTURE2D:
				MipLevel	= SRVDesc.Texture2D.MostDetailedMip;
				NumMips		= SRVDesc.Texture2D.MipLevels;
				break;
			case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
				MipLevel	= SRVDesc.Texture2DArray.MostDetailedMip;
				NumMips		= SRVDesc.Texture2DArray.MipLevels;
				ArraySlice	= SRVDesc.Texture2DArray.FirstArraySlice;
				NumSlices	= SRVDesc.Texture2DArray.ArraySize;
				break;
			case D3D11_SRV_DIMENSION_TEXTURE2DMS:
				MipLevel	= 0;
				NumMips		= 1;
				break;
			case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
				MipLevel	= 0;
				NumMips		= 1;
				ArraySlice	= SRVDesc.Texture2DMSArray.FirstArraySlice;
				NumSlices	= SRVDesc.Texture2DMSArray.ArraySize;
				break;
			case D3D11_SRV_DIMENSION_TEXTURE3D:
				MipLevel	= SRVDesc.Texture3D.MostDetailedMip;
				NumMips		= SRVDesc.Texture3D.MipLevels;
				break;
			case D3D11_SRV_DIMENSION_TEXTURECUBE:
				MipLevel = SRVDesc.TextureCube.MostDetailedMip;
				NumMips		= SRVDesc.TextureCube.MipLevels;
				break;
			case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
				MipLevel	= SRVDesc.TextureCubeArray.MostDetailedMip;
				NumMips		= SRVDesc.TextureCubeArray.MipLevels;
				ArraySlice	= SRVDesc.TextureCubeArray.First2DArrayFace;
				NumSlices	= SRVDesc.TextureCubeArray.NumCubes;
				break;
			case D3D11_SRV_DIMENSION_BUFFER:
			case D3D11_SRV_DIMENSION_BUFFEREX:
			default:
				break;
		}
	}
}

void FD3D11DynamicRHI::CheckIfSRVIsResolved(ID3D11ShaderResourceView* SRV)
{
#if CHECK_SRV_TRANSITIONS
	if (IsRunningRHIInSeparateThread() || !SRV)
	{
		return;
	}

	static const auto CheckSRVCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.CheckSRVTransitions"));	
	if (!CheckSRVCVar->GetValueOnRenderThread())
	{
		return;
	}

	ID3D11Resource* SRVResource = nullptr;
	SRV->GetResource(&SRVResource);

	int32 MipLevel;
	int32 NumMips;
	int32 ArraySlice;
	int32 NumSlices;
	GetMipAndSliceInfoFromSRV(SRV, MipLevel, NumMips, ArraySlice, NumSlices);

	//d3d uses -1 to mean 'all mips'
	int32 LastMip = MipLevel + NumMips - 1;
	int32 LastSlice = ArraySlice + NumSlices - 1;

	TArray<FUnresolvedRTInfo> RTInfoArray;
	check(UnresolvedTargetsConcurrencyGuard.Increment() == 1);
	UnresolvedTargets.MultiFind(SRVResource, RTInfoArray);
	check(UnresolvedTargetsConcurrencyGuard.Decrement() == 0);

	for (int32 InfoIndex = 0; InfoIndex < RTInfoArray.Num(); ++InfoIndex)
	{
		const FUnresolvedRTInfo& RTInfo = RTInfoArray[InfoIndex];
		int32 RTLastMip = RTInfo.MipLevel + RTInfo.NumMips - 1;		
		ensureMsgf((MipLevel == -1 || NumMips == -1) || (LastMip < RTInfo.MipLevel || MipLevel > RTLastMip), TEXT("SRV is set to read mips in range %i to %i.  Target %s is unresolved for mip %i"), MipLevel, LastMip, *RTInfo.ResourceName.ToString(), RTInfo.MipLevel);
		ensureMsgf(NumMips != -1, TEXT("SRV is set to read all mips.  Target %s is unresolved for mip %i"), *RTInfo.ResourceName.ToString(), RTInfo.MipLevel);

		int32 RTLastSlice = RTInfo.ArraySlice + RTInfo.ArraySize - 1;
		ensureMsgf((ArraySlice == -1 || LastSlice == -1) || (LastSlice < RTInfo.ArraySlice || ArraySlice > RTLastSlice), TEXT("SRV is set to read slices in range %i to %i.  Target %s is unresolved for mip %i"), ArraySlice, LastSlice, *RTInfo.ResourceName.ToString(), RTInfo.ArraySlice);
		ensureMsgf(ArraySlice == -1 || NumSlices != -1, TEXT("SRV is set to read all slices.  Target %s is unresolved for slice %i"));
	}
	SRVResource->Release();
#endif
}

extern int32 GEnableDX11TransitionChecks;
template <EShaderFrequency ShaderFrequency>
void FD3D11DynamicRHI::InternalSetShaderResourceView(FD3D11BaseShaderResource* Resource, ID3D11ShaderResourceView* SRV, int32 ResourceIndex, FName SRVName, FD3D11StateCache::ESRV_Type SrvType)
{
	// Check either both are set, or both are null.
	check((Resource && SRV) || (!Resource && !SRV));
	CheckIfSRVIsResolved(SRV);

	//avoid state cache crash
	if (!((Resource && SRV) || (!Resource && !SRV)))
	{
		//UE_LOG(LogRHI, Warning, TEXT("Bailing on InternalSetShaderResourceView on resource: %i, %s"), ResourceIndex, *SRVName.ToString());
		return;
	}

	if (Resource)
	{		
		const EResourceTransitionAccess CurrentAccess = Resource->GetCurrentGPUAccess();
		const bool bAccessPass = CurrentAccess == EResourceTransitionAccess::EReadable || (CurrentAccess == EResourceTransitionAccess::ERWBarrier && !Resource->IsDirty()) || CurrentAccess == EResourceTransitionAccess::ERWSubResBarrier;
		ensureMsgf((GEnableDX11TransitionChecks == 0) || bAccessPass || Resource->GetLastFrameWritten() != PresentCounter, TEXT("Shader resource %s is not GPU readable.  Missing a call to RHITransitionResources()"), *SRVName.ToString());
	}

	FD3D11BaseShaderResource*& ResourceSlot = CurrentResourcesBoundAsSRVs[ShaderFrequency][ResourceIndex];
	int32& MaxResourceIndex = MaxBoundShaderResourcesIndex[ShaderFrequency];

	if (Resource)
	{
		// We are binding a new SRV.
		// Update the max resource index to the highest bound resource index.
		MaxResourceIndex = FMath::Max(MaxResourceIndex, ResourceIndex);
		ResourceSlot = Resource;
	}
	else if (ResourceSlot != nullptr)
	{
		// Unbind the resource from the slot.
		ResourceSlot = nullptr;

		// If this was the highest bound resource...
		if (MaxResourceIndex == ResourceIndex)
		{
			// Adjust the max resource index downwards until we
			// hit the next non-null slot, or we've run out of slots.
			do
			{
				MaxResourceIndex--;
			}
			while (MaxResourceIndex >= 0 && CurrentResourcesBoundAsSRVs[ShaderFrequency][MaxResourceIndex] == nullptr);
		} 
	}

	// Set the SRV we have been given (or null).
	StateCache.SetShaderResourceView<ShaderFrequency>(SRV, ResourceIndex, SrvType);
}

template <EShaderFrequency ShaderFrequency>
void FD3D11DynamicRHI::ClearShaderResourceViews(FD3D11BaseShaderResource* Resource)
{
	int32 MaxIndex = MaxBoundShaderResourcesIndex[ShaderFrequency];
	for (int32 ResourceIndex = MaxIndex; ResourceIndex >= 0; --ResourceIndex)
	{
		if (CurrentResourcesBoundAsSRVs[ShaderFrequency][ResourceIndex] == Resource)
		{
			// Unset the SRV from the device context
			InternalSetShaderResourceView<ShaderFrequency>(nullptr, nullptr, ResourceIndex, NAME_None);
		}
	}
}

void FD3D11DynamicRHI::ConditionalClearShaderResource(FD3D11BaseShaderResource* Resource)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D11ClearShaderResourceTime);
	check(Resource);
	ClearShaderResourceViews<SF_Vertex>(Resource);
	ClearShaderResourceViews<SF_Hull>(Resource);
	ClearShaderResourceViews<SF_Domain>(Resource);
	ClearShaderResourceViews<SF_Pixel>(Resource);
	ClearShaderResourceViews<SF_Geometry>(Resource);
	ClearShaderResourceViews<SF_Compute>(Resource);
}

template <EShaderFrequency ShaderFrequency>
void FD3D11DynamicRHI::ClearAllShaderResourcesForFrequency()
{
	int32 MaxIndex = MaxBoundShaderResourcesIndex[ShaderFrequency];
	for (int32 ResourceIndex = MaxIndex; ResourceIndex >= 0; --ResourceIndex)
	{
		if (CurrentResourcesBoundAsSRVs[ShaderFrequency][ResourceIndex] != nullptr)
		{
			// Unset the SRV from the device context
			InternalSetShaderResourceView<ShaderFrequency>(nullptr, nullptr, ResourceIndex, NAME_None);
		}
	}
}

void FD3D11DynamicRHI::ClearAllShaderResources()
{
	ClearAllShaderResourcesForFrequency<SF_Vertex>();
	ClearAllShaderResourcesForFrequency<SF_Hull>();
	ClearAllShaderResourcesForFrequency<SF_Domain>();
	ClearAllShaderResourcesForFrequency<SF_Geometry>();
	ClearAllShaderResourcesForFrequency<SF_Pixel>();
	ClearAllShaderResourcesForFrequency<SF_Compute>();
}

void FD3DGPUProfiler::BeginFrame(FD3D11DynamicRHI* InRHI)
{
	CurrentEventNode = NULL;
	check(!bTrackingEvents);
	check(!CurrentEventNodeFrame); // this should have already been cleaned up and the end of the previous frame

	// latch the bools from the game thread into our private copy
	bLatchedGProfilingGPU = GTriggerGPUProfile;
	bLatchedGProfilingGPUHitches = GTriggerGPUHitchProfile;
	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI 
	bLatchedRequestProfileForStatUnitVxgi = bRequestProfileForStatUnitVxgi;
	bRequestProfileForStatUnitVxgi = false;
#endif
	// NVCHANGE_END: Add VXGI
	if (bLatchedGProfilingGPUHitches)
	{
		bLatchedGProfilingGPU = false; // we do NOT permit an ordinary GPU profile during hitch profiles
	}

	// if we are starting a hitch profile or this frame is a gpu profile, then save off the state of the draw events
	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI 
	if (bLatchedGProfilingGPU || (!bPreviousLatchedGProfilingGPUHitches && bLatchedGProfilingGPUHitches) || bLatchedRequestProfileForStatUnitVxgi)
#else
	// NVCHANGE_END: Add VXGI
	if (bLatchedGProfilingGPU || (!bPreviousLatchedGProfilingGPUHitches && bLatchedGProfilingGPUHitches))
		// NVCHANGE_BEGIN: Add VXGI
#endif
		// NVCHANGE_END: Add VXGI
	{
		bOriginalGEmitDrawEvents = GEmitDrawEvents;
	}

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI 
	if (bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches || bLatchedRequestProfileForStatUnitVxgi)
#else
	// NVCHANGE_END: Add VXGI
	if (bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches)
		// NVCHANGE_BEGIN: Add VXGI
#endif
		// NVCHANGE_END: Add VXGI
	{
		if (bLatchedGProfilingGPUHitches && GPUHitchDebounce)
		{
			// if we are doing hitches and we had a recent hitch, wait to recover
			// the reasoning is that collecting the hitch report may itself hitch the GPU
			GPUHitchDebounce--; 
		}
		else
		{
			GEmitDrawEvents = true;  // thwart an attempt to turn this off on the game side
			bTrackingEvents = true;
			CurrentEventNodeFrame = new FD3D11EventNodeFrame(InRHI);
			CurrentEventNodeFrame->StartFrame();
		}
	}
	else if (bPreviousLatchedGProfilingGPUHitches)
	{
		// hitch profiler is turning off, clear history and restore draw events
		GPUHitchEventNodeFrames.Empty();
		GEmitDrawEvents = bOriginalGEmitDrawEvents;
	}
	bPreviousLatchedGProfilingGPUHitches = bLatchedGProfilingGPUHitches;

	// Skip timing events when using SLI, they will not be accurate anyway
	if (GNumActiveGPUsForRendering == 1)
	{
		FrameTiming.StartTiming();
	}

	if (GEmitDrawEvents)
	{
		PushEvent(TEXT("FRAME"), FColor(0, 255, 0, 255));
	}
}

void FD3D11DynamicRHI::RHIEndFrame()
{
	GPUProfilingData.EndFrame();
	CurrentComputeShader = nullptr;
}

// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI 
void GatherVxgiGPUTimes(FGPUProfilerEventNode* Node, int32 Depth, float& OutVxgiWorldSpaceTime, float& OutVxgiScreenSpaceTime)
{
	//UE_LOG(LogD3D11RHI, Display, TEXT("%d: %s"), Depth, *Node->Name);

	if (Node->Name.StartsWith(TEXT("VXGI")))
	{
		Node->TimingResult = Node->GetTiming() * 1000.f;

		if (Node->Name == TEXT("VXGITracing") || Node->Name == TEXT("VXGICompositeDiffuse"))
			OutVxgiScreenSpaceTime += Node->TimingResult;
		else
			OutVxgiWorldSpaceTime += Node->TimingResult;
	}
	else
	{
		int32 NumChildren = Node->Children.Num();
		for (int32 Child = 0; Child < NumChildren; Child++)
		{
			GatherVxgiGPUTimes(Node->Children[Child], Depth + 1, OutVxgiWorldSpaceTime, OutVxgiScreenSpaceTime);
		}
	}
}
#endif
// NVCHANGE_END: Add VXGI

void FD3DGPUProfiler::EndFrame()
{
	if (GEmitDrawEvents)
	{
		PopEvent();
	}

	// Skip timing events when using SLI, they will not be accurate anyway
	if (GNumActiveGPUsForRendering == 1)
	{
		FrameTiming.EndTiming();
	}

	// Skip timing events when using SLI, as they will block the GPU and we want maximum throughput
	// Stat unit GPU time is not accurate anyway with SLI
	if (FrameTiming.IsSupported() && GNumActiveGPUsForRendering == 1)
	{
		uint64 GPUTiming = FrameTiming.GetTiming();
		uint64 GPUFreq = FrameTiming.GetTimingFrequency();
		GGPUFrameTime = FMath::TruncToInt( double(GPUTiming) / double(GPUFreq) / FPlatformTime::GetSecondsPerCycle() );
	}
	else
	{
		GGPUFrameTime = 0;
	}

	// if we have a frame open, close it now.
	if (CurrentEventNodeFrame)
	{
		CurrentEventNodeFrame->EndFrame();
	}

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI 
	check(!bTrackingEvents || bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches || bLatchedRequestProfileForStatUnitVxgi);
#else
	// NVCHANGE_END: Add VXGI
	check(!bTrackingEvents || bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches);
	// NVCHANGE_BEGIN: Add VXGI
#endif
	// NVCHANGE_END: Add VXGI

	check(!bTrackingEvents || CurrentEventNodeFrame);
	if (bLatchedGProfilingGPU)
	{
		if (bTrackingEvents)
		{
			GEmitDrawEvents = bOriginalGEmitDrawEvents;
			UE_LOG(LogD3D11RHI, Warning, TEXT(""));
			UE_LOG(LogD3D11RHI, Warning, TEXT(""));
			CurrentEventNodeFrame->DumpEventTree();
			GTriggerGPUProfile = false;
			bLatchedGProfilingGPU = false;

			if (RHIConfig::ShouldSaveScreenshotAfterProfilingGPU()
				&& GEngine->GameViewport)
			{
				GEngine->GameViewport->Exec( NULL, TEXT("SCREENSHOT"), *GLog);
			}
		}
	}
	else if (bLatchedGProfilingGPUHitches)
	{
		//@todo this really detects any hitch, even one on the game thread.
		// it would be nice to restrict the test to stalls on D3D, but for now...
		// this needs to be out here because bTrackingEvents is false during the hitch debounce
		static double LastTime = -1.0;
		double Now = FPlatformTime::Seconds();
		if (bTrackingEvents)
		{
			/** How long, in seconds a frame much be to be considered a hitch **/
			const float HitchThreshold = RHIConfig::GetGPUHitchThreshold();
			float ThisTime = Now - LastTime;
			bool bHitched = (ThisTime > HitchThreshold) && LastTime > 0.0 && CurrentEventNodeFrame;
			if (bHitched)
			{
				UE_LOG(LogD3D11RHI, Warning, TEXT("*******************************************************************************"));
				UE_LOG(LogD3D11RHI, Warning, TEXT("********** Hitch detected on CPU, frametime = %6.1fms"),ThisTime * 1000.0f);
				UE_LOG(LogD3D11RHI, Warning, TEXT("*******************************************************************************"));

				for (int32 Frame = 0; Frame < GPUHitchEventNodeFrames.Num(); Frame++)
				{
					UE_LOG(LogD3D11RHI, Warning, TEXT(""));
					UE_LOG(LogD3D11RHI, Warning, TEXT(""));
					UE_LOG(LogD3D11RHI, Warning, TEXT("********** GPU Frame: Current - %d"),GPUHitchEventNodeFrames.Num() - Frame);
					GPUHitchEventNodeFrames[Frame].DumpEventTree();
				}
				UE_LOG(LogD3D11RHI, Warning, TEXT(""));
				UE_LOG(LogD3D11RHI, Warning, TEXT(""));
				UE_LOG(LogD3D11RHI, Warning, TEXT("********** GPU Frame: Current"));
				CurrentEventNodeFrame->DumpEventTree();

				UE_LOG(LogD3D11RHI, Warning, TEXT("*******************************************************************************"));
				UE_LOG(LogD3D11RHI, Warning, TEXT("********** End Hitch GPU Profile"));
				UE_LOG(LogD3D11RHI, Warning, TEXT("*******************************************************************************"));
				if (GEngine->GameViewport)
				{
					GEngine->GameViewport->Exec( NULL, TEXT("SCREENSHOT"), *GLog);
				}

				GPUHitchDebounce = 5; // don't trigger this again for a while
				GPUHitchEventNodeFrames.Empty(); // clear history
			}
			else if (CurrentEventNodeFrame) // this will be null for discarded frames while recovering from a recent hitch
			{
				/** How many old frames to buffer for hitch reports **/
				static const int32 HitchHistorySize = 4;

				if (GPUHitchEventNodeFrames.Num() >= HitchHistorySize)
				{
					GPUHitchEventNodeFrames.RemoveAt(0);
				}
				GPUHitchEventNodeFrames.Add((FD3D11EventNodeFrame*)CurrentEventNodeFrame);
				CurrentEventNodeFrame = NULL;  // prevent deletion of this below; ke kept it in the history
			}
		}
		LastTime = Now;
	}

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI 
	else if (bLatchedRequestProfileForStatUnitVxgi)
	{
		// Use local variables for accumulation because the members are read from a different thread
		float WorldSpaceTime = 0.f;
		float ScreenSpaceTime = 0.f;

		if (CurrentEventNodeFrame)
		{
			int32 NumEvents = CurrentEventNodeFrame->EventTree.Num();
			for (int32 Event = 0; Event < NumEvents; Event++)
			{
				GatherVxgiGPUTimes(CurrentEventNodeFrame->EventTree[Event], 0, WorldSpaceTime, ScreenSpaceTime);
			}
		}

		VxgiWorldSpaceTime = WorldSpaceTime;
		VxgiScreenSpaceTime = ScreenSpaceTime;
	}
#endif
	// NVCHANGE_END: Add VXGI

	bTrackingEvents = false;
	delete CurrentEventNodeFrame;
	CurrentEventNodeFrame = NULL;
}

float FD3D11EventNode::GetTiming()
{
	float Result = 0;

	if (Timing.IsSupported())
	{
		// Get the timing result and block the CPU until it is ready
		const uint64 GPUTiming = Timing.GetTiming(true);
		const uint64 GPUFreq = Timing.GetTimingFrequency();

		Result = double(GPUTiming) / double(GPUFreq);
	}

	return Result;
}

void FD3D11DynamicRHI::RHIBeginScene()
{
	// Increment the frame counter. INDEX_NONE is a special value meaning "uninitialized", so if
	// we hit it just wrap around to zero.
	SceneFrameCounter++;
	if (SceneFrameCounter == INDEX_NONE)
	{
		SceneFrameCounter++;
	}

	static auto* ResourceTableCachingCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("rhi.ResourceTableCaching"));
	if (ResourceTableCachingCvar == NULL || ResourceTableCachingCvar->GetValueOnAnyThread() == 1)
	{
		ResourceTableFrameCounter = SceneFrameCounter;
	}
}

void FD3D11DynamicRHI::RHIEndScene()
{
	ResourceTableFrameCounter = INDEX_NONE;
}

void FD3DGPUProfiler::PushEvent(const TCHAR* Name, FColor Color)
{
#if NV_AFTERMATH
	if(GDX11NVAfterMathEnabled)
	{
		uint32 CRC = FCrc::StrCrc32<TCHAR>(Name);	
		
		if (CachedStrings.Num() > 10000)
		{
			CachedStrings.Empty(10000);
		}

		if (CachedStrings.Find(CRC) == nullptr)
		{
			CachedStrings.Emplace(CRC, FString(Name));
		}
		PushPopStack.Push(CRC);

		auto* DeviceContext = D3D11RHI->GetDeviceContext();
		GFSDK_Aftermath_DX11_SetEventMarker(DeviceContext, &PushPopStack[0], PushPopStack.Num() * sizeof(uint32));
	}
#endif

#if WITH_DX_PERF
	D3DPERF_BeginEvent(Color.DWColor(),Name);
#endif

	FGPUProfiler::PushEvent(Name, Color);
}

void FD3DGPUProfiler::PopEvent()
{
#if NV_AFTERMATH
	if (GDX11NVAfterMathEnabled)
	{
		PushPopStack.Pop(false);
	}
#endif

#if WITH_DX_PERF
	D3DPERF_EndEvent();
#endif

	FGPUProfiler::PopEvent();
}

extern CORE_API bool GIsGPUCrashed;
bool FD3DGPUProfiler::CheckGpuHeartbeat() const
{
#if NV_AFTERMATH
	if (GDX11NVAfterMathEnabled)
	{
		auto* DeviceContext = D3D11RHI->GetDeviceContext();
		GFSDK_Aftermath_Status Status;
		GFSDK_Aftermath_ContextData ContextDataOut;
		auto Result = GFSDK_Aftermath_DX11_GetData(1, &DeviceContext, &ContextDataOut, &Status);
		if (Result == GFSDK_Aftermath_Result_Success)
		{
			if (Status != GFSDK_Aftermath_Status_Active)
			{
				GIsGPUCrashed = true;
				const TCHAR* AftermathReason[] = { TEXT("Active"), TEXT("Timeout"), TEXT("OutOfMemory"), TEXT("PageFault"), TEXT("Unknown") };
				check(Status < 5);
				UE_LOG(LogRHI, Error, TEXT("[Aftermath] Status: %s"), AftermathReason[Status]);
				UE_LOG(LogRHI, Error, TEXT("[Aftermath] GPU Stack Dump"));
				uint32 NumCRCs = ContextDataOut.markerSize / sizeof(uint32);
				uint32* Data = (uint32*)ContextDataOut.markerData;
				for (uint32 i = 0; i < NumCRCs; i++)
				{
					const FString* Frame = CachedStrings.Find(Data[i]);
					if (Frame != nullptr)
					{
						UE_LOG(LogRHI, Error, TEXT("[Aftermath] %i: %s"), i, *(*Frame));
					}
				}
				UE_LOG(LogRHI, Error, TEXT("[Aftermath] GPU Stack Dump"));
				return false;
			}
		}
	}
#endif
	return true;
}

/** Start this frame of per tracking */
void FD3D11EventNodeFrame::StartFrame()
{
	EventTree.Reset();
	DisjointQuery.StartTracking();
	RootEventTiming.StartTiming();
}

/** End this frame of per tracking, but do not block yet */
void FD3D11EventNodeFrame::EndFrame()
{
	RootEventTiming.EndTiming();
	DisjointQuery.EndTracking();
}

float FD3D11EventNodeFrame::GetRootTimingResults()
{
	double RootResult = 0.0f;
	if (RootEventTiming.IsSupported())
	{
		const uint64 GPUTiming = RootEventTiming.GetTiming(true);
		const uint64 GPUFreq = RootEventTiming.GetTimingFrequency();

		RootResult = double(GPUTiming) / double(GPUFreq);
	}

	return (float)RootResult;
}

void FD3D11EventNodeFrame::LogDisjointQuery()
{
	UE_LOG(LogRHI, Warning, TEXT("%s"),
		DisjointQuery.IsResultValid()
		? TEXT("Profiled range was continuous.")
		: TEXT("Profiled range was disjoint!  GPU switched to doing something else while profiling."));
}

void UpdateBufferStats(TRefCountPtr<ID3D11Buffer> Buffer, bool bAllocating)
{
	D3D11_BUFFER_DESC Desc;
	Buffer->GetDesc(&Desc);

	const bool bUniformBuffer = !!(Desc.BindFlags & D3D11_BIND_CONSTANT_BUFFER);
	const bool bIndexBuffer = !!(Desc.BindFlags & D3D11_BIND_INDEX_BUFFER);
	const bool bVertexBuffer = !!(Desc.BindFlags & D3D11_BIND_VERTEX_BUFFER);

	if (bAllocating)
	{
		if (bUniformBuffer)
		{
			INC_MEMORY_STAT_BY(STAT_UniformBufferMemory,Desc.ByteWidth);
		}
		else if (bIndexBuffer)
		{
			INC_MEMORY_STAT_BY(STAT_IndexBufferMemory,Desc.ByteWidth);
		}
		else if (bVertexBuffer)
		{
			INC_MEMORY_STAT_BY(STAT_VertexBufferMemory,Desc.ByteWidth);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_StructuredBufferMemory,Desc.ByteWidth);
		}
	}
	else
	{ //-V523
		if (bUniformBuffer)
		{
			DEC_MEMORY_STAT_BY(STAT_UniformBufferMemory,Desc.ByteWidth);
		}
		else if (bIndexBuffer)
		{
			DEC_MEMORY_STAT_BY(STAT_IndexBufferMemory,Desc.ByteWidth);
		}
		else if (bVertexBuffer)
		{
			DEC_MEMORY_STAT_BY(STAT_VertexBufferMemory,Desc.ByteWidth);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_StructuredBufferMemory,Desc.ByteWidth);
		}
	}
}

#ifndef PLATFORM_IMPLEMENTS_FASTVRAMALLOCATOR
	#define PLATFORM_IMPLEMENTS_FASTVRAMALLOCATOR		0
#endif

#if !PLATFORM_IMPLEMENTS_FASTVRAMALLOCATOR
FFastVRAMAllocator* FFastVRAMAllocator::GetFastVRAMAllocator()
{
	static FFastVRAMAllocator FastVRAMAllocatorSingleton;
	return &FastVRAMAllocatorSingleton;
}
#endif

