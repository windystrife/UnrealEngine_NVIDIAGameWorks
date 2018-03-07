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

#if !defined(__TBB_machine_H) || defined(__TBB_machine_windows_ia32_H)
#error Do not #include this internal file directly; use public TBB headers instead.
#endif

#define __TBB_machine_windows_ia32_H

#define __TBB_WORDSIZE 4
#define __TBB_BIG_ENDIAN 0

#if __INTEL_COMPILER
    #define __TBB_compiler_fence() __asm { __asm nop }
#elif _MSC_VER >= 1300
    extern "C" void _ReadWriteBarrier();
    #pragma intrinsic(_ReadWriteBarrier)
    #define __TBB_compiler_fence() _ReadWriteBarrier()
#else
    #error Unsupported compiler - need to define __TBB_{control,acquire,release}_consistency_helper to support it
#endif

#define __TBB_control_consistency_helper() __TBB_compiler_fence()
#define __TBB_acquire_consistency_helper() __TBB_compiler_fence()
#define __TBB_release_consistency_helper() __TBB_compiler_fence()
#define __TBB_full_memory_fence()          __asm { __asm mfence }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    // Workaround for overzealous compiler warnings in /Wp64 mode
    #pragma warning (push)
    #pragma warning (disable: 4244 4267)
#endif

extern "C" {
    __int64 __TBB_EXPORTED_FUNC __TBB_machine_cmpswp8 (volatile void *ptr, __int64 value, __int64 comparand );
    __int64 __TBB_EXPORTED_FUNC __TBB_machine_fetchadd8 (volatile void *ptr, __int64 addend );
    __int64 __TBB_EXPORTED_FUNC __TBB_machine_fetchstore8 (volatile void *ptr, __int64 value );
    void __TBB_EXPORTED_FUNC __TBB_machine_store8 (volatile void *ptr, __int64 value );
    __int64 __TBB_EXPORTED_FUNC __TBB_machine_load8 (const volatile void *ptr);
}

#define __TBB_MACHINE_DEFINE_ATOMICS(S,T,U,A,C) \
static inline T __TBB_machine_cmpswp##S ( volatile void * ptr, U value, U comparand ) { \
    T result; \
    volatile T *p = (T *)ptr; \
    __asm \
    { \
       __asm mov edx, p \
       __asm mov C , value \
       __asm mov A , comparand \
       __asm lock cmpxchg [edx], C \
       __asm mov result, A \
    } \
    return result; \
} \
\
static inline T __TBB_machine_fetchadd##S ( volatile void * ptr, U addend ) { \
    T result; \
    volatile T *p = (T *)ptr; \
    __asm \
    { \
        __asm mov edx, p \
        __asm mov A, addend \
        __asm lock xadd [edx], A \
        __asm mov result, A \
    } \
    return result; \
}\
\
static inline T __TBB_machine_fetchstore##S ( volatile void * ptr, U value ) { \
    T result; \
    volatile T *p = (T *)ptr; \
    __asm \
    { \
        __asm mov edx, p \
        __asm mov A, value \
        __asm lock xchg [edx], A \
        __asm mov result, A \
    } \
    return result; \
}


__TBB_MACHINE_DEFINE_ATOMICS(1, __int8, __int8, al, cl)
__TBB_MACHINE_DEFINE_ATOMICS(2, __int16, __int16, ax, cx)
__TBB_MACHINE_DEFINE_ATOMICS(4, ptrdiff_t, ptrdiff_t, eax, ecx)

#undef __TBB_MACHINE_DEFINE_ATOMICS

#if ( _MSC_VER>=1400 && !defined(__INTEL_COMPILER) ) ||  (__INTEL_COMPILER>=1200)
// MSVC did not have this intrinsic prior to VC8.
// ICL 11.1 fails to compile a TBB example if __TBB_Log2 uses the intrinsic.
#define __TBB_LOG2_USE_BSR_INTRINSIC 1
extern "C" unsigned char _BitScanReverse( unsigned long* i, unsigned long w );
#pragma intrinsic(_BitScanReverse)
#endif

static inline intptr_t __TBB_machine_lg( uintptr_t i ) {
    unsigned long j;
#if __TBB_LOG2_USE_BSR_INTRINSIC
    _BitScanReverse( &j, i );
#else
    __asm
    {
        bsr eax, i
        mov j, eax
    }
#endif
    return j;
}

static inline void __TBB_machine_OR( volatile void *operand, __int32 addend ) {
   __asm 
   {
       mov eax, addend
       mov edx, [operand]
       lock or [edx], eax
   }
}

static inline void __TBB_machine_AND( volatile void *operand, __int32 addend ) {
   __asm 
   {
       mov eax, addend
       mov edx, [operand]
       lock and [edx], eax
   }
}

static inline void __TBB_machine_pause (__int32 delay ) {
    _asm 
    {
        mov eax, delay
      __TBB_L1: 
        pause
        add eax, -1
        jne __TBB_L1  
    }
    return;
}

#define __TBB_AtomicOR(P,V) __TBB_machine_OR(P,V)
#define __TBB_AtomicAND(P,V) __TBB_machine_AND(P,V)

//TODO: Check if it possible and profitable for IA-32 on (Linux and Windows)
//to use of 64-bit load/store via floating point registers together with full fence
//for sequentially consistent load/store, instead of CAS.
#define __TBB_USE_FETCHSTORE_AS_FULL_FENCED_STORE           1
#define __TBB_USE_GENERIC_HALF_FENCED_LOAD_STORE            1
#define __TBB_USE_GENERIC_RELAXED_LOAD_STORE                1
#define __TBB_USE_GENERIC_SEQUENTIAL_CONSISTENCY_LOAD_STORE 1

// Definition of other functions
extern "C" __declspec(dllimport) int __stdcall SwitchToThread( void );
#define __TBB_Yield()  SwitchToThread()
#define __TBB_Pause(V) __TBB_machine_pause(V)
#define __TBB_Log2(V)  __TBB_machine_lg(V)

#if defined(_MSC_VER)&&_MSC_VER<1400
    static inline void* __TBB_machine_get_current_teb () {
        void* pteb;
        __asm mov eax, fs:[0x18]
        __asm mov pteb, eax
        return pteb;
    }
#endif

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    #pragma warning (pop)
#endif // warnings 4244, 4267 are back

// API to retrieve/update FPU control setting
#define __TBB_CPU_CTL_ENV_PRESENT 1

struct __TBB_cpu_ctl_env_t {
    int     mxcsr;
    short   x87cw;
};
inline void __TBB_get_cpu_ctl_env ( __TBB_cpu_ctl_env_t* ctl ) {
    __asm {
        __asm mov     eax, ctl
        __asm stmxcsr [eax]
        __asm fstcw   [eax+4]
    }
}
inline void __TBB_set_cpu_ctl_env ( const __TBB_cpu_ctl_env_t* ctl ) {
    __asm {
        __asm mov     eax, ctl
        __asm ldmxcsr [eax]
        __asm fldcw   [eax+4]
    }
}

