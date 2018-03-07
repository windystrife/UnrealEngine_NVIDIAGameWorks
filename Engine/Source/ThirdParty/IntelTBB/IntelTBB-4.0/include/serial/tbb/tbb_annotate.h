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

#ifndef __TBB_annotate_H
#define __TBB_annotate_H

// Macros used by the Intel(R) Parallel Advisor.
#ifdef __TBB_NORMAL_EXECUTION
    #define ANNOTATE_SITE_BEGIN( site )
    #define ANNOTATE_SITE_END( site )
    #define ANNOTATE_TASK_BEGIN( task )
    #define ANNOTATE_TASK_END( task )
    #define ANNOTATE_LOCK_ACQUIRE( lock )
    #define ANNOTATE_LOCK_RELEASE( lock )
#else
    #include <advisor-annotate.h>
#endif

#endif /* __TBB_annotate_H */
