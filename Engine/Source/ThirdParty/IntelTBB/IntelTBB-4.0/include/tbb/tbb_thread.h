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

#ifndef __TBB_tbb_thread_H
#define __TBB_tbb_thread_H

#if _WIN32||_WIN64
#include "machine/windows_api.h"
#define __TBB_NATIVE_THREAD_ROUTINE unsigned WINAPI
#define __TBB_NATIVE_THREAD_ROUTINE_PTR(r) unsigned (WINAPI* r)( void* )
#else
#define __TBB_NATIVE_THREAD_ROUTINE void*
#define __TBB_NATIVE_THREAD_ROUTINE_PTR(r) void* (*r)( void* )
#include <pthread.h>
#endif // _WIN32||_WIN64

#include "tbb_stddef.h"
#include "tick_count.h"

#if !TBB_USE_EXCEPTIONS && _MSC_VER
    // Suppress "C++ exception handler used, but unwind semantics are not enabled" warning in STL headers
    #pragma warning (push)
    #pragma warning (disable: 4530)
#endif

#include <iosfwd>

#if !TBB_USE_EXCEPTIONS && _MSC_VER
    #pragma warning (pop)
#endif

namespace tbb {

//! @cond INTERNAL
namespace internal {
    
    class tbb_thread_v3;

} // namespace internal

inline void swap( internal::tbb_thread_v3& t1, internal::tbb_thread_v3& t2 ); 

namespace internal {

    //! Allocate a closure
    void* __TBB_EXPORTED_FUNC allocate_closure_v3( size_t size );
    //! Free a closure allocated by allocate_closure_v3
    void __TBB_EXPORTED_FUNC free_closure_v3( void* );
   
    struct thread_closure_base {
        void* operator new( size_t size ) {return allocate_closure_v3(size);}
        void operator delete( void* ptr ) {free_closure_v3(ptr);}
    };

    template<class F> struct thread_closure_0: thread_closure_base {
        F function;

        static __TBB_NATIVE_THREAD_ROUTINE start_routine( void* c ) {
            thread_closure_0 *self = static_cast<thread_closure_0*>(c);
            self->function();
            delete self;
            return 0;
        }
        thread_closure_0( const F& f ) : function(f) {}
    };
    //! Structure used to pass user function with 1 argument to thread.  
    template<class F, class X> struct thread_closure_1: thread_closure_base {
        F function;
        X arg1;
        //! Routine passed to Windows's _beginthreadex by thread::internal_start() inside tbb.dll
        static __TBB_NATIVE_THREAD_ROUTINE start_routine( void* c ) {
            thread_closure_1 *self = static_cast<thread_closure_1*>(c);
            self->function(self->arg1);
            delete self;
            return 0;
        }
        thread_closure_1( const F& f, const X& x ) : function(f), arg1(x) {}
    };
    template<class F, class X, class Y> struct thread_closure_2: thread_closure_base {
        F function;
        X arg1;
        Y arg2;
        //! Routine passed to Windows's _beginthreadex by thread::internal_start() inside tbb.dll
        static __TBB_NATIVE_THREAD_ROUTINE start_routine( void* c ) {
            thread_closure_2 *self = static_cast<thread_closure_2*>(c);
            self->function(self->arg1, self->arg2);
            delete self;
            return 0;
        }
        thread_closure_2( const F& f, const X& x, const Y& y ) : function(f), arg1(x), arg2(y) {}
    };

    //! Versioned thread class.
    class tbb_thread_v3 {
        tbb_thread_v3(const tbb_thread_v3&); // = delete;   // Deny access
    public:
#if _WIN32||_WIN64
        typedef HANDLE native_handle_type; 
#else
        typedef pthread_t native_handle_type; 
#endif // _WIN32||_WIN64

        class id;
        //! Constructs a thread object that does not represent a thread of execution. 
        tbb_thread_v3() : my_handle(0)
#if _WIN32||_WIN64
            , my_thread_id(0)
#endif // _WIN32||_WIN64
        {}
        
        //! Constructs an object and executes f() in a new thread
        template <class F> explicit tbb_thread_v3(F f) {
            typedef internal::thread_closure_0<F> closure_type;
            internal_start(closure_type::start_routine, new closure_type(f));
        }
        //! Constructs an object and executes f(x) in a new thread
        template <class F, class X> tbb_thread_v3(F f, X x) {
            typedef internal::thread_closure_1<F,X> closure_type;
            internal_start(closure_type::start_routine, new closure_type(f,x));
        }
        //! Constructs an object and executes f(x,y) in a new thread
        template <class F, class X, class Y> tbb_thread_v3(F f, X x, Y y) {
            typedef internal::thread_closure_2<F,X,Y> closure_type;
            internal_start(closure_type::start_routine, new closure_type(f,x,y));
        }

        tbb_thread_v3& operator=(tbb_thread_v3& x) {
            if (joinable()) detach();
            my_handle = x.my_handle;
            x.my_handle = 0;
#if _WIN32||_WIN64
            my_thread_id = x.my_thread_id;
            x.my_thread_id = 0;
#endif // _WIN32||_WIN64
            return *this;
        }
        void swap( tbb_thread_v3& t ) {tbb::swap( *this, t );}
        bool joinable() const {return my_handle!=0; }
        //! The completion of the thread represented by *this happens before join() returns.
        void __TBB_EXPORTED_METHOD join();
        //! When detach() returns, *this no longer represents the possibly continuing thread of execution.
        void __TBB_EXPORTED_METHOD detach();
        ~tbb_thread_v3() {if( joinable() ) detach();}
        inline id get_id() const;
        native_handle_type native_handle() { return my_handle; }
    
        //! The number of hardware thread contexts.
        /** Before TBB 3.0 U4 this methods returned the number of logical CPU in
            the system. Currently on Windows, Linux and FreeBSD it returns the
            number of logical CPUs available to the current process in accordance
            with its affinity mask.
            
            NOTE: The return value of this method never changes after its first
            invocation. This means that changes in the process affinity mask that
            took place after this method was first invoked will not affect the
            number of worker threads in the TBB worker threads pool. **/
        static unsigned __TBB_EXPORTED_FUNC hardware_concurrency();
    private:
        native_handle_type my_handle; 
#if _WIN32||_WIN64
        DWORD my_thread_id;
#endif // _WIN32||_WIN64

        /** Runs start_routine(closure) on another thread and sets my_handle to the handle of the created thread. */
        void __TBB_EXPORTED_METHOD internal_start( __TBB_NATIVE_THREAD_ROUTINE_PTR(start_routine), 
                             void* closure );
        friend void __TBB_EXPORTED_FUNC move_v3( tbb_thread_v3& t1, tbb_thread_v3& t2 );
        friend void tbb::swap( tbb_thread_v3& t1, tbb_thread_v3& t2 ); 
    };
        
    class tbb_thread_v3::id { 
#if _WIN32||_WIN64
        DWORD my_id;
        id( DWORD id_ ) : my_id(id_) {}
#else
        pthread_t my_id;
        id( pthread_t id_ ) : my_id(id_) {}
#endif // _WIN32||_WIN64
        friend class tbb_thread_v3;
    public:
        id() : my_id(0) {}

        friend bool operator==( tbb_thread_v3::id x, tbb_thread_v3::id y );
        friend bool operator!=( tbb_thread_v3::id x, tbb_thread_v3::id y );
        friend bool operator<( tbb_thread_v3::id x, tbb_thread_v3::id y );
        friend bool operator<=( tbb_thread_v3::id x, tbb_thread_v3::id y );
        friend bool operator>( tbb_thread_v3::id x, tbb_thread_v3::id y );
        friend bool operator>=( tbb_thread_v3::id x, tbb_thread_v3::id y );
        
        template<class charT, class traits>
        friend std::basic_ostream<charT, traits>&
        operator<< (std::basic_ostream<charT, traits> &out, 
                    tbb_thread_v3::id id)
        {
            out << id.my_id;
            return out;
        }
        friend tbb_thread_v3::id __TBB_EXPORTED_FUNC thread_get_id_v3();
    }; // tbb_thread_v3::id

    tbb_thread_v3::id tbb_thread_v3::get_id() const {
#if _WIN32||_WIN64
        return id(my_thread_id);
#else
        return id(my_handle);
#endif // _WIN32||_WIN64
    }
    void __TBB_EXPORTED_FUNC move_v3( tbb_thread_v3& t1, tbb_thread_v3& t2 );
    tbb_thread_v3::id __TBB_EXPORTED_FUNC thread_get_id_v3();
    void __TBB_EXPORTED_FUNC thread_yield_v3();
    void __TBB_EXPORTED_FUNC thread_sleep_v3(const tick_count::interval_t &i);

    inline bool operator==(tbb_thread_v3::id x, tbb_thread_v3::id y)
    {
        return x.my_id == y.my_id;
    }
    inline bool operator!=(tbb_thread_v3::id x, tbb_thread_v3::id y)
    {
        return x.my_id != y.my_id;
    }
    inline bool operator<(tbb_thread_v3::id x, tbb_thread_v3::id y)
    {
        return x.my_id < y.my_id;
    }
    inline bool operator<=(tbb_thread_v3::id x, tbb_thread_v3::id y)
    {
        return x.my_id <= y.my_id;
    }
    inline bool operator>(tbb_thread_v3::id x, tbb_thread_v3::id y)
    {
        return x.my_id > y.my_id;
    }
    inline bool operator>=(tbb_thread_v3::id x, tbb_thread_v3::id y)
    {
        return x.my_id >= y.my_id;
    }

} // namespace internal;

//! Users reference thread class by name tbb_thread
typedef internal::tbb_thread_v3 tbb_thread;

using internal::operator==;
using internal::operator!=;
using internal::operator<;
using internal::operator>;
using internal::operator<=;
using internal::operator>=;

inline void move( tbb_thread& t1, tbb_thread& t2 ) {
    internal::move_v3(t1, t2);
}

inline void swap( internal::tbb_thread_v3& t1, internal::tbb_thread_v3& t2 ) {
    tbb::tbb_thread::native_handle_type h = t1.my_handle;
    t1.my_handle = t2.my_handle;
    t2.my_handle = h;
#if _WIN32||_WIN64
    DWORD i = t1.my_thread_id;
    t1.my_thread_id = t2.my_thread_id;
    t2.my_thread_id = i;
#endif /* _WIN32||_WIN64 */
}

namespace this_tbb_thread {
    inline tbb_thread::id get_id() { return internal::thread_get_id_v3(); }
    //! Offers the operating system the opportunity to schedule another thread.
    inline void yield() { internal::thread_yield_v3(); }
    //! The current thread blocks at least until the time specified.
    inline void sleep(const tick_count::interval_t &i) { 
        internal::thread_sleep_v3(i);  
    }
}  // namespace this_tbb_thread

} // namespace tbb

#endif /* __TBB_tbb_thread_H */
