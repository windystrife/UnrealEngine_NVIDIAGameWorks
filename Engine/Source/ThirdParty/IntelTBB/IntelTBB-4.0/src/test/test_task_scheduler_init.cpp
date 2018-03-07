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

#include "tbb/task_scheduler_init.h"
#include <cstdlib>
#include "harness_assert.h"

#include <cstdio>

#if !TBB_USE_EXCEPTIONS && _MSC_VER
    // Suppress "C++ exception handler used, but unwind semantics are not enabled" warning in STL headers
    #pragma warning (push)
    #pragma warning (disable: 4530)
#endif

#include <stdexcept>

#if !TBB_USE_EXCEPTIONS && _MSC_VER
    #pragma warning (pop)
#endif

#include "harness.h"

//! Test that task::initialize and task::terminate work when doing nothing else.
/** maxthread is treated as the "maximum" number of worker threads. */
void InitializeAndTerminate( int maxthread ) {
    __TBB_TRY {
        for( int i=0; i<200; ++i ) {
            switch( i&3 ) {
                default: {
                    tbb::task_scheduler_init init( std::rand() % maxthread + 1 );
                    ASSERT(init.is_active(), NULL);
                    break;
                }
                case 0: {   
                    tbb::task_scheduler_init init;
                    ASSERT(init.is_active(), NULL);
                    break;
                }
                case 1: {
                    tbb::task_scheduler_init init( tbb::task_scheduler_init::automatic );
                    ASSERT(init.is_active(), NULL);
                    break;
                }
                case 2: {
                    tbb::task_scheduler_init init( tbb::task_scheduler_init::deferred );
                    ASSERT(!init.is_active(), "init should not be active; initialization was deferred");
                    init.initialize( std::rand() % maxthread + 1 );
                    ASSERT(init.is_active(), NULL);
                    init.terminate();
                    ASSERT(!init.is_active(), "init should not be active; it was terminated");
                    break;
                }
            }
        }
    } __TBB_CATCH( std::runtime_error& error ) {
#if TBB_USE_EXCEPTIONS
        REPORT("ERROR: %s\n", error.what() );
#endif /* TBB_USE_EXCEPTIONS */
    }
}

#if _WIN64
namespace std {      // 64-bit Windows compilers have not caught up with 1998 ISO C++ standard
    using ::srand;
}
#endif /* _WIN64 */

struct ThreadedInit {
    void operator()( int ) const {
        InitializeAndTerminate(MaxThread);
    }
};

#if _MSC_VER
#include "tbb/machine/windows_api.h"
#include <tchar.h>
#endif /* _MSC_VER */

#include "harness_concurrency_tracker.h"
#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"

typedef tbb::blocked_range<int> Range;

class ConcurrencyTrackingBody {
public:
    void operator() ( const Range& ) const {
        Harness::ConcurrencyTracker ct;
        for ( volatile int i = 0; i < 1000000; ++i )
            ;
    }
};

/** The test will fail in particular if task_scheduler_init mistakenly hooks up 
    auto-initialization mechanism. **/
void AssertExplicitInitIsNotSupplanted () {
    int hardwareConcurrency = tbb::task_scheduler_init::default_num_threads();
    tbb::task_scheduler_init init(1);
    Harness::ConcurrencyTracker::Reset();
    tbb::parallel_for( Range(0, hardwareConcurrency * 2, 1), ConcurrencyTrackingBody(), tbb::simple_partitioner() );
    ASSERT( Harness::ConcurrencyTracker::PeakParallelism() == 1, 
            "Manual init provided more threads than requested. See also the comment at the beginning of main()." );
}

int TestMain () {
    // Do not use tbb::task_scheduler_init directly in the scope of main's body,
    // as a static variable, or as a member of a static variable.
#if _MSC_VER && !__TBB_NO_IMPLICIT_LINKAGE && !defined(__TBB_LIB_NAME)
    #ifdef _DEBUG
        ASSERT(!GetModuleHandle(_T("tbb.dll")) && GetModuleHandle(_T("tbb_debug.dll")),
            "test linked with wrong (non-debug) tbb library");
    #else
        ASSERT(!GetModuleHandle(_T("tbb_debug.dll")) && GetModuleHandle(_T("tbb.dll")),
            "test linked with wrong (debug) tbb library");
    #endif
#endif /* _MSC_VER && !__TBB_NO_IMPLICIT_LINKAGE && !__TBB_LIB_NAME */
    std::srand(2);
    InitializeAndTerminate(MaxThread);
    for( int p=MinThread; p<=MaxThread; ++p ) {
        REMARK("testing with %d threads\n", p );
        NativeParallelFor( p, ThreadedInit() );
    }
    AssertExplicitInitIsNotSupplanted();
    return Harness::Done;
}
