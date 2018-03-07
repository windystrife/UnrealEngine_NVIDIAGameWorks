/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoHandleMap.h"

#ifdef NV_DEBUG
#	include <Nv/Common/Random/NvCoRandomGenerator.h>
#	include <Nv/Common/Container/NvCoArray.h>
#	include <Nv/Common/NvCoFreeList.h>
#	include <Nv/Common/NvCoUniquePtr.h>
#endif

namespace nvidia {
namespace Common {

HandleMapBase::HandleMapBase()
{
	Entry& entry = m_entries.expandOne();
	entry.m_handle = makeHandle(0, 1);
	entry.m_data = NV_NULL;
}

Bool HandleMapBase::remove(Handle handle)
{
	const IndexT numEntries = m_entries.getSize();
	IndexT index = getIndex(handle);
	if (index < numEntries)
	{
		Entry& entry = m_entries[index];
		if (entry.m_handle == handle)
		{
			entry.m_data = NV_NULL;				// Mark as not used
			entry.changeCount();				// Make the count different (makes the isValid test slightly faster)
			m_freeIndices.pushBack(index);
			return true;
		}	
	}
	return false;
}

Bool HandleMapBase::removeByIndex(IndexT index)
{
	const IndexT numEntries = m_entries.getSize();
	if (index > 0 && index < numEntries)
	{
		Entry& entry = m_entries[index];
		NV_CORE_ASSERT(entry.m_data);
		entry.m_data = NV_NULL;					// Mark as not used
		entry.changeCount();					// Make the count different (makes the isValid test slightly faster)
		m_freeIndices.pushBack(index);
		return true;
	}
	return false;
}

Bool HandleMapBase::set(Handle handle, Void* data)
{
	NV_CORE_ASSERT(data);
	const IndexT numEntries = m_entries.getSize();
	IndexT index = getIndex(handle);
	if (index > 0 && index < numEntries)
	{
		Entry& entry = m_entries[index];
		if (entry.m_handle == handle)
		{
			entry.m_data = data;
			return true;
		}
	}
	return false;
}

Void HandleMapBase::clear()
{
	Entry* cur = m_entries.begin();
	const IndexT size = m_entries.getSize();
	m_freeIndices.setSize(size - 1);
	IndexT* freeIndices = m_freeIndices.begin();

	for (IndexT i = 1; i < size; i++)
	{
		if (cur->m_data)
		{
			cur->changeCount();
			cur->m_data = NV_NULL;
		}
		freeIndices[i - 1] = i;
	}
}

Void HandleMapBase::reset()
{
	m_entries.reset();
	m_freeIndices.reset();
	Entry& entry = m_entries.expandOne();
	entry.m_handle = makeHandle(0, 1);
	entry.m_data = NV_NULL;
}

HandleMapBase::Handle HandleMapBase::getIterator() const
{
	// Find the first handle that is valid
	const IndexT size = m_entries.getSize();
	const Entry* entries = m_entries.begin();
	for (IndexT i = 1; i < size; i++)
	{
		const Entry& entry = entries[i];
		if (entry.m_data)
		{
			return entry.m_handle;
		}
	}
	return 0;
}

HandleMapBase::Handle HandleMapBase::getNext(Handle handle) const
{
	const IndexT numEntries = m_entries.getSize();
	const Entry* entries = m_entries.begin();
	IndexT index = getIndex(handle);
	if (index < numEntries && entries[index].m_handle == handle)
	{
		// First matched, so search for next match (where m_data != NV_NULL)
		for (index++; index < numEntries; index++)
		{
			const Entry& entry = entries[index];
			if (entry.m_data)
			{
				return entry.m_handle;
			}
		}
	}
	return 0;
}

#if NV_DEBUG

namespace { // anonymous

struct Element
{
	HandleMapHandle m_handle;
	Void* m_data;
};

} // anonymous

/* static */Void HandleMapBase::selfTest()
{

	{
		const Char* hello = "Hello";
		const Char* world = "World";

		HandleMap<const Char> map;

		NV_CORE_ASSERT(map.getSize() == 0);

		NV_CORE_ASSERT(map.get(0) == NV_NULL);
		NV_CORE_ASSERT(map.isValid(0) == false);

		HandleMapHandle a = map.add(hello);
		HandleMapHandle b = map.add(world);

		NV_CORE_ASSERT(map.getSize() == 2);

		NV_CORE_ASSERT(map.get(a) == hello);
		NV_CORE_ASSERT(map.get(b) == world);

		NV_CORE_ASSERT(map.isValid(a));

		map.remove(a);

		NV_CORE_ASSERT(map.get(a) == NV_NULL);
		NV_CORE_ASSERT(map.isValid(a) == false);

		NV_CORE_ASSERT(map.getSize() == 1);

		HandleMapHandle a1 = map.add(hello);
		NV_CORE_ASSERT(a1 != a);
		
	}

	{
		HandleMap<Void> map;
		UniquePtr<RandomGenerator> rand(RandomGenerator::create(0x12133));
		Array<Element> elements;
		FreeList freeList(1, 1, 100, NV_NULL);

		for (IndexT i = 0; i < 1000000; i++)
		{
			NV_CORE_ASSERT(map.getSize() == elements.getSize());

			Int32 cmd = rand->nextInt32InRange(0, 100);
			if (cmd < 1)
			{
				elements.clear();
				map.clear();
			}
			else if (cmd < 5)
			{
				IndexT numRemove = rand->nextInt32InRange(1, 5);
				for (IndexT j = 0; j < numRemove && elements.getSize() > 0; j++)
				{
					IndexT index = rand->nextInt32InRange(0, Int32(elements.getSize()));

					const Element& element = elements[index];

					NV_CORE_ASSERT(map.get(element.m_handle) == element.m_data);

					freeList.deallocate(element.m_data);
					map.remove(element.m_handle);
					
					NV_CORE_ASSERT(!map.isValid(element.m_handle));

					// Remove element
					elements.removeAt(index);
				}
			}
			else
			{
				Element& ele = elements.expandOne();

				ele.m_data = freeList.allocate();
				ele.m_handle = map.add(ele.m_data);
			}
		}
	}
}
#endif

} // namespace Common 
} // namespace nvidia

