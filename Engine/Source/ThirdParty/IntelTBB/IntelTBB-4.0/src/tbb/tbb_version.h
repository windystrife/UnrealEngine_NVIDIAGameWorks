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

// Please define version number in the file:
#include "tbb/tbb_stddef.h"

// And don't touch anything below
#ifndef ENDL
#define ENDL "\n"
#endif
#include "version_string.ver"

#ifndef __TBB_VERSION_STRINGS
#pragma message("Warning: version_string.ver isn't generated properly by version_info.sh script!")
// here is an example of macros value:
#define __TBB_VERSION_STRINGS \
"TBB: BUILD_HOST\tUnknown\n" \
"TBB: BUILD_ARCH\tUnknown\n" \
"TBB: BUILD_OS\t\tUnknown\n" \
"TBB: BUILD_CL\t\tUnknown\n" \
"TBB: BUILD_COMPILER\tUnknown\n" \
"TBB: BUILD_COMMAND\tUnknown\n"
#endif
#ifndef __TBB_DATETIME
#ifdef RC_INVOKED
#define __TBB_DATETIME "Unknown"
#else
#define __TBB_DATETIME __DATE__ __TIME__
#endif
#endif

#define __TBB_VERSION_NUMBER(N) #N ": VERSION\t\t" __TBB_STRING(TBB_VERSION_MAJOR.TBB_VERSION_MINOR) ENDL
#define __TBB_INTERFACE_VERSION_NUMBER(N) #N ": INTERFACE VERSION\t" __TBB_STRING(TBB_INTERFACE_VERSION) ENDL

#define __TBB_VERSION_DATETIME(N) #N ": BUILD_DATE\t\t" __TBB_DATETIME ENDL
#ifndef TBB_USE_DEBUG
    #define __TBB_VERSION_USE_DEBUG(N) #N ": TBB_USE_DEBUG\tundefined" ENDL
#elif TBB_USE_DEBUG==0
    #define __TBB_VERSION_USE_DEBUG(N) #N ": TBB_USE_DEBUG\t0" ENDL
#elif TBB_USE_DEBUG==1
    #define __TBB_VERSION_USE_DEBUG(N) #N ": TBB_USE_DEBUG\t1" ENDL
#elif TBB_USE_DEBUG==2
    #define __TBB_VERSION_USE_DEBUG(N) #N ": TBB_USE_DEBUG\t2" ENDL
#else
    #error Unexpected value for TBB_USE_DEBUG
#endif

/* Make __TBB_VERSION_USE_ASSERT and __TBB_VERSION_DO_NOTIFY empty for rc
 * because rc from VS2005 crashed with fatal error RC10056 for too complex
 * macros (for example, when __TBB_CPF_BUILD is enabled).
 * All information is available in BUILD_COMMAND anyway.
 */

#ifdef RC_INVOKED
    #define __TBB_VERSION_USE_ASSERT(N)
#else // RC_INVOKED
#ifndef TBB_USE_ASSERT
    #define __TBB_VERSION_USE_ASSERT(N) #N ": TBB_USE_ASSERT\tundefined" ENDL
#elif TBB_USE_ASSERT==0
    #define __TBB_VERSION_USE_ASSERT(N) #N ": TBB_USE_ASSERT\t0" ENDL
#elif TBB_USE_ASSERT==1
    #define __TBB_VERSION_USE_ASSERT(N) #N ": TBB_USE_ASSERT\t1" ENDL
#elif TBB_USE_ASSERT==2
    #define __TBB_VERSION_USE_ASSERT(N) #N ": TBB_USE_ASSERT\t2" ENDL
#else
    #error Unexpected value for TBB_USE_ASSERT
#endif
#endif // RC_INVOKED

#ifndef __TBB_CPF_BUILD
    #define __TBB_VERSION_TBB_PREVIEW_BINARY(N)
#else
    #define __TBB_VERSION_TBB_PREVIEW_BINARY(N) #N ": TBB_PREVIEW_BINARY\t1" ENDL
#endif

#ifdef RC_INVOKED
    #define __TBB_VERSION_DO_NOTIFY(N)
#else
#ifndef DO_ITT_NOTIFY
    #define __TBB_VERSION_DO_NOTIFY(N) #N ": DO_ITT_NOTIFY\tundefined" ENDL
#elif DO_ITT_NOTIFY==1
    #define __TBB_VERSION_DO_NOTIFY(N) #N ": DO_ITT_NOTIFY\t1" ENDL
#elif DO_ITT_NOTIFY==0
    #define __TBB_VERSION_DO_NOTIFY(N)
#else
    #error Unexpected value for DO_ITT_NOTIFY
#endif
#endif // RC_INVOKED

#define TBB_VERSION_STRINGS_P(N) __TBB_VERSION_NUMBER(N) __TBB_INTERFACE_VERSION_NUMBER(N) __TBB_VERSION_DATETIME(N) __TBB_VERSION_STRINGS(N) __TBB_VERSION_USE_DEBUG(N) __TBB_VERSION_USE_ASSERT(N) __TBB_VERSION_TBB_PREVIEW_BINARY(N) __TBB_VERSION_DO_NOTIFY(N)

#define TBB_VERSION_STRINGS TBB_VERSION_STRINGS_P(TBB)
#define TBBMALLOC_VERSION_STRINGS TBB_VERSION_STRINGS_P(TBBmalloc)

// numbers
#ifndef __TBB_VERSION_YMD
#define __TBB_VERSION_YMD 0, 0
#endif

#define TBB_VERNUMBERS TBB_VERSION_MAJOR, TBB_VERSION_MINOR, __TBB_VERSION_YMD

#define TBB_VERSION __TBB_STRING(TBB_VERNUMBERS)
