// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/CommandLine.h"
#include "Misc/MessageDialog.h"
#include "Logging/LogMacros.h"
#include "Misc/Parse.h"
#include "Misc/CoreMisc.h"
#include "Internationalization/Text.h"
#include "Internationalization/Internationalization.h"

/*-----------------------------------------------------------------------------
	FCommandLine
-----------------------------------------------------------------------------*/

bool FCommandLine::bIsInitialized = false;
TCHAR FCommandLine::CmdLine[FCommandLine::MaxCommandLineSize] = TEXT("");
TCHAR FCommandLine::OriginalCmdLine[FCommandLine::MaxCommandLineSize] = TEXT("");
TCHAR FCommandLine::LoggingCmdLine[FCommandLine::MaxCommandLineSize] = TEXT("");
TCHAR FCommandLine::LoggingOriginalCmdLine[FCommandLine::MaxCommandLineSize] = TEXT("");
FString FCommandLine::SubprocessCommandLine(TEXT(" -Multiprocess"));

bool FCommandLine::IsInitialized()
{
	return bIsInitialized;
}

const TCHAR* FCommandLine::Get()
{
	UE_CLOG(!bIsInitialized, LogInit, Fatal, TEXT("Attempting to get the command line but it hasn't been initialized yet."));
	return CmdLine;
}

const TCHAR* FCommandLine::GetForLogging()
{
	UE_CLOG(!bIsInitialized, LogInit, Fatal, TEXT("Attempting to get the command line but it hasn't been initialized yet."));
	return LoggingCmdLine;
}

const TCHAR* FCommandLine::GetOriginal()
{
	UE_CLOG(!bIsInitialized, LogInit, Fatal, TEXT("Attempting to get the command line but it hasn't been initialized yet."));
	return OriginalCmdLine;
}

const TCHAR* FCommandLine::GetOriginalForLogging()
{
	UE_CLOG(!bIsInitialized, LogInit, Fatal, TEXT("Attempting to get the command line but it hasn't been initialized yet."));
	return LoggingOriginalCmdLine;
}

bool FCommandLine::Set(const TCHAR* NewCommandLine)
{
	if (!bIsInitialized)
	{
		FCString::Strncpy(OriginalCmdLine, NewCommandLine, ARRAY_COUNT(OriginalCmdLine));
		FCString::Strncpy(LoggingOriginalCmdLine, NewCommandLine, ARRAY_COUNT(LoggingOriginalCmdLine));
	}

	FCString::Strncpy( CmdLine, NewCommandLine, ARRAY_COUNT(CmdLine) );
	FCString::Strncpy(LoggingCmdLine, NewCommandLine, ARRAY_COUNT(LoggingCmdLine));
	// If configured as part of the build, strip out any unapproved args
	WhitelistCommandLines();

	bIsInitialized = true;

	// Check for the '-' that normal ones get converted to in Outlook. It's important to do it AFTER the command line is initialized
	if (StringHasBadDashes(NewCommandLine))
	{
		FText ErrorMessage = FText::Format(NSLOCTEXT("Engine", "ComdLineHasInvalidChar", "Error: Command-line contains an invalid '-' character, likely pasted from an email.\nCmdline = {0}"), FText::FromString(NewCommandLine));
#if !UE_BUILD_SHIPPING
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		return false;
#else
		UE_LOG(LogInit, Fatal, TEXT("%s"), *ErrorMessage.ToString());
#endif
	}

	return true;
}

void FCommandLine::Append(const TCHAR* AppendString)
{
	FCString::Strncat( CmdLine, AppendString, ARRAY_COUNT(CmdLine) );
	// If configured as part of the build, strip out any unapproved args
	WhitelistCommandLines();
}

#if WANTS_COMMANDLINE_WHITELIST
TArray<FString> FCommandLine::ApprovedArgs;
TArray<FString> FCommandLine::FilterArgsForLogging;

#ifdef OVERRIDE_COMMANDLINE_WHITELIST
/**
 * When overriding this setting make sure that your define looks like the following in your .cs file:
 *
 *		OutCPPEnvironmentConfiguration.Definitions.Add("OVERRIDE_COMMANDLINE_WHITELIST=\"-arg1 -arg2 -arg3 -arg4\"");
 *
 * The important part is the \" as they quotes get stripped off by the compiler without them
 */
const TCHAR* OverrideList = TEXT(OVERRIDE_COMMANDLINE_WHITELIST);
#else
// Default list most conservative restrictions
const TCHAR* OverrideList = TEXT("-fullscreen /windowed");
#endif

#ifdef FILTER_COMMANDLINE_LOGGING
/**
 * When overriding this setting make sure that your define looks like the following in your .cs file:
 *
 *		OutCPPEnvironmentConfiguration.Definitions.Add("FILTER_COMMANDLINE_LOGGING=\"-arg1 -arg2 -arg3 -arg4\"");
 *
 * The important part is the \" as they quotes get stripped off by the compiler without them
 */
const TCHAR* FilterForLoggingList = TEXT(FILTER_COMMANDLINE_LOGGING);
#else
const TCHAR* FilterForLoggingList = TEXT("");
#endif

void FCommandLine::WhitelistCommandLines()
{
	if (ApprovedArgs.Num() == 0)
	{
		TArray<FString> Ignored;
		FCommandLine::Parse(OverrideList, ApprovedArgs, Ignored);
	}
	if (FilterArgsForLogging.Num() == 0)
	{
		TArray<FString> Ignored;
		FCommandLine::Parse(FilterForLoggingList, FilterArgsForLogging, Ignored);
	}
	// Process the original command line
	TArray<FString> OriginalList = FilterCommandLine(OriginalCmdLine);
	BuildWhitelistCommandLine(OriginalCmdLine, ARRAY_COUNT(OriginalCmdLine), OriginalList);
	// Process the current command line
	TArray<FString> CmdList = FilterCommandLine(CmdLine);
	BuildWhitelistCommandLine(CmdLine, ARRAY_COUNT(CmdLine), CmdList);
	// Process the command line for logging purposes
	TArray<FString> LoggingCmdList = FilterCommandLineForLogging(LoggingCmdLine);
	BuildWhitelistCommandLine(LoggingCmdLine, ARRAY_COUNT(LoggingCmdLine), LoggingCmdList);
	// Process the original command line for logging purposes
	TArray<FString> LoggingOriginalCmdList = FilterCommandLineForLogging(LoggingOriginalCmdLine);
	BuildWhitelistCommandLine(LoggingOriginalCmdLine, ARRAY_COUNT(LoggingOriginalCmdLine), LoggingOriginalCmdList);
}

TArray<FString> FCommandLine::FilterCommandLine(TCHAR* CommandLine)
{
	TArray<FString> Ignored;
	TArray<FString> ParsedList;
	// Parse the command line list
	FCommandLine::Parse(CommandLine, ParsedList, Ignored);
	// Remove any that are not in our approved list
	for (int32 Index = 0; Index < ParsedList.Num(); Index++)
	{
		bool bFound = false;
		for (auto ApprovedArg : ApprovedArgs)
		{
			if (ParsedList[Index].StartsWith(ApprovedArg))
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			ParsedList.RemoveAt(Index);
			Index--;
		}
	}
	return ParsedList;
}

TArray<FString> FCommandLine::FilterCommandLineForLogging(TCHAR* CommandLine)
{
	TArray<FString> Ignored;
	TArray<FString> ParsedList;
	// Parse the command line list
	FCommandLine::Parse(CommandLine, ParsedList, Ignored);
	// Remove any that are not in our approved list
	for (int32 Index = 0; Index < ParsedList.Num(); Index++)
	{
		for (auto Filter : FilterArgsForLogging)
		{
			if (ParsedList[Index].StartsWith(Filter))
			{
				ParsedList.RemoveAt(Index);
				Index--;
				break;
			}
		}
	}
	return ParsedList;
}

void FCommandLine::BuildWhitelistCommandLine(TCHAR* CommandLine, uint32 ArrayCount, const TArray<FString>& FilteredArgs)
{
	check(ArrayCount > 0);
	// Zero the whole string
	FMemory::Memzero(CommandLine, sizeof(TCHAR) * ArrayCount);

	uint32 StartIndex = 0;
	for (auto Arg : FilteredArgs)
	{
		if ((StartIndex + Arg.Len() + 2) < ArrayCount)
		{
			if (StartIndex != 0)
			{
				CommandLine[StartIndex++] = TEXT(' ');
			}
			CommandLine[StartIndex++] = TEXT('-');
			FCString::Strncpy(&CommandLine[StartIndex], *Arg, ArrayCount - StartIndex);
			StartIndex += Arg.Len();
		}
	}
}
#endif

void FCommandLine::AddToSubprocessCommandline( const TCHAR* Param )
{
	check( Param != NULL );
	if (Param[0] != ' ')
	{
		SubprocessCommandLine += TEXT(" ");
	}
	SubprocessCommandLine += Param;
}

const FString& FCommandLine::GetSubprocessCommandline()
{
	return SubprocessCommandLine;
}

/** 
 * Removes the executable name from a commandline, denoted by parentheses.
 */
const TCHAR* FCommandLine::RemoveExeName(const TCHAR* InCmdLine)
{
	// Skip over executable that is in the commandline
	if( *InCmdLine=='\"' )
	{
		InCmdLine++;
		while( *InCmdLine && *InCmdLine!='\"' )
		{
			InCmdLine++;
		}
		if( *InCmdLine )
		{
			InCmdLine++;
		}
	}
	while( *InCmdLine && *InCmdLine!=' ' )
	{
		InCmdLine++;
	}
	// skip over any spaces at the start, which Vista likes to toss in multiple
	while (*InCmdLine == ' ')
	{
		InCmdLine++;
	}
	return InCmdLine;
}


/**
 * Parses a string into tokens, separating switches (beginning with -) from
 * other parameters
 *
 * @param	CmdLine		the string to parse
 * @param	Tokens		[out] filled with all parameters found in the string
 * @param	Switches	[out] filled with all switches found in the string
 */
void FCommandLine::Parse(const TCHAR* InCmdLine, TArray<FString>& Tokens, TArray<FString>& Switches)
{
	FString NextToken;
	while (FParse::Token(InCmdLine, NextToken, false))
	{
		if ((**NextToken == TCHAR('-')) 
#if PLATFORM_WINDOWS	// deprecate, but temporarily continue support /-prefixed switches on Windows
			|| (**NextToken == TCHAR('/'))
#endif // PLATFORM_WINDOWS
			)
		{
#if PLATFORM_WINDOWS	// warn as a courtesy
			if (**NextToken == TCHAR('/'))
			{
				UE_LOG(LogInit, Warning, TEXT("Passing commandline switches using / instead of - has been deprecated and will be removed in future versions of Unreal Engine."));
			}
#endif // PLATFORM_WINDOWS

			new(Switches) FString(NextToken.Mid(1));
			new(Tokens) FString(NextToken.Right(NextToken.Len() - 1));
		}
		else
		{
			new(Tokens) FString(NextToken);
		}
	}
}
