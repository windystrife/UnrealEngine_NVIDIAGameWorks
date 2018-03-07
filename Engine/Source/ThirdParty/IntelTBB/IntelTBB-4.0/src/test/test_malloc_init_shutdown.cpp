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

#include "tbb/scalable_allocator.h"
#include "tbb/atomic.h"
#include "tbb/aligned_space.h"

#include "harness.h"
#include "harness_barrier.h"
#include "harness_tbb_independence.h"

tbb::atomic<int> FinishedTasks;
const int MaxTasks = 16;

/*--------------------------------------------------------------------*/
// The regression test against a bug triggered when malloc initialization
// and thread shutdown were called simultaneously, in which case
// Windows dynamic loader lock and allocator initialization/termination lock
// were taken in different order.

class TestFunc1 {
    Harness::SpinBarrier* my_barr;
public:
    TestFunc1 (Harness::SpinBarrier& barr) : my_barr(&barr) {}
    void operator() (bool do_malloc) const {
        my_barr->wait();
        if (do_malloc) scalable_malloc(10);
        ++FinishedTasks;
    }
};

typedef NativeParallelForTask<bool,TestFunc1> TestTask1;

void Test1 () {
    int NTasks = min(MaxTasks, max(2, MaxThread));
    Harness::SpinBarrier barr(NTasks);
    TestFunc1 tf(barr);
    FinishedTasks = 0;
    tbb::aligned_space<TestTask1,MaxTasks> tasks;

    for(int i=0; i<NTasks; ++i) {
        TestTask1* t = tasks.begin()+i;
        new(t) TestTask1(i%2==0, tf);
        t->start();
    }

    Harness::Sleep(1000); // wait a second :)
    ASSERT( FinishedTasks==NTasks, "Some threads appear to deadlock" );

    for(int i=0; i<NTasks; ++i) {
        TestTask1* t = tasks.begin()+i;
        t->wait_to_finish();
        t->~TestTask1();
    }
}

/*--------------------------------------------------------------------*/
// The regression test against a bug when cross-thread deallocation
// caused livelock at thread shutdown.

void* gPtr = NULL;

class TestFunc2a {
    Harness::SpinBarrier* my_barr;
public:
    TestFunc2a (Harness::SpinBarrier& barr) : my_barr(&barr) {}
    void operator() (int) const {
        gPtr = scalable_malloc(8);
        my_barr->wait();
        ++FinishedTasks;
    }
};

typedef NativeParallelForTask<int,TestFunc2a> TestTask2a;

class TestFunc2b: NoAssign {
    Harness::SpinBarrier* my_barr;
    TestTask2a& my_ward;
public:
    TestFunc2b (Harness::SpinBarrier& barr, TestTask2a& t) : my_barr(&barr), my_ward(t) {}
    void operator() (int) const {
        tbb::internal::spin_wait_while_eq(gPtr, (void*)NULL);
        scalable_free(gPtr);
        my_barr->wait();
        my_ward.wait_to_finish();
        ++FinishedTasks;
    }
};
void Test2() {
    Harness::SpinBarrier barr(2);
    TestFunc2a func2a(barr);
    TestTask2a t2a(0, func2a);
    TestFunc2b func2b(barr, t2a);
    NativeParallelForTask<int,TestFunc2b> t2b(1, func2b);
    FinishedTasks = 0;
    t2a.start(); t2b.start();
    Harness::Sleep(1000); // wait a second :)
    ASSERT( FinishedTasks==2, "Threads appear to deadlock" );
    t2b.wait_to_finish(); // t2a is monitored by t2b
}

#if _WIN32||_WIN64

void TestKeyDtor() {}

#else

void *currSmall, *prevSmall, *currLarge, *prevLarge;

extern "C" void threadDtor(void*) {
    // First, release memory that was allocated before;
    // it will not re-initialize the thread-local data if already deleted
    prevSmall = currSmall;
    scalable_free(currSmall);
    prevLarge = currLarge;
    scalable_free(currLarge);
    // Then, allocate more memory.
    // It will re-initialize the allocator data in the thread.
    scalable_free(scalable_malloc(8));
}

struct TestThread: NoAssign {
    TestThread(int ) {}

    void operator()( int /*id*/ ) const {
        pthread_key_t key;

        currSmall = scalable_malloc(8);
        ASSERT(!prevSmall || currSmall==prevSmall, "Possible memory leak");
        currLarge = scalable_malloc(32*1024);
        ASSERT(!prevLarge || currLarge==prevLarge, "Possible memory leak");
        pthread_key_create( &key, &threadDtor );
        pthread_setspecific(key, (const void*)42);
    }
};

// test releasing memory from pthread key destructor
void TestKeyDtor() {
    for (int i=0; i<4; i++)
        NativeParallelFor( 1, TestThread(1) );
}

#endif // _WIN32||_WIN64

int TestMain () {
    Test1(); // requires malloc initialization so should be first
    Test2();
    TestKeyDtor();
    return Harness::Done;
}
