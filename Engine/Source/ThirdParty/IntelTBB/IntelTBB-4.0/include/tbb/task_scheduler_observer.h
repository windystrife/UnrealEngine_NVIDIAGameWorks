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

#ifndef __TBB_task_scheduler_observer_H
#define __TBB_task_scheduler_observer_H

#include "atomic.h"
#if __TBB_TASK_ARENA
#include "task_arena.h"
#endif //__TBB_TASK_ARENA

#if __TBB_SCHEDULER_OBSERVER

namespace tbb {
namespace interface6 {
class task_scheduler_observer;
}
namespace internal {

class observer_proxy;
class observer_list;

class task_scheduler_observer_v3 {
    friend class observer_proxy;
    friend class observer_list;
    friend class interface6::task_scheduler_observer;

    //! Pointer to the proxy holding this observer.
    /** Observers are proxied by the scheduler to maintain persistent lists of them. **/
    observer_proxy* my_proxy;

    //! Counter preventing the observer from being destroyed while in use by the scheduler.
    /** Valid only when observation is on. **/
    atomic<intptr_t> my_busy_count;

public:
    //! Enable or disable observation
    /** For local observers the method can be used only when the current thread
        has the task scheduler initialized or is attached to an arena.

        Repeated calls with the same state are no-ops. **/
    void __TBB_EXPORTED_METHOD observe( bool state=true );

    //! Returns true if observation is enabled, false otherwise.
    bool is_observing() const {return my_proxy!=NULL;}

    //! Construct observer with observation disabled.
    task_scheduler_observer_v3() : my_proxy(NULL) { my_busy_count.store<relaxed>(0); }

    //! Entry notification
    /** Invoked from inside observe(true) call and whenever a worker enters the arena 
        this observer is associated with. If a thread is already in the arena when
        the observer is activated, the entry notification is called before it
        executes the first stolen task.

        Obsolete semantics. For global observers it is called by a thread before
        the first steal since observation became enabled. **/
    virtual void on_scheduler_entry( bool /*is_worker*/ ) {} 

    //! Exit notification
    /** Invoked from inside observe(false) call and whenever a worker leaves the
        arena this observer is associated with.

        Obsolete semantics. For global observers it is called by a thread before
        the first steal since observation became enabled. **/
    virtual void on_scheduler_exit( bool /*is_worker*/ ) {}

    //! Destructor automatically switches observation off if it is enabled.
    virtual ~task_scheduler_observer_v3() { if(my_proxy) observe(false);}
};

} // namespace internal

#if TBB_PREVIEW_LOCAL_OBSERVER
namespace interface6 {
class task_scheduler_observer : public internal::task_scheduler_observer_v3 {
    friend class internal::task_scheduler_observer_v3;
    friend class internal::observer_proxy;
    friend class internal::observer_list;

    /** Negative numbers with the largest absolute value to minimize probability
        of coincidence in case of a bug in busy count usage. **/
    // TODO: take more high bits for version number
    static const intptr_t v6_trait = (intptr_t)((~(uintptr_t)0 >> 1) + 1);

    //! contains task_arena pointer or tag indicating local or global semantics of the observer
    intptr_t my_context_tag;
    enum { global_tag = 0, implicit_tag = 1 };

public:
    //! Construct local or global observer in inactive state (observation disabled).
    /** For a local observer entry/exit notifications are invoked whenever a worker
        thread joins/leaves the arena of the observer's owner thread. If a thread is
        already in the arena when the observer is activated, the entry notification is
        called before it executes the first stolen task. **/
    /** TODO: Obsolete.
        Global observer semantics is obsolete as it violates master thread isolation
        guarantees and is not composable. Thus the current default behavior of the
        constructor is obsolete too and will be changed in one of the future versions
        of the library. **/
    task_scheduler_observer( bool local = false ) {
        my_busy_count.store<relaxed>(v6_trait);
        my_context_tag = local? implicit_tag : global_tag;
    }

#if __TBB_TASK_ARENA
    //! Construct local observer for a given arena in inactive state (observation disabled).
    /** entry/exit notifications are invoked whenever a thread joins/leaves arena.
        If a thread is already in the arena when the observer is activated, the entry notification
        is called before it executes the first stolen task. **/
    task_scheduler_observer( task_arena & a) {
        my_busy_count.store<relaxed>(v6_trait);
        my_context_tag = (intptr_t)&a;
    }
#endif //__TBB_TASK_ARENA

    //! The callback can be invoked in a worker thread before it leaves an arena.
    /** If it returns false, the thread remains in the arena. Will not be called for masters
        or if the worker leaves arena due to rebalancing or priority changes, etc.
        NOTE: The preview library must be linked for this method to take effect **/
    virtual bool on_scheduler_leaving() { return true; }

    //! Destructor additionally protects concurrent on_scheduler_leaving notification
    // It is recommended to disable observation before destructor of a derived class starts,
    // otherwise it can lead to concurrent notification callback on partly destroyed object
    virtual ~task_scheduler_observer() { if(my_proxy) observe(false);}
};

} //namespace interface6
using interface6::task_scheduler_observer;
#else /*TBB_PREVIEW_LOCAL_OBSERVER*/
typedef tbb::internal::task_scheduler_observer_v3 task_scheduler_observer;
#endif /*TBB_PREVIEW_LOCAL_OBSERVER*/

} // namespace tbb

#endif /* __TBB_SCHEDULER_OBSERVER */

#endif /* __TBB_task_scheduler_observer_H */
