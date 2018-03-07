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

#ifndef __RML_wait_counter_H
#define __RML_wait_counter_H

#include "thread_monitor.h"
#include "tbb/atomic.h"

namespace rml {
namespace internal {

class wait_counter {
    thread_monitor my_monitor;
    tbb::atomic<int> my_count;
    tbb::atomic<int> n_transients;
public:
    wait_counter() { 
        // The "1" here is subtracted by the call to "wait".
        my_count=1;
        n_transients=0;
    }

    //! Wait for number of operator-- invocations to match number of operator++ invocations.
    /** Exactly one thread should call this method. */
    void wait() {
        int k = --my_count;
        __TBB_ASSERT( k>=0, "counter underflow" );
        if( k>0 ) {
            thread_monitor::cookie c;
            my_monitor.prepare_wait(c);
            if( my_count )
                my_monitor.commit_wait(c);
            else 
                my_monitor.cancel_wait();
        }
        while( n_transients>0 )
            __TBB_Yield();
    }
    void operator++() {
        ++my_count;
    }
    void operator--() {
        ++n_transients;
        int k = --my_count;
        __TBB_ASSERT( k>=0, "counter underflow" );
        if( k==0 ) 
            my_monitor.notify();
        --n_transients;
    }
};

} // namespace internal
} // namespace rml

#endif /* __RML_wait_counter_H */
