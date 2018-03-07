/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_UNIQUE_PTR_H
#define NV_CO_UNIQUE_PTR_H

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/*! Similiar to stl::unique_ptr, but simpler in that doesn't support user defined delete functions.
 The functionality is such that only one UniquePtr can hold a reference at any one time, and 
 assignment will MOVE who owns the pointer. */
template <class T>
class UniquePtr
{
public:
	typedef T Type;
	typedef UniquePtr ThisType;

		/// Default Ctor. 
	NV_FORCE_INLINE UniquePtr() :m_ptr(NV_NULL) {}
		/// Ctor with dumb ptr.
	NV_FORCE_INLINE explicit UniquePtr(T* ptr) :m_ptr(ptr) { }
		/// Copy ctor
	NV_FORCE_INLINE UniquePtr(const ThisType& rhs) : m_ptr(rhs.m_ptr) { rhs.m_ptr = NV_NULL; }

#ifdef NV_HAS_MOVE_SEMANTICS
		/// Move Ctor
	NV_FORCE_INLINE UniquePtr(ThisType&& rhs) : m_ptr(rhs.m_ptr) { rhs.m_ptr = NV_NULL; }
		/// Move assign
	NV_FORCE_INLINE ThisType& operator=(ThisType&& rhs) { T* swap = m_ptr; m_ptr = rhs.m_ptr; rhs.m_ptr = swap; return *this; }
#endif

		/// Destructor deletes the pointer if set
	NV_FORCE_INLINE ~UniquePtr() { if (m_ptr) delete m_ptr; }

	  /// Returns the dumb pointer
	NV_FORCE_INLINE operator T *() const { return m_ptr; }

	NV_FORCE_INLINE T& operator*() { return *m_ptr; }
		/// Gets the address of the dumb pointer.
	NV_FORCE_INLINE T** operator&();
		/// For making method invocations through the smart pointer work through the dumb pointer
	NV_FORCE_INLINE T* operator->() const { return m_ptr; }

		/// Assign (moves from rhs!)
	NV_FORCE_INLINE ThisType& operator=(ThisType& rhs);
		/// Assign from a dumb ptr
	NV_FORCE_INLINE T* operator=(T* in);

		/// Get the pointer and don't ref
	NV_FORCE_INLINE T* get() const { return m_ptr; }
		/// Release a contained NV_NULL pointer if set
	NV_FORCE_INLINE void setNull();

		/// Detach
	NV_FORCE_INLINE T* detach() { T* ptr = m_ptr; m_ptr = NV_NULL; return ptr; }

	/// Swap
	Void swap(ThisType& rhs);

protected:
	T* m_ptr;
};

//----------------------------------------------------------------------------
template <typename T>
Void UniquePtr<T>::setNull()
{
	if (m_ptr)
	{
		delete m_ptr;
		m_ptr = NV_NULL;
	}
}
//----------------------------------------------------------------------------
template <typename T>
T** UniquePtr<T>::operator&()
{
	NV_CORE_ASSERT(m_ptr == NV_NULL);
	return &m_ptr;
}
//----------------------------------------------------------------------------
template <typename T>
UniquePtr<T>& UniquePtr<T>::operator=(ThisType& rhs)
{	
	// Need the casts to avoid the operator& being invoked
	if (this != (ThisType*)&reinterpret_cast<char&>(rhs))
	{
		if (m_ptr)
		{
			delete m_ptr;
		}
		m_ptr = rhs.m_ptr;
		rhs.m_ptr = NV_NULL;
	}
	return *this;
}
//----------------------------------------------------------------------------
template <typename T>
T* UniquePtr<T>::operator=(T* ptr)
{
	NV_CORE_ASSERT(ptr == NV_NULL || m_ptr != ptr);
	if (m_ptr)
	{
		delete m_ptr;
	}
	m_ptr = ptr;
	return m_ptr;
}
//----------------------------------------------------------------------------
template <typename T>
Void UniquePtr<T>::swap(ThisType& rhs)
{
	T* tmp = m_ptr;
	m_ptr = rhs.m_ptr;
	rhs.m_ptr = tmp;
}

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_UNIQUE_PTR_H
