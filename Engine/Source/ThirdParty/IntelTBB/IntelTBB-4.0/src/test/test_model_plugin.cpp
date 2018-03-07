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

#include "tbb/tbb_config.h"

#if __TBB_DEFINE_MIC

#ifndef _USRDLL
#define HARNESS_NO_PARSE_COMMAND_LINE 1
#include "harness.h"
int TestMain() {
    return Harness::Skipped;
}
#endif

#else /* !__MIC__ */

#if _WIN32 || _WIN64
#include "tbb/machine/windows_api.h"
#else
#include <dlfcn.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#if !TBB_USE_EXCEPTIONS && _MSC_VER
    // Suppress "C++ exception handler used, but unwind semantics are not enabled" warning in STL headers
    #pragma warning (push)
    #pragma warning (disable: 4530)
#endif

#include <stdexcept>

#if !TBB_USE_EXCEPTIONS && _MSC_VER
    #pragma warning (pop)
#endif

#if TBB_USE_EXCEPTIONS
    #include "harness_report.h"
#endif

#ifdef _USRDLL
#include "tbb/task_scheduler_init.h"

class CModel {
public:
    CModel(void) {};
    static tbb::task_scheduler_init tbb_init;

    void init_and_terminate( int );
};

tbb::task_scheduler_init CModel::tbb_init(1);

//! Test that task::initialize and task::terminate work when doing nothing else.
/** maxthread is treated as the "maximum" number of worker threads. */
void CModel::init_and_terminate( int maxthread ) {
    for( int i=0; i<200; ++i ) {
        switch( i&3 ) {
            default: {
                tbb::task_scheduler_init init( rand() % maxthread + 1 );
                break;
            }
            case 0: {
                tbb::task_scheduler_init init;
                break;
            }
            case 1: {
                tbb::task_scheduler_init init( tbb::task_scheduler_init::automatic );
                break;
            }
            case 2: {
                tbb::task_scheduler_init init( tbb::task_scheduler_init::deferred );
                init.initialize( rand() % maxthread + 1 );
                init.terminate();
                break;
            }
        }
    }
}

extern "C"
#if _WIN32 || _WIN64
__declspec(dllexport)
#endif
void plugin_call(int maxthread)
{
    srand(2);
    __TBB_TRY {
        CModel model;
        model.init_and_terminate(maxthread);
    } __TBB_CATCH( std::runtime_error& error ) {
#if TBB_USE_EXCEPTIONS
        REPORT("ERROR: %s\n", error.what());
#endif /* TBB_USE_EXCEPTIONS */
    }
}

#else /* _USRDLL undefined */

#define HARNESS_NO_ASSERT 1
#include "harness.h"
#include "harness_dynamic_libs.h"

extern "C" void plugin_call(int);

void report_error_in(const char* function_name)
{
#if _WIN32 || _WIN64
    char* message;
    int code = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, code,MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (char*)&message, 0, NULL );
#else
    char* message = (char*)dlerror();
    int code = 0;
#endif
    REPORT( "%s failed with error %d: %s\n", function_name, code, message);

#if _WIN32 || _WIN64
    LocalFree(message);
#endif
}

int use_lot_of_tls() {
    int count = 0;
#if _WIN32 || _WIN64
    DWORD last_handles[10];
    DWORD result;
    result = TlsAlloc();
    while( result!=TLS_OUT_OF_INDEXES ) {
        last_handles[++count%10] = result;
        result = TlsAlloc();
    }
    for( int i=0; i<10; ++i )
        TlsFree(last_handles[i]);
#else
    pthread_key_t last_handles[10];
    pthread_key_t result;
    int setspecific_dummy=10;
    while( pthread_key_create(&result, NULL)==0 
           && count < 4096 ) // Sun Solaris doesn't have any built-in limit, so we set something big enough
    {
        last_handles[++count%10] = result;
        pthread_setspecific(result,&setspecific_dummy);
    }
    REMARK("Created %d keys\n", count);
    for( int i=0; i<10; ++i )
        pthread_key_delete(last_handles[i]);
#endif
    return count-10;
}

typedef void (*PLUGIN_CALL)(int);

#if __linux__
    #define RML_LIBRARY_NAME(base) TEST_LIBRARY_NAME(base) ".1"
#else
    #define RML_LIBRARY_NAME(base) TEST_LIBRARY_NAME(base)
#endif

int TestMain () {
#if !RML_USE_WCRM
    PLUGIN_CALL my_plugin_call;

    int tls_key_count = use_lot_of_tls();
    REMARK("%d thread local objects allocated in advance\n", tls_key_count);

    Harness::LIBRARY_HANDLE hLib;
#if _WIN32 || _WIN64
    hLib = LoadLibrary("irml.dll");
    if ( !hLib )
        hLib = LoadLibrary("irml_debug.dll");
    if ( !hLib )
        return Harness::Skipped; // No shared RML, skip the test
    FreeLibrary(hLib);
#else /* !WIN */
#if __TBB_ARENA_PER_MASTER
    hLib = dlopen(RML_LIBRARY_NAME("libirml"), RTLD_LAZY);
    if ( !hLib )
        hLib = dlopen(RML_LIBRARY_NAME("libirml_debug"), RTLD_LAZY);
    if ( !hLib )
        return Harness::Skipped;
    dlclose(hLib);
#endif /* __TBB_ARENA_PER_MASTER */
#endif /* OS */
    for( int i=1; i<100; ++i ) {  
        REMARK("Iteration %d, loading plugin library...\n", i);
        hLib = Harness::OpenLibrary(TEST_LIBRARY_NAME("test_model_plugin_dll"));
        if ( !hLib ) {
#if !__TBB_NO_IMPLICIT_LINKAGE
#if _WIN32 || _WIN64
            report_error_in("LoadLibrary");
#else
            report_error_in("dlopen");
#endif
            return -1;
#else
            return Harness::Skipped;
#endif
        }
        my_plugin_call = (PLUGIN_CALL)Harness::GetAddress(hLib, "plugin_call");
        if (my_plugin_call==NULL) {
#if _WIN32 || _WIN64
            report_error_in("GetProcAddress");
#else
            report_error_in("dlsym");
#endif
            return -1;
        }
        REMARK("Calling plugin method...\n");
        my_plugin_call(MaxThread);

        REMARK("Unloading plugin library...\n");
        Harness::CloseLibrary(hLib);
    } // end for(1,100)

    return Harness::Done;
#else
    return Harness::Skipped;
#endif /* !RML_USE_WCRM */
}

#endif//_USRDLL
#endif//__MIC__
