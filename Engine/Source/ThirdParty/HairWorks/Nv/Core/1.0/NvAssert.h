/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CORE_ASSERT_H
#define NV_CORE_ASSERT_H

#ifndef NV_ENABLE_ASSERTS
#	define NV_ENABLE_ASSERTS (NV_DEBUG && !defined(__CUDACC__)) 
#endif

#if NV_ENABLE_ASSERTS
#	include <assert.h>
#endif

/** \addtogroup core
@{
*/

/*! define NV_BREAK_ON_ASSERT to make an assert failure call NV_BREAKPOINT. This can on some systems be 
more effective at breaking into a debugger than the system assert mechanism */
#if defined(NV_BREAK_ON_ASSERT) && !defined(NV_CORE_ASSERT)
#	define NV_CORE_ASSERT(x) { if (!(x)) NV_BREAKPOINT(0); }
#	define NV_CORE_ALWAYS_ASSERT(x) NV_CORE_ASSERT(0)
#endif

/* Assert macros */
#ifndef NV_CORE_ASSERT
#	if NV_ENABLE_ASSERTS
#		define NV_CORE_ASSERT(exp) (assert(exp))
#		define NV_CORE_ALWAYS_ASSERT(x) NV_CORE_ASSERT(0)
#	else
#		define NV_CORE_ASSERT(exp) 
#		define NV_CORE_ALWAYS_ASSERT(x) 
#	endif
#endif

/** @} */
#endif // #ifndef NV_CORE_ASSERT_H
