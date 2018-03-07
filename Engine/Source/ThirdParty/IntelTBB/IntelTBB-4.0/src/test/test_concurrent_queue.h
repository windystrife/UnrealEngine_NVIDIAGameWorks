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

#include "tbb/atomic.h"
#include "tbb/cache_aligned_allocator.h"
#include "tbb/spin_mutex.h"
#include "tbb/tbb_machine.h"

#include "tbb/concurrent_monitor.h"

struct hacked_micro_queue {
    tbb::atomic<uintptr_t> head_page;
    tbb::atomic<size_t> head_counter;

    tbb::atomic<uintptr_t> tail_page;
    tbb::atomic<size_t> tail_counter;

    tbb::spin_mutex page_mutex;
 };

// hacks for strict_ppl::concurrent_queue
struct hacked_concurrent_queue_rep {
    static const size_t phi = 3;
    static const size_t n_queue = 8;

    tbb::atomic<size_t> head_counter;
    char pad1[tbb::internal::NFS_MaxLineSize-sizeof(tbb::atomic<size_t>)];
    tbb::atomic<size_t> tail_counter;
    char pad2[tbb::internal::NFS_MaxLineSize-sizeof(tbb::atomic<size_t>)];

    size_t items_per_page;
    size_t item_size;
    tbb::atomic<size_t> n_invalid_entries;
    char pad3[tbb::internal::NFS_MaxLineSize-sizeof(size_t)-sizeof(size_t)-sizeof(tbb::atomic<size_t>)];

    hacked_micro_queue array[n_queue];
};

struct hacked_concurrent_queue_page_allocator {
    size_t foo;
};

template <typename T>
struct hacked_concurrent_queue : public hacked_concurrent_queue_page_allocator {
    hacked_concurrent_queue_rep* my_rep;
    typename tbb::cache_aligned_allocator<T>::template rebind<char>::other my_allocator;
};

// hacks for concurrent_bounded_queue and deprecated::concurrent_queue
struct hacked_bounded_concurrent_queue_rep {
    static const size_t phi = 3;
    static const size_t n_queue = 8;

    tbb::atomic<size_t> head_counter;
    char cmon_items_avail[ sizeof(tbb::internal::concurrent_monitor) ];
    tbb::atomic<size_t> n_invalid_entries;
    char pad1[tbb::internal::NFS_MaxLineSize-((sizeof(tbb::atomic<size_t>)+sizeof(tbb::internal::concurrent_monitor)+sizeof(tbb::atomic<size_t>))&(tbb::internal::NFS_MaxLineSize-1))];

    tbb::atomic<size_t> tail_counter;
    char cmon_slots_avail[ sizeof(tbb::internal::concurrent_monitor) ];
    char pad2[tbb::internal::NFS_MaxLineSize-((sizeof(tbb::atomic<size_t>)+sizeof(tbb::internal::concurrent_monitor))&(tbb::internal::NFS_MaxLineSize-1))];
    hacked_micro_queue array[n_queue];

    static const ptrdiff_t infinite_capacity = ptrdiff_t(~size_t(0)/2);
};

struct hacked_bounded_concurrent_queue  {
    size_t foo;
    hacked_bounded_concurrent_queue_rep* my_rep;
    ptrdiff_t my_capacity;
    size_t items_per_page;
    size_t item_size;
};
