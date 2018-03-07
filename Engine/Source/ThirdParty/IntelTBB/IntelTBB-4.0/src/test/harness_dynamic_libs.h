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

#if _WIN32 || _WIN64
#include "tbb/machine/windows_api.h"
#else
#include <dlfcn.h>
#endif

namespace Harness {

#if _WIN32 || _WIN64
typedef  HMODULE LIBRARY_HANDLE;
#else
typedef void *LIBRARY_HANDLE;
#endif

#if _WIN32 || _WIN64
#define TEST_LIBRARY_NAME(base) base".dll"
#elif __APPLE__
#define TEST_LIBRARY_NAME(base) base".dylib"
#else
#define TEST_LIBRARY_NAME(base) base".so"
#endif

LIBRARY_HANDLE OpenLibrary(const char *name)
{
#if _WIN32 || _WIN64
    return ::LoadLibrary(name);
#else
    return dlopen(name, RTLD_NOW|RTLD_GLOBAL);
#endif
}

void CloseLibrary(LIBRARY_HANDLE lib)
{
#if _WIN32 || _WIN64
    BOOL ret = FreeLibrary(lib);
    ASSERT(ret, "FreeLibrary must be successful");
#else
    int ret = dlclose(lib);
    ASSERT(ret == 0, "dlclose must be successful");
#endif
}

typedef void (*FunctionAddress)();

FunctionAddress GetAddress(Harness::LIBRARY_HANDLE lib, const char *name)
{
    union { FunctionAddress func; void *symb; } converter;
#if _WIN32 || _WIN64
    converter.symb = (void*)GetProcAddress(lib, name);
#else
    converter.symb = (void*)dlsym(lib, name);
#endif
    ASSERT(converter.func, "Can't find required symbol in dynamic library");
    return converter.func;
}

}  // namespace Harness
