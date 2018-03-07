/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_COM_PTR_H
#define NV_CO_COM_PTR_H

#include "NvCoComTypes.h"

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/*! \brief ComPtr is a simple smart pointer that manages types which implement COM based interfaces. 
\details A class that implements a COM, must derive from the IUnknown interface or a type that matches
it's layout exactly (such as IForwardUnknown). Trying to use this template with a class that doesn't follow
these rules, will lead to undefined behavior. 
This is a 'strong' pointer type, and will AddRef when a non null pointer is set and Release when the pointer 
leaves scope. 
Using 'detach' allows a pointer to be removed from the management of the ComPtr.
To set the smart pointer to null, there is the method setNull, or alternatively just assign NV_NULL.

One edge case using the template is that sometimes you want access as a pointer to a pointer. Sometimes this
is to write into the smart pointer, other times to pass as an array. To handle these different behaviors 
there are the methods readRef and writeRef, which are used instead of the & (ref) operator. For example

\code

Void doSomething(ID3D12Resource** resources, IndexT numResources);

// ...
ComPtr<ID3D12Resource> resources[3];

doSomething(resources[0].readRef(), NV_COUNT_OF(resource));
\endcode

A more common scenario writing to the pointer 

\code
IUnknown* unk = ...;

ComPtr<ID3D12Resource> resource;
Result res = unk->QueryInterface(resource.writeRef());
\endcode

*/
template <class T>
class ComPtr
{
public:
	typedef T Type;
	typedef ComPtr ThisType;
	typedef IForwardUnknown* Ptr;

		/// Constructors
		/// Default Ctor. Sets to NV_NULL
	NV_FORCE_INLINE ComPtr() :m_ptr(NV_NULL) {}
		/// Sets, and ref counts.
	NV_FORCE_INLINE explicit ComPtr(T* ptr) :m_ptr(ptr) { if (ptr) ((Ptr)ptr)->AddRef(); }
		/// The copy ctor
	NV_FORCE_INLINE ComPtr(const ThisType& rhs) : m_ptr(rhs.m_ptr) { if (m_ptr) ((Ptr)m_ptr)->AddRef(); }

#ifdef NV_HAS_MOVE_SEMANTICS
		/// Move Ctor
	NV_FORCE_INLINE ComPtr(ThisType&& rhs) : m_ptr(rhs.m_ptr) { rhs.m_ptr = NV_NULL; }
		/// Move assign
	NV_FORCE_INLINE ComPtr& operator=(ThisType&& rhs) { T* swap = m_ptr; m_ptr = rhs.m_ptr; rhs.m_ptr = swap; return *this; }
#endif

	/// Destructor releases the pointer, assuming it is set
	NV_FORCE_INLINE ~ComPtr() { if (m_ptr) ((Ptr)m_ptr)->Release(); }

	// !!! Operators !!!

	  /// Returns the dumb pointer
	NV_FORCE_INLINE operator T *() const { return m_ptr; }

	NV_FORCE_INLINE T& operator*() { return *m_ptr; }
		/// For making method invocations through the smart pointer work through the dumb pointer
	NV_FORCE_INLINE T* operator->() const { return m_ptr; }

		/// Assign 
	NV_FORCE_INLINE const ThisType &operator=(const ThisType& rhs);
		/// Assign from dumb ptr
	NV_FORCE_INLINE T* operator=(T* in);

		/// Get the pointer and don't ref
	NV_FORCE_INLINE T* get() const { return m_ptr; }
		/// Release a contained NV_NULL pointer if set
	NV_FORCE_INLINE void setNull();

		/// Detach
	NV_FORCE_INLINE T* detach() { T* ptr = m_ptr; m_ptr = NV_NULL; return ptr; }
		/// Set to a pointer without changing the ref count
	NV_FORCE_INLINE void attach(T* in) { m_ptr = in; }

		/// Get ready for writing (nulls contents)
	NV_FORCE_INLINE T** writeRef() { setNull(); return &m_ptr; }
		/// Get for read access
	NV_FORCE_INLINE T*const* readRef() const { return &m_ptr; }

		/// Swap
	Void swap(ThisType& rhs);

protected:
	/// Gets the address of the dumb pointer.
	NV_FORCE_INLINE T** operator&();

	T* m_ptr;
};

//----------------------------------------------------------------------------
template <typename T>
Void ComPtr<T>::setNull()
{
	if (m_ptr)
	{
		((Ptr)m_ptr)->Release();
		m_ptr = NV_NULL;
	}
}
//----------------------------------------------------------------------------
template <typename T>
T** ComPtr<T>::operator&()
{
	NV_CORE_ASSERT(m_ptr == NV_NULL);
	return &m_ptr;
}
//----------------------------------------------------------------------------
template <typename T>
const ComPtr<T>& ComPtr<T>::operator=(const ThisType& rhs)
{
	if (rhs.m_ptr) ((Ptr)rhs.m_ptr)->AddRef();
	if (m_ptr) ((Ptr)m_ptr)->Release();
	m_ptr = rhs.m_ptr;
	return *this;
}
//----------------------------------------------------------------------------
template <typename T>
T* ComPtr<T>::operator=(T* ptr)
{
	if (ptr) ((Ptr)ptr)->AddRef();
	if (m_ptr) ((Ptr)m_ptr)->Release();
	m_ptr = ptr;           
	return m_ptr;
}
//----------------------------------------------------------------------------
template <typename T>
Void ComPtr<T>::swap(ThisType& rhs)
{
	T* tmp = m_ptr;
	m_ptr = rhs.m_ptr;
	rhs.m_ptr = tmp;
}

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_COM_PTR_H
