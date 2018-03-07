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

#ifndef _TBB_tbb_main_H
#define _TBB_tbb_main_H

#include "tbb/atomic.h"

namespace tbb {

namespace internal {

void DoOneTimeInitializations ();

//------------------------------------------------------------------------
// __TBB_InitOnce
//------------------------------------------------------------------------

//! Class that supports TBB initialization. 
/** It handles acquisition and release of global resources (e.g. TLS) during startup and shutdown,
    as well as synchronization for DoOneTimeInitializations. */
class __TBB_InitOnce {
    friend void DoOneTimeInitializations();
    friend void ITT_DoUnsafeOneTimeInitialization ();

    static atomic<int> count;

    //! Platform specific code to acquire resources.
    static void acquire_resources();

    //! Platform specific code to release resources.
    static void release_resources();

    //! Specifies if the one-time initializations has been done.
    static bool InitializationDone;

    //! Global initialization lock
    /** Scenarios are possible when tools interop has to be initialized before the
        TBB itself. This imposes a requirement that the global initialization lock 
        has to support valid static initialization, and does not issue any tool
        notifications in any build mode. **/
    static __TBB_atomic_flag InitializationLock;

public:
    static void lock()   { __TBB_LockByte( InitializationLock ); }

    static void unlock() { __TBB_UnlockByte( InitializationLock, 0 ); }

    static bool initialization_done() { return __TBB_load_with_acquire(InitializationDone); }

    //! Add initial reference to resources. 
    /** We assume that dynamic loading of the library prevents any other threads 
        from entering the library until this constructor has finished running. **/
    __TBB_InitOnce() { add_ref(); }

    //! Remove the initial reference to resources.
    /** This is not necessarily the last reference if other threads are still running. **/
    ~__TBB_InitOnce() {
        remove_ref();
        // We assume that InitializationDone is not set after file-scope destructors
        // start running, and thus no race on InitializationDone is possible.
        if( initialization_done() ) {
            // Remove an extra reference that was added in DoOneTimeInitializations.
            remove_ref();  
        }
    } 
    //! Add reference to resources.  If first reference added, acquire the resources.
    static void add_ref();

    //! Remove reference to resources.  If last reference removed, release the resources.
    static void remove_ref();
}; // class __TBB_InitOnce


} // namespace internal

} // namespace tbb

#endif /* _TBB_tbb_main_H */
