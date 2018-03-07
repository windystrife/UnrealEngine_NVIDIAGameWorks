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

#include "tbb/queuing_rw_mutex.h"
#include "tbb/spin_rw_mutex.h"
#include "harness.h"

using namespace tbb;

volatile int Count;

template<typename RWMutex>
struct Hammer: NoAssign {
    RWMutex &MutexProtectingCount;
    mutable volatile int dummy;

    Hammer(RWMutex &m): MutexProtectingCount(m) {}
    void operator()( int /*thread_id*/ ) const {
        for( int j=0; j<100000; ++j ) {
            typename RWMutex::scoped_lock lock(MutexProtectingCount,false);
            int c = Count;
            for( int k=0; k<10; ++k ) {
                ++dummy;
            }
            if( lock.upgrade_to_writer() ) {
                // The upgrade succeeded without any intervening writers
                ASSERT( c==Count, "another thread modified Count while I held a read lock" );
            } else {
                c = Count;
            }
            for( int k=0; k<10; ++k ) {
                ++Count;
            }
            lock.downgrade_to_reader();
            for( int k=0; k<10; ++k ) {
                ++dummy;
            }
        }
    }
};

queuing_rw_mutex QRW_mutex;
spin_rw_mutex SRW_mutex;

int TestMain () {
    for( int p=MinThread; p<=MaxThread; ++p ) {
        REMARK("Testing on %d threads", p);
        Count = 0;
        NativeParallelFor( p, Hammer<queuing_rw_mutex>(QRW_mutex) ); 
        Count = 0;
        NativeParallelFor( p, Hammer<spin_rw_mutex>(SRW_mutex) );
    }
    return Harness::Done;
}
