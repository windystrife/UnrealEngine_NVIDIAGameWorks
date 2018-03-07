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

// test reader_writer_lock
#include "tbb/reader_writer_lock.h"
#include "tbb/atomic.h"
#include "tbb/tbb_exception.h"
#include "harness_assert.h"
#include "harness.h"

tbb::reader_writer_lock the_mutex;
const int MAX_WORK = 10000;

tbb::atomic<size_t> active_readers, active_writers;
tbb::atomic<bool> sim_readers;


int BusyWork(int percentOfMaxWork) {
  int iters = 0;
  for (int i=0; i<MAX_WORK*((double)percentOfMaxWork/100.0); ++i) {
      iters++;
  }
  return iters;
}

struct StressRWLBody : NoAssign {
    const int nThread;
    const int percentMax;

    StressRWLBody(int nThread_, int percentMax_) : nThread(nThread_), percentMax(percentMax_) {}

    void operator()(const int /* threadID */ ) const {
        int nIters = 100;
        int r_result=0, w_result=0;
        for(int i=0; i<nIters; ++i) {
            // test unscoped blocking write lock
            the_mutex.lock();
            w_result += BusyWork(percentMax);
#if TBB_USE_EXCEPTIONS && !__TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN
            // test exception for recursive write lock
            bool was_caught = false;
            try {
                the_mutex.lock();
            }
            catch(tbb::improper_lock& ex) {
                REMARK("improper_lock: %s\n", ex.what());
                was_caught = true;
            }
            catch(...) {
                REPORT("Wrong exception caught during recursive lock attempt.");
            }
            ASSERT(was_caught, "Recursive lock attempt exception not caught properly.");
            // test exception for recursive read lock
            was_caught = false;
            try {
                the_mutex.lock_read();
            }
            catch(tbb::improper_lock& ex) {
                REMARK("improper_lock: %s\n", ex.what());
                was_caught = true;
            }
            catch(...) {
                REPORT("Wrong exception caught during recursive lock attempt.");
            }
            ASSERT(was_caught, "Recursive lock attempt exception not caught properly.");
#endif /* TBB_USE_EXCEPTIONS && !__TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN */
            the_mutex.unlock();
            // test unscoped non-blocking write lock
            if (the_mutex.try_lock()) {
                w_result += BusyWork(percentMax);
                the_mutex.unlock();
            }
            // test unscoped blocking read lock
            the_mutex.lock_read();
            r_result += BusyWork(percentMax);
            the_mutex.unlock();
            // test unscoped non-blocking read lock
            if(the_mutex.try_lock_read()) {
                r_result += BusyWork(percentMax);
                the_mutex.unlock();
            }
            { // test scoped blocking write lock
                tbb::reader_writer_lock::scoped_lock my_lock(the_mutex);
                w_result += BusyWork(percentMax);
            }
            { // test scoped blocking read lock
                tbb::reader_writer_lock::scoped_lock_read my_lock(the_mutex);
                r_result += BusyWork(percentMax);
            }
        }
        REMARK(" R%d/W%d", r_result, w_result); // reader/writer iterations of busy work completed
    }
};

struct CorrectRWLScopedBody : NoAssign {
    const int nThread;

    CorrectRWLScopedBody(int nThread_) : nThread(nThread_) {}

    void operator()(const int /* threadID */ ) const {
        for (int i=0; i<50; i++) {
            const bool is_reader = i%5==0; // 1 writer for every 4 readers

            if (is_reader) {
                tbb::reader_writer_lock::scoped_lock_read my_lock(the_mutex);
                active_readers++;
                if (active_readers > 1) sim_readers = true;
                ASSERT(active_writers==0, "Active writers in read-locked region.");
                Harness::Sleep(10);
                active_readers--;
            }
            else { // is writer
                tbb::reader_writer_lock::scoped_lock my_lock(the_mutex);
                active_writers++;
                ASSERT(active_readers==0, "Active readers in write-locked region.");
                ASSERT(active_writers<=1, "More than one active writer in write-locked region.");
                Harness::Sleep(10);
                active_writers--;
            }
        }
    }
};

struct CorrectRWLBody : NoAssign {
    const int nThread;

    CorrectRWLBody(int nThread_) : nThread(nThread_) {}

    void operator()(const int /* threadID */ ) const {
        for (int i=0; i<50; i++) {
            const bool is_reader = i%5==0; // 1 writer for every 4 readers

            if (is_reader) {
                the_mutex.lock_read();
                active_readers++;
                if (active_readers > 1) sim_readers = true;
                ASSERT(active_writers==0, "Active writers in read-locked region.");
            }
            else { // is writer
                the_mutex.lock();
                active_writers++;
                ASSERT(active_readers==0, "Active readers in write-locked region.");
                ASSERT(active_writers<=1, "More than one active writer in write-locked region.");
            }
            Harness::Sleep(10);
            if (is_reader) {
                active_readers--;
            }
            else { // is writer
                active_writers--;
            }
            the_mutex.unlock();
        }
    }
};

void TestReaderWriterLockOnNThreads(int nThreads) {
    // Stress-test all interfaces
    for (int pc=0; pc<=100; pc+=20) {
        REMARK("Testing with %d threads, percent of MAX_WORK=%d...", nThreads, pc);
        StressRWLBody myStressBody(nThreads, pc);
        NativeParallelFor(nThreads, myStressBody);
        REMARK(" OK.\n");
    }

    REMARK("Testing with %d threads, direct/unscoped locking mode...", nThreads); // TODO: choose direct or unscoped?
    CorrectRWLBody myCorrectBody(nThreads);
    active_writers = active_readers = 0;
    sim_readers = false;
    NativeParallelFor(nThreads, myCorrectBody);
    ASSERT(sim_readers || nThreads<2, "There were no simultaneous readers.");
    REMARK(" OK.\n");

    REMARK("Testing with %d threads, scoped locking mode...", nThreads);
    CorrectRWLScopedBody myCorrectScopedBody(nThreads);
    active_writers = active_readers = 0;
    sim_readers = false;
    NativeParallelFor(nThreads, myCorrectScopedBody);
    ASSERT(sim_readers || nThreads<2, "There were no simultaneous readers.");
    REMARK(" OK.\n");
}

void TestReaderWriterLock() {
    for(int p = MinThread; p <= MaxThread; p++) {
        TestReaderWriterLockOnNThreads(p);
    }
}


int TestMain() {
    if(MinThread <= 0) MinThread = 1;
    if(MaxThread > 0) {
        TestReaderWriterLock();
    }
    return Harness::Done;
}
