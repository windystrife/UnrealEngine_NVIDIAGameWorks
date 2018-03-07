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

// Test for function template parallel_for.h

// Enable testing of serial subset.
#define TBB_PREVIEW_SERIAL_SUBSET 1

#if _MSC_VER
#pragma warning (push)
#if !defined(__INTEL_COMPILER)
    // Suppress pointless "unreachable code" warning.
    #pragma warning (disable: 4702)
#endif
#if defined(_Wp64)
    // Workaround for overzealous compiler warnings in /Wp64 mode
    #pragma warning (disable: 4267)
#endif

#define _SCL_SECURE_NO_WARNINGS
#endif //#if _MSC_VER

#include "harness_defs.h"
#include "tbb/parallel_for.h"
#include "tbb/atomic.h"
#include "harness_assert.h"
#include "harness.h"

static tbb::atomic<int> FooBodyCount;

//! A range object whose only public members are those required by the Range concept.
template<size_t Pad>
class FooRange {
    //! Start of range
    int start;

    //! Size of range
    int size;
    FooRange( int start_, int size_ ) : start(start_), size(size_) {
        zero_fill<char>(pad, Pad);
        pad[Pad-1] = 'x';
    }
    template<typename Flavor_, size_t Pad_> friend void Flog( int nthread );
    template<size_t Pad_> friend class FooBody;
    void operator&();

    char pad[Pad];
public:
    bool empty() const {return size==0;}
    bool is_divisible() const {return size>1;}
    FooRange( FooRange& original, tbb::split ) : size(original.size/2) {
        original.size -= size;
        start = original.start+original.size;
        ASSERT( original.pad[Pad-1]=='x', NULL );
        pad[Pad-1] = 'x';
    }
};

//! A range object whose only public members are those required by the parallel_for.h body concept.
template<size_t Pad>
class FooBody {
    static const int LIVE = 0x1234;
    tbb::atomic<int>* array;
    int state;
    friend class FooRange<Pad>;
    template<typename Flavor_, size_t Pad_> friend void Flog( int nthread );
    FooBody( tbb::atomic<int>* array_ ) : array(array_), state(LIVE) {}
public:
    ~FooBody() {
        --FooBodyCount;
        for( size_t i=0; i<sizeof(*this); ++i )
            reinterpret_cast<char*>(this)[i] = -1;
    }
    //! Copy constructor
    FooBody( const FooBody& other ) : array(other.array), state(other.state) {
        ++FooBodyCount;
        ASSERT( state==LIVE, NULL );
    }
    void operator()( FooRange<Pad>& r ) const {
        for( int k=0; k<r.size; ++k ) {
            const int i = array[r.start+k]++;
            ASSERT( i==0, NULL );
        }
    }
};

#include "tbb/tick_count.h"

static const int N = 500;
static tbb::atomic<int> Array[N];

struct serial_tag {};
struct parallel_tag {};

template <typename Flavor, typename Range, typename Body>
struct Invoker;

template <typename Range, typename Body>
struct Invoker<serial_tag, Range, Body> {
    void operator()( const Range& r, const Body& body, const tbb::simple_partitioner& p ) {
        tbb::serial:: parallel_for( r, body, p );
    }
    void operator()( const Range& r, const Body& body, const tbb::auto_partitioner& p ) {
        tbb::serial:: parallel_for( r, body, p );
    }
    void operator()( const Range& r, const Body& body, tbb::affinity_partitioner& p ) {
        tbb::serial:: parallel_for( r, body, p );
    }
    void operator()( const Range& r, const Body& body ) {
        tbb::serial:: parallel_for( r, body, tbb::auto_partitioner() );
    }
};

template <typename Range, typename Body>
struct Invoker<parallel_tag, Range, Body> {
    void operator()( const Range& r, const Body& body, const tbb::simple_partitioner& p ) {
        tbb:: parallel_for( r, body, p );
    }
    void operator()( const Range& r, const Body& body, const tbb::auto_partitioner& p ) {
        tbb:: parallel_for( r, body, p );
    }
    void operator()( const Range& r, const Body& body, tbb::affinity_partitioner& p ) {
        tbb:: parallel_for( r, body, p );
    }
    void operator()( const Range& r, const Body& body ) {
        tbb:: parallel_for( r, body, tbb::auto_partitioner() );
    }
};

template <typename Flavor, typename T, typename Body>
struct InvokerStep;

template <typename T, typename Body>
struct InvokerStep<serial_tag, T, Body> {
    void operator()( const T& first, const T& last, const Body& f ) {
        tbb::serial:: parallel_for( first, last, f );
    }
    void operator()( const T& first, const T& last, const T& step, const Body& f ) {
        tbb::serial:: parallel_for( first, last, step, f );
    }
};

template <typename T, typename Body>
struct InvokerStep<parallel_tag, T, Body> {
    void operator()( const T& first, const T& last, const Body& f ) {
        tbb:: parallel_for( first, last, f );
    }
    void operator()( const T& first, const T& last, const T& step, const Body& f ) {
        tbb:: parallel_for( first, last, step, f );
    }
};

template<typename Flavor, size_t Pad>
void Flog( int nthread ) {
    tbb::tick_count T0 = tbb::tick_count::now();
    for( int i=0; i<N; ++i ) {
        for ( int mode = 0; mode < 4; ++mode) {
            FooRange<Pad> r( 0, i );
            const FooRange<Pad> rc = r;
            FooBody<Pad> f( Array );
            const FooBody<Pad> fc = f;
            memset( Array, 0, sizeof(Array) );
            FooBodyCount = 1;
            Invoker< Flavor, FooRange<Pad>, FooBody<Pad> > invoke_for;
            switch (mode) {
                case 0:
                    invoke_for( rc, fc );
                break;
                case 1:
                    invoke_for( rc, fc, tbb::simple_partitioner() );
                break;
                case 2:
                    invoke_for( rc, fc, tbb::auto_partitioner() );
                break;
                case 3: {
                    static tbb::affinity_partitioner affinity;
                    invoke_for( rc, fc, affinity );
                }
                break;
            }
            for( int j=0; j<i; ++j )
                ASSERT( Array[j]==1, NULL );
            for( int j=i; j<N; ++j )
                ASSERT( Array[j]==0, NULL );
            ASSERT( FooBodyCount==1, NULL );
        }
    }
    tbb::tick_count T1 = tbb::tick_count::now();
    REMARK("time=%g\tnthread=%d\tpad=%d\n",(T1-T0).seconds(),nthread,int(Pad));
}

// Testing parallel_for with step support
const size_t PFOR_BUFFER_TEST_SIZE = 1024;
// test_buffer has some extra items beyond its right bound
const size_t PFOR_BUFFER_ACTUAL_SIZE = PFOR_BUFFER_TEST_SIZE + 1024;
size_t pfor_buffer[PFOR_BUFFER_ACTUAL_SIZE];

template<typename T>
class TestFunctor{
public:
    void operator ()(T index) const {
        pfor_buffer[index]++;
    }
};

#if !TBB_USE_EXCEPTIONS && _MSC_VER
    // Suppress "C++ exception handler used, but unwind semantics are not enabled" warning in STL headers
    #pragma warning (push)
    #pragma warning (disable: 4530)
#endif

#include <stdexcept> // std::invalid_argument

#if !TBB_USE_EXCEPTIONS && _MSC_VER
    #pragma warning (pop)
#endif

template <typename Flavor, typename T>
void TestParallelForWithStepSupport()
{
    const T pfor_buffer_test_size = static_cast<T>(PFOR_BUFFER_TEST_SIZE);
    const T pfor_buffer_actual_size = static_cast<T>(PFOR_BUFFER_ACTUAL_SIZE);
    // Testing parallel_for with different step values
    InvokerStep< Flavor, T, TestFunctor<T> > invoke_for;
    for (T begin = 0; begin < pfor_buffer_test_size - 1; begin += pfor_buffer_test_size / 10 + 1) {
        T step;
        for (step = 1; step < pfor_buffer_test_size; step++) {
            memset(pfor_buffer, 0, pfor_buffer_actual_size * sizeof(size_t));
            if (step == 1){
                invoke_for(begin, pfor_buffer_test_size, TestFunctor<T>());
            } else {
                invoke_for(begin, pfor_buffer_test_size, step, TestFunctor<T>());
            }
            // Verifying that parallel_for processed all items it should
            for (T i = begin; i < pfor_buffer_test_size; i = i + step) {
                ASSERT(pfor_buffer[i] == 1, "parallel_for didn't process all required elements");
                pfor_buffer[i] = 0;
            }
            // Verifying that no extra items were processed and right bound of array wasn't crossed
            for (T i = 0; i < pfor_buffer_actual_size; i++) {
                ASSERT(pfor_buffer[i] == 0, "parallel_for processed an extra element");
            }
        }
    }

    // Testing some corner cases
    tbb::parallel_for(static_cast<T>(2), static_cast<T>(1), static_cast<T>(1), TestFunctor<T>());
#if TBB_USE_EXCEPTIONS && !__TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN
    try{
        tbb::parallel_for(static_cast<T>(1), static_cast<T>(100), static_cast<T>(0), TestFunctor<T>());  // should cause std::invalid_argument
    }catch(std::invalid_argument){
        return;
    }
    catch ( ... ) {
        ASSERT ( __TBB_EXCEPTION_TYPE_INFO_BROKEN, "Unrecognized exception. std::invalid_argument is expected" );
    }
#endif /* TBB_USE_EXCEPTIONS && !__TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN */
}

#if __TBB_TASK_GROUP_CONTEXT
// Exception support test
#define HARNESS_EH_SIMPLE_MODE 1
#include "tbb/tbb_exception.h"
#include "harness_eh.h"

#if TBB_USE_EXCEPTIONS
class test_functor_with_exception {
public:
    void operator ()(size_t) const { ThrowTestException(); }
};

void TestExceptionsSupport() {
    REMARK (__FUNCTION__);
    { // Tests version with a step provided
        ResetEhGlobals();
        TRY();
            tbb::parallel_for((size_t)0, (size_t)PFOR_BUFFER_TEST_SIZE, (size_t)1, test_functor_with_exception());
        CATCH_AND_ASSERT();
    }
    { // Tests version without a step
        ResetEhGlobals();
        TRY();
            tbb::parallel_for((size_t)0, (size_t)PFOR_BUFFER_TEST_SIZE, test_functor_with_exception());
        CATCH_AND_ASSERT();
    }
}
#endif /* TBB_USE_EXCEPTIONS */

// Cancellation support test
class functor_to_cancel {
public:
    void operator()(size_t) const {
        ++g_CurExecuted;
        CancellatorTask::WaitUntilReady();
    }
};

size_t g_worker_task_step = 0;

class my_worker_pfor_step_task : public tbb::task
{
    tbb::task_group_context &my_ctx;

    tbb::task* execute () {
        if (g_worker_task_step == 0){
            tbb::parallel_for((size_t)0, (size_t)PFOR_BUFFER_TEST_SIZE, functor_to_cancel(), my_ctx);
        }else{
            tbb::parallel_for((size_t)0, (size_t)PFOR_BUFFER_TEST_SIZE, g_worker_task_step, functor_to_cancel(), my_ctx);
        }
        return NULL;
    }
public:
    my_worker_pfor_step_task ( tbb::task_group_context &context_) : my_ctx(context_) { }
};

void TestCancellation()
{
    // tests version without a step
    g_worker_task_step = 0;
    ResetEhGlobals();
    RunCancellationTest<my_worker_pfor_step_task, CancellatorTask>();

    // tests version with step
    g_worker_task_step = 1;
    ResetEhGlobals();
    RunCancellationTest<my_worker_pfor_step_task, CancellatorTask>();
}
#endif /* __TBB_TASK_GROUP_CONTEXT */

#include "harness_m128.h"

#if (HAVE_m128 || HAVE_m256) && !__TBB_SSE_STACK_ALIGNMENT_BROKEN
template<typename ClassWithVectorType>
struct SSE_Functor {
    ClassWithVectorType* Src, * Dst;
    SSE_Functor( ClassWithVectorType* src, ClassWithVectorType* dst ) : Src(src), Dst(dst) {}

    void operator()( tbb::blocked_range<int>& r ) const {
        for( int i=r.begin(); i!=r.end(); ++i )
            Dst[i] = Src[i];
    }
};

//! Test that parallel_for works with stack-allocated __m128
template<typename ClassWithVectorType>
void TestVectorTypes() {
    ClassWithVectorType Array1[N], Array2[N];
    for( int i=0; i<N; ++i ) {
        // VC8 does not properly align a temporary value; to work around, use explicit variable
        ClassWithVectorType foo(i);
        Array1[i] = foo;
    }
    tbb::parallel_for( tbb::blocked_range<int>(0,N), SSE_Functor<ClassWithVectorType>(Array1, Array2) );
    for( int i=0; i<N; ++i ) {
        ClassWithVectorType foo(i);
        ASSERT( Array2[i]==foo, NULL ) ;
    }
}
#endif /* HAVE_m128 || HAVE_m256 */

#include <vector>
#include <tbb/blocked_range.h>
#include <sstream>
struct TestSimplePartitionerStabilityFunctor:NoAssign{
  std::vector<int> & ranges;
  TestSimplePartitionerStabilityFunctor(std::vector<int> & theRanges):ranges(theRanges){}
  void operator()(tbb::blocked_range<size_t>& r)const{
      ranges.at(r.begin())=true;
  }
};
void TestSimplePartitionerStability(){
    const std::size_t repeat_count= 10;
    const std::size_t rangeToSplitSize=1000000;
    const std::size_t grainsizeStep=rangeToSplitSize/repeat_count;
    typedef TestSimplePartitionerStabilityFunctor FunctorType;

    for (std::size_t i=0 , grainsize=grainsizeStep; i<repeat_count;i++, grainsize+=grainsizeStep){
        std::vector<int> firstSeries(rangeToSplitSize,0);
        std::vector<int> secondSeries(rangeToSplitSize,0);

        tbb::parallel_for(tbb::blocked_range<size_t>(0,rangeToSplitSize,grainsize),FunctorType(firstSeries),tbb::simple_partitioner());
        tbb::parallel_for(tbb::blocked_range<size_t>(0,rangeToSplitSize,grainsize),FunctorType(secondSeries),tbb::simple_partitioner());
        std::stringstream str; str<<i;
        ASSERT(firstSeries==secondSeries,("splitting range with tbb::simple_partitioner must be reproducible; i=" +str.str()).c_str() );
    }
}
#include <cstdio>
#include "tbb/task_scheduler_init.h"
#include "harness_cpu.h"

int TestMain () {
    if( MinThread<1 ) {
        REPORT("number of threads must be positive\n");
        exit(1);
    }
    for( int p=MinThread; p<=MaxThread; ++p ) {
        if( p>0 ) {
            tbb::task_scheduler_init init( p );
            Flog<parallel_tag,1>(p);
            Flog<parallel_tag,10>(p);
            Flog<parallel_tag,100>(p);
            Flog<parallel_tag,1000>(p);
            Flog<parallel_tag,10000>(p);

            // Testing with different integer types
            TestParallelForWithStepSupport<parallel_tag,short>();
            TestParallelForWithStepSupport<parallel_tag,unsigned short>();
            TestParallelForWithStepSupport<parallel_tag,int>();
            TestParallelForWithStepSupport<parallel_tag,unsigned int>();
            TestParallelForWithStepSupport<parallel_tag,long>();
            TestParallelForWithStepSupport<parallel_tag,unsigned long>();
            TestParallelForWithStepSupport<parallel_tag,long long>();
            TestParallelForWithStepSupport<parallel_tag,unsigned long long>();
            TestParallelForWithStepSupport<parallel_tag,size_t>();

            // This is for testing serial implementation.
            if( p == MaxThread ) {
                Flog<serial_tag,1>(p);
                Flog<serial_tag,10>(p);
                Flog<serial_tag,100>(p);
                TestParallelForWithStepSupport<serial_tag,short>();
                TestParallelForWithStepSupport<serial_tag,unsigned short>();
                TestParallelForWithStepSupport<serial_tag,int>();
                TestParallelForWithStepSupport<serial_tag,unsigned int>();
                TestParallelForWithStepSupport<serial_tag,long>();
                TestParallelForWithStepSupport<serial_tag,unsigned long>();
                TestParallelForWithStepSupport<serial_tag,long long>();
                TestParallelForWithStepSupport<serial_tag,unsigned long long>();
                TestParallelForWithStepSupport<serial_tag,size_t>();
            }

#if TBB_USE_EXCEPTIONS && !__TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN
            TestExceptionsSupport();
#endif /* TBB_USE_EXCEPTIONS && !__TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN */
#if __TBB_TASK_GROUP_CONTEXT
            if ( p > 1 )
                TestCancellation();
#endif /* __TBB_TASK_GROUP_CONTEXT */
#if !__TBB_SSE_STACK_ALIGNMENT_BROKEN
    #if HAVE_m128
            TestVectorTypes<ClassWithSSE>();
    #endif
    #if HAVE_m256
            if (have_AVX()) TestVectorTypes<ClassWithAVX>();
    #endif
#endif /*!__TBB_SSE_STACK_ALIGNMENT_BROKEN*/
            // Test that all workers sleep when no work
            TestCPUUserTime(p);
            TestSimplePartitionerStability();
        }
    }
#if __TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN
    REPORT("Known issue: exception handling tests are skipped.\n");
#endif
#if (HAVE_m128 || HAVE_m256) && __TBB_SSE_STACK_ALIGNMENT_BROKEN
    REPORT("Known issue: stack alignment for SSE/AVX not tested.\n");
#endif
    return Harness::Done;
}

#if _MSC_VER
#pragma warning (pop)
#endif
