/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoCircularHeap.h"

#if NV_DEBUG
#	include <Nv/Common/Random/NvCoRandomGenerator.h>
#	include <Nv/Common/NvCoUniquePtr.h>
#	include <Nv/Common/Container/NvCoArray.h>
#endif

namespace nvidia {
namespace Common {

void CircularHeap::_deallocateAllBlocks()
{
	Block* start = m_frontBlock;
	if (start)
	{
		Block* cur = start->m_next;
		m_allocator->deallocate(start, m_blockSize);
		while ( cur != start)
		{
			Block* next = cur->m_next;
			m_allocator->deallocate(cur, m_blockSize);
			cur = next;
		}
	}
}

Void CircularHeap::_init()
{
	m_frontBlock = NV_NULL;
	m_frontStart = NV_NULL;
	m_frontEnd = NV_NULL;

	m_backBlock = NV_NULL;
	m_backStart = NV_NULL;

	m_blockSize = 0;
	m_allocator = NV_NULL;
}

Void CircularHeap::_init(SizeT blockSize, MemoryAllocator* allocator)
{
	allocator = allocator ? allocator : MemoryAllocator::getInstance();
	NV_CORE_ASSERT(allocator);
	m_allocator = allocator;

	NV_CORE_ASSERT(blockSize > 16);
	m_blockSize = blockSize;

	m_frontBlock = NV_NULL;
	m_frontStart = NV_NULL;
	m_frontEnd = NV_NULL;

	m_backBlock = NV_NULL;
	m_backStart = NV_NULL;
}

Void CircularHeap::init(SizeT blockSize, MemoryAllocator* allocator)
{
	_deallocateAllBlocks();
	_init(blockSize, allocator);
}

Void CircularHeap::deallocateAll()
{
	if (m_frontBlock)
	{
		m_frontStart = m_frontBlock->getData();
		m_frontEnd = m_frontBlock->getEnd(m_blockSize);
		
		m_backBlock = m_frontBlock;
		m_backStart = m_frontStart;
	}
}

Bool CircularHeap::isValidAllocation(const Void* dataIn, SizeT size, SizeT align) const
{
	NV_CORE_ASSERT(BitUtil::isPowerTwo(align));
	if ((SizeT(dataIn) & (align - 1)) != 0)
	{
		// The alignment is wrong
		return false;
	}
	UInt8* data = (UInt8*)dataIn;
	if (m_backBlock == m_frontBlock)
	{
		// Special case if front and back are the same block
		NV_CORE_ASSERT(m_backStart <= m_frontStart);
		return data >= m_backStart && data + size <= m_frontStart;
	}
	
	// Handle the back block
	if (data >= m_backStart && data + size <= m_backBlock->getEnd(m_blockSize))
	{
		return true;
	}	
	// Handle the middle blocks
	{
		Block* cur = m_backBlock->m_next;
		while (cur != m_frontBlock)
		{
			if (data >= cur->getData() && data + size <= cur->getEnd(m_blockSize))
			{
				return true;
			}
			cur = cur->m_next;
		}

	}
	// Handle the front block
	return data >= m_frontBlock->getData() && data + size <= m_frontStart;
}

Bool CircularHeap::isOk() const
{
	if (m_frontBlock == NV_NULL)
	{
		return m_backBlock == NV_NULL &&
			m_backStart == NV_NULL &&
			m_frontStart == NV_NULL && 
			m_frontEnd == NV_NULL;
	}

	if (m_frontEnd != m_frontBlock->getEnd(m_blockSize))
	{
		return false;
	}

	if (m_frontBlock == m_backBlock)
	{
		return _isInBlock(m_backBlock, m_backStart) && 
			   _isInBlock(m_frontBlock, m_frontStart) && m_backStart <= m_frontStart && m_frontStart <= m_frontEnd;
	}

	// Make sure it loops
	{
		Bool hitFront = false;
	
		Block* cur = m_backBlock->m_next;
		while (cur != m_backBlock)
		{
			hitFront |= (cur == m_frontBlock);
			cur = cur->m_next;
		}
		if (!hitFront)
		{
			return false;
		}
	}

	return _isInBlock(m_backBlock, m_backStart) && _isInBlock(m_frontBlock, m_frontStart);
}


Bool CircularHeap::isValidAllocation(const Void* dataIn) const
{
	UInt8* data = (UInt8*)dataIn;
	if (m_backBlock == m_frontBlock)
	{
		// Special case if front and back are the same block
		NV_CORE_ASSERT(m_backStart <= m_frontStart);
		return data >= m_backStart && data <= m_frontStart;
	}

	// Handle the back block
	if (data >= m_backStart && data <= m_backBlock->getEnd(m_blockSize))
	{
		return true;
	}
	// Handle the middle blocks
	{
		Block* cur = m_backBlock->m_next;
		while (cur != m_frontBlock)
		{
			if (data >= cur->getData() && data <= cur->getEnd(m_blockSize))
			{
				return true;
			}
			// next block
			cur = cur->m_next;
		}

	}
	// Handle the front block
	return data >= m_frontBlock->getData() && data <= m_frontStart;
}

Void* CircularHeap::_allocate(SizeT size, SizeT align, Cursor& cursorOut)
{
	NV_CORE_ASSERT(align > 0 && BitUtil::isPowerTwo(align));
	NV_CORE_ASSERT(!_canDirectlyAllocate(size, align));

	if (!m_frontBlock)
	{
		m_frontBlock = (Block*)m_allocator->allocate(m_blockSize);
		m_frontBlock->m_next = m_frontBlock;

		m_backBlock = m_frontBlock;
		m_backStart = m_backBlock->getData();
	}
	else
	{
		Block* freeBlock = m_frontBlock->m_next;
		if (freeBlock == m_backBlock)
		{
			// Create new block
			freeBlock = (Block*)m_allocator->allocate(m_blockSize);
			freeBlock->m_next = m_frontBlock->m_next;
			m_frontBlock->m_next = freeBlock;
		}
		m_frontBlock = freeBlock;
	}

	UInt8* frontStart = m_frontBlock->getData();
	m_frontEnd = m_frontBlock->getEnd(m_blockSize);

	// Do the allocation
	UInt8* cur = (UInt8*)(PtrDiffT(frontStart + align - 1) & -PtrDiffT(align));
	NV_CORE_ASSERT(cur + size <= m_frontEnd);

	m_frontStart = cur + size;

	cursorOut.m_block = m_frontBlock;
	cursorOut.m_end = m_frontStart;
	 
	return cur;
}

Void* CircularHeap::_allocate(SizeT size, SizeT align)
{
	Cursor cursor;
	return _allocate(size, align, cursor);
}

Void CircularHeap::deallocateTo(const Cursor& cursor)
{
	NV_CORE_ASSERT(_isInBlock(cursor.m_block, cursor.m_end));
	NV_CORE_ASSERT(isValidAllocation(cursor.m_end));

	Block* cursorBlock = cursor.m_block;
	if (cursorBlock == m_backBlock)
	{
		NV_CORE_ASSERT(cursor.m_end >= m_backStart);
		NV_CORE_ASSERT(m_backBlock != m_frontBlock || cursor.m_end <= m_frontStart);
		m_backStart = cursor.m_end;
		return;
	}

	// Search for cursor from current back position
	Block* cur = m_backBlock->m_next;
	while (cur != cursorBlock && cur != m_frontBlock)
	{
		cur = cur->m_next;
	}

	if (cur != cursorBlock)
	{
		NV_CORE_ASSERT(cur == m_frontBlock);
		// We hit the front block without hitting the cursor block
		NV_CORE_ALWAYS_ASSERT("Cursor seems invalid");
		return;
	}

	if (cur == m_frontBlock)
	{
		NV_CORE_ASSERT(cursor.m_end <= m_frontStart);
	}
	
	m_backStart = cursor.m_end;
	m_backBlock = cursor.m_block;
}

Void CircularHeap::reset()
{
	_deallocateAllBlocks();

	m_frontBlock = NV_NULL;
	m_frontStart = NV_NULL;
	m_frontEnd = NV_NULL;

	m_backBlock = NV_NULL;
	m_backStart = NV_NULL;
}

SizeT CircularHeap::calcUsedSize() const
{
	if (m_frontBlock == m_backBlock)
	{
		return SizeT(m_frontStart - m_backStart);
	}
	else
	{
		SizeT size = SizeT(m_backBlock->getEnd(m_blockSize) - m_backStart);
		for (Block* cur = m_backBlock->m_next; cur != m_frontBlock; cur = cur->m_next)
		{
			size += m_blockSize - sizeof(Block);
		}
		return size + SizeT(m_frontStart - m_frontBlock->getData());
	}
}

SizeT CircularHeap::calcFreeSize() const
{
	if (m_frontBlock == m_backBlock)
	{
		return SizeT(m_frontEnd - m_frontStart) + SizeT(m_backStart - m_backBlock->getData());
	}
	else
	{
		SizeT size = SizeT(m_frontEnd - m_frontStart);
		for (Block* cur = m_frontBlock->m_next; cur != m_backBlock; cur = cur->m_next)
		{
			size += m_blockSize - sizeof(Block);
		}
		return size + SizeT(m_backStart - m_backBlock->getData());
	}
}

#if NV_DEBUG
/* static */Void CircularHeap::selfTest()
{
	UniquePtr<RandomGenerator> rand(RandomGenerator::create(0x1344513));

	Array<CircularHeap::Cursor> cursors;

	CircularHeap heap(100, NV_NULL);

	SizeT maxAlloced = 0;

	for (Int i = 0; i < 100000; i++)
	{
		NV_CORE_ASSERT(heap.isOk());

		NV_CORE_ASSERT(cursors.getSize() == 0 || heap.calcUsedSize() > 0);

		SizeT totalAllocSize = heap.calcUsedSize();
		maxAlloced = (totalAllocSize > maxAlloced) ? totalAllocSize : maxAlloced;

		// Check all the allocations are valid
		for (IndexT j = 0; j < cursors.getSize(); j++)
		{
			NV_CORE_ASSERT(heap.isValidAllocation(cursors[j].m_end));
		}

		const Int op = rand->nextInt32InRange(0, 10);
		
		if (op == 0)
		{
			if (rand->nextInt32InRange(0, 10) == 0)
			{
				heap.reset();
			}
			else
			{
				heap.deallocateAll();
			}
			cursors.clear();
		}
		else if (op == 1)
		{
			// Calculate num to free
			if (cursors.getSize() > 0)
			{
				Int numFree = rand->nextInt32InRange(0, Int32(cursors.getSize() - 1));
				const Cursor& cursor = cursors[numFree];
				heap.deallocateTo(cursor);
				cursors.removeRange(0, numFree + 1);
			}
		}
		else
		{
			Int allocSize = rand->nextInt32InRange(1, 30);
			CircularHeap::Cursor cursor;

			Int align = 1 << rand->nextInt32InRange(0, 5);

			Void* data = heap.allocate(allocSize, align, cursor);
			NV_CORE_ASSERT((SizeT(data) & SizeT(align - 1)) == 0);

			NV_CORE_ASSERT(heap.isValidAllocation(data, allocSize, align));

			cursors.pushBack(cursor);
		}
	}
}
#endif


} // namespace Common 
} // namespace nvidia

