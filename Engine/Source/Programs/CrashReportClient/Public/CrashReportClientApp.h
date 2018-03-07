// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

// FORCE_CRASH_REPORT_UNATTENDED can be added to target.cs or UBT commandline in order to set CRASH_REPORT_UNATTENDED_ONLY
// It should not be defined in code other than when undefined to please IntelliSense.
#ifndef FORCE_CRASH_REPORT_UNATTENDED
#define FORCE_CRASH_REPORT_UNATTENDED 0
#endif

#define CRASH_REPORT_UNATTENDED_ONLY			PLATFORM_LINUX || FORCE_CRASH_REPORT_UNATTENDED

// Pre-compiled header includes
#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#if !CRASH_REPORT_UNATTENDED_ONLY
	#include "StandaloneRenderer.h"
#endif // CRASH_REPORT_UNATTENDED_ONLY

#include "CrashReportClientConfig.h"

DECLARE_LOG_CATEGORY_EXTERN(CrashReportClientLog, Log, All)

/**
 * Run the crash report client app
 */
void RunCrashReportClient(const TCHAR* Commandline);
