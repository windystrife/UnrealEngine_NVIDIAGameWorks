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

#ifndef __TBB_machine_H
#error Do not #include this internal file directly; use public TBB headers instead.
#endif

#include <sched.h>
#define __TBB_Yield()  sched_yield()

/* Futex definitions */
#include <sys/syscall.h>

#if defined(SYS_futex)

#define __TBB_USE_FUTEX 1
#include <limits.h>
#include <errno.h>
// Unfortunately, some versions of Linux do not have a header that defines FUTEX_WAIT and FUTEX_WAKE.

#ifdef FUTEX_WAIT
#define __TBB_FUTEX_WAIT FUTEX_WAIT
#else
#define __TBB_FUTEX_WAIT 0
#endif

#ifdef FUTEX_WAKE
#define __TBB_FUTEX_WAKE FUTEX_WAKE
#else
#define __TBB_FUTEX_WAKE 1
#endif

#ifndef __TBB_ASSERT
#error machine specific headers must be included after tbb_stddef.h
#endif

namespace tbb {

namespace internal {

inline int futex_wait( void *futex, int comparand ) {
    int r = ::syscall( SYS_futex,futex,__TBB_FUTEX_WAIT,comparand,NULL,NULL,0 );
#if TBB_USE_ASSERT
    int e = errno;
    __TBB_ASSERT( r==0||r==EWOULDBLOCK||(r==-1&&(e==EAGAIN||e==EINTR)), "futex_wait failed." );
#endif /* TBB_USE_ASSERT */
    return r;
}

inline int futex_wakeup_one( void *futex ) {
    int r = ::syscall( SYS_futex,futex,__TBB_FUTEX_WAKE,1,NULL,NULL,0 );
    __TBB_ASSERT( r==0||r==1, "futex_wakeup_one: more than one thread woken up?" );
    return r;
}

inline int futex_wakeup_all( void *futex ) {
    int r = ::syscall( SYS_futex,futex,__TBB_FUTEX_WAKE,INT_MAX,NULL,NULL,0 );
    __TBB_ASSERT( r>=0, "futex_wakeup_all: error in waking up threads" );
    return r;
}

} /* namespace internal */

} /* namespace tbb */

#endif /* SYS_futex */
