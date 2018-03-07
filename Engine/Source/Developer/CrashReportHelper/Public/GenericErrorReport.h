// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "Misc/Paths.h"
#include "CrashDescription.h"

class FText;
class FCrashDebugHelperModule;
struct FPrimaryCrashProperties;

/**
 * Helper that works with Windows Error Reports
 */
class FGenericErrorReport
{
public:
	/**
	 * Default constructor: creates a report with no files
	 */
	FGenericErrorReport()
	{
	}

	/**
	 * Discover all files in the crash report directory
	 * @param Directory Full path to directory containing the report
	 */
	explicit FGenericErrorReport(const FString& Directory);

	/**
	 * One-time initialisation: does nothing by default
	 */
	static void Init()
	{
	}

	/**
	 * One-time clean-up: does nothing by default
	 */
	static void ShutDown()
	{
	}

	/**
	 * Write the provided comment into the error report
	 * @param UserComment Information provided by the user to add to the report
	 * @return Whether the comment was successfully written to the report
	 */
	bool SetUserComment(const FText& UserComment);

	/** Sets properties specific to the processed crash. Used to convert the old data into the crash context format. */
	void SetPrimaryCrashProperties( FPrimaryCrashProperties& out_PrimaryCrashProperties );

	/** Sets the version string in the error report and saves the change */
	void SetCrashReportClientVersion(const FString& InVersion);

	/**
	 * Provide full paths to all the report files
	 * @return List of paths
	 */
	TArray<FString> GetFilesToUpload() const;

	/**
	 * Load the WER XML file for this report
	 * @note This is Windows specific and so shouldn't really be part of the public interface, but currently the server
	 * is Windows-specific in its checking of reports, so this is needed.
	 * @param OutString String to load the file into
	 * @return Whether finding and loading the file succeeded
	 */
	bool LoadWindowsReportXmlFile( FString& OutString ) const;

	/**
	 * @return Whether the file was found and successfully read
	 */
	bool TryReadDiagnosticsFile();

	/**
	 * Provide the full path of the error report directory
	 */
	FString GetReportDirectory() const
	{
		return ReportDirectory;
	}

	/**
	 * Provide the name of the error report directory
	 */
	FString GetReportDirectoryLeafName() const
	{
		// Using GetCleanFilename to actually get directory name
		return FPaths::GetCleanFilename(ReportDirectory);
	}

	/**
	 * Get the name of the crashed app from the report
	 */
	FString FindCrashedAppName() const;

	/**
	 * Get the full path of the crashed app from the report
	 */
	FString FindCrashedAppPath() const;

	/**
	 * Is there anything to upload?
	 */
	bool HasFilesToUpload() const
	{
		return ReportFilenames.Num() != 0;
	}


	/**
	 * Look thought the list of report files to find one with the given extension
	 * @return Whether a file with the extension was found
	 */
	bool FindFirstReportFileWithExtension(FString& OutFilename, const TCHAR* Extension) const;
    
    /**
     * Deletes all of the files for the report
     */
    void DeleteFiles();

protected:
	/** Full path to the directory the report files are in */
	FString ReportDirectory;

	/** List of leaf filenames of all the files in the report folder */
	TArray<FString> ReportFilenames;

	/** Whether the error report generated an valid callstack. */
	mutable bool bValidCallstack;
};
