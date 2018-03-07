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

/* Regression test against bug in TBB allocator, manifested when 
   dynamic library calls atexit or register dtors of static objects.
   If the allocator is not initialized yet, we can got deadlock, 
   because allocator library has static object dtors as well, they
   registred during allocator initialization, and atexit is protected 
   by non-recursive mutex in some GLIBCs.
 */

#if _USRDLL

#include <stdlib.h>

#if _WIN32||_WIN64
// isMallocOverloaded must be defined in DLL to linker not drop the dependence
// to the DLL.
extern __declspec(dllexport) bool isMallocOverloaded();

bool isMallocOverloaded()
{
    return true;
}

#else

#include <dlfcn.h>

bool isMallocOverloaded()
{
    return dlsym(RTLD_DEFAULT, "__TBB_malloc_proxy");
}

#endif    

#ifndef _PGO_INSTRUMENT
void dummyFunction() {}

class Foo {
public:
    Foo() {
        // add a lot of exit handlers to cause memory allocation
        for (int i=0; i<1024; i++)
            atexit(dummyFunction);
    }
};

static Foo f;
#endif

#else // _USRDLL
#include "harness.h"

#if _WIN32||_WIN64
extern __declspec(dllimport)
#endif
bool isMallocOverloaded();

int TestMain () {
#ifdef _PGO_INSTRUMENT
    REPORT("Known issue: test_malloc_atexit hangs if compiled with -prof-genx\n");
    return Harness::Skipped;
#else
    return isMallocOverloaded()? Harness::Done : Harness::Skipped;
#endif
}

#endif // _USRDLL
