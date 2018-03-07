// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Implementation of D3D12 fence functions

//-----------------------------------------------------------------------------
//	Include Files
//-----------------------------------------------------------------------------
#include "D3D12RHIPrivate.h"

void FD3D12Fence::InternalSignal(ID3D12CommandQueue* pCommandQueue, uint64 FenceToSignal)
{
	check(pCommandQueue != nullptr);

#if DEBUG_FENCES
	UE_LOG(LogD3D12RHI, Log, TEXT("*** GPU SIGNAL (CmdQueue: %016llX) Fence: %016llX (%s), Value: %u ***"), pCommandQueue, FenceCore->GetFence(), *GetName().ToString(), FenceToSignal);
#endif

	VERIFYD3D12RESULT(pCommandQueue->Signal(FenceCore->GetFence(), FenceToSignal));

	LastSignaledFence = FenceToSignal;
}

void FD3D12Fence::WaitForFence(uint64 FenceValue)
{
	check(FenceCore);
	if (!IsFenceComplete(FenceValue))
	{
#if DEBUG_FENCES
		UE_LOG(LogD3D12RHI, Log, TEXT("*** CPU WAIT Fence: %016llX (%s), Value: %u ***"), FenceCore->GetFence(), *GetName().ToString(), FenceValue);
#endif

		SCOPE_CYCLE_COUNTER(STAT_D3D12WaitForFenceTime);
		{
			// Multiple threads can be using the same FD3D12Fence (texture streaming).
			FScopeLock Lock(&WaitForFenceCS);

			// We must wait.  Do so with an event handler so we don't oversleep.
			VERIFYD3D12RESULT(FenceCore->GetFence()->SetEventOnCompletion(FenceValue, FenceCore->GetCompletionEvent()));

			// Wait for the event to complete (the event is automatically reset afterwards)
			const uint32 WaitResult = WaitForSingleObject(FenceCore->GetCompletionEvent(), INFINITE);
			check(0 == WaitResult);
		}

		checkf(FenceValue <= FenceCore->GetFence()->GetCompletedValue(), TEXT("Wait for fence value (%llu) failed! Last completed value is still %llu."), FenceValue, FenceCore->GetFence()->GetCompletedValue());

		// Refresh the completed fence value
		GetLastCompletedFence();
	}
}