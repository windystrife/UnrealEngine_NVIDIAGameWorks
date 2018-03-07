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

#ifndef __TBB_parallel_for_each_H
#define __TBB_parallel_for_each_H

#include "parallel_do.h"

namespace tbb {

//! @cond INTERNAL
namespace internal {
    // The class calls user function in operator()
    template <typename Function, typename Iterator>
    class parallel_for_each_body : internal::no_assign {
        const Function &my_func;
    public:
        parallel_for_each_body(const Function &_func) : my_func(_func) {}
        parallel_for_each_body(const parallel_for_each_body<Function, Iterator> &_caller) : my_func(_caller.my_func) {}

        void operator() ( typename std::iterator_traits<Iterator>::reference value ) const {
            my_func(value);
        }
    };
} // namespace internal
//! @endcond

/** \name parallel_for_each
    **/
//@{
//! Calls function f for all items from [first, last) interval using user-supplied context
/** @ingroup algorithms */
#if __TBB_TASK_GROUP_CONTEXT
template<typename InputIterator, typename Function>
void parallel_for_each(InputIterator first, InputIterator last, const Function& f, task_group_context &context) {
    internal::parallel_for_each_body<Function, InputIterator> body(f);
    tbb::parallel_do (first, last, body, context);
}
#endif /* __TBB_TASK_GROUP_CONTEXT */

//! Uses default context
template<typename InputIterator, typename Function>
void parallel_for_each(InputIterator first, InputIterator last, const Function& f) {
    internal::parallel_for_each_body<Function, InputIterator> body(f);
    tbb::parallel_do (first, last, body);
}

//@}

} // namespace

#endif /* __TBB_parallel_for_each_H */
