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

/*  This compilation unit provides definition of task::destroy( task& )
    that is binary compatible with TBB 2.x. In TBB 3.0, the method became
    static, and its name decoration changed, though the definition remained.

    The macro switch should be set prior to including task.h
    or any TBB file that might bring task.h up.
*/
#define __TBB_DEPRECATED_TASK_INTERFACE 1
#include "tbb/task.h"

namespace tbb {

void task::destroy( task& victim ) {
    // Forward to static version
    task_base::destroy( victim );
}

} // namespace tbb
