/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_FREELIST_HEAP_H
#define NV_CO_FREELIST_HEAP_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>
#include <Nv/Common/NvCoMemory.h>
#include <Nv/Common/Util/NvCoBitUtil.h>

#include "NvCoFreeList.h"

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/*! \brief A FreeListHeap is a simple and fast general purpose allocator, that uses FreeLists for small blocks, and an underlying MemoryAllocator for larger sizes.

\details A freelist heap is a general purpose heap type that for smaller block types uses freelists. For larger blocks, it
will defer to a regular allocator, passed in as a parameter to the constructor. Using freelists for smaller blocks, 
means very fast allocation/frees. For the larger blocks sizes the regular allocator is used, but the blocks are still
tracked, and will still be automatically released when the Heap goes out of scope.
*/
class FreeListHeap
{
	NV_CO_DECLARE_CLASS_BASE(FreeListHeap);

	struct Block
	{
		NV_FORCE_INLINE Void* getData();

		Block* m_next;				///< The next block
		Block* m_previous;			///< All of the non freelist held blocks are held in a doubly linked list for performance
		SizeT m_totalSize;			///< Size of this block (including the Block struct)
	};

	enum 
	{
		ALIGNMENT = MemoryAllocator::DEFAULT_ALIGNMENT,
		MIN_SIZE = ALIGNMENT,									///< Must be a power of 2
		MIN_SHIFT = 4,				
		NUM_FREE_LISTS = 6,										///< Number of freelists used
		MAX_FREE_LIST_SIZE = MIN_SIZE << (NUM_FREE_LISTS - 1),			///< Minimum size where regular blocks are used
		BLOCK_SIZE = (sizeof(Block) + MemoryAllocator::DEFAULT_ALIGNMENT - 1) & ~SizeT(MemoryAllocator::DEFAULT_ALIGNMENT - 1),
	};

		/// Allocate a single element
	NV_FORCE_INLINE Void* allocate(SizeT size);
		/// Deallocate
	NV_FORCE_INLINE Void deallocate(Void* data, SizeT size);
	
		/// Returns true if this is from a valid allocation
	Bool isValidAllocation(const Void* dataIn, SizeT size) const;
		/// Returns the allocation size, 0 if not an allocation. NOTE! This is slow as generally it searches all allocations.
	SizeT calcAllocationSize(const Void* data) const;
	SizeT calcAllocationSize(const Void* data, SizeT size) const;
		/// True if this is a valid allocation
	Bool isValidAllocation(const Void* data) const { return calcAllocationSize(data) > 0; }

		/// Deallocate and free any backing memory
	Void reset();

		/// Ctor
	FreeListHeap(MemoryAllocator* allocator = NV_NULL);
		/// Dtor
	~FreeListHeap() { reset(); }
	
		/// For a given allocation size, returns the freelist index to use, or -1 if a freelist can't be used
	NV_FORCE_INLINE static IndexT calcFreeListIndex(SizeT size);

	protected:
	NV_FORCE_INLINE static Block* _getBlock(Void* data)
	{
		NV_CORE_ASSERT(data);
		NV_CORE_ASSERT((SizeT(data) & SizeT(ALIGNMENT - 1)) == 0);
		return  (Block*)(((UInt8*)data) - BLOCK_SIZE);
	}

	Void* _allocateBlock(SizeT size);
	Void _deallocateBlock(Void* data, SizeT size);
	Bool _isBlockAllocation(const Void* data, SizeT size) const;
	Block* _findBlock(const Void* data) const;

	Block m_blocks;								///< Circular linked list of blocks which are larger than freelist handles
	FreeList m_freeLists[NUM_FREE_LISTS];		
	MemoryAllocator* m_allocator;
};


// ---------------------------------------------------------------------------
/* static */IndexT FreeListHeap::calcFreeListIndex(SizeT size)
{
	if (size <= MIN_SIZE)
	{
		return 0;
	}
	if (size <= MAX_FREE_LIST_SIZE)
	{
		Int msb = BitUtil::calcMsb(UInt32(size) >> MIN_SHIFT);
		msb += !BitUtil::isPowerTwo(size);
		return msb;
	}
	return -1;
}
// ---------------------------------------------------------------------------
NV_FORCE_INLINE Void* FreeListHeap::allocate(SizeT size)
{
	IndexT freeListIndex = calcFreeListIndex(size);
	if (freeListIndex >= 0)
	{
		return m_freeLists[freeListIndex].allocate();
	}
	else
	{
		return _allocateBlock(size);
	}
}
// ---------------------------------------------------------------------------
NV_FORCE_INLINE Void FreeListHeap::deallocate(Void* data, SizeT size)
{
	IndexT freeListIndex = calcFreeListIndex(size);
	if (freeListIndex >= 0)
	{
		return m_freeLists[freeListIndex].deallocate(data);
	}
	else
	{
		return _deallocateBlock(data, size);
	}
}

// ---------------------------------------------------------------------------
NV_FORCE_INLINE Void* FreeListHeap::Block::getData()
{
	UInt8* cur = (UInt8*)this;
	return cur + BLOCK_SIZE;
}


} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_CIRCULAR_HEAP_H