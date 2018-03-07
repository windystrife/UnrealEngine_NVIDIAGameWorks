// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

/** Whether we should generate crash reports even if the debugger is attached. */
extern CORE_API bool GAlwaysReportCrash;

/** Whether to use ClientReportClient rather than AutoReporter. */
extern CORE_API bool GUseCrashReportClient;

/** Whether we should ignore the attached debugger. */
extern CORE_API bool GIgnoreDebugger;

extern CORE_API TCHAR MiniDumpFilenameW[1024];

// #CrashReport: 2014-09-11 Move to PlatformExceptionHandling
#if PLATFORM_WINDOWS
#include "Windows/WindowsSystemIncludes.h"
#include <excpt.h>
// #CrashReport: 2014-10-09 These methods are specific to windows, remove from here.
extern CORE_API int32 ReportCrash( Windows::LPEXCEPTION_POINTERS ExceptionInfo );
extern CORE_API void NewReportEnsure( const TCHAR* ErrorMessage );
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneSystemIncludes.h"
// #CrashReport: 2014-10-09 Should be move to another file
extern CORE_API int32 ReportCrash( int ExceptionCode, LPEXCEPTION_POINTERS ExceptionInfo );
extern CORE_API void NewReportEnsure( const TCHAR* ErrorMessage );
#elif PLATFORM_MAC
// #CrashReport: 2014-10-09 Should be move to another file
#include <signal.h>
extern CORE_API int32 ReportCrash( ucontext_t *Context, int32 Signal, struct __siginfo* Info );
extern CORE_API void NewReportEnsure( const TCHAR* ErrorMessage );
#elif PLATFORM_LINUX
extern CORE_API void NewReportEnsure( const TCHAR* ErrorMessage );
#endif
/** Placeholder functions for checking image integrity. May or may not be used. DO NOT DELETE! */
extern CORE_API void CheckImageIntegrity();
extern CORE_API void CheckImageIntegrityAtRuntime();

enum class ECrashType
{
	Debug = -1,
	Normal = 0
};

/** In debug builds we can crash on demand so we want to let the crash reporting system know about it */
extern CORE_API void SetCrashType(ECrashType InCrashType);
extern CORE_API int32 GetCrashType();

extern CORE_API void ReportInteractiveEnsure(const TCHAR* InMessage);
extern CORE_API bool IsInteractiveEnsureMode();