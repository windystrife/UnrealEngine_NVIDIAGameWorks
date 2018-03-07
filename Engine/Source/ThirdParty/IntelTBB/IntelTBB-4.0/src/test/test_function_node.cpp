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
#include "tbb/spin_mutex.h"

tbb::spin_mutex global_mutex;

#define N 100
#define MAX_NODES 4

//! Performs test on function nodes with limited concurrency and buffering
/** Theses tests check:
    1) that the number of executing copies never exceed the concurreny limit
    2) that the node never rejects
    3) that no items are lost
    and 4) all of this happens even if there are multiple predecessors and successors
*/

template< typename InputType >
struct parallel_put_until_limit : private NoAssign {

    harness_counting_sender<InputType> *my_senders;

    parallel_put_until_limit( harness_counting_sender<InputType> *senders ) : my_senders(senders) {}

    void operator()( int i ) const  {
        if ( my_senders ) {
            my_senders[i].try_put_until_limit();
        }
    }

};
     
template< typename InputType, typename OutputType, typename Body >
void buffered_levels( size_t concurrency, Body body ) {

   // Do for lc = 1 to concurrency level
   for ( size_t lc = 1; lc <= concurrency; ++lc ) { 
   tbb::flow::graph g;

   // Set the execute_counter back to zero in the harness
   harness_graph_executor<InputType, OutputType>::execute_count = 0;
   // Set the max allowed executors to lc.  There is a check in the functor to make sure this is never exceeded.
   harness_graph_executor<InputType, OutputType>::max_executors = lc;

   // Create the function_node with the appropriate concurreny level, and use default buffering
   tbb::flow::function_node< InputType, OutputType > exe_node( g, lc, body );
   
   //Create a vector of identical exe_nodes
   std::vector< tbb::flow::function_node< InputType, OutputType > > exe_vec(2, exe_node);

   for (size_t node_idx=0; node_idx<exe_vec.size(); ++node_idx) {
   // For num_receivers = 1 to MAX_NODES
   for (size_t num_receivers = 1; num_receivers <= MAX_NODES; ++num_receivers ) {
        // Create num_receivers counting receivers and connect the exe_vec[node_idx] to them.
        harness_mapped_receiver<OutputType> *receivers = new harness_mapped_receiver<OutputType>[num_receivers];
        for (size_t r = 0; r < num_receivers; ++r ) {
            tbb::flow::make_edge( exe_vec[node_idx], receivers[r] );
        }

        // Do the test with varying numbers of senders
        harness_counting_sender<InputType> *senders = NULL;
        for (size_t num_senders = 1; num_senders <= MAX_NODES; ++num_senders ) {
            // Create num_senders senders, set there message limit each to N, and connect them to the exe_vec[node_idx]
            senders = new harness_counting_sender<InputType>[num_senders];
            for (size_t s = 0; s < num_senders; ++s ) {
               senders[s].my_limit = N;
               tbb::flow::make_edge( senders[s], exe_vec[node_idx] );
            }

            // Initialize the receivers so they know how many senders and messages to check for
            for (size_t r = 0; r < num_receivers; ++r ) {
                 receivers[r].initialize_map( N, num_senders ); 
            }

            // Do the test
            NativeParallelFor( (int)num_senders, parallel_put_until_limit<InputType>(senders) );
            g.wait_for_all();

            // cofirm that each sender was requested from N times 
            for (size_t s = 0; s < num_senders; ++s ) {
                size_t n = senders[s].my_received;
                ASSERT( n == N, NULL ); 
                ASSERT( senders[s].my_receiver == &exe_vec[node_idx], NULL );
            }
            // validate the receivers
            for (size_t r = 0; r < num_receivers; ++r ) {
                receivers[r].validate();
            }
            delete [] senders;
        }
        for (size_t r = 0; r < num_receivers; ++r ) {
            tbb::flow::remove_edge( exe_vec[node_idx], receivers[r] );
        }
        ASSERT( exe_vec[node_idx].try_put( InputType() ) == true, NULL );
        g.wait_for_all();
        for (size_t r = 0; r < num_receivers; ++r ) {
            // since it's detached, nothing should have changed
            receivers[r].validate();
        }
        delete [] receivers;
    }
    } 
    }
}

const size_t Offset = 123;
tbb::atomic<size_t> global_execute_count;

struct inc_functor {

    tbb::atomic<size_t> local_execute_count;
    inc_functor( ) { local_execute_count = 0; }
    inc_functor( const inc_functor &f ) { local_execute_count = f.local_execute_count; }

    int operator()( int i ) {
       ++global_execute_count;
       ++local_execute_count;
       return i; 
    }

};

template< typename InputType, typename OutputType >
void buffered_levels_with_copy( size_t concurrency ) {

    // Do for lc = 1 to concurrency level
    for ( size_t lc = 1; lc <= concurrency; ++lc ) { 
        tbb::flow::graph g;

        inc_functor cf;
        cf.local_execute_count = Offset;
        global_execute_count = Offset;
       
        tbb::flow::function_node< InputType, OutputType > exe_node( g, lc, cf );

        for (size_t num_receivers = 1; num_receivers <= MAX_NODES; ++num_receivers ) {
           harness_mapped_receiver<OutputType> *receivers = new harness_mapped_receiver<OutputType>[num_receivers];
           for (size_t r = 0; r < num_receivers; ++r ) {
               tbb::flow::make_edge( exe_node, receivers[r] );
            }

            harness_counting_sender<InputType> *senders = NULL;
            for (size_t num_senders = 1; num_senders <= MAX_NODES; ++num_senders ) {
                senders = new harness_counting_sender<InputType>[num_senders];
                for (size_t s = 0; s < num_senders; ++s ) {
                    senders[s].my_limit = N;
                    tbb::flow::make_edge( senders[s], exe_node );
                }

                for (size_t r = 0; r < num_receivers; ++r ) {
                    receivers[r].initialize_map( N, num_senders ); 
                }

                NativeParallelFor( (int)num_senders, parallel_put_until_limit<InputType>(senders) );
                g.wait_for_all();

                for (size_t s = 0; s < num_senders; ++s ) {
                    size_t n = senders[s].my_received;
                    ASSERT( n == N, NULL ); 
                    ASSERT( senders[s].my_receiver == &exe_node, NULL );
                }
                for (size_t r = 0; r < num_receivers; ++r ) {
                    receivers[r].validate();
                }
                delete [] senders;
            }
            for (size_t r = 0; r < num_receivers; ++r ) {
                tbb::flow::remove_edge( exe_node, receivers[r] );
            }
            ASSERT( exe_node.try_put( InputType() ) == true, NULL );
            g.wait_for_all();
            for (size_t r = 0; r < num_receivers; ++r ) {
                receivers[r].validate();
            }
            delete [] receivers;
        }

        // validate that the local body matches the global execute_count and both are correct
        inc_functor body_copy = tbb::flow::copy_body<inc_functor>( exe_node );
        const size_t expected_count = N/2 * MAX_NODES * MAX_NODES * ( MAX_NODES + 1 ) + MAX_NODES + Offset; 
        size_t global_count = global_execute_count;
        size_t inc_count = body_copy.local_execute_count;
        ASSERT( global_count == expected_count && global_count == inc_count, NULL ); 
    }
}

template< typename InputType, typename OutputType >
void run_buffered_levels( int c ) {
    harness_graph_executor<InputType, OutputType, tbb::spin_mutex>::max_executors = c;
    #if __TBB_LAMBDAS_PRESENT
    buffered_levels<InputType,OutputType>( c, []( InputType i ) -> OutputType { return harness_graph_executor<InputType, OutputType, tbb::spin_mutex>::func(i); } );
    #endif
    buffered_levels<InputType,OutputType>( c, &harness_graph_executor<InputType, OutputType, tbb::spin_mutex>::func );
    buffered_levels<InputType,OutputType>( c, typename harness_graph_executor<InputType, OutputType, tbb::spin_mutex>::functor() );
    buffered_levels_with_copy<InputType,OutputType>( c );
}


//! Performs test on executable nodes with limited concurrency
/** Theses tests check:
    1) that the nodes will accepts puts up to the concurrency limit,
    2) the nodes do not exceed the concurrency limit even when run with more threads (this is checked in the harness_graph_executor), 
    3) the nodes will receive puts from multiple successors simultaneously,
    and 4) the nodes will send to multiple predecessors.
    There is no checking of the contents of the messages for corruption.
*/
     
template< typename InputType, typename OutputType, typename Body >
void concurrency_levels( size_t concurrency, Body body ) {

   for ( size_t lc = 1; lc <= concurrency; ++lc ) { 
   tbb::flow::graph g;
   harness_graph_executor<InputType, OutputType, tbb::spin_mutex>::execute_count = 0;

   tbb::flow::function_node< InputType, OutputType, tbb::flow::rejecting > exe_node( g, lc, body );

   for (size_t num_receivers = 1; num_receivers <= MAX_NODES; ++num_receivers ) {

        harness_counting_receiver<OutputType> *receivers = new harness_counting_receiver<OutputType>[num_receivers];

        for (size_t r = 0; r < num_receivers; ++r ) {
            tbb::flow::make_edge( exe_node, receivers[r] );
        }

        harness_counting_sender<InputType> *senders = NULL;

        for (size_t num_senders = 1; num_senders <= MAX_NODES; ++num_senders ) {
            {
                // lock m to prevent exe_node from finishing
                tbb::spin_mutex::scoped_lock l( harness_graph_executor< InputType, OutputType, tbb::spin_mutex >::mutex );

                // put to lc level, it will accept and then block at m
                for ( size_t c = 0 ; c < lc ; ++c ) {
                    ASSERT( exe_node.try_put( InputType() ) == true, NULL );
                }
                // it only accepts to lc level
                ASSERT( exe_node.try_put( InputType() ) == false, NULL );

                senders = new harness_counting_sender<InputType>[num_senders];
                for (size_t s = 0; s < num_senders; ++s ) {
                   // register a sender
                   senders[s].my_limit = N;
                   exe_node.register_predecessor( senders[s] );
                }

            } // release lock at end of scope, setting the exe node free to continue
            // wait for graph to settle down
            g.wait_for_all();

            // cofirm that each sender was requested from N times 
            for (size_t s = 0; s < num_senders; ++s ) {
                size_t n = senders[s].my_received;
                ASSERT( n == N, NULL ); 
                ASSERT( senders[s].my_receiver == &exe_node, NULL );
            }
            // cofirm that each receivers got N * num_senders + the initial lc puts
            for (size_t r = 0; r < num_receivers; ++r ) {
                size_t n = receivers[r].my_count;
                ASSERT( n == num_senders*N+lc, NULL );
                receivers[r].my_count = 0;
            }
            delete [] senders;
        }
        for (size_t r = 0; r < num_receivers; ++r ) {
            tbb::flow::remove_edge( exe_node, receivers[r] );
        }
        ASSERT( exe_node.try_put( InputType() ) == true, NULL );
        g.wait_for_all();
        for (size_t r = 0; r < num_receivers; ++r ) {
            ASSERT( int(receivers[r].my_count) == 0, NULL );
        }
        delete [] receivers;
    }

    }
}

template< typename InputType, typename OutputType >
void run_concurrency_levels( int c ) {
    harness_graph_executor<InputType, OutputType, tbb::spin_mutex>::max_executors = c;
    #if __TBB_LAMBDAS_PRESENT
    concurrency_levels<InputType,OutputType>( c, []( InputType i ) -> OutputType { return harness_graph_executor<InputType, OutputType, tbb::spin_mutex>::func(i); } );
    #endif
    concurrency_levels<InputType,OutputType>( c, &harness_graph_executor<InputType, OutputType, tbb::spin_mutex>::func );
    concurrency_levels<InputType,OutputType>( c, typename harness_graph_executor<InputType, OutputType, tbb::spin_mutex>::functor() );
}


struct empty_no_assign { 
   empty_no_assign() {}
   empty_no_assign( int ) {}
   operator int() { return 0; }
};

template< typename InputType >
struct parallel_puts : private NoAssign {

    tbb::flow::receiver< InputType > * const my_exe_node;

    parallel_puts( tbb::flow::receiver< InputType > &exe_node ) : my_exe_node(&exe_node) {}

    void operator()( int ) const  {
        for ( int i = 0; i < N; ++i ) {
            // the nodes will accept all puts
            ASSERT( my_exe_node->try_put( InputType() ) == true, NULL );
        }
    }

};

//! Performs test on executable nodes with unlimited concurrency
/** Theses tests check:
    1) that the nodes will accept all puts
    2) the nodes will receive puts from multiple predecessors simultaneously,
    and 3) the nodes will send to multiple successors.
    There is no checking of the contents of the messages for corruption.
*/

template< typename InputType, typename OutputType, typename Body >
void unlimited_concurrency( Body body ) {

    for (int p = 1; p < 2*MaxThread; ++p) {
        tbb::flow::graph g;
        tbb::flow::function_node< InputType, OutputType, tbb::flow::rejecting > exe_node( g, tbb::flow::unlimited, body );

        for (size_t num_receivers = 1; num_receivers <= MAX_NODES; ++num_receivers ) {

            harness_counting_receiver<OutputType> *receivers = new harness_counting_receiver<OutputType>[num_receivers];
            harness_graph_executor<InputType, OutputType>::execute_count = 0;

            for (size_t r = 0; r < num_receivers; ++r ) {
                tbb::flow::make_edge( exe_node, receivers[r] );
            }

            NativeParallelFor( p, parallel_puts<InputType>(exe_node) );
            g.wait_for_all(); 

            // 2) the nodes will receive puts from multiple predecessors simultaneously,
            size_t ec = harness_graph_executor<InputType, OutputType>::execute_count;
            ASSERT( (int)ec == p*N, NULL ); 
            for (size_t r = 0; r < num_receivers; ++r ) {
                size_t c = receivers[r].my_count;
                // 3) the nodes will send to multiple successors.
                ASSERT( (int)c == p*N, NULL );
            }
        }
    }
}

template< typename InputType, typename OutputType >
void run_unlimited_concurrency() {
    harness_graph_executor<InputType, OutputType>::max_executors = 0;
    #if __TBB_LAMBDAS_PRESENT
    unlimited_concurrency<InputType,OutputType>( []( InputType i ) -> OutputType { return harness_graph_executor<InputType, OutputType>::func(i); } );
    #endif
    unlimited_concurrency<InputType,OutputType>( &harness_graph_executor<InputType, OutputType>::func );
    unlimited_concurrency<InputType,OutputType>( typename harness_graph_executor<InputType, OutputType>::functor() );
}

struct continue_msg_to_int : private NoAssign {
    int my_int;
    continue_msg_to_int(int x) : my_int(x) {}
    int operator()(tbb::flow::continue_msg) { return my_int; }
};

void test_function_node_with_continue_msg_as_input() {
    // If this function terminates, then this test is successful
    tbb::flow::graph g;

    tbb::flow::broadcast_node<tbb::flow::continue_msg> Start(g);

    tbb::flow::function_node<tbb::flow::continue_msg, int, tbb::flow::rejecting> FN1( g, tbb::flow::serial, continue_msg_to_int(42));
    tbb::flow::function_node<tbb::flow::continue_msg, int, tbb::flow::rejecting> FN2( g, tbb::flow::serial, continue_msg_to_int(43));
    
    tbb::flow::make_edge( Start, FN1 );
    tbb::flow::make_edge( Start, FN2 );
    
    Start.try_put( tbb::flow::continue_msg() );
    g.wait_for_all();
}

//! Tests limited concurrency cases for nodes that accept data messages
void test_concurrency(int num_threads) {
    tbb::task_scheduler_init init(num_threads);
    run_concurrency_levels<int,int>(num_threads);
    run_concurrency_levels<int,tbb::flow::continue_msg>(num_threads);
    run_buffered_levels<int, int>(num_threads);
    run_unlimited_concurrency<int,int>();
    run_unlimited_concurrency<int,empty_no_assign>();
    run_unlimited_concurrency<empty_no_assign,int>();
    run_unlimited_concurrency<empty_no_assign,empty_no_assign>();
    run_unlimited_concurrency<int,tbb::flow::continue_msg>();
    run_unlimited_concurrency<empty_no_assign,tbb::flow::continue_msg>();
    test_function_node_with_continue_msg_as_input();
}

int TestMain() { 
    current_executors = 0;
    if( MinThread<1 ) {
        REPORT("number of threads must be positive\n");
        exit(1);
    }
    for( int p=MinThread; p<=MaxThread; ++p ) {
       test_concurrency(p);
   }
   return Harness::Done;
}

