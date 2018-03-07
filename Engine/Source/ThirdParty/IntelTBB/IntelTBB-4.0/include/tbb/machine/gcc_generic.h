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

#if !defined(__TBB_machine_H) || defined(__TBB_machine_gcc_generic_H)
#error Do not #include this internal file directly; use public TBB headers instead.
#endif

#define __TBB_machine_gcc_generic_H

#include <stdint.h>
#include <unistd.h>

#define __TBB_WORDSIZE      __SIZEOF_POINTER__

#ifdef __BYTE_ORDER__
    #if __BYTE_ORDER__==__ORDER_BIG_ENDIAN__
        #define __TBB_BIG_ENDIAN    1
    #elif __BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__
        #define __TBB_BIG_ENDIAN    0
    #elif __BYTE_ORDER__==__ORDER_PDP_ENDIAN__
        #define __TBB_BIG_ENDIAN -1 // not currently supported
    #endif
#endif

/** As this generic implementation has absolutely no information about underlying
    hardware, its performance most likely will be sub-optimal because of full memory
    fence usages where a more lightweight synchronization means (or none at all)
    could suffice. Thus if you use this header to enable TBB on a new platform,
    consider forking it and relaxing below helpers as appropriate. **/
#define __TBB_acquire_consistency_helper()  __sync_synchronize()
#define __TBB_release_consistency_helper()  __sync_synchronize()
#define __TBB_full_memory_fence()           __sync_synchronize()
#define __TBB_control_consistency_helper()  __sync_synchronize()

#define __TBB_MACHINE_DEFINE_ATOMICS(S,T)                                                         \
inline T __TBB_machine_cmpswp##S( volatile void *ptr, T value, T comparand ) {                    \
    return __sync_val_compare_and_swap(reinterpret_cast<volatile T *>(ptr),comparand,value);      \
}                                                                                                 \
                                                                                                  \
inline T __TBB_machine_fetchadd##S( volatile void *ptr, T value ) {                               \
    return __sync_fetch_and_add(reinterpret_cast<volatile T *>(ptr),value);                       \
}                                                                                                 \

__TBB_MACHINE_DEFINE_ATOMICS(1,int8_t)
__TBB_MACHINE_DEFINE_ATOMICS(2,int16_t)
__TBB_MACHINE_DEFINE_ATOMICS(4,int32_t)
__TBB_MACHINE_DEFINE_ATOMICS(8,int64_t)

#undef __TBB_MACHINE_DEFINE_ATOMICS

namespace tbb{ namespace internal { namespace gcc_builtins {
    inline int clz(unsigned int x){ return __builtin_clz(x);};
    inline int clz(unsigned long int x){ return __builtin_clzl(x);};
    inline int clz(unsigned long long int x){ return __builtin_clzll(x);};
}}}
//gcc __builtin_clz builtin count _number_ of leading zeroes
static inline intptr_t __TBB_machine_lg( uintptr_t x ) {
    return sizeof(x)*8 - tbb::internal::gcc_builtins::clz(x) -1 ;
}

static inline void __TBB_machine_or( volatile void *ptr, uintptr_t addend ) {
    __sync_fetch_and_or(reinterpret_cast<volatile uintptr_t *>(ptr),addend);
}

static inline void __TBB_machine_and( volatile void *ptr, uintptr_t addend ) {
    __sync_fetch_and_and(reinterpret_cast<volatile uintptr_t *>(ptr),addend);
}


typedef unsigned char __TBB_Flag;

typedef __TBB_atomic __TBB_Flag __TBB_atomic_flag;

inline bool __TBB_machine_try_lock_byte( __TBB_atomic_flag &flag ) {
    return __sync_lock_test_and_set(&flag,1)==0;
}

inline void __TBB_machine_unlock_byte( __TBB_atomic_flag &flag , __TBB_Flag) {
    __sync_lock_release(&flag);
}

// Machine specific atomic operations
#define __TBB_AtomicOR(P,V)     __TBB_machine_or(P,V)
#define __TBB_AtomicAND(P,V)    __TBB_machine_and(P,V)

#define __TBB_TryLockByte   __TBB_machine_try_lock_byte
#define __TBB_UnlockByte    __TBB_machine_unlock_byte

// Definition of other functions
#define __TBB_Log2(V)           __TBB_machine_lg(V)

#define __TBB_USE_GENERIC_FETCH_STORE                       1
#define __TBB_USE_GENERIC_HALF_FENCED_LOAD_STORE            1
#define __TBB_USE_GENERIC_RELAXED_LOAD_STORE                1
#define __TBB_USE_GENERIC_SEQUENTIAL_CONSISTENCY_LOAD_STORE 1

#if __TBB_WORDSIZE==4
    #define __TBB_USE_GENERIC_DWORD_LOAD_STORE              1
#endif
