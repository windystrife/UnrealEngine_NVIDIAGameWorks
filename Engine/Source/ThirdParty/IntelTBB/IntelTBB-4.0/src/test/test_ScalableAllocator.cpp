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

// Test whether scalable_allocator complies with the requirements in 20.1.5 of ISO C++ Standard (1998).

#define __TBB_EXTRA_DEBUG 1 // enables additional checks
#define TBB_PREVIEW_MEMORY_POOL 1

#include "harness_assert.h"
#if __linux__  && __ia64__
// Currently pools high-level interface has dependency to TBB library
// to get atomics. For sake of testing add rudementary implementation of them.
#include "harness_tbb_independence.h"
#endif
#include "tbb/memory_pool.h"
#include "tbb/scalable_allocator.h"

#if __TBB_SOURCE_DIRECTLY_INCLUDED && (_WIN32||_WIN64)
#include "../tbbmalloc/tbbmalloc_internal_api.h"
#define __TBBMALLOC_CALL_THREAD_SHUTDOWN 1
#endif
// the actual body of the test is there:
#include "test_allocator.h"
#include "harness_allocator.h"

#if _MSC_VER
#include "tbb/machine/windows_api.h"
#endif /* _MSC_VER */

typedef static_counting_allocator<tbb::memory_pool_allocator<char> > cnt_alloc_t;
typedef local_counting_allocator<std::allocator<char> > cnt_provider_t;
class MinimalAllocator : cnt_provider_t {
public:
    typedef char value_type;
    MinimalAllocator() {
        REMARK("%p::ctor\n", this);
    }
    MinimalAllocator(const MinimalAllocator&s) : cnt_provider_t(s) {
        REMARK("%p::ctor(%p)\n", this, &s);
    }
    ~MinimalAllocator() {
        REMARK("%p::dtor: alloc=%u/%u free=%u/%u\n", this,
            unsigned(items_allocated),unsigned(allocations),
            unsigned(items_freed), unsigned(frees) );
        ASSERT(allocations==frees && items_allocated==items_freed,0);
        if( allocations ) { // non-temporal copy
            // TODO: describe consumption requirements
            ASSERT(items_allocated>cnt_alloc_t::items_allocated, 0);
        }
    }
    void *allocate(size_t sz) {
        void *p = cnt_provider_t::allocate(sz);
        REMARK("%p::allocate(%u) = %p\n", this, unsigned(sz), p);
        return p;
    }
    void deallocate(void *p, size_t sz) {
        ASSERT(allocations>frees,0);
        REMARK("%p::deallocate(%p, %u)\n", this, p, unsigned(sz));
        cnt_provider_t::deallocate(cnt_provider_t::pointer(p), sz);
    }
};

class NullAllocator {
public:
    typedef char value_type;
    NullAllocator() { }
    NullAllocator(const NullAllocator&) { }
    ~NullAllocator() { }
    void *allocate(size_t) { return NULL; }
    void deallocate(void *, size_t) { ASSERT(0, NULL); }
};

void TestZeroSpaceMemoryPool()
{
    try {
        tbb::memory_pool<NullAllocator> pool;
        ASSERT(0, "Useless allocator with no memory must not be created");
    } catch (std::bad_alloc) {
    } catch (...) {
        ASSERT(0, "wrong exception type; expected bad_alloc");
    }
}

/* test that pools in small space are either usable or not created
   (i.e., exception raised) */
void TestSmallFixedSizePool()
{
    char *buf;
    bool allocated = false;

    for (size_t sz = 0; sz < 64*1024; sz = sz? 3*sz : 3) {
        buf = (char*)malloc(sz);
        try {
            tbb::fixed_pool pool(buf, sz);
            allocated = pool.malloc( 16 ) || pool.malloc( 9*1024 );
            ASSERT(allocated, NULL);
        } catch (std::bad_alloc) {
        } catch (...) {
            ASSERT(0, "wrong exception type; expected bad_alloc");
        }
        free(buf);
    }
    ASSERT(allocated, "Maximal buf size should be enough to create working fixed_pool");
    try {
        tbb::fixed_pool pool(NULL, 10*1024*1024);
        ASSERT(0, "Useless allocator with no memory must not be created");
    } catch (std::bad_alloc) {
    } catch (...) {
        ASSERT(0, "wrong exception type; expected bad_alloc");
    }
}

int TestMain () {
#if _MSC_VER && !__TBBMALLOC_NO_IMPLICIT_LINKAGE
    #ifdef _DEBUG
        ASSERT(!GetModuleHandle("tbbmalloc.dll") && GetModuleHandle("tbbmalloc_debug.dll"),
            "test linked with wrong (non-debug) tbbmalloc library");
    #else
        ASSERT(!GetModuleHandle("tbbmalloc_debug.dll") && GetModuleHandle("tbbmalloc.dll"),
            "test linked with wrong (debug) tbbmalloc library");
    #endif
#endif /* _MSC_VER && !__TBBMALLOC_NO_IMPLICIT_LINKAGE */
    int result = TestMain<tbb::scalable_allocator<void> >();
    {
        tbb::memory_pool<tbb::scalable_allocator<int> > pool;
        result += TestMain(tbb::memory_pool_allocator<void>(pool) );
    }{
        tbb::memory_pool<MinimalAllocator> pool;
        cnt_alloc_t alloc(( tbb::memory_pool_allocator<char>(pool) )); // double parentheses to avoid function declaration
        result += TestMain(alloc);
    }{
        static char buf[1024*1024*4];
        tbb::fixed_pool pool(buf, sizeof(buf));
        const char *text = "this is a test";// 15 bytes
        char *p1 = (char*)pool.malloc( 16 );
        ASSERT(p1, NULL);
        strcpy(p1, text);
        char *p2 = (char*)pool.realloc( p1, 15 );
        ASSERT( p2 && !strcmp(p2, text), "realloc broke memory" );

        result += TestMain(tbb::memory_pool_allocator<void>(pool) );

        // try allocate almost entire buf keeping some reasonable space for internals
        char *p3 = (char*)pool.realloc( p2, sizeof(buf)-128*1024 );
        ASSERT( p3, "defragmentation failed" );
        ASSERT( !strcmp(p3, text), "realloc broke memory" );
        for( size_t sz = 10; sz < sizeof(buf); sz *= 2) {
            ASSERT( pool.malloc( sz ), NULL);
            pool.recycle();
        }

        result += TestMain(tbb::memory_pool_allocator<void>(pool) );
    }
    TestSmallFixedSizePool();
    TestZeroSpaceMemoryPool();

    ASSERT( !result, NULL );
    return Harness::Done;
}
