/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_MEMORY_H
#define NV_CO_MEMORY_H

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

struct Memory
{
		/// Copy size bytes from src to dst. Note if src and dst overlap, the result is undefined!
		/// @param dst The destination to write the src data to
		/// @param src The source of the data
		/// @param size The amount of bytes to copy from src to dst
	NV_FORCE_INLINE static Void copy(void* NV_RESTRICT dst,  const void* NV_RESTRICT src, SizeT size) { ::memcpy(dst, src, size); }
		/// Zero memory at dst
		/// @param dst The area of memory to zero
		/// @param size The amount of bytes to zero 
	NV_FORCE_INLINE static Void zero(void* NV_RESTRICT dst, SizeT size) { ::memset(dst, 0, size); }
		/// Move bytes from src to dst, taking into account any overlap
		/// @param dst Where to move data to
		/// @param src Where to move from
		/// @param size The amount of data to move
	NV_FORCE_INLINE static Void move(void* dst, void* src, SizeT size) { ::memmove(dst, src, size); }
		/// Set all of the bytes at dst to the lowest byte in value
		/// @param dst Where to write to
		/// @param value The byte value to write 
		/// @param size The amount of bytes to write 
	NV_FORCE_INLINE static Void set(void* dst, Int value, SizeT size) { ::memset(dst, int(value), size); }

		/// Zero the in value type
		/// @param in The value type to zero 
	template <typename T>
	NV_FORCE_INLINE static Void zero(T& in) { ::memset(&in, 0, sizeof(T)); }
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_MEMORY_H
