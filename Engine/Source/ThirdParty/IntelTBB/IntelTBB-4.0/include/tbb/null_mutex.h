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

#ifndef __TBB_null_mutex_H
#define __TBB_null_mutex_H

namespace tbb {
    
//! A mutex which does nothing
/** A null_mutex does no operation and simulates success.
    @ingroup synchronization */
class null_mutex {   
    //! Deny assignment and copy construction 
    null_mutex( const null_mutex& );   
    void operator=( const null_mutex& );   
public:   
    //! Represents acquisition of a mutex.
    class scoped_lock {   
    public:   
        scoped_lock() {}
        scoped_lock( null_mutex& ) {}   
        ~scoped_lock() {}
        void acquire( null_mutex& ) {}
        bool try_acquire( null_mutex& ) { return true; }
        void release() {}
    };
  
    null_mutex() {}
    
    // Mutex traits   
    static const bool is_rw_mutex = false;   
    static const bool is_recursive_mutex = true;
    static const bool is_fair_mutex = true;
};  

}

#endif /* __TBB_null_mutex_H */
