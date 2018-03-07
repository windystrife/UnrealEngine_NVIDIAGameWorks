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

#if _USRDLL

#include <stdlib.h>
#include "harness_defs.h"
#include "tbb/scalable_allocator.h"
#if __TBB_SOURCE_DIRECTLY_INCLUDED
#include "../tbbmalloc/tbbmalloc_internal_api.h"
#endif

#define HARNESS_CUSTOM_MAIN 1
#define HARNESS_NO_PARSE_COMMAND_LINE 1
#include "harness.h"
#include "harness_assert.h"

#if _WIN32||_WIN64
extern "C" {
    extern __declspec(dllexport) void callDll();
}
#endif

extern "C" void callDll()
{
    static const int NUM = 20;
    void *ptrs[NUM];

    for (int i=0; i<NUM; i++) {
        ptrs[i] = scalable_malloc(i*1024);
        ASSERT(ptrs[i], NULL);
    }
    for (int i=0; i<NUM; i++)
        scalable_free(ptrs[i]);
#if __TBB_SOURCE_DIRECTLY_INCLUDED && (_WIN32||_WIN64)
    __TBB_mallocThreadShutdownNotification();
#endif
}

#if __TBB_SOURCE_DIRECTLY_INCLUDED

struct RegisterProcessShutdownNotification {
    ~RegisterProcessShutdownNotification() {
        __TBB_mallocProcessShutdownNotification();
    }
};

static RegisterProcessShutdownNotification reg;

#endif

#else // _USRDLL

#define __TBB_NO_IMPLICIT_LINKAGE 1
#define HARNESS_NO_PARSE_COMMAND_LINE 1
#include "harness.h"
#include "harness_memory.h"
#include "harness_tbb_independence.h"
#include "harness_barrier.h"
#include "harness_dynamic_libs.h"

class UseDll {
    Harness::FunctionAddress run;
public:
    UseDll(Harness::FunctionAddress runPtr) : run(runPtr) { }
    void operator()( int /*id*/ ) const {
        (*run)();
    }
};

void LoadThreadsUnload()
{
    Harness::LIBRARY_HANDLE lib =
        Harness::OpenLibrary(TEST_LIBRARY_NAME("test_malloc_used_by_lib"));
    ASSERT(lib, "Can't load " TEST_LIBRARY_NAME("test_malloc_used_by_lib"));
    NativeParallelFor( 4, UseDll( Harness::GetAddress(lib, "callDll") ) );
    Harness::CloseLibrary(lib);
}

struct UnloadCallback {
    Harness::LIBRARY_HANDLE lib;

    void operator() () const {
        Harness::CloseLibrary(lib);
    }
};

struct RunWithLoad : NoAssign {
    static Harness::SpinBarrier startBarr, endBarr;
    static UnloadCallback unloadCallback;
    static Harness::FunctionAddress runPtr;

    void operator()(int id) const {
        if (!id) {
            Harness::LIBRARY_HANDLE lib =
                Harness::OpenLibrary(TEST_LIBRARY_NAME("test_malloc_used_by_lib"));
            ASSERT(lib, "Can't load "TEST_LIBRARY_NAME("test_malloc_used_by_lib"));
            runPtr = Harness::GetAddress(lib, "callDll");
            unloadCallback.lib = lib;
        }
        startBarr.wait();
        (*runPtr)();
        endBarr.wait(unloadCallback);
    }
};

Harness::SpinBarrier RunWithLoad::startBarr, RunWithLoad::endBarr;
UnloadCallback RunWithLoad::unloadCallback;
Harness::FunctionAddress RunWithLoad::runPtr;

void ThreadsLoadUnload()
{
    const int threads = 4;

    RunWithLoad::startBarr.initialize(threads);
    RunWithLoad::endBarr.initialize(threads);
    NativeParallelFor(threads, RunWithLoad());
}

int TestMain () {
    const int ITERS = 20;
    int i;
    std::ptrdiff_t memory_leak = 0;

    GetMemoryUsage();

    for (int run = 0; run<2; run++) {
        // expect that memory consumption stabilized after several runs
        for (i=0; i<ITERS; i++) {
            std::size_t memory_in_use = GetMemoryUsage();
            if (run)
                LoadThreadsUnload();
            else
                ThreadsLoadUnload();
            memory_leak = GetMemoryUsage() - memory_in_use;
            if (memory_leak == 0)  // possibly too strong?
                break;
        }
        if(i==ITERS) {
            // not stabilized, could be leak
            REPORT( "Error: memory leak of up to %ld bytes\n", static_cast<long>(memory_leak));
            exit(1);
        }
    }

    return Harness::Done;
}

#endif // _USRDLL
