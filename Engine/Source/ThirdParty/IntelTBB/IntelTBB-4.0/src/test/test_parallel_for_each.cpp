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

#if _MSC_VER && !defined(__INTEL_COMPILER)
#pragma warning(disable: 4180) // "qualifier applied to function type has no meaning; ignored"
#endif

#include "tbb/parallel_for_each.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/atomic.h"
#include "harness.h"
#include "harness_iterator.h"

// Some old compilers can't deduce template paremeter type for parallel_for_each
// if the function name is passed without explicit cast to function pointer.
typedef void (*TestFunctionType)(size_t);

tbb::atomic<size_t> sum;

// This function is called via parallel_for_each
void TestFunction (size_t value) {
    sum += (unsigned int)value;
}

const size_t NUMBER_OF_ELEMENTS = 1000;

// Tests tbb::parallel_for_each functionality
template <typename Iterator>
void RunPForEachTests()
{
    size_t test_vector[NUMBER_OF_ELEMENTS + 1];

    sum = 0;
    size_t test_sum = 0;

    for (size_t i =0; i < NUMBER_OF_ELEMENTS; i++) { 
        test_vector[i] = i;
        test_sum += i;
    }
    test_vector[NUMBER_OF_ELEMENTS] = 1000000; // parallel_for_each shouldn't touch this element

    Iterator begin(&test_vector[0]);
    Iterator end(&test_vector[NUMBER_OF_ELEMENTS]);

    tbb::parallel_for_each(begin, end, (TestFunctionType)TestFunction);
    ASSERT(sum == test_sum, "Not all items of test vector were processed by parallel_for_each");
    ASSERT(test_vector[NUMBER_OF_ELEMENTS] == 1000000, "parallel_for_each processed an extra element");
}

typedef void (*TestMutatorType)(size_t&);

void TestMutator(size_t& value) {
    ASSERT(value==0,NULL);
    ++sum;
    ++value;
}

//! Test that tbb::parallel_for_each works for mutable iterators.
template <typename Iterator>
void RunMutablePForEachTests() {
    size_t test_vector[NUMBER_OF_ELEMENTS];
    for( size_t i=0; i<NUMBER_OF_ELEMENTS; ++i )
        test_vector[i] = 0;
    sum = 0;
    tbb::parallel_for_each( Iterator(test_vector), Iterator(test_vector+NUMBER_OF_ELEMENTS), (TestMutatorType)TestMutator );
    ASSERT( sum==NUMBER_OF_ELEMENTS, "parallel_for_each called function wrong number of times" );
    for( size_t i=0; i<NUMBER_OF_ELEMENTS; ++i )
        ASSERT( test_vector[i]==1, "parallel_for_each did not process each element exactly once" );
}

#if __TBB_TASK_GROUP_CONTEXT
#define HARNESS_EH_SIMPLE_MODE 1
#include "tbb/tbb_exception.h"
#include "harness_eh.h"

#if TBB_USE_EXCEPTIONS
void test_function_with_exception(size_t) {
    ThrowTestException();
}

template <typename Iterator>
void TestExceptionsSupport()
{
    REMARK (__FUNCTION__);
    size_t test_vector[NUMBER_OF_ELEMENTS + 1];

    for (size_t i = 0; i < NUMBER_OF_ELEMENTS; i++) { 
        test_vector[i] = i;
    }

    Iterator begin(&test_vector[0]);
    Iterator end(&test_vector[NUMBER_OF_ELEMENTS]);

    TRY();
        tbb::parallel_for_each(begin, end, (TestFunctionType)test_function_with_exception);
    CATCH_AND_ASSERT();
}
#endif /* TBB_USE_EXCEPTIONS */

// Cancelation support test
void function_to_cancel(size_t ) {
    ++g_CurExecuted;
    CancellatorTask::WaitUntilReady();
}

template <typename Iterator>
class my_worker_pforeach_task : public tbb::task
{
    tbb::task_group_context &my_ctx;

    tbb::task* execute () {
        size_t test_vector[NUMBER_OF_ELEMENTS + 1];
        for (size_t i = 0; i < NUMBER_OF_ELEMENTS; i++) { 
            test_vector[i] = i;
        }
        Iterator begin(&test_vector[0]);
        Iterator end(&test_vector[NUMBER_OF_ELEMENTS]);

        tbb::parallel_for_each(begin, end, (TestFunctionType)function_to_cancel);
        
        return NULL;
    }
public:
    my_worker_pforeach_task ( tbb::task_group_context &ctx) : my_ctx(ctx) { }
};

template <typename Iterator>
void TestCancellation()
{
    REMARK (__FUNCTION__);
    ResetEhGlobals();
    RunCancellationTest<my_worker_pforeach_task<Iterator>, CancellatorTask>();
}
#endif /* __TBB_TASK_GROUP_CONTEXT */

#include "harness_cpu.h"

int TestMain () {
    if( MinThread<1 ) {
        REPORT("number of threads must be positive\n");
        exit(1);
    }
    for( int p=MinThread; p<=MaxThread; ++p ) {
        tbb::task_scheduler_init init( p );

        RunPForEachTests<Harness::RandomIterator<size_t> >();
        RunPForEachTests<Harness::ConstRandomIterator<size_t> >();
        RunPForEachTests<Harness::InputIterator<size_t> >();
        RunPForEachTests<Harness::ForwardIterator<size_t> >();

        RunMutablePForEachTests<Harness::RandomIterator<size_t> >();
        RunMutablePForEachTests<Harness::ForwardIterator<size_t> >();

#if TBB_USE_EXCEPTIONS && !__TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN
        TestExceptionsSupport<Harness::RandomIterator<size_t> >();
        TestExceptionsSupport<Harness::InputIterator<size_t> >();
        TestExceptionsSupport<Harness::ForwardIterator<size_t> >();
#endif /* TBB_USE_EXCEPTIONS && !__TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN */

#if __TBB_TASK_GROUP_CONTEXT
        if (p > 1) {
            TestCancellation<Harness::RandomIterator<size_t> >();
            TestCancellation<Harness::InputIterator<size_t> >();
            TestCancellation<Harness::ForwardIterator<size_t> >();
        }
#endif /* __TBB_TASK_GROUP_CONTEXT */

        // Test that all workers sleep when no work
        TestCPUUserTime(p);
    }
#if __TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN
    REPORT("Known issue: exception handling tests are skipped.\n");
#endif
    return Harness::Done;
}
