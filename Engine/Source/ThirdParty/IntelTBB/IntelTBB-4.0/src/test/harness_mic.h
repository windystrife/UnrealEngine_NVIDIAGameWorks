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

#ifndef tbb_test_harness_mic_H
#define tbb_test_harness_mic_H

#if ! __TBB_DEFINE_MIC
    #error test/harness_mic.h should be included only when building for Intel(R) Many Integrated Core Architecture
#endif

// test for unifed sources. See makefiles
#undef HARNESS_INCOMPLETE_SOURCES

#include <stdlib.h>
#include <stdio.h>

#define TBB_TEST_LOW_WORKLOAD 1

#define REPORT_FATAL_ERROR  REPORT
#define HARNESS_EXPORT

#if __TBB_MIC_NATIVE
    #define HARNESS_EXIT_ON_ASSERT 1
    #define __TBB_PLACEMENT_NEW_EXCEPTION_SAFETY_BROKEN 1
#else
    #define HARNESS_TERMINATE_ON_ASSERT 1
#endif

#endif /* tbb_test_harness_mic_H */
