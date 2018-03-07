/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include <Nv/Common/NvCoCommon.h>

#include "NvCoDx12CircularResourceHeap.h"


namespace nvidia {
namespace Common {

Dx12CircularResourceHeap::Dx12CircularResourceHeap():
	m_fence(NV_NULL),
	m_device(NV_NULL),
	m_blockFreeList(sizeof(Block), NV_ALIGN_OF(Block), 16, NV_NULL),
	m_blocks(NV_NULL)
{
	m_back.m_block = NV_NULL;
	m_back.m_position = NV_NULL;
	m_front.m_block = NV_NULL;
	m_front.m_position = NV_NULL;
}

Dx12CircularResourceHeap::~Dx12CircularResourceHeap()
{
	_freeBlockListResources(m_blocks);
}

Void Dx12CircularResourceHeap::_freeBlockListResources(const Block* start)
{
	if (start)
	{
		{
			ID3D12Resource* resource = start->m_resource;
			resource->Unmap(0, NV_NULL);
			resource->Release();
		}
		for (Block* block = start->m_next; block != start; block = block->m_next)
		{
			ID3D12Resource* resource = block->m_resource;
			resource->Unmap(0, NV_NULL);
			resource->Release();
		}
	} 
}

Result Dx12CircularResourceHeap::init(ID3D12Device* device, const Desc& desc, Dx12CounterFence* fence)
{
	NV_CORE_ASSERT(m_blocks == NV_NULL); 
	NV_CORE_ASSERT(desc.m_blockSize > 0);

	m_fence = fence;
	m_desc = desc;
	m_device = device;

	return NV_OK;
}

Void Dx12CircularResourceHeap::addSync(UInt64 signalValue)
{
	NV_CORE_ASSERT(signalValue == m_fence->getCurrentValue());
	PendingEntry entry;
	entry.m_completedValue = signalValue;
	entry.m_cursor = m_front;
	m_pendingQueue.pushBack(entry); 
}

Void Dx12CircularResourceHeap::updateCompleted()
{
	const UInt64 completedValue = m_fence->getCompletedValue();
	while (!m_pendingQueue.isEmpty())
	{
		const PendingEntry& entry = m_pendingQueue.front();
		if (entry.m_completedValue <= completedValue)
		{
			m_back = entry.m_cursor;
			m_pendingQueue.popFront();
		}
		else
		{
			break;
		}
	}
}

Dx12CircularResourceHeap::Block* Dx12CircularResourceHeap::_newBlock()
{
	D3D12_RESOURCE_DESC desc;

	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = 0;
	desc.Width = m_desc.m_blockSize;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ComPtr<ID3D12Resource> resource;
	Result res = m_device->CreateCommittedResource(&m_desc.m_heapProperties, m_desc.m_heapFlags, &desc, m_desc.m_initialState, NV_NULL, IID_PPV_ARGS(resource.writeRef()));
	if (NV_FAILED(res))
	{
		NV_CORE_ALWAYS_ASSERT("Resource allocation failed");
		return NV_NULL;
	}

	UInt8* data = NV_NULL;
	if (m_desc.m_heapProperties.Type == D3D12_HEAP_TYPE_READBACK)
	{
	}
	else
	{
		// Map it, and keep it mapped
		resource->Map(0, NV_NULL, (Void**)&data);
	}

	// We have no blocks -> so lets allocate the first
	Block* block = (Block*)m_blockFreeList.allocate();
	block->m_next = NV_NULL;

	block->m_resource = resource.detach();
	block->m_start = data;
	return block;
}

Dx12CircularResourceHeap::Cursor Dx12CircularResourceHeap::allocate(SizeT size, SizeT alignment)
{
	const SizeT blockSize = getBlockSize();

	NV_CORE_ASSERT(size <= blockSize);

	// If nothing is allocated add the first block
	if (m_blocks == NV_NULL)
	{
		Block* block = _newBlock();
		if (!block)
		{
			Cursor cursor = {};
			return cursor;
		}
		m_blocks = block;
		// Make circular
		block->m_next = block;

		// Point front and back to same position, as currently it is all free
		m_back = { block, block->m_start };
		m_front = m_back;
	}

	// If front and back are in the same block then front MUST be ahead of back (as that defined as 
	// an invariant and is required for block insertion to be possible
	Block* block = m_front.m_block;

	// Check the invariant
	NV_CORE_ASSERT(block != m_back.m_block || m_front.m_position >= m_back.m_position);

	{
		UInt8* cur = (UInt8*)((SizeT(m_front.m_position) + alignment - 1) & ~(alignment - 1));
		// Does the the allocation fit?
		if (cur + size <= block->m_start + blockSize)
		{
			// It fits
			// Move the front forward
			m_front.m_position = cur + size;
			Cursor cursor = { block, cur };
			return cursor;
		}
	}

	// Okay I can't fit into current block...
	 
	// If the next block contains front, we need to add a block, else we can use that block
	if (block->m_next == m_back.m_block)
	{
		Block* newBlock = _newBlock();
		// Insert into the list
		newBlock->m_next = block->m_next;
		block->m_next = newBlock;
	}
	
	// Use the block we are going to add to
	block = block->m_next;
	UInt8* cur = (UInt8*)((SizeT(block->m_start) + alignment - 1) & ~(alignment - 1));
	// Does the the allocation fit?
	if (cur + size > block->m_start + blockSize)
	{
		NV_CORE_ALWAYS_ASSERT("Couldn't fit into a free block(!) Alignment breaks it?");
		Cursor cursor = {};
		return cursor;
	}
	// It fits
	// Move the front forward
	m_front.m_block = block;
	m_front.m_position = cur + size;
	Cursor cursor = { block, cur };
	return cursor;
}

} // namespace Common
} // namespace nvidia
