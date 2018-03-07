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

#include "harness.h"
#include "tbb/flow_graph.h"

#include "tbb/atomic.h"

const int N = 1000;
const int R = 4;

class int_convertable_type : private NoAssign {

   int my_value;

public:

   int_convertable_type( int v ) : my_value(v) {}
   operator int() const { return my_value; }

};


template< typename T >
class counting_array_receiver : public tbb::flow::receiver<T> {

    tbb::atomic<size_t> my_counters[N];

public:

    counting_array_receiver() {
        for (int i = 0; i < N; ++i )
           my_counters[i] = 0;
    }

    size_t operator[]( int i ) {
        size_t v = my_counters[i];
        return v;
    }

    /* override */ bool try_put( const T &v ) {
        ++my_counters[(int)v];
        return true;
    }

    /*override*/void reset_receiver() { }

};

template< typename T >
void test_serial_broadcasts() {

    tbb::flow::graph g;
    tbb::flow::broadcast_node<T> b(g);
    
    for ( int num_receivers = 1; num_receivers < R; ++num_receivers ) {
        counting_array_receiver<T> *receivers = new counting_array_receiver<T>[num_receivers];

        for ( int r = 0; r < num_receivers; ++r ) {
            tbb::flow::make_edge( b, receivers[r] );
        } 

        for (int n = 0; n < N; ++n ) {
            ASSERT( b.try_put( (T)n ), NULL );
        }

        for ( int r = 0; r < num_receivers; ++r ) {
            for (int n = 0; n < N; ++n ) {
                ASSERT( receivers[r][n] == 1, NULL );
            }
            tbb::flow::remove_edge( b, receivers[r] );
        } 
        ASSERT( b.try_put( (T)0 ), NULL );
        for ( int r = 0; r < num_receivers; ++r ) 
            ASSERT( receivers[0][0] == 1, NULL ) ;

        delete [] receivers;

    }

}

template< typename T >
class native_body : private NoAssign {

    tbb::flow::broadcast_node<T> &my_b;

public:

    native_body( tbb::flow::broadcast_node<T> &b ) : my_b(b) {} 

    void operator()(int) const {
        for (int n = 0; n < N; ++n ) {
            ASSERT( my_b.try_put( (T)n ), NULL );
        }
    }
 
};

template< typename T >
void run_parallel_broadcasts(int p, tbb::flow::broadcast_node<T>& b) {
    for ( int num_receivers = 1; num_receivers < R; ++num_receivers ) {
        counting_array_receiver<T> *receivers = new counting_array_receiver<T>[num_receivers];

        for ( int r = 0; r < num_receivers; ++r ) {
            tbb::flow::make_edge( b, receivers[r] );
        } 

        NativeParallelFor( p, native_body<T>( b ) );

        for ( int r = 0; r < num_receivers; ++r ) {
            for (int n = 0; n < N; ++n ) {
                ASSERT( (int)receivers[r][n] == p, NULL );
            }
            tbb::flow::remove_edge( b, receivers[r] );
        } 
        ASSERT( b.try_put( (T)0 ), NULL );
        for ( int r = 0; r < num_receivers; ++r ) 
            ASSERT( (int)receivers[r][0] == p, NULL ) ;

        delete [] receivers;

    }
}

template< typename T >
void test_parallel_broadcasts(int p) {

    tbb::flow::graph g;
    tbb::flow::broadcast_node<T> b(g);
    run_parallel_broadcasts(p, b);
    
    // test copy constructor
    tbb::flow::broadcast_node<T> b_copy(b);
    run_parallel_broadcasts(p, b_copy);
}

int TestMain() { 
    if( MinThread<1 ) {
        REPORT("number of threads must be positive\n");
        exit(1);
    }

   test_serial_broadcasts<int>();
   test_serial_broadcasts<float>();
   test_serial_broadcasts<int_convertable_type>();

   for( int p=MinThread; p<=MaxThread; ++p ) {
       test_parallel_broadcasts<int>(p);
       test_parallel_broadcasts<float>(p);
       test_parallel_broadcasts<int_convertable_type>(p);
   }
   return Harness::Done;
}
