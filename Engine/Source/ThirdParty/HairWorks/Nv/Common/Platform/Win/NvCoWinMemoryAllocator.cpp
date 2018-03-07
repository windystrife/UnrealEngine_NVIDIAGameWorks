/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoWinMemoryAllocator.h"

#include <stdlib.h>
#include <string.h>

namespace nvidia {
namespace Common {


/* static */WinMemoryAllocator WinMemoryAllocator::s_singleton;

void* WinMemoryAllocator::simpleAllocate(SizeT size)
{
	return ::_aligned_malloc(size, DEFAULT_ALIGNMENT);
}

void WinMemoryAllocator::simpleDeallocate(const void* ptr)
{
	return ::_aligned_free(const_cast<void*>(ptr));
}

void* WinMemoryAllocator::allocate(SizeT size)
{
	if (size < DEFAULT_ALIGNMENT)
	{
		return ::malloc(size);
	}
	else
	{
		return ::_aligned_malloc(size, DEFAULT_ALIGNMENT);
	}
}

void WinMemoryAllocator::deallocate(const void* ptr, SizeT size)
{
	if (size < DEFAULT_ALIGNMENT)
	{
		return ::free(const_cast<void*>(ptr));
	}
	else
	{
		return ::_aligned_free(const_cast<void*>(ptr));
	}
}

void* WinMemoryAllocator::reallocate(void* ptr, SizeT oldSize, SizeT oldUsed, SizeT newSize)
{
	// If the size doesn't change there is nothing to do
	if (oldSize == newSize)
	{
		return ptr;
	}
	// If the ptr is null it's the same as an allocation
	if (ptr == NV_NULL)
	{
		NV_CORE_ASSERT(oldSize == 0 && oldUsed == 0);
		return allocate(newSize);
	}
	
	// As the memory size changes, the alignment can change
	// To make compatible with allocate, and deallocate, we need to move the allocation type
	// around as the size passes the DEFAULT_ALIGNMENT size threshold
	if (oldSize < DEFAULT_ALIGNMENT)
	{
		if (newSize < DEFAULT_ALIGNMENT)
		{
			return ::realloc(ptr, newSize);
		}
		else
		{
			// Need to allocate a new block that is aligned...
			Void* newPtr = ::_aligned_malloc(newSize, DEFAULT_ALIGNMENT);
			// Only copy what was used
			if (oldUsed)
			{
				memcpy(newPtr, ptr, oldUsed);
			}
			// Can now free the old allocation
			::free(ptr);
			return newPtr;
		}
	}
	else
	{
		if (newSize < DEFAULT_ALIGNMENT)
		{
			void* newPtr = ::malloc(newSize);
			if (oldUsed)
			{
				memcpy(newPtr, ptr, oldUsed);
			}
			::_aligned_free(ptr);
			return newPtr;
		}
		else
		{
			return ::_aligned_realloc(ptr, newSize, DEFAULT_ALIGNMENT);
		}
	}
}

void* WinMemoryAllocator::alignedAllocate(SizeT size, SizeT align)
{
	return ::_aligned_malloc(size, align);
}

void WinMemoryAllocator::alignedDeallocate(const void* ptr, SizeT align, SizeT size)
{
	NV_UNUSED(size);
	NV_UNUSED(align);
	return ::_aligned_free(const_cast<void*>(ptr));
}

void* WinMemoryAllocator::alignedReallocate(void* ptr, SizeT align, SizeT oldSize, SizeT oldUsed, SizeT newSize)
{
	NV_UNUSED(oldSize);
	NV_UNUSED(oldUsed);
	return ::_aligned_realloc(ptr, newSize, align);
}

} // namespace Common 
} // namespace nvidia

