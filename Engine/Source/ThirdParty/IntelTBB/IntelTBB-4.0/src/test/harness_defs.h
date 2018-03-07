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

#ifndef __TBB_harness_defs_H
#define __TBB_harness_defs_H

#include "tbb/tbb_config.h"

#if __TBB_TEST_PIC && !__PIC__
#define __TBB_TEST_SKIP_PIC_MODE 1
#else
#define __TBB_TEST_SKIP_PIC_MODE 0
#endif

#if __TBB_TEST_GCC_BUILTINS && !__TBB_GCC_BUILTIN_ATOMICS_PRESENT
#define __TBB_TEST_SKIP_BUILTINS_MODE 1
#else
#define __TBB_TEST_SKIP_BUILTINS_MODE 0
#endif

#ifndef TBB_USE_GCC_BUILTINS
  #define TBB_USE_GCC_BUILTINS         __TBB_TEST_GCC_BUILTINS && __TBB_GCC_BUILTIN_ATOMICS_PRESENT
#endif

#if __INTEL_COMPILER
  #define __TBB_LAMBDAS_PRESENT ( _TBB_CPP0X && __INTEL_COMPILER > 1100 )
#elif __GNUC__
  #define __TBB_LAMBDAS_PRESENT ( _TBB_CPP0X && __TBB_GCC_VERSION >= 40500 )
#elif _MSC_VER
  #define __TBB_LAMBDAS_PRESENT ( _MSC_VER >= 1600 )
#endif

#if _MSC_VER
  #define __TBB_EXCEPTION_TYPE_INFO_BROKEN ( _MSC_VER < 1400 )
#else
  #define __TBB_EXCEPTION_TYPE_INFO_BROKEN 0
#endif

//! a function ptr cannot be converted to const T& template argument without explicit cast
#define __TBB_FUNC_PTR_AS_TEMPL_PARAM_BROKEN ((__linux__ || __APPLE__) && __INTEL_COMPILER && __INTEL_COMPILER < 1100) || __SUNPRO_CC
#define __TBB_UNQUALIFIED_CALL_OF_DTOR_BROKEN (__GNUC__==3 && __GNUC_MINOR__<=3)

#if __TBB_LIBSTDCPP_EXCEPTION_HEADERS_BROKEN
  #define _EXCEPTION_PTR_H /* prevents exception_ptr.h inclusion */
  #define _GLIBCXX_NESTED_EXCEPTION_H /* prevents nested_exception.h inclusion */
#endif

#endif /* __TBB_harness_defs_H */
