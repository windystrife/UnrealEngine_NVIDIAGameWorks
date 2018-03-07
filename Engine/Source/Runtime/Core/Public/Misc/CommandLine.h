// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"

/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/

#ifndef WANTS_COMMANDLINE_WHITELIST
	#define WANTS_COMMANDLINE_WHITELIST 0
#endif

struct CORE_API FCommandLine
{
	/** maximum size of the command line */
	static const uint32 MaxCommandLineSize = 16384;

	/** 
	 * Returns an edited version of the executable's command line with the game name and certain other parameters removed. 
	 */
	static const TCHAR* Get();
	
	/**
	 * Returns an edited version of the executable's command line. 
	 */
	static const TCHAR* GetForLogging();

	/**
	 * Returns the command line originally passed to the executable.
	 */
	static const TCHAR* GetOriginal();

	/**
	 * Returns an edited version of the command line originally passed to the executable.
	 */
	static const TCHAR* GetOriginalForLogging();

	/** 
	 * Checks if the command line has been initialized. 
	 */
	static bool IsInitialized();

	/**
	 * Sets CmdLine to the string given
	 */
	static bool Set(const TCHAR* NewCommandLine);

	/**
	 * Appends the passed string to the command line as it is (no space is added).
	 * @param AppendString String that should be appended to the commandline.
	 */
	static void Append(const TCHAR* AppendString);

	/**
	 * Adds a new parameter to subprocess command line. If Param
	 * does not start with a space, one is added.
	 *
	 * @param Param Command line param string.
	 */
	static void AddToSubprocessCommandline( const TCHAR* Param );

	/** 
	 * Returns the subprocess command line string 
	 */
	static const FString& GetSubprocessCommandline();

	/** 
	* Removes the executable name from the passed CmdLine, denoted by parentheses.
	* Returns the CmdLine string without the executable name.
	*/
	static const TCHAR* RemoveExeName(const TCHAR* CmdLine);

	/**
	 * Parses a string into tokens, separating switches (beginning with -) from
	 * other parameters
	 *
	 * @param	CmdLine		the string to parse
	 * @param	Tokens		[out] filled with all parameters found in the string
	 * @param	Switches	[out] filled with all switches found in the string
	 */
	static void Parse(const TCHAR* CmdLine, TArray<FString>& Tokens, TArray<FString>& Switches);
private:
#if WANTS_COMMANDLINE_WHITELIST
	/** Filters both the original and current command line list for approved only args */
	static void WhitelistCommandLines();
	/** Filters any command line args that aren't on the approved list */
	static TArray<FString> FilterCommandLine(TCHAR* CommandLine);
	/** Filters any command line args that are on the to-strip list */
	static TArray<FString> FilterCommandLineForLogging(TCHAR* CommandLine);
	/** Rebuilds the command line using the filtered args */
	static void BuildWhitelistCommandLine(TCHAR* CommandLine, uint32 Length, const TArray<FString>& FilteredArgs);
	static TArray<FString> ApprovedArgs;
	static TArray<FString> FilterArgsForLogging;
#else
#define WhitelistCommandLines()
#endif

	/** Flag to check if the commandline has been initialized or not. */
	static bool bIsInitialized;
	/** character buffer containing the command line */
	static TCHAR CmdLine[MaxCommandLineSize];
	/** character buffer containing the original command line */
	static TCHAR OriginalCmdLine[MaxCommandLineSize];
	/** character buffer containing the command line filtered for logging purposes */
	static TCHAR LoggingCmdLine[MaxCommandLineSize];
	/** character buffer containing the original command line filtered for logging purposes */
	static TCHAR LoggingOriginalCmdLine[MaxCommandLineSize];
	/** subprocess command line */
	static FString SubprocessCommandLine;
};

