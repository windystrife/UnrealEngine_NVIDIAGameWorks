/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include <Nv/Common/NvCoCommon.h>

#include "NvCoDx12DescriptorCache.h"

namespace nvidia {
namespace Common {

Dx12DescriptorSet::Hash Dx12DescriptorSet::calcHash() const
{
	Hash hash = Hash(m_size);
	if (m_size > 0)
	{
		if (m_descriptors)
		{
			const Int num = Int((m_size * sizeof(D3D12_CPU_DESCRIPTOR_HANDLE)) / sizeof(Hash));
			const SizeT* src = (const SizeT*)m_descriptors;
			// Rabin-Karp hash
			for (Int i = 0; i < num; i++)
			{
				hash = hash * 31 + src[i];
			}
		}
		else
		{
			const SizeT* src = (const SizeT*)&m_base;
			const Int num = Int(sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) / sizeof(Hash));
			// Rabin-Karp hash
			for (Int i = 0; i < num; i++)
			{
				hash = hash * 31 + src[i];
			}
		}
	}
	return hash;
}

Bool Dx12DescriptorSet::operator==(const ThisType& rhs) const
{
	if (this == &rhs)
	{
		return true;
	}
	if ((m_size != rhs.m_size) || ((m_descriptors == NV_NULL) ^ (rhs.m_descriptors == NV_NULL)))
	{
		return false;
	}
	if (m_descriptors)
	{
		const D3D12_CPU_DESCRIPTOR_HANDLE* a = m_descriptors;
		const D3D12_CPU_DESCRIPTOR_HANDLE* b = rhs.m_descriptors;
		if (a != b)
		{
			for (IndexT i = 0; i < m_size; i++)
			{
				if (a[i].ptr != b[i].ptr)
				{
					return false;
				}
			}
		}
		return true;
	}
	return m_base.ptr == rhs.m_base.ptr;
}

Bool Dx12DescriptorSet::hasNull() const
{
	if (m_size <= 0)
	{
		return false;
	}
	if (m_descriptors)
	{
		for (IndexT i = 0; i < m_size; i++)
		{
			if (m_descriptors[i].ptr == 0)
			{
				return true;
			}
		}
		return false;
	}
	else
	{
		return m_base.ptr == 0;
	}
}

Bool Dx12DescriptorSet::hasHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle, Int descriptorSize) const
{
	if (m_descriptors)
	{
		const D3D12_CPU_DESCRIPTOR_HANDLE* src = m_descriptors;
		const IndexT num = m_size;
		for (IndexT i = 0; i < num; i++)
		{
			if (src[i].ptr == handle.ptr)
			{
				return true;
			}
		}
		return false;
	}

	return (handle.ptr >= m_base.ptr && handle.ptr < m_base.ptr + m_size * descriptorSize);
}

Bool Dx12DescriptorSet::_hasIntersectionRunRun(const ThisType& rhs, Int descriptorSize) const
{
	NV_CORE_ASSERT(isRun() && rhs.isRun());
	const SizeT sa(m_base.ptr);
	const SizeT ea(m_base.ptr + descriptorSize * m_size);
	const SizeT sb(rhs.m_base.ptr);
	const SizeT eb(rhs.m_base.ptr + descriptorSize * rhs.m_size);
	return !(sa >= eb ||  sb >= ea );
}

Bool Dx12DescriptorSet::_hasIntersectionListList(const ThisType& rhs) const
{
	NV_CORE_ASSERT(isList() && rhs.isList());
	for (IndexT i = 0; i < m_size; i++)
	{
		for (IndexT j = 0; j < rhs.m_size; j++)
		{
			if (m_descriptors[i].ptr == rhs.m_descriptors[j].ptr)
			{
				return true;
			}
		}
	}
	return false;
}

Bool Dx12DescriptorSet::_hasIntersectionRunList(const ThisType& rhs, Int descriptorSize) const
{
	NV_CORE_ASSERT(isRun() && rhs.isList());

	const SizeT sa(m_base.ptr);
	const SizeT ea(m_base.ptr + descriptorSize * m_size);

	for (IndexT i = 0; i < rhs.m_size; i++)
	{
		const SizeT b(rhs.m_descriptors[i].ptr);
		if (b >= sa && b < ea)
		{
			return true;
		}
	}
	return false;
}

Bool Dx12DescriptorSet::hasIntersection(const ThisType& rhs, Int descriptorSize) const
{
	if (m_size <= 0 || rhs.m_size <= 0)
	{
		return false;
	}
	if (this == &rhs)
	{
		return true;
	}
	// Special case if its a single handle
	if (m_size == 1)
	{
		return rhs.hasHandle(getInitial(), descriptorSize); 
	}
	else if (rhs.m_size == 1)
	{
		return hasHandle(rhs.getInitial(), descriptorSize);
	}

	if (m_descriptors)
	{
		// this is a discrete list
		return rhs.m_descriptors ? _hasIntersectionListList(rhs) : rhs._hasIntersectionRunList(*this, descriptorSize);
	}
	else
	{
		return rhs.m_descriptors ? _hasIntersectionRunList(rhs, descriptorSize) : _hasIntersectionRunRun(rhs, descriptorSize);
	}
}

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!! Dx12DescriptorCache !!!!!!!!!!!!!!!!!!!!!!!!!!!! */

Dx12DescriptorCache::Dx12DescriptorCache():
	m_fence(NV_NULL),
	m_device(NV_NULL),
	m_subHeapFreeList(sizeof(SubHeap), NV_ALIGN_OF(SubHeap), 8, NV_NULL)
{
	m_activeSubHeap = NV_NULL;
	m_descriptorSize = 0;
	m_maxLinearDescriptors = 0;
	m_subHeapSize = 0;
	
	Memory::zero(m_bins);
}

Dx12DescriptorCache::~Dx12DescriptorCache()
{
	_freeSubHeap(m_activeSubHeap);
	_freeSubHeaps(m_pendingSubHeaps.begin(), m_pendingSubHeaps.getSize());
	_freeSubHeaps(m_freeSubHeaps.begin(), m_freeSubHeaps.getSize());
}

Void Dx12DescriptorCache::_freeSubHeap(SubHeap* subHeap)
{
	if (subHeap)
	{
		subHeap->m_heap->Release();
		m_subHeapFreeList.deallocate(subHeap);
	} 
}

Void Dx12DescriptorCache::_freeSubHeaps(SubHeap*const* subHeaps, IndexT numSubHeaps)
{
	for (IndexT i = 0; i < numSubHeaps; i++)
	{
		_freeSubHeap(subHeaps[i]);
	}
}

Result Dx12DescriptorCache::init(ID3D12Device* device, Int subHeapSize, Int maxLinearDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, Dx12CounterFence* fence)
{
	NV_CORE_ASSERT(subHeapSize > 0 && maxLinearDescriptors > 0);
	NV_CORE_ASSERT(maxLinearDescriptors <= subHeapSize);

	m_maxLinearDescriptors = maxLinearDescriptors;
	
	m_fence = fence;
	m_device = device;

	m_heapType = type;
	m_heapFlags = flags;

	m_subHeapSize = subHeapSize;
	m_maxLinearDescriptors = maxLinearDescriptors;

	m_entryFreeList.init(sizeof(Entry) + sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * (maxLinearDescriptors - 1), NV_ALIGN_OF(Entry), 16, NV_NULL);

	m_descriptorSize = device->GetDescriptorHandleIncrementSize(type);

	m_activeSubHeap = _newSubHeap();
	m_activeFreeIndex = 0;
	return NV_OK;
}

Dx12DescriptorCache::SubHeap* Dx12DescriptorCache::_newSubHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = m_subHeapSize;
	desc.Flags = m_heapFlags;
	desc.Type = m_heapType;

	ComPtr<ID3D12DescriptorHeap> heap;
	if (NV_FAILED(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(heap.writeRef()))))
	{
		NV_CORE_ALWAYS_ASSERT("Unable to create heap");
		return NV_NULL;
	}

	SubHeap* subHeap = (SubHeap*)m_subHeapFreeList.allocate();
	subHeap->m_numActiveRefs = 0;
	subHeap->m_numPendingRefs = 0;
	subHeap->m_heap = heap.detach();
	return subHeap;
}

Dx12DescriptorCache::SubHeap* Dx12DescriptorCache::_nextFreeSubHeap()
{
	if (m_freeSubHeaps.getSize())
	{
		SubHeap* subHeap = m_freeSubHeaps.back();
		NV_CORE_ASSERT(subHeap->m_numActiveRefs == 0 && subHeap->m_numPendingRefs == 0 && subHeap->m_heap);
		m_freeSubHeaps.popBack();

		// We need to remove any entries that still point to this subHeap
		_removeEntriesOnSubHeap(subHeap);
		
		return subHeap;
	}
	else
	{
		return _newSubHeap();
	}
}

Void Dx12DescriptorCache::_addSync(UInt64 signalValue, SubHeap* subHeap)
{
	if (subHeap->m_numActiveRefs > 0)
	{
		subHeap->m_numPendingRefs += subHeap->m_numActiveRefs;

		PendingEntry entry;
		entry.m_subHeap = subHeap;
		entry.m_numRefs = subHeap->m_numActiveRefs;
		entry.m_completedValue = signalValue;

		m_pendingQueue.pushBack(entry);

		subHeap->m_numActiveRefs = 0;
	}
}

Void Dx12DescriptorCache::addSync(UInt64 signalValue)
{
	// Find any with activeRefs, add syncs for those and zero active
	{
		IndexT numPending = m_pendingSubHeaps.getSize();
		for (IndexT i = 0; i < numPending; i++)
		{
			SubHeap* subHeap = m_pendingSubHeaps[i];
			if (subHeap->m_numActiveRefs > 0)
			{
				_addSync(signalValue, subHeap);
			}
		}
	}
	// Add a sync (if necessary) for active
	_addSync(signalValue, m_activeSubHeap);
}

Void Dx12DescriptorCache::_removeEntriesOnSubHeap(SubHeap* subHeap)
{
	for (Int i = 0; i < NUM_BINS; i++)
	{
		Entry** prev = &m_bins[i];
		Entry* cur = m_bins[i];

		while (cur)
		{
			if (cur->m_subHeap == subHeap)
			{
				// Remove from the list
				Entry* next = cur->m_next;
				*prev = next;

				// Free the memory
				m_entryFreeList.deallocate(cur);

				// Work on next
				cur = next;
			}
			else
			{
				// Next
				prev = &cur->m_next;
				cur = cur->m_next;
			}
		}
	}
}

Void Dx12DescriptorCache::_transitionPendingToFree(SubHeap* subHeap)
{
	IndexT index = m_pendingSubHeaps.indexOf(subHeap);
	NV_CORE_ASSERT(index >= 0);
	if (index < 0)
	{
		return;
	}

	// There are no newPending, so can just remove
	m_pendingSubHeaps.removeAt(index);
	// If moved to the free list - there cannot be any refs otherwise it's not strictly free
	NV_CORE_ASSERT(subHeap->m_numActiveRefs == 0 && subHeap->m_numPendingRefs == 0);
	m_freeSubHeaps.pushBack(subHeap);
}

Void Dx12DescriptorCache::updateCompleted()
{
	const UInt64 completedValue = m_fence->getCompletedValue();
	while (!m_pendingQueue.isEmpty())
	{
		const PendingEntry& entry = m_pendingQueue.front();
		if (entry.m_completedValue <= completedValue)
		{
			SubHeap* subHeap = entry.m_subHeap;

			// Fix the subheaps pending refs, by taking off the refs from the entry
			subHeap->m_numPendingRefs -= entry.m_numRefs;

			// Transition to free, if no pending or active refs, and not active subHeap
			if (subHeap != m_activeSubHeap && subHeap->m_numPendingRefs == 0 && subHeap->m_numActiveRefs == 0)
			{
				_transitionPendingToFree(subHeap);
			}
			// Remove from the front
			m_pendingQueue.popFront();
		}
		else
		{
			break;
		}
	}
}

Dx12DescriptorCache::Entry* Dx12DescriptorCache::_newEntry(Hash hash, const Dx12DescriptorSet& set)
{
	const Int size = Int(set.getSize());
	requireSpace(size);

	Entry* entry = (Entry*)m_entryFreeList.allocate();

	// Set up the descriptor set
	entry->m_descriptorSet = set;
	if (size > 0 && set.m_descriptors)
	{
		// Save the handles
		Memory::copy(entry->m_handles, set.m_descriptors, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * size);
		entry->m_descriptorSet.m_descriptors = entry->m_handles;
	}

	entry->m_hash = hash;
	entry->m_subHeap = NV_NULL;
	entry->m_startIndex = -1;

	// Add to the bin
	const Int binIndex = _calcBinIndex(hash);
	entry->m_next = m_bins[binIndex];
	m_bins[binIndex] = entry;
	return entry;
}

Dx12DescriptorCache::Entry* Dx12DescriptorCache::_findEntry(Hash hash, const Dx12DescriptorSet& set)
{
	// See if we already have it
	Entry* entry = m_bins[_calcBinIndex(hash)];
	while (entry)
	{
		if (entry->m_hash == hash && entry->m_descriptorSet == set)
		{
			return entry;
		}
		// Next in the bin
		entry = entry->m_next;
	}
	return NV_NULL;
}

Void Dx12DescriptorCache::_transitionActiveToPending()
{
	m_pendingSubHeaps.pushBack(m_activeSubHeap);
	m_activeSubHeap = _nextFreeSubHeap();
	NV_CORE_ASSERT(m_activeSubHeap);
	m_activeFreeIndex = 0;
}

Void Dx12DescriptorCache::requireSpace(Int numHandles)
{
	NV_CORE_ASSERT(numHandles <= m_subHeapSize);
	// Make sure we have space on the active heap
	if (m_activeFreeIndex + numHandles > m_subHeapSize)
	{
		_transitionActiveToPending();
	}
}

Void Dx12DescriptorCache::_copyHandles(const Dx12DescriptorSet& set, D3D12_CPU_DESCRIPTOR_HANDLE dst)
{
	const IndexT size = set.getSize();
	if (size > 0)
	{
		const D3D12_CPU_DESCRIPTOR_HANDLE* src = set.m_descriptors;
		if (src)
		{
			for (IndexT i = 0; i < size; i++)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE handle = src[i];
				// Only copy if it's non null. If it's null we'll just ignore it, because it means it's not used.

				// Use null handle if one is set
				//handle = handle.ptr ? handle : m_nullHandles[set.m_type];

				if (handle.ptr)
				{
					m_device->CopyDescriptorsSimple(1, dst, handle, m_heapType);
				}
				else
				{
					// https://msdn.microsoft.com/en-us/library/windows/desktop/dn899109(v=vs.85).aspx#Null_descriptors
					// Just initialize to something
					switch (m_heapType)
					{
						case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
						{
							switch (set.m_type)
							{
								case Dx12DescriptorSet::TYPE_CBV:
								{
									m_device->CreateConstantBufferView(NV_NULL, dst);
									break;
								}
								case Dx12DescriptorSet::TYPE_SRV:
								{
									D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
									desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
									desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
									desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

									m_device->CreateShaderResourceView(NV_NULL, &desc, dst);
									break;
								}
								case Dx12DescriptorSet::TYPE_UAV:
								{
									m_device->CreateUnorderedAccessView(NV_NULL, NV_NULL, NV_NULL, dst);
									break;
								}
								default:
								{
									NV_CORE_ASSERT("Invalid descriptor type");
									break;
								}
							}
							// This could be incorrect for a given usage. But by passing NV_NULL the app is saying this will not be touched
							// so all it is doing is making it's state defined.
							//m_device->CreateConstantBufferView(NV_NULL, dst);
							break;
						}
						case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
						{
							NV_CORE_ASSERT(set.m_type == Dx12DescriptorSet::TYPE_OTHER);
							m_device->CreateSampler(NV_NULL, dst);
							break;
						}
						default: break;
					}
				}

				// Next
				dst.ptr += m_descriptorSize;
			}
		}
		else
		{
			m_device->CopyDescriptorsSimple(UINT(size), dst, set.m_base, m_heapType);
		}
	}
}

Dx12DescriptorCache::Cursor Dx12DescriptorCache::put(Dx12DescriptorSet::Type type, const D3D12_CPU_DESCRIPTOR_HANDLE* handles, Int numHandles, Bool hasChanged)
{
	const Dx12DescriptorSet set(type, handles, numHandles);
	Entry* entry = _put(set, hasChanged);
	NV_CORE_ASSERT(entry->m_subHeap == m_activeSubHeap);
	// Mark that it's ref'd
	m_activeSubHeap->m_numActiveRefs++;
	return entry;
}

ID3D12DescriptorHeap* Dx12DescriptorCache::put(const Dx12DescriptorSet& set, Bool hasChanged, D3D12_GPU_DESCRIPTOR_HANDLE* handleOut)
{
	IndexT numDesc = set.getSize();
	if (numDesc > 0)
	{
		Entry* entry = _put(set, hasChanged);
		NV_CORE_ASSERT(entry->m_subHeap == m_activeSubHeap);
		// Mark that it's ref'd
		m_activeSubHeap->m_numActiveRefs++;
		*handleOut = getGpuHandle(entry, 0);
	}
	else
	{
		*handleOut = m_activeSubHeap->m_heap->GetGPUDescriptorHandleForHeapStart();
	}
	return getActiveHeap();
}

ID3D12DescriptorHeap* Dx12DescriptorCache::put(const ConstSlice<const Dx12DescriptorSet>& sets, UInt32 hasChangedFlags, D3D12_GPU_DESCRIPTOR_HANDLE* handlesOut)
{
	const IndexT numSets = sets.getSize();
	if (numSets <= 0)
	{
		return NV_NULL;
	}
	if (numSets == 1)
	{	
		return put(sets[0], (hasChangedFlags & 1) != 0, handlesOut);
	}

	const IndexT maxEntries = 8;
	NV_CORE_ASSERT(numSets <= maxEntries);
	Entry* entries[maxEntries];

	IndexT totalNumDescs = 0;
	IndexT numDescsRequired = 0;

	{
		UInt32 bit = 1;
		for (IndexT i = 0; i < numSets; i++, bit += bit)
		{
			const Dx12DescriptorSet& set = sets[i];
			const IndexT numDescs = set.getSize();

			// If it doesn't have any entries, we don't need to cache anything. Mark the list as this is an 
			// uncached entry with NV_NULL. 
			if (numDescs <= 0)
			{
				entries[i] = NV_NULL;
				continue;
			}

			Hash hash = set.calcHash();
			Entry* entry = _findEntry(hash, set);
			if (!entry)
			{
				// Create a new entry, but don't assign it any heap yet
				entry = _newEntry(hash, set);
				numDescsRequired += numDescs;			
			}
			else
			{
				if (bit & hasChangedFlags)
				{
					// Need a totally fresh copy
					entry->m_subHeap = NV_NULL;
					numDescsRequired += numDescs;
				}
				else if (entry->m_subHeap != m_activeSubHeap)
				{
					// Just need a copy to the active heap
					numDescsRequired += numDescs;
				}
			}

			entries[i] = entry;
			totalNumDescs += numDescs;
		}
	}

	// Make sure the active heap has enough space
	requireSpace(Int(numDescsRequired));

	{
		for (IndexT i = 0; i < numSets; i++)
		{
			Entry* entry = entries[i];
			if (entry)
			{
				if (entry->m_subHeap != m_activeSubHeap)
				{
					_allocateAndCopyHandles(entry);
				}
				// Must be on active heap
				NV_CORE_ASSERT(entry->m_subHeap == m_activeSubHeap);
				// Save the handles out
				handlesOut[i] = getGpuHandle(entry, 0);
			}
			else
			{
				// Is a non cached entry (because size is 0) so just return the start of the active heap.
				handlesOut[i] = m_activeSubHeap->m_heap->GetGPUDescriptorHandleForHeapStart();
			}
		}
		// Mark that it's ref'd
		m_activeSubHeap->m_numActiveRefs ++;
	}
	return getActiveHeap();
}

Void Dx12DescriptorCache::_allocateAndCopyHandles(Entry* entry)
{
	const IndexT numDescs = entry->m_descriptorSet.getSize();

	NV_CORE_ASSERT(Int(m_activeFreeIndex + numDescs) <= m_subHeapSize);

	D3D12_CPU_DESCRIPTOR_HANDLE dst = getCpuHandle(m_activeSubHeap->m_heap, m_activeFreeIndex);

	/* Currently the copying from a subheap is disabled, and we just recreate from scratch. This stops a crash bug on release builds. 
	It may be this is a bad idead anyway, because it will be doing a GPU-GPU address space copy. */
	//const SubHeap* subHeap = entry->m_subHeap;
	/* if (subHeap)
	{
		// Copy to active
		m_device->CopyDescriptorsSimple(UINT(numDescs), dst, getCpuHandle(subHeap->m_heap, entry->m_startIndex), m_heapType);
	}
	else */
	{
		// Copy the set to the target
		_copyHandles(entry->m_descriptorSet, dst);
	}
	// Set heap and offset
	entry->m_subHeap = m_activeSubHeap;
	entry->m_startIndex = m_activeFreeIndex;
	// Move allocation index
	m_activeFreeIndex += Int(numDescs);
}

Dx12DescriptorCache::Entry* Dx12DescriptorCache::_put(const Dx12DescriptorSet& set, Bool hasChanged)
{
	Int totalNumHandles = Int(set.getSize());
	NV_CORE_ASSERT(totalNumHandles <= m_maxLinearDescriptors);
	const Hash hash = set.calcHash();
	Entry* entry = _findEntry(hash, set);
	if (entry)
	{
		if (hasChanged)
		{
			// Mark that the previous copy is no use
			entry->m_subHeap = NV_NULL;
		}
		else if (entry->m_subHeap == m_activeSubHeap)
		{
			// We've got it
			return entry;
		}
	}
	else
	{
		entry = _newEntry(hash, set);
	}
	// Make space
	requireSpace(totalNumHandles);
	// Copy over the handles
	_allocateAndCopyHandles(entry);
	return entry;
}

Void Dx12DescriptorCache::clearCache()
{
	m_entryFreeList.deallocateAll();
	Memory::zero(m_bins);
}

Bool Dx12DescriptorCache::hasDescriptor(Cursor cursor, D3D12_CPU_DESCRIPTOR_HANDLE handle) const
{
	const Entry* entry = static_cast<const Entry*>(cursor);
	return entry->m_descriptorSet.hasHandle(handle, m_descriptorSize);
}

IndexT Dx12DescriptorCache::evict(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	// If null, just ignore
	if (handle.ptr == 0)
	{
		return 0;
	}
	IndexT numEvicted = 0;
	// Search the hash map to see if this handle is used 
	for (Int i = 0; i < NUM_BINS; i++)
	{
		Entry** prev = &m_bins[i];
		Entry* cur = m_bins[i];

		while (cur)
		{
			if (hasDescriptor(cur, handle))
			{
				// Remove from the list
				Entry* next = cur->m_next;
				*prev = next;
				// Free the memory
				m_entryFreeList.deallocate(cur);
				numEvicted++;
				
				// Work on next
				cur = next;
			}
			else
			{
				// Next
				prev = &cur->m_next;
				cur = cur->m_next;
			}
		}
	}
	return numEvicted;
}

IndexT Dx12DescriptorCache::evictIntersects(const Dx12DescriptorSet& set)
{
	// Can't contain null
	NV_CORE_ASSERT(!set.hasNull());
	if (set.m_size <= 0)
	{
		return 0;
	}
	if (set.m_size == 1)
	{
		return evict(set.getInitial());
	}

	IndexT numEvicted = 0;
	// Search the hash map to see if this handle is used 
	for (Int i = 0; i < NUM_BINS; i++)
	{
		Entry** prev = &m_bins[i];
		Entry* cur = m_bins[i];
		while (cur)
		{
			if (cur->m_descriptorSet.hasIntersection(set, m_descriptorSize))
			{
				// Remove from the list
				Entry* next = cur->m_next;
				*prev = next;
				// Free the memory
				m_entryFreeList.deallocate(cur);
				numEvicted++;
				// Work on next
				cur = next;
			}
			else
			{
				// Next
				prev = &cur->m_next;
				cur = cur->m_next;
			}
		}
	}
	return numEvicted;
}

} // namespace Common
} // namespace nvidia
