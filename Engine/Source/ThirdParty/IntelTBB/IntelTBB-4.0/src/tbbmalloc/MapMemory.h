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

#ifndef _itt_shared_malloc_MapMemory_H
#define _itt_shared_malloc_MapMemory_H

#include <stdlib.h>

void *ErrnoPreservingMalloc(size_t bytes)
{
    int prevErrno = errno;
    void *ret = malloc( bytes );
    if (!ret)
        errno = prevErrno;
    return ret;
}

#if __linux__ || __APPLE__ || __sun || __FreeBSD__

#if __sun && !defined(_XPG4_2)
 // To have void* as mmap's 1st argument
 #define _XPG4_2 1
 #define XPG4_WAS_DEFINED 1
#endif

#include <sys/mman.h>
#if __linux__
/* __TBB_MAP_HUGETLB is MAP_HUGETLB from system header linux/mman.h.
   The header do not included here, as on some Linux flavors inclusion of
   linux/mman.h leads to compilation error,
   while changing of MAP_HUGETLB is highly unexpected.
*/
#define __TBB_MAP_HUGETLB 0x40000
#else
#define __TBB_MAP_HUGETLB 0
#endif

#if XPG4_WAS_DEFINED
 #undef _XPG4_2
 #undef XPG4_WAS_DEFINED
#endif

#define MEMORY_MAPPING_USES_MALLOC 0
void* MapMemory (size_t bytes, bool hugePages)
{
    void* result = 0;
    int prevErrno = errno;
#ifndef MAP_ANONYMOUS
// Mac OS* X defines MAP_ANON, which is deprecated in Linux.
#define MAP_ANONYMOUS MAP_ANON
#endif /* MAP_ANONYMOUS */
    int addFlags = hugePages? __TBB_MAP_HUGETLB : 0;
    result = mmap(NULL, bytes, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|addFlags, -1, 0);
    if (result==MAP_FAILED)
        errno = prevErrno;
    return result==MAP_FAILED? 0: result;
}

int UnmapMemory(void *area, size_t bytes)
{
    int prevErrno = errno;
    int ret = munmap(area, bytes);
    if (-1 == ret)
        errno = prevErrno;
    return ret;
}

#elif (_WIN32 || _WIN64) && !_XBOX
#include <windows.h>

#define MEMORY_MAPPING_USES_MALLOC 0
void* MapMemory (size_t bytes, bool)
{
    /* Is VirtualAlloc thread safe? */
    return VirtualAlloc(NULL, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

int UnmapMemory(void *area, size_t bytes)
{
    BOOL result = VirtualFree(area, 0, MEM_RELEASE);
    return !result;
}

#else

#define MEMORY_MAPPING_USES_MALLOC 1
void* MapMemory (size_t bytes, bool)
{
    return ErrnoPreservingMalloc( bytes );
}

int UnmapMemory(void *area, size_t bytes)
{
    free( area );
    return 0;
}

#endif /* OS dependent */

#if MALLOC_CHECK_RECURSION && MEMORY_MAPPING_USES_MALLOC
#error Impossible to protect against malloc recursion when memory mapping uses malloc.
#endif

#endif /* _itt_shared_malloc_MapMemory_H */
