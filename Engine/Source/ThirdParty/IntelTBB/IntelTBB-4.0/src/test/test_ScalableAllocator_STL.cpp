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

// Test whether scalable_allocator works with some of the host's STL containers.

#define HARNESS_NO_PARSE_COMMAND_LINE 1
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

// The actual body of the test is there:
#include "test_allocator_STL.h"

int TestMain () {
    TestAllocatorWithSTL<tbb::scalable_allocator<void> >();
    tbb::memory_pool<tbb::scalable_allocator<int> > mpool;
    TestAllocatorWithSTL(tbb::memory_pool_allocator<void>(mpool) );
    static char buf[1024*1024*4];
    tbb::fixed_pool fpool(buf, sizeof(buf));
    TestAllocatorWithSTL(tbb::memory_pool_allocator<void>(fpool) );
    return Harness::Done;
}
