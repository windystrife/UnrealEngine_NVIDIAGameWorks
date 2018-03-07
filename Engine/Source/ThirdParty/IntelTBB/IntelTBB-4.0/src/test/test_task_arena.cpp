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

#if __TBB_CPF_BUILD
// undefine __TBB_CPF_BUILD to simulate user's setup
#undef __TBB_CPF_BUILD

#define TBB_PREVIEW_TASK_ARENA 1

#include "tbb/task_arena.h"
#include "tbb/task_scheduler_observer.h"
#include "tbb/task_scheduler_init.h"
#include <cstdlib>
#include "harness_assert.h"

#include <cstdio>

#include "harness.h"
#include "harness_barrier.h"
#include "harness_concurrency_tracker.h"
#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"
#include "tbb/enumerable_thread_specific.h"

#if _MSC_VER
// plays around __TBB_NO_IMPLICIT_LINKAGE. __TBB_LIB_NAME should be defined (in makefiles)
    #pragma comment(lib, __TBB_STRING(__TBB_LIB_NAME))
#endif

typedef tbb::blocked_range<int> Range;

class ConcurrencyTrackingBody {
public:
    void operator() ( const Range& ) const {
        Harness::ConcurrencyTracker ct;
        for ( volatile int i = 0; i < 1000000; ++i )
            ;
    }
};

Harness::SpinBarrier our_barrier;

static tbb::enumerable_thread_specific<int> local_id, old_id;
void ResetTLS() {
    local_id.clear();
    old_id.clear();
}

class ArenaObserver : public tbb::task_scheduler_observer {
    int myId;
    tbb::atomic<int> myTrappedSlot;
    /*override*/
    void on_scheduler_entry( bool is_worker ) {
        REMARK("a %s #%p is entering arena %d from %d\n", is_worker?"worker":"master", &local_id.local(), myId, local_id.local());
        ASSERT(!old_id.local(), "double-call to on_scheduler_entry");
        old_id.local() = local_id.local();
        ASSERT(old_id.local() != myId, "double-entry to the same arena");
        local_id.local() = myId;
        if(is_worker) ASSERT(tbb::task_arena::current_slot()>0, NULL);
        else ASSERT(tbb::task_arena::current_slot()==0, NULL);
    }
    /*override*/
    void on_scheduler_exit( bool is_worker ) {
        REMARK("a %s #%p is leaving arena %d to %d\n", is_worker?"worker":"master", &local_id.local(), myId, old_id.local());
        //ASSERT(old_id.local(), "call to on_scheduler_exit without prior entry");
        ASSERT(local_id.local() == myId, "nesting of arenas is broken");
        local_id.local() = old_id.local();
        old_id.local() = 0;
    }
    /*override*/
    bool on_scheduler_leaving() {
        return tbb::task_arena::current_slot() >= myTrappedSlot;
    }
public:
    ArenaObserver(tbb::task_arena &a, int id, int trap = 0) : tbb::task_scheduler_observer(a) {
        observe(true);
        ASSERT(id, NULL);
        myId = id;
        myTrappedSlot = trap;
    }
    ~ArenaObserver () {
        ASSERT(!old_id.local(), "inconsistent observer state");
    }
};

struct AsynchronousWork : NoAssign {
    Harness::SpinBarrier &my_barrier;
    bool my_is_blocking;
    AsynchronousWork(Harness::SpinBarrier &a_barrier, bool blocking = true)
    : my_barrier(a_barrier), my_is_blocking(blocking) {}
    void operator()() const {
        ASSERT(local_id.local() != 0, "not in explicit arena");
        tbb::parallel_for(Range(0,35), ConcurrencyTrackingBody());
        if(my_is_blocking) my_barrier.timed_wait(10); // must be asynchronous to master thread
        else my_barrier.signal_nowait();
    }
};

void TestConcurrentArenas(int p) {
    //Harness::ConcurrencyTracker::Reset();
    tbb::task_arena a1(1);
    ArenaObserver o1(a1, p*2+1);
    tbb::task_arena a2(2);
    ArenaObserver o2(a2, p*2+2);
    Harness::SpinBarrier barrier(2);
    AsynchronousWork work(barrier);
    a1.enqueue(work); // put async work
    barrier.timed_wait(10);
    a2.enqueue(work); // another work
    a2.execute(work); // my_barrier.timed_wait(10) inside
    a1.wait_until_empty();
    a2.wait_until_empty();
}

class MultipleMastersBody : NoAssign {
    tbb::task_arena &my_a;
    Harness::SpinBarrier &my_b;
public:
    MultipleMastersBody(tbb::task_arena &a, Harness::SpinBarrier &b)
    : my_a(a), my_b(b) {}
    void operator()(int) const {
        my_a.execute(AsynchronousWork(my_b, /*blocking=*/false));
        my_a.wait_until_empty();
    }
};

void TestMultipleMasters(int p) {
    REMARK("multiple masters\n");
    tbb::task_arena a(1);
    ArenaObserver o(a, 1);
    Harness::SpinBarrier barrier(p+1);
    NativeParallelFor( p, MultipleMastersBody(a, barrier) );
    a.wait_until_empty();
    barrier.timed_wait(10);
}


int TestMain () {
    // TODO: a workaround for temporary p-1 issue in market
    tbb::task_scheduler_init init_market_p_plus_one(MaxThread+1);
    for( int p=MinThread; p<=MaxThread; ++p ) {
        REMARK("testing with %d threads\n", p );
        NativeParallelFor( p, &TestConcurrentArenas );
        ResetTLS();
        TestMultipleMasters( p );
        ResetTLS();
    }
    return Harness::Done;
}
#else // __TBB_CPF_BUILD
#include "harness.h"
int TestMain () {
    return Harness::Skipped;
}
#endif
