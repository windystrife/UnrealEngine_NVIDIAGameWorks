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

#ifndef tbb_tests_harness_concurrency_tracker_H
#define tbb_tests_harness_concurrency_tracker_H

#include "harness.h"
#include "tbb/atomic.h"
#include "../tbb/tls.h"

namespace Harness {

static tbb::atomic<unsigned> ctInstantParallelism;
static tbb::atomic<unsigned> ctPeakParallelism;
static tbb::internal::tls<uintptr_t>  ctNested;

class ConcurrencyTracker {
    bool    m_Outer;

    static void Started () {
        unsigned p = ++ctInstantParallelism;
        unsigned q = ctPeakParallelism;
        while( q<p ) {
            q = ctPeakParallelism.compare_and_swap(p,q);
        }
    }

    static void Stopped () {
        ASSERT ( ctInstantParallelism > 0, "Mismatched call to ConcurrencyTracker::Stopped()" );
        --ctInstantParallelism;
    }
public:
    ConcurrencyTracker() : m_Outer(false) {
        uintptr_t nested = ctNested;
        ASSERT (nested == 0 || nested == 1, NULL);
        if ( !ctNested ) {
            Started();
            m_Outer = true;
            ctNested = 1;
        }
    }
    ~ConcurrencyTracker() {
        if ( m_Outer ) {
            Stopped();
            ctNested = 0;
        }
    }

    static unsigned PeakParallelism() { return ctPeakParallelism; }
    static unsigned InstantParallelism() { return ctInstantParallelism; }

    static void Reset() {
        ASSERT (ctInstantParallelism == 0, "Reset cannot be called when concurrency tracking is underway");
        ctInstantParallelism = ctPeakParallelism = 0;
    }
}; // ConcurrencyTracker

} // namespace Harness

#endif /* tbb_tests_harness_concurrency_tracker_H */
