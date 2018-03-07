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

#ifndef __TBB_tbb_windef_H
#error Do not #include this internal file directly; use public TBB headers instead.
#endif /* __TBB_tbb_windef_H */

// Check that the target Windows version has all API calls requried for TBB.
// Do not increase the version in condition beyond 0x0500 without prior discussion!
#if defined(_WIN32_WINNT) && _WIN32_WINNT<0x0400
#error TBB is unable to run on old Windows versions; _WIN32_WINNT must be 0x0400 or greater.
#endif

#if !defined(_MT)
#error TBB requires linkage with multithreaded C/C++ runtime library. \
       Choose multithreaded DLL runtime in project settings, or use /MD[d] compiler switch.
#endif

// Workaround for the problem with MVSC headers failing to define namespace std
namespace std {
  using ::size_t; using ::ptrdiff_t;
}

#define __TBB_STRING_AUX(x) #x
#define __TBB_STRING(x) __TBB_STRING_AUX(x)

// Default setting of TBB_USE_DEBUG
#ifdef TBB_USE_DEBUG
#    if TBB_USE_DEBUG 
#        if !defined(_DEBUG)
#            pragma message(__FILE__ "(" __TBB_STRING(__LINE__) ") : Warning: Recommend using /MDd if compiling with TBB_USE_DEBUG!=0")
#        endif
#    else
#        if defined(_DEBUG)
#            pragma message(__FILE__ "(" __TBB_STRING(__LINE__) ") : Warning: Recommend using /MD if compiling with TBB_USE_DEBUG==0")
#        endif
#    endif
#endif

#if (__TBB_BUILD || __TBBMALLOC_BUILD) && !defined(__TBB_NO_IMPLICIT_LINKAGE)
#define __TBB_NO_IMPLICIT_LINKAGE 1
#endif

#if _MSC_VER
    #if !__TBB_NO_IMPLICIT_LINKAGE
        #ifdef __TBB_LIB_NAME
	        #pragma comment(lib, __TBB_STRING(__TBB_LIB_NAME))
        #else
			#ifdef _DEBUG
				#pragma comment(lib, "tbb_debug.lib")
			#else
				#pragma comment(lib, "tbb.lib")
			#endif
        #endif
    #endif
#endif
