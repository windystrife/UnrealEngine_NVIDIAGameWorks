/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_ALLOCATE_MACROS_H
#define NV_CO_ALLOCATE_MACROS_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoMemoryAllocator.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

//! Macros used for setting up allocation on types

#define NV_CO_OPERATOR_NEW_PLACEMENT_NEW \
    static NV_FORCE_INLINE void* operator new(::NvSizeT /* size */, void* place) { return place; }  \
    static NV_FORCE_INLINE void operator delete(void* /* place */, void* /*in */) { } 

/// Operator new when the size of the type is not known on delete 
#define NV_CO_OPERATOR_NEW_UNKNOWN_SIZE \
	NV_FORCE_INLINE static void* operator new(::NvSizeT size) \
	{ \
		return ::NvCo::MemoryAllocator::getInstance()->simpleAllocate(size); \
	} \
    NV_FORCE_INLINE static void operator delete(void* data)  \
	{ \
		::NvCo::MemoryAllocator::getInstance()->simpleDeallocate(data); \
	} 

/// Operator new for a type that size does not change, or is known to always be deleted from the most derived type
/// NOTE! Only works if the type defines ThisType, as the NV_DECLARE_CLASS macros do
#define NV_CO_OPERATOR_NEW_KNOWN_SIZE \
	NV_FORCE_INLINE static void* operator new(::NvSizeT size) \
	{ \
		NV_CORE_ASSERT(size == sizeof(ThisType)); \
		return ::NvCo::MemoryAllocator::getInstance()->allocate(size); \
	} \
    NV_FORCE_INLINE static void operator delete(void* data)  \
	{ \
		::NvCo::MemoryAllocator::getInstance()->deallocate(data, sizeof(ThisType)); \
	} 

#define NV_CO_OPERATOR_NEW_DISABLE \
	private: \
	static void* operator new(::NvSizeT size); \
	public: \
	static void operator delete(void*) { NV_CORE_ASSERT(!"Incorrect allocator or use of delete"); } 

#define NV_CO_CLASS_UNKNOWN_SIZE_ALLOC \
	NV_CO_OPERATOR_NEW_UNKNOWN_SIZE \
    NV_CO_OPERATOR_NEW_PLACEMENT_NEW 

#define NV_CO_CLASS_KNOWN_SIZE_ALLOC \
	NV_CO_OPERATOR_NEW_KNOWN_SIZE \
    NV_CO_OPERATOR_NEW_PLACEMENT_NEW 

#define NV_CO_CLASS_NO_ALLOC \
	NV_CO_OPERATOR_NEW_DISABLE

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_ALLOCATE_MACROS_H