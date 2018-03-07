/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_POD_BUFFER_H
#define NV_CO_POD_BUFFER_H

#include <Nv/Common/NvCoMemoryAllocator.h>
#include <Nv/Common/NvCoTypeMacros.h>
#include <Nv/Common/NvCoMemory.h>
#include "NvCoArrayUtil.h"

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/*! 
\brief A buffer for holding POD (Plain Ordinary Data) types. The buffer is really just a memory repository, 
generally for a fixed amount of items. This is a slightly more flexible version of C++ built in fixed array type.
Although the size of the buffer can be adjusted once the buffer is created, the contents of the buffer is 
generally lost. Typical usage is to create the buffer of the appropriate size once and then fill in the
contents.
NOTE! because the types are POD - they will not have ctors or dtors run */
template <typename T>
class PodBuffer
{
	public:
	typedef PodBuffer ThisType;
	NV_CO_CLASS_KNOWN_SIZE_ALLOC

		/// Conversions to a pointer
	NV_FORCE_INLINE operator T*() { return m_data; }
	NV_FORCE_INLINE operator const T*() const { return m_data; }

		/// Get the amount of elements in the buffer
	NV_FORCE_INLINE IndexT getSize() const { return m_size; }
		/// Set the size with the amount of elements. NOTE! Setting size does not maintain contents. Use resize if you want the contents to remain.
	Void setSize(IndexT size);
		
		/// Changes size and keeps contents
	Void resize(IndexT size);

		/// Set contents to be exactly this
	Void set(const T* in, IndexT size);

		/// Start iterator
	NV_FORCE_INLINE T* begin() { return m_data; }
		/// Const start iterator
	NV_FORCE_INLINE const T* begin() const { return m_data; }

		/// End iterator
	NV_FORCE_INLINE T* end() { return m_data + m_size; }
		/// Const end iterator
	NV_FORCE_INLINE const T* end() const { return m_data + m_size; }

		/// [] 
	T& operator[](IndexT i) { NV_CORE_ASSERT(i >= 0 && i < m_size); return m_data[i]; }
		/// [] const
	const T& operator[](IndexT i) const { NV_CORE_ASSERT(i >= 0 && i < m_size); return m_data[i]; }

		/// Returns the index of element - as searched for from the start of the buffer
		/// @param in The element to search for.
		/// @return index of element. -1 if not found.
	IndexT indexOf(const T& in) const;

		/// ==
	Bool operator==(const ThisType& rhs) const;
		/// !=
	NV_FORCE_INLINE Bool operator!=(const ThisType& rhs) const { return !(*this == rhs);}
		
		/// Zero all of the contents
	Void zero() { if (m_size > 0 ) { Memory::zero(m_data, sizeof(T) * m_size); } }
		/// Assignment
	Void operator=(const ThisType& rhs);

		/// Inplace swap
	Void swap(ThisType& rhs) { Op::swap(m_data, rhs.m_data); Op::swap(m_size, rhs.m_size); Op::swap(m_allocator, rhs.m_allocator); }

		/// ==
	NV_FORCE_INLINE Bool operator==(const ThisType& rhs) { return (this == &rhs) || (m_size == rhs.m_size && ArrayUtil::equal(m_data, rhs.m_data, m_size)); }
		/// !=
	NV_FORCE_INLINE Bool operator!=(const ThisType& rhs) { return !(*this == rhs); }

		/// Copy ctor - copys using the default memory allocator
	PodBuffer(const ThisType& rhs, MemoryAllocator* allocator = MemoryAllocator::getInstance());
		/// Default Ctor
	PodBuffer():m_data(NV_NULL), m_size(0), m_allocator(MemoryAllocator::getInstance()) {}
		/// Ctor with the amount of elements wanted
	explicit PodBuffer(IndexT size, MemoryAllocator* allocator = MemoryAllocator::getInstance());

#ifdef NV_HAS_MOVE_SEMANTICS
	/// Move Ctor
	NV_FORCE_INLINE PodBuffer(ThisType&& rhs): m_allocator(rhs.m_allocator), m_size(rhs.m_size), m_data(rhs.m_data) { m_data = NV_NULL; }
	/// Move Assign
	NV_FORCE_INLINE PodBuffer& operator=(ThisType&& rhs) { swap(rhs); }
#endif

		/// Dtor
	NV_FORCE_INLINE ~PodBuffer();

	private:
	T* m_data;						///< Holds pointer to the start of the data in buffer. Can be NV_NULL if m_size = 0.
	IndexT m_size;					///< Size of the buffer
	MemoryAllocator* m_allocator;	///< The allocator used to manage m_data.
};

// ---------------------------------------------------------------------------
template <typename T>
PodBuffer<T>::PodBuffer(IndexT size, MemoryAllocator* allocator) :
	m_size(size),
	m_allocator(allocator),
	m_data(NV_NULL)
{
	NV_CORE_ASSERT(size >= 0);
	if (size > 0)
	{ 
		m_data = (T*)allocator->allocate(sizeof(T) * size);
	}
}
// ---------------------------------------------------------------------------
template <typename T>
PodBuffer<T>::~PodBuffer()
{
	if (m_data)
	{
		m_allocator->deallocate(m_data, sizeof(T) * m_size);
	}
}
// ---------------------------------------------------------------------------
template <typename T>
PodBuffer<T>::PodBuffer(const ThisType& rhs, MemoryAllocator* allocator) :
	m_size(rhs.m_size),
	m_allocator(allocator),
	m_data(NV_NULL)
{
	if (m_size > 0)
	{
		m_data = (T*)allocator->allocate(m_size * sizeof(T));
		Memory::copy(m_data, rhs.m_data, sizeof(T) * rhs.m_size);
	}
}
// ---------------------------------------------------------------------------
template <typename T>
Void PodBuffer<T>::setSize(IndexT size)
{
	NV_CORE_ASSERT(size >= 0);
	if (m_size == size)
	{
		return;
	}
	if (m_data)
	{
		m_allocator->deallocate(m_data, sizeof(T) * m_size);
	}
	m_data = NV_NULL;
	m_size = size;
	if (size > 0)
	{
		m_data = (T*)m_allocator->allocate(sizeof(T) * size);
	}
}
// ---------------------------------------------------------------------------
template <typename T>
Void PodBuffer<T>::resize(IndexT size)
{
	NV_CORE_ASSERT(size >= 0);
	if (m_size == size)
	{
		return;
	}
	
	if (size == 0)
	{
		if (m_data)
		{
			m_allocator->deallocate(m_data, sizeof(T) * m_size);
			m_data = NV_NULL;
		}
	}
	else
	{ 
		if (m_data)
		{
			m_data = (T*)m_allocator->reallocate(m_data, sizeof(T) * m_size, sizeof(T) * m_size, sizeof(T) * size);
		}
		else
		{
			m_data = (T*)m_allocator->allocate(sizeof(T) * size);
		}
	}
	m_size = size;
}

// ---------------------------------------------------------------------------
template <typename T>
Void PodBuffer<T>::set(const T* in, IndexT size)
{
	setSize(size);
	Memory::copy(m_data, in, sizeof(T) * size);
}
// ---------------------------------------------------------------------------
template <typename T>
IndexT PodBuffer<T>::indexOf(const T& in) const
{
	for (IndexT i = 0; i < m_size; i++)
	{
		if (m_data[i] == in)
		{
			return i;
		}
	}
	return -1;
}
// ---------------------------------------------------------------------------
template <typename T>
Void PodBuffer<T>::operator=(const ThisType& rhs)
{
	if (this == &rhs)
	{
		// Assignment to self does nothing
		return;
	}
	// Only need to reallocate if the size is different
	if (m_size != rhs.m_size)
	{
		if (m_data)
		{
			m_allocator->deallocate(m_data, sizeof(T) * m_size);
			m_data = NV_NULL;
		}
		if (rhs.m_size > 0)
		{
			m_data = (T*)m_allocator->allocate(sizeof(T) * rhs.m_size);
		}
		m_size = rhs.m_size;
	}
	// Do the actual copy
	Memory::copy(m_data, rhs.m_data, sizeof(T) * m_size);
}
// ---------------------------------------------------------------------------
template <typename T>
Bool PodBuffer<T>::operator==(const ThisType& rhs) const
{
	if (this == &rhs)
	{
		return true;
	}
	if (rhs.m_size != m_size)
	{
		return false;
	}
	/// You can't do a memory compare here generally as the PODs can still have 
	/// holes due to alignment issues. So assume the the pod has == and compare
	const T* a = m_data;
	const T* b = rhs.m_data;
	for (IndexT i = 0; i < m_size; i++)
	{
		if (a[i] != b[i])
		{
			return false;
		}
	}
	return true;
}

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_POD_BUFFER_H
