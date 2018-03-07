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

// TODO: revise by comparing with mac_ppc.h

#if !defined(__TBB_machine_H) || defined(__TBB_machine_xbox360_ppc_H)
#error Do not #include this internal file directly; use public TBB headers instead.
#endif

#define __TBB_machine_xbox360_ppc_H

#define NONET
#define NOD3D
#include "xtl.h"    
#include "ppcintrinsics.h"

#if _MSC_VER >= 1300
extern "C" void _MemoryBarrier();
#pragma intrinsic(_MemoryBarrier)
#define __TBB_control_consistency_helper() __isync()
#define __TBB_acquire_consistency_helper() _MemoryBarrier()
#define __TBB_release_consistency_helper() _MemoryBarrier()
#endif

#define __TBB_full_memory_fence() __sync()

#define __TBB_WORDSIZE 4
#define __TBB_BIG_ENDIAN 1

//todo: define __TBB_USE_FENCED_ATOMICS and define acquire/release primitives to maximize performance

inline __int32 __TBB_machine_cmpswp4(volatile void *ptr, __int32 value, __int32 comparand ) {                               
 __sync();
 __int32 result = InterlockedCompareExchange((volatile LONG*)ptr, value, comparand);
 __isync();
 return result;
}

inline __int64 __TBB_machine_cmpswp8(volatile void *ptr, __int64 value, __int64 comparand )
{
 __sync();
 __int64 result = InterlockedCompareExchange64((volatile LONG64*)ptr, value, comparand);
 __isync();
 return result;
}

#define __TBB_USE_GENERIC_PART_WORD_CAS                     1
#define __TBB_USE_GENERIC_FETCH_ADD                         1
#define __TBB_USE_GENERIC_FETCH_STORE                       1
#define __TBB_USE_GENERIC_HALF_FENCED_LOAD_STORE            1
#define __TBB_USE_GENERIC_RELAXED_LOAD_STORE                1
#define __TBB_USE_GENERIC_DWORD_LOAD_STORE                  1
#define __TBB_USE_GENERIC_SEQUENTIAL_CONSISTENCY_LOAD_STORE 1

#pragma optimize( "", off )
inline void __TBB_machine_pause (__int32 delay ) 
{
 for (__int32 i=0; i<delay; i++) {;};
}
#pragma optimize( "", on ) 

#define __TBB_Yield()  Sleep(0)
#define __TBB_Pause(V) __TBB_machine_pause(V)

// This port uses only 2 hardware threads for TBB on XBOX 360. 
// Others are left to sound etc.
// Change the following mask to allow TBB use more HW threads.
static const int __TBB_XBOX360_HARDWARE_THREAD_MASK = 0x0C;

static inline int __TBB_XBOX360_DetectNumberOfWorkers() 
{
     char a[__TBB_XBOX360_HARDWARE_THREAD_MASK];  //compile time assert - at least one bit should be set always
     a[0]=0;

     return ((__TBB_XBOX360_HARDWARE_THREAD_MASK >> 0) & 1) +
            ((__TBB_XBOX360_HARDWARE_THREAD_MASK >> 1) & 1) +
            ((__TBB_XBOX360_HARDWARE_THREAD_MASK >> 2) & 1) +
            ((__TBB_XBOX360_HARDWARE_THREAD_MASK >> 3) & 1) +
            ((__TBB_XBOX360_HARDWARE_THREAD_MASK >> 4) & 1) +
            ((__TBB_XBOX360_HARDWARE_THREAD_MASK >> 5) & 1) + 1;  // +1 accomodates for the master thread
}

static inline int __TBB_XBOX360_GetHardwareThreadIndex(int workerThreadIndex)
{
    workerThreadIndex %= __TBB_XBOX360_DetectNumberOfWorkers()-1;
    int m = __TBB_XBOX360_HARDWARE_THREAD_MASK;
    int index = 0;
    int skipcount = workerThreadIndex;
    while (true)
    {
        if ((m & 1)!=0) 
        {
            if (skipcount==0) break;
            skipcount--;
        }
        m >>= 1;
       index++;
    }
    return index; 
}

#define __TBB_HardwareConcurrency() __TBB_XBOX360_DetectNumberOfWorkers()
