/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_WEAK_COM_PTR_H
#define NV_CO_WEAK_COM_PTR_H

#include "NvCoComTypes.h"

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

template <class T>
class WeakComPtr
{
public:
	typedef T Type;
	typedef IForwardUnknown* Ptr;
	typedef WeakComPtr ThisType;

		/// Constructors
	NV_FORCE_INLINE WeakComPtr(T* ptr = NV_NULL) :m_ptr(ptr) { }

	// !!! Operators !!!

	  /// Returns the dumb pointer
	NV_FORCE_INLINE operator T *() const { return m_ptr; }

	NV_FORCE_INLINE T& operator*() { return *m_ptr; }
		/// Gets the address of the dumb pointer.
	NV_FORCE_INLINE T** operator&() { return &m_ptr; }
		/// For making method invocations through the smart pointer work through the dumb pointer
	NV_FORCE_INLINE T* operator->() const { return m_ptr; }

		/// Assign
	NV_FORCE_INLINE T* operator=(T* in) { m_ptr = in; }

		/// Get the pointer
	NV_FORCE_INLINE T* get() const { return m_ptr; }
		/// Set to null
	NV_FORCE_INLINE void setNull() { m_ptr = NV_NULL; }

		/// Get ready for writing (nulls contents)
	NV_FORCE_INLINE T** writeRef() { return &m_ptr; }
		/// Get for read access
	NV_FORCE_INLINE T*const* readRef() const { return &m_ptr; }

		/// Call Release on the pointer if set, and then set to NV_NULL
	NV_FORCE_INLINE Void release();

		/// Swap
	Void swap(ThisType& rhs);

protected:
	T* m_ptr;
};

//----------------------------------------------------------------------------
template <typename T>
Void WeakComPtr<T>::swap(ThisType& rhs)
{
	T* tmp = m_ptr;
	m_ptr = rhs.m_ptr;
	rhs.m_ptr = tmp;
}
//----------------------------------------------------------------------------
template <typename T>
Void WeakComPtr<T>::release()
{
	if (m_ptr)
	{
		((Ptr)m_ptr)->Release();
		m_ptr = NV_NULL;
	}
}

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_WEAK_COM_PTR_H
