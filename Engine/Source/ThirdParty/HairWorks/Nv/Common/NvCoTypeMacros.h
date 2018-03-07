/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_TYPE_MACROS_H
#define NV_CO_TYPE_MACROS_H

#include "NvCoAllocateMacros.h"

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

//! Macros used for type definitions

/// NOTE! That types allocated with this definition, can only be deleted from their most derived 
/// type. That is generally going to be the case, otherwise the dtor will be incorrect.
#define NV_CO_DECLARE_CLASS_BASE(type) \
	public: \
	typedef type ThisType; \
	NV_CO_CLASS_KNOWN_SIZE_ALLOC

#define NV_CO_DECLARE_CLASS(type, parent) \
	public: \
	typedef parent Parent; \
	typedef type ThisType; \
	NV_CO_CLASS_KNOWN_SIZE_ALLOC

// 
#define NV_CO_DECLARE_POLYMORPHIC_CLASS_BASE(type) \
	public: \
	typedef type ThisType; \
	NV_CO_CLASS_UNKNOWN_SIZE_ALLOC

#define NV_CO_DECLARE_POLYMORPHIC_CLASS(type, parent) \
	public: \
	typedef parent Parent; \
	typedef type ThisType; \
	NV_CO_CLASS_UNKNOWN_SIZE_ALLOC


// Type is an interface - it is abstract and can only be created as part of a derived type 
#define NV_CO_DECLARE_INTERFACE_BASE(type) \
	public: \
	typedef type ThisType; \
	NV_CO_CLASS_NO_ALLOC

#define NV_CO_DECLARE_INTERFACE(type, parent) \
	public: \
	typedef parent Parent; \
	typedef type ThisType; \
	NV_CO_CLASS_NO_ALLOC

// The type is polymorphic but cannot be new'd, deleted but can only be created on the stack
#define NV_CO_DECLARE_STACK_POLYMORPHIC_CLASS_BASE(type) \
	NV_CO_DECLARE_INTERFACE_BASE(type)

#define NV_CO_DECLARE_STACK_POLYMORPHIC_CLASS(type, parent) \
	NV_CO_DECLARE_INTERFACE(type, parent)

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_TYPE_MACROS_H