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

#include "tbb/task.h"
#include "harness.h"
#include "tbb/task_scheduler_init.h"

using tbb::task;

#if __TBB_ipf
    const unsigned StackSize = 1024*1024*6;
#else /*  */
    const unsigned StackSize = 1024*1024*3;
#endif

// GCC and ICC on Linux store TLS data in the stack space. This test makes sure
// that the stealing limiting heuristic used by the task scheduler does not
// switch off stealing when a large amount of TLS data is reserved.
#if _MSC_VER
__declspec(thread) 
#elif __linux__ || ((__MINGW32__ || __MINGW64__) && __TBB_GCC_VERSION >= 40500)
__thread
#endif
    char map2[1024*1024*2];

class TestTask : public task {
public:
    static volatile int completed;
    task* execute() {
        completed = 1;
        return NULL;
    };
};

volatile int TestTask::completed = 0;

void TestStealingIsEnabled () {
    tbb::task_scheduler_init init(2, StackSize);
    task &r = *new( task::allocate_root() ) tbb::empty_task;
    task &t = *new( r.allocate_child() ) TestTask;
    r.set_ref_count(2);
    r.spawn(t);
    int count = 0;
    while ( !TestTask::completed && ++count < 6 )
        Harness::Sleep(1000);
    ASSERT( TestTask::completed, "Stealing is disabled or the machine is heavily oversubscribed" );
    r.wait_for_all();
    task::destroy(r);
}

int TestMain () {
    if ( tbb::task_scheduler_init::default_num_threads() == 1 ) {
        REPORT( "Known issue: Test requires at least 2 hardware threads.\n" );
        return Harness::Skipped;
    }
    TestStealingIsEnabled();
    return Harness::Done;
}
