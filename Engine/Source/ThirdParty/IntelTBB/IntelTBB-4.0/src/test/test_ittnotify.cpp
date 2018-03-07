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

#if !TBB_USE_THREADING_TOOLS
    #define TBB_USE_THREADING_TOOLS 1
#endif

#include "harness.h"

#if DO_ITT_NOTIFY

#include "tbb/spin_mutex.h"
#include "tbb/spin_rw_mutex.h"
#include "tbb/queuing_rw_mutex.h"
#include "tbb/queuing_mutex.h"
#include "tbb/mutex.h"
#include "tbb/recursive_mutex.h"
#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"
#include "tbb/task_scheduler_init.h"


#include "../tbb/itt_notify.h"


template<typename M>
class WorkEmulator: NoAssign {
    M& m_mutex;
    static volatile size_t s_anchor;
public:
    void operator()( tbb::blocked_range<size_t>& range ) const {
        for( size_t i=range.begin(); i!=range.end(); ++i ) {
            typename M::scoped_lock lock(m_mutex);
            for ( size_t j = 0; j!=range.end(); ++j )
                s_anchor = (s_anchor - i) / 2 + (s_anchor + j) / 2;
        }
    }
    WorkEmulator( M& mutex ) : m_mutex(mutex) {}
};

template<typename M>
volatile size_t WorkEmulator<M>::s_anchor = 0;


template<class M>
void Test( const char * name ) {
    REMARK("Testing %s\n",name);
    M mtx;
    tbb::profiling::set_name(mtx, name);

    const int n = 10000;
    tbb::parallel_for( tbb::blocked_range<size_t>(0,n,n/100), WorkEmulator<M>(mtx) );
}

    #define TEST_MUTEX(type, name)  Test<tbb::type>( name )

#endif /* !DO_ITT_NOTIFY */

int TestMain () {
#if DO_ITT_NOTIFY
    for( int p=MinThread; p<=MaxThread; ++p ) {
        REMARK( "testing with %d workers\n", p );
        tbb::task_scheduler_init init( p );
        TEST_MUTEX( spin_mutex, "Spin Mutex" );
        TEST_MUTEX( queuing_mutex, "Queuing Mutex" );
        TEST_MUTEX( queuing_rw_mutex, "Queuing RW Mutex" );
        TEST_MUTEX( spin_rw_mutex, "Spin RW Mutex" );
    }
    return Harness::Done;
#else /* !DO_ITT_NOTIFY */
    return Harness::Skipped;
#endif /* !DO_ITT_NOTIFY */
}
