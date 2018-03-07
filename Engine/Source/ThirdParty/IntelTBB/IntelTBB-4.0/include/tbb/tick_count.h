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

#ifndef __TBB_tick_count_H
#define __TBB_tick_count_H

#include "tbb_stddef.h"

#if _WIN32||_WIN64
#include "machine/windows_api.h"
#elif __linux__
#include <ctime>
#else /* generic Unix */
#include <sys/time.h>
#endif /* (choice of OS) */

namespace tbb {

//! Absolute timestamp
/** @ingroup timing */
class tick_count {
public:
    //! Relative time interval.
    class interval_t {
        long long value;
        explicit interval_t( long long value_ ) : value(value_) {}
    public:
        //! Construct a time interval representing zero time duration
        interval_t() : value(0) {};

        //! Construct a time interval representing sec seconds time  duration
        explicit interval_t( double sec );

        //! Return the length of a time interval in seconds
        double seconds() const;

        friend class tbb::tick_count;

        //! Extract the intervals from the tick_counts and subtract them.
        friend interval_t operator-( const tick_count& t1, const tick_count& t0 );

        //! Add two intervals.
        friend interval_t operator+( const interval_t& i, const interval_t& j ) {
            return interval_t(i.value+j.value);
        }

        //! Subtract two intervals.
        friend interval_t operator-( const interval_t& i, const interval_t& j ) {
            return interval_t(i.value-j.value);
        }

        //! Accumulation operator
        interval_t& operator+=( const interval_t& i ) {value += i.value; return *this;}

        //! Subtraction operator
        interval_t& operator-=( const interval_t& i ) {value -= i.value; return *this;}
    };
    
    //! Construct an absolute timestamp initialized to zero.
    tick_count() : my_count(0) {};

    //! Return current time.
    static tick_count now();
    
    //! Subtract two timestamps to get the time interval between
    friend interval_t operator-( const tick_count& t1, const tick_count& t0 );

private:
    long long my_count;
};

inline tick_count tick_count::now() {
    tick_count result;
#if _WIN32||_WIN64
    LARGE_INTEGER qpcnt;
    QueryPerformanceCounter(&qpcnt);
    result.my_count = qpcnt.QuadPart;
#elif __linux__
    struct timespec ts;
#if TBB_USE_ASSERT
    int status = 
#endif /* TBB_USE_ASSERT */
        clock_gettime( CLOCK_REALTIME, &ts );
    __TBB_ASSERT( status==0, "CLOCK_REALTIME not supported" );
    result.my_count = static_cast<long long>(1000000000UL)*static_cast<long long>(ts.tv_sec) + static_cast<long long>(ts.tv_nsec);
#else /* generic Unix */
    struct timeval tv;
#if TBB_USE_ASSERT
    int status = 
#endif /* TBB_USE_ASSERT */
        gettimeofday(&tv, NULL);
    __TBB_ASSERT( status==0, "gettimeofday failed" );
    result.my_count = static_cast<long long>(1000000)*static_cast<long long>(tv.tv_sec) + static_cast<long long>(tv.tv_usec);
#endif /*(choice of OS) */
    return result;
}

inline tick_count::interval_t::interval_t( double sec )
{
#if _WIN32||_WIN64
    LARGE_INTEGER qpfreq;
    QueryPerformanceFrequency(&qpfreq);
    value = static_cast<long long>(sec*qpfreq.QuadPart);
#elif __linux__
    value = static_cast<long long>(sec*1E9);
#else /* generic Unix */
    value = static_cast<long long>(sec*1E6);
#endif /* (choice of OS) */
}

inline tick_count::interval_t operator-( const tick_count& t1, const tick_count& t0 ) {
    return tick_count::interval_t( t1.my_count-t0.my_count );
}

inline double tick_count::interval_t::seconds() const {
#if _WIN32||_WIN64
    LARGE_INTEGER qpfreq;
    QueryPerformanceFrequency(&qpfreq);
    return value/(double)qpfreq.QuadPart;
#elif __linux__
    return value*1E-9;
#else /* generic Unix */
    return value*1E-6;
#endif /* (choice of OS) */
}

} // namespace tbb

#endif /* __TBB_tick_count_H */

