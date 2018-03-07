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

//TODO: when removing TBB_PREVIEW_LOCAL_OBSERVER, change the header or defines here
#include "tbb/task_scheduler_observer.h"

typedef uintptr_t FlagType;
const int MaxFlagIndex = sizeof(FlagType)*8-1;

class MyObserver: public tbb::task_scheduler_observer {
    FlagType flags;
    /*override*/ void on_scheduler_entry( bool is_worker );
    /*override*/ void on_scheduler_exit( bool is_worker );
public:
    MyObserver( FlagType flags_ ) : flags(flags_) {
        observe(true);
    }
};

#include "harness_assert.h"
#include "tbb/atomic.h"

tbb::atomic<int> EntryCount;
tbb::atomic<int> ExitCount;

struct State {
    FlagType MyFlags;
    bool IsMaster;
    State() : MyFlags(), IsMaster() {}
};

#include "../tbb/tls.h"
tbb::internal::tls<State*> LocalState;

void MyObserver::on_scheduler_entry( bool is_worker ) {
    State& state = *LocalState;
    ASSERT( is_worker==!state.IsMaster, NULL );
    ++EntryCount;
    state.MyFlags |= flags;
}

void MyObserver::on_scheduler_exit( bool is_worker ) {
    State& state = *LocalState;
    ASSERT( is_worker==!state.IsMaster, NULL );
    ++ExitCount;
    state.MyFlags &= ~flags;
}

#include "tbb/task.h"

class FibTask: public tbb::task {
    const int n;
    FlagType flags;
public:
    FibTask( int n_, FlagType flags_ ) : n(n_), flags(flags_) {}
    /*override*/ tbb::task* execute() {
        ASSERT( !(~LocalState->MyFlags & flags), NULL );
        if( n>=2 ) {
            set_ref_count(3);
            spawn(*new( allocate_child() ) FibTask(n-1,flags));
            spawn_and_wait_for_all(*new( allocate_child() ) FibTask(n-2,flags));
        }
        return NULL;
    }
};

void DoFib( FlagType flags ) {
    tbb::task* t = new( tbb::task::allocate_root() ) FibTask(10,flags);
    tbb::task::spawn_root_and_wait(*t);
}

#include "tbb/task_scheduler_init.h"
#include "harness.h"

class DoTest {
    int nthread;
public:
    DoTest( int n ) : nthread(n) {}
    void operator()( int i ) const {
        LocalState->IsMaster = true;
        if( i==0 ) {   
            tbb::task_scheduler_init init(nthread);
            DoFib(0);
        } else {
            FlagType f = i<=MaxFlagIndex? 1<<i : 0;
            MyObserver w(f);
            tbb::task_scheduler_init init(nthread);
            DoFib(f);
        }
    }
};

void TestObserver( int p, int q ) {
    NativeParallelFor( p, DoTest(q) );
}

int TestMain () {
    for( int p=MinThread; p<=MaxThread; ++p ) 
        for( int q=MinThread; q<=MaxThread; ++q ) 
            TestObserver(p,q);
    ASSERT( EntryCount>0, "on_scheduler_entry not exercised" );
    ASSERT( ExitCount>0, "on_scheduler_exit not exercised" );
    return Harness::Done;
}
