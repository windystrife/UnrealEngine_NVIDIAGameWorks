// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../GenericErrorReport.h"

/**
 * Helper that works with Mac Error Reports
 */
class FIOSErrorReport : public FGenericErrorReport
{
public:
	/**
	 * Default constructor: creates a report with no files
	 */
	FIOSErrorReport()
	{
	}
	
	/**
	 * Load helper modules
	 */
	static void Init();
	
	/**
	 * Unload helper modules
	 */
	static void ShutDown();

	/**
	 * Discover all files in the crash report directory
	 * @param Directory Full path to directory containing the report
	 */
	explicit FIOSErrorReport(const FString& Directory);

	/**
	 * Parse the callstack from the Apple-style crash report log
	 * @return UE4 crash diagnosis text
	 */
	FText DiagnoseReport() const;

	/**
	 * Get the full path of the crashed app from the report (hides implementation in FGenericErrorReport)
	 */
	FString FindCrashedAppPath() const;

	/**
	 * Look for the most recent Mac Error Report
	 * @return Full path to the most recent report, or an empty string if none found
	 */
	static void FindMostRecentErrorReports(TArray<FString>& ErrorReportPaths, const FTimespan& MaxCrashReportAge);
};
