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

// Test whether cache_aligned_allocator works with some of the host's STL containers.

#include "tbb/cache_aligned_allocator.h"
#include "tbb/tbb_allocator.h"

#define HARNESS_NO_PARSE_COMMAND_LINE 1
// the real body of the test is there:
#include "test_allocator.h"

template<>
struct is_zero_filling<tbb::zero_allocator<void> > {
    static const bool value = true;
};

// Test that NFS_Allocate() throws bad_alloc if cannot allocate memory.
void Test_NFS_Allocate_Throws() {
#if TBB_USE_EXCEPTIONS && !__TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN
    using namespace tbb::internal;

    bool exception_caught = false;
    // First, allocate a reasonably big amount of memory, big enough
    // to not cause warp around in system allocator after adding object header
    // during address2 allocation.
    const size_t itemsize = 1024;
    const size_t nitems   = 1024;
    void *address1 = NULL;
    try {
        address1 = NFS_Allocate( nitems, itemsize, NULL );
    } catch( ... ) {
        address1 = NULL;
    }
    ASSERT( address1, "NFS_Allocate unable to obtain 32*1024 bytes" );

    void *address2 = NULL;
    try {
        // Try allocating more memory than left in the address space; should cause std::bad_alloc
        address2 = NFS_Allocate( 1, ~size_t(0) - itemsize*nitems + NFS_GetLineSize(), NULL);
    } catch( std::bad_alloc ) {
        exception_caught = true;
    } catch( ... ) {
        ASSERT( __TBB_EXCEPTION_TYPE_INFO_BROKEN, "Unexpected exception type (std::bad_alloc was expected)" );
        exception_caught = true;
    }
    ASSERT( exception_caught, "NFS_Allocate did not throw bad_alloc" );
    ASSERT( !address2, "NFS_Allocate returned garbage?" );

    try {
        NFS_Free( address1 );
    } catch( ... ) {
        ASSERT( false, "NFS_Free did not accept the address obtained with NFS_Allocate" );
    }
#endif /* TBB_USE_EXCEPTIONS && !__TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN */
}

int TestMain () {
    int result = TestMain<tbb::cache_aligned_allocator<void> >();
    result += TestMain<tbb::tbb_allocator<void> >();
    result += TestMain<tbb::zero_allocator<void> >();
    ASSERT( !result, NULL );
    Test_NFS_Allocate_Throws();
    return Harness::Done;
}
