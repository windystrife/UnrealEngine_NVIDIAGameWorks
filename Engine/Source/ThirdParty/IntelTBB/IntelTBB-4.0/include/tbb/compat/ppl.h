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

#ifndef __TBB_compat_ppl_H
#define __TBB_compat_ppl_H

#include "../task_group.h"
#include "../parallel_invoke.h"
#include "../parallel_for_each.h"
#include "../parallel_for.h"
#include "../tbb_exception.h"
#include "../critical_section.h"
#include "../reader_writer_lock.h"
#include "../combinable.h"

namespace Concurrency {

#if __TBB_TASK_GROUP_CONTEXT
    using tbb::task_handle;
    using tbb::task_group_status;
    using tbb::task_group;
    using tbb::structured_task_group;
    using tbb::invalid_multiple_scheduling;
    using tbb::missing_wait;
    using tbb::make_task;

    using tbb::not_complete;
    using tbb::complete;
    using tbb::canceled;

    using tbb::is_current_task_group_canceling;
#endif /* __TBB_TASK_GROUP_CONTEXT */

    using tbb::parallel_invoke;
    using tbb::strict_ppl::parallel_for;
    using tbb::parallel_for_each;
    using tbb::critical_section;
    using tbb::reader_writer_lock;
    using tbb::combinable;

    using tbb::improper_lock;

} // namespace Concurrency

#endif /* __TBB_compat_ppl_H */
