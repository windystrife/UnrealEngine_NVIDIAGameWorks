/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_WIN_MEMORY_ALLOCATOR_H
#define NV_CO_WIN_MEMORY_ALLOCATOR_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoMemoryAllocator.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/// Windows implementation of memory allocator
class WinMemoryAllocator: public MemoryAllocator
{
public:
	virtual ~WinMemoryAllocator() {}

	void* simpleAllocate(SizeT size) NV_OVERRIDE;
	void simpleDeallocate(const void* pointer) NV_OVERRIDE;
	void* allocate(SizeT size) NV_OVERRIDE;
	void deallocate(const void* ptr, SizeT size) NV_OVERRIDE;
	void* reallocate(void* ptr, SizeT oldSize, SizeT oldUsed, SizeT newSize) NV_OVERRIDE;
	void* alignedAllocate(SizeT size, SizeT align) NV_OVERRIDE;
	void alignedDeallocate(const void* ptr, SizeT align, SizeT size) NV_OVERRIDE;
	void* alignedReallocate(void* ptr, SizeT align, SizeT oldSize, SizeT oldUsed, SizeT newSize) NV_OVERRIDE;

		/// Get the singleton
	NV_FORCE_INLINE static WinMemoryAllocator* getSingleton() { return &s_singleton; }

private:
	static WinMemoryAllocator s_singleton;

	WinMemoryAllocator() {}
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_WIN_MEMORY_ALLOCATOR_H