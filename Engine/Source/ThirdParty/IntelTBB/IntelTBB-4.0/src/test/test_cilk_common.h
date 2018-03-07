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

// This file is a common part of test_cilk_interop and test_cilk_dynamic_load tests

int TBB_Fib( int n );

class FibCilkSubtask: public tbb::task {
    int n;
    int& result;
    /*override*/ task* execute() {
        if( n<2 ) {
            result = n;
        } else {
            int x, y;
            x = cilk_spawn TBB_Fib(n-2);
            y = cilk_spawn TBB_Fib(n-1);
            cilk_sync;
            result = x+y;
        }
        return NULL;
    }
public:
    FibCilkSubtask( int& result_, int n_ ) : result(result_), n(n_) {}
};

class FibTask: public tbb::task {
    int n;
    int& result;
    /*override*/ task* execute() {
        if( !g_sandwich && n<2 ) {
            result = n;
        } else {
            int x,y;
            tbb::task_scheduler_init init(P_nested);
            task* self0 = &task::self();
            set_ref_count( 3 );
            if ( g_sandwich ) {
                spawn (*new( allocate_child() ) FibCilkSubtask(x,n-1));
                spawn (*new( allocate_child() ) FibCilkSubtask(y,n-2));
            }
            else {
                spawn (*new( allocate_child() ) FibTask(x,n-1));
                spawn (*new( allocate_child() ) FibTask(y,n-2));
            }
            wait_for_all(); 
            task* self1 = &task::self();
            ASSERT( self0 == self1, "failed to preserve TBB TLS" );
            result = x+y;
        }
        return NULL;
    }
public:
    FibTask( int& result_, int n_ ) : result(result_), n(n_) {}
};

int TBB_Fib( int n ) {
    if( n<2 ) {
        return n;
    } else {
        int result;
        tbb::task_scheduler_init init(P_nested);
        tbb::task::spawn_root_and_wait(*new( tbb::task::allocate_root()) FibTask(result,n) );
        return result;
    }
}
