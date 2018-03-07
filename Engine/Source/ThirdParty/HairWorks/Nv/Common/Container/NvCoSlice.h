/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_SLICE_H
#define NV_CO_SLICE_H

#include <Nv/Common/NvCoCommon.h>

#include "NvCoArrayUtil.h"

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/* 
\brief A slice is like an array with no memory backing, or can be thought of as a 'view' on an array.
 A ConstSlice only allows read access to the contents. */
template <typename T>
class ConstSlice
{
	public:
	typedef ConstSlice ThisType;

		/// Start of data 
	NV_FORCE_INLINE const T* begin() const { return m_data; }
		/// End of the data
	NV_FORCE_INLINE const T* end() const { return m_data + m_size; }
		/// Get the size
	NV_FORCE_INLINE IndexT getSize() const { return m_size; }

		/// Indexing operator
	NV_FORCE_INLINE const T& operator[] (IndexT i) const { NV_CORE_ASSERT(i >= 0 && i < m_size); return m_data[i]; }
		/// Set
	NV_FORCE_INLINE Void set(const T* data, IndexT size) { m_data = const_cast<T*>(data); m_size = size; }
	
		/// Returns first found match starting at 0. Returns -1 if not found.
	IndexT indexOf(const T& in) const;

		/// ==
	NV_FORCE_INLINE Bool operator==(const ThisType& rhs) const { return m_size == rhs.m_size && ArrayUtil::equal(m_data, rhs.m_data, m_size); }
		/// !=
	NV_FORCE_INLINE Bool operator!=(const ThisType& rhs) const { return !(*this == rhs); }

		/// Takes the head number of elements, or size whichever is less. Can use negative numbers to wrap around.
	NV_INLINE ThisType head(IndexT end) const;
		/// Takes the elements from start until the end. Can use negative numbers to wrap around.
	NV_INLINE ThisType tail(IndexT start) const;

		/// Ctor
	template <SizeT SIZE>
	NV_FORCE_INLINE ConstSlice(const T(&in)[SIZE]) :m_data(const_cast<T*>(in)), m_size(IndexT(SIZE)) {}
		/// Default Ctor
	NV_FORCE_INLINE ConstSlice():m_data(NV_NULL), m_size(0) {}
		/// Ctor with data and size
	NV_FORCE_INLINE ConstSlice(const T* data, IndexT size): m_data(const_cast<T*>(data)), m_size(size) {}

	protected:
	T* m_data;				///< Not const so Slice can derive from it
	IndexT m_size;			///< Size
};

// ---------------------------------------------------------------------------
template <typename T>
ConstSlice<T> ConstSlice<T>::head(IndexT end) const
{
	const IndexT size = m_size;
	if (end < 0)
	{
		end = size + end;
		end = (end < 0) ? 0 : end;
	}
	else
	{
		end = (end > size) ? size : end;
	}
	return ThisType(m_data, end);
}
// ---------------------------------------------------------------------------
template <typename T>
ConstSlice<T> ConstSlice<T>::tail(IndexT start) const
{
	const IndexT size = m_size;
	if (start < 0)
	{
		start = size + start;
		start = (start < 0) ? 0 : start;
	}
	else
	{
		start = (start > size) ? size : start;
	}
	return ThisType(m_data + start, size - start);
}
// ---------------------------------------------------------------------------
template<typename T>
IndexT ConstSlice<T>::indexOf(const T& a) const
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

/*
\brief A Slice extends a ConstSlice as it now allows access to the members contents. The actual Slice itself is immutable. */
template <typename T>
class Slice: public ConstSlice<T>
{
	public:
	typedef ConstSlice<T> Parent;
	typedef Slice ThisType;

	using Parent::begin;
	using Parent::end;
	using Parent::operator[];

		/// Start of data 
	NV_FORCE_INLINE T* begin() const { return m_data; }
		/// End of the data
	NV_FORCE_INLINE T* end() const { return m_data + m_size; }

		/// Indexing operator
	NV_FORCE_INLINE T& operator[] (IndexT i) { NV_CORE_ASSERT(i >= 0 && i < m_size); return m_data[i]; }

		/// Set
	NV_FORCE_INLINE Void set(T* data, IndexT size) { m_data = data; m_size = size; }

		/// Takes the head number of elements, or size whichever is less. Can use negative numbers to wrap around.
	NV_FORCE_INLINE ThisType head(IndexT end) const { return static_cast<const ThisType&>(Parent::head(end)); }
		/// Takes the elements from start until the end. Can use negative numbers to wrap around.
	NV_FORCE_INLINE ThisType tail(IndexT start) const { return static_cast<const ThisType&>(Parent::tail(end)); }

		/// Ctor
	template <SizeT SIZE>
	NV_FORCE_INLINE Slice(T(&in)[SIZE]) :Parent(in, SIZE) {}
		/// Default Ctor
	Slice(): Parent() {}
		/// Ctor with data and size
	NV_FORCE_INLINE Slice(T* data, IndexT size) : m_data(data), m_size(size) {}
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_SLICE_H