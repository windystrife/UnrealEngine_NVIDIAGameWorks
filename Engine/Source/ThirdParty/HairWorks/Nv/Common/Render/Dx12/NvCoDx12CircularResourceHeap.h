/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_DX12_CIRCULAR_RESOURCE_HEAP_H
#define NV_CO_DX12_CIRCULAR_RESOURCE_HEAP_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>

#include <Nv/Common/NvCoComPtr.h>
#include <Nv/Common/Container/NvCoArray.h>
#include <Nv/Common/NvCoFreeList.h>
#include <Nv/Common/Container/NvCoQueue.h>

#include "NvCoDx12CounterFence.h"

// Dx12 types
#include <d3d12.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common { 

/*! \brief The Dx12CircularResourceHeap is a heap that is suited for size constrained real-time resources allocation that 
is transitory in nature. It is designed to allocate resources which are used and discarded, often used where in 
previous versions of DirectX the 'DISCARD' flag was used. 

The idea is to have a heap which chunks of resource can be allocated, and used for GPU execution, 
and that the heap is able through the addSync/updateCompleted idiom is able to track when the usage of the resources is 
completed allowing them to be reused. The heap is arranged as circularly, with new allocations made from the front, and the back 
being updated as the GPU updating the back when it is informed anything using prior parts of the heap have completed. In this 
arrangement all the heap between the back and the front can be thought of as in use or potentially in use by the GPU. All the heap 
from the front back around to the back, is free and can be allocated from. It is the responsibility of the user of the Heap to make 
sure the invariant holds, but in most normal usage it does so simply. 

Another feature of the heap is that it does not require upfront knowledge of how big a heap is needed. The backing resources will be expanded 
dynamically with requests as needed. The only requirement is that know single request can be larger than m_blockSize specified in the Desc 
used to initialize the heap. This is because all the backing resources are allocated to a single size. This limitation means the Dx12CircularResourceHeap
may not be the best use for example for uploading a texture - because it's design is really around transitory uploads or write backs, and so more suited 
to constant buffers, vertex buffer, index buffers and the like. 

To upload a texture at program startup it is most likely better to use a Dx12ResourceScopeManager.

### The addSync/updateCompleted Idiom

Lots of classes in Nv/Common/Platform/Dx12 use the addSync/update idiom. The idiom enables a class to track GPU progress fairly simply. The 
two methods are generally called at a fairly low frequency - say once a frame. The addSync method is given a signalValue that should be the
value generated from nextSignal on the Dx12Fence that is passed on construction of the type. Calling addSync means when this value is hit 
ALL previous heap allocations will no longer be used. Thus in practice usage looks something like

\code{.cpp}

typedef Dx12CircularResourceHeap Heap;

Heap::Cursor cursor = heap.allocateVertexBuffer(sizeof(Vertex) * numVerts);
Memory:copy(cursor.m_position, verts, sizeof(Vertex) * numVerts);

// Do a command using the GPU handle 
m_commandList->...
// Do another command using the GPU handle

m_commandList->...

// Execute the command list on the command queue
{
	ID3D12CommandList* lists[] = { m_commandList };
	m_commandQueue->ExecuteCommandLists(NV_COUNT_OF(lists), lists);
}

// Add a sync point
const UInt64 signalValue = m_fence.nextSignal(m_commandQueue);
heap.addSync(signalValue)

// The cursors cannot be used anymore

// At some later point call updateCompleted. This will see where the GPU is at, and make resources available that the GPU no longer accesses.
heap.updateCompleted();

\endcode

### Implementation

Front and back can be in the same block, but ONLY if back is behind front, because we have to always be able to insert
new blocks in front of front. So it must be possible to do an block insertion between the two of them.

|--B---F-----|     |----------|

When B and F are on top of one another it means there is nothing in the list. NOTE this also means that a move of front can never place it
top of the back. 

https://msdn.microsoft.com/en-us/library/windows/desktop/dn899125%28v=vs.85%29.aspx
https://msdn.microsoft.com/en-us/library/windows/desktop/mt426646%28v=vs.85%29.aspx
*/
class Dx12CircularResourceHeap
{
	protected:
	struct Block;

	NV_CO_DECLARE_CLASS_BASE(Dx12CircularResourceHeap);

		/// The alignment used for VERTEX_BUFFER allocations 
		/// Strictly speaking it seems the hardware can handle 4 byte alignment, but since often in use
		/// data will be copied from CPU memory to the allocation, using 16 byte alignment is superior as allows
		/// significantly faster memcpy.
		/// The sample that shows sizeof(float) - 4 bytes is appropriate is at the link below.
		/// https://msdn.microsoft.com/en-us/library/windows/desktop/mt426646%28v=vs.85%29.aspx
	enum 
	{
		VERTEX_BUFFER_ALIGNMENT = 16,
	};

	struct Desc 
	{
		Void init()
		{
			{
				D3D12_HEAP_PROPERTIES& props = m_heapProperties;

				props.Type = D3D12_HEAP_TYPE_UPLOAD;
				props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				props.CreationNodeMask = 1;
				props.VisibleNodeMask = 1;
			}
			m_heapFlags = D3D12_HEAP_FLAG_NONE;
			m_initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
			m_blockSize = 0;
		}

		D3D12_HEAP_PROPERTIES m_heapProperties;
		D3D12_HEAP_FLAGS m_heapFlags;
		D3D12_RESOURCE_STATES m_initialState;
		SizeT m_blockSize;
	};

	/// Cursor position
	struct Cursor
	{
			/// Get GpuHandle 
		NV_FORCE_INLINE D3D12_GPU_VIRTUAL_ADDRESS getGpuHandle() const { return m_block->m_resource->GetGPUVirtualAddress() + SizeT(m_position - m_block->m_start); }
			/// Must have a block and position
		NV_FORCE_INLINE Bool isValid() const { return m_block != NV_NULL; }
			/// Calculate the offset into the underlying resource
		NV_FORCE_INLINE SizeT getOffset() const { return SizeT(m_position - m_block->m_start); }
			/// Get the underlying resource
		NV_FORCE_INLINE ID3D12Resource* getResource() const { return m_block->m_resource; }

		Block* m_block; 		///< The block index
		UInt8* m_position;		///< The current position
	};

		/// Get the desc used to initialize the heap
	NV_FORCE_INLINE const Desc& getDesc() const { return m_desc; }

		/// Must be called before used
		/// Block size must be at least as large as the _largest_ thing allocated
		/// Also note depending on alignment of a resource allocation, the block size might also need to take into account the 
		/// maximum alignment use. It is a REQUIREMENT that a newly allocated resource block is large enough to hold any 
		/// allocation taking into account the alignment used.
	Result init(ID3D12Device* device, const Desc& desc, Dx12CounterFence* fence);
	
		/// Get the block size
	NV_FORCE_INLINE SizeT getBlockSize() const { return m_desc.m_blockSize; }

		/// Allocate constant buffer of specified size
	Cursor allocate(SizeT size, SizeT alignment);

		/// Allocate a constant buffer
	NV_FORCE_INLINE Cursor allocateConstantBuffer(SizeT size) { return allocate(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT); }
		/// Allocate a vertex buffer
	NV_FORCE_INLINE Cursor allocateVertexBuffer(SizeT size) { return allocate(size, VERTEX_BUFFER_ALIGNMENT); }

		/// Create filled in constant buffer
	NV_FORCE_INLINE Cursor newConstantBuffer(const Void* data, SizeT size) { Cursor cursor = allocateConstantBuffer(size); Memory::copy(cursor.m_position, data, size); return cursor; }
		/// Create in filled in constant buffer 
	template <typename T>
	NV_FORCE_INLINE Cursor newConstantBuffer(const T& in) { return newConstantBuffer(&in, sizeof(T)); }

		/// Look where the GPU has got to and release anything not currently used
	Void updateCompleted();
		/// Add a sync point - meaning that when this point is hit in the queue
		/// all of the resources up to this point will no longer be used.
	Void addSync(UInt64 signalValue);

		/// Get the gpu address of this cursor
	D3D12_GPU_VIRTUAL_ADDRESS getGpuHandle(const Cursor& cursor) const  { return cursor.m_block->m_resource->GetGPUVirtualAddress() + SizeT(cursor.m_position - cursor.m_block->m_start);  }

		/// Ctor
	Dx12CircularResourceHeap();
		/// Dtor
	~Dx12CircularResourceHeap();

	protected:
	
	struct Block
	{
		ID3D12Resource* m_resource;		///< The mapped resource
		UInt8* m_start;					///< Once created the resource is mapped to here
		Block* m_next;					///< Points to next block in the list 					
	};
	struct PendingEntry
	{
		UInt64 m_completedValue;			///< The value when this is completed
		Cursor m_cursor;					///< the cursor at that point
	};
	Void _freeBlockListResources(const Block* block);
		/// Create a new block (with associated resource), do not add the block list
	Block* _newBlock();

	Block* m_blocks;					///< Circular singly linked list of block. NV_NULL initially
	FreeList m_blockFreeList;			///< Free list of actual allocations of blocks
	Queue<PendingEntry> m_pendingQueue;	///< Holds the list of pending positions. When the fence value is greater than the value on the queue entry, the entry is done.

	// Allocation is made from the front, and freed from the back.
	Cursor m_back;						///< Current back position. 
	Cursor m_front;						///< Current front position. 
	
	Desc m_desc;						///< Describes the heap

	Dx12CounterFence* m_fence;			///< The fence to use
	ID3D12Device* m_device;				///< The device that resources will be constructed on
};

} // namespace Common
} // namespace nvidia

/** @} */

#endif // NV_CO_DX12_CIRCULAR_RESOURCE_HEAP_H
