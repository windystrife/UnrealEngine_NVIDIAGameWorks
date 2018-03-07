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

#define THREAD tbb::tbb_thread
#define THIS_THREAD tbb::this_tbb_thread
#define THIS_THREAD_SLEEP THIS_THREAD::sleep
#include "test_thread.h"
#include "harness.h"

/* we want to test tbb::tbb_thread */
int TestMain () {
    CheckSignatures();
    RunTests();
    return Harness::Done;
}
