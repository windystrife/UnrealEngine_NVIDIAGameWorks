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

/*
Replacing the standard memory allocation routines in Microsoft* C/C++ RTL 
(malloc/free, global new/delete, etc.) with the TBB memory allocator. 

Include the following header to a source of any binary which is loaded during 
application startup

#include "tbb/tbbmalloc_proxy.h"

or add following parameters to the linker options for the binary which is 
loaded during application startup. It can be either exe-file or dll.

For win32
tbbmalloc_proxy.lib /INCLUDE:"___TBB_malloc_proxy"
win64
tbbmalloc_proxy.lib /INCLUDE:"__TBB_malloc_proxy"
*/

#ifndef __TBB_tbbmalloc_proxy_H
#define __TBB_tbbmalloc_proxy_H

#if _MSC_VER

#ifdef _DEBUG
    #pragma comment(lib, "tbbmalloc_proxy_debug.lib")
#else
    #pragma comment(lib, "tbbmalloc_proxy.lib")
#endif

#if defined(_WIN64)
    #pragma comment(linker, "/include:__TBB_malloc_proxy")
#else
    #pragma comment(linker, "/include:___TBB_malloc_proxy")
#endif

#else
/* Primarily to support MinGW */

extern "C" void __TBB_malloc_proxy();
struct __TBB_malloc_proxy_caller {
    __TBB_malloc_proxy_caller() { __TBB_malloc_proxy(); }
} volatile __TBB_malloc_proxy_helper_object;

#endif // _MSC_VER

#endif //__TBB_tbbmalloc_proxy_H
