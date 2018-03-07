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

#ifndef harness_tbb_independence_H
#define harness_tbb_independence_H

#if __linux__  && __ia64__

#define __TBB_NO_IMPLICIT_LINKAGE 1
#include "tbb/tbb_machine.h"

#include <pthread.h>

// Can't use Intel compiler intrinsic due to internal error reported by 10.1 compiler
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

int32_t __TBB_machine_fetchadd4__TBB_full_fence (volatile void *ptr, int32_t value)
{
    pthread_mutex_lock(&counter_mutex);
    int32_t result = *(int32_t*)ptr;
    *(int32_t*)ptr = result + value;
    pthread_mutex_unlock(&counter_mutex);
    return result;
}

int64_t __TBB_machine_fetchadd8__TBB_full_fence (volatile void *ptr, int64_t value)
{
    pthread_mutex_lock(&counter_mutex);
    int32_t result = *(int32_t*)ptr;
    *(int32_t*)ptr = result + value;
    pthread_mutex_unlock(&counter_mutex);
    return result;
}

void __TBB_machine_pause(int32_t /*delay*/) {  __TBB_Yield(); }

pthread_mutex_t cas_mutex = PTHREAD_MUTEX_INITIALIZER;

extern "C" int64_t __TBB_machine_cmpswp8__TBB_full_fence(volatile void *ptr, int64_t value, int64_t comparand)
{
    pthread_mutex_lock(&cas_mutex);
    int64_t result = *(int64_t*)ptr;
    if (result == comparand)
        *(int64_t*)ptr = value;
    pthread_mutex_unlock(&cas_mutex);
    return result;
}

#endif /* __linux__  && __ia64 */

#endif // harness_tbb_independence_H
