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

#if _WIN32||_WIN64
#include <process.h>        // _beginthreadex()
#endif
#include "tbb_misc.h"       // handle_win_error(), ThreadStackSize
#include "tbb/tbb_stddef.h"
#include "tbb/tbb_thread.h"
#include "tbb/tbb_allocator.h"
#include "governor.h"       // default_num_threads()

namespace tbb {
namespace internal {

//! Allocate a closure
void* allocate_closure_v3( size_t size )
{
    return allocate_via_handler_v3( size );
}

//! Free a closure allocated by allocate_closure_v3
void free_closure_v3( void *ptr )
{
    deallocate_via_handler_v3( ptr );
}

void tbb_thread_v3::join()
{
    __TBB_ASSERT( joinable(), "thread should be joinable when join called" );
#if _WIN32||_WIN64 
    DWORD status = WaitForSingleObject( my_handle, INFINITE );
    if ( status == WAIT_FAILED )
        handle_win_error( GetLastError() );
    BOOL close_stat = CloseHandle( my_handle );
    if ( close_stat == 0 )
        handle_win_error( GetLastError() );
    my_thread_id = 0;
#else
    int status = pthread_join( my_handle, NULL );
    if( status )
        handle_perror( status, "pthread_join" );
#endif // _WIN32||_WIN64 
    my_handle = 0;
}

void tbb_thread_v3::detach() {
    __TBB_ASSERT( joinable(), "only joinable thread can be detached" );
#if _WIN32||_WIN64
    BOOL status = CloseHandle( my_handle );
    if ( status == 0 )
      handle_win_error( GetLastError() );
    my_thread_id = 0;
#else
    int status = pthread_detach( my_handle );
    if( status )
        handle_perror( status, "pthread_detach" );
#endif // _WIN32||_WIN64
    my_handle = 0;
}

void tbb_thread_v3::internal_start( __TBB_NATIVE_THREAD_ROUTINE_PTR(start_routine),
                                    void* closure ) {
#if _WIN32||_WIN64
    unsigned thread_id;
    // The return type of _beginthreadex is "uintptr_t" on new MS compilers,
    // and 'unsigned long' on old MS compilers.  uintptr_t works for both.
    uintptr_t status = _beginthreadex( NULL, ThreadStackSize, start_routine,
                                     closure, 0, &thread_id ); 
    if( status==0 )
        handle_perror(errno,"__beginthreadex");
    else {
        my_handle = (HANDLE)status;
        my_thread_id = thread_id;
    }
#else
    pthread_t thread_handle;
    int status;
    pthread_attr_t stack_size;
    status = pthread_attr_init( &stack_size );
    if( status )
        handle_perror( status, "pthread_attr_init" );
    status = pthread_attr_setstacksize( &stack_size, ThreadStackSize );
    if( status )
        handle_perror( status, "pthread_attr_setstacksize" );

    status = pthread_create( &thread_handle, &stack_size, start_routine, closure );
    if( status )
        handle_perror( status, "pthread_create" );

    my_handle = thread_handle;
#endif // _WIN32||_WIN64
}

unsigned tbb_thread_v3::hardware_concurrency() {
    return governor::default_num_threads();
}

tbb_thread_v3::id thread_get_id_v3() {
#if _WIN32||_WIN64
    return tbb_thread_v3::id( GetCurrentThreadId() );
#else
    return tbb_thread_v3::id( pthread_self() );
#endif // _WIN32||_WIN64
}
    
void move_v3( tbb_thread_v3& t1, tbb_thread_v3& t2 )
{
    if (t1.joinable())
        t1.detach();
    t1.my_handle = t2.my_handle;
    t2.my_handle = 0;
#if _WIN32||_WIN64
    t1.my_thread_id = t2.my_thread_id;
    t2.my_thread_id = 0;
#endif // _WIN32||_WIN64
}

void thread_yield_v3()
{
    __TBB_Yield();
}

void thread_sleep_v3(const tick_count::interval_t &i)
{
#if _WIN32||_WIN64
     tick_count t0 = tick_count::now();
     tick_count t1 = t0;
     for(;;) {
         double remainder = (i-(t1-t0)).seconds()*1e3;  // milliseconds remaining to sleep
         if( remainder<=0 ) break;
         DWORD t = remainder>=INFINITE ? INFINITE-1 : DWORD(remainder);
         Sleep( t );
         t1 = tick_count::now();
    }
#else
    struct timespec req;
    double sec = i.seconds();

    req.tv_sec = static_cast<long>(sec);
    req.tv_nsec = static_cast<long>( (sec - req.tv_sec)*1e9 );
    nanosleep(&req, NULL);
#endif // _WIN32||_WIN64
}

} // internal
} // tbb
