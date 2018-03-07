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

#ifdef __cplusplus
#error For testing purpose, this file should be compiled with a C compiler, not C++
#endif /*__cplusplus */

#include "tbb/scalable_allocator.h"
#include <stdio.h>
#include <assert.h>

/*
 *  The test is to check if the scalable_allocator.h and its functions
 *  can be used from pure C programs; also some regression checks are done
 */

int main(void) {
    size_t i, j;
    void *p1, *p2;
    for( i=0; i<=1<<16; ++i) {
        p1 = scalable_malloc(i);
        if( !p1 )
            printf("Warning: there should be memory but scalable_malloc returned NULL\n");
        scalable_free(p1);
    }
    p1 = p2 = NULL;
    for( i=1024*1024; ; i/=2 )
    {
        scalable_free(p1);
        p1 = scalable_realloc(p2, i);
        p2 = scalable_calloc(i, 32);
        if (p2) {
            if (i<sizeof(size_t)) {
                for (j=0; j<i; j++)
                    assert(0==*((char*)p2+j));
            } else {
                for (j=0; j<i; j+=sizeof(size_t))
                    assert(0==*((size_t*)p2+j));
            }
        }
        scalable_free(p2);
        p2 = scalable_malloc(i);
        if (i==0) break;
    }
    for( i=1; i<1024*1024; i*=2 )
    {
        scalable_free(p1);
        p1 = scalable_realloc(p2, i);
        p2 = scalable_malloc(i);
    }
    scalable_free(p1);
    scalable_free(p2);
    printf("done\n");
    return 0;
}
