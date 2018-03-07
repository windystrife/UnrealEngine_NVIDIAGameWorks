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

#ifndef __TBB__aggregator_impl_H
#define __TBB__aggregator_impl_H

#include "../atomic.h"
#include "../tbb_profiling.h"

namespace tbb {
namespace interface6 {
namespace internal {

using namespace tbb::internal;

//! aggregated_operation base class
template <typename Derived>
class aggregated_operation {
 public:
    uintptr_t status;
    Derived *next;
    aggregated_operation() : status(0), next(NULL) {}
};

//! Aggregator base class
/** An aggregator for collecting operations coming from multiple sources and executing
    them serially on a single thread.  operation_type must be derived from
    aggregated_operation. The parameter handler_type is a functor that will be passed the
    list of operations and is expected to handle each operation appropriately, setting the
    status of each operation to non-zero.*/
template < typename handler_type, typename operation_type >
class aggregator {
 public:
    aggregator() : handler_busy(false) { pending_operations = NULL; }
    explicit aggregator(handler_type h) : handler_busy(false), handle_operations(h) {
        pending_operations = NULL;
    }

    void initialize_handler(handler_type h) { handle_operations = h; }

    //! Place operation in list
    /** Place operation in list and either handle list or wait for operation to
        complete.  */
    void execute(operation_type *op) {
        operation_type *res;

        // ITT note: &(op->status) tag is used to cover accesses to this op node. This
        // thread has created the operation, and now releases it so that the handler
        // thread may handle the associated operation w/o triggering a race condition;
        // thus this tag will be acquired just before the operation is handled in the
        // handle_operations functor.
        call_itt_notify(releasing, &(op->status));
        // insert the operation in the queue
        do {
            // ITT may flag the following line as a race; it is a false positive:
            // This is an atomic read; we don't provide itt_hide_load_word for atomics
            op->next = res = pending_operations; // NOT A RACE
        } while (pending_operations.compare_and_swap(op, res) != res);
        if (!res) { // first in the list; handle the operations
            // ITT note: &pending_operations tag covers access to the handler_busy flag,
            // which this waiting handler thread will try to set before entering
            // handle_operations.
            call_itt_notify(acquired, &pending_operations);
            start_handle_operations();
            __TBB_ASSERT(op->status, NULL);
        }
        else { // not first; wait for op to be ready
            call_itt_notify(prepare, &(op->status));
            spin_wait_while_eq(op->status, uintptr_t(0));
            itt_load_word_with_acquire(op->status);
        }
    }

 private:
    //! An atomically updated list (aka mailbox) of pending operations
    atomic<operation_type *> pending_operations;
    //! Controls thread access to handle_operations
    uintptr_t handler_busy;
    handler_type handle_operations;

    //! Trigger the handling of operations when the handler is free
    void start_handle_operations() {
        operation_type *op_list;

        // ITT note: &handler_busy tag covers access to pending_operations as it is passed
        // between active and waiting handlers.  Below, the waiting handler waits until
        // the active handler releases, and the waiting handler acquires &handler_busy as
        // it becomes the active_handler. The release point is at the end of this
        // function, when all operations in pending_operations have been handled by the
        // owner of this aggregator.
        call_itt_notify(prepare, &handler_busy);
        // get the handler_busy:
        // only one thread can possibly spin here at a time
        spin_wait_until_eq(handler_busy, uintptr_t(0));
        call_itt_notify(acquired, &handler_busy);
        // acquire fence not necessary here due to causality rule and surrounding atomics
        __TBB_store_with_release(handler_busy, uintptr_t(1));

        // ITT note: &pending_operations tag covers access to the handler_busy flag
        // itself. Capturing the state of the pending_operations signifies that
        // handler_busy has been set and a new active handler will now process that list's
        // operations.
        call_itt_notify(releasing, &pending_operations);
        // grab pending_operations
        op_list = pending_operations.fetch_and_store(NULL);

        // handle all the operations
        handle_operations(op_list);

        // release the handler
        itt_store_word_with_release(handler_busy, uintptr_t(0));
    }
};

// the most-compatible friend declaration (vs, gcc, icc) is
//    template<class U, class V> friend class aggregating_functor;
template<typename aggregating_class, typename operation_list>
class aggregating_functor {
    aggregating_class *fi;
public:
    aggregating_functor() {}
    aggregating_functor(aggregating_class *fi_) : fi(fi_) {}
    void operator()(operation_list* op_list) { fi->handle_operations(op_list); }
};

} // namespace internal
} // namespace interface6

namespace internal {
    using interface6::internal::aggregated_operation;
    using interface6::internal::aggregator;
    using interface6::internal::aggregating_functor;
} // namespace internal

} // namespace tbb

#endif  // __TBB__aggregator_impl_H
