/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

// this
#include "NvCoDx12AsyncManager.h"

namespace nvidia {
namespace Common {

Dx12AsyncManager::Dx12AsyncManager()
{
	Memory::zero(m_lists);
}

Void Dx12AsyncManager::addSync(UInt64 signalValue)
{
	Dx12Async* cur = m_lists[Dx12Async::STATE_NEW];
	if (cur)
	{
		while(true)
		{
			// Set completion value
			cur->m_completedValue = signalValue;
			// Mark as pending
			cur->m_state = Dx12Async::STATE_PENDING;
			// Handle next if there is one
			Dx12Async* next = cur->m_next;
			if (!next)
			{
				break;
			}
			// Next
			cur = next;
		}
		// Cur points to the last element in the list
		cur->m_next = m_lists[Dx12Async::STATE_PENDING];
		m_lists[Dx12Async::STATE_PENDING] = m_lists[Dx12Async::STATE_NEW];

		// New is now empty
		m_lists[Dx12Async::STATE_NEW] = NV_NULL;
	}
}

Dx12Async* Dx12AsyncManager::updateCompleted(UInt64 completedValue)
{
	Dx12Async* completedEnd = m_lists[Dx12Async::STATE_COMPLETED];
	// Holds start of completed list
	Dx12Async* completed = completedEnd;

	Dx12Async** prev = &m_lists[Dx12Async::STATE_PENDING];
	Dx12Async* async = m_lists[Dx12Async::STATE_PENDING];

	while (async)
	{
		if (async->m_completedValue <= completedValue)
		{
			Dx12Async* next = async->m_next;
			// Remove from current list
			*prev = next;

			// Add to completed
			async->m_next = completed;
			completed = async;

			// Mark as on completed list
			async->m_state = Dx12Async::STATE_COMPLETED;

			//onAsyncCompleted(async);

			async = next;
		}
		else
		{
			prev = &async->m_next;
			async = async->m_next;
		}
	}
	m_lists[Dx12Async::STATE_COMPLETED] = completed;
	return completedEnd;
}


Dx12Async* Dx12AsyncManager::find(Dx12Async::State state, Int type, Void* owner, Int32 uniqueCount) const
{
	Dx12Async* async = m_lists[state];
	while (async)
	{
		if (async->m_type == type && async->m_owner == owner && async->m_uniqueCount == uniqueCount)
		{
			return async;
		}
		async = async->m_next;
	}
	return NV_NULL;
}

Void Dx12AsyncManager::_destroy(Dx12Async* async)
{
	m_handleMap.removeByIndex(async->m_handleIndex);
	m_heap.deallocate(async, async->m_totalSize);
}

IndexT Dx12AsyncManager::onOwnerDestroyed(State state, Void* owner)
{
	Dx12Async** prev = &m_lists[state];
	Dx12Async* async = m_lists[state];
	IndexT numRemoved = 0;
	while (async)
	{
		if (async->m_owner == owner)
		{
			numRemoved++;
			Dx12Async* next = async->m_next;
			*prev = next;
			_destroy(async);
			async = next;
		}
		else
		{
			prev = &async->m_next;
			async = async->m_next;
		}
	}
	return numRemoved;
}

IndexT Dx12AsyncManager::onOwnerDestroyed(Void* owner)
{
	IndexT numRemoved = onOwnerDestroyed(Dx12Async::STATE_NEW, owner);
	numRemoved += onOwnerDestroyed(Dx12Async::STATE_COMPLETED, owner);
	numRemoved += onOwnerDestroyed(Dx12Async::STATE_PENDING, owner);
	return numRemoved;
}

Void Dx12AsyncManager::detach(Dx12Async* removeAsync)
{
	NV_CORE_ASSERT(removeAsync->m_state != Dx12Async::STATE_UNKNOWN);

	Dx12Async** prev = &m_lists[removeAsync->m_state];
	Dx12Async* async = m_lists[removeAsync->m_state];

	while (async)
	{
		if (async == removeAsync)
		{
			async->m_state = Dx12Async::STATE_UNKNOWN;
			*prev = async->m_next;
			return;
		}

		prev = &async->m_next;
		async = async->m_next;
	}

	NV_CORE_ALWAYS_ASSERT("Not found");
}

Void Dx12AsyncManager::releaseCompleted(Dx12Async* async)
{
	NV_CORE_ASSERT(async->m_state == Dx12Async::STATE_COMPLETED);
	if (async->m_state == Dx12Async::STATE_COMPLETED)
	{
		if (--async->m_refCount <= 0)
		{
			detach(async);
			// Free it
			_destroy(async);
		}
	}
}

Void Dx12AsyncManager::release(Dx12Async* async)
{
	if (async->m_state != Dx12Async::STATE_UNKNOWN)
	{
		if (--async->m_refCount <= 0)
		{
			detach(async);
			// Free it
			_destroy(async);
		}
	}
}

Void Dx12AsyncManager::destroy(Dx12Async* async)
{
	if (async->m_state != Dx12Async::STATE_UNKNOWN)
	{
		detach(async);
	}
	_destroy(async);
}

Dx12Async* Dx12AsyncManager::create(Int type, Void* owner, Int32 uniqueCount, SizeT size)
{
	NV_CORE_ASSERT(size >= sizeof(Dx12Async));
	Dx12Async* async = (Dx12Async*)m_heap.allocate(size);
	// Set up initial
	async->m_type = UInt8(type);
	async->m_state = Dx12Async::STATE_NEW;
	async->m_completedValue = 0;
	async->m_owner = owner;
	async->m_uniqueCount = uniqueCount;
	
	async->m_refCount = 1;
	async->m_totalSize = UInt32(size);
	async->m_handleIndex = Int32(m_handleMap.addIndex(async));
	
	// Add to the list
	async->m_next = m_lists[Dx12Async::STATE_NEW];
	m_lists[Dx12Async::STATE_NEW] = async;
	return async;
}

IndexT Dx12AsyncManager::cancel(Dx12Async*const* asyncs, IndexT numHandles, Bool allReferences)
{
	IndexT numRefs = 0;
	for (IndexT i = 0; i < numHandles; i++)
	{
		Dx12Async* async = asyncs[i];
		if (async)
		{
			if (allReferences)
			{
				numRefs += async->m_refCount;
				destroy(async);
			}
			else
			{
				if (async->m_refCount > 0)
				{
					numRefs ++;
					release(async);
				}
			}
		}
	}
	return numRefs;
}

IndexT Dx12AsyncManager::cancel(const Handle* handles, IndexT numHandles, Bool allReferences)
{
	IndexT numRefs = 0;
	for (IndexT i = 0; i < numHandles; i++)
	{
		Dx12Async* async = getByHandle(handles[i]);
		if (async)
		{
			if (allReferences)
			{
				numRefs += async->m_refCount;
				destroy(async);
			}
			else
			{
				if (async->m_refCount > 0)
				{
					numRefs++;
					release(async);
				}
			}
		}
	}
	return numRefs;
}

IndexT Dx12AsyncManager::_cancel(Dx12Async::State state, Void* owner, Bool allReferences)
{
	IndexT numRefs = 0;

	Dx12Async** prev = &m_lists[state];
	Dx12Async* cur = m_lists[state];
	while (cur)
	{
		Dx12Async* next = cur->m_next;
		if (cur->m_owner == owner)
		{
			// Detach
			*prev = next;
			// Handle refs
			if (allReferences)
			{
				numRefs += cur->m_refCount;
				_destroy(cur);
			}
			else
			{
				numRefs++;
				if (--cur->m_refCount <= 0)
				{
					_destroy(cur);
				}
			}
		}
		else
		{
			// Update prev
			prev = &cur->m_next;
		}
		// Next
		cur = next;
	}
	return numRefs;
}

IndexT Dx12AsyncManager::cancel(Void* owner, Bool allReferences)
{
	IndexT numRefs = 0;
	for (Int i = 0; i < Dx12Async::STATE_COUNT_OF; i++)
	{
		numRefs += _cancel(Dx12Async::State(i), owner, allReferences);
	}
	return numRefs;
}

Result Dx12AsyncManager::complete(Int type, Handle* asyncInOut, Bool asyncRepeat, Dx12Async** asyncOut)
{
	*asyncOut = NV_NULL;
	if (!asyncInOut)
	{
		return NV_OK;
	}
	const Handle handle = *asyncInOut;
	if (handle == 0)
	{
		return NV_OK;
	}
	Dx12Async* async = get(type, handle);
	if (async)
	{
		if (async->isCompleted())
		{
			*asyncInOut = 0;
			*asyncOut = async;
			return NV_OK;
		}
		// It's pending
		return NV_E_MISC_PENDING;
	}
	else
	{
		// It's an invalid handle...
		// Return NV_OK if we have asyncRepeat, so we kick off the next get without waiting
		*asyncInOut = 0;
		return asyncRepeat ? NV_OK : NV_E_MISC_INVALID_HANDLE;
	}
}

} // namespace Common 
} // namespace nvidia 