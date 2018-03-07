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

// Basic testing of an allocator
// Tests against requirements in 20.1.5 of ISO C++ Standard (1998).
// Does not check for thread safety or false sharing issues.
//
// Tests for compatibility with the host's STL are in 
// test_Allocator_STL.h.  Those tests are in a separate file
// because they bring in lots of STL headers, and the tests here
// are supposed to work in the abscense of STL.

#include "harness.h"

template<typename A>
struct is_zero_filling {
    static const bool value = false;
};

int NumberOfFoo;

template<typename T, size_t N>
struct Foo {
    T foo_array[N];
    Foo() {
        zero_fill<T>(foo_array, N);
        ++NumberOfFoo;
    }
    Foo( const Foo& x ) {
        *this = x;
        ++NumberOfFoo;
    }
    ~Foo() {
        --NumberOfFoo;
    }
};

inline char PseudoRandomValue( size_t j, size_t k ) {
    return char(j*3 ^ j>>4 ^ k);
}

//! T is type and A is allocator for that type 
template<typename T, typename A>
void TestBasic( A& a ) {
    T x;
    const T cx = T();

    // See Table 32 in ISO ++ Standard
    typename A::pointer px = &x;
    typename A::const_pointer pcx = &cx;

    typename A::reference rx = x;
    ASSERT( &rx==&x, NULL );

    typename A::const_reference rcx = cx;
    ASSERT( &rcx==&cx, NULL );

    typename A::value_type v = x;

    typename A::size_type size;
    size = 0;
    --size;
    ASSERT( size>0, "not an unsigned integral type?" );

    typename A::difference_type difference;
    difference = 0;
    --difference;
    ASSERT( difference<0, "not an signed integral type?" );

    // "rebind" tested by our caller 

    ASSERT( a.address(rx)==px, NULL );

    ASSERT( a.address(rcx)==pcx, NULL );

    typename A::pointer array[100];
    size_t sizeof_T = sizeof(T);
    for( size_t k=0; k<100; ++k ) {
        array[k] = k&1 ? a.allocate(k,array[0]) : a.allocate(k);
        char* s = reinterpret_cast<char*>(reinterpret_cast<void*>(array[k]));
        for( size_t j=0; j<k*sizeof_T; ++j )
            s[j] = PseudoRandomValue(j,k);
    }

    // Test hint argument. This can't be compiled when hint is void*, It should be const void*
    typename A::pointer a_ptr;
    const void * const_hint = NULL;    
    a_ptr = a.allocate (1, const_hint);    
    a.deallocate(a_ptr, 1);

    // Test "a.deallocate(p,n)
    for( size_t k=0; k<100; ++k ) {
        char* s = reinterpret_cast<char*>(reinterpret_cast<void*>(array[k]));
        for( size_t j=0; j<k*sizeof_T; ++j )
            ASSERT( s[j] == PseudoRandomValue(j,k), NULL );
        a.deallocate(array[k],k);
    }

    // Test "a.max_size()"
    AssertSameType( a.max_size(), typename A::size_type(0) );
    // Following assertion catches case where max_size() is so large that computation of 
    // number of bytes for such an allocation would overflow size_type.
    ASSERT( a.max_size()*typename A::size_type(sizeof(T))>=a.max_size(), "max_size larger than reasonable" ); 

    // Test "a.construct(p,t)"
    int n = NumberOfFoo;
    typename A::pointer p = a.allocate(1);
    a.construct( p, cx );
    ASSERT( NumberOfFoo==n+1, "constructor for Foo not called?" );

    // Test "a.destroy(p)"
    a.destroy( p );
    ASSERT( NumberOfFoo==n, "destructor for Foo not called?" );
    a.deallocate(p,1);
}

#include "tbb/blocked_range.h"

#if _MSC_VER && !defined(__INTEL_COMPILER)
    // Workaround for erroneous "conditional expression is constant" warning in method check_allocate.
    #pragma warning (disable: 4127)
#endif

// A is an allocator for some type
template<typename A>
struct Body: NoAssign {
    static const size_t max_k = 100000;
    A &a;
    Body(A &a_) : a(a_) {}
    void check_allocate( typename A::pointer array[], size_t i, size_t t ) const
    {
        ASSERT(array[i] == 0, NULL);
        size_t size = i * (i&3);
        array[i] = i&1 ? a.allocate(size, array[i>>3]) : a.allocate(size);
        ASSERT(array[i] != 0, "allocator returned null");
        char* s = reinterpret_cast<char*>(reinterpret_cast<void*>(array[i]));
        for( size_t j=0; j<size*sizeof(typename A::value_type); ++j ) {
            if(is_zero_filling<typename A::template rebind<void>::other>::value)
                ASSERT( !s[j], NULL);
            s[j] = PseudoRandomValue(i, t);
        }
    }

    void check_deallocate( typename A::pointer array[], size_t i, size_t t ) const
    {
        ASSERT(array[i] != 0, NULL);
        size_t size = i * (i&3);
        char* s = reinterpret_cast<char*>(reinterpret_cast<void*>(array[i]));
        for( size_t j=0; j<size*sizeof(typename A::value_type); ++j )
            ASSERT( s[j] == PseudoRandomValue(i, t), "Thread safety test failed" );
        a.deallocate(array[i], size);
        array[i] = 0;
    }

    void operator()( size_t thread_id ) const {
        typename A::pointer array[256];

        for( size_t k=0; k<256; ++k )
            array[k] = 0;
        for( size_t k=0; k<max_k; ++k ) {
            size_t i = static_cast<unsigned char>(PseudoRandomValue(k,thread_id));
            if(!array[i]) check_allocate(array, i, thread_id);
            else check_deallocate(array, i, thread_id);
        }
        for( size_t k=0; k<256; ++k )
            if(array[k])
                check_deallocate(array, k, thread_id);
#if __TBBMALLOC_CALL_THREAD_SHUTDOWN
        __TBB_mallocThreadShutdownNotification();
#endif
    }
};

// A is an allocator for some type, and U is another type
template<typename U, typename A>
void Test(A &a) {
    typename A::template rebind<U>::other b(a);
    TestBasic<U>(b);
    TestBasic<typename A::value_type>(a);

    // thread safety
    NativeParallelFor( 4, Body<A>(a) );
    ASSERT( NumberOfFoo==0, "Allocate/deallocate count mismatched" );
 
    ASSERT( a==b, NULL );
    ASSERT( !(a!=b), NULL );
}

template<typename Allocator>
int TestMain(const Allocator &a = Allocator() ) {
    NumberOfFoo = 0;
    typename Allocator::template rebind<Foo<char,1> >::other a1(a);
    typename Allocator::template rebind<Foo<double,1> >::other a2(a);
    Test<Foo<int,17> >( a1 );
    Test<Foo<float,23> >( a2 );
    return 0;
}

