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

#ifndef __TBB_blocked_range_H
#define __TBB_blocked_range_H

#include "tbb_stddef.h"

namespace tbb {

/** \page range_req Requirements on range concept
    Class \c R implementing the concept of range must define:
    - \code R::R( const R& ); \endcode               Copy constructor
    - \code R::~R(); \endcode                        Destructor
    - \code bool R::is_divisible() const; \endcode   True if range can be partitioned into two subranges
    - \code bool R::empty() const; \endcode          True if range is empty
    - \code R::R( R& r, split ); \endcode            Split range \c r into two subranges.
**/

//! A range over which to iterate.
/** @ingroup algorithms */
template<typename Value>
class blocked_range {
public:
    //! Type of a value
    /** Called a const_iterator for sake of algorithms that need to treat a blocked_range
        as an STL container. */
    typedef Value const_iterator;

    //! Type for size of a range
    typedef std::size_t size_type;

    //! Construct range with default-constructed values for begin and end.
    /** Requires that Value have a default constructor. */
    blocked_range() : my_end(), my_begin() {}

    //! Construct range over half-open interval [begin,end), with the given grainsize.
    blocked_range( Value begin_, Value end_, size_type grainsize_=1 ) : 
        my_end(end_), my_begin(begin_), my_grainsize(grainsize_) 
    {
        __TBB_ASSERT( my_grainsize>0, "grainsize must be positive" );
    }

    //! Beginning of range.
    const_iterator begin() const {return my_begin;}

    //! One past last value in range.
    const_iterator end() const {return my_end;}

    //! Size of the range
    /** Unspecified if end()<begin(). */
    size_type size() const {
        __TBB_ASSERT( !(end()<begin()), "size() unspecified if end()<begin()" );
        return size_type(my_end-my_begin);
    }

    //! The grain size for this range.
    size_type grainsize() const {return my_grainsize;}

    //------------------------------------------------------------------------
    // Methods that implement Range concept
    //------------------------------------------------------------------------

    //! True if range is empty.
    bool empty() const {return !(my_begin<my_end);}

    //! True if range is divisible.
    /** Unspecified if end()<begin(). */
    bool is_divisible() const {return my_grainsize<size();}

    //! Split range.  
    /** The new Range *this has the second half, the old range r has the first half. 
        Unspecified if end()<begin() or !is_divisible(). */
    blocked_range( blocked_range& r, split ) : 
        my_end(r.my_end),
        my_begin(do_split(r)),
        my_grainsize(r.my_grainsize)
    {}

private:
    /** NOTE: my_end MUST be declared before my_begin, otherwise the forking constructor will break. */
    Value my_end;
    Value my_begin;
    size_type my_grainsize;

    //! Auxiliary function used by forking constructor.
    /** Using this function lets us not require that Value support assignment or default construction. */
    static Value do_split( blocked_range& r ) {
        __TBB_ASSERT( r.is_divisible(), "cannot split blocked_range that is not divisible" );
        Value middle = r.my_begin + (r.my_end-r.my_begin)/2u;
        r.my_end = middle;
        return middle;
    }

    template<typename RowValue, typename ColValue>
    friend class blocked_range2d;

    template<typename RowValue, typename ColValue, typename PageValue>
    friend class blocked_range3d;
};

} // namespace tbb 

#endif /* __TBB_blocked_range_H */
