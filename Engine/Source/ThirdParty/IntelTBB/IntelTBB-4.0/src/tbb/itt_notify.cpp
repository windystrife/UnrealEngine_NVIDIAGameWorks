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

#if DO_ITT_NOTIFY

#if _WIN32||_WIN64
    #ifndef UNICODE
        #define UNICODE
    #endif
#else
    #pragma weak dlopen
    #pragma weak dlsym
    #pragma weak dlerror
#endif /* WIN */

#if __TBB_BUILD

extern "C" void ITT_DoOneTimeInitialization();
#define __itt_init_ittlib_name(x,y) (ITT_DoOneTimeInitialization(), true)

#elif __TBBMALLOC_BUILD

extern "C" void MallocInitializeITT();
#define __itt_init_ittlib_name(x,y) (MallocInitializeITT(), true)

#else
#error This file is expected to be used for either TBB or TBB allocator build.
#endif // __TBB_BUILD

#include "tools_api/ittnotify_static.c"

namespace tbb {
namespace internal {
int __TBB_load_ittnotify() {
    int returnVal = __itt_init_ittlib(NULL,          // groups for:
      (__itt_group_id)(__itt_group_sync     // prepare/cancel/acquired/releasing
                       | __itt_group_thread // name threads
                       | __itt_group_stitch // stack stitching
                           ));
	return returnVal;
}

}} // namespaces

#endif /* DO_ITT_NOTIFY */

#define __TBB_NO_IMPLICIT_LINKAGE 1
#include "itt_notify.h"

namespace tbb {

#if DO_ITT_NOTIFY
    const tchar 
            *SyncType_GlobalLock = _T("TbbGlobalLock"),
            *SyncType_Scheduler = _T("%Constant")
            ;
    const tchar 
            *SyncObj_SchedulerInitialization = _T("TbbSchedulerInitialization"),
            *SyncObj_SchedulersList = _T("TbbSchedulersList"),
            *SyncObj_WorkerLifeCycleMgmt = _T("TBB Scheduler"),
            *SyncObj_TaskStealingLoop = _T("TBB Scheduler"),
            *SyncObj_WorkerTaskPool = _T("TBB Scheduler"),
            *SyncObj_MasterTaskPool = _T("TBB Scheduler"),
            *SyncObj_TaskPoolSpinning = _T("TBB Scheduler"),
            *SyncObj_Mailbox = _T("TBB Scheduler"),
            *SyncObj_TaskReturnList = _T("TBB Scheduler"),
            *SyncObj_TaskStream = _T("TBB Scheduler"),
            *SyncObj_ContextsList = _T("TBB Scheduler")
            ;
#endif /* DO_ITT_NOTIFY */

} // namespace tbb

