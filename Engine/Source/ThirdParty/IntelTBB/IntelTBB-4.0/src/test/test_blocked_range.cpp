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

#include "tbb/blocked_range.h"
#include "harness_assert.h"

// First test as much as we can without including other headers.
// Doing so should catch problems arising from failing to include headers.

class AbstractValueType {
    AbstractValueType() {}
    int value;
public: 
    friend AbstractValueType MakeAbstractValueType( int i );
    friend int GetValueOf( const AbstractValueType& v ) {return v.value;}
};

AbstractValueType MakeAbstractValueType( int i ) {
    AbstractValueType x;
    x.value = i;
    return x;
}

std::size_t operator-( const AbstractValueType& u, const AbstractValueType& v ) {
    return GetValueOf(u)-GetValueOf(v);
}

bool operator<( const AbstractValueType& u, const AbstractValueType& v ) {
    return GetValueOf(u)<GetValueOf(v);
}

AbstractValueType operator+( const AbstractValueType& u, std::size_t offset ) {
    return MakeAbstractValueType(GetValueOf(u)+int(offset));
}

static void SerialTest() {
    for( int x=-10; x<10; ++x )
        for( int y=-10; y<10; ++y ) {
            AbstractValueType i = MakeAbstractValueType(x);
            AbstractValueType j = MakeAbstractValueType(y);
            for( std::size_t k=1; k<10; ++k ) {
                typedef tbb::blocked_range<AbstractValueType> range_type;
                range_type r( i, j, k );
                AssertSameType( r.empty(), true );
                AssertSameType( range_type::size_type(), std::size_t() );
                AssertSameType( static_cast<range_type::const_iterator*>(0), static_cast<AbstractValueType*>(0) );
                AssertSameType( r.begin(), MakeAbstractValueType(0) );
                AssertSameType( r.end(), MakeAbstractValueType(0) );
                ASSERT( r.empty()==(y<=x), NULL );
                ASSERT( r.grainsize()==k, NULL );
                if( x<=y ) {
                    AssertSameType( r.is_divisible(), true );
                    ASSERT( r.is_divisible()==(std::size_t(y-x)>k), NULL );
                    ASSERT( r.size()==std::size_t(y-x), NULL );
                    if( r.is_divisible() ) {
                        tbb::blocked_range<AbstractValueType> r2(r,tbb::split());
                        ASSERT( GetValueOf(r.begin())==x, NULL );
                        ASSERT( GetValueOf(r.end())==GetValueOf(r2.begin()), NULL );
                        ASSERT( GetValueOf(r2.end())==y, NULL );
                        ASSERT( r.grainsize()==k, NULL );
                        ASSERT( r2.grainsize()==k, NULL );
                    }
                }
            }
        }
}

#include "tbb/parallel_for.h"
#include "harness.h"

const int N = 1<<22;

unsigned char Array[N];

struct Striker {
    // Note: we use <int> here instead of <long> in order to test for Quad 407676
    void operator()( const tbb::blocked_range<int>& r ) const {
        for( tbb::blocked_range<int>::const_iterator i=r.begin(); i!=r.end(); ++i )
            ++Array[i];
    }
};

void ParallelTest() {
    for( int i=0; i<N; i=i<3 ? i+1 : i*3 ) {
        const tbb::blocked_range<int> r( 0, i, 10 );
        tbb::parallel_for( r, Striker() );
        for( int k=0; k<N; ++k ) {
            ASSERT( Array[k]==(k<i), NULL );
            Array[k] = 0;
        }
    }
}

#include "tbb/task_scheduler_init.h"

int TestMain () {
    SerialTest();
    for( int p=MinThread; p<=MaxThread; ++p ) {
        tbb::task_scheduler_init init(p);
        ParallelTest();
    }
    return Harness::Done;
}
