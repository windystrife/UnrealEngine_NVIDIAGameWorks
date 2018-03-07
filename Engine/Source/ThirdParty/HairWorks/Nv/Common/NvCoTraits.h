/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_TRAITS_H
#define NV_CO_TRAITS_H

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

// Predeclare the ComPtr type
template <class T>
class ComPtr;
template <class T>
class WeakComPtr;

namespace Traits {

// Helper to strip const from a type
template <typename T>
struct StripConst
{
	typedef T Value;
};
template <typename T>
struct StripConst<const T>
{
	typedef T Value;
};

/* Find the underlying type of a pointer taking into account smart pointers*/
namespace GetPointerTypeImpl
{
template <typename T>
struct Strip;

template <typename T>
struct Strip<T*>
{
	typedef T Value;
};
template <typename T>
struct Strip<const T*>
{
	typedef T Value;
};
template <typename T>
struct Strip<ComPtr<T> >
{
	typedef typename Strip<T*>::Value Value;
};
template <typename T>
struct Strip<WeakComPtr<T> >
{
	typedef typename Strip<T*>::Value Value;
};
} // namespace PointerTypeImpl

// To get the underlying thing a pointer points to 
template <typename T>
struct GetPointerType
{
	typedef typename GetPointerTypeImpl::Strip<T>::Value Value;
};

template <typename T>
struct GetPointerType<const T>
{
	typedef typename GetPointerTypeImpl::Strip<T>::Value Value;
};

} // namespace Traits

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_TRAITS_H
