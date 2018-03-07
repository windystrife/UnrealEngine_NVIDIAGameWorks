/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoFreeListHeap.h"


namespace nvidia {
namespace Common {

FreeListHeap::FreeListHeap(MemoryAllocator* allocator)
{
	allocator = allocator ? allocator : MemoryAllocator::getInstance();
	NV_CORE_ASSERT(allocator);
	m_allocator = allocator;

	NV_COMPILE_TIME_ASSERT(1 << MIN_SHIFT == MIN_SIZE);

	SizeT size = MIN_SIZE;
	for (IndexT i = 0; i < NUM_FREE_LISTS; i++)
	{
		SizeT numElems;
		if (size <= 64)
		{
			numElems = 16;
		}
		else
		{
			numElems = 32 >> i;
			numElems = (numElems < 8) ? 8 : numElems;
		}
		m_freeLists[i].init(size, ALIGNMENT, numElems, allocator);

		// Double the size
		size += size;
	}

	m_blocks.m_next = &m_blocks;
	m_blocks.m_previous = &m_blocks;
	m_blocks.m_totalSize = 0;
}

Void FreeListHeap::reset()
{
	for (IndexT i = 0; i < NV_COUNT_OF(m_freeLists); i++)
	{
		m_freeLists[i].reset();
	}
	// Find the block
	{
		Block* block = m_blocks.m_next;
		while (block != &m_blocks)
		{
			Block* next = block->m_next;
			m_allocator->deallocate(block, block->m_totalSize);
			block = next;
		}
	}

	m_blocks.m_next = &m_blocks;
	m_blocks.m_previous = &m_blocks;
}

FreeListHeap::Block* FreeListHeap::_findBlock(const Void* data) const
{
	if (data == NV_NULL || (SizeT(data) & SizeT(ALIGNMENT - 1)))
	{
		/// Null, or incorrect alignment
		return NV_NULL;
	}
	Block* findBlock = _getBlock(const_cast<Void*>(data));
	Block* block = m_blocks.m_next;
	while (block != &m_blocks)
	{
		if (block == findBlock)
		{
			return block;
		}
		block = block->m_next;
	}
	return NV_NULL;
}

Bool FreeListHeap::_isBlockAllocation(const Void* data, SizeT size) const
{
	Block* block = _findBlock(data);
	return block && block->m_totalSize == size + BLOCK_SIZE;
}

SizeT FreeListHeap::calcAllocationSize(const Void* data, SizeT size) const
{
	IndexT freeListIndex = calcFreeListIndex(size);
	if (freeListIndex)
	{
		return m_freeLists[freeListIndex].getElementSize();
	}
	else
	{
		Block* block = _findBlock(data);
		return block ? (block->m_totalSize - BLOCK_SIZE) : 0;
	}
}

SizeT FreeListHeap::calcAllocationSize(const Void* data) const
{
	if (data == NV_NULL || (SizeT(data) & SizeT(ALIGNMENT - 1)))
	{
		/// Null, or incorrect alignment
		return 0;
	}
	Block* block = _findBlock(data);
	if (block)
	{
		return block->m_totalSize - BLOCK_SIZE;
	}
	for (IndexT i = 0; i < NV_COUNT_OF(m_freeLists); i++)
	{
		const FreeList& freeList = m_freeLists[i];
		if (freeList.isValidAllocation(data))
		{
			return freeList.getElementSize();
		}
	}
	return 0;
}

Void* FreeListHeap::_allocateBlock(SizeT size)
{
	SizeT totalSize = size + BLOCK_SIZE;
	Block* block = (Block*)m_allocator->allocate(totalSize);
	block->m_totalSize = totalSize;

	// Add to the m_blocks list
	Block* prev = &m_blocks;
	Block* next = m_blocks.m_next;

	prev->m_next = block;
	block->m_previous = prev;
	
	next->m_previous = block;
	block->m_next = next;

	return block->getData();
}

Void FreeListHeap::_deallocateBlock(Void* data, SizeT size)
{
	NV_UNUSED(size)

	NV_CORE_ASSERT(_isBlockAllocation(data, size));
	Block* block = _getBlock(data);
	
	// Remove from linked list
	Block* prev = block->m_previous;
	Block* next = block->m_next;

	prev->m_next = next;
	next->m_previous = prev;

	m_allocator->deallocate(block, block->m_totalSize);
}

Bool FreeListHeap::isValidAllocation(const Void* data, SizeT size) const
{
	if (data == NV_NULL || (SizeT(data) & SizeT(ALIGNMENT - 1)))
	{
		/// Null, or incorrect alignment
		return false;
	}
	IndexT freeListIndex = calcFreeListIndex(size);
	if (freeListIndex >= 0)
	{
		return m_freeLists[freeListIndex].isValidAllocation(data);
	}
	return _isBlockAllocation(data, size);
}

} // namespace Common 
} // namespace nvidia

