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

#ifndef __TBB_spin_mutex_H
#define __TBB_spin_mutex_H

#include <cstddef>
#include <new>
#include "aligned_space.h"
#include "tbb_stddef.h"
#include "tbb_machine.h"
#include "tbb_profiling.h"

namespace tbb {

//! A lock that occupies a single byte.
/** A spin_mutex is a spin mutex that fits in a single byte.  
    It should be used only for locking short critical sections 
    (typically less than 20 instructions) when fairness is not an issue.  
    If zero-initialized, the mutex is considered unheld.
    @ingroup synchronization */
class spin_mutex {
    //! 0 if lock is released, 1 if lock is acquired.
    __TBB_atomic_flag flag;

public:
    //! Construct unacquired lock.
    /** Equivalent to zero-initialization of *this. */
    spin_mutex() : flag(0) {
#if TBB_USE_THREADING_TOOLS
        internal_construct();
#endif
    }

    //! Represents acquisition of a mutex.
    class scoped_lock : internal::no_copy {
    private:
        //! Points to currently held mutex, or NULL if no lock is held.
        spin_mutex* my_mutex; 

        //! Value to store into spin_mutex::flag to unlock the mutex. 
        /** This variable is no longer used. Instead, 0 and 1 are used to 
            represent that the lock is free and acquired, respectively. 
            We keep the member variable here to ensure backward compatibility */
        __TBB_Flag my_unlock_value;

        //! Like acquire, but with ITT instrumentation.
        void __TBB_EXPORTED_METHOD internal_acquire( spin_mutex& m );

        //! Like try_acquire, but with ITT instrumentation.
        bool __TBB_EXPORTED_METHOD internal_try_acquire( spin_mutex& m );

        //! Like release, but with ITT instrumentation.
        void __TBB_EXPORTED_METHOD internal_release();

        friend class spin_mutex;

    public:
        //! Construct without acquiring a mutex.
        scoped_lock() : my_mutex(NULL), my_unlock_value(0) {}

        //! Construct and acquire lock on a mutex.
        scoped_lock( spin_mutex& m ) : my_unlock_value(0) { 
#if TBB_USE_THREADING_TOOLS||TBB_USE_ASSERT
            my_mutex=NULL;
            internal_acquire(m);
#else
            __TBB_LockByte(m.flag);
            my_mutex=&m;
#endif /* TBB_USE_THREADING_TOOLS||TBB_USE_ASSERT*/
        }

        //! Acquire lock.
        void acquire( spin_mutex& m ) {
#if TBB_USE_THREADING_TOOLS||TBB_USE_ASSERT
            internal_acquire(m);
#else
            __TBB_LockByte(m.flag);
            my_mutex = &m;
#endif /* TBB_USE_THREADING_TOOLS||TBB_USE_ASSERT*/
        }

        //! Try acquiring lock (non-blocking)
        /** Return true if lock acquired; false otherwise. */
        bool try_acquire( spin_mutex& m ) {
#if TBB_USE_THREADING_TOOLS||TBB_USE_ASSERT
            return internal_try_acquire(m);
#else
            bool result = __TBB_TryLockByte(m.flag);
            if( result )
                my_mutex = &m;
            return result;
#endif /* TBB_USE_THREADING_TOOLS||TBB_USE_ASSERT*/
        }

        //! Release lock
        void release() {
#if TBB_USE_THREADING_TOOLS||TBB_USE_ASSERT
            internal_release();
#else
            __TBB_UnlockByte(my_mutex->flag, 0);
            my_mutex = NULL;
#endif /* TBB_USE_THREADING_TOOLS||TBB_USE_ASSERT */
        }

        //! Destroy lock.  If holding a lock, releases the lock first.
        ~scoped_lock() {
            if( my_mutex ) {
#if TBB_USE_THREADING_TOOLS||TBB_USE_ASSERT
                internal_release();
#else
                __TBB_UnlockByte(my_mutex->flag, 0);
#endif /* TBB_USE_THREADING_TOOLS||TBB_USE_ASSERT */
            }
        }
    };

    void __TBB_EXPORTED_METHOD internal_construct();

    // Mutex traits
    static const bool is_rw_mutex = false;
    static const bool is_recursive_mutex = false;
    static const bool is_fair_mutex = false;

    // ISO C++0x compatibility methods

    //! Acquire lock
    void lock() {
#if TBB_USE_THREADING_TOOLS
        aligned_space<scoped_lock,1> tmp;
        new(tmp.begin()) scoped_lock(*this);
#else
        __TBB_LockByte(flag);
#endif /* TBB_USE_THREADING_TOOLS*/
    }

    //! Try acquiring lock (non-blocking)
    /** Return true if lock acquired; false otherwise. */
    bool try_lock() {
#if TBB_USE_THREADING_TOOLS
        aligned_space<scoped_lock,1> tmp;
        return (new(tmp.begin()) scoped_lock)->internal_try_acquire(*this);
#else
        return __TBB_TryLockByte(flag);
#endif /* TBB_USE_THREADING_TOOLS*/
    }

    //! Release lock
    void unlock() {
#if TBB_USE_THREADING_TOOLS
        aligned_space<scoped_lock,1> tmp;
        scoped_lock& s = *tmp.begin();
        s.my_mutex = this;
        s.internal_release();
#else
        __TBB_store_with_release(flag, 0);
#endif /* TBB_USE_THREADING_TOOLS */
    }

    friend class scoped_lock;
};

__TBB_DEFINE_PROFILING_SET_NAME(spin_mutex)

} // namespace tbb

#endif /* __TBB_spin_mutex_H */
