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

// This file is compiled with C++, but linked with a program written in C.
// The intent is to find dependencies on the C++ run-time.

#include <stdlib.h>
#define RML_PURE_VIRTUAL_HANDLER abort

#if _MSC_VER==1500 && !defined(__INTEL_COMPILER)
// VS2008/VC9 seems to have an issue; 
#pragma warning( push )
#pragma warning( disable: 4100 ) 
#endif          
#include "rml_omp.h"
#if _MSC_VER==1500 && !defined(__INTEL_COMPILER)
#pragma warning( pop )
#endif

rml::versioned_object::version_type Version;

class MyClient: public __kmp::rml::omp_client {
public:
    /*override*/rml::versioned_object::version_type version() const {return 0;}
    /*override*/size_type max_job_count() const {return 1024;}
    /*override*/size_t min_stack_size() const {return 1<<20;}
    /*override*/rml::job* create_one_job() {return NULL;}
    /*override*/void acknowledge_close_connection() {}
    /*override*/void cleanup(job&) {}
    /*override*/policy_type policy() const {return throughput;}
    /*override*/void process( job&, void*, __kmp::rml::omp_client::size_type ) {}
   
};

//! Never actually set, because point of test is to find linkage issues.
__kmp::rml::omp_server* MyServerPtr;

#define HARNESS_NO_PARSE_COMMAND_LINE 1
#define HARNESS_CUSTOM_MAIN 1
#include "harness.h"

extern "C" void Cplusplus() {
    MyClient client;
    Version = client.version();
    REPORT("done\n");
}
