/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_CIRCULAR_HEAP_H
#define NV_CO_CIRCULAR_HEAP_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>
#include <Nv/Common/NvCoMemory.h>
#include <Nv/Common/Util/NvCoBitUtil.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/*!
\brief A circular heap is a form of memory allocator, that can allocate blocks of memory of different sizes and 
alignments very quickly. The downside is that memory cannot be deallocated individually, but all memory up to 
and including a previous allocation can be freed. 

An invariant is that the back can never be behind the front _in the same block_. This is necessary such that 
blocks can be inserted in order when memory is exhausted. 

Allocated memory goes from m_backBlock/m_backStart to m_frontBlock, m_frontStart. m_frontEnd, is the end of the m_front block.
*/
class CircularHeap
{
	NV_CO_DECLARE_CLASS_BASE(CircularHeap);

	struct Block
	{
		NV_FORCE_INLINE UInt8* getData() { return (UInt8*)(this + 1); }
		NV_FORCE_INLINE UInt8* getEnd(SizeT blockSize) { return ((UInt8*)this) + blockSize; }
		Block* m_next;				///< The next block
	};

	struct Cursor
	{
		Block* m_block;
		UInt8* m_end;
	};

		/// Allocate a single element
	NV_FORCE_INLINE Void* allocate(SizeT size, SizeT alignment);
		/// Allocate, and produce a cursor to free by
	NV_FORCE_INLINE Void* allocate(SizeT size, SizeT align, Cursor& cursorOut);

		/// Deallocates all allocations up to and including the one that generated the cursor
	Void deallocateTo(const Cursor& cursor);

		/// Returns true if this is from a valid allocation
	Bool isValidAllocation(const Void* dataIn, SizeT size, SizeT align) const;
	Bool isValidAllocation(const Void* dataIn) const;

		/// Get the total size of each individual block allocation in bytes
	NV_FORCE_INLINE SizeT getBlockSize() const { return m_blockSize; }

		/// Calculate the used space
	SizeT calcUsedSize() const;
		/// Calculate the used size
	SizeT calcFreeSize() const;

		/// Deallocates all allocations
	Void deallocateAll();
		/// Deallocate and free any backing memory
	Void reset();

		/// Returns true if appears to be in valid state
	Bool isOk() const;

		/// Initialize. If called on an already initialized heap, the heap will be deallocated.
	Void init(SizeT blockSize, MemoryAllocator* allocator);

		/// Default Ctor
	CircularHeap() { _init(); }
		/// Ctor
	CircularHeap(SizeT blockSize, MemoryAllocator* allocator) { _init(blockSize, allocator); }
		/// Dtor
	~CircularHeap() { _deallocateAllBlocks(); }

#if NV_DEBUG
	static Void selfTest();
#endif

	protected:

	Bool _canDirectlyAllocate(SizeT size, SizeT align)
	{
		UInt8* cur = (UInt8*)(PtrDiffT(m_frontStart + align - 1) & -PtrDiffT(align));
		return (cur + size <= m_frontEnd);
	}
	
	Bool _isInBlock(Block* block, const Void* dataIn) const
	{
		const UInt8* data = (const UInt8*)dataIn;
		return data >= block->getData() && data <= block->getEnd(m_blockSize);
	}

	Void _deallocateAllBlocks();
	Void _init(SizeT blockSize, MemoryAllocator* allocator);
	Void* _allocate(SizeT size, SizeT alignment);
	Void* _allocate(SizeT size, SizeT alignment, Cursor& cursorOut);
	Void _deallocateBlocks(Block* block);
		/// Initializes setting everything to empty (doesn't free anything if already allocated)
	Void _init();

	Block* m_frontBlock;				///< The front block
	UInt8* m_frontStart;				///< The first free byte in the front block, up to m_frontEnd
	UInt8* m_frontEnd;					///< The end of the current block
	
	Block* m_backBlock;					///< The block the end is positioned in
	UInt8* m_backStart;					///< All memory between backStart/m_backBlock and m_frontStart/m_frontBlock is 'allocated'

	SizeT m_blockSize;
	MemoryAllocator* m_allocator;
};

// --------------------------------------------------------------------------
NV_FORCE_INLINE Void* CircularHeap::allocate(SizeT size, SizeT align)
{
	NV_CORE_ASSERT(align > 0 && BitUtil::isPowerTwo(align));
	// align the top
	UInt8* cur = (UInt8*)(PtrDiffT(m_frontStart + align - 1) & -PtrDiffT(align));
	if (cur + size <= m_frontEnd)
	{
		m_frontStart = cur + size;
		return cur;
	}
	return _allocate(size, align);
}
// --------------------------------------------------------------------------
Void* CircularHeap::allocate(SizeT size, SizeT align, Cursor& cursorOut)
{
	NV_CORE_ASSERT(align > 0 && BitUtil::isPowerTwo(align));
	// align the top
	UInt8* cur = (UInt8*)(PtrDiffT(m_frontStart + align - 1) & -PtrDiffT(align));
	if (cur + size <= m_frontEnd)
	{
		m_frontStart = cur + size;
		cursorOut.m_block = m_frontBlock;
		cursorOut.m_end = m_frontStart;
		return cur;
	}
	return _allocate(size, align, cursorOut);
}

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_CIRCULAR_HEAP_H