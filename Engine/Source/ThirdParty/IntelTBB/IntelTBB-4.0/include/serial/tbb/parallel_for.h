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

#ifndef __TBB_SERIAL_parallel_for_H
#define __TBB_SERIAL_parallel_for_H

#if !TBB_USE_EXCEPTIONS && _MSC_VER
    // Suppress "C++ exception handler used, but unwind semantics are not enabled" warning in STL headers
    #pragma warning (push)
    #pragma warning (disable: 4530)
#endif

#include <stdexcept>
#include <string> // required to construct std exception classes

#if !TBB_USE_EXCEPTIONS && _MSC_VER
    #pragma warning (pop)
#endif

#include "tbb_annotate.h"

#ifndef __TBB_NORMAL_EXECUTION
#include "tbb/blocked_range.h"
#include "tbb/partitioner.h"
#endif

namespace tbb {
namespace serial {
namespace interface6 {

// parallel_for serial annotated implementation

template< typename Range, typename Body, typename Partitioner >
class start_for : tbb::internal::no_copy {
    Range my_range;
    const Body my_body;
    typename Partitioner::task_partition_type my_partition;
    void execute();

    //! Constructor for root task.
    start_for( const Range& range, const Body& body, Partitioner& partitioner ) :
        my_range( range ),
        my_body( body ),
        my_partition( partitioner )
    {
    }

    //! Splitting constructor used to generate children.
    /** this becomes left child.  Newly constructed object is right child. */
    start_for( start_for& parent_, split ) :
        my_range( parent_.my_range, split() ),
        my_body( parent_.my_body ),
        my_partition( parent_.my_partition, split() )
    {
    }

public:
    static void run(  const Range& range, const Body& body, const Partitioner& partitioner ) {
        if( !range.empty() ) {
            ANNOTATE_SITE_BEGIN( tbb_parallel_for );
            {
                start_for a( range, body, const_cast< Partitioner& >( partitioner ) );
                a.execute();
            }
            ANNOTATE_SITE_END( tbb_parallel_for );
        }
    }
};

template< typename Range, typename Body, typename Partitioner >
void start_for< Range, Body, Partitioner >::execute() {
    if( !my_range.is_divisible() || !my_partition.divisions_left() ) {
        ANNOTATE_TASK_BEGIN( tbb_parallel_for_range );
        {
            my_body( my_range );
        }
        ANNOTATE_TASK_END( tbb_parallel_for_range );
    } else {
        start_for b( *this, split() );
        this->execute(); // Execute the left interval first to keep the serial order.
        b.execute();     // Execute the right interval then.
    }
}

//! Parallel iteration over range with default partitioner..
/** @ingroup algorithms **/
template<typename Range, typename Body>
void parallel_for( const Range& range, const Body& body ) {
    serial::interface6::start_for<Range,Body,auto_partitioner>::run(range,body,auto_partitioner());
}

//! Parallel iteration over range with simple partitioner.
/** @ingroup algorithms **/
template<typename Range, typename Body>
void parallel_for( const Range& range, const Body& body, const simple_partitioner& partitioner ) {
    serial::interface6::start_for<Range,Body,simple_partitioner>::run(range,body,partitioner);
}

//! Parallel iteration over range with auto_partitioner.
/** @ingroup algorithms **/
template<typename Range, typename Body>
void parallel_for( const Range& range, const Body& body, const auto_partitioner& partitioner ) {
    serial::interface6::start_for<Range,Body,auto_partitioner>::run(range,body,partitioner);
}

//! Parallel iteration over range with affinity_partitioner.
/** @ingroup algorithms **/
template<typename Range, typename Body>
void parallel_for( const Range& range, const Body& body, affinity_partitioner& partitioner ) {
    serial::interface6::start_for<Range,Body,affinity_partitioner>::run(range,body,partitioner);
}

//! Parallel iteration over a range of integers with a step value
template <typename Index, typename Function>
void parallel_for(Index first, Index last, Index step, const Function& f) {
    if (step <= 0 )
        throw std::invalid_argument( "nonpositive_step" );
    else if (last > first) {
        // Above "else" avoids "potential divide by zero" warning on some platforms
        ANNOTATE_SITE_BEGIN( tbb_parallel_for );
        for( Index i = first; i < last; i = i + step ) {
            ANNOTATE_TASK_BEGIN( tbb_parallel_for_iteration );
            { f( i ); }
            ANNOTATE_TASK_END( tbb_parallel_for_iteration );
        }
        ANNOTATE_SITE_END( tbb_parallel_for );
    }
}

//! Parallel iteration over a range of integers with a default step value
template <typename Index, typename Function>
void parallel_for(Index first, Index last, const Function& f) {
    parallel_for(first, last, static_cast<Index>(1), f);
}

} // namespace interface6

using interface6::parallel_for;

} // namespace serial

#ifndef __TBB_NORMAL_EXECUTION
using serial::interface6::parallel_for;
#endif

} // namespace tbb

#endif /* __TBB_SERIAL_parallel_for_H */
