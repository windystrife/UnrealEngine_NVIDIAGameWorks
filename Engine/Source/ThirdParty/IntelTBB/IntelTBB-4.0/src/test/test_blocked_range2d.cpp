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

#include "tbb/blocked_range2d.h"
#include "harness_assert.h"

// First test as much as we can without including other headers.
// Doing so should catch problems arising from failing to include headers.

template<typename Tag>
class AbstractValueType {
    AbstractValueType() {}
    int value;
public: 
    template<typename OtherTag>
    friend AbstractValueType<OtherTag> MakeAbstractValueType( int i );

    template<typename OtherTag>
    friend int GetValueOf( const AbstractValueType<OtherTag>& v ) ;
};

template<typename Tag>
AbstractValueType<Tag> MakeAbstractValueType( int i ) {
    AbstractValueType<Tag> x;
    x.value = i;
    return x;
}

template<typename Tag>
int GetValueOf( const AbstractValueType<Tag>& v ) {return v.value;}

template<typename Tag>
bool operator<( const AbstractValueType<Tag>& u, const AbstractValueType<Tag>& v ) {
    return GetValueOf(u)<GetValueOf(v);
}

template<typename Tag>
std::size_t operator-( const AbstractValueType<Tag>& u, const AbstractValueType<Tag>& v ) {
    return GetValueOf(u)-GetValueOf(v);
}

template<typename Tag>
AbstractValueType<Tag> operator+( const AbstractValueType<Tag>& u, std::size_t offset ) {
    return MakeAbstractValueType<Tag>(GetValueOf(u)+int(offset));
}

struct RowTag {};
struct ColTag {};

static void SerialTest() {
    typedef AbstractValueType<RowTag> row_type;
    typedef AbstractValueType<ColTag> col_type;
    typedef tbb::blocked_range2d<row_type,col_type> range_type;
    for( int rowx=-10; rowx<10; ++rowx ) {
        for( int rowy=rowx; rowy<10; ++rowy ) {
            row_type rowi = MakeAbstractValueType<RowTag>(rowx);
            row_type rowj = MakeAbstractValueType<RowTag>(rowy);
            for( int rowg=1; rowg<10; ++rowg ) {
                for( int colx=-10; colx<10; ++colx ) {
                    for( int coly=colx; coly<10; ++coly ) {
                        col_type coli = MakeAbstractValueType<ColTag>(colx);
                        col_type colj = MakeAbstractValueType<ColTag>(coly);
                        for( int colg=1; colg<10; ++colg ) {
                            range_type r( rowi, rowj, rowg, coli, colj, colg );
                            AssertSameType( r.is_divisible(), true );
                            AssertSameType( r.empty(), true );
                            AssertSameType( static_cast<range_type::row_range_type::const_iterator*>(0), static_cast<row_type*>(0) );
                            AssertSameType( static_cast<range_type::col_range_type::const_iterator*>(0), static_cast<col_type*>(0) );
                            AssertSameType( r.rows(), tbb::blocked_range<row_type>( rowi, rowj, 1 ));
                            AssertSameType( r.cols(), tbb::blocked_range<col_type>( coli, colj, 1 ));
                            ASSERT( r.empty()==(rowx==rowy||colx==coly), NULL );
                            ASSERT( r.is_divisible()==(rowy-rowx>rowg||coly-colx>colg), NULL );
                            if( r.is_divisible() ) {
                                range_type r2(r,tbb::split());
                                if( GetValueOf(r2.rows().begin())==GetValueOf(r.rows().begin()) ) {
                                    ASSERT( GetValueOf(r2.rows().end())==GetValueOf(r.rows().end()), NULL );
                                    ASSERT( GetValueOf(r2.cols().begin())==GetValueOf(r.cols().end()), NULL );
                                } else {
                                    ASSERT( GetValueOf(r2.cols().end())==GetValueOf(r.cols().end()), NULL );
                                    ASSERT( GetValueOf(r2.rows().begin())==GetValueOf(r.rows().end()), NULL );
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

#include "tbb/parallel_for.h"
#include "harness.h"

const int N = 1<<10;

unsigned char Array[N][N];

struct Striker {
   // Note: we use <int> here instead of <long> in order to test for problems similar to Quad 407676
    void operator()( const tbb::blocked_range2d<int>& r ) const {
        for( tbb::blocked_range<int>::const_iterator i=r.rows().begin(); i!=r.rows().end(); ++i )
            for( tbb::blocked_range<int>::const_iterator j=r.cols().begin(); j!=r.cols().end(); ++j )
                ++Array[i][j];
    }
};

void ParallelTest() {
    for( int i=0; i<N; i=i<3 ? i+1 : i*3 ) {
        for( int j=0; j<N; j=j<3 ? j+1 : j*3 ) {
            const tbb::blocked_range2d<int> r( 0, i, 7, 0, j, 5 );
            tbb::parallel_for( r, Striker() );
            for( int k=0; k<N; ++k ) {
                for( int l=0; l<N; ++l ) {
                    ASSERT( Array[k][l]==(k<i && l<j), NULL );
                    Array[k][l] = 0;
                }
            }   
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
