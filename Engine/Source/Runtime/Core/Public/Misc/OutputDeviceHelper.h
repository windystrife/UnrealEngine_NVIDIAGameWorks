// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"

/** Helper functions used by FOutputDevice derived classes **/
struct CORE_API FOutputDeviceHelper
{
	/**
	 * Converts verbosity to a string
	 * @param Verbosity verbosity enum
	 * @returns String representation of the verbosity enum
	 */
	static const TCHAR* VerbosityToString(ELogVerbosity::Type Verbosity);

	/**
	 * Formats a log line with date, time, category and verbosity prefix
	 * @param Verbosity Message verbosity
	 * @param Category Message category
	 * @param Message Optional message text. If nullptr, only the date/time/category/verbosity prefix will be returned
	 * @param LogTime Time format
	 * @param Time Time in seconds
	 * @returns Formatted log line
	 */
	static FString FormatLogLine(ELogVerbosity::Type Verbosity, const class FName& Category, const TCHAR* Message = nullptr, ELogTimes::Type LogTime = ELogTimes::None, const double Time = -1.0);

	/**
	 * Formats, casts to ANSI char and serializes a message to archive. Optimized for small number of allocations and Serialize calls
	 * @param Output Output archive 
	 * @param Message Log message
	 * @param Verbosity Message verbosity
	 * @param Category Message category
	 * @param Time Message time
	 * @param bSuppressEventTag True if the message date/time prefix should be suppressed
	 * @param bAutoEmitLineTerminator True if the message should be automatically appended with a line terminator
	 **/
	static void FormatCastAndSerializeLine(class FArchive& Output, const TCHAR* Message, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time, bool bSuppressEventTag, bool bAutoEmitLineTerminator);
};
