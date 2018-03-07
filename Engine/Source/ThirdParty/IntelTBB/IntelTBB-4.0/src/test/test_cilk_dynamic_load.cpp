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

#include "tbb/tbb_config.h"

// Skip the test if no interoperability with cilkrts
#define __TBB_CILK_INTEROP   (__TBB_SURVIVE_THREAD_SWITCH && __INTEL_COMPILER>=1200)
// Skip the test when cilkrts did not have dlopen()/dlclose() start up feature
#define CILK_SYMBOLS_VISIBLE (_WIN32||_WIN64)
// The compiler does not add "-lcilkrts" linker option on some linux systems
#define CILK_LINKAGE_BROKEN  (__linux__ && __GNUC__<4 && __INTEL_COMPILER_BUILD_DATE <= 20110427)
// Currently, the interop doesn't support the situation:
//1) TBB is outermost;
//2)   Cilk, and it should be dynamically loaded with dlopen/LoadLibrary (possibly via a 3rd party module);
//3)     TBB again;
//4)       Cilk again.
#define HEAVY_NESTED_INTEROP_SUPPORT __INTEL_COMPILER_BUILD_DATE < 20110427

#if __TBB_CILK_INTEROP && CILK_SYMBOLS_VISIBLE && !CILK_LINKAGE_BROKEN && HEAVY_NESTED_INTEROP_SUPPORT

#include "tbb/task_scheduler_init.h"
#include "tbb/task.h"

static const int N = 25;
static const int P_outer = 4;
static const int P_nested = 2;

#ifdef _USRDLL

#include <cilk/cilk.h>
#define HARNESS_CUSTOM_MAIN 1
#include "harness.h"
#undef HARNESS_CUSTOM_MAIN

#if _WIN32 || _WIN64
#define CILK_TEST_EXPORT extern "C" __declspec(dllexport)
#else
#define CILK_TEST_EXPORT extern "C"
#endif /* _WIN32 || _WIN64 */

bool g_sandwich = true; // have to be declare before #include "test_cilk_common.h"
#include "test_cilk_common.h"

CILK_TEST_EXPORT int CilkFib( int n )
{
    return TBB_Fib(n);
}

CILK_TEST_EXPORT void CilkShutdown()
{
    __cilkrts_end_cilk();
}

#else /* _USRDLL undefined */

#include "harness.h"
#include "harness_dynamic_libs.h"

int SerialFib( int n ) {
    int a=0, b=1;
    for( int i=0; i<n; ++i ) {
        b += a;
        a = b-a;
    }
    return a;
}

int F = SerialFib(N);

typedef int (*CILK_CALL)(int);
CILK_CALL CilkFib = 0;

typedef void (*CILK_SHUTDOWN)();
CILK_SHUTDOWN CilkShutdown = 0;

class FibTask: public tbb::task {
    int n;
    int& result;
    /*override*/ task* execute() {
        if( n<2 ) {
            result = n;
        } else {

            // TODO: why RTLD_LAZY was used here?
            Harness::LIBRARY_HANDLE hLib =
                Harness::OpenLibrary(TEST_LIBRARY_NAME("test_cilk_dynamic_load_dll"));
            CilkFib = (CILK_CALL)Harness::GetAddress(hLib, "CilkFib");
            CilkShutdown = (CILK_SHUTDOWN)Harness::GetAddress(hLib, "CilkShutdown");

            int x, y;
            x = CilkFib(n-2);
            y = CilkFib(n-1);
            result = x+y;

            CilkShutdown();

            Harness::CloseLibrary(hLib);
        }
        return NULL;
    }
public:
    FibTask( int& result_, int n_ ) : result(result_), n(n_) {}
};


int TBB_Fib( int n ) {
    if( n<2 ) {
        return n;
    } else {
        int result;
        tbb::task_scheduler_init init(P_nested);
        tbb::task::spawn_root_and_wait(*new( tbb::task::allocate_root()) FibTask(result,n) );
        return result;
    }
}

void RunSandwich() { 
    tbb::task_scheduler_init init(P_outer);
    int m = TBB_Fib(N);
    ASSERT( m == F, NULL );
}

int TestMain () {
    for ( int i = 0; i < 20; ++i )
        RunSandwich();
    return Harness::Done;
}

#endif /* _USRDLL */

#else /* !__TBB_CILK_INTEROP */

#include "harness.h"

int TestMain () {
    return Harness::Skipped;
}

#endif /* !__TBB_CILK_INTEROP */
