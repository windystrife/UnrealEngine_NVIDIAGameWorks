/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_DX12_DESCRIPTOR_CACHE_H
#define NV_CO_DX12_DESCRIPTOR_CACHE_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>

#include <Nv/Common/NvCoComPtr.h>
#include <Nv/Common/Container/NvCoArray.h>
#include <Nv/Common/NvCoFreeList.h>
#include <Nv/Common/Container/NvCoQueue.h>
#include <Nv/Common/Container/NvCoSlice.h>

#include "NvCoDx12CounterFence.h"

// Dx12 types
#include <d3d12.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common { 

/*!
\brief Describes a set of D3D12_CPU_DESCRIPTOR_HANDLE. The handles can either be specified explicitly as in a list, or 
as a continuous run of descriptors by specifying a start descriptor and a size. 
*/
struct Dx12DescriptorSet
{
	typedef Dx12DescriptorSet ThisType;
	typedef SizeT Hash;

	enum Type
	{
		TYPE_UNKNOWN,
		TYPE_UAV,
		TYPE_CBV,
		TYPE_SRV,
		TYPE_OTHER,
		TYPE_COUNT_OF,
	};

		/// Calculate a hash 
	Hash calcHash() const; 

		/// ==
	Bool operator==(const ThisType& rhs) const;
		/// !=
	NV_FORCE_INLINE Bool operator!=(const ThisType& rhs) const { return !(*this == rhs); }

		/// True if there is an intersection (ie contains one or more handles in both)
	Bool hasIntersection(const ThisType& rhs, Int descriptorSize) const;
		/// Returns true if this set references the handle
	Bool hasHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle, Int descriptorSize) const;

		/// Set
	Void set(const D3D12_CPU_DESCRIPTOR_HANDLE* handles, IndexT size) { m_descriptors = handles; m_size = size; m_base.ptr = 0; }
	Void set(D3D12_CPU_DESCRIPTOR_HANDLE base, IndexT size) { m_descriptors = NV_NULL; m_size = size; m_base = base; }

		/// True if it's a run (ie defined by m_base and m_size only)
	NV_FORCE_INLINE Bool isRun() const { return m_size > 0 && m_descriptors == NV_NULL; }
		/// True if a list (ie members are defined in m_descriptors array)
	NV_FORCE_INLINE Bool isList() const { return m_size > 0 && m_descriptors != NV_NULL; }

		/// True if doesn't contain anything
	NV_FORCE_INLINE Bool isEmpty() const { return m_size <= 0; }

		/// Get the total amount of descriptors
	NV_FORCE_INLINE IndexT getSize() const { return m_size; }
		/// Get the type
	NV_FORCE_INLINE Type getType() const { return m_type; }

		/// True if contains a null
	Bool hasNull() const;

		/// Get the initial entry
	NV_FORCE_INLINE D3D12_CPU_DESCRIPTOR_HANDLE getInitial() const { NV_CORE_ASSERT(m_size > 0); return m_descriptors ? m_descriptors[0] : m_base; }

		/// Ctor with 
	template <SizeT SIZE>
	NV_FORCE_INLINE Dx12DescriptorSet(Type type, const D3D12_CPU_DESCRIPTOR_HANDLE(&in)[SIZE]): m_type(type), m_descriptors(in), m_size(IndexT(SIZE)) { m_base.ptr = 0; }
		/// Default ctor
	Dx12DescriptorSet() :m_type(TYPE_UNKNOWN), m_size(0), m_descriptors(NV_NULL) { m_base.ptr = 0; }
		/// Ctor with array 
	NV_FORCE_INLINE Dx12DescriptorSet(Type type, const D3D12_CPU_DESCRIPTOR_HANDLE* descs, IndexT size): m_type(type), m_descriptors(descs), m_size(size) { m_base.ptr = 0; }
		/// Ctor with array and contiguous
	NV_FORCE_INLINE Dx12DescriptorSet(D3D12_CPU_DESCRIPTOR_HANDLE base, Int size) : m_type(TYPE_UNKNOWN), m_descriptors(NV_NULL), m_size(size), m_base(base) {}

	protected:
	Bool _hasIntersectionListList(const ThisType& rhs) const;
	Bool _hasIntersectionRunList(const ThisType& rhs, Int descriptorSize) const;
	Bool _hasIntersectionRunRun(const ThisType& rhs, Int descriptorSize) const;
	public:

	Type m_type;
	const D3D12_CPU_DESCRIPTOR_HANDLE* m_descriptors;	///< Uniquely specified. If m_numDescriptors > 0 and NV_NULL the base is used
	IndexT m_size;											///< The number of descriptors
	D3D12_CPU_DESCRIPTOR_HANDLE m_base;
};

/*! \brief Dx12DescriptorCache provides a mechanism for caching often used descriptors such they can be arranged linearly, 
and such that they are all on the same GPU ID3D12DesciptorHeap, as DX12 requires for work submission. 

Dx12DescriptorCache serves multiple purposes surrounding the the use of descriptors. The motivating issues are

* Dx12 needs all descriptors to be held on a single heap
* If possible we want to use the minimum amount of heaps (ideally only one)
* We want to cache sets of descriptors that do not change
* Dx12 often likes descriptors arranged linearly
* We need to track GPU usage of heaps - as even if we have run out of space on heap and are using another it still might be in use 

The cache addresses all of these issues, to do so it has to make some assumptions about descriptors. The main assumption is 
that the contents of descriptors change rarely. That being the case it is not unreasonable to use an array of handles to uniquely 
identify a descriptor combination. In taking this approach though, if a descriptors _contents_ is changed it is necessary to 
inform the cache (using onDescriptorChanged).

Each 'SubHeap' consists of a Dx12 shader visible descriptor heap. The cache will create new heaps as needed, but with a correctly
sized cache there should only be 2 - one that is active, and another that is pending GPU completion. 
*/
class Dx12DescriptorCache
{
	NV_CO_DECLARE_CLASS_BASE(Dx12DescriptorCache);

	struct EntryBase {};
	typedef const EntryBase* Cursor;
	typedef Dx12DescriptorSet::Hash Hash;

	enum 
	{
		NUM_BIN_SHIFT = 6,
		NUM_BINS = 1 << NUM_BIN_SHIFT,				///< Number of bins used for hashing. Must be a power of 2
	};

		/// Must be called before used
		/// @param device The device the heaps are created on
		/// @param subHeapSize The size the ID3D12DescriptorHeap allocated for a SubHeap. Must be at least as large as maxLinearDescriptors, and ideally large enough to hold all descriptors for a frame
		/// @param type The type of things the heap will hold
		/// @param flags Any flags needed for heap construction
		/// @param fence A fence, used to track GPU progress
	Result init(ID3D12Device* device, Int subHeapSize, Int maxLinearDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, Dx12CounterFence* fence);
	
		/// Get specified handles contiguously on the active heap
	Cursor put(Dx12DescriptorSet::Type type, const D3D12_CPU_DESCRIPTOR_HANDLE* handles, Int numHandles, Bool hasChanged = false);
	
		/// Look where the GPU has got to and release anything not currently used
	Void updateCompleted();
		/// Add a sync point - meaning that when this point is hit in the queue
		/// all of the resources up to this point will no longer be used.
	Void addSync(UInt64 signalValue);

		/// Entirely clear the cache
	Void clearCache();

		/// Evict from cache entry that has this handle.
		/// Returns the number entries evicted
	IndexT evict(D3D12_CPU_DESCRIPTOR_HANDLE handle);
		/// Evict any cache entry that intersects the set.
		/// Returns the number of entries evicted
	IndexT evictIntersects(const Dx12DescriptorSet& set);

		/// Makes the active heap have space for the amount of handles specified
		/// If it is necessary to have multiple runs on the same heap - this method can be called with the combined size of all the runs before calling put
		/// doing so will guarantee they are all on the single active heap, and the cache will be honored, and extra space only used if required
	Void requireSpace(Int numHandles);

		/// Returns true if holds descriptor
	Bool hasDescriptor(Cursor cursor, D3D12_CPU_DESCRIPTOR_HANDLE handle) const;

		/// Puts multiple sets on the active heap and returns handles in same order as in the sets
		/// Bit 0 of has changedFlags corresponds to set[0], bit 1 to set[1] and so forth. A set bit means the contents is dirty.
	ID3D12DescriptorHeap* put(const ConstSlice<const Dx12DescriptorSet>& sets, UInt32 hasChangedFlags, D3D12_GPU_DESCRIPTOR_HANDLE* handlesOut);
		/// Puts the set on the heap. Returns the heap the instance is created on, and the specific handle to handleOut
	ID3D12DescriptorHeap* put(const Dx12DescriptorSet& set, Bool hasChanged, D3D12_GPU_DESCRIPTOR_HANDLE* handleOut);

		/// Get the Cpu handle for a heap
	NV_FORCE_INLINE D3D12_CPU_DESCRIPTOR_HANDLE getCpuHandle(Cursor cursor, Int index) const;
		/// Get the Gpu handle for a heap
	NV_FORCE_INLINE D3D12_GPU_DESCRIPTOR_HANDLE getGpuHandle(Cursor cursor, Int index) const;
	
		/// Get the Cpu handle for a heap
	NV_FORCE_INLINE D3D12_CPU_DESCRIPTOR_HANDLE getCpuHandle(ID3D12DescriptorHeap* heap, Int index) const;
		/// Get the Gpu handle for a heap
	NV_FORCE_INLINE D3D12_GPU_DESCRIPTOR_HANDLE getGpuHandle(ID3D12DescriptorHeap* heap, Int index) const;

		/// Get the active heap
	NV_FORCE_INLINE ID3D12DescriptorHeap* getActiveHeap() const { return m_activeSubHeap->m_heap; }
		/// Get the size of a descriptor
	NV_FORCE_INLINE Int getDescriptorSize() const { return m_descriptorSize; }

		/// Ctor
	Dx12DescriptorCache();
		/// Dtor
	~Dx12DescriptorCache();

	protected:
	
	struct SubHeap
	{
		ID3D12DescriptorHeap* m_heap;			///< The heap
		Int m_numPendingRefs;					///< Number of pending refs (ie refs to be removed from the queue)
		Int m_numActiveRefs;					///< Num active refs
	};
	struct Entry: public EntryBase
	{
		Hash m_hash;							///< Hash of the descriptorSet
		Int m_startIndex;						///< The start index to the descriptors on the SubHeap descriptor heap
		SubHeap* m_subHeap;						///< The heap that has the descriptors on it, or NV_NULL if not on any heap
		Entry* m_next;							///< Next in the hash bin

		Dx12DescriptorSet m_descriptorSet;		///< The descriptor set
		D3D12_CPU_DESCRIPTOR_HANDLE m_handles[1];	///< Must be last entry, stores the actual handles
	};
	struct PendingEntry
	{
		UInt64 m_completedValue;			///< The value when this is completed
		SubHeap* m_subHeap;					///< The heap that has outstanding references
		Int m_numRefs;						///< The amount of pending references. When this pending entry completes this will be subtracted from the m_numPendingRefs on subHeap 
	};

		/// Calculate the bin index
	NV_FORCE_INLINE static Int _calcBinIndex(Hash hashIn) 
	{ 
		// Make 32 bit to try and distribute the bits 
#if NV_PTR_IS_64
		UInt32 hash = UInt32(hashIn >> 32) ^ UInt32(hashIn);
#else	
		UInt32 hash = UInt32(hashIn);
#endif
#if 1
		// Mix top and bottom 16 bits
		hash ^= (hash >> 16) | (hash << 16);
		const Int numBits = 16;
		UInt32 shift = hash >> NUM_BIN_SHIFT;
		const Int numIter = (numBits + NUM_BIN_SHIFT - 1) / NUM_BIN_SHIFT;
		for (Int i = 0; i < numIter - 1; i++)
		{
			hash ^= shift;
			shift >>= NUM_BIN_SHIFT;
		}
		return Int(hash & (NUM_BINS - 1));
#else
		// 32 bit divide is MUCH faster than 64 bit
		return hash % NUM_BINS;
#endif
	}

	Void _freeSubHeap(SubHeap* subHeap);
	Void _freeSubHeaps(SubHeap*const* subHeaps, IndexT numSubHeaps);
	SubHeap* _newSubHeap();
	Void _addSync(UInt64 signalValue, SubHeap* subHeap);

	Void _transitionPendingToFree(SubHeap* subHeap);
	Void _transitionActiveToPending();
		/// Gets an entry that has these handles on the active heap, copying if necessary
	Entry* _put(const Dx12DescriptorSet& set, Bool hasChanged);
		/// Copy the handles defined into the set to the dst
	Void _copyHandles(const Dx12DescriptorSet& set, D3D12_CPU_DESCRIPTOR_HANDLE dst);
		/// Returns a SubHeap from the freelist, or if there aren't any will create a new one
	SubHeap* _nextFreeSubHeap();
	
	Void _allocateAndCopyHandles(Entry* entry);

	Entry* _findEntry(Hash hash, const Dx12DescriptorSet& set);
	Entry* _newEntry(Hash hash, const Dx12DescriptorSet& set); 

		/// Any cache entries which reference this subheap will be removed (typically on transition from free to active)
	Void _removeEntriesOnSubHeap(SubHeap* subHeap);

	Int m_activeFreeIndex;				///< Descriptors at this and after are available
	SubHeap* m_activeSubHeap;			///< The active heap
	
	FreeList m_entryFreeList;			///< Freelist holding entries
	FreeList m_subHeapFreeList;			///< Freelist holding sub heaps

	Queue<PendingEntry> m_pendingQueue;	///< Holds the outstanding references

	Array<SubHeap*> m_pendingSubHeaps;	///< List of heaps that still contain references, so can't be used
	Array<SubHeap*> m_freeSubHeaps;		///< Heaps that are free for reuse

	Int m_descriptorSize;				///< The size of each descriptor
	Int m_maxLinearDescriptors;			///< Number of linear descriptors that can be handled
	Int m_subHeapSize;					///< Size of 

	D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
	D3D12_DESCRIPTOR_HEAP_FLAGS m_heapFlags;

	Dx12CounterFence* m_fence;			///< The fence to use
	ID3D12Device* m_device;				///< The device that resources will be constructed on

	Entry* m_bins[NUM_BINS];			///< Bins are indexed by the hash
};

// --------------------------------------------------------------------------
NV_FORCE_INLINE D3D12_CPU_DESCRIPTOR_HANDLE Dx12DescriptorCache::getCpuHandle(ID3D12DescriptorHeap* heap, Int index) const
{
	NV_CORE_ASSERT(index >= 0 && index < m_subHeapSize);
	D3D12_CPU_DESCRIPTOR_HANDLE start = heap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE dst;
	dst.ptr = start.ptr + m_descriptorSize * index;
	return dst;
}
// ---------------------------------------------------------------------------
NV_FORCE_INLINE D3D12_GPU_DESCRIPTOR_HANDLE Dx12DescriptorCache::getGpuHandle(ID3D12DescriptorHeap* heap, Int index) const
{
	NV_CORE_ASSERT(index >= 0 && index < m_subHeapSize);
	D3D12_GPU_DESCRIPTOR_HANDLE start = heap->GetGPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE dst;
	dst.ptr = start.ptr + m_descriptorSize * index;
	return dst;
}
// ---------------------------------------------------------------------------
NV_FORCE_INLINE D3D12_CPU_DESCRIPTOR_HANDLE Dx12DescriptorCache::getCpuHandle(Cursor cursor, Int index) const 
{ 
	const Entry* entry = static_cast<const Entry*>(cursor); 
	NV_CORE_ASSERT(entry && index >= 0 && index < entry->m_descriptorSet.getSize()); 
	return getCpuHandle(entry->m_subHeap->m_heap, entry->m_startIndex + index); 
}
// ---------------------------------------------------------------------------
NV_FORCE_INLINE D3D12_GPU_DESCRIPTOR_HANDLE Dx12DescriptorCache::getGpuHandle(Cursor cursor, Int index) const 
{ 
	const Entry* entry = static_cast<const Entry*>(cursor); 
	NV_CORE_ASSERT(entry && index >= 0 && index < entry->m_descriptorSet.getSize()); 
	return getGpuHandle(entry->m_subHeap->m_heap, entry->m_startIndex + index); 
}

} // namespace Common
} // namespace nvidia

/** @} */

#endif // NV_CO_DX12_DESCRIPTOR_CACHE_H
