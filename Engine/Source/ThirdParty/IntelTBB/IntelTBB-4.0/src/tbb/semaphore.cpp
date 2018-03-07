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

#include "semaphore.h"
#if _WIN32||_WIN64
#if defined(RTL_SRWLOCK_INIT)
#include "dynamic_link.h" // Refers to src/tbb, not include/tbb
#include "tbb_misc.h"
#endif
#endif

namespace tbb {
namespace internal {

#if _WIN32||_WIN64
#if defined(RTL_SRWLOCK_INIT)

static atomic<do_once_state> concmon_module_inited;

void WINAPI init_binsem_using_event( SRWLOCK* h_ )
{
    srwl_or_handle* shptr = (srwl_or_handle*) h_;
    shptr->h = CreateEvent( NULL, FALSE/*manual reset*/, FALSE/*not signalled initially*/, NULL);
}

void WINAPI acquire_binsem_using_event( SRWLOCK* h_ )
{
    srwl_or_handle* shptr = (srwl_or_handle*) h_;
    WaitForSingleObject( shptr->h, INFINITE );
}

void WINAPI release_binsem_using_event( SRWLOCK* h_ )
{
    srwl_or_handle* shptr = (srwl_or_handle*) h_;
    SetEvent( shptr->h );
}

static void (WINAPI *__TBB_init_binsem)( SRWLOCK* ) = (void (WINAPI *)(SRWLOCK*))&init_binsem_using_event;
static void (WINAPI *__TBB_acquire_binsem)( SRWLOCK* ) = (void (WINAPI *)(SRWLOCK*))&acquire_binsem_using_event;
static void (WINAPI *__TBB_release_binsem)( SRWLOCK* ) = (void (WINAPI *)(SRWLOCK*))&release_binsem_using_event;

//! Table describing the how to link the handlers.
static const dynamic_link_descriptor SRWLLinkTable[] = {
    DLD(InitializeSRWLock,       __TBB_init_binsem),
    DLD(AcquireSRWLockExclusive, __TBB_acquire_binsem),
    DLD(ReleaseSRWLockExclusive, __TBB_release_binsem)
};

inline void init_concmon_module()
{
    __TBB_ASSERT( (uintptr_t)__TBB_init_binsem==(uintptr_t)&init_binsem_using_event, NULL );
    if( dynamic_link( "Kernel32.dll", SRWLLinkTable, sizeof(SRWLLinkTable)/sizeof(dynamic_link_descriptor) ) ) {
        __TBB_ASSERT( (uintptr_t)__TBB_init_binsem!=(uintptr_t)&init_binsem_using_event, NULL );
        __TBB_ASSERT( (uintptr_t)__TBB_acquire_binsem!=(uintptr_t)&acquire_binsem_using_event, NULL );
        __TBB_ASSERT( (uintptr_t)__TBB_release_binsem!=(uintptr_t)&release_binsem_using_event, NULL );
    }
}

binary_semaphore::binary_semaphore() {
    atomic_do_once( &init_concmon_module, concmon_module_inited );

    __TBB_init_binsem( &my_sem.lock ); 
    if( (uintptr_t)__TBB_init_binsem!=(uintptr_t)&init_binsem_using_event )
        P();
}

binary_semaphore::~binary_semaphore() {
    if( (uintptr_t)__TBB_init_binsem==(uintptr_t)&init_binsem_using_event )
        CloseHandle( my_sem.h );
}

void binary_semaphore::P() { __TBB_acquire_binsem( &my_sem.lock ); }

void binary_semaphore::V() { __TBB_release_binsem( &my_sem.lock ); }

#endif /* defined(RTL_SRWLOCK_INIT) */
#endif /* _WIN32||_WIN64 */

} // namespace internal
} // namespace tbb
