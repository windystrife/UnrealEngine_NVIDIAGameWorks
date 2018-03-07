// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SubversionSourceControlRevision.h"
#include "SubversionSourceControlState.h"

class FSubversionSourceControlCommand;
class FXmlFile;

/**
 * Helper struct for maintaining temporary files for passing to commands
 */
class FSVNScopedTempFile
{
public:

	/** Constructor - open & write string to temp file */
	FSVNScopedTempFile(const FString& InText);

	FSVNScopedTempFile(const FText& InText);

	/** Destructor - delete temp file */
	~FSVNScopedTempFile();

	/** Get the filename of this temp file - empty if it failed to be created */
	const FString& GetFilename() const;

private:
	/** The filename we are writing to */
	FString Filename;
};

class FSubversionSourceControlRevision;
class FXmlFile;
class FSubversionSourceControlCommand;

namespace SubversionSourceControlUtils
{

/**
 * Run an svn command - output is either a string or an FXmlFile.
 *
 * @param	InCommand			The svn command - e.g. info
 * @param	InFiles				The files to be operated on
 * @param	InParameters		The parameters to the svn command
 * @param	OutResults			The results (from StdOut) as an array per-line
 * @param	OutXmlResults		The results output as XML, potentially from multiple commands
 * @param	OutErrorMessages	Any errors (from StdErr)
 * @param	UserName			The username too use with this command
 * @param	Password			The password (if any) we should use when accessing svn. Note that 
 *								the password is stored by the system after the first successful use.
 * @returns true if the command succeeded and returned no errors - note some svn commands return 0 but output errors anyway!
 */			
bool RunCommand(const FString& InCommand, const TArray<FString>& InFiles, const TArray<FString>& InParameters, TArray<class FXmlFile>& OutXmlResults, TArray<FString>& OutErrorMessages, const FString& UserName, const FString& Password = FString());
bool RunCommand(const FString& InCommand, const TArray<FString>& InFiles, const TArray<FString>& InParameters, TArray<FString>& OutResults, TArray<FString>& OutErrorMessages, const FString& UserName, const FString& Password = FString());

/**
 * Run an atomic svn command - don't split it into multiple commands if there are too many files, assert instead.
 *
 * @param	InCommand			The svn command - e.g. info
 * @param	InFiles				The files to be operated on
 * @param	InParameters		The parameters to the svn command
 * @param	OutResults			The results (from StdOut) as an array per-line
 * @param	OutErrorMessages	Any errors (from StdErr)
 * @param	UserName			The username too use with this command
 * @param	Password			The password (if any) we should use when accessing svn. Note that 
 *								the password is stored by the system after the first successful use.
 * @returns true if the command succeeded and returned no errors - note some svn commands return 0 but output errors anyway!
 */			
bool RunAtomicCommand(const FString& InCommand, const TArray<FString>& InFiles, const TArray<FString>& InParameters, TArray<FString>& OutResults, TArray<FString>& OutErrorMessages, const FString& UserName, const FString& Password = FString());

typedef TMap<FString, TArray< TSharedRef<FSubversionSourceControlRevision, ESPMode::ThreadSafe> > > FHistoryOutput;

/**
 * Translate SVN action strings into actions that more closely resemble old-style Perforce actions.
 */
FString TranslateAction(const FString& InAction);

/**
 * Parse a date string as output from SVN commands.
 */
FDateTime GetDate(const FString& InDateString);

/**
 * Parse the xml results of an 'svn log' command
 */
void ParseLogResults(const FString& InFilename, const TArray<FXmlFile>& ResultsXml, const FString& UserName, SubversionSourceControlUtils::FHistoryOutput& OutHistory);

/**
 * Parse the xml results of an 'svn info' command
 */
void ParseInfoResults(const TArray<FXmlFile>& ResultsXml, FString& OutWorkingCopyRoot, FString& OutRepoRoot);

/**
 * Parse the xml results of an 'svn status' command
 */
void ParseStatusResults(const TArray<FXmlFile>& ResultsXml, const TArray<FString>& InErrorMessages, const FString& InUserName, const FString& InWorkingCopyRoot, TArray<FSubversionSourceControlState>& OutStates);

/**
 * Helper function for various commands to update cached states.
 * @returns true if any states were updated
 */
bool UpdateCachedStates(const TArray<FSubversionSourceControlState>& InStates);

/**
 * Checks a filename for 'Perforce-style' wildcards, as SVN does not support this.
 */
bool CheckFilename(const FString& InString);

/**
 * Checks filenames for 'Perforce-style' wildcards, as SVN does not support this.
 */
bool CheckFilenames(const TArray<FString>& InStrings);

/** 
 * Remove redundant errors (that contain a particular string) and also
 * update the commands success status if all errors were removed.
 */
void RemoveRedundantErrors(FSubversionSourceControlCommand& InCommand, const FString& InFilter);

/**
 * Surround input filename with quotes, for sending to a command-line.
 */
void QuoteFilename(FString& InString);

/**
 * Surround input filenames with quotes, for sending to a command-line.
 */
void QuoteFilenames(TArray<FString>& InStrings);

}
