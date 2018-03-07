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

#ifndef __TBB_parallel_sort_H
#define __TBB_parallel_sort_H

#include "parallel_for.h"
#include "blocked_range.h"
#include <algorithm>
#include <iterator>
#include <functional>

namespace tbb {

//! @cond INTERNAL
namespace internal {

//! Range used in quicksort to split elements into subranges based on a value.
/** The split operation selects a splitter and places all elements less than or equal 
    to the value in the first range and the remaining elements in the second range.
    @ingroup algorithms */
template<typename RandomAccessIterator, typename Compare>
class quick_sort_range: private no_assign {

    inline size_t median_of_three(const RandomAccessIterator &array, size_t l, size_t m, size_t r) const {
        return comp(array[l], array[m]) ? ( comp(array[m], array[r]) ? m : ( comp( array[l], array[r]) ? r : l ) ) 
                                        : ( comp(array[r], array[m]) ? m : ( comp( array[r], array[l] ) ? r : l ) );
    }

    inline size_t pseudo_median_of_nine( const RandomAccessIterator &array, const quick_sort_range &range ) const {
        size_t offset = range.size/8u;
        return median_of_three(array, 
                               median_of_three(array, 0, offset, offset*2),
                               median_of_three(array, offset*3, offset*4, offset*5),
                               median_of_three(array, offset*6, offset*7, range.size - 1) );

    }

public:

    static const size_t grainsize = 500;
    const Compare &comp;
    RandomAccessIterator begin;
    size_t size;

    quick_sort_range( RandomAccessIterator begin_, size_t size_, const Compare &comp_ ) :
        comp(comp_), begin(begin_), size(size_) {}

    bool empty() const {return size==0;}
    bool is_divisible() const {return size>=grainsize;}

    quick_sort_range( quick_sort_range& range, split ) : comp(range.comp) {
        RandomAccessIterator array = range.begin;
        RandomAccessIterator key0 = range.begin; 
        size_t m = pseudo_median_of_nine(array, range);
        if (m) std::swap ( array[0], array[m] );

        size_t i=0;
        size_t j=range.size;
        // Partition interval [i+1,j-1] with key *key0.
        for(;;) {
            __TBB_ASSERT( i<j, NULL );
            // Loop must terminate since array[l]==*key0.
            do {
                --j;
                __TBB_ASSERT( i<=j, "bad ordering relation?" );
            } while( comp( *key0, array[j] ));
            do {
                __TBB_ASSERT( i<=j, NULL );
                if( i==j ) goto partition;
                ++i;
            } while( comp( array[i],*key0 ));
            if( i==j ) goto partition;
            std::swap( array[i], array[j] );
        }
partition:
        // Put the partition key were it belongs
        std::swap( array[j], *key0 );
        // array[l..j) is less or equal to key.
        // array(j..r) is greater or equal to key.
        // array[j] is equal to key
        i=j+1;
        begin = array+i;
        size = range.size-i;
        range.size = j;
    }
};

#if __TBB_TASK_GROUP_CONTEXT
//! Body class used to test if elements in a range are presorted
/** @ingroup algorithms */
template<typename RandomAccessIterator, typename Compare>
class quick_sort_pretest_body : internal::no_assign {
    const Compare &comp;

public:
    quick_sort_pretest_body(const Compare &_comp) : comp(_comp) {}

    void operator()( const blocked_range<RandomAccessIterator>& range ) const {
        task &my_task = task::self();
        RandomAccessIterator my_end = range.end();

        int i = 0;
        for (RandomAccessIterator k = range.begin(); k != my_end; ++k, ++i) {
            if ( i%64 == 0 && my_task.is_cancelled() ) break;
          
            // The k-1 is never out-of-range because the first chunk starts at begin+serial_cutoff+1
            if ( comp( *(k), *(k-1) ) ) {
                my_task.cancel_group_execution();
                break;
            }
        }
    }

};
#endif /* __TBB_TASK_GROUP_CONTEXT */

//! Body class used to sort elements in a range that is smaller than the grainsize.
/** @ingroup algorithms */
template<typename RandomAccessIterator, typename Compare>
struct quick_sort_body {
    void operator()( const quick_sort_range<RandomAccessIterator,Compare>& range ) const {
        //SerialQuickSort( range.begin, range.size, range.comp );
        std::sort( range.begin, range.begin + range.size, range.comp );
    }
};

//! Wrapper method to initiate the sort by calling parallel_for.
/** @ingroup algorithms */
template<typename RandomAccessIterator, typename Compare>
void parallel_quick_sort( RandomAccessIterator begin, RandomAccessIterator end, const Compare& comp ) {
#if __TBB_TASK_GROUP_CONTEXT
    task_group_context my_context;
    const int serial_cutoff = 9;

    __TBB_ASSERT( begin + serial_cutoff < end, "min_parallel_size is smaller than serial cutoff?" );
    RandomAccessIterator k;
    for ( k = begin ; k != begin + serial_cutoff; ++k ) {
        if ( comp( *(k+1), *k ) ) {
            goto do_parallel_quick_sort;
        }
    }

    parallel_for( blocked_range<RandomAccessIterator>(k+1, end),
                  quick_sort_pretest_body<RandomAccessIterator,Compare>(comp),
                  auto_partitioner(),
                  my_context);

    if (my_context.is_group_execution_cancelled())
do_parallel_quick_sort:
#endif /* __TBB_TASK_GROUP_CONTEXT */
        parallel_for( quick_sort_range<RandomAccessIterator,Compare>(begin, end-begin, comp ), 
                      quick_sort_body<RandomAccessIterator,Compare>(),
                      auto_partitioner() );
}

} // namespace internal
//! @endcond

/** \page parallel_sort_iter_req Requirements on iterators for parallel_sort
    Requirements on value type \c T of \c RandomAccessIterator for \c parallel_sort:
    - \code void swap( T& x, T& y ) \endcode        Swaps \c x and \c y
    - \code bool Compare::operator()( const T& x, const T& y ) \endcode
                                                    True if x comes before y;
**/

/** \name parallel_sort
    See also requirements on \ref parallel_sort_iter_req "iterators for parallel_sort". **/
//@{

//! Sorts the data in [begin,end) using the given comparator 
/** The compare function object is used for all comparisons between elements during sorting.
    The compare object must define a bool operator() function.
    @ingroup algorithms **/
template<typename RandomAccessIterator, typename Compare>
void parallel_sort( RandomAccessIterator begin, RandomAccessIterator end, const Compare& comp) { 
    const int min_parallel_size = 500; 
    if( end > begin ) {
        if (end - begin < min_parallel_size) { 
            std::sort(begin, end, comp);
        } else {
            internal::parallel_quick_sort(begin, end, comp);
        }
    }
}

//! Sorts the data in [begin,end) with a default comparator \c std::less<RandomAccessIterator>
/** @ingroup algorithms **/
template<typename RandomAccessIterator>
inline void parallel_sort( RandomAccessIterator begin, RandomAccessIterator end ) { 
    parallel_sort( begin, end, std::less< typename std::iterator_traits<RandomAccessIterator>::value_type >() );
}

//! Sorts the data in the range \c [begin,end) with a default comparator \c std::less<T>
/** @ingroup algorithms **/
template<typename T>
inline void parallel_sort( T * begin, T * end ) {
    parallel_sort( begin, end, std::less< T >() );
}   
//@}


} // namespace tbb

#endif

