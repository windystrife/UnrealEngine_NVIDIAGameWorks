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

#ifndef __TBB_tbbmalloc_internal_api_H
#define __TBB_tbbmalloc_internal_api_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void __TBB_mallocProcessShutdownNotification();
#if _WIN32||_WIN64
void __TBB_mallocThreadShutdownNotification();
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif // __TBB_tbbmalloc_internal_api_H
