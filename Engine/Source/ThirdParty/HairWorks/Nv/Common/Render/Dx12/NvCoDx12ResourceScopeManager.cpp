/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include <Nv/Common/NvCoCommon.h>

#include "NvCoDx12ResourceScopeManager.h"

namespace nvidia {
namespace Common {

Dx12ResourceScopeManager::Dx12ResourceScopeManager():
	m_fence(NV_NULL),
	m_device(NV_NULL)
{
}

Dx12ResourceScopeManager::~Dx12ResourceScopeManager()
{
	while (!m_entryQueue.isEmpty())
	{
		Entry& entry = m_entryQueue.front();
		entry.m_resource->Release();
		m_entryQueue.popFront();
	}
}

Result Dx12ResourceScopeManager::init(ID3D12Device* device, Dx12CounterFence* fence)
{
	m_fence = fence;
	m_device = device;
	return NV_OK;
}

Void Dx12ResourceScopeManager::addSync(UInt64 signalValue)
{	
	NV_UNUSED(signalValue)

	NV_CORE_ASSERT(m_fence->getCurrentValue() == signalValue);
}

Void Dx12ResourceScopeManager::updateCompleted()
{
	const UInt64 completedValue = m_fence->getCompletedValue();
	if (!m_entryQueue.isEmpty())
	{
		const Entry& entry = m_entryQueue.front();
		if (entry.m_completedValue >= completedValue)
		{
			return;
		}
		entry.m_resource->Release();
		m_entryQueue.popFront();
	}
}

ID3D12Resource* Dx12ResourceScopeManager::newUploadResource(const D3D12_RESOURCE_DESC& resourceDesc, const D3D12_CLEAR_VALUE* clearValue)
{
	D3D12_HEAP_PROPERTIES heapProps;
	{
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;
	}
	const D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;

	const D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_GENERIC_READ;

	ComPtr<ID3D12Resource> resource;
	Result res = m_device->CreateCommittedResource(&heapProps, heapFlags, &resourceDesc, initialState, clearValue, IID_PPV_ARGS(resource.writeRef()));
	if (NV_FAILED(res)) return NV_NULL;

	// Get the current fence count
	const UInt64 completedValue = m_fence->getCurrentValue();

	Entry& entry = m_entryQueue.expandBack();
	entry.m_completedValue = completedValue;
	entry.m_resource = resource.detach();
	return entry.m_resource;
}

/* static */Void Dx12ResourceScopeManager::copy(const D3D12_SUBRESOURCE_DATA& src, SizeT rowSizeInBytes, Int numRows, Int numSlices, const D3D12_MEMCPY_DEST& dst)
{
	for (Int i = 0; i < numSlices; ++i)
	{
		UInt8* dstSlice = reinterpret_cast<UInt8*>(dst.pData) + dst.SlicePitch * i;
		const UInt8* srcSlice = reinterpret_cast<const UInt8*>(src.pData) + src.SlicePitch * i;
		for (Int j = 0; j < numRows; ++j)
		{
			Memory::copy(dstSlice + dst.RowPitch * j, srcSlice + src.RowPitch * j, rowSizeInBytes);
		}
	}
}

Result Dx12ResourceScopeManager::upload(ID3D12GraphicsCommandList* commandList, const Void* srcDataIn, ID3D12Resource* targetResource, D3D12_RESOURCE_STATES targetState)
{
	// Get the targetDesc
	const D3D12_RESOURCE_DESC targetDesc = targetResource->GetDesc();
	// Ensure it is just a regular buffer
	NV_CORE_ASSERT(targetDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER);
	NV_CORE_ASSERT(targetDesc.Layout == D3D12_TEXTURE_LAYOUT_ROW_MAJOR);

	// The buffer size is the width 
	const SizeT bufferSize = SizeT(targetDesc.Width);

	D3D12_RESOURCE_DESC uploadDesc = targetDesc;
	uploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	// Create the upload resource
	ID3D12Resource* uploadResource = newUploadResource(uploadDesc);

	// Map it and copy 
	{
		UInt8* uploadMapped;
		NV_RETURN_ON_FAIL(uploadResource->Map(0, NV_NULL, (Void**)&uploadMapped));
		Memory::copy(uploadMapped, srcDataIn, bufferSize);
		uploadResource->Unmap(0, NV_NULL);
	}

	// Add the copy
	commandList->CopyBufferRegion(targetResource, 0, uploadResource, 0, bufferSize);

	// Add the barrier
	{
		D3D12_RESOURCE_BARRIER barrier = {};

		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		D3D12_RESOURCE_TRANSITION_BARRIER& transition = barrier.Transition;
		transition.pResource = targetResource;
		transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		transition.StateAfter = targetState;
		transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		commandList->ResourceBarrier(1, &barrier);
	}

	return NV_OK;
}

Void Dx12ResourceScopeManager::add(ID3D12Resource* resource)
{
	NV_CORE_ASSERT(resource);

	// Get the current fence count
	const UInt64 completedValue = m_fence->getCurrentValue();

	Entry& entry = m_entryQueue.expandBack();
	entry.m_completedValue = completedValue;
	resource->AddRef();
	entry.m_resource = resource;
}

Result Dx12ResourceScopeManager::uploadWithState(ID3D12GraphicsCommandList* commandList, const Void* srcDataIn, Dx12Resource& target, D3D12_RESOURCE_STATES targetState)
{
	// make sure we are in the correct initial state
	if (target.getState() != D3D12_RESOURCE_STATE_COPY_DEST)
	{
		Dx12BarrierSubmitter submitter(commandList);
		target.transition(D3D12_RESOURCE_STATE_COPY_DEST, submitter);
	}
	// Do the upload
	NV_RETURN_ON_FAIL(upload(commandList, srcDataIn, target.getResource(), targetState));
	target.setState(targetState);
	return NV_OK;
}


} // namespace Common
} // namespace nvidia
