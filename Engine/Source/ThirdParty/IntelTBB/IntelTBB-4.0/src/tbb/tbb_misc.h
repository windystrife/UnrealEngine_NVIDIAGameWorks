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

#ifndef _TBB_tbb_misc_H
#define _TBB_tbb_misc_H

#include "tbb/tbb_stddef.h"
#include "tbb/tbb_machine.h"
#include "tbb/atomic.h"     // For atomic_xxx definitions

#if __linux__ || __FreeBSD__
#include <sys/param.h>  // __FreeBSD_version
#if __FreeBSD_version >= 701000
#include <sys/cpuset.h>
#endif
#endif

namespace tbb {
namespace internal {

const size_t MByte = 1024*1024;

const size_t ThreadStackSize = (sizeof(uintptr_t) <= 4 ? 2 : 4 )*MByte;

#ifndef __TBB_HardwareConcurrency

//! Returns maximal parallelism level supported by the current OS configuration.
int AvailableHwConcurrency();

#else

inline int AvailableHwConcurrency() {
    int n = __TBB_HardwareConcurrency();
    return n > 0 ? n : 1; // Fail safety strap
}
#endif /* __TBB_HardwareConcurrency */


#if _WIN32||_WIN64

//! Returns number of processor groups in the current OS configuration.
/** AvailableHwConcurrency must be called at least once before calling this method. **/
int NumberOfProcessorGroups();

//! Retrieves index of processor group containing processor with the given index
int FindProcessorGroupIndex ( int processorIndex );

//! Affinitizes the thread to the specified processor group
void MoveThreadIntoProcessorGroup( void* hThread, int groupIndex );

#endif /* _WIN32||_WIN64 */

//! Throws std::runtime_error with what() returning error_code description prefixed with aux_info
void handle_win_error( int error_code );

//! True if environment variable with given name is set and not 0; otherwise false.
bool GetBoolEnvironmentVariable( const char * name );

//! Prints TBB version information on stderr
void PrintVersion();

//! Prints arbitrary extra TBB version information on stderr
void PrintExtraVersionInfo( const char* category, const char* format, ... );

//! A callback routine to print RML version information on stderr
void PrintRMLVersionInfo( void* arg, const char* server_info );

// For TBB compilation only; not to be used in public headers
#if defined(min) || defined(max)
#undef min
#undef max
#endif

//! Utility template function returning lesser of the two values.
/** Provided here to avoid including not strict safe <algorithm>.\n
    In case operands cause signed/unsigned or size mismatch warnings it is caller's
    responsibility to do the appropriate cast before calling the function. **/
template<typename T1, typename T2>
T1 min ( const T1& val1, const T2& val2 ) {
    return val1 < val2 ? val1 : val2;
}

//! Utility template function returning greater of the two values.
/** Provided here to avoid including not strict safe <algorithm>.\n
    In case operands cause signed/unsigned or size mismatch warnings it is caller's
    responsibility to do the appropriate cast before calling the function. **/
template<typename T1, typename T2>
T1 max ( const T1& val1, const T2& val2 ) {
    return val1 < val2 ? val2 : val1;
}

//! Utility template function to prevent "unused" warnings by various compilers.
template<typename T>
void suppress_unused_warning( const T& ) {}

//------------------------------------------------------------------------
// FastRandom
//------------------------------------------------------------------------

/** Defined in tbb_main.cpp **/
unsigned GetPrime ( unsigned seed );

//! A fast random number generator.
/** Uses linear congruential method. */
class FastRandom {
    unsigned x, a;
public:
    //! Get a random number.
    unsigned short get() {
        return get(x);
    }
    //! Get a random number for the given seed; update the seed for next use.
    unsigned short get( unsigned& seed ) {
        unsigned short r = (unsigned short)(seed>>16);
        seed = seed*a+1;
        return r;
    }
    //! Construct a random number generator.
    FastRandom( unsigned seed ) {
        x = seed;
        a = GetPrime( seed );
    }
};

//------------------------------------------------------------------------
// Atomic extensions
//------------------------------------------------------------------------

//! Atomically replaces value of dst with newValue if they satisfy condition of compare predicate
/** Return value semantics is the same as for CAS. **/
template<typename T1, typename T2, class Pred>
T1 atomic_update ( tbb::atomic<T1>& dst, T2 newValue, Pred compare ) {
    T1 oldValue = dst;
    while ( compare(oldValue, newValue) ) {
        if ( dst.compare_and_swap((T1)newValue, oldValue) == oldValue )
            break;
        oldValue = dst;
    }
    return oldValue;
}

//! One-time initialization states
enum do_once_state {
    do_once_uninitialized = 0,  ///< No execution attempts have been undertaken yet
    do_once_pending,            ///< A thread is executing associated do-once routine
    do_once_executed,           ///< Do-once routine has been executed
    initialization_complete = do_once_executed  ///< Convenience alias
};

//! One-time initialization function
/** /param initializer Pointer to function without arguments
           The variant that returns bool is used for cases when initialization can fail
           and it is OK to continue execution, but the state should be reset so that
           the initialization attempt was repeated the next time.
    /param state Shared state associated with initializer that specifies its
            initialization state. Must be initially set to #uninitialized value
            (e.g. by means of default static zero initialization). **/
template <typename F>
void atomic_do_once ( const F& initializer, atomic<do_once_state>& state ) {
    // tbb::atomic provides necessary acquire and release fences.
    // The loop in the implementation is necessary to avoid race when thread T2
    // that arrived in the middle of initialization attempt by another thread T1
    // has just made initialization possible.
    // In such a case T2 has to rely on T1 to initialize, but T1 may already be past
    // the point where it can recognize the changed conditions.
    while ( state != do_once_executed ) {
        if( state == do_once_uninitialized ) {
            if( state.compare_and_swap( do_once_pending, do_once_uninitialized ) == do_once_uninitialized ) {
                run_initializer( initializer, state );
                break;
            }
        }
        spin_wait_while_eq( state, do_once_pending );
    }
}

// Run the initializer which can not fail
inline void run_initializer( void (*f)(), atomic<do_once_state>& state ) {
    f();
    state = do_once_executed;
}

// Run the initializer which can require repeated call
inline void run_initializer( bool (*f)(), atomic<do_once_state>& state ) {
    state = f() ? do_once_executed : do_once_uninitialized;
}

#ifdef __TBB_HardwareConcurrency
    class affinity_helper {
    public:
        void protect_affinity_mask() {}
    };
#elif __linux__ || __FreeBSD_version >= 701000
  #if __linux__
    typedef cpu_set_t basic_mask_t;
  #elif __FreeBSD_version >= 701000
    typedef cpuset_t basic_mask_t;
  #else
    #error affinity_helper is not implemented in this OS
  #endif
    class affinity_helper {
        basic_mask_t* threadMask;
        int is_changed;
    public:
        affinity_helper() : threadMask(NULL), is_changed(0) {}
        ~affinity_helper();
        void protect_affinity_mask();
    };
#elif defined(_SC_NPROCESSORS_ONLN)
    class affinity_helper {
    public:
        void protect_affinity_mask() {}
    };
#elif _WIN32||_WIN64
    class affinity_helper {
    public:
        void protect_affinity_mask() {}
    };
#else
    #error affinity_helper is not implemented in this OS
#endif /* __TBB_HardwareConcurrency */

} // namespace internal
} // namespace tbb

#endif /* _TBB_tbb_misc_H */
