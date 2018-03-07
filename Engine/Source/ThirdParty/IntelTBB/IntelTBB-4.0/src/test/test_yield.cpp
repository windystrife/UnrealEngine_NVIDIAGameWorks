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

// Test that __TBB_Yield works.
// On Red Hat EL4 U1, it does not work, because sched_yield is broken.

#include "tbb/tbb_machine.h"
#include "tbb/tick_count.h"
#include "harness.h"

static volatile long CyclicCounter;
static volatile bool Quit;
double SingleThreadTime;

struct RoundRobin: NoAssign {
    const int number_of_threads;
    RoundRobin( long p ) : number_of_threads(p) {}
    void operator()( long k ) const {
        tbb::tick_count t0 = tbb::tick_count::now();
        for( long i=0; i<10000; ++i ) {
            // Wait for previous thread to notify us 
            for( int j=0; CyclicCounter!=k && !Quit; ++j ) {
                __TBB_Yield();
                if( j%100==0 ) {
                    tbb::tick_count t1 = tbb::tick_count::now();
                    if( (t1-t0).seconds()>=1.0*number_of_threads ) {
                        REPORT("Warning: __TBB_Yield failing to yield with %d threads (or system is heavily loaded)\n",number_of_threads);
                        Quit = true;
                        return;
                    }
                }
            }
            // Notify next thread that it can run            
            CyclicCounter = (k+1)%number_of_threads;
        }
    }
};

int TestMain () {
    for( int p=MinThread; p<=MaxThread; ++p ) {
        REMARK("testing with %d threads\n", p );
        CyclicCounter = 0;
        Quit = false;
        NativeParallelFor( long(p), RoundRobin(p) );
    }
    return Harness::Done;
}

