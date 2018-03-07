/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include <Nv/Common/NvCoCommon.h>

#include "NvCoArrayUtil.h"

#include "NvCoArray.h"
#include <Nv/Common/NvCoMemory.h>

namespace nvidia {
namespace Common { 

// They should be the same size!
NV_COMPILE_TIME_ASSERT(sizeof(Array<UInt>) == sizeof(ArrayUtil::Layout));




/* static */IndexT ArrayUtil::calcCapacityIncrement(IndexT capacity, SizeT elemSize)
{
	if (capacity <= 0)
	{
		// Let's figure that an allocation size < 16 is a waste of time
		// But if the element is huge we might want to reign it in
		// An initial size then
		if (elemSize < 4) return 16;
		if (elemSize < 16) return 4;
		return 1;
	}
	else if (capacity < 4)
	{
		// So we have a small number -> may as well jump if the elemSize is small
		const SizeT currentSize = capacity * elemSize;
		if (currentSize < 4 * 16)
		{
			return 8;
		}
		// Just double then
		return capacity + capacity;
	}
	else if (capacity < 1024)
	{
		// Just double
		return capacity + capacity;
	}
	else
	{
		// This is getting big... shall we reign it in a little
		if (capacity * elemSize > 16 * 1024)
		{
			// Let's go geometrically, but not double
			return capacity + (capacity >> 1);
		}
		else
		{
			// Double it is
			return capacity + capacity;
		}
	}
}

/* static */Void ArrayUtil::setCapacity(Layout& layout, IndexT newCapacity, SizeT elemSize)
{
	NV_CORE_ASSERT(newCapacity >= 0);
	const IndexT capacity = layout.m_capacity;
	if (capacity == newCapacity)
	{
		return;
	}

	MemoryAllocator* allocator = layout.m_allocator;
	Void* data = layout.m_data;
	
	if (newCapacity > capacity)
	{
		if (!allocator)
		{
			allocator = MemoryAllocator::getInstance();
			layout.m_allocator = allocator;
		}

		Void* newData;
		if (data)
		{
			newData = allocator->reallocate(data, capacity * elemSize, layout.m_size * elemSize, newCapacity * elemSize);
		}
		else
		{
			newData = allocator->allocate(newCapacity * elemSize);		
		}

		layout.m_capacity = newCapacity;
		layout.m_data = newData;
	}
	else
	{
		// Only make smaller if have an allocator
		if (allocator)
		{
			Void* newData;
			if (newCapacity == 0)
			{
				allocator->deallocate(data, capacity * elemSize);
				newData = NV_NULL;
			}
			else
			{
				newData = allocator->reallocate(data, capacity * elemSize, layout.m_size * elemSize, newCapacity * elemSize);
			}

			layout.m_capacity = newCapacity;
			layout.m_data = newData;
		}
		else
		{
			// This is 'user data' -> we can't reallocate, and lowering the capacity would just make less of this useful. So ignore.
		}
	}
}

/* static */Void ArrayUtil::ctorSetCapacity(Layout& layout, IndexT capacity, SizeT elemSize, MemoryAllocator* allocator)
{
	NV_CORE_ASSERT(capacity >= 0);
	Void* data = NV_NULL;
	if (capacity > 0)
	{
		allocator = allocator ? allocator : MemoryAllocator::getInstance();
		data = allocator->allocate(elemSize * capacity);
	}

	layout.m_data = data;
	layout.m_capacity = capacity;
	layout.m_size = 0;
	layout.m_allocator = allocator;
}

/* static */Void ArrayUtil::expandCapacity(Layout& layout, IndexT minCapacity, SizeT elemSize)
{
	NV_CORE_ASSERT(layout.m_capacity < minCapacity);
	IndexT nextCapacity = calcCapacityIncrement(layout.m_capacity, elemSize);
	nextCapacity = nextCapacity > minCapacity ? nextCapacity : minCapacity;
	setCapacity(layout, nextCapacity, elemSize);
}

/* static */Void ArrayUtil::expandCapacityByOne(Layout& layout, SizeT elemSize)
{
	NV_CORE_ASSERT(layout.m_capacity <= layout.m_size);

	const IndexT capacity = layout.m_capacity;
	// Calculate the next capacity
	const IndexT nextCapacity = calcCapacityIncrement(capacity, elemSize);
	MemoryAllocator* allocator = layout.m_allocator;
	const IndexT size = layout.m_size;

	NV_CORE_ASSERT(nextCapacity > capacity);

	if (layout.m_data)
	{
		Void* newData;
		if (allocator)
		{
			// We need to realloc...
			newData = allocator->reallocate(layout.m_data, elemSize * capacity, elemSize * size, elemSize * nextCapacity);
		}
		else
		{
			// Get the default allocator
			allocator = MemoryAllocator::getInstance();
			layout.m_allocator = allocator;
			// Allocate memory
			newData = allocator->allocate(nextCapacity * elemSize);
			// Copy over contents
			if (size > 0)
			{
				Memory::copy(newData, layout.m_data, elemSize * size);
			}
		}
		layout.m_data = newData;
		layout.m_capacity = nextCapacity;
	}
	else
	{

		if (!allocator)
		{
			// Get the default allocator
			allocator = MemoryAllocator::getInstance();
			layout.m_allocator = allocator;
		}
		Void* newData = allocator->allocate(nextCapacity * elemSize);
		layout.m_data = newData;
		layout.m_capacity = nextCapacity;
	}
}

} // namespace Common 
} // namespace nvidia

