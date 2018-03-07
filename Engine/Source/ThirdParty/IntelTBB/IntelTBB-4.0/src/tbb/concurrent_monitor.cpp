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

#include "concurrent_monitor.h"

namespace tbb {
namespace internal {

void concurrent_monitor::thread_context::init() {
    new (sema.begin()) binary_semaphore;
    ready = true;
}

concurrent_monitor::~concurrent_monitor() {
    abort_all();
    __TBB_ASSERT( waitset_ec.empty(), "waitset not empty?" );
}

void concurrent_monitor::prepare_wait( thread_context& thr, uintptr_t ctx ) {
    if( !thr.ready )
        thr.init();
    // this is good place to pump previous spurious wakeup
    else if( thr.spurious ) {
        thr.spurious = false;
        thr.semaphore().P();
    }
    thr.context = ctx;
    thr.in_waitset = true;
    {
        tbb::spin_mutex::scoped_lock l( mutex_ec );
        __TBB_store_relaxed( thr.epoch, __TBB_load_relaxed(epoch) );
        waitset_ec.add( (waitset_t::node_t*)&thr );
    }
    atomic_fence();
}

void concurrent_monitor::cancel_wait( thread_context& thr ) {
    // spurious wakeup will be pumped in the following prepare_wait()
    thr.spurious = true;
    // try to remove node from waitset
    bool th_in_waitset = thr.in_waitset;
    if( th_in_waitset ) {
        tbb::spin_mutex::scoped_lock l( mutex_ec );
        if (thr.in_waitset) {
            // successfully removed from waitset,
            // so there will be no spurious wakeup
            thr.in_waitset = false;
            thr.spurious = false;
            waitset_ec.remove( (waitset_t::node_t&)thr );
        }
    }
}

void concurrent_monitor::notify_one_relaxed() {
    if( waitset_ec.empty() )
        return;
    waitset_node_t* n;
    const waitset_node_t* end = waitset_ec.end();
    {
        tbb::spin_mutex::scoped_lock l( mutex_ec );
        __TBB_store_relaxed( epoch, __TBB_load_relaxed(epoch) + 1 );
        n = waitset_ec.front();
        if( n!=end ) {
            waitset_ec.remove( *n );
            to_thread_context(n)->in_waitset = false;
        }
    }
    if( n!=end )
        to_thread_context(n)->semaphore().V();
}

void concurrent_monitor::notify_all_relaxed() {
    if( waitset_ec.empty() )
        return;
    dllist_t temp;
    const waitset_node_t* end;
    {
        tbb::spin_mutex::scoped_lock l( mutex_ec );
        __TBB_store_relaxed( epoch, __TBB_load_relaxed(epoch) + 1 );
        waitset_ec.flush_to( temp );
        end = temp.end();
        for( waitset_node_t* n=temp.front(); n!=end; n=n->next )
            to_thread_context(n)->in_waitset = false;
    }
    waitset_node_t* nxt;
    for( waitset_node_t* n=temp.front(); n!=end; n=nxt ) {
        nxt = n->next;
        to_thread_context(n)->semaphore().V();
    }
#if TBB_USE_ASSERT
    temp.clear();
#endif
}

void concurrent_monitor::abort_all_relaxed() {
    if( waitset_ec.empty() )
        return;
    dllist_t temp;
    const waitset_node_t* end;
    {
        tbb::spin_mutex::scoped_lock l( mutex_ec );
        __TBB_store_relaxed( epoch, __TBB_load_relaxed(epoch) + 1 );
        waitset_ec.flush_to( temp );
        end = temp.end();
        for( waitset_node_t* n=temp.front(); n!=end; n=n->next )
            to_thread_context(n)->in_waitset = false;
    }
    waitset_node_t* nxt;
    for( waitset_node_t* n=temp.front(); n!=end; n=nxt ) {
        nxt = n->next;
        to_thread_context(n)->aborted = true;
        to_thread_context(n)->semaphore().V();
    }
#if TBB_USE_ASSERT
    temp.clear();
#endif
}

} // namespace internal
} // namespace tbb
