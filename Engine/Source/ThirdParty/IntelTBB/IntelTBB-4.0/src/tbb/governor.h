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

#ifndef _TBB_governor_H
#define _TBB_governor_H

#include "tbb/task_scheduler_init.h"
#include "../rml/include/rml_tbb.h"

#include "tbb_misc.h" // for AvailableHwConcurrency and ThreadStackSize
#include "tls.h"

#if __TBB_SURVIVE_THREAD_SWITCH
#include "cilk-tbb-interop.h"
#endif /* __TBB_SURVIVE_THREAD_SWITCH */

namespace tbb {
namespace internal {

class market;
class generic_scheduler;
class __TBB_InitOnce;

//------------------------------------------------------------------------
// Class governor
//------------------------------------------------------------------------

//! The class handles access to the single instance of market, and to TLS to keep scheduler instances.
/** It also supports automatic on-demand initialization of the TBB scheduler.
    The class contains only static data members and methods.*/
class governor {
    friend class __TBB_InitOnce;
    friend class market;

    //! TLS for scheduler instances associated with individual threads
    static basic_tls<generic_scheduler*> theTLS;

    //! Caches the maximal level of paralellism supported by the hardware
    static unsigned DefaultNumberOfThreads;

    static rml::tbb_factory theRMLServerFactory;

    static bool UsePrivateRML;

    //! Create key for thread-local storage and initialize RML.
    static void acquire_resources ();

    //! Destroy the thread-local storage key and deinitialize RML.
    static void release_resources ();

    static rml::tbb_server* create_rml_server ( rml::tbb_client& );

    //! The internal routine to undo automatic initialization.
    /** The signature is written with void* so that the routine
        can be the destructor argument to pthread_key_create. */
    static void auto_terminate(void* scheduler);

public:
    static unsigned default_num_threads () {
        // No memory fence required, because at worst each invoking thread calls AvailableHwConcurrency once.
        return DefaultNumberOfThreads ? DefaultNumberOfThreads :
                                        DefaultNumberOfThreads = AvailableHwConcurrency();
    }
    //! Processes scheduler initialization request (possibly nested) in a master thread
    /** If necessary creates new instance of arena and/or local scheduler.
        The auto_init argument specifies if the call is due to automatic initialization. **/
    static generic_scheduler* init_scheduler( unsigned num_threads, stack_size_type stack_size, bool auto_init = false );

    //! Processes scheduler termination request (possibly nested) in a master thread
    static void terminate_scheduler( generic_scheduler* s );

    //! Returns number of worker threads in the currently active arena.
    inline static unsigned max_number_of_workers ();

    //! Register TBB scheduler instance in thread-local storage.
    static void sign_on(generic_scheduler* s);

    //! Unregister TBB scheduler instance from thread-local storage.
    static void sign_off(generic_scheduler* s);

    //! Used to check validity of the local scheduler TLS contents.
    static bool is_set ( generic_scheduler* s ) { return theTLS.get() == s; }

    //! Temporarily set TLS slot to the given scheduler
    static void assume_scheduler( generic_scheduler* s ) { theTLS.set( s ); }

    //! Obtain the thread-local instance of the TBB scheduler.
    /** If the scheduler has not been initialized yet, initialization is done automatically.
        Note that auto-initialized scheduler instance is destroyed only when its thread terminates. **/
    static generic_scheduler* local_scheduler () {
        generic_scheduler* s = theTLS.get();
        return s ? s : init_scheduler( (unsigned)task_scheduler_init::automatic, 0, true );
    }

    static generic_scheduler* local_scheduler_if_initialized () {
        return theTLS.get();
    }

    //! Undo automatic initialization if necessary; call when a thread exits.
    static void terminate_auto_initialized_scheduler() {
        auto_terminate( theTLS.get() );
    }

    static void print_version_info ();

    static void initialize_rml_factory ();

#if __TBB_SURVIVE_THREAD_SWITCH
    static __cilk_tbb_retcode stack_op_handler( __cilk_tbb_stack_op op, void* );
#endif /* __TBB_SURVIVE_THREAD_SWITCH */
}; // class governor

} // namespace internal
} // namespace tbb

#include "scheduler.h"

inline unsigned tbb::internal::governor::max_number_of_workers () {
    return local_scheduler()->number_of_workers_in_my_arena();
}

#endif /* _TBB_governor_H */
