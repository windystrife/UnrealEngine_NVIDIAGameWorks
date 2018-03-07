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

#ifndef _TBB_task_stream_H
#define _TBB_task_stream_H

#include "tbb/tbb_stddef.h"
#include <deque>
#include <climits>
#include "tbb/atomic.h" // for __TBB_Atomic*
#include "tbb/spin_mutex.h"
#include "tbb/tbb_allocator.h"
#include "scheduler_common.h"
#include "tbb_misc.h" // for FastRandom

namespace tbb {
namespace internal {

//! Essentially, this is just a pair of a queue and a mutex to protect the queue.
/** The reason std::pair is not used is that the code would look less clean
    if field names were replaced with 'first' and 'second'. **/
template< typename T, typename mutex_t >
struct queue_and_mutex {
    typedef std::deque< T, tbb_allocator<T> > queue_base_t;

    queue_base_t my_queue;
    mutex_t      my_mutex;

    queue_and_mutex () : my_queue(), my_mutex() {}
    ~queue_and_mutex () {}
};

const uintptr_t one = 1;

inline void set_one_bit( uintptr_t& dest, int pos ) {
    __TBB_ASSERT( pos>=0, NULL );
    __TBB_ASSERT( pos<32, NULL );
    __TBB_AtomicOR( &dest, one<<pos );
}

inline void clear_one_bit( uintptr_t& dest, int pos ) {
    __TBB_ASSERT( pos>=0, NULL );
    __TBB_ASSERT( pos<32, NULL );
    __TBB_AtomicAND( &dest, ~(one<<pos) );
}

inline bool is_bit_set( uintptr_t val, int pos ) {
    __TBB_ASSERT( pos>=0, NULL );
    __TBB_ASSERT( pos<32, NULL );
    return (val & (one<<pos)) != 0;
}

//! The container for "fairness-oriented" aka "enqueued" tasks.
class task_stream : no_copy{
    typedef queue_and_mutex <task*, spin_mutex> lane_t;
    unsigned N;
    uintptr_t population;
    FastRandom random;
    padded<lane_t>* lanes;

public:
    task_stream() : N(), population(), random(unsigned(&N-(unsigned*)NULL)), lanes()
    {
        __TBB_ASSERT( sizeof(population) * CHAR_BIT >= 32, NULL );
    }

    void initialize( unsigned n_lanes ) {
        N = n_lanes>=32 ? 32 : n_lanes>2 ? 1<<(__TBB_Log2(n_lanes-1)+1) : 2;
        __TBB_ASSERT( N==32 || N>=n_lanes && ((N-1)&N)==0, "number of lanes miscalculated");
        lanes = new padded<lane_t>[N];
        __TBB_ASSERT( !population, NULL );
    }

    ~task_stream() { if (lanes) delete[] lanes; }

    //! Push a task into a lane.
    void push( task* source, unsigned& last_random ) {
        // Lane selection is random. Each thread should keep a separate seed value.
        unsigned idx;
        for( ; ; ) {
            idx = random.get(last_random) & (N-1);
            spin_mutex::scoped_lock lock;
            if( lock.try_acquire(lanes[idx].my_mutex) ) {
                lanes[idx].my_queue.push_back(source);
                set_one_bit( population, idx ); //TODO: avoid atomic op if the bit is already set
                break;
            }
        }
    }
    //! Try finding and popping a task.
    /** Does not change destination if unsuccessful. */
    void pop( task*& dest, unsigned& last_used_lane ) {
        if( !population ) return; // keeps the hot path shorter
        // Lane selection is round-robin. Each thread should keep its last used lane.
        unsigned idx = (last_used_lane+1)&(N-1);
        for( ; population; idx=(idx+1)&(N-1) ) {
            if( is_bit_set( population, idx ) ) {
                lane_t& lane = lanes[idx];
                spin_mutex::scoped_lock lock;
                if( lock.try_acquire(lane.my_mutex) && !lane.my_queue.empty() ) {
                    dest = lane.my_queue.front();
                    lane.my_queue.pop_front();
                    if( lane.my_queue.empty() )
                        clear_one_bit( population, idx );
                    break;
                }
            }
        }
        last_used_lane = idx;
    }

    //! Checks existence of a task.
    bool empty() {
        return !population;
    }
    //! Destroys all remaining tasks in every lane. Returns the number of destroyed tasks.
    /** Tasks are not executed, because it would potentially create more tasks at a late stage.
        The scheduler is really expected to execute all tasks before task_stream destruction. */
    intptr_t drain() {
        intptr_t result = 0;
        for(unsigned i=0; i<N; ++i) {
            lane_t& lane = lanes[i];
            spin_mutex::scoped_lock lock(lane.my_mutex);
            for(lane_t::queue_base_t::iterator it=lane.my_queue.begin();
                it!=lane.my_queue.end(); ++it, ++result)
            {
                task* t = *it;
                tbb::task::destroy(*t);
            }
            lane.my_queue.clear();
            clear_one_bit( population, i );
        }
        return result;
    }
}; // task_stream

} // namespace internal
} // namespace tbb

#endif /* _TBB_task_stream_H */
