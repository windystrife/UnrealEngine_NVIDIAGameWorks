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

#if HARNESS_USE_PROXY

// The test includes injects scheduler directly, so skip it when proxy tested.

#undef HARNESS_USE_PROXY
#include "harness.h"

int TestMain () {
    return Harness::Skipped;
}

#else // HARNESS_USE_PROXY

// Test correctness of forceful TBB initialization before any dynamic initialization
// of static objects inside the library took place.
namespace tbb { 
namespace internal {
    // Forward declaration of the TBB general initialization routine from task.cpp
    void DoOneTimeInitializations();
}}

struct StaticInitializationChecker {
    StaticInitializationChecker () { tbb::internal::DoOneTimeInitializations(); }
} theChecker;

//------------------------------------------------------------------------
// Test that important assertions in class task fail as expected.
//------------------------------------------------------------------------

#include "harness_inject_scheduler.h"

#define HARNESS_NO_PARSE_COMMAND_LINE 1
#include "harness.h"
#include "harness_bad_expr.h"

#if TRY_BAD_EXPR_ENABLED
//! Task that will be abused.
tbb::task* volatile AbusedTask;

//! Number of times that AbuseOneTask
int AbuseOneTaskRan;

//! Body used to create task in thread 0 and abuse it in thread 1.
struct AbuseOneTask {
    void operator()( int ) const {
        tbb::task_scheduler_init init;
        // Thread 1 attempts to incorrectly use the task created by thread 0.
        tbb::task_list list;
        // spawn_root_and_wait over empty list should vacuously succeed.
        tbb::task::spawn_root_and_wait(list);

        // Check that spawn_root_and_wait fails on non-empty list. 
        list.push_back(*AbusedTask);

        // Try abusing recycle_as_continuation
        TRY_BAD_EXPR(AbusedTask->recycle_as_continuation(), "execute" );
        TRY_BAD_EXPR(AbusedTask->recycle_as_safe_continuation(), "execute" );
        TRY_BAD_EXPR(AbusedTask->recycle_to_reexecute(), "execute" );
        ++AbuseOneTaskRan;
    }
};

//! Test various __TBB_ASSERT assertions related to class tbb::task.
void TestTaskAssertions() {
    // Catch assertion failures
    tbb::set_assertion_handler( AssertionFailureHandler );
    tbb::task_scheduler_init init;
    // Create task to be abused
    AbusedTask = new( tbb::task::allocate_root() ) tbb::empty_task;
    NativeParallelFor( 1, AbuseOneTask() );
    ASSERT( AbuseOneTaskRan==1, NULL );
    tbb::task::destroy(*AbusedTask);
    // Restore normal assertion handling
    tbb::set_assertion_handler( NULL );
}

int TestMain () {
    TestTaskAssertions();
    return Harness::Done;
}

#else /* !TRY_BAD_EXPR_ENABLED */

int TestMain () {
    return Harness::Skipped;
}

#endif /* !TRY_BAD_EXPR_ENABLED */

#endif // HARNESS_USE_PROXY
