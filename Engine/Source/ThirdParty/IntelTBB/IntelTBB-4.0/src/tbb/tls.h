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

#ifndef _TBB_tls_H
#define _TBB_tls_H

#if USE_PTHREAD
#include <pthread.h>
#else /* assume USE_WINTHREAD */
#include "tbb/machine/windows_api.h"
#endif

namespace tbb {

namespace internal {

typedef void (*tls_dtor_t)(void*);

//! Basic cross-platform wrapper class for TLS operations.
template <typename T>
class basic_tls {
#if USE_PTHREAD
    typedef pthread_key_t tls_key_t;
public:
    int  create( tls_dtor_t dtor = NULL ) {
        return pthread_key_create(&my_key, dtor);
    }
    int  destroy()      { return pthread_key_delete(my_key); }
    void set( T value ) { pthread_setspecific(my_key, (void*)value); }
    T    get()          { return (T)pthread_getspecific(my_key); }
#else /* USE_WINTHREAD */
    typedef DWORD tls_key_t;
public:
    int create() {
        tls_key_t tmp = TlsAlloc();
        if( tmp==TLS_OUT_OF_INDEXES )
            return TLS_OUT_OF_INDEXES;
        my_key = tmp;
        return 0;
    }
    int  destroy()      { TlsFree(my_key); my_key=0; return 0; }
    void set( T value ) { TlsSetValue(my_key, (LPVOID)value); }
    T    get()          { return (T)TlsGetValue(my_key); }
#endif
private:
    tls_key_t my_key;
};

//! More advanced TLS support template class.
/** It supports RAII and to some extent mimic __declspec(thread) variables. */
template <typename T>
class tls : public basic_tls<T> {
    typedef basic_tls<T> base;
public:
    tls()  { base::create();  }
    ~tls() { base::destroy(); }
    T operator=(T value) { base::set(value); return value; }
    operator T() { return base::get(); }
};

template <typename T>
class tls<T*> : basic_tls<T*> {
    typedef basic_tls<T*> base;
    static void internal_dtor(void* ptr) {
        if (ptr) delete (T*)ptr;
    }
    T* internal_get() {
        T* result = base::get();
        if (!result) {
            result = new T;
            base::set(result);
        }
        return result;
    }
public:
    tls()  {
#if USE_PTHREAD
        base::create( internal_dtor );
#else
        base::create();
#endif
    }
    ~tls() { base::destroy(); }
    T* operator=(T* value) { base::set(value); return value; }
    operator T*()   { return  internal_get(); }
    T* operator->() { return  internal_get(); }
    T& operator*()  { return *internal_get(); }
};

} // namespace internal

} // namespace tbb

#endif /* _TBB_tls_H */
