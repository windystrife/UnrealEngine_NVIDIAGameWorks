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

#include "thread_monitor.h"
#include "harness.h"
#include "harness_memory.h"
#include "tbb/semaphore.cpp"

class ThreadState {
    void loop();
public:
    static __RML_DECL_THREAD_ROUTINE routine( void* arg ) {
        static_cast<ThreadState*>(arg)->loop();
        return 0;
    }
    typedef rml::internal::thread_monitor thread_monitor;
    thread_monitor monitor;
    volatile int request;
    volatile int ack;
    volatile unsigned clock;
    volatile unsigned stamp;
    ThreadState() : request(-1), ack(-1), clock(0) {}
};

void ThreadState::loop() {
    for(;;) {
        ++clock;
        if( ack==request ) {
            thread_monitor::cookie c;
            monitor.prepare_wait(c);
            if( ack==request ) {
                REMARK("%p: request=%d ack=%d\n", this, request, ack );
                monitor.commit_wait(c);
            } else
                monitor.cancel_wait();
        } else {
            // Throw in delay occasionally
            switch( request%8 ) {
                case 0: 
                case 1:
                case 5:
                    rml::internal::thread_monitor::yield();
            }
            int r = request;
            ack = request;
            if( !r ) return;
        }
    }
}

// Linux on IA-64 seems to require at least 1<<18 bytes per stack.
const size_t MinStackSize = 1<<18;
const size_t MaxStackSize = 1<<22;

int TestMain () {
    for( int p=MinThread; p<=MaxThread; ++p ) {
        ThreadState* t = new ThreadState[p];
        for( size_t stack_size = MinStackSize; stack_size<=MaxStackSize; stack_size*=2 ) {
            REMARK("launching %d threads\n",p);
            for( int i=0; i<p; ++i )
                rml::internal::thread_monitor::launch( ThreadState::routine, t+i, stack_size ); 
            for( int k=1000; k>=0; --k ) {
                if( k%8==0 ) {
                    // Wait for threads to wait.
                    for( int i=0; i<p; ++i ) {
                        unsigned count = 0;
                        do {
                            t[i].stamp = t[i].clock;
                            rml::internal::thread_monitor::yield();
                            if( ++count>=1000 ) {
                                REPORT("Warning: thread %d not waiting\n",i);
                                break;
                            }
                        } while( t[i].stamp!=t[i].clock );
                    }
                }
                REMARK("notifying threads\n");
                for( int i=0; i<p; ++i ) {
                    // Change state visible to launched thread
                    t[i].request = k;
                    t[i].monitor.notify();
                }
                REMARK("waiting for threads to respond\n");
                for( int i=0; i<p; ++i ) 
                    // Wait for thread to respond 
                    while( t[i].ack!=k ) 
                        rml::internal::thread_monitor::yield();
            }
        }
        delete[] t;
    }

    return Harness::Done;
}
