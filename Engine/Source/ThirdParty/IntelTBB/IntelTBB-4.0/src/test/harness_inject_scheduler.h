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

// Used in tests that work with TBB scheduler but do not link to the TBB library.
// In other words it embeds the TBB library core into the test executable.

#ifndef harness_inject_scheduler_H
#define harness_inject_scheduler_H

// Suppress usage of #pragma comment
#define __TBB_NO_IMPLICIT_LINKAGE 1

// Enable preview features if any 
#define __TBB_BUILD 1

#undef DO_ITT_NOTIFY

#define __TBB_SOURCE_DIRECTLY_INCLUDED 1
#include "../tbb/tbb_main.cpp"
#include "../tbb/dynamic_link.cpp"
#include "../tbb/tbb_misc_ex.cpp"

// Tasking subsystem files
#include "../tbb/governor.cpp"
#include "../tbb/market.cpp"
#include "../tbb/arena.cpp"
#include "../tbb/scheduler.cpp"
#include "../tbb/observer_proxy.cpp"
#include "../tbb/task.cpp"
#include "../tbb/task_group_context.cpp"

// Other dependencies
#include "../tbb/cache_aligned_allocator.cpp"
#include "../tbb/tbb_thread.cpp"
#include "../tbb/mutex.cpp"
#include "../tbb/spin_rw_mutex.cpp"
#include "../tbb/spin_mutex.cpp"
#include "../tbb/private_server.cpp"
#if _WIN32||_WIN64
#include "../tbb/semaphore.cpp"
#endif
#include "../rml/client/rml_tbb.cpp"

#endif /* harness_inject_scheduler_H */
