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

#ifndef _TBB_ITT_NOTIFY
#define _TBB_ITT_NOTIFY

#include "tbb/tbb_stddef.h"

#if DO_ITT_NOTIFY

#if _WIN32||_WIN64
    #ifndef UNICODE
        #define UNICODE
    #endif
#endif /* WIN */

#ifndef INTEL_ITTNOTIFY_API_PRIVATE
#define INTEL_ITTNOTIFY_API_PRIVATE
#endif

#include "tools_api/ittnotify.h"
#include "tools_api/legacy/ittnotify.h"

#if _WIN32||_WIN64
    #undef _T
    #undef __itt_event_create
    #define __itt_event_create __itt_event_createA
#endif /* WIN */


#endif /* DO_ITT_NOTIFY */

#if !ITT_CALLER_NULL
#define ITT_CALLER_NULL ((__itt_caller)0)
#endif

namespace tbb {
//! Unicode support
#if (_WIN32||_WIN64) && !__MINGW32__
    //! Unicode character type. Always wchar_t on Windows.
    /** We do not use typedefs from Windows TCHAR family to keep consistence of TBB coding style. **/
    typedef wchar_t tchar;
    //! Standard Windows macro to markup the string literals. 
    #define _T(string_literal) L ## string_literal
#else /* !WIN */
    typedef char tchar;
    //! Standard Windows style macro to markup the string literals.
    #define _T(string_literal) string_literal
#endif /* !WIN */
} // namespace tbb

#if DO_ITT_NOTIFY
namespace tbb {
    //! Display names of internal synchronization types
    extern const tchar 
            *SyncType_GlobalLock,
            *SyncType_Scheduler;
    //! Display names of internal synchronization components/scenarios
    extern const tchar 
            *SyncObj_SchedulerInitialization,
            *SyncObj_SchedulersList,
            *SyncObj_WorkerLifeCycleMgmt,
            *SyncObj_TaskStealingLoop,
            *SyncObj_WorkerTaskPool,
            *SyncObj_MasterTaskPool,
            *SyncObj_TaskPoolSpinning,
            *SyncObj_Mailbox,
            *SyncObj_TaskReturnList,
            *SyncObj_TaskStream,
            *SyncObj_ContextsList
            ;

    namespace internal {
        void __TBB_EXPORTED_FUNC itt_set_sync_name_v3( void* obj, const tchar* name); 

    } // namespace internal

} // namespace tbb

// const_cast<void*>() is necessary to cast off volatility
#define ITT_NOTIFY(name,obj)            __itt_notify_##name(const_cast<void*>(static_cast<volatile void*>(obj)))
#define ITT_THREAD_SET_NAME(name)       __itt_thread_set_name(name)
#define ITT_SYNC_CREATE(obj, type, name) __itt_sync_create((void*)(obj), type, name, 2)
#define ITT_SYNC_RENAME(obj, name)      __itt_sync_rename(obj, name)
#define ITT_STACK_CREATE(obj)           obj = __itt_stack_caller_create()
#if __TBB_TASK_GROUP_CONTEXT
#define ITT_STACK(precond, name, obj)   (precond) ? __itt_stack_##name(obj) : ((void)0);
#else
#define ITT_STACK(precond, name, obj)      ((void)0)
#endif /* !__TBB_TASK_GROUP_CONTEXT */

#else /* !DO_ITT_NOTIFY */

#define ITT_NOTIFY(name,obj)            ((void)0)
#define ITT_THREAD_SET_NAME(name)       ((void)0)
#define ITT_SYNC_CREATE(obj, type, name) ((void)0)
#define ITT_SYNC_RENAME(obj, name)      ((void)0)
#define ITT_STACK_CREATE(obj)           ((void)0)
#define ITT_STACK(precond, name, obj)   ((void)0)

#endif /* !DO_ITT_NOTIFY */

namespace tbb {
namespace internal {
int __TBB_load_ittnotify();
}}

#endif /* _TBB_ITT_NOTIFY */
