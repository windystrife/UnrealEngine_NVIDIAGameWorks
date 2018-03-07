/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include <Nv/Common/NvCoCommon.h>

#include "NvCoQueueUtil.h"

#include "NvCoQueue.h"
#include <Nv/Common/NvCoMemory.h>

namespace nvidia {
namespace Common { 


/* static */Void QueueUtil::increaseCapacity(Void* queueIn, IndexT extraCapacity, SizeT extraCapacityInBytes)
{
	if (extraCapacity <= 0)
	{
		return;
	}
	Queue<UInt8>& queue = *(Queue<UInt8>*)queueIn;

	IndexT capacity = queue.m_capacity;
	UInt8* back = queue.m_back;
	UInt8* front = queue.m_front;
	UInt8* data = queue.m_data;
	UInt8* end = queue.m_end;
	const IndexT size = queue.m_size;

	MemoryAllocator* allocator = queue.m_allocator;

	if (capacity == 0)
	{
		NV_CORE_ASSERT(size == 0);
		data = (UInt8*)allocator->allocate(extraCapacityInBytes);
		end = data + extraCapacityInBytes;
		// set back to end, as it can never be data
		front = data;
		back = end;
	}
	else
	{
		const SizeT capacityInBytes = SizeT(end - data);
		const SizeT newCapacityInBytes = capacityInBytes + extraCapacityInBytes;
		
		{
			UInt8* newData = (UInt8*)allocator->reallocate(data, capacityInBytes, capacityInBytes, newCapacityInBytes);
			// Fix the pointers if the data has shifted
			PtrDiffT fix = newData - data;
			data = newData;
			end = data + newCapacityInBytes;
			front += fix;
			back += fix;
		}

		// Copy the contents to make the queue valid again
		if (size > 0 && back <= front)
		{
			SizeT inFrontInBytes = SizeT(back - data);
			if (inFrontInBytes > extraCapacityInBytes)
			{
				// Shift Some to the end, and shift some down to the front
				Memory::move(data + capacityInBytes, data, extraCapacityInBytes);
				Memory::move(data, data + extraCapacityInBytes, inFrontInBytes - extraCapacityInBytes);
				back = data + inFrontInBytes - extraCapacityInBytes;
			}
			else
			{
				// Copy to the end, there is enough space for everything in front
				Memory::move(data + capacityInBytes, data, inFrontInBytes);
				back = data + capacityInBytes + inFrontInBytes;
			}
		}
	}

	queue.m_data = data;
	queue.m_end = end;
	queue.m_front = front;
	queue.m_back = back;
	queue.m_capacity += extraCapacity;
}

} // namespace Common 
} // namespace nvidia

