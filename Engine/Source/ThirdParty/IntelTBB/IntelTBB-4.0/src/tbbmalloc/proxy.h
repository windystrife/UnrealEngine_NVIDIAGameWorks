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

#ifndef _TBB_malloc_proxy_H_
#define _TBB_malloc_proxy_H_

#if __linux__
#define MALLOC_LD_PRELOAD 1
#endif

// MALLOC_LD_PRELOAD depends on MALLOC_CHECK_RECURSION stuff
#if __linux__ || __APPLE__ || __sun || __FreeBSD__ || MALLOC_LD_PRELOAD
#define MALLOC_CHECK_RECURSION 1
#endif

#include <stddef.h>

extern "C" {
    void * scalable_malloc(size_t size);
    void * scalable_calloc(size_t nobj, size_t size);
    void   scalable_free(void *ptr);
    void * scalable_realloc(void* ptr, size_t size);
    void * scalable_aligned_malloc(size_t size, size_t alignment);
    void * scalable_aligned_realloc(void* ptr, size_t size, size_t alignment);
    int    scalable_posix_memalign(void **memptr, size_t alignment, size_t size);
    size_t scalable_msize(void *ptr);
    void   safer_scalable_free( void *ptr, void (*original_free)(void*));
    void * safer_scalable_realloc( void *ptr, size_t, void* );
    void * safer_scalable_aligned_realloc( void *ptr, size_t, size_t, void* );
    size_t safer_scalable_msize( void *ptr, size_t (*orig_msize_crt80d)(void*));

    void * __TBB_internal_malloc(size_t size);
    void * __TBB_internal_calloc(size_t num, size_t size);
    void   __TBB_internal_free(void *ptr);
    void * __TBB_internal_realloc(void* ptr, size_t sz);
    int    __TBB_internal_posix_memalign(void **memptr, size_t alignment, size_t size);
    
    bool   __TBB_internal_find_original_malloc(int num, const char *names[], void *table[]);
} // extern "C"

// Struct with original free() and _msize() pointers
struct orig_ptrs {
    void   (*orig_free) (void*);  
    size_t (*orig_msize)(void*); 
};

#endif /* _TBB_malloc_proxy_H_ */
