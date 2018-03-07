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
#include "tbb/task_scheduler_init.h"
#include "tbb/tick_count.h"

#include <cstdio>

#define N 1000
#define C 10

template< typename T >
void spin_try_get( tbb::flow::buffer_node<T> &b, T &value ) {
    while ( b.try_get(value) != true ) {}
}

template< typename T >
void check_item( T* count_value, T &value ) {
    count_value[value / N] += value % N;
}

template< typename T >
struct parallel_puts : NoAssign {

    tbb::flow::buffer_node<T> &my_b;

    parallel_puts( tbb::flow::buffer_node<T> &b ) : my_b(b) {}

    void operator()(int i) const {
        for (int j = 0; j < N; ++j) {
            bool msg = my_b.try_put( T(N*i + j) );
            ASSERT( msg == true, NULL );
        }
    }
};

template< typename T >
struct touches {

    bool **my_touches;
    int my_num_threads;

    touches( int num_threads ) : my_num_threads(num_threads) {
        my_touches = new bool* [my_num_threads]; 
        for ( int p = 0; p < my_num_threads; ++p) {
            my_touches[p] = new bool[N]; 
            for ( int n = 0; n < N; ++n)
                my_touches[p][n] = false;
        }
    }

    ~touches() {
        for ( int p = 0; p < my_num_threads; ++p) {
            delete [] my_touches[p];
        }
        delete [] my_touches;
    }

    bool check( T v ) {
        ASSERT ( my_touches[v/N][v%N] == false, NULL);
        my_touches[v/N][v%N] = true; 
        return true;
    }

    bool validate_touches() {
        for ( int p = 0; p < my_num_threads; ++p) {
            for ( int n = 0; n < N; ++n) {
                ASSERT ( my_touches[p][n] == true, NULL);
            }
        }
        return true;
    }
};

template< typename T >
struct parallel_gets : NoAssign {

    tbb::flow::buffer_node<T> &my_b;
    touches<T> &my_touches;

    parallel_gets( tbb::flow::buffer_node<T> &b, touches<T> &t) : my_b(b), my_touches(t) {}

    void operator()(int) const {
        for (int j = 0; j < N; ++j) {
            T v;
            spin_try_get( my_b, v );
            my_touches.check( v );
        }
    }

};

template< typename T >
struct parallel_put_get : NoAssign {

    tbb::flow::buffer_node<T> &my_b;
    touches<T> &my_touches;

    parallel_put_get( tbb::flow::buffer_node<T> &b, touches<T> &t ) : my_b(b), my_touches(t) {}

    void operator()(int tid) const {

        for ( int i = 0; i < N; i+=C ) {
            int j_end = ( N < i + C ) ? N : i + C; 
            // dump about C values into the buffer
            for ( int j = i; j < j_end; ++j ) {
                ASSERT( my_b.try_put( T (N*tid + j ) ) == true, NULL );
            }
            // receiver about C values from the buffer
            for ( int j = i; j < j_end; ++j ) {
                T v;
                spin_try_get( my_b, v );
                my_touches.check( v );
            }
        }
    }

};

//
// Tests
//
// Item can be reserved, released, consumed ( single serial receiver )
//
template< typename T >
int test_reservation() {
    tbb::flow::graph g;
    T bogus_value(-1);

    // Simple tests
    tbb::flow::buffer_node<T> b(g);

    b.try_put(T(1));
    b.try_put(T(2));
    b.try_put(T(3));
 
    T v, vsum;
    ASSERT( b.try_reserve(v) == true, NULL );
    ASSERT( b.try_release() == true, NULL );
    v = bogus_value;
    g.wait_for_all();
    ASSERT( b.try_reserve(v) == true, NULL );
    ASSERT( b.try_consume() == true, NULL );
    vsum += v;
    v = bogus_value;
    g.wait_for_all();
 
    ASSERT( b.try_get(v) == true, NULL );
    vsum += v;
    v = bogus_value;
    g.wait_for_all();
    
    ASSERT( b.try_reserve(v) == true, NULL );
    ASSERT( b.try_release() == true, NULL );
    v = bogus_value;
    g.wait_for_all();
    ASSERT( b.try_reserve(v) == true, NULL );
    ASSERT( b.try_consume() == true, NULL );
    vsum += v;
    ASSERT( vsum == T(6), NULL);
    v = bogus_value;
    g.wait_for_all();

    return 0;
}

//
// Tests
//
// multilpe parallel senders, items in arbitrary order
// multilpe parallel senders, multiple parallel receivers, items in arbitrary order and all items received
//   * overlapped puts / gets
//   * all puts finished before any getS
//
template< typename T >
int test_parallel(int num_threads) {
    tbb::flow::graph g;
    tbb::flow::buffer_node<T> b(g);
    tbb::flow::buffer_node<T> b2(g);
    tbb::flow::buffer_node<T> b3(g);
    T bogus_value(-1);
    T j = bogus_value;

    NativeParallelFor( num_threads, parallel_puts<T>(b) );

    T *next_value = new T[num_threads];
    for (int tid = 0; tid < num_threads; ++tid) next_value[tid] = T(0);

    for (int i = 0; i < num_threads * N; ++i ) {
        spin_try_get( b, j );
        check_item( next_value, j );
        j = bogus_value;
    }
    for (int tid = 0; tid < num_threads; ++tid)  {
        ASSERT( next_value[tid] == T((N*(N-1))/2), NULL );
    }

    j = bogus_value;
    g.wait_for_all();
    ASSERT( b.try_get( j ) == false, NULL );
    ASSERT( j == bogus_value, NULL );

    NativeParallelFor( num_threads, parallel_puts<T>(b) );

    {
        touches< T > t( num_threads );
        NativeParallelFor( num_threads, parallel_gets<T>(b, t) );
        g.wait_for_all();
        ASSERT( t.validate_touches(), NULL );
    }
    j = bogus_value;
    ASSERT( b.try_get( j ) == false, NULL );
    ASSERT( j == bogus_value, NULL );

    g.wait_for_all();
    {
        touches< T > t( num_threads );
        NativeParallelFor( num_threads, parallel_put_get<T>(b, t) );
        g.wait_for_all();
        ASSERT( t.validate_touches(), NULL );
    }
    j = bogus_value;
    ASSERT( b.try_get( j ) == false, NULL );
    ASSERT( j == bogus_value, NULL );

    tbb::flow::make_edge( b, b2 );
    tbb::flow::make_edge( b2, b3 );

    NativeParallelFor( num_threads, parallel_puts<T>(b) );
    {
        touches< T > t( num_threads );
        NativeParallelFor( num_threads, parallel_gets<T>(b3, t) );
        g.wait_for_all();
        ASSERT( t.validate_touches(), NULL );
    }
    j = bogus_value;
    g.wait_for_all();
    ASSERT( b.try_get( j ) == false, NULL );
    g.wait_for_all();
    ASSERT( b2.try_get( j ) == false, NULL );
    g.wait_for_all();
    ASSERT( b3.try_get( j ) == false, NULL );
    ASSERT( j == bogus_value, NULL );

    // test copy constructor
    ASSERT( b.remove_successor( b2 ), NULL );
    // fill up b:
    NativeParallelFor( num_threads, parallel_puts<T>(b) );
    // copy b:
    tbb::flow::buffer_node<T> b_copy(b);

    // b_copy should be empty
    j = bogus_value;
    g.wait_for_all();
    ASSERT( b_copy.try_get( j ) == false, NULL );

    // hook them together:
    ASSERT( b.register_successor(b_copy) == true, NULL );
    // try to get content from b_copy
    {
        touches< T > t( num_threads );
        NativeParallelFor( num_threads, parallel_gets<T>(b_copy, t) );
        g.wait_for_all();
        ASSERT( t.validate_touches(), NULL );
    }
    // now both should be empty
    j = bogus_value;
    g.wait_for_all();
    ASSERT( b.try_get( j ) == false, NULL );
    g.wait_for_all();
    ASSERT( b_copy.try_get( j ) == false, NULL );
    ASSERT( j == bogus_value, NULL );

    return 0;
}

//
// Tests
//
// Predecessors cannot be registered
// Empty buffer rejects item requests 
// Single serial sender, items in arbitrary order
// Chained buffers ( 2 & 3 ), single sender, items at last buffer in arbitrary order
//

template< typename T >
int test_serial() {
    tbb::flow::graph g;
    T bogus_value(-1);

    tbb::flow::buffer_node<T> b(g);
    tbb::flow::buffer_node<T> b2(g);
    T j = bogus_value;

    //
    // Rejects attempts to add / remove predecessor
    // Rejects request from empty buffer
    //
    ASSERT( b.register_predecessor( b2 ) == false, NULL );
    ASSERT( b.remove_predecessor( b2 ) == false, NULL );
    ASSERT( b.try_get( j ) == false, NULL );
    ASSERT( j == bogus_value, NULL );

    //
    // Simple puts and gets
    //

    for (int i = 0; i < N; ++i) {
        bool msg = b.try_put( T(i) );
        ASSERT( msg == true, NULL );
    }

    T vsum = T(0);
    for (int i = 0; i < N; ++i) {
        j = bogus_value;
        spin_try_get( b, j );
        vsum += j;
    }
    ASSERT( vsum == (N*(N-1))/2, NULL);
    j = bogus_value;
    g.wait_for_all();
    ASSERT( b.try_get( j ) == false, NULL );
    ASSERT( j == bogus_value, NULL );

    tbb::flow::make_edge(b, b2);

    vsum = T(0);
    for (int i = 0; i < N; ++i) {
        bool msg = b.try_put( T(i) );
        ASSERT( msg == true, NULL );
    }

    for (int i = 0; i < N; ++i) {
        j = bogus_value;
        spin_try_get( b2, j );
        vsum += j;
    }
    ASSERT( vsum == (N*(N-1))/2, NULL);
    j = bogus_value;
    g.wait_for_all();
    ASSERT( b.try_get( j ) == false, NULL );
    g.wait_for_all();
    ASSERT( b2.try_get( j ) == false, NULL );
    ASSERT( j == bogus_value, NULL );

    tbb::flow::remove_edge(b, b2);
    ASSERT( b.try_put( 1 ) == true, NULL );
    g.wait_for_all();
    ASSERT( b2.try_get( j ) == false, NULL );
    ASSERT( j == bogus_value, NULL );
    g.wait_for_all();
    ASSERT( b.try_get( j ) == true, NULL );
    ASSERT( j == 1, NULL );

    tbb::flow::buffer_node<T> b3(g);
    tbb::flow::make_edge( b, b2 );
    tbb::flow::make_edge( b2, b3 );

    vsum = T(0);
    for (int i = 0; i < N; ++i) {
        bool msg = b.try_put( T(i) );
        ASSERT( msg == true, NULL );
    }

    for (int i = 0; i < N; ++i) {
        j = bogus_value;
        spin_try_get( b3, j );
        vsum += j;
    }
    ASSERT( vsum == (N*(N-1))/2, NULL);
    j = bogus_value;
    g.wait_for_all();
    ASSERT( b.try_get( j ) == false, NULL );
    g.wait_for_all();
    ASSERT( b2.try_get( j ) == false, NULL );
    g.wait_for_all();
    ASSERT( b3.try_get( j ) == false, NULL );
    ASSERT( j == bogus_value, NULL );

    tbb::flow::remove_edge(b, b2);
    ASSERT( b.try_put( 1 ) == true, NULL );
    g.wait_for_all();
    ASSERT( b2.try_get( j ) == false, NULL );
    ASSERT( j == bogus_value, NULL );
    g.wait_for_all();
    ASSERT( b3.try_get( j ) == false, NULL );
    ASSERT( j == bogus_value, NULL );
    g.wait_for_all();
    ASSERT( b.try_get( j ) == true, NULL );
    ASSERT( j == 1, NULL );

    return 0;
}

int TestMain() { 
    tbb::tick_count start = tbb::tick_count::now(), stop;
    for (int p = 1; p < 4; ++p) {
        tbb::task_scheduler_init init(p);
        test_serial<int>();
        test_parallel<int>(p);
    } 
    stop = tbb::tick_count::now();
    REMARK("Buffer_Node Time=%6.6f\n", (stop-start).seconds());
    return Harness::Done;
}
