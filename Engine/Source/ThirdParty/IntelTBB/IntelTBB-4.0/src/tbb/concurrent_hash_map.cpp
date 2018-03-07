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

#include "tbb/concurrent_hash_map.h"

namespace tbb {

namespace internal {
#if !TBB_NO_LEGACY
struct hash_map_segment_base {
    typedef spin_rw_mutex segment_mutex_t;
    //! Type of a hash code.
    typedef size_t hashcode_t;
    //! Log2 of n_segment
    static const size_t n_segment_bits = 6;
    //! Maximum size of array of chains
    static const size_t max_physical_size = size_t(1)<<(8*sizeof(hashcode_t)-n_segment_bits);
    //! Mutex that protects this segment
    segment_mutex_t my_mutex;
    // Number of nodes
    atomic<size_t> my_logical_size;
    // Size of chains
    /** Always zero or a power of two */
    size_t my_physical_size;
    //! True if my_logical_size>=my_physical_size.
    /** Used to support Intel(R) Thread Checker. */
    bool __TBB_EXPORTED_METHOD internal_grow_predicate() const;
};

bool hash_map_segment_base::internal_grow_predicate() const {
    // Intel(R) Thread Checker considers the following reads to be races, so we hide them in the 
    // library so that Intel(R) Thread Checker will ignore them.  The reads are used in a double-check
    // context, so the program is nonetheless correct despite the race.
    return my_logical_size >= my_physical_size && my_physical_size < max_physical_size;
}
#endif//!TBB_NO_LEGACY

} // namespace internal

} // namespace tbb

