/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include <Nv/Common/NvCoCommon.h>

#include "NvCoDx12CounterFence.h"


namespace nvidia {
namespace Common {

Dx12CounterFence::~Dx12CounterFence()
{
	if (m_event)
	{
		CloseHandle(m_event);
	}
}

Result Dx12CounterFence::init(ID3D12Device* device, UInt64 initialValue)
{
	m_currentValue = initialValue;

	NV_RETURN_ON_FAIL(device->CreateFence(m_currentValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.writeRef())));
	// Create an event handle to use for frame synchronization.
	m_event = CreateEvent(NV_NULL, FALSE, FALSE, NV_NULL);
	if (m_event == NV_NULL)
	{
		Result res = HRESULT_FROM_WIN32(GetLastError());
		return NV_FAILED(res) ? res : NV_FAIL;
	}
	return NV_OK;
}

UInt64 Dx12CounterFence::nextSignal(ID3D12CommandQueue* commandQueue)
{
	// Increment the fence value. Save on the frame - we'll know that frame is done when the fence value >= 
	m_currentValue++;
	// Schedule a Signal command in the queue.
	Result res = commandQueue->Signal(m_fence, m_currentValue);
	if (NV_FAILED(res))
	{
		NV_CORE_ALWAYS_ASSERT("Signal failed");	
	}
	return m_currentValue;
}

Void Dx12CounterFence::waitUntilCompleted(UInt64 completedValue)
{
	// You can only wait for a value that is less than or equal to the current value
	NV_CORE_ASSERT(completedValue <= m_currentValue);

	// Wait until the previous frame is finished.
	while (m_fence->GetCompletedValue() < completedValue)
	{
		// Make it signal with the current value
		NV_CORE_ASSERT_VOID_ON_FAIL(m_fence->SetEventOnCompletion(completedValue, m_event));
		WaitForSingleObject(m_event, INFINITE);
	}
}

Void Dx12CounterFence::nextSignalAndWait(ID3D12CommandQueue* commandQueue)
{
	waitUntilCompleted(nextSignal(commandQueue));
}

} // namespace Common
} // namespace nvidia
