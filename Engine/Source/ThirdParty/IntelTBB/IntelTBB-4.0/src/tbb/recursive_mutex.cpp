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

#include "tbb/recursive_mutex.h"
#include "itt_notify.h"

namespace tbb {

void recursive_mutex::scoped_lock::internal_acquire( recursive_mutex& m ) {
#if _WIN32||_WIN64
    switch( m.state ) {
      case INITIALIZED: 
        // since we cannot look into the internal of the CriticalSection object
        // we won't know how many times the lock has been acquired, and thus
        // we won't know when we may safely set the state back to INITIALIZED
        // if we change the state to HELD as in mutex.cpp.  thus, we won't change
        // the state for recursive_mutex
        EnterCriticalSection( &m.impl );
        break;
      case DESTROYED: 
        __TBB_ASSERT(false,"recursive_mutex::scoped_lock: mutex already destroyed"); 
        break;
      default: 
        __TBB_ASSERT(false,"recursive_mutex::scoped_lock: illegal mutex state");
        break;
    }
#else
    int error_code = pthread_mutex_lock(&m.impl);
    __TBB_ASSERT_EX(!error_code,"recursive_mutex::scoped_lock: pthread_mutex_lock failed");
#endif /* _WIN32||_WIN64 */
    my_mutex = &m;
}

void recursive_mutex::scoped_lock::internal_release() {
    __TBB_ASSERT( my_mutex, "recursive_mutex::scoped_lock: not holding a mutex" );
#if _WIN32||_WIN64    
    switch( my_mutex->state ) {
      case INITIALIZED: 
        LeaveCriticalSection( &my_mutex->impl );
        break;
      case DESTROYED: 
        __TBB_ASSERT(false,"recursive_mutex::scoped_lock: mutex already destroyed"); 
        break;
      default: 
        __TBB_ASSERT(false,"recursive_mutex::scoped_lock: illegal mutex state");
        break;
    }
#else
     int error_code = pthread_mutex_unlock(&my_mutex->impl);
     __TBB_ASSERT_EX(!error_code, "recursive_mutex::scoped_lock: pthread_mutex_unlock failed");
#endif /* _WIN32||_WIN64 */
     my_mutex = NULL;
}

bool recursive_mutex::scoped_lock::internal_try_acquire( recursive_mutex& m ) {
#if _WIN32||_WIN64
    switch( m.state ) {
      case INITIALIZED: 
        break;
      case DESTROYED: 
        __TBB_ASSERT(false,"recursive_mutex::scoped_lock: mutex already destroyed"); 
        break;
      default: 
        __TBB_ASSERT(false,"recursive_mutex::scoped_lock: illegal mutex state");
        break;
    }
#endif /* _WIN32||_WIN64 */
    bool result;
#if _WIN32||_WIN64
    result = TryEnterCriticalSection(&m.impl)!=0;
#else
    result = pthread_mutex_trylock(&m.impl)==0;
#endif /* _WIN32||_WIN64 */
    if( result )
        my_mutex = &m;
    return result;
}

void recursive_mutex::internal_construct() {
#if _WIN32||_WIN64
    InitializeCriticalSection(&impl);
    state = INITIALIZED;
#else
    pthread_mutexattr_t mtx_attr;
    int error_code = pthread_mutexattr_init( &mtx_attr );
    if( error_code )
        tbb::internal::handle_perror(error_code,"recursive_mutex: pthread_mutexattr_init failed");

    pthread_mutexattr_settype( &mtx_attr, PTHREAD_MUTEX_RECURSIVE );
    error_code = pthread_mutex_init( &impl, &mtx_attr );
    if( error_code )
        tbb::internal::handle_perror(error_code,"recursive_mutex: pthread_mutex_init failed");
    pthread_mutexattr_destroy( &mtx_attr );
#endif /* _WIN32||_WIN64*/    
    ITT_SYNC_CREATE(&impl, _T("tbb::recursive_mutex"), _T(""));
}

void recursive_mutex::internal_destroy() {
#if _WIN32||_WIN64
    switch( state ) {
      case INITIALIZED:
        DeleteCriticalSection(&impl);
        break;
      case DESTROYED: 
        __TBB_ASSERT(false,"recursive_mutex: already destroyed");
        break;
      default: 
         __TBB_ASSERT(false,"recursive_mutex: illegal state for destruction");
         break;
    }
    state = DESTROYED;
#else
    int error_code = pthread_mutex_destroy(&impl); 
    __TBB_ASSERT_EX(!error_code,"recursive_mutex: pthread_mutex_destroy failed");
#endif /* _WIN32||_WIN64 */
}

} // namespace tbb
