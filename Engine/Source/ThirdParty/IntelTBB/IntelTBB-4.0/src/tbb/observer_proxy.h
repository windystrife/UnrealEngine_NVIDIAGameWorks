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

#ifndef _TBB_observer_proxy_H
#define _TBB_observer_proxy_H

#if __TBB_SCHEDULER_OBSERVER

#include "scheduler_common.h" // to include task.h
#include "tbb/task_scheduler_observer.h"
#include "tbb/spin_rw_mutex.h"
#include "tbb/aligned_space.h"

namespace tbb {
namespace internal {

class arena;
class observer_proxy;

class observer_list {
    friend class arena;

    // Mutex is wrapped with aligned_space to shut up warnings when its destructor
    // is called while threads are still using it.
    typedef aligned_space<spin_rw_mutex,1>  my_mutex_type;

    //! Pointer to the head of this list.
    observer_proxy* my_head;

    //! Pointer to the tail of this list.
    observer_proxy* my_tail;

    //! Mutex protecting this list.
    my_mutex_type my_mutex;

    //! Back-pointer to the arena this list belongs to.
    arena* my_arena;

    //! Decrement refcount of the proxy p if there are other outstanding references.
    /** In case of success sets p to NULL. Must be invoked from under the list lock. **/
    inline static void remove_ref_fast( observer_proxy*& p );

    //! Implements notify_entry_observers functionality.
    void do_notify_entry_observers( observer_proxy*& last, bool worker );

    //! Implements notify_exit_observers functionality.
    void do_notify_exit_observers( observer_proxy* last, bool worker );

public:
    observer_list () : my_head(NULL), my_tail(NULL) {}

    //! Removes and destroys all observer proxies from the list.
    /** Cannot be used concurrently with other methods. **/
    void clear ();

    //! Add observer proxy to the tail of the list.
    void insert ( observer_proxy* p );

    //! Remove observer proxy from the list.
    void remove ( observer_proxy* p );

    //! Decrement refcount of the proxy and destroy it if necessary.
    /** When refcount reaches zero removes the proxy from the list and destructs it. **/
    void remove_ref( observer_proxy* p );

    //! Type of the scoped lock for the reader-writer mutex associated with the list.
    typedef spin_rw_mutex::scoped_lock scoped_lock;

    //! Accessor to the reader-writer mutex associated with the list.
    spin_rw_mutex& mutex () { return my_mutex.begin()[0]; }

    bool empty () const { return my_head == NULL; }

    //! Call entry notifications on observers added after last was notified.
    /** Updates last to become the last notified observer proxy (in the global list)
        or leaves it to be NULL. The proxy has its refcount incremented. **/
    inline void notify_entry_observers( observer_proxy*& last, bool worker );

    //! Call exit notifications on last and observers added before it.
    inline void notify_exit_observers( observer_proxy* last, bool worker );

    //! Call on_scheduler_leaving callbacks to ask for permission for a worker thread to leave an arena
    bool ask_permission_to_leave();
}; // class observer_list

//! Wrapper for an observer object
/** To maintain shared lists of observers the scheduler first wraps each observer
    object into a proxy so that a list item remained valid even after the corresponding
    proxy object is destroyed by the user code. **/
class observer_proxy {
    friend class task_scheduler_observer_v3;
    friend class observer_list;
    //! Reference count used for garbage collection.
    /** 1 for reference from my task_scheduler_observer.
        1 for each task dispatcher's last observer pointer. 
        No accounting for neighbors in the shared list. */
    atomic<int> my_ref_count;
    //! Reference to the list this observer belongs to.
    observer_list* my_list;
    //! Pointer to next observer in the list specified by my_head.
    /** NULL for the last item in the list. **/
    observer_proxy* my_next;
    //! Pointer to the previous observer in the list specified by my_head.
    /** For the head of the list points to the last item. **/
    observer_proxy* my_prev;
    //! Associated observer
    task_scheduler_observer_v3* my_observer;
    //! Version
    char my_version;

    interface6::task_scheduler_observer* get_v6_observer();
    bool is_global(); //TODO: move them back inline when un-CPF'ing

    //! Constructs proxy for the given observer and adds it to the specified list.
    observer_proxy( task_scheduler_observer_v3& );

#if TBB_USE_ASSERT
    ~observer_proxy();
#endif /* TBB_USE_ASSERT */

    //! Shut up the warning
    observer_proxy& operator = ( const observer_proxy& );
}; // class observer_proxy

inline void observer_list::remove_ref_fast( observer_proxy*& p ) {
    if( p->my_observer ) {
        // 2 = 1 for observer and 1 for last
        __TBB_ASSERT( p->my_ref_count>=2, NULL );
        // Can decrement refcount quickly, as it cannot drop to zero while under the lock.
        --p->my_ref_count;
        p = NULL;
    } else {
        // Use slow form of refcount decrementing, after the lock is released.
    }
}

inline void observer_list::notify_entry_observers( observer_proxy*& last, bool worker ) {
    if ( last == my_tail )
        return;
    do_notify_entry_observers( last, worker );
}

inline void observer_list::notify_exit_observers( observer_proxy* last, bool worker ) {
    if ( !last )
        return;
    do_notify_exit_observers( last, worker );
}

extern observer_list the_global_observer_list;

} // namespace internal
} // namespace tbb

#endif /* __TBB_SCHEDULER_OBSERVER */

#endif /* _TBB_observer_proxy_H */
