/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoFreeList.h"

namespace nvidia {
namespace Common {

FreeList::~FreeList()
{
	_deallocateBlocks(m_activeBlocks);
	_deallocateBlocks(m_freeBlocks);
}

Void FreeList::_init()
{
	m_top = NV_NULL;
	m_end = NV_NULL;
	
	m_activeBlocks = NV_NULL;
	m_freeBlocks = NV_NULL;

	m_freeElements = NV_NULL;

	m_elementSize = 0;
	m_alignment = 1;
	m_blockSize = 0;
	m_blockAllocationSize = 0;
	m_allocator = NV_NULL;
}

Void FreeList::_init(SizeT elementSize, SizeT alignment, SizeT elemsPerBlock, MemoryAllocator* allocator)
{
	allocator = allocator ? allocator : MemoryAllocator::getInstance();
	NV_CORE_ASSERT(allocator);
	m_allocator = allocator;

	alignment = (alignment < sizeof(Void*)) ? sizeof(Void*) : alignment;

	// Alignment must be a power of 2
	NV_CORE_ASSERT(((alignment - 1) & alignment) == 0);

	// The elementSize must at least be 
	elementSize = (elementSize >= alignment) ? elementSize : alignment;
	m_blockSize = elementSize * elemsPerBlock;
	m_elementSize = elementSize;
	m_alignment = alignment;

	// Calculate the block size neeed, correcting for alignment 
	const SizeT alignedBlockSize = (alignment <= MemoryAllocator::DEFAULT_ALIGNMENT) ? 
		_calcAlignedBlockSize(MemoryAllocator::DEFAULT_ALIGNMENT) : 
		_calcAlignedBlockSize(alignment);

	// Make the block struct size aligned
	m_blockAllocationSize = m_blockSize + alignedBlockSize;

	m_top = NV_NULL;
	m_end = NV_NULL;

	m_activeBlocks = NV_NULL;
	m_freeBlocks = NV_NULL;			///< Blocks that there are no allocations in

	m_freeElements = NV_NULL;
}

Void FreeList::init(SizeT elementSize, SizeT alignment, SizeT elemsPerBlock, MemoryAllocator* allocator)
{
	_deallocateBlocks(m_activeBlocks);
	_deallocateBlocks(m_freeBlocks);
	_init(elementSize, alignment, elemsPerBlock, allocator);
}

Void FreeList::_deallocateBlocks(Block* block)
{
	while (block)
	{
		Block* next = block->m_next;

#ifdef NV_CO_FREE_LIST_INIT_MEM
		Memory::set(block, 0xfd, m_blockAllocationSize);
#endif

		m_allocator->deallocate(block, m_blockAllocationSize);
		block = next;
	}
}

Bool FreeList::isValidAllocation(const Void* dataIn) const
{
	UInt8* data = (UInt8*)dataIn;

	Block* block = m_activeBlocks;
	while (block)
	{
		UInt8* start = block->m_data;
		UInt8* end = start + m_blockSize;

		if (data >= start && data < end)
		{
			// Check it's aligned correctly
			if ((data - start) % m_elementSize)
			{
				return false;
			}

			// Non allocated data is between top and end
			if (data >= m_top && data < m_end)
			{
				return false;
			}

			// It can't be in the free list
			Element* ele = m_freeElements;
			while (ele)
			{
				if (ele == (Element*)data)
				{
					return false;
				}

				ele = ele->m_next;
			}
			return true;
		}

		block = block->m_next;
	}
	// It's not in an active block -> it cannot be a valid allocation
	return false;
}

Void* FreeList::_allocate()
{
	Block* block = m_freeBlocks;
	if (block)
	{
		/// Remove from the free blocks
		m_freeBlocks = block->m_next;
	}
	else
	{
		block = (Block*)m_allocator->allocate(m_blockAllocationSize);
		if (!block)
		{
			// Allocation failed... doh
			return NV_NULL;
		}
		// Do the alignment
		{
			SizeT fix = (SizeT(block) + sizeof(Block) + m_alignment - 1) & ~(m_alignment - 1);
			block->m_data = (UInt8*)fix;
		}
	} 

	// Attach to the active blocks
	block->m_next = m_activeBlocks;
	m_activeBlocks = block;

	// Set up top and end
	m_end = block->m_data + m_blockSize;

	// Return the first element
	UInt8* element = block->m_data;
	m_top = element + m_elementSize;

	NV_CO_FREE_LIST_INIT_ALLOCATE(element)

	return element;
}

Void FreeList::deallocateAll()
{
	Block* block = m_activeBlocks;
	if (block)
	{
		// Find the end block
		while (block->m_next) 
		{
#ifdef NV_CO_FREE_LIST_INIT_MEM
			Memory::set(block->m_data, 0xfd, m_blockSize);	
#endif
			block = block->m_next;
		}
		// Attach to the freeblocks
		block->m_next = m_freeBlocks;
		// The list is now all freelists
		m_freeBlocks = m_activeBlocks;
		// There are no active blocks
		m_activeBlocks = NV_NULL;
	}

	m_top = NV_NULL;
	m_end = NV_NULL;
}

Void FreeList::reset()
{
	_deallocateBlocks(m_activeBlocks);
	_deallocateBlocks(m_freeBlocks);

	m_top = NV_NULL;
	m_end = NV_NULL;

	m_activeBlocks = NV_NULL;
	m_freeBlocks = NV_NULL;			

	m_freeElements = NV_NULL;
}


Void FreeList::_initAllocate(Void* mem)
{
	Memory::set(mem, 0xcd, m_elementSize);
}

Void FreeList::_initDeallocate(Void* mem)
{
	Memory::set(mem, 0xfd, m_elementSize);
}

} // namespace Common 
} // namespace nvidia

