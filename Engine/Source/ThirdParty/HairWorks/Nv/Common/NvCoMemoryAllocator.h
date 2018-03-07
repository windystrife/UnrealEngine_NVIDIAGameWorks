/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_MEMORY_ALLOCATOR_H
#define NV_CO_MEMORY_ALLOCATOR_H

#include <Nv/Common/NvCoCommon.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/// Memory allocator interface. All methods must be thread safe.
/// In general to use a user specific allocation system this interface must be implemented.
/// On most platforms the getDefault() method will return a default implementation.
/// To allocate memory in normal usage, you would do something like
/// 
/// MemoryAllocator* allocator = MemoryAllocator::getInstance();
/// const SizeT size = 100; 
/// void* mem = allocator->allocate(size);
/// allocator->deallocate(mem, size);
class MemoryAllocator
{
public:
	enum 
	{
		/// For any allocation greater in size than the default alignment, the alignment is guaranteed 
		/// to be on that alignment. For any allocation less, the alignment is guarenteed to be sizeof(void*) or
		/// larger
		DEFAULT_ALIGNMENT = 16,
	};

	virtual ~MemoryAllocator() {}

		/*! Allocate heap memory. The alignment is at least DEFAULT_ALIGNMENT if the size >= DEFAULT_ALIGNMENT.
		@param size The size of allocation in bytes - zero sized is allowed.
		@return The allocated memory
		Zero-sized allocation is allowed. */
   	virtual void* simpleAllocate(SizeT size) = 0;
		/*! Deallocate heap memory without having to know the size of the original allocation.
		@param pointer Value must previously have been returned by simpleAllocate()
		Passing a null pointer to free is allowed. */
	virtual void simpleDeallocate(const void* pointer) = 0;

		/*! Allocate heap memory. The alignment is at least DEFAULT_ALIGNMENT if the size >= DEFAULT_ALIGNMENT.
		@param size The size of allocation in bytes - zero sized is allowed.
		@return The allocated memory
		Zero-sized allocation is allowed. */
   	virtual void* allocate(SizeT size) = 0;
		/*! Deallocate heap memory.
		@param ptr Value must previously have been returned by allocate()
		@param size Size of the allocation - must be identical to the size passed to allocate.
		Passing a null pointer to free is allowed. */
	virtual void deallocate(const void* ptr, SizeT size) = 0;
		/*! Reallocate a piece of memory allocated with size. 
		@param ptr Points the memory allocated with sizedAllocate, or NV_NULL (then equivalent to allocate)
		@param oldSize Size of the old allocation - must be identical to the size from sizedAllocate 
		@param oldUsed Amount of memory used previously (0 <= oldUsed <= oldSize). Hint to control copy size, set to oldSize if unknown.
		@param newSize New size of memory required
		@return Allocation of newSize, holding same data as was in memory at ptr to oldUsed */
	virtual void* reallocate(void* ptr, SizeT oldSize, SizeT oldUsed, SizeT newSize) = 0;

		/*! Allocate heap memory. Alignment is at least as good as align.
		@param size The size of allocation in bytes - zero sized is allowed.
		@param align The alignment - must be a power of 2 and >= 1
		@return The allocated memory */
   	virtual void* alignedAllocate(SizeT size, SizeT align) = 0;
		/*! Deallocate heap memory.
		@param ptr Value must previously have been returned by allocate(). NV_NULL is accepted.
		@param align Must be the same alignment as the memory was allocated with
		@param size Size of the allocation - must be identical to the size passed to allocate. */
	virtual void alignedDeallocate(const void* ptr, SizeT align, SizeT size) = 0;
		/*! Reallocate a piece of memory allocated with size. 
		@param ptr Points the memory allocated with sizedAllocate, or NV_NULL (then equivalent to allocate)
		@param align Must be the same alignment as ptrs allocation.
		@param oldSize Must be the size of ptrs allocation.
		@param oldUsed Amount of memory used previously (0 <= oldUsed <= oldSize). Hint to control copy size, set to oldSize if unknown.
		@param newSize New size of the chunk of memory wanted
		@return Allocation of newSize, holding same data as was in memory at ptr to oldUsed */
	virtual void* alignedReallocate(void* ptr, SizeT align, SizeT oldSize, SizeT oldUsed, SizeT newSize) = 0;

		/// Get the instance
	NV_FORCE_INLINE static MemoryAllocator* getInstance() { return s_instance;  }
		/// Set the instance
	NV_FORCE_INLINE static void setInstance(MemoryAllocator* alloc) { s_instance = alloc;  }

		/// Get the default for this platform. May return NV_NULL if not defined
	static MemoryAllocator* getDefault();

private:
	static MemoryAllocator* s_instance;
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_MEMORY_ALLOCATOR_H