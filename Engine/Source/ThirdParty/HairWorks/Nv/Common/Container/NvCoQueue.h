/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_QUEUE_H
#define NV_CO_QUEUE_H

#include "NvCoArrayUtil.h"
#include "NvCoQueueUtil.h"
#include <Nv/Common/NvCoTypeMacros.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common { 

// Range provides a fast and easy way to iterate over a queue. By iterating from start0 to end0, then from start1 to end1, you will 
// iterate over all member in order (from start to back).
template <typename T>
struct QueueRange
{
	T* m_start0;
	T* m_end0;
	T* m_start1;
	T* m_end1;
};

template <typename T>
struct ConstQueueRange
{
	const T* m_start0;
	const T* m_end0;
	const T* m_start1;
	const T* m_end1;
};

/*! 
\brief A queue container exhibiting first in last out behavior. The implementation is based on a circular queue, with members held in an
array which can wrap around. Elements can also be accessed by index from the front of the queue. Generally speaking elements are pushed 
on the front and popped from the back. */
template<typename T>
class Queue 
{
public:
	friend struct QueueUtil;

	typedef Queue ThisType; 
	NV_CO_CLASS_KNOWN_SIZE_ALLOC

		/// Ctor with optional allocator
	explicit Queue(MemoryAllocator* allocator = MemoryAllocator::getInstance());
		/// Dtor
	~Queue();		

		/// Create an element at the back, and return it initialized
	NV_FORCE_INLINE T& expandBack(); 
		// Push onto back
	NV_FORCE_INLINE Void pushBack(const T& value);
		/// Pop from the back
	NV_FORCE_INLINE Void popBack();
		/// Get the back element
	NV_FORCE_INLINE T& back() { NV_CORE_ASSERT(m_size > 0); return m_back[-1]; }
	NV_FORCE_INLINE const T& back() const { NV_CORE_ASSERT(m_size >0); return m_back[-1]; }

		/// Create an element at the front and return it initialized
	NV_FORCE_INLINE T& expandFront();
		/// Push onto front
	NV_FORCE_INLINE Void pushFront(const T& value);
		/// Pop from the front
	NV_FORCE_INLINE Void popFront();
		/// Get the front element
	NV_FORCE_INLINE T& front() { NV_CORE_ASSERT(m_size > 0); return *m_front; }
	NV_FORCE_INLINE const T& front() const { NV_CORE_ASSERT(m_size > 0); return *m_front; }

		/// Get the const range
	NV_FORCE_INLINE ConstQueueRange<T> getRange() const;
	NV_FORCE_INLINE QueueRange<T> getRange();

		/// Index into queue
	NV_FORCE_INLINE T& operator[](IndexT index);
	NV_FORCE_INLINE const T& operator[](IndexT index) const { return const_cast<ThisType*>(this)->operator[index]; }

		/// True if empty
	NV_FORCE_INLINE Bool isEmpty() const { return m_size <= 0; }

		/// Clear the contents
	NV_FORCE_INLINE Void clear();

		/// Get the capacity
	NV_FORCE_INLINE IndexT getCapacity() const { return m_capacity; }
		/// Get the size
	NV_FORCE_INLINE IndexT getSize() const { return m_size; }

		/// True is the data is linear
	NV_FORCE_INLINE Bool isLinear() const { return m_size <= 1 || (m_front == m_data || m_front < m_back); }

		/// Increase capacity by specified amount
	NV_FORCE_INLINE Void increaseCapacity(IndexT extraCapacity) { NV_CORE_ASSERT(extraCapacity >= 0); QueueUtil::increaseCapacity(this, extraCapacity, extraCapacity * sizeof(T)); }
		/// Increase capacity by at least one unit
	NV_FORCE_INLINE Void increaseCapacity();

		// If true the memory is not currently managed by an allocator.
	NV_FORCE_INLINE	Bool isUserMemory() const { return m_allocator == NV_NULL; }
		/// Get the allocator
	NV_FORCE_INLINE MemoryAllocator* getAllocator() const { return m_allocator; }

protected:
	Void _deleteElements();

	T* m_data;						///< Pointer to the data held in the array
	T* m_end;						///< m_data + m_capacity
	MemoryAllocator* m_allocator;	///< Allocator used. If NV_NULL it means the memory is 'user allocated'. 
	
	T* m_front;						///< Front element
	T* m_back;						///< m_back[-1] is the last valid element. Min value is '1' (ie it's never 0 if capacity > 0)
	
	IndexT m_size;					///< The size of active members of the array
	IndexT m_capacity;				///< Total amount of underlying space 
};

// ---------------------------------------------------------------------------
template<typename T>
Queue<T>::Queue(MemoryAllocator* allocator):
	m_data(NV_NULL),
	m_end(NV_NULL),
	m_capacity(0),
	m_front(NV_NULL),
	m_back(NV_NULL),
	m_size(0),
	m_allocator(allocator)
{
}
// ---------------------------------------------------------------------------
template<typename T>
Queue<T>::~Queue()
{	
	_deleteElements();
	if (m_allocator && m_capacity)
	{
		m_allocator->deallocate(m_data, SizeT((UInt8*)m_end - (UInt8*)m_data));
	}
}
// ---------------------------------------------------------------------------
template<typename T>
Void Queue<T>::_deleteElements()
{
	if (m_back < m_front)
	{
		ArrayUtil::dtor(m_front, m_end);
		ArrayUtil::dtor(m_data, m_back);
	}
	else
	{
		ArrayUtil::dtor(m_front, m_back);
	}
}
// ---------------------------------------------------------------------------
template<typename T>
NV_FORCE_INLINE ConstQueueRange<T> Queue<T>::getRange() const
{
	if (m_back < m_front)
	{
		ConstRange range = { m_front, m_end, m_data, m_back };
		return range;
	}
	else
	{
		ConstRange range = { m_front, m_back, m_end, m_end };
		return range;
	}
}
// ---------------------------------------------------------------------------
template<typename T>
NV_FORCE_INLINE QueueRange<T> Queue<T>::getRange()
{
	if (m_back < m_front)
	{
		ConstRange range = { m_front, m_end, m_data, m_back };
		return range;
	}
	else
	{
		ConstRange range = { m_front, m_back, m_end, m_end };
		return range;
	}
}
// ---------------------------------------------------------------------------
template<typename T>
Void Queue<T>::clear()
{
	_deleteElements();
	m_front = m_data;
	m_back = m_end;
	m_size = 0;
}
// ---------------------------------------------------------------------------
template<typename T>
T& Queue<T>::operator[](IndexT index)
{
	NV_CORE_ASSERT(index >= 0 && index < m_size);
	T* cur = m_front + index;
	cur = (cur >= m_end) ? (cur - m_end + m_data) : cur;
	return *cur;
}
// ---------------------------------------------------------------------------
template<typename T>
Void Queue<T>::increaseCapacity()
{
	if (m_capacity == 0)
	{
		increaseCapacity(8);
	}
	else
	{
		IndexT extraCapacity = ((m_capacity * sizeof(T)) > 4 * 1024) ? (m_capacity >> 1) : m_capacity;
		increaseCapacity(extraCapacity);
	}
}
// ---------------------------------------------------------------------------
template<typename T>
Void Queue<T>::pushBack(const T& value)
{
	if (m_size >= m_capacity)
	{
		increaseCapacity();
	}
	T* back = m_back;
	back = (back == m_end) ? m_data : back;
	new (back) T(value);
	m_back = back + 1;
	++m_size;
}
// ---------------------------------------------------------------------------
template<typename T>
T& Queue<T>::expandBack()
{
	if (m_size >= m_capacity)
	{
		increaseCapacity();
	}
	T* back = m_back;
	back = (back == m_end) ? m_data : back;
	new (back) T;
	m_back = back + 1;
	++m_size;
	return *back;
}
// ---------------------------------------------------------------------------
template<typename T>
Void Queue<T>::popBack()
{
	NV_CORE_ASSERT(m_size > 0);
	T* back = m_back - 1;
	back->~T();
	back = (back == m_data) ? m_end : back;
	m_back = back;
	m_size--;
}
// ---------------------------------------------------------------------------
template<typename T>
Void Queue<T>::pushFront(const T& value)
{
	if (m_size >= m_capacity)
	{
		increaseCapacity();
	}
	T* front = m_front;
	front = (front == m_data) ? m_end : front;
	--front;
	new (front) T(value);
	m_front = front;
	++m_size;
}
// ---------------------------------------------------------------------------
template<typename T>
T& Queue<T>::expandFront()
{
	if (m_size >= m_capacity)
	{
		increaseCapacity();
	}
	T* front = m_front;
	front = (front == m_data) ? m_end : front;
	--front;
	new (front) T;
	m_front = front;
	++m_size;
	return *front;
}
// ---------------------------------------------------------------------------
template<typename T>
Void Queue<T>::popFront()
{
	NV_CORE_ASSERT(m_size > 0);
	T* front = m_front;
	front->~T();
	front++;
	m_front = (front == m_end) ? m_data : front;
	--m_size;
}

} // namespace Common
} // namespace nvidia

/** @} */

#endif
