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

#ifndef __TBB_tbb_config_H
#define __TBB_tbb_config_H

/** This header is supposed to contain macro definitions and C style comments only.
    The macros defined here are intended to control such aspects of TBB build as 
    - presence of compiler features
    - compilation modes
    - feature sets
    - known compiler/platform issues
**/

#define __TBB_GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if __clang__
    #define __TBB_CLANG_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#endif

/** Presence of compiler features **/

#if __INTEL_COMPILER == 9999 && __INTEL_COMPILER_BUILD_DATE == 20110811
/* Intel Composer XE 2011 Update 6 incorrectly sets __INTEL_COMPILER. Fix it. */
    #undef __INTEL_COMPILER
    #define __INTEL_COMPILER 1210
#endif

#if (__TBB_GCC_VERSION >= 40400) && !defined(__INTEL_COMPILER)
    /** warning suppression pragmas available in GCC since 4.4 **/
    #define __TBB_GCC_WARNING_SUPPRESSION_PRESENT 1
#endif

/* Select particular features of C++11 based on compiler version.
   ICC 12.1 (Linux), GCC 4.3 and higher, clang 2.9 and higher
   set __GXX_EXPERIMENTAL_CXX0X__ in c++11 mode.

   Compilers that mimics other compilers (ICC, clang) must be processed before
   compilers they mimic (GCC, MSVC).

   TODO: The following conditions should be extended when new compilers/runtimes
   support added.
 */

#if __INTEL_COMPILER
    /** On Windows environment when using Intel C++ compiler with Visual Studio 2010*,
        the C++0x features supported by Visual C++ 2010 are enabled by default
        TODO: find a way to get know if c++0x mode is specified in command line on windows **/
    #define __TBB_CPP11_VARIADIC_TEMPLATES_PRESENT __GXX_EXPERIMENTAL_CXX0X__ && __VARIADIC_TEMPLATES
    #define __TBB_CPP11_RVALUE_REF_PRESENT         (__GXX_EXPERIMENTAL_CXX0X__ || _MSC_VER >= 1600) && (__INTEL_COMPILER >= 1200)
    #if  _MSC_VER >= 1600
        #define __TBB_EXCEPTION_PTR_PRESENT        __INTEL_COMPILER > 1300                                                  \
                                                   /*ICC 12.1 Upd 10 and 13 beta Upd 2 fixed exception_ptr linking  issue*/ \
                                                   || (__INTEL_COMPILER == 1300 && __INTEL_COMPILER_BUILD_DATE >= 20120530) \
                                                   || (__INTEL_COMPILER == 1210 && __INTEL_COMPILER_BUILD_DATE >= 20120410)
    /** libstc++ that comes with GCC 4.6 use C++ features not yet supported by current ICC (12.1)**/
    #elif (__TBB_GCC_VERSION >= 40404) && (__TBB_GCC_VERSION < 40600)
        #define __TBB_EXCEPTION_PTR_PRESENT        __GXX_EXPERIMENTAL_CXX0X__ && __INTEL_COMPILER >= 1200
    #else
        #define __TBB_EXCEPTION_PTR_PRESENT        0
    #endif
    #define __TBB_MAKE_EXCEPTION_PTR_PRESENT       (_MSC_VER >= 1700 || (__GXX_EXPERIMENTAL_CXX0X__ && __TBB_GCC_VERSION >= 40600))
    #define __TBB_CPP11_TUPLE_PRESENT              (_MSC_VER >= 1600) || ((__GXX_EXPERIMENTAL_CXX0X__) && (__TBB_GCC_VERSION >= 40300))
#elif __clang__
//TODO: these options need to be rechecked
    #define __TBB_CPP11_VARIADIC_TEMPLATES_PRESENT (__GXX_EXPERIMENTAL_CXX0X__ && __TBB_CLANG_VERSION >= 20900)
    #define __TBB_CPP11_RVALUE_REF_PRESENT         (__GXX_EXPERIMENTAL_CXX0X__ && __TBB_CLANG_VERSION >= 20900)
    #define __TBB_EXCEPTION_PTR_PRESENT            __GXX_EXPERIMENTAL_CXX0X__
    #define __TBB_MAKE_EXCEPTION_PTR_PRESENT       (__GXX_EXPERIMENTAL_CXX0X__ && __TBB_CLANG_VERSION > 30100)// TODO: check version
    #define __TBB_CPP11_TUPLE_PRESENT              ((__GXX_EXPERIMENTAL_CXX0X__) && (__TBB_GCC_VERSION >= 40300))
#elif __GNUC__
    #define __TBB_CPP11_VARIADIC_TEMPLATES_PRESENT __GXX_EXPERIMENTAL_CXX0X__
    #define __TBB_CPP11_RVALUE_REF_PRESENT         __GXX_EXPERIMENTAL_CXX0X__
    /** __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4 here is a substitution for _GLIBCXX_ATOMIC_BUILTINS_4, which is a prerequisite 
        for exception_ptr but cannot be used in this file because it is defined in a header, not by the compiler. 
        If the compiler has no atomic intrinsics, the C++ library should not expect those as well. **/
    #define __TBB_EXCEPTION_PTR_PRESENT            (__GXX_EXPERIMENTAL_CXX0X__ && (__TBB_GCC_VERSION >= 40404) && __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4)
    #define __TBB_MAKE_EXCEPTION_PTR_PRESENT       (__GXX_EXPERIMENTAL_CXX0X__ && __TBB_GCC_VERSION >= 40600)
    #define __TBB_CPP11_TUPLE_PRESENT              ((__GXX_EXPERIMENTAL_CXX0X__) && (__TBB_GCC_VERSION >= 40300))
#elif _MSC_VER
    #define __TBB_CPP11_VARIADIC_TEMPLATES_PRESENT 0
    #define __TBB_CPP11_RVALUE_REF_PRESENT         0
    #define __TBB_EXCEPTION_PTR_PRESENT            (_MSC_VER >= 1600)
    #define __TBB_MAKE_EXCEPTION_PTR_PRESENT       (_MSC_VER >= 1700)
    #define __TBB_CPP11_TUPLE_PRESENT              (_MSC_VER >= 1600)
#else
    #define __TBB_CPP11_VARIADIC_TEMPLATES_PRESENT 0
    #define __TBB_CPP11_RVALUE_REF_PRESENT         0
    #define __TBB_EXCEPTION_PTR_PRESENT            0
    #define __TBB_MAKE_EXCEPTION_PTR_PRESENT       0    
    #define __TBB_CPP11_TUPLE_PRESENT              0
#endif

//TODO: not clear how exactly this macro affects exception_ptr - investigate
// On linux ICC fails to find existing std::exception_ptr in libstdc++ without this define
#if __INTEL_COMPILER && __GNUC__ && __TBB_EXCEPTION_PTR_PRESENT && !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4)
    #define __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4 1
#endif

// Work around a bug in MinGW32
#if __MINGW32__ && __TBB_EXCEPTION_PTR_PRESENT && !defined(_GLIBCXX_ATOMIC_BUILTINS_4)
    #define _GLIBCXX_ATOMIC_BUILTINS_4
#endif

#if __GNUC__ || __SUNPRO_CC || __IBMCPP__
    /* ICC defines __GNUC__ and so is covered */
    #define __TBB_ATTRIBUTE_ALIGNED_PRESENT 1
#elif _MSC_VER && (_MSC_VER >= 1300 || __INTEL_COMPILER)
    #define __TBB_DECLSPEC_ALIGN_PRESENT 1
#endif

/* TODO: change the version back to 4.1.2 once macro __TBB_WORD_SIZE become optional */
#if (__TBB_GCC_VERSION >= 40306) && !defined(__INTEL_COMPILER)
    /** built-in atomics available in GCC since 4.1.2 **/
    #define __TBB_GCC_BUILTIN_ATOMICS_PRESENT 1
#endif

/** User controlled TBB features & modes **/

#ifndef TBB_USE_DEBUG
#ifdef TBB_DO_ASSERT
#define TBB_USE_DEBUG TBB_DO_ASSERT
#else
#ifdef _DEBUG
#define TBB_USE_DEBUG _DEBUG
#else
#define TBB_USE_DEBUG 0
#endif
#endif /* TBB_DO_ASSERT */
#endif /* TBB_USE_DEBUG */

#ifndef TBB_USE_ASSERT
#ifdef TBB_DO_ASSERT
#define TBB_USE_ASSERT TBB_DO_ASSERT
#else 
#define TBB_USE_ASSERT TBB_USE_DEBUG
#endif /* TBB_DO_ASSERT */
#endif /* TBB_USE_ASSERT */

#ifndef TBB_USE_THREADING_TOOLS
#ifdef TBB_DO_THREADING_TOOLS
#define TBB_USE_THREADING_TOOLS TBB_DO_THREADING_TOOLS
#else 
#define TBB_USE_THREADING_TOOLS TBB_USE_DEBUG
#endif /* TBB_DO_THREADING_TOOLS */
#endif /* TBB_USE_THREADING_TOOLS */

#ifndef TBB_USE_PERFORMANCE_WARNINGS
#ifdef TBB_PERFORMANCE_WARNINGS
#define TBB_USE_PERFORMANCE_WARNINGS TBB_PERFORMANCE_WARNINGS
#else 
#define TBB_USE_PERFORMANCE_WARNINGS TBB_USE_DEBUG
#endif /* TBB_PEFORMANCE_WARNINGS */
#endif /* TBB_USE_PERFORMANCE_WARNINGS */

#if __MIC__ || __MIC2__
#define __TBB_DEFINE_MIC 1
#endif

#if !defined(__EXCEPTIONS) && !defined(_CPPUNWIND) && !defined(__SUNPRO_CC) || defined(_XBOX)
    #if TBB_USE_EXCEPTIONS
        #error Compilation settings do not support exception handling. Please do not set TBB_USE_EXCEPTIONS macro or set it to 0.
    #elif !defined(TBB_USE_EXCEPTIONS)
        #define TBB_USE_EXCEPTIONS 0
    #endif
#elif !defined(TBB_USE_EXCEPTIONS)
    #if __TBB_DEFINE_MIC
    #define TBB_USE_EXCEPTIONS 0
    #else
    #define TBB_USE_EXCEPTIONS 1
    #endif
#elif TBB_USE_EXCEPTIONS && __TBB_DEFINE_MIC
    #error Please do not set TBB_USE_EXCEPTIONS macro or set it to 0.
#endif

#ifndef TBB_IMPLEMENT_CPP0X
    /** By default, use C++0x classes if available **/
    #if __GNUC__==4 && __GNUC_MINOR__>=4 && __GXX_EXPERIMENTAL_CXX0X__
        #define TBB_IMPLEMENT_CPP0X 0
    #else
        #define TBB_IMPLEMENT_CPP0X 1
    #endif
#endif /* TBB_IMPLEMENT_CPP0X */

/* TBB_USE_CAPTURED_EXCEPTION should be explicitly set to either 0 or 1, as it is used as C++ const */
#ifndef TBB_USE_CAPTURED_EXCEPTION
    /** linux pre-built TBB binary does not support exception_ptr. **/
    #if __TBB_EXCEPTION_PTR_PRESENT && !defined(__GNUC__)
        #define TBB_USE_CAPTURED_EXCEPTION 0
    #else
        #define TBB_USE_CAPTURED_EXCEPTION 1
    #endif
#else /* defined TBB_USE_CAPTURED_EXCEPTION */
    #if !TBB_USE_CAPTURED_EXCEPTION && !__TBB_EXCEPTION_PTR_PRESENT
        #error Current runtime does not support std::exception_ptr. Set TBB_USE_CAPTURED_EXCEPTION and make sure that your code is ready to catch tbb::captured_exception.
    #endif
#endif /* defined TBB_USE_CAPTURED_EXCEPTION */

/** Check whether the request to use GCC atomics can be satisfied **/
#if (TBB_USE_GCC_BUILTINS && !__TBB_GCC_BUILTIN_ATOMICS_PRESENT)
    #error "GCC atomic built-ins are not supported."
#endif

/** Internal TBB features & modes **/

/** __TBB_DYNAMIC_LOAD_ENABLED describes the system possibility to dynamic load libraries
    __TBB_SOURCE_DIRECTLY_INCLUDED is a mode used in whitebox testing when 
    it's necessary to test internal functions not exported from TBB DLLs
**/

#ifndef __TBB_DYNAMIC_LOAD_ENABLED
    #define __TBB_DYNAMIC_LOAD_ENABLED 1
#elif !(_WIN32||_WIN64) && !__TBB_DYNAMIC_LOAD_ENABLED
    #define __TBB_WEAK_SYMBOLS 1
#endif

#if (_WIN32||_WIN64) && __TBB_SOURCE_DIRECTLY_INCLUDED
    #define __TBB_NO_IMPLICIT_LINKAGE 1
    #define __TBBMALLOC_NO_IMPLICIT_LINKAGE 1
#endif

#ifndef __TBB_COUNT_TASK_NODES
    #define __TBB_COUNT_TASK_NODES TBB_USE_ASSERT
#endif

#ifndef __TBB_TASK_GROUP_CONTEXT
    #define __TBB_TASK_GROUP_CONTEXT 1
#endif /* __TBB_TASK_GROUP_CONTEXT */

#ifndef __TBB_SCHEDULER_OBSERVER
    #define __TBB_SCHEDULER_OBSERVER 1
#endif /* __TBB_SCHEDULER_OBSERVER */

#if !defined(TBB_PREVIEW_TASK_ARENA) && __TBB_BUILD
    #define TBB_PREVIEW_TASK_ARENA __TBB_CPF_BUILD
#endif /* TBB_PREVIEW_TASK_ARENA */
#define __TBB_TASK_ARENA TBB_PREVIEW_TASK_ARENA
#if TBB_PREVIEW_TASK_ARENA
    #define TBB_PREVIEW_LOCAL_OBSERVER 1
    #define __TBB_NO_IMPLICIT_LINKAGE 1
    #define __TBB_TASK_PRIORITY 0 // TODO: it will be removed in next versions
    #if !__TBB_SCHEDULER_OBSERVER
        #error TBB_PREVIEW_TASK_ARENA requires __TBB_SCHEDULER_OBSERVER to be enabled
    #endif
#endif /* TBB_PREVIEW_TASK_ARENA */

#if !defined(TBB_PREVIEW_LOCAL_OBSERVER) && __TBB_BUILD && __TBB_SCHEDULER_OBSERVER
    #define TBB_PREVIEW_LOCAL_OBSERVER 1
#endif /* TBB_PREVIEW_LOCAL_OBSERVER */

#if TBB_USE_EXCEPTIONS && !__TBB_TASK_GROUP_CONTEXT
    #error TBB_USE_EXCEPTIONS requires __TBB_TASK_GROUP_CONTEXT to be enabled
#endif

#ifndef __TBB_TASK_PRIORITY
    #define __TBB_TASK_PRIORITY __TBB_TASK_GROUP_CONTEXT
#endif /* __TBB_TASK_PRIORITY */

#if __TBB_TASK_PRIORITY && !__TBB_TASK_GROUP_CONTEXT
    #error __TBB_TASK_PRIORITY requires __TBB_TASK_GROUP_CONTEXT to be enabled
#endif

#if !defined(__TBB_SURVIVE_THREAD_SWITCH) && \
          (_WIN32 || _WIN64 || __APPLE__ || __linux__)
    #define __TBB_SURVIVE_THREAD_SWITCH 1
#endif /* __TBB_SURVIVE_THREAD_SWITCH */

#ifndef __TBB_DEFAULT_PARTITIONER
#if TBB_DEPRECATED
/** Default partitioner for parallel loop templates in TBB 1.0-2.1 */
#define __TBB_DEFAULT_PARTITIONER tbb::simple_partitioner
#else
/** Default partitioner for parallel loop templates since TBB 2.2 */
#define __TBB_DEFAULT_PARTITIONER tbb::auto_partitioner
#endif /* TBB_DEPRECATED */
#endif /* !defined(__TBB_DEFAULT_PARTITIONER */

#ifdef _VARIADIC_MAX
#define __TBB_VARIADIC_MAX _VARIADIC_MAX
#else
#if _MSC_VER >= 1700
#define __TBB_VARIADIC_MAX 5  /* current VS11 setting, may change. */
#else
#define __TBB_VARIADIC_MAX 10
#endif
#endif

/** Macros of the form __TBB_XXX_BROKEN denote known issues that are caused by
    the bugs in compilers, standard or OS specific libraries. They should be 
    removed as soon as the corresponding bugs are fixed or the buggy OS/compiler
    versions go out of the support list. 
**/

#if __GNUC__ && __TBB_x86_64 && __INTEL_COMPILER == 1200
    #define __TBB_ICC_12_0_INL_ASM_FSTCW_BROKEN 1
#endif

#if _MSC_VER && __INTEL_COMPILER && (__INTEL_COMPILER<1110 || __INTEL_COMPILER==1110 && __INTEL_COMPILER_BUILD_DATE < 20091012)
    /** Necessary to avoid ICL error (or warning in non-strict mode): 
        "exception specification for implicitly declared virtual destructor is 
        incompatible with that of overridden one". **/
    #define __TBB_DEFAULT_DTOR_THROW_SPEC_BROKEN 1
#endif

#if defined(_MSC_VER) && _MSC_VER < 1500 && !defined(__INTEL_COMPILER)
    /** VS2005 and earlier do not allow declaring template class as a friend 
        of classes defined in other namespaces. **/
    #define __TBB_TEMPLATE_FRIENDS_BROKEN 1
#endif

#if __GLIBC__==2 && __GLIBC_MINOR__==3 || __MINGW32__ || (__APPLE__ && __INTEL_COMPILER==1200 && !TBB_USE_DEBUG)
    /** Macro controlling EH usages in TBB tests.
        Some older versions of glibc crash when exception handling happens concurrently. **/
    #define __TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN 1
#else
    #define __TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN 0
#endif

#if (_WIN32||_WIN64) && __INTEL_COMPILER == 1110
    /** That's a bug in Intel compiler 11.1.044/IA-32/Windows, that leads to a worker thread crash on the thread's startup. **/
    #define __TBB_ICL_11_1_CODE_GEN_BROKEN 1
#endif

#if __clang__ || (__GNUC__==3 && __GNUC_MINOR__==3 && !defined(__INTEL_COMPILER))
    /** Bugs with access to nested classes declared in protected area */
    #define __TBB_PROTECTED_NESTED_CLASS_BROKEN 1
#endif

#if __MINGW32__ && (__GNUC__<4 || __GNUC__==4 && __GNUC_MINOR__<2)
    /** MinGW has a bug with stack alignment for routines invoked from MS RTLs.
        Since GCC 4.2, the bug can be worked around via a special attribute. **/
    #define __TBB_SSE_STACK_ALIGNMENT_BROKEN 1
#else
    #define __TBB_SSE_STACK_ALIGNMENT_BROKEN 0
#endif

#if __GNUC__==4 && __GNUC_MINOR__==3 && __GNUC_PATCHLEVEL__==0
    /* GCC of this version may rashly ignore control dependencies */
    #define __TBB_GCC_OPTIMIZER_ORDERING_BROKEN 1
#endif

#if __FreeBSD__
    /** A bug in FreeBSD 8.0 results in kernel panic when there is contention 
        on a mutex created with this attribute. **/
    #define __TBB_PRIO_INHERIT_BROKEN 1

    /** A bug in FreeBSD 8.0 results in test hanging when an exception occurs 
        during (concurrent?) object construction by means of placement new operator. **/
    #define __TBB_PLACEMENT_NEW_EXCEPTION_SAFETY_BROKEN 1
#endif /* __FreeBSD__ */

#if (__linux__ || __APPLE__) && __i386__ && defined(__INTEL_COMPILER)
    /** The Intel compiler for IA-32 (Linux|Mac OS X) crashes or generates 
        incorrect code when __asm__ arguments have a cast to volatile. **/
    #define __TBB_ICC_ASM_VOLATILE_BROKEN 1
#endif

#if !__INTEL_COMPILER && (_MSC_VER || __GNUC__==3 && __GNUC_MINOR__<=2)
    /** Bug in GCC 3.2 and MSVC compilers that sometimes return 0 for __alignof(T) 
        when T has not yet been instantiated. **/
    #define __TBB_ALIGNOF_NOT_INSTANTIATED_TYPES_BROKEN 1
#endif

#if __INTEL_COMPILER
    #define __TBB_CPP11_STD_FORWARD_BROKEN 1
#else
    #define __TBB_CPP11_STD_FORWARD_BROKEN 0
#endif

#if __TBB_DEFINE_MIC
    /** Main thread and user's thread have different default thread affinity masks. **/
    #define __TBB_MAIN_THREAD_AFFINITY_BROKEN 1
#endif

#if !defined(__EXCEPTIONS) && __GNUC__==4 && (__GNUC_MINOR__==4 ||__GNUC_MINOR__==5) && defined(__GXX_EXPERIMENTAL_CXX0X__)
/* There is an issue for specific GCC toolchain when C++11 is enabled
   and exceptions are disabled:
   exceprion_ptr.h/nested_exception.h are using throw unconditionally.
 */
    #define __TBB_LIBSTDCPP_EXCEPTION_HEADERS_BROKEN 1
#else
    #define __TBB_LIBSTDCPP_EXCEPTION_HEADERS_BROKEN 0
#endif

#endif /* __TBB_tbb_config_H */
