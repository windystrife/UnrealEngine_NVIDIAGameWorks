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

// Program for basic correctness testing of assembly-language routines.
#include "harness_defs.h"

#if __TBB_TEST_SKIP_BUILTINS_MODE
#include "harness.h"
int TestMain() {
    REPORT("Known issue: GCC builtins aren't available\n");
    return Harness::Skipped;
}
#else

#include "tbb/task.h"

#include <new>
#include "harness.h"

using tbb::internal::reference_count;

//! Test __TBB_CompareAndSwapW
static void TestCompareExchange() {
    ASSERT( intptr_t(-10)<10, "intptr_t not a signed integral type?" ); 
    REMARK("testing __TBB_CompareAndSwapW\n");
    for( intptr_t a=-10; a<10; ++a )
        for( intptr_t b=-10; b<10; ++b )
            for( intptr_t c=-10; c<10; ++c ) {
// Workaround for a bug in GCC 4.3.0; and one more is below.
#if __TBB_GCC_OPTIMIZER_ORDERING_BROKEN
                intptr_t x;
                __TBB_store_with_release( x, a );
#else
                intptr_t x = a;
#endif
                intptr_t y = __TBB_CompareAndSwapW(&x,b,c);
                ASSERT( y==a, NULL ); 
                if( a==c ) 
                    ASSERT( x==b, NULL );
                else
                    ASSERT( x==a, NULL );
            }
}

//! Test __TBB___TBB_FetchAndIncrement and __TBB___TBB_FetchAndDecrement
static void TestAtomicCounter() {
    // "canary" is a value used to detect illegal overwrites.
    const reference_count canary = ~(uintptr_t)0/3;
    REMARK("testing __TBB_FetchAndIncrement\n");
    struct {
        reference_count prefix, i, suffix;
    } x;
    x.prefix = canary;
    x.i = 0;
    x.suffix = canary;
    for( int k=0; k<10; ++k ) {
        reference_count j = __TBB_FetchAndIncrementWacquire((volatile void *)&x.i);
        ASSERT( x.prefix==canary, NULL );
        ASSERT( x.suffix==canary, NULL );
        ASSERT( x.i==k+1, NULL );
        ASSERT( j==k, NULL );
    }
    REMARK("testing __TBB_FetchAndDecrement\n");
    x.i = 10;
    for( int k=10; k>0; --k ) {
        reference_count j = __TBB_FetchAndDecrementWrelease((volatile void *)&x.i);
        ASSERT( j==k, NULL );
        ASSERT( x.i==k-1, NULL );
        ASSERT( x.prefix==canary, NULL );
        ASSERT( x.suffix==canary, NULL );
    }
}

static void TestTinyLock() {
    REMARK("testing __TBB_LockByte\n");
    __TBB_atomic_flag flags[16];
    for( unsigned int i=0; i<16; ++i )
        flags[i] = (__TBB_Flag)i;
#if __TBB_GCC_OPTIMIZER_ORDERING_BROKEN
    __TBB_store_with_release( flags[8], 0 );
#else
    flags[8] = 0;
#endif
    __TBB_LockByte(flags[8]);
    for( unsigned int i=0; i<16; ++i )
        #ifdef __sparc
        ASSERT( flags[i]==(i==8?0xff:i), NULL );
        #else
        ASSERT( flags[i]==(i==8?1:i), NULL );
        #endif
    __TBB_UnlockByte(flags[8], 0);
    for( unsigned int i=0; i<16; ++i )
        ASSERT( flags[i] == (i==8?0:i), NULL );
}

static void TestLog2() {
    REMARK("testing __TBB_Log2\n");
    for( uintptr_t i=1; i; i<<=1 ) {
        for( uintptr_t j=1; j<1<<16; ++j ) {
            if( uintptr_t k = i*j ) {
                uintptr_t actual = __TBB_Log2(k);
                const uintptr_t ONE = 1; // warning suppression again
                ASSERT( k >= ONE<<actual, NULL );          
                ASSERT( k>>1 < ONE<<actual, NULL );        
            }
        }
    }
}

static void TestPause() {
    REMARK("testing __TBB_Pause\n");
    __TBB_Pause(1);
}

int TestMain () {
    __TBB_TRY {
        TestLog2();
        TestTinyLock();
        TestCompareExchange();
        TestAtomicCounter();
        TestPause();
    } __TBB_CATCH(...) {
        ASSERT(0,"unexpected exception");
    }
    return Harness::Done;
}
#endif // __TBB_TEST_SKIP_BUILTINS_MODE
