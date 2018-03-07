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

#ifndef __TBB_aligned_space_H
#define __TBB_aligned_space_H

#include "tbb_stddef.h"
#include "tbb_machine.h"

namespace tbb {

//! Block of space aligned sufficiently to construct an array T with N elements.
/** The elements are not constructed or destroyed by this class.
    @ingroup memory_allocation */
template<typename T,size_t N>
class aligned_space {
private:
    typedef __TBB_TypeWithAlignmentAtLeastAsStrict(T) element_type;
    element_type array[(sizeof(T)*N+sizeof(element_type)-1)/sizeof(element_type)];
public:
    //! Pointer to beginning of array
    T* begin() {return internal::punned_cast<T*>(this);}

    //! Pointer to one past last element in array.
    T* end() {return begin()+N;}
};

} // namespace tbb 

#endif /* __TBB_aligned_space_H */
