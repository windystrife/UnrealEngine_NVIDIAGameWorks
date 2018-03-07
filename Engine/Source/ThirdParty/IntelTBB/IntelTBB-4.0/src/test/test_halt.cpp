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

#include "harness_defs.h"
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <utility>
#include "tbb/task.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/tick_count.h"
#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"
#include "tbb/mutex.h"
#include "tbb/spin_mutex.h"
#include "tbb/queuing_mutex.h"
#include "harness.h"

using namespace std;
using namespace tbb;

///////////////////// Parallel methods ////////////////////////

// *** Serial shared by mutexes *** //
int SharedI = 1, SharedN;
template<typename M>
class SharedSerialFibBody: NoAssign {
    M &mutex;
public:
    SharedSerialFibBody( M &m ) : mutex( m ) {}
    //! main loop
    void operator()( const blocked_range<int>& /*range*/ ) const {
        for(;;) {
            typename M::scoped_lock lock( mutex );
            if(SharedI >= SharedN) break;
            volatile double sum = 7.3; 
            sum *= 11.17;
            ++SharedI;
        }
    }
};

//! Root function
template<class M>
void SharedSerialFib(int n)
{
    SharedI = 1; 
    SharedN = n; 
    M mutex;
    parallel_for( blocked_range<int>(0,4,1), SharedSerialFibBody<M>( mutex ) );
}

/////////////////////////// Main ////////////////////////////////////////////////////

double Tsum = 0; int Tnum = 0;

typedef void (*MeasureFunc)(int);
//! Measure ticks count in loop [2..n]
void Measure(const char *name, MeasureFunc func, int n)
{
    tick_count t0;
    tick_count::interval_t T;
    REMARK("%s",name);
    t0 = tick_count::now();
    for(int number = 2; number <= n; number++)
        func(number);
    T = tick_count::now() - t0;
    double avg = Tnum? Tsum/Tnum : 1;
    if (avg == 0.0) avg = 1;
    if(avg * 100 < T.seconds()) {
        REPORT("Warning: halting detected (%g sec, av: %g)\n", T.seconds(), avg);
        ASSERT(avg * 1000 > T.seconds(), "Too long halting period");
    } else {
        Tsum += T.seconds(); Tnum++;
    }
    REMARK("\t- in %f msec\n", T.seconds()*1000);
}

int TestMain () {
    MinThread = max(2, MinThread);
    int NumbersCount = 100;
    short recycle = 100;
    do {
        for(int threads = MinThread; threads <= MaxThread; threads++) {
            task_scheduler_init scheduler_init(threads);
            REMARK("Threads number is %d\t", threads);
            Measure("Shared serial (wrapper mutex)\t", SharedSerialFib<mutex>, NumbersCount);
            //sum = Measure("Shared serial (spin_mutex)", SharedSerialFib<tbb::spin_mutex>, NumbersCount);
            //sum = Measure("Shared serial (queuing_mutex)", SharedSerialFib<tbb::queuing_mutex>, NumbersCount);
        }
    } while(--recycle);
    return Harness::Done;
}
