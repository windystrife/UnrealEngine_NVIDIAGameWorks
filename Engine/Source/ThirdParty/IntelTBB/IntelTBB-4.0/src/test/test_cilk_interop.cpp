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
#include "harness.h"

// Skip the test if no interoperability with cilkrts
#define __TBB_CILK_INTEROP   (__TBB_SURVIVE_THREAD_SWITCH && __INTEL_COMPILER>=1200)
// The compiler does not add "-lcilkrts" linker option on some linux systems
#define CILK_LINKAGE_BROKEN  (__linux__ && __GNUC__<4 && __INTEL_COMPILER_BUILD_DATE <= 20110427)
// In U4, cilkrts incorrectly sends the interop notifications to TBB
#define CILK_NOTIFICATIONS_BROKEN ( __INTEL_COMPILER_BUILD_DATE == 20110427 )

#if __TBB_CILK_INTEROP && !CILK_LINKAGE_BROKEN && !CILK_NOTIFICATIONS_BROKEN

static const int N = 14;
static const int P_outer = 4;
static const int P_nested = 2;

#include <cilk/cilk.h>
#define private public
#include "tbb/task.h"
#undef private
#include "tbb/task_scheduler_init.h"
#include <cstdio>
#include <cassert>

enum tbb_sched_injection_mode_t {
    tbbsched_none = 0,
    tbbsched_explicit_only = 1,
    tbbsched_auto_only = 2,
    tbbsched_mixed = 3
};

tbb_sched_injection_mode_t g_sim = tbbsched_none;

bool g_sandwich = false; // have to be declare before #include "test_cilk_common.h"
#include "test_cilk_common.h"

// A time delay routine
void Delay( int n ) {
    static volatile int Global;
    for( int k=0; k<10000; ++k )
        for( int i=0; i<n; ++i )
            ++Global;
}

int SerialFib( int n ) {
    int a=0, b=1;
    for( int i=0; i<n; ++i ) {
        b += a;
        a = b-a;
    }
    return a;
}

int F = SerialFib(N);

int Fib ( int n ) {
    if( n < 2 ) {
        if ( g_sim ) {
            tbb::task_scheduler_init tsi(P_nested);
        }
        return n;
    } else {
        tbb::task_scheduler_init *tsi = NULL;
        tbb::task *cur = NULL;
        if ( g_sim ) {
            if ( n % 2 == 0 ) {
                if ( g_sim == tbbsched_auto_only || (g_sim == tbbsched_mixed && n % 4 == 0) ) {
                    // Trigger TBB scheduler auto-initialization
                    cur = &tbb::task::self();
                }
                else {
                    ASSERT ( g_sim == tbbsched_explicit_only || (g_sim == tbbsched_mixed && n % 4 != 0), NULL );
                    // Initialize TBB scheduler explicitly
                    tsi = new tbb::task_scheduler_init(P_nested);
                }
            }
        }
        int x, y;
        x = cilk_spawn Fib(n-2);
        y = cilk_spawn Fib(n-1);
        cilk_sync;
        if ( tsi )
            delete tsi;
        return x+y;
    }
}

void RunCilkOnly ( tbb_sched_injection_mode_t sim ) {
    g_sim = sim;
    int m = Fib(N);
    ASSERT( m == F, NULL );
}

struct FibBody : NoAssign, Harness::NoAfterlife {
    void operator() ( int ) const {
        int m = Fib(N);
        ASSERT( m == F, NULL );
    }
};

void RunCilkOnlyConcurrently ( tbb_sched_injection_mode_t sim ) {
    g_sim = sim;
    NativeParallelFor( P_outer, FibBody() );
}

void RunSandwich( bool sandwich ) { 
    g_sandwich = sandwich;
    tbb::task_scheduler_init init(P_outer);
    int m = TBB_Fib(N);
    ASSERT( g_sandwich == sandwich, "Memory corruption detected" );
    ASSERT( m == F, NULL );
}

int TestMain () {
    for ( int i = 0; i < 100; ++i )
        RunCilkOnlyConcurrently( tbbsched_none );
    RunCilkOnly( tbbsched_none );
    RunCilkOnly( tbbsched_explicit_only );
    RunCilkOnly( tbbsched_auto_only );
    RunCilkOnly( tbbsched_mixed );
    RunSandwich( false );
    for ( int i = 0; i < 10; ++i )
        RunSandwich( true );
    __cilkrts_end_cilk();
    return Harness::Done;
}

#else /* !__TBB_CILK_INTEROP */

int TestMain () {
    return Harness::Skipped;
}

#endif /* !__TBB_CILK_INTEROP */
