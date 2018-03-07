/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_ARRAY_H
#define NV_CO_ARRAY_H

#include "NvCoArrayUtil.h"
#include <Nv/Common/NvCoTypeMacros.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common { 

/*! 
\brief An array is a sequential container, similar in many ways to STLs vector type. 
One important difference is that this container assumes that any types it contains 
are ok if their underlying memory is moved. Most types this is not a problem (like int, float etc), 
but a type that contains a pointer to some part of memory contained in itself will not work. 

Pointers and most smart pointers are okay. But the following MyClass example does not work - as m_current can point to 
memory contained in MyClass (m_buffer), if the array moves memory around m_current can become invalid. 

\code{.cpp} 
struct MyClass
{
	MyClass():m_current(m_buffer) {}
	char* m_current;
	char m_buffer[8];
};
\endcode

The Array uses a MemoryAllocator to supply memory to store elements. By default it will use the default MemoryAllocator, 
thats set for MemoryAllocator::getInstance(). The Array can also opporate on a chunk of memory not maintained by the 
MemoryAllocator by passing in the memory to use in a constructor.
*/
template<typename T>
class Array 
{
public:
	typedef ArrayUtil::Layout Layout;
	typedef Array ThisType; 
	NV_CO_CLASS_KNOWN_SIZE_ALLOC

	enum AliasUserMemory
	{
		ALIAS_USER_MEMORY
	};

	typedef T*			Iterator;
	typedef const T*	ConstIterator;

		/// Default array constructor. Initialize an empty array
	NV_FORCE_INLINE Array() : m_allocator(NV_NULL), m_data(NV_NULL), m_size(0), m_capacity(0) {}
		/// Default with allocator specified
	NV_FORCE_INLINE explicit Array(MemoryAllocator* alloc): m_allocator(allocator), m_data(NV_NULL), m_size(0), m_capacity(0) {}

		/// Initialize array with given capacity
	explicit Array(IndexT capacity, MemoryAllocator* allocator = MemoryAllocator::getInstance());
		/// Copy-constructor. Copy all entries from other array
	Array(const ThisType& rhs, MemoryAllocator* allocator = MemoryAllocator::getInstance());
		/// Initialize array with given length
	Array(const T* data, IndexT size, MemoryAllocator* allocator = MemoryAllocator::getInstance());
		
		/// Set up a user array, aliasing over the memory passed in as data
	NV_FORCE_INLINE Array(AliasUserMemory, Void* data, IndexT size, IndexT capacity);

#ifdef NV_HAS_MOVE_SEMANTICS
		/// Move Ctor
	NV_FORCE_INLINE Array(ThisType&& rhs):m_allocator(rhs.m_allocator), m_size(rhs.m_size), m_capacity(rhs.m_capacity), m_data(rhs.m_data) { rhs.m_size = 0; m_allocator = NV_NULL; }
		/// Move Assign
	NV_FORCE_INLINE Array& operator=(ThisType&& rhs) { swap(rhs); }
#endif

		/// Dtor
	NV_FORCE_INLINE ~Array();

		/// Assignment operator. Copy content (deep-copy)
	Array& operator=(const ThisType& rhs);

		/// Indexing operator
	NV_FORCE_INLINE const T& operator[] (IndexT i) const { NV_CORE_ASSERT(i >= 0 && i < m_size); return m_data[i]; }
		/// Const indexing operator
	NV_FORCE_INLINE T& operator[] (IndexT i) { NV_CORE_ASSERT(i >= 0 && i < m_size); return m_data[i]; }

		/// Returns a pointer to the initial element of the array.
	NV_FORCE_INLINE ConstIterator begin() const { return m_data; }
		/// Returns a const pointer to the initial element of the array.
	NV_FORCE_INLINE Iterator begin() { return m_data; }

		/// Returns a const iterator beyond the last element of the array. Do not dereference.
	NV_FORCE_INLINE ConstIterator end() const  { return m_data + m_size; }
		/// Returns a iterator beyond the last element of the array. Do not dereference.
	NV_FORCE_INLINE Iterator end() { return m_data + m_size; }

		/// Returns a const reference to the first element of the array. Undefined if the array is empty.
	NV_FORCE_INLINE const T& front() const { NV_CORE_ASSERT(m_size > 0); return m_data[0]; }
		/// Returns a reference to the first element of the array. Undefined if the array is empty.
	NV_FORCE_INLINE T& front() { NV_CORE_ASSERT(m_size > 0); return m_data[0]; }

		/// Returns a const reference to the last element of the array. Undefined if the array is empty
	NV_FORCE_INLINE const T& back() const  { NV_CORE_ASSERT(m_size); return m_data[m_size - 1]; }
		/// Returns a reference to the last element of the array. Undefined if the array is empty
	NV_FORCE_INLINE T& back() { NV_CORE_ASSERT(m_size); return m_data[m_size - 1]; }

		/// Returns the number of entries in the array. This can, and probably will, differ from the array capacity.		
	NV_FORCE_INLINE IndexT getSize() const { return m_size; }
		/// Get the capacity (allocated memory) for the array.
	NV_FORCE_INLINE IndexT getCapacity() const { return m_capacity; }

		/// Clears the array.
	NV_INLINE void clear();
	
		/// Returns whether the array is empty (i.e. whether its size is 0).
	NV_FORCE_INLINE Bool isEmpty() const { return m_size == 0; }

		/// Finds the index of a, if not found returns -1.
	NV_FORCE_INLINE IndexT indexOf(const T& a) const;

		/// Push back multiple elements to the end.
	void pushBack(const T* start, IndexT num);
		/// Adds one element to the end of the array. O(1).
	T& pushBack(const T& a);
		/// Returns the element at the end of the array. Only legal if the array is non-empty. O(1)
	T popBack();

		/// Add one element to the end of the array
	NV_FORCE_INLINE T& expandOne();
		/// Add num elements to the end of the array, and return the starting address of added area
	T* expandBy(IndexT num);

		/// Remove the element at index i. If i is not 'back' do the removal by copying the last element over i. O(1)
	void removeAt(IndexT i);
		/// Removes the element at index i, and copys back all elements behind it. O(n)
	void removeAtCopyBack(IndexT i);

		/*! Removes a range from the array.  Shifts the array so order is maintained. O(n)
		@param start Starting index of the array
		@param count The number of elements to remove */
	NV_INLINE void removeRange(IndexT start, IndexT count);

		/// Set the size
	void setSize(IndexT size);
		/// Set a size specifying the default value set
	void setSizeWithDefault(IndexT size, const T& def);

		/// Resize array such that only as much memory is allocated to hold the existing elements
	NV_FORCE_INLINE void shrink();
		/// Deletes all array elements and frees memory.
	NV_INLINE void reset();
		/// Ensure that the array has at least size capacity.
	NV_INLINE void reserve(IndexT capacity);

		/// In place swap. Note that this swaps the allocator too.
	NV_FORCE_INLINE void swap(ThisType& rhs) { ((Layout*)this)->swap((Layout*)&rhs); }

		/// Assign a range of values to this vector (resizes to length of range)
	NV_INLINE void set(const T* first, IndexT size);

		/// Detach the contents
	NV_FORCE_INLINE T* detach() { T* out = m_data; m_data = RE4_NULL; m_capacity = 0; m_size = 0; }

		/// == NOTE the comparison is only on contents (they can be equal and have different allocators)
	NV_FORCE_INLINE Bool operator==(const ThisType& rhs) const { return (this == &rhs) || (m_size == rhs.m_size && ArrayUtil::equal(m_data, rhs.m_data, m_size)); }
		/// != NOTE the comparison is only on contents 
	NV_FORCE_INLINE Bool operator!=(const ThisType& rhs) const { return !(*this == rhs); }

		/// Sets the array to alias over the specified user memory
		/// NOTE! Does not release, or dtor any previously set data. It is client responsibility to 
		/// call reset if necessary. Similarly if the array goes out of scope it will run dtors on this data.
		/// So great care must be taken with this method.
	void aliasUserMemory(Void* data, IndexT size, IndexT capacity);

		// If true the memory is not currently managed by an allocator.
	NV_FORCE_INLINE	Bool isUserMemory() const { return m_allocator == NV_NULL; }
		/// Get the allocator
	NV_FORCE_INLINE MemoryAllocator* getAllocator() const { return m_allocator; }

protected:
	/// DO NOT CHANGE THESE MEMBERS without changing ArrayUtil::Layout appropriately
	T* m_data;						///< Pointer to the data held in the array
	MemoryAllocator* m_allocator;	///< Allocator used. If NV_NULL it means the memory is 'user allocated'. 
	IndexT m_size;					///< The size of active members of the array
	IndexT m_capacity;				///< Total amount of underlying space 
};

// ---------------------------------------------------------------------------
template<typename T>
Array<T>::Array(IndexT capacity, MemoryAllocator* allocator)
{
	ArrayUtil::ctorSetCapacity(*(Layout*)this, capacity, sizeof(T), allocator);
}
// ---------------------------------------------------------------------------
template<typename T>
Array<T>::Array(const ThisType& rhs, MemoryAllocator* allocator)
{
	ArrayUtil::ctorSetCapacity(*(Layout*)this, rhs.m_size, sizeof(T), allocator);
	ArrayUtil::ctorArray(m_data, m_data + rhs.m_size, rhs.m_data);
	m_size = rhs.m_size;
}
// ---------------------------------------------------------------------------
template<typename T>
Array<T>::Array(const T* data, IndexT size, MemoryAllocator* allocator)
{
	ArrayUtil::ctorSetCapacity(*(Layout*)this, m_size, sizeof(T), allocator);
	ArrayUtil::ctorArray(m_data, m_data + rhs.m_size, rhs.m_data);
	m_size = size;
}
// ---------------------------------------------------------------------------
template<typename T>
Array<T>::Array(AliasUserMemory, Void* data, IndexT size, IndexT capacity) :
	m_allocator(NV_NULL),
	m_data(data),
	m_size(size),
	m_capacity(capacity)
{
	NV_CORE_ASSERT((SizeT(data) & (NV_ALIGN_OF(T) - 1)) == 0);
}
// ---------------------------------------------------------------------------
template<typename T>
Array<T>::~Array()
{
	if (m_size)
	{
		ArrayUtil::dtor(m_data, m_data + m_size);
	}
	if (m_allocator && m_capacity)
	{
		m_allocator->deallocate(m_data, sizeof(T) * m_capacity);
	}
}
// ---------------------------------------------------------------------------
template<typename T>
Array<T>& Array<T>::operator=(const ThisType& rhs)
{
	if (&rhs != this)
	{
		set(rhs.m_data, rhs.m_size);
	}
	return *this;
}
// ---------------------------------------------------------------------------
template<typename T>
void Array<T>::set(const T* rhsData, IndexT rhsSize)
{
	// Make sure we have the capacity
	if (rhsSize > m_capacity)
		ArrayUtil::setCapacity(*(Layout*)this, rhsSize, sizeof(T));
	if (m_size < rhsSize)
	{
		ArrayUtil::assign(m_data, m_data + m_size, rhsData);
		ArrayUtil::ctorArray(m_data + m_size, m_data + rhsSize, rhsData + m_size);
	}
	else
	{
		ArrayUtil::dtor(m_data + rhsSize, m_data + m_size);
		ArrayUtil::assign(m_data, m_data + rhsSize, rhsData);
	}
	m_size = rhsSize;
}
// ---------------------------------------------------------------------------
template<typename T>
void Array<T>::aliasUserMemory(Void* data, IndexT size, IndexT capacity)
{
	NV_CORE_ASSERT((SizeT(data) & (NV_ALIGN_OF(T) - 1)) == 0);
	m_data = data;
	m_size = size;
	m_capacity = capacity;
	m_allocator = NV_NULL;			// Mark as 'user data'
}
// ---------------------------------------------------------------------------
template<typename T>
void Array<T>::clear() 
{ 
	if (m_size > 0)
	{
		ArrayUtil::dtor(m_data, m_data + m_size);
		m_size = 0;
	}
}
// ---------------------------------------------------------------------------
template<typename T>
IndexT Array<T>::indexOf(const T& a) const
{
	for (IndexT i = 0; i < m_size; i++)
	{
		if (m_data[i] == a)
		{
			return i;
		}
	}
	return -1;
}
// ---------------------------------------------------------------------------
template<typename T>
void Array<T>::removeRange(IndexT start, IndexT count)
{
	NV_CORE_ASSERT(start < m_size);
	NV_CORE_ASSERT((start + count) <= m_size);
	for (IndexT i = 0; i < count; i++)
	{
		m_data[start + i].~T(); // call the destructor on the ones being removed first.
	}
	T* dst = m_data + start; // location we are copying the tail end objects to
	T* src = m_data + start + count; // start of tail objects
	const IndexT moveCount = m_size - (start + count); // compute remainder that needs to be copied down
	for (IndexT i = 0; i < moveCount; i++)
	{
		dst[i] = src[i]; // Assign
		src->~T();		// call the destructor on the old location
	}
	m_size -= count;
}
// ---------------------------------------------------------------------------
template<typename T>
void Array<T>::removeAtCopyBack(IndexT i)
{
	NV_CORE_ASSERT(i < m_size);
	for (T* it = m_data + i; it->~T(), ++i < m_size; ++it)
		new(it) T(m_data[i]);
	--m_size;
}
// ---------------------------------------------------------------------------
template<typename T>
void Array<T>::removeAt(IndexT i)
{
	NV_CORE_ASSERT(i < m_size);
	T& last = m_data[m_size - 1];
	if (i < m_size - 1)
	{
		// Copy down, only if it's not on top of itself
		m_data[i] = last;
	}
	last.~T();
	m_size--;
}
// ---------------------------------------------------------------------------
template<typename T>
T& Array<T>::pushBack(const T& a)
{
	if (m_capacity <= m_size)
	{
		ArrayUtil::expandCapacityByOne(*(Layout*)this, sizeof(T));
	}
	T& dst = m_data[m_size++];
	new ((void*)&dst) T(a);
	return dst;
}
// ---------------------------------------------------------------------------
template<typename T>
void Array<T>::pushBack(const T* start, IndexT num)
{
	if (m_capacity < m_size + num)
	{
		ArrayUtil::expandCapacity(*(Layout*)this, m_size + num, sizeof(T));
	}
	ArrayUtil::ctorArray(m_data + m_size, m_data + m_size + num, start);
	m_size += num;
}
// ---------------------------------------------------------------------------
template<typename T>
T Array<T>::popBack()
{
	NV_CORE_ASSERT(m_size);
	T t = m_data[m_size - 1];
	m_data[--m_size].~T();
	return t;
}
// ---------------------------------------------------------------------------
template<typename T>
T& Array<T>::expandOne()
{
	if (m_capacity <= m_size)
	{
		ArrayUtil::expandCapacityByOne(*(Layout*)this, sizeof(T));
	}
	T* ptr = m_data + m_size++;
	new (ptr)T; // not 'T()' because PODs should not get default-initialized.
	return *ptr;
}

// ---------------------------------------------------------------------------
template<typename T>
T* Array<T>::expandBy(IndexT num)
{
	IndexT newSize = m_size + num;
	if (m_capacity < newSize)
	{
		ArrayUtil::expandCapacity(*(Layout*)this, newSize, sizeof(T));
	}
	T* start = m_data + m_size;
	ArrayUtil::ctorDefault(start, m_data + newSize);
	m_size = newSize;
	return start;
}
// ---------------------------------------------------------------------------
template<typename T>
void Array<T>::setSize(IndexT size)
{
	if (m_size != size)
	{
		if (size > m_size)
		{
			if (size > m_capacity)
			{
				ArrayUtil::setCapacity(*(Layout*)this, size, sizeof(T));
			}
			ArrayUtil::ctorDefault(m_data + m_size, m_data + size);
		}
		else
		{
			ArrayUtil::dtor(m_data + size, m_data + m_size);
		}
		m_size = size;
	}
}
// ---------------------------------------------------------------------------
template<typename T>
void Array<T>::setSizeWithDefault(IndexT size, const T& def)
{
	if (m_size != size)
	{
		if (size > m_size)
		{
			if (size > m_capacity)
			{
				ArrayUtil::setCapacity(*(Layout*)this, size, sizeof(T));
			}
			ArrayUtil::ctor(m_data + m_size, m_data + size - m_size, def);
		}
		else
		{
			ArrayUtil::dtor(m_data + size, m_data + m_size - size);
		}
		m_size = size;
	}
}
// ---------------------------------------------------------------------------
template<typename T>
void Array<T>::shrink()
{
	if (m_size != m_capacity)
	{
		ArrayUtil::setCapacity(*(Layout*)this, m_size, sizeof(T));
	}
}
// ---------------------------------------------------------------------------
template<typename T>
void Array<T>::reserve(IndexT capacity)
{
	if (capacity > m_capacity)
	{
		ArrayUtil::setCapacity(*(Layout*)this, capacity, sizeof(T));
	}
}
// ---------------------------------------------------------------------------
template<typename T>
void Array<T>::reset()
{
	if (m_size > 0)
	{
		ArrayUtil::dtor(m_data, m_data + m_size);
		m_size = 0;
	}
	if (m_allocator && m_capacity > 0)
	{
		m_allocator->deallocate(m_data, sizeof(T) * m_capacity);
		m_data = NV_NULL;
		m_capacity = 0;
	}
}

namespace Op {
// Replace global swap op with method swap
template <typename T>
NV_FORCE_INLINE void swap(Array<T>& a, Array<T>& b) 
{
	a.swap(b);
}
} // namespace Op

} // namespace Common
} // namespace nvidia

/** @} */

#endif
