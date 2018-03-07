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

#include "tbb/tbb_config.h"

#if __TBB_GCC_WARNING_SUPPRESSION_PRESENT
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

//! Wrapper around T where all members are private.
/** Used to prove that aligned_space<T,N> never calls member of T. */
template<typename T>
class Minimal {
    Minimal();
    Minimal( Minimal& min );
    ~Minimal();
    void operator=( const Minimal& );
    T pad;
    template<typename U>
    friend void AssignToCheckAlignment( Minimal<U>& dst, const Minimal<U>& src ) ;
};

template<typename T>
void AssignToCheckAlignment( Minimal<T>& dst, const Minimal<T>& src ) {
    dst.pad = src.pad;
}

#include "tbb/aligned_space.h"
#include "harness_assert.h"

static bool SpaceWasted;

template<typename U, size_t N>
void TestAlignedSpaceN() {
    typedef Minimal<U> T;
    struct {
        //! Pad byte increases chance that subsequent member will be misaligned if there is a problem.
        char pad;
        tbb::aligned_space<T ,N> space;
    } x;
    AssertSameType( static_cast< T *>(0), x.space.begin() );
    AssertSameType( static_cast< T *>(0), x.space.end() );
    ASSERT( reinterpret_cast<void *>(x.space.begin())==reinterpret_cast< void *>(&x.space), NULL );
    ASSERT( x.space.end()-x.space.begin()==N, NULL );
    ASSERT( reinterpret_cast<void *>(x.space.begin())>=reinterpret_cast< void *>(&x.space), NULL );
    ASSERT( x.space.end()<=reinterpret_cast< T *>(&x.space+1), NULL );
    // Though not required, a good implementation of aligned_space<T,N> does not use any more space than a T[N].
    SpaceWasted |= sizeof(x.space)!=sizeof(T)*N;
    for( size_t k=1; k<N; ++k )
        AssignToCheckAlignment( x.space.begin()[k-1], x.space.begin()[k] );
}

static void PrintSpaceWastingWarning( const char* type_name );

#include <typeinfo>

template<typename T>
void TestAlignedSpace() {
    SpaceWasted = false;
    TestAlignedSpaceN<T,1>();
    TestAlignedSpaceN<T,2>();
    TestAlignedSpaceN<T,3>();
    TestAlignedSpaceN<T,4>();
    TestAlignedSpaceN<T,5>();
    TestAlignedSpaceN<T,6>();
    TestAlignedSpaceN<T,7>();
    TestAlignedSpaceN<T,8>();
    if( SpaceWasted )
        PrintSpaceWastingWarning( typeid(T).name() );
}

#define HARNESS_NO_PARSE_COMMAND_LINE 1
#include "harness.h"

#include "harness_m128.h"

int TestMain () {
    TestAlignedSpace<char>();
    TestAlignedSpace<short>();
    TestAlignedSpace<int>();
    TestAlignedSpace<float>();
    TestAlignedSpace<double>();
    TestAlignedSpace<long double>();
    TestAlignedSpace<size_t>();
#if HAVE_m128
    TestAlignedSpace<__m128>();
#endif
#if HAVE_m256
    if (have_AVX()) TestAlignedSpace<__m256>();
#endif
    return Harness::Done;
}

static void PrintSpaceWastingWarning( const char* type_name ) {
    REPORT("Consider rewriting aligned_space<%s,N> to waste less space\n", type_name ); 
}

