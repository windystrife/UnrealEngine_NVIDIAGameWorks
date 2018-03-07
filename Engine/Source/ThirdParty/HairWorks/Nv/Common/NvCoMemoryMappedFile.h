/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_MEMORY_MAPPED_FILE_H
#define NV_MEMORY_MAPPED_FILE_H

#include <Nv/Common/NvCoTypeMacros.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

class MemoryMappedFile 
{
	NV_CO_DECLARE_POLYMORPHIC_CLASS_BASE(MemoryMappedFile);

		/// Get the base address
	NV_FORCE_INLINE void* getBaseAddress() { return m_baseAddress; }
		/// Get the size
	NV_FORCE_INLINE SizeT getSize() const { return m_size; }
		/// Dtor
	virtual ~MemoryMappedFile() {}

		/// Creates a new MemoryMappedFile. Returns NV_NULL if not possible or failed.
	static MemoryMappedFile* create(const char* name, SizeT size);

protected:
	MemoryMappedFile():m_baseAddress(NV_NULL), m_size(0) {}

	Void* m_baseAddress;
	SizeT m_size;
};
	
} // namespace Common 
} // namespace nvidia

/** @} */

#endif
