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

#include "tbb/tbb_machine.h"
#include "tbb/spin_mutex.h"
#include "itt_notify.h"
#include "tbb_misc.h"

namespace tbb {

void spin_mutex::scoped_lock::internal_acquire( spin_mutex& m ) {
    __TBB_ASSERT( !my_mutex, "already holding a lock on a spin_mutex" );
    ITT_NOTIFY(sync_prepare, &m);
    __TBB_LockByte(m.flag);
    my_mutex = &m;
    ITT_NOTIFY(sync_acquired, &m);
}

void spin_mutex::scoped_lock::internal_release() {
    __TBB_ASSERT( my_mutex, "release on spin_mutex::scoped_lock that is not holding a lock" );

    ITT_NOTIFY(sync_releasing, my_mutex);
    __TBB_UnlockByte(my_mutex->flag, 0);
    my_mutex = NULL;
}

bool spin_mutex::scoped_lock::internal_try_acquire( spin_mutex& m ) {
    __TBB_ASSERT( !my_mutex, "already holding a lock on a spin_mutex" );
    bool result = bool( __TBB_TryLockByte(m.flag) );
    if( result ) {
        my_mutex = &m;
        ITT_NOTIFY(sync_acquired, &m);
    }
    return result;
}

void spin_mutex::internal_construct() {
    ITT_SYNC_CREATE(this, _T("tbb::spin_mutex"), _T(""));
}

} // namespace tbb
