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

#ifndef __TBB_dynamic_link
#define __TBB_dynamic_link

// Support for dynamic loading entry points from other shared libraries.

#ifndef LIBRARY_ASSERT
    #include "tbb/tbb_config.h"
#endif /* !LIBRARY_ASSERT */

/** By default, symbols declared and defined here go into namespace tbb::internal.
    To put them in other namespace, define macros OPEN_INTERNAL_NAMESPACE
    and CLOSE_INTERNAL_NAMESPACE to override the following default definitions. **/
#ifndef OPEN_INTERNAL_NAMESPACE
#define OPEN_INTERNAL_NAMESPACE namespace tbb { namespace internal {
#define CLOSE_INTERNAL_NAMESPACE }}
#endif /* OPEN_INTERNAL_NAMESPACE */

#include <stddef.h>
#if _WIN32||_WIN64
#include "tbb/machine/windows_api.h"
#endif /* _WIN32||_WIN64 */

OPEN_INTERNAL_NAMESPACE

//! Type definition for a pointer to a void somefunc(void)
typedef void (*pointer_to_handler)();

// Double cast through the void* from func_ptr in DLD macro is necessary to
// prevent warnings from some compilers (g++ 4.1)
#if __TBB_WEAK_SYMBOLS

#define DLD(s,h) {(pointer_to_handler)&s, (pointer_to_handler*)(void*)(&h)}
//! Association between a handler name and location of pointer to it.
struct dynamic_link_descriptor {
    //! pointer to the handler
    pointer_to_handler ptr;
    //! Pointer to the handler
    pointer_to_handler* handler;
};

#else /* !__TBB_WEAK_SYMBOLS */

#define DLD(s,h) {#s, (pointer_to_handler*)(void*)(&h)}
//! Association between a handler name and location of pointer to it.
struct dynamic_link_descriptor {
    //! Name of the handler
    const char* name;
    //! Pointer to the handler
    pointer_to_handler* handler;
};

#endif /* !__TBB_WEAK_SYMBOLS */

#if _WIN32||_WIN64
typedef HMODULE dynamic_link_handle;
#else
typedef void* dynamic_link_handle;
#endif /* _WIN32||_WIN64 */

//! Fill in dynamically linked handlers.
/** 'n' is the length of the array descriptors[].
    'required' is the number of the initial entries in the array descriptors[]
    that have to be found in order for the call to succeed. If the library and
    all the required handlers are found, then the corresponding handler pointers
    are set, and the return value is true.  Otherwise the original array of
    descriptors is left untouched and the return value is false. **/
bool dynamic_link( const char* libraryname,
                   const dynamic_link_descriptor descriptors[],
                   size_t n,
                   size_t required = ~(size_t)0,
                   dynamic_link_handle* handle = 0 );

bool dynamic_link( dynamic_link_handle module,
                   const dynamic_link_descriptor descriptors[],
                   size_t n,
                   size_t required  = ~(size_t)0 );

void dynamic_unlink( dynamic_link_handle handle );

#if __TBB_BUILD
void dynamic_unlink_all();
#endif /* __TBB_BUILD */

enum dynamic_link_error_t {
    dl_success = 0,
    dl_lib_not_found,     // char const * lib, dlerr_t err
    dl_sym_not_found,     // char const * sym, dlerr_t err
                          // Note: dlerr_t depends on OS: it is char const * on Linux and Mac, int on Windows.
    dl_sys_fail,          // char const * func, int err
    dl_buff_too_small     // none
}; // dynamic_link_error_t

CLOSE_INTERNAL_NAMESPACE

#endif /* __TBB_dynamic_link */
