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

#include "harness_defs.h"

#if __TBB_TEST_SKIP_PIC_MODE || __TBB_TEST_SKIP_BUILTINS_MODE
#include "harness.h"
int TestMain() {
    REPORT("Known issue: %s\n",
           __TBB_TEST_SKIP_PIC_MODE? "PIC mode is not supported" : "GCC builtins aren't available");
    return Harness::Skipped;
}
#else

// Put tbb/atomic.h first, so if it is missing a prerequisite header, we find out about it.
// The tests here do *not* test for atomicity, just serial correctness. */

#include "tbb/atomic.h"
#include "harness_assert.h"
#include <string.h> // memcmp

#if _MSC_VER && !defined(__INTEL_COMPILER)
    // Unary minus operator applied to unsigned type, result still unsigned
    // Constant conditional expression
    #pragma warning( disable: 4127 4310 )
#endif

enum LoadStoreExpression {
    UseOperators,
    UseImplicitAcqRel,
    UseExplicitFullyFenced,
    UseExplicitAcqRel,
    UseExplicitRelaxed,
    UseGlobalHelperFullyFenced,
    UseGlobalHelperAcqRel,
    UseGlobalHelperRelaxed
};

//! Structure that holds an atomic<T> and some guard bytes around it.
template<typename T, LoadStoreExpression E = UseOperators>
struct TestStruct {
    typedef unsigned char byte_type;
    T prefix;
    tbb::atomic<T> counter;
    T suffix;
    TestStruct( T i ) {
        ASSERT( sizeof(*this)==3*sizeof(T), NULL );
        for (size_t j = 0; j < sizeof(T); ++j) {
            reinterpret_cast<byte_type*>(&prefix)[j]             = byte_type(0x11*(j+1));
            reinterpret_cast<byte_type*>(&suffix)[sizeof(T)-j-1] = byte_type(0x11*(j+1));
        }
        if ( E == UseOperators )
            counter = i;
        else if ( E == UseExplicitRelaxed )
            counter.template store<tbb::relaxed>(i);
        else
            tbb::store<tbb::full_fence>( counter, i );
    }
    ~TestStruct() {
        // Check for writes outside the counter.
        for (size_t j = 0; j < sizeof(T); ++j) {
            ASSERT( reinterpret_cast<byte_type*>(&prefix)[j]             == byte_type(0x11*(j+1)), NULL );
            ASSERT( reinterpret_cast<byte_type*>(&suffix)[sizeof(T)-j-1] == byte_type(0x11*(j+1)), NULL );
        }
    }
    static tbb::atomic<T> gCounter;
};

// A global variable of type tbb::atomic<>
template<typename T, LoadStoreExpression E> tbb::atomic<T> TestStruct<T, E>::gCounter;

//! Test compare_and_swap template members of class atomic<T> for memory_semantics=M
template<typename T,tbb::memory_semantics M>
void TestCompareAndSwapWithExplicitOrdering( T i, T j, T k ) {
    ASSERT( i!=k, "values must be distinct" ); 
    // Test compare_and_swap that should fail
    TestStruct<T> x(i);
    T old = x.counter.template compare_and_swap<M>( j, k );
    ASSERT( old==i, NULL );
    ASSERT( x.counter==i, "old value not retained" );
    // Test compare and swap that should succeed
    old = x.counter.template compare_and_swap<M>( j, i );
    ASSERT( old==i, NULL );
    ASSERT( x.counter==j, "value not updated?" );
}

//! i, j, k must be different values
template<typename T>
void TestCompareAndSwap( T i, T j, T k ) {
    ASSERT( i!=k, "values must be distinct" );
    // Test compare_and_swap that should fail
    TestStruct<T> x(i);
    T old = x.counter.compare_and_swap( j, k );
    ASSERT( old==i, NULL );
    ASSERT( x.counter==i, "old value not retained" );
    // Test compare and swap that should succeed
    old = x.counter.compare_and_swap( j, i );
    ASSERT( old==i, NULL );
    if( x.counter==i ) {
        ASSERT( x.counter==j, "value not updated?" );
    } else {
        ASSERT( x.counter==j, "value trashed" );
    }
    // Check that atomic global variables work
    TestStruct<T>::gCounter = i;
    old = TestStruct<T>::gCounter.compare_and_swap( j, i );
    ASSERT( old==i, NULL );
    ASSERT( TestStruct<T>::gCounter==j, "value not updated?" );
    TestCompareAndSwapWithExplicitOrdering<T,tbb::full_fence>(i,j,k);
    TestCompareAndSwapWithExplicitOrdering<T,tbb::acquire>(i,j,k);
    TestCompareAndSwapWithExplicitOrdering<T,tbb::release>(i,j,k);
    TestCompareAndSwapWithExplicitOrdering<T,tbb::relaxed>(i,j,k);
}

//! memory_semantics variation on TestFetchAndStore
template<typename T, tbb::memory_semantics M>
void TestFetchAndStoreWithExplicitOrdering( T i, T j ) {
    ASSERT( i!=j, "values must be distinct" );
    TestStruct<T> x(i);
    T old = x.counter.template fetch_and_store<M>( j );
    ASSERT( old==i, NULL );
    ASSERT( x.counter==j, NULL );
}

//! i and j must be different values
template<typename T>
void TestFetchAndStore( T i, T j ) {
    ASSERT( i!=j, "values must be distinct" );
    TestStruct<T> x(i);
    T old = x.counter.fetch_and_store( j );
    ASSERT( old==i, NULL );
    ASSERT( x.counter==j, NULL );
    // Check that atomic global variables work
    TestStruct<T>::gCounter = i;
    old = TestStruct<T>::gCounter.fetch_and_store( j );
    ASSERT( old==i, NULL );
    ASSERT( TestStruct<T>::gCounter==j, "value not updated?" );
    TestFetchAndStoreWithExplicitOrdering<T,tbb::full_fence>(i,j);
    TestFetchAndStoreWithExplicitOrdering<T,tbb::acquire>(i,j);
    TestFetchAndStoreWithExplicitOrdering<T,tbb::release>(i,j);
    TestFetchAndStoreWithExplicitOrdering<T,tbb::relaxed>(i,j);
}

#if _MSC_VER && !defined(__INTEL_COMPILER)
    // conversion from <bigger integer> to <smaller integer>, possible loss of data
    // the warning seems a complete nonsense when issued for e.g. short+=short
    #pragma warning( disable: 4244 )
#endif

//! Test fetch_and_add members of class atomic<T> for memory_semantics=M
template<typename T,tbb::memory_semantics M>
void TestFetchAndAddWithExplicitOrdering( T i ) {
    TestStruct<T> x(i);
    T actual;
    T expected = i;

    // Test fetch_and_add member template
    for( int j=0; j<10; ++j ) {
        actual = x.counter.fetch_and_add(j);
        ASSERT( actual==expected, NULL );
        expected += j;
    }
    for( int j=0; j<10; ++j ) {
        actual = x.counter.fetch_and_add(-j);
        ASSERT( actual==expected, NULL );
        expected -= j;
    }

    // Test fetch_and_increment member template
    ASSERT( x.counter==i, NULL );
    actual = x.counter.template fetch_and_increment<M>();
    ASSERT( actual==i, NULL );
    ASSERT( x.counter==T(i+1), NULL );

    // Test fetch_and_decrement member template
    actual = x.counter.template fetch_and_decrement<M>();
    ASSERT( actual==T(i+1), NULL );
    ASSERT( x.counter==i, NULL );
}

//! Test fetch_and_add and related operators
template<typename T>
void TestFetchAndAdd( T i ) {
    TestStruct<T> x(i);
    T value;
    value = ++x.counter;
    ASSERT( value==T(i+1), NULL );
    value = x.counter++;
    ASSERT( value==T(i+1), NULL );
    value = x.counter--;
    ASSERT( value==T(i+2), NULL );
    value = --x.counter;
    ASSERT( value==i, NULL );
    T actual;
    T expected = i;
    for( int j=-100; j<=100; ++j ) {
        expected += j;
        actual = x.counter += j;
        ASSERT( actual==expected, NULL );
    }
    for( int j=-100; j<=100; ++j ) {
        expected -= j;
        actual = x.counter -= j;
        ASSERT( actual==expected, NULL );
    }
    // Test fetch_and_increment
    ASSERT( x.counter==i, NULL );
    actual = x.counter.fetch_and_increment();
    ASSERT( actual==i, NULL );
    ASSERT( x.counter==T(i+1), NULL );

    // Test fetch_and_decrement
    actual = x.counter.fetch_and_decrement();
    ASSERT( actual==T(i+1), NULL );
    ASSERT( x.counter==i, NULL );
    x.counter = i;
    ASSERT( x.counter==i, NULL );

    // Check that atomic global variables work
    TestStruct<T>::gCounter = i;
    value = TestStruct<T>::gCounter.fetch_and_add( 42 );
    expected = i+42;
    ASSERT( value==i, NULL );
    ASSERT( TestStruct<T>::gCounter==expected, "value not updated?" );
    TestFetchAndAddWithExplicitOrdering<T,tbb::full_fence>(i);
    TestFetchAndAddWithExplicitOrdering<T,tbb::acquire>(i);
    TestFetchAndAddWithExplicitOrdering<T,tbb::release>(i);
    TestFetchAndAddWithExplicitOrdering<T,tbb::relaxed>(i);
}

//! A type with unknown size.
class IncompleteType;

void TestFetchAndAdd( IncompleteType* ) {
    // There are no fetch-and-add operations on a IncompleteType*.
}
void TestFetchAndAdd( void* ) {
    // There are no fetch-and-add operations on a void*.
}

void TestFetchAndAdd( bool ) {
    // There are no fetch-and-add operations on a bool.
}

template<typename T>
void TestConst( T i ) {
    // Try const
    const TestStruct<T> x(i);
    ASSERT( memcmp( &i, &x.counter, sizeof(T) )==0, "write to atomic<T> broken?" );
    ASSERT( x.counter==i, "read of atomic<T> broken?" );
    const TestStruct<T, UseExplicitRelaxed> y(i);
    ASSERT( memcmp( &i, &y.counter, sizeof(T) )==0, "relaxed write to atomic<T> broken?" );
    ASSERT( tbb::load<tbb::relaxed>(y.counter) == i, "relaxed read of atomic<T> broken?" );
    const TestStruct<T, UseGlobalHelperFullyFenced> z(i);
    ASSERT( memcmp( &i, &z.counter, sizeof(T) )==0, "sequentially consistent write to atomic<T> broken?" );
    ASSERT( z.counter.template load<tbb::full_fence>() == i, "sequentially consistent read of atomic<T> broken?" );
}

template<typename T>
void TestOperations( T i, T j, T k ) {
    TestConst(i);
    TestCompareAndSwap(i,j,k);
    TestFetchAndStore(i,k);    // Pass i,k instead of i,j, because callee requires two distinct values.
}

template<typename T>
void TestParallel( const char* name );

bool ParallelError;

template<typename T>
struct AlignmentChecker {
    char c;
    tbb::atomic<T> i;
};

#include "harness.h"

#if _MSC_VER && !defined(__INTEL_COMPILER)
    // unary minus operator applied to unsigned type, result still unsigned
    #pragma warning( disable: 4146 )
#endif

/** T is an integral type. */
template<typename T>
void TestAtomicInteger( const char* name ) {
    REMARK("testing atomic<%s> (size=%d)\n",name,sizeof(tbb::atomic<T>));
#if ( __linux__ && __TBB_x86_32 && __GNUC__==3 && __GNUC_MINOR__==3 ) || defined(__SUNPRO_CC)
    // gcc 3.3 has known problem for 32-bit Linux, so only warn if there is a problem.
    // SUNPRO_CC does have this problem as well
    if( sizeof(T)==8 ) {
        if( sizeof(AlignmentChecker<T>)!=2*sizeof(tbb::atomic<T>) ) {
            REPORT("Known issue: alignment for atomic<%s> is wrong with gcc 3.3 and sunCC 5.9 2008/01/28 for IA32\n",name);
        }
    } else
#endif /* ( __linux__ && __TBB_x86_32 && __GNUC__==3 && __GNUC_MINOR__==3 ) || defined(__SUNPRO_CC) */
    ASSERT( sizeof(AlignmentChecker<T>)==2*sizeof(tbb::atomic<T>), NULL );
    TestOperations<T>(0L,T(-T(1)),T(1));
    for( int k=0; k<int(sizeof(long))*8-1; ++k ) {
        TestOperations<T>(T(1L<<k),T(~(1L<<k)),T(1-(1L<<k)));
        TestOperations<T>(T(-1L<<k),T(~(-1L<<k)),T(1-(-1L<<k)));
        TestFetchAndAdd<T>(T(-1L<<k));
    }
    TestParallel<T>( name );
}

template<typename T>
struct Foo {
    T x, y, z;
};

template<typename T>
void TestIndirection() {
    Foo<T> item;
    tbb::atomic<Foo<T>*> pointer;
    pointer = &item;
    for( int k=-10; k<=10; ++k ) {
        // Test various syntaxes for indirection to fields with non-zero offset.
        T value1=T(), value2=T();
        for( size_t j=0; j<sizeof(T); ++j ) {
            ((char*)&value1)[j] = char(k^j);
            ((char*)&value2)[j] = char(k^j*j);

        }
        pointer->y = value1;
        (*pointer).z = value2;
        T result1 = (*pointer).y;
        T result2 = pointer->z;
        ASSERT( memcmp(&value1,&result1,sizeof(T))==0, NULL );
        ASSERT( memcmp(&value2,&result2,sizeof(T))==0, NULL );
    }
}

//! Test atomic<T*>
template<typename T>
void TestAtomicPointer() {
    REMARK("testing atomic pointer (%d)\n",int(sizeof(T)));
    T array[1000];
    TestOperations<T*>(&array[500],&array[250],&array[750]);
    TestFetchAndAdd<T*>(&array[500]);
    TestIndirection<T>();
    TestParallel<T*>( "pointer" );
}

//! Test atomic<Ptr> where Ptr is a pointer to a type of unknown size
template<typename Ptr>
void TestAtomicPointerToTypeOfUnknownSize( const char* name ) {
    REMARK("testing atomic<%s>\n",name);
    char array[1000];
    TestOperations<Ptr>((Ptr)(void*)&array[500],(Ptr)(void*)&array[250],(Ptr)(void*)&array[750]);
    TestParallel<Ptr>( name );
}

void TestAtomicBool() {
    REMARK("testing atomic<bool>\n");
    TestOperations<bool>(true,true,false);
    TestOperations<bool>(false,false,true);
    TestParallel<bool>( "bool" );
}

enum Color {Red=0,Green=1,Blue=-1};

void TestAtomicEnum() {
    REMARK("testing atomic<Color>\n");
    TestOperations<Color>(Red,Green,Blue);
    TestParallel<Color>( "Color" );
}

template<typename T>
void TestAtomicFloat( const char* name ) {
    REMARK("testing atomic<%s>\n", name );
    TestOperations<T>(0.5,3.25,10.75);
    TestParallel<T>( name );
}

#if __TBB_BIG_ENDIAN!=-1
namespace masked_cas_helpers {
    const int numMaskedOperations = 100000;
    const int testSpaceSize = 8;
    int prime[testSpaceSize] = {3,5,7,11,13,17,19,23};


    template<typename T>
    class TestMaskedCAS_Body: NoAssign {
        T*  test_space_uncontended;
        T*  test_space_contended;
    public:
        TestMaskedCAS_Body( T* _space1, T* _space2 ) : test_space_uncontended(_space1), test_space_contended(_space2) {}
        void operator()( int my_idx ) const {
            using tbb::internal::__TBB_MaskedCompareAndSwap;
            const T my_prime = T(prime[my_idx]);
            T* const my_ptr = test_space_uncontended+my_idx;
            T old_value=0;
            for( int i=0; i<numMaskedOperations; ++i, old_value+=my_prime ){
                T result;
            // Test uncontended case
                T new_value = old_value + my_prime;
                // The following CAS should always fail
                result = __TBB_MaskedCompareAndSwap<T>(my_ptr,new_value,old_value-1);
                ASSERT(result!=old_value-1, "masked CAS succeeded while it should fail");
                ASSERT(result==*my_ptr, "masked CAS result mismatch with real value");
                // The following one should succeed
                result = __TBB_MaskedCompareAndSwap<T>(my_ptr,new_value,old_value);
                ASSERT(result==old_value && *my_ptr==new_value, "masked CAS failed while it should succeed");
                // The following one should fail again
                result = __TBB_MaskedCompareAndSwap<T>(my_ptr,new_value,old_value);
                ASSERT(result!=old_value, "masked CAS succeeded while it should fail");
                ASSERT(result==*my_ptr, "masked CAS result mismatch with real value");
            // Test contended case
                for( int j=0; j<testSpaceSize; ++j ){
                    // try adding my_prime until success
                    T value;
                    do {
                        value = test_space_contended[j];
                        result = __TBB_MaskedCompareAndSwap<T>(test_space_contended+j,value+my_prime,value);
                    } while( result!=value );
                }
            }
        }
    };

    template<typename T>
    struct intptr_as_array_of
    {
        static const int how_many_Ts = sizeof(intptr_t)/sizeof(T);
        union {
            intptr_t result;
            T space[ how_many_Ts ];
        };
    };

    template<typename T>
    intptr_t getCorrectUncontendedValue(int slot_idx) {
        intptr_as_array_of<T> slot;
        slot.result = 0;
        for( int i=0; i<slot.how_many_Ts; ++i ) {
            const T my_prime = T(prime[slot_idx*slot.how_many_Ts + i]);
            for( int j=0; j<numMaskedOperations; ++j )
                slot.space[i] += my_prime;
        }
        return slot.result;
    }

    template<typename T>
    intptr_t getCorrectContendedValue() {
        intptr_as_array_of<T>  slot;
        slot.result = 0;
        for( int i=0; i<slot.how_many_Ts; ++i )
            for( int primes=0; primes<testSpaceSize; ++primes )
                for( int j=0; j<numMaskedOperations; ++j )
                    slot.space[i] += prime[primes];
        return slot.result;
    }
} // namespace masked_cas_helpers
template<typename T>
void TestMaskedCAS() {
    using namespace masked_cas_helpers;
    REMARK("testing masked CAS<%d>\n",int(sizeof(T)));

    const int num_slots = sizeof(T)*testSpaceSize/sizeof(intptr_t);
    intptr_t arr1[num_slots+2]; // two more "canary" slots at boundaries
    intptr_t arr2[num_slots+2];
    for(int i=0; i<num_slots+2; ++i)
        arr2[i] = arr1[i] = 0;
    T* test_space_uncontended = (T*)(arr1+1);
    T* test_space_contended = (T*)(arr2+1);

    NativeParallelFor( testSpaceSize, TestMaskedCAS_Body<T>(test_space_uncontended, test_space_contended) );

    ASSERT( arr1[0]==0 && arr1[num_slots+1]==0 && arr2[0]==0 && arr2[num_slots+1]==0 , "adjacent memory was overwritten" );
    const intptr_t correctContendedValue = getCorrectContendedValue<T>();
    for(int i=0; i<num_slots; ++i) {
        ASSERT( arr1[i+1]==getCorrectUncontendedValue<T>(i), "unexpected value in an uncontended slot" );
        ASSERT( arr2[i+1]==correctContendedValue, "unexpected value in a contended slot" );
    }
}
#endif
template <typename T>
class TestRelaxedLoadStorePlainBody {
    static T s_turn,
             s_ready;

public:
    static unsigned s_count1,
                    s_count2;

    void operator() ( int id ) const {
        using tbb::internal::__TBB_load_relaxed;
        using tbb::internal::__TBB_store_relaxed;

        if ( id == 0 ) {
            while ( !__TBB_load_relaxed(s_turn) ) {
                ++s_count1;
                __TBB_store_relaxed(s_ready, 1);
            }
        }
        else {
            while ( !__TBB_load_relaxed(s_ready) ) {
                ++s_count2;
                continue;
            }
            __TBB_store_relaxed(s_turn, 1);
        }
    }
}; // class TestRelaxedLoadStorePlainBody<T>

template <typename T> T TestRelaxedLoadStorePlainBody<T>::s_turn = 0;
template <typename T> T TestRelaxedLoadStorePlainBody<T>::s_ready = 0;
template <typename T> unsigned TestRelaxedLoadStorePlainBody<T>::s_count1 = 0;
template <typename T> unsigned TestRelaxedLoadStorePlainBody<T>::s_count2 = 0;

template <typename T>
class TestRelaxedLoadStoreAtomicBody {
    static tbb::atomic<T> s_turn,
                          s_ready;

public:
    static unsigned s_count1,
                    s_count2;

    void operator() ( int id ) const {
        if ( id == 0 ) {
            while ( s_turn.template load<tbb::relaxed>() == 0 ) {
                ++s_count1;
                s_ready.template store<tbb::relaxed>(1);
            }
        }
        else {
            while ( s_ready.template load<tbb::relaxed>() == 0 ) {
                ++s_count2;
                continue;
            }
            s_turn.template store<tbb::relaxed>(1);
        }
    }
}; // class TestRelaxedLoadStoreAtomicBody<T>

template <typename T> tbb::atomic<T> TestRelaxedLoadStoreAtomicBody<T>::s_turn;
template <typename T> tbb::atomic<T> TestRelaxedLoadStoreAtomicBody<T>::s_ready;
template <typename T> unsigned TestRelaxedLoadStoreAtomicBody<T>::s_count1 = 0;
template <typename T> unsigned TestRelaxedLoadStoreAtomicBody<T>::s_count2 = 0;

template <typename T>
void TestRegisterPromotionSuppression () {
    REMARK("testing register promotion suppression (size=%d)\n", (int)sizeof(T));
    NativeParallelFor( 2, TestRelaxedLoadStorePlainBody<T>() );
    NativeParallelFor( 2, TestRelaxedLoadStoreAtomicBody<T>() );
}

template<unsigned N>
class ArrayElement {
    char item[N];
};

#include "harness_barrier.h"
namespace bit_operation_test_suite{
    struct fixture : NoAssign{
        static const uintptr_t zero = 0;
        const uintptr_t random_value ;
        const uintptr_t inverted_random_value ;
        fixture():
            random_value (tbb::internal::size_t_select(0x9E3779B9,0x9E3779B97F4A7C15ULL)),
            inverted_random_value ( ~random_value)
        {}
    };

    struct TestAtomicORSerially : fixture {
        void operator()(){
            //these additional variable are needed to get more meaningful expression in the assert
            uintptr_t initial_value = zero;
            uintptr_t atomic_or_result = initial_value;
            uintptr_t atomic_or_operand = random_value;

            __TBB_AtomicOR(&atomic_or_result,atomic_or_operand);

            ASSERT(atomic_or_result == (initial_value | atomic_or_operand),"AtomicOR should do the OR operation");
        }
    };
    struct TestAtomicANDSerially : fixture {
        void operator()(){
            //these additional variable are needed to get more meaningful expression in the assert
            uintptr_t initial_value = inverted_random_value;
            uintptr_t atomic_and_result = initial_value;
            uintptr_t atomic_and_operand = random_value;

            __TBB_AtomicAND(&atomic_and_result,atomic_and_operand);

            ASSERT(atomic_and_result == (initial_value & atomic_and_operand),"AtomicAND should do the AND operation");
        }
    };

    struct TestAtomicORandANDConcurrently : fixture {
        static const uintptr_t bit_per_word = sizeof(uintptr_t) * 8;
        static const uintptr_t threads_number = bit_per_word;
        Harness::SpinBarrier m_barrier;
        uintptr_t bitmap;
        TestAtomicORandANDConcurrently():bitmap(zero) {}

        struct thread_body{
            TestAtomicORandANDConcurrently* test;
            thread_body(TestAtomicORandANDConcurrently* the_test) : test(the_test) {}
            void operator()(int thread_index)const{
                const uintptr_t single_bit_mask = ((uintptr_t)1u) << (thread_index % bit_per_word);
                test->m_barrier.wait();
                static const char* error_msg = "AtomicOR and AtomicAND should be atomic";
                for (uintptr_t attempts=0; attempts<1000; attempts++ ){
                    //Set and clear designated bits in a word.
                    __TBB_AtomicOR(&test->bitmap,single_bit_mask);
                     __TBB_Yield();
                    bool the_bit_is_set_after_set_via_atomic_or = ((__TBB_load_with_acquire(test->bitmap) & single_bit_mask )== single_bit_mask);
                    ASSERT(the_bit_is_set_after_set_via_atomic_or,error_msg);

                    __TBB_AtomicAND(&test->bitmap,~single_bit_mask);
                    __TBB_Yield();
                    bool the_bit_is_clear_after_clear_via_atomic_and = ((__TBB_load_with_acquire(test->bitmap) & single_bit_mask )== zero);
                    ASSERT(the_bit_is_clear_after_clear_via_atomic_and,error_msg);
                }
            }
        };
        void operator()(){
            m_barrier.initialize(threads_number);
            NativeParallelFor(threads_number,thread_body(this));
        }
    };
}
void TestBitOperations(){
    using namespace bit_operation_test_suite;
    TestAtomicORSerially()();
    TestAtomicANDSerially()();
    TestAtomicORandANDConcurrently()();
}

int TestMain () {
    #if __TBB_64BIT_ATOMICS
    TestAtomicInteger<unsigned long long>("unsigned long long");
    TestAtomicInteger<long long>("long long");
    #else
    REPORT("64-bit atomics not supported\n");
    ASSERT(sizeof(long long)==8, "type long long is not 64 bits");
    #endif
    TestAtomicInteger<unsigned long>("unsigned long");
    TestAtomicInteger<long>("long");
    TestAtomicInteger<unsigned int>("unsigned int");
    TestAtomicInteger<int>("int");
    TestAtomicInteger<unsigned short>("unsigned short");
    TestAtomicInteger<short>("short");
    TestAtomicInteger<signed char>("signed char");
    TestAtomicInteger<unsigned char>("unsigned char");
    TestAtomicInteger<char>("char");
    TestAtomicInteger<wchar_t>("wchar_t");
    TestAtomicInteger<size_t>("size_t");
    TestAtomicInteger<ptrdiff_t>("ptrdiff_t");
    TestAtomicPointer<ArrayElement<1> >();
    TestAtomicPointer<ArrayElement<2> >();
    TestAtomicPointer<ArrayElement<3> >();
    TestAtomicPointer<ArrayElement<4> >();
    TestAtomicPointer<ArrayElement<5> >();
    TestAtomicPointer<ArrayElement<6> >();
    TestAtomicPointer<ArrayElement<7> >();
    TestAtomicPointer<ArrayElement<8> >();
    TestAtomicPointerToTypeOfUnknownSize<IncompleteType*>( "IncompleteType*" );
    TestAtomicPointerToTypeOfUnknownSize<void*>( "void*" );
    TestAtomicBool();
    TestAtomicEnum();
    TestAtomicFloat<float>("float");
    #if __TBB_64BIT_ATOMICS
    TestAtomicFloat<double>("double");
    #else
    ASSERT(sizeof(double)==8, "type double is not 64 bits");
    #endif
    ASSERT( !ParallelError, NULL );
    #if __TBB_BIG_ENDIAN!=-1
        TestMaskedCAS<unsigned char>();
        TestMaskedCAS<unsigned short>();
    #else
        REPORT("Generic part-word CAS is not available\n");
    #endif
#if __TBB_64BIT_ATOMICS
    TestRegisterPromotionSuppression<tbb::internal::int64_t>();
#endif
    TestRegisterPromotionSuppression<tbb::internal::int32_t>();
    TestRegisterPromotionSuppression<tbb::internal::int16_t>();
    TestRegisterPromotionSuppression<tbb::internal::int8_t>();
    TestBitOperations();

    return Harness::Done;
}

template<typename T>
struct FlagAndMessage {
    //! 0 if message not set yet, 1 if message is set.
    tbb::atomic<T> flag;
    /** Force flag and message to be on distinct cache lines for machines with cache line size <= 4096 bytes */
    char pad[4096/sizeof(T)];
    //! Non-zero if message is ready
    T message;
};

// A special template function used for summation.
// Actually it is only necessary because of its specialization for void*
template<typename T>
T special_sum(intptr_t arg1, intptr_t arg2) {
    return (T)((T)arg1 + arg2);
}

// The specialization for IncompleteType* is required
// because pointer arithmetic (+) is impossible with IncompleteType*
template<>
IncompleteType* special_sum<IncompleteType*>(intptr_t arg1, intptr_t arg2) {
    return (IncompleteType*)(arg1 + arg2);
}

// The specialization for void* is required
// because pointer arithmetic (+) is impossible with void*
template<>
void* special_sum<void*>(intptr_t arg1, intptr_t arg2) {
    return (void*)(arg1 + arg2);
}

// The specialization for bool is required to shut up gratuitous compiler warnings,
// because some compilers warn about casting int to bool.
template<>
bool special_sum<bool>(intptr_t arg1, intptr_t arg2) {
    return ((arg1!=0) + arg2)!=0;
}

volatile int One = 1;

inline bool IsRelaxed ( LoadStoreExpression e ) {
    return e == UseExplicitRelaxed || e == UseGlobalHelperRelaxed;
}

template <typename T, LoadStoreExpression E>
struct LoadStoreTraits;

template <typename T>
struct LoadStoreTraits<T, UseOperators> {
    static void load ( T& dst, const tbb::atomic<T>& src ) { dst = src; }
    static void store ( tbb::atomic<T>& dst, const T& src ) { dst = src; }
};

template <typename T>
struct LoadStoreTraits<T, UseImplicitAcqRel> {
    static void load ( T& dst, const tbb::atomic<T>& src ) { dst = src.load(); }
    static void store ( tbb::atomic<T>& dst, const T& src ) { dst.store(src); }
};

template <typename T>
struct LoadStoreTraits<T, UseExplicitFullyFenced> {
    static void load ( T& dst, const tbb::atomic<T>& src ) { dst = src.template load<tbb::full_fence>(); }
    static void store ( tbb::atomic<T>& dst, const T& src ) { dst.template store<tbb::full_fence>(src); }
};

template <typename T>
struct LoadStoreTraits<T, UseExplicitAcqRel> {
    static void load ( T& dst, const tbb::atomic<T>& src ) { dst = src.template load<tbb::acquire>(); }
    static void store ( tbb::atomic<T>& dst, const T& src ) { dst.template store<tbb::release>(src); }
};

template <typename T>
struct LoadStoreTraits<T, UseExplicitRelaxed> {
    static void load ( T& dst, const tbb::atomic<T>& src ) { dst = src.template load<tbb::relaxed>(); }
    static void store ( tbb::atomic<T>& dst, const T& src ) { dst.template store<tbb::relaxed>(src); }
};

template <typename T>
struct LoadStoreTraits<T, UseGlobalHelperFullyFenced> {
    static void load ( T& dst, const tbb::atomic<T>& src ) { dst = tbb::load<tbb::full_fence>(src); }
    static void store ( tbb::atomic<T>& dst, const T& src ) { tbb::store<tbb::full_fence>(dst, src); }
};

template <typename T>
struct LoadStoreTraits<T, UseGlobalHelperAcqRel> {
    static void load ( T& dst, const tbb::atomic<T>& src ) { dst = tbb::load<tbb::acquire>(src); }
    static void store ( tbb::atomic<T>& dst, const T& src ) { tbb::store<tbb::release>(dst, src); }
};

template <typename T>
struct LoadStoreTraits<T, UseGlobalHelperRelaxed> {
    static void load ( T& dst, const tbb::atomic<T>& src ) { dst = tbb::load<tbb::relaxed>(src); }
    static void store ( tbb::atomic<T>& dst, const T& src ) { tbb::store<tbb::relaxed>(dst, src); }
};

template<typename T, LoadStoreExpression E>
class HammerLoadAndStoreFence: NoAssign {
    typedef LoadStoreTraits<T, E> trait;
    FlagAndMessage<T>* fam;
    const int n;
    const int p;
    const int trial;
    const char* name;
    mutable T accum;
public:
    HammerLoadAndStoreFence( FlagAndMessage<T>* fam_, int n_, int p_, const char* name_, int trial_ ) : fam(fam_), n(n_), p(p_), trial(trial_), name(name_) {}
    void operator()( int k ) const {
        int one = One;
        FlagAndMessage<T>* s = fam+k;
        FlagAndMessage<T>* s_next = fam + (k+1)%p;
        for( int i=0; i<n; ++i ) {
            // The inner for loop is a spin-wait loop, which is normally considered very bad style.
            // But we must use it here because we are interested in examining subtle hardware effects.
            for(unsigned short cnt=1; ; ++cnt) {
                if( !(cnt%1024) ) // to help 1-core or oversubscribed systems complete the test, yield every 2^10 iterations
                    __TBB_Yield();
                // Compilers typically generate non-trivial sequence for division by a constant.
                // The expression here is dependent on the loop index i, so it cannot be hoisted.
                #define COMPLICATED_ZERO (i*(one-1)/100)
                // Read flag and then the message
                T flag, message;
                if( trial&1 ) {
                    // COMPLICATED_ZERO here tempts compiler to hoist load of message above reading of flag.
                    trait::load( flag, (s+COMPLICATED_ZERO)->flag );
                    message = s->message;
                } else {
                    trait::load( flag, s->flag );
                    message = s->message;
                }
                if( flag ) {
                    if( flag!=(T)-1 ) {
                        REPORT("ERROR: flag!=(T)-1 k=%d i=%d trial=%x type=%s (atomicity problem?)\n", k, i, trial, name );
                        ParallelError = true;
                    } 
                    if( !IsRelaxed(E) && message!=(T)-1 ) {
                        REPORT("ERROR: message!=(T)-1 k=%d i=%d trial=%x type=%s mode=%d (memory fence problem?)\n", k, i, trial, name, E );
                        ParallelError = true;
                    }
                    s->message = T(0);
                    trait::store( s->flag, T(0) );
                    // Prevent deadlock possible in relaxed mode because of store(0)
                    // to the first thread's flag being reordered after the last
                    // thread's store(-1) into it.
                    if ( IsRelaxed(E) ) {
                        while( s_next->flag.template load<tbb::relaxed>() != 0 )
                            __TBB_Yield();
                    }
                    else
                        ASSERT( s_next->flag == 0, NULL );
                    // Set message and then the flag
                    if( trial&2 ) {
                        // COMPLICATED_ZERO here tempts compiler to sink store below setting of flag
                        s_next->message = special_sum<T>(-1, COMPLICATED_ZERO);
                        trait::store( s_next->flag, (T)-1 );
                    } else {
                        s_next->message = (T)-1;
                        trait::store( s_next->flag, (T)-1 );
                    }
                    break;
                } else {
                    // Force compiler to use message anyway, so it cannot sink read of s->message below the if.
                    accum = message;
                }
            }
        }
    }
};

//! Test that atomic<T> has acquire semantics for loads and release semantics for stores.
/** Test performs round-robin passing of message among p processors,
    where p goes from MinThread to MaxThread. */
template<typename T, LoadStoreExpression E>
void TestLoadAndStoreFences( const char* name ) {
    for( int p=MinThread<2 ? 2 : MinThread; p<=MaxThread; ++p ) {
        FlagAndMessage<T>* fam = new FlagAndMessage<T>[p];
        // Each of four trials exercise slightly different expression pattern within the test.
        // See occurrences of COMPLICATED_ZERO for details.
        for( int trial=0; trial<4; ++trial ) {
            memset( fam, 0, p*sizeof(FlagAndMessage<T>) );
            fam->message = (T)-1;
            fam->flag = (T)-1;
            NativeParallelFor( p, HammerLoadAndStoreFence<T, E>( fam, 100, p, name, trial ) );
            if ( !IsRelaxed(E) ) {
                for( int k=0; k<p; ++k ) {
                    ASSERT( fam[k].message==(k==0 ? (T)-1 : 0), "incomplete round-robin?" );
                    ASSERT( fam[k].flag==(k==0 ? (T)-1 : 0), "incomplete round-robin?" );
                }
            }
        }
        delete[] fam;
    }
}

//! Sparse set of values of integral type T.
/** Set is designed so that if a value is read or written non-atomically,
    the resulting intermediate value is likely to not be a member of the set. */
template<typename T>
class SparseValueSet {
    T factor;
public:
    SparseValueSet() {
        // Compute factor such that:
        // 1. It has at least one 1 in most of its bytes.
        // 2. The bytes are typically different.
        // 3. When multiplied by any value <=127, the product does not overflow.
        factor = T(0);
        for( unsigned i=0; i<sizeof(T)*8-7; i+=7 )
            factor = T(factor | T(1)<<i);
     }
     //! Get ith member of set
     T get( int i ) const {
         // Create multiple of factor.  The & prevents overflow of the product.
         return T((i&0x7F)*factor);
     }
     //! True if set contains x
     bool contains( T x ) const {
         // True if
         return (x%factor)==0;
     }
};

//! Specialization for pointer types.  The pointers are random and should not be dereferenced.
template<typename T>
class SparseValueSet<T*> {
    SparseValueSet<ptrdiff_t> my_set;
public:
    T* get( int i ) const {return reinterpret_cast<T*>(my_set.get(i));}
    bool contains( T* x ) const {return my_set.contains(reinterpret_cast<ptrdiff_t>(x));}
};

//! Specialization for bool.
/** Checking bool for atomic read/write is pointless in practice, because
    there is no way to *not* atomically read or write a bool value. */
template<>
class SparseValueSet<bool> {
public:
    bool get( int i ) const {return i&1;}
    bool contains( bool ) const {return true;}
};

#if _MSC_VER==1500 && !defined(__INTEL_COMPILER)
    // VS2008/VC9 seems to have an issue; limits pull in math.h
    #pragma warning( push )
    #pragma warning( disable: 4985 )
#endif
#include <limits> /* Need std::numeric_limits */
#if _MSC_VER==1500 && !defined(__INTEL_COMPILER)
    #pragma warning( pop )
#endif

//! Commonality inherited by specializations for floating-point types.
template<typename T>
class SparseFloatSet: NoAssign {
    const T epsilon;
public:
    SparseFloatSet() : epsilon(std::numeric_limits<T>::epsilon()) {}
    T get( int i ) const {
        return i==0 ? T(0) : 1/T((i&0x7F)+1);
    }
    bool contains( T x ) const {
        if( x==T(0) ) {
            return true;
        } else {
            int j = int(1/x+T(0.5));
            if( 0<j && j<=128 ) {
                T error = x*T(j)-T(1);
                // In the calculation above, if x was indeed generated by method get, the error should be
                // at most epsilon, because x is off by at most 1/2 ulp from its infinitely precise value,
                // j is exact, and the multiplication incurs at most another 1/2 ulp of round-off error.
                if( -epsilon<=error && error<=epsilon ) {
                    return true;
                } else {
                    REPORT("Warning: excessive floating-point error encountered j=%d x=%.15g error=%.15g\n",j,x,error);
                }
            }
            return false;
        }
    };
};

template<>
class SparseValueSet<float>: public SparseFloatSet<float> {};

template<>
class SparseValueSet<double>: public SparseFloatSet<double> {};

template<typename T>
class HammerAssignment: NoAssign {
    tbb::atomic<T>& x;
    const char* name;
    SparseValueSet<T> set;
public:
    HammerAssignment( tbb::atomic<T>& x_, const char* name_ ) : x(x_), name(name_) {}
    void operator()( int k ) const {
        const int n = 1000000;
        if( k ) {
            tbb::atomic<T> z;
            AssertSameType( z=x, z );    // Check that return type from assignment is correct
            for( int i=0; i<n; ++i ) {
                // Read x atomically into z.
                z = x;
                if( !set.contains(z) ) {
                    REPORT("ERROR: assignment of atomic<%s> is not atomic\n", name);
                    ParallelError = true;
                    return;
                }
            }
        } else {
            tbb::atomic<T> y;
            for( int i=0; i<n; ++i ) {
                // Get pseudo-random value.
                y = set.get(i);
                // Write y atomically into x.
                x = y;
            }
        }
    }
};

// Compile-time check that a class method has the required signature.
// Intended to check the assignment operator of tbb::atomic.
template<typename T> void TestAssignmentSignature( T& (T::*)(const T&) ) {}

#if _MSC_VER && !defined(__INTEL_COMPILER)
    #pragma warning( disable: 4355 4800 )
#endif

template<typename T>
void TestAssignment( const char* name ) {
    TestAssignmentSignature( &tbb::atomic<T>::operator= );
    tbb::atomic<T> x;
    x = T(0);
    NativeParallelFor( 2, HammerAssignment<T>( x, name ) );
#if __TBB_x86_32 && (__linux__ || __FreeBSD__ || _WIN32)
    if( sizeof(T)==8 ) {
        // Some compilers for IA-32 fail to provide 8-byte alignment of objects on the stack,
        // even if the object specifies 8-byte alignment.  On such platforms, the IA-32 implementation
        // of atomic<long long> and atomic<unsigned long long> use different tactics depending upon
        // whether the object is properly aligned or not.  The following abusive test ensures that we
        // cover both the proper and improper alignment cases, one with the x above and the other with
        // the y below, perhaps not respectively.

        // Allocate space big enough to always contain 8-byte locations that are aligned and misaligned.
        char raw_space[15];
        // Set delta to 0 if x is aligned, 4 otherwise.
        uintptr_t delta = ((reinterpret_cast<uintptr_t>(&x)&7) ? 0 : 4);
        // y crosses 8-byte boundary if and only if x does not cross.
        tbb::atomic<T>& y = *reinterpret_cast<tbb::atomic<T>*>((reinterpret_cast<uintptr_t>(&raw_space[7+delta])&~7u) - delta);
        // Assertion checks that y really did end up somewhere inside "raw_space".
        ASSERT( raw_space<=reinterpret_cast<char*>(&y), "y starts before raw_space" );
        ASSERT( reinterpret_cast<char*>(&y+1) <= raw_space+sizeof(raw_space), "y starts after raw_space" );
        y = T(0);
        NativeParallelFor( 2, HammerAssignment<T>( y, name ) );
    }
#endif /* __TBB_x86_32 && (__linux__ || __FreeBSD__ || _WIN32) */
}

static const unsigned Primes[] = {
    0x9e3779b1, 0xffe6cc59, 0x2109f6dd, 0x43977ab5, 0xba5703f5, 0xb495a877, 0xe1626741, 0x79695e6b,
    0xbc98c09f, 0xd5bee2b3, 0x287488f9, 0x3af18231, 0x9677cd4d, 0xbe3a6929, 0xadc6a877, 0xdcf0674b,
    0xbe4d6fe9, 0x5f15e201, 0x99afc3fd, 0xf3f16801, 0xe222cfff, 0x24ba5fdb, 0x0620452d, 0x79f149e3,
    0xc8b93f49, 0x972702cd, 0xb07dd827, 0x6c97d5ed, 0x085a3d61, 0x46eb5ea7, 0x3d9910ed, 0x2e687b5b,
    0x29609227, 0x6eb081f1, 0x0954c4e1, 0x9d114db9, 0x542acfa9, 0xb3e6bd7b, 0x0742d917, 0xe9f3ffa7,
    0x54581edb, 0xf2480f45, 0x0bb9288f, 0xef1affc7, 0x85fa0ca7, 0x3ccc14db, 0xe6baf34b, 0x343377f7,
    0x5ca19031, 0xe6d9293b, 0xf0a9f391, 0x5d2e980b, 0xfc411073, 0xc3749363, 0xb892d829, 0x3549366b,
    0x629750ad, 0xb98294e5, 0x892d9483, 0xc235baf3, 0x3d2402a3, 0x6bdef3c9, 0xbec333cd, 0x40c9520f
};

class FastRandom {
    unsigned x, a;
public:
    unsigned short get() {
        unsigned short r = (unsigned short)(x>>16);
        x = x*a+1;
        return r;
    }
    FastRandom( unsigned seed ) {
        x = seed;
        a = Primes[seed % (sizeof(Primes)/sizeof(Primes[0]))];
    }
};

template <typename T, LoadStoreExpression E>
class ArbitrationBody : NoAssign, Harness::NoAfterlife {
    typedef LoadStoreTraits<T, E> trait;

    mutable FastRandom my_rand;
    static const unsigned short c_rand_ceil = 10;

    static tbb::atomic<T> s_ready[2];
    static tbb::atomic<T> s_turn;
    static volatile bool s_inside;

public:
    void operator() ( int id ) const {
        const int me = id;
        const T other = (T)(uintptr_t)(1 - id),
                cleared = T(0),
                signaled = T(1);
        for ( int i = 0; i < 100000; ++i ) {
            trait::store( s_ready[me], signaled );
            trait::store( s_turn, other );
            T r, t;
            for ( int j = 0; ; ++j ) {
                trait::load(r, s_ready[(uintptr_t)other]);
                trait::load(t, s_turn);
                if ( r != signaled || t != other )
                    break;
                __TBB_Pause(1);
                if ( j == 2<<12 ) {
                    j = 0;
                    __TBB_Yield();
                }
            }
            // Entered critical section
            ASSERT( !s_inside, "Peterson lock is broken - some fences are missing" );
            s_inside = true;
            unsigned short spin = my_rand.get() % c_rand_ceil;
            for ( volatile int j = 0; j < spin; ++j )
                continue;
            s_inside = false;
            ASSERT( !s_inside, "Peterson lock is broken - some fences are missing" );
            // leaving critical section
            trait::store( s_ready[me], cleared );
            spin = my_rand.get() % c_rand_ceil;
            for ( volatile int j = 0; j < spin; ++j )
                continue;
        }
    }

    ArbitrationBody () : my_rand((unsigned)(uintptr_t)this) {}
};

template<typename T, LoadStoreExpression E> tbb::atomic<T> ArbitrationBody<T, E>::s_ready[2];
template<typename T, LoadStoreExpression E> tbb::atomic<T> ArbitrationBody<T, E>::s_turn;
template<typename T, LoadStoreExpression E> volatile bool ArbitrationBody<T, E>::s_inside = false;

template <typename T, LoadStoreExpression E>
void TestDekkerArbitration () {
    NativeParallelFor( 2, ArbitrationBody<T, E>() );
}

template<typename T>
void TestParallel( const char* name ) {
    TestLoadAndStoreFences<T, UseOperators>(name);
    TestLoadAndStoreFences<T, UseImplicitAcqRel>(name);
    TestLoadAndStoreFences<T, UseExplicitFullyFenced>(name);
    TestLoadAndStoreFences<T, UseExplicitAcqRel>(name);
    TestLoadAndStoreFences<T, UseExplicitRelaxed>(name);
    TestLoadAndStoreFences<T, UseGlobalHelperFullyFenced>(name);
    TestLoadAndStoreFences<T, UseGlobalHelperAcqRel>(name);
    TestLoadAndStoreFences<T, UseGlobalHelperRelaxed>(name);
    TestAssignment<T>(name);
    TestDekkerArbitration<T, UseExplicitFullyFenced>();
    TestDekkerArbitration<T, UseGlobalHelperFullyFenced>();
}

#endif // __TBB_TEST_SKIP_PIC_MODE || __TBB_TEST_SKIP_BUILTINS_MODE
