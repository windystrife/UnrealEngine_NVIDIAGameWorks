// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../GenericErrorReport.h"

/**
 * Helper that works with Windows Error Reports
 */
class FWindowsErrorReport : public FGenericErrorReport
{
public:
	/**
	 * Default constructor: creates a report with no files
	 */
	FWindowsErrorReport()
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
	explicit FWindowsErrorReport(const FString& Directory);

	/**
	 * Provide the exception and a call-stack as plain text if possible
	 * @note This can take quite a long time
	 */
	FText DiagnoseReport() const;

	/**
	 * Get the full path of the crashed app from the report
	 */
	FString FindCrashedAppPath() const;

	/**
	 * Look for the most recent Windows Error Reports
	 * @return Full path to the most recent report, or an empty string if none found
	 */
	static void FindMostRecentErrorReports(TArray<FString>& ErrorReportPaths, const FTimespan& MaxCrashReportAge);
};
