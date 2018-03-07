/*
    Copyright 2005-2012 Intel Corporation.  All Rights Reserved.

    The source code contained or described herein and all documents related
    to the source code ("Material") are owned by Intel Corporation or its
    suppliers or licensors.  Title to the Material remains with Intel
    Corporation or its suppliers and licensors.  The Material is protected
    by worldwide copyright laws and treaty provisions.  No part of the
    Material may be used, copied, reproduced, modified, published, uploaded,
    posted, transmitted, distributed, or disclosed in any way without
    Intel's prior express written permission.

    No license under any patent, copyright, trade secret or other
    intellectual property right is granted to or conferred upon you by
    disclosure or delivery of the Materials, either expressly, by
    implication, inducement, estoppel or otherwise.  Any license under such
    intellectual property rights must be express and approved by Intel in
    writing.
*/

#ifndef _itt_shared_malloc_TypeDefinitions_H_
#define _itt_shared_malloc_TypeDefinitions_H_

// Define preprocessor symbols used to determine architecture
#if _WIN32||_WIN64
#   if defined(_M_X64)||defined(__x86_64__)  // the latter for MinGW support
#       define __ARCH_x86_64 1
#   elif defined(_M_IA64)
#       define __ARCH_ipf 1
#   elif defined(_M_IX86)||defined(__i386__) // the latter for MinGW support
#       define __ARCH_x86_32 1
#   else
#       error Unknown processor architecture for Windows
#   endif
#   define USE_WINTHREAD 1
#else /* Assume generic Unix */
#   if __x86_64__
#       define __ARCH_x86_64 1
#   elif __ia64__
#       define __ARCH_ipf 1
#   elif __i386__ || __i386
#       define __ARCH_x86_32 1
#   else
#       define __ARCH_other 1
#   endif
#   define USE_PTHREAD 1
#endif

// According to C99 standard INTPTR_MIN defined for C++
// iff __STDC_LIMIT_MACROS pre-defined
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

// Include files containing declarations of intptr_t and uintptr_t
#include <stddef.h>  // size_t
#if _MSC_VER
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

namespace rml {
namespace internal {

extern bool  original_malloc_found;
extern void* (*original_malloc_ptr)(size_t);
extern void  (*original_free_ptr)(void*);

} } // namespaces

//! PROVIDE YOUR OWN Customize.h IF YOU FEEL NECESSARY
#include "Customize.h"

/*
 * Functions to align an integer down or up to the given power of two,
 * and test for such an alignment, and for power of two.
 */
template<typename T>
static inline T alignDown(T arg, uintptr_t alignment) {
    return T( (uintptr_t)arg                & ~(alignment-1));
}
template<typename T>
static inline T alignUp  (T arg, uintptr_t alignment) {
    return T(((uintptr_t)arg+(alignment-1)) & ~(alignment-1));
    // /*is this better?*/ return (((uintptr_t)arg-1) | (alignment-1)) + 1;
}
template<typename T> // works for not power-of-2 alignments
static inline T alignUpGeneric(T arg, uintptr_t alignment) {
    if (size_t rem = arg % alignment) {
        arg += alignment - rem;
    }
    return arg;
}
template<typename T>
static inline bool isAligned(T arg, uintptr_t alignment) {
    return 0==((uintptr_t)arg & (alignment-1));
}
static inline bool isPowerOfTwo(uintptr_t arg) {
    return arg && (0==(arg & (arg-1)));
}
static inline bool isPowerOfTwoMultiple(uintptr_t arg, uintptr_t divisor) {
    // Divisor is assumed to be a power of two (which is valid for current uses).
    MALLOC_ASSERT( isPowerOfTwo(divisor), "Divisor should be a power of two" );
    return arg && (0==(arg & (arg-divisor)));
}

#endif /* _itt_shared_malloc_TypeDefinitions_H_ */
