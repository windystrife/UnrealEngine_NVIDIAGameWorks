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

#include "harness_graph.h"

#include "tbb/task_scheduler_init.h"

const int N = 1000;

template< typename T >
class test_push_receiver : public tbb::flow::receiver<T> {

    tbb::atomic<int> my_counters[N];

public:

    test_push_receiver() {  
        for (int i = 0; i < N; ++i ) 
            my_counters[i] = 0;
    }

    int get_count( int i ) {
       int v = my_counters[i]; 
       return v;
    }

    bool try_put( const T &v ) {
       int i = (int)v;
       ++my_counters[i];
       return true;
    }

    /*override*/void reset_receiver() {}
};

template< typename T >
class source_body {

   tbb::atomic<int> my_count;
 
public:

   source_body() { my_count = 0; }

   bool operator()( T &v ) {
      v = (T)my_count.fetch_and_increment();
      if ( (int)v < N )
         return true;
      else
         return false;
   } 

};

template< typename T >
class function_body {

    tbb::atomic<int> *my_counters;

public:

    function_body( tbb::atomic<int> *counters ) : my_counters(counters) {  
        for (int i = 0; i < N; ++i ) 
            my_counters[i] = 0;
    }

    bool operator()( T v ) {
        ++my_counters[(int)v];
        return true;
    } 

};

template< typename T >
void test_single_dest() {

   // push only
   tbb::flow::graph g;
   tbb::flow::source_node<T> src(g, source_body<T>() );
   test_push_receiver<T> dest;
   tbb::flow::make_edge( src, dest );
   g.wait_for_all();
   for (int i = 0; i < N; ++i ) {
       ASSERT( dest.get_count(i) == 1, NULL ); 
   }

   // push only
   tbb::atomic<int> counters3[N];
   tbb::flow::source_node<T> src3(g, source_body<T>() );
   function_body<T> b3( counters3 );
   tbb::flow::function_node<T,bool> dest3(g, tbb::flow::unlimited, b3 );
   tbb::flow::make_edge( src3, dest3 );
   g.wait_for_all();
   for (int i = 0; i < N; ++i ) {
       int v = counters3[i];
       ASSERT( v == 1, NULL ); 
   }

   // push & pull 
   tbb::flow::source_node<T> src2(g, source_body<T>() );
   tbb::atomic<int> counters2[N];
   function_body<T> b2( counters2 );
   tbb::flow::function_node<T,bool> dest2(g, tbb::flow::serial, b2 );
   tbb::flow::make_edge( src2, dest2 );
   g.wait_for_all();
   for (int i = 0; i < N; ++i ) {
       int v = counters2[i];
       ASSERT( v == 1, NULL ); 
   }

   // test copy constructor
   tbb::flow::source_node<T> src_copy(src);
   test_push_receiver<T> dest_c;
   ASSERT( src_copy.register_successor(dest_c), NULL );
   g.wait_for_all();
   for (int i = 0; i < N; ++i ) {
       ASSERT( dest_c.get_count(i) == 1, NULL ); 
   }
}

int TestMain() { 
    if( MinThread<1 ) {
        REPORT("number of threads must be positive\n");
        exit(1);
    }
    for ( int p = MinThread; p < MaxThread; ++p ) {
        tbb::task_scheduler_init init(p);
        test_single_dest<int>();
        test_single_dest<float>();
    }
    return Harness::Done;
}

