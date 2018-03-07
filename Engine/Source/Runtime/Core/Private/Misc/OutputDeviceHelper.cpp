// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/OutputDeviceHelper.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformTime.h"
#include "Containers/StringConv.h"
#include "CoreGlobals.h"

const TCHAR* FOutputDeviceHelper::VerbosityToString(ELogVerbosity::Type Verbosity)
{
	switch (Verbosity & ELogVerbosity::VerbosityMask)
	{
	case ELogVerbosity::NoLogging:
		return TEXT("NoLogging");
	case ELogVerbosity::Fatal:
		return TEXT("Fatal");
	case ELogVerbosity::Error:
		return TEXT("Error");
	case ELogVerbosity::Warning:
		return TEXT("Warning");
	case ELogVerbosity::Display:
		return TEXT("Display");
	case ELogVerbosity::Log:
		return TEXT("Log");
	case ELogVerbosity::Verbose:
		return TEXT("Verbose");
	case ELogVerbosity::VeryVerbose:
		return TEXT("VeryVerbose");
	}
	return TEXT("UknownVerbosity");
}

FString FOutputDeviceHelper::FormatLogLine( ELogVerbosity::Type Verbosity, const class FName& Category, const TCHAR* Message /*= nullptr*/, ELogTimes::Type LogTime /*= ELogTimes::None*/, const double Time /*= -1.0*/ )
{
	const bool bShowCategory = GPrintLogCategory && Category != NAME_None;
	FString Format;

	switch (LogTime)
	{
		case ELogTimes::SinceGStartTime:
		{																	
			const double RealTime = Time == -1.0f ? FPlatformTime::Seconds() - GStartTime : Time;
			Format = FString::Printf( TEXT( "[%07.2f][%3llu]" ), RealTime, GFrameCounter % 1000);
			break;
		}

		case ELogTimes::UTC:
			Format = FString::Printf(TEXT("[%s][%3llu]"), *FDateTime::UtcNow().ToString(TEXT("%Y.%m.%d-%H.%M.%S:%s")), GFrameCounter % 1000);
			break;

		case ELogTimes::Local:
			Format = FString::Printf(TEXT("[%s][%3llu]"), *FDateTime::Now().ToString(TEXT("%Y.%m.%d-%H.%M.%S:%s")), GFrameCounter % 1000);
			break;

		default:
			break;
	}	

	if (bShowCategory)
	{
		Format += Category.ToString();
		Format += TEXT(": ");

		if (Verbosity != ELogVerbosity::Log)
		{
			Format += VerbosityToString(Verbosity);
			Format += TEXT(": ");
		}
	}
	else
	{
		if (Verbosity != ELogVerbosity::Log)
		{
#if !HACK_HEADER_GENERATOR
			Format += VerbosityToString(Verbosity);
			Format += TEXT(": ");
#endif
		}
	}

	if (Message)
	{
		Format += Message;
	}
	return Format;
}

void FOutputDeviceHelper::FormatCastAndSerializeLine(FArchive& Output, const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time, bool bSuppressEventTag, bool bAutoEmitLineTerminator)
{
	// First gather all strings to serialize (prefix, data, terminator) including their size before and after conversion
	// then allocate the conversion buffer only once and serialize only once
#if PLATFORM_LINUX
	// on Linux, we still want to have logs with Windows line endings so they can be opened with Windows tools like infamous notepad.exe
	static const TCHAR* Terminator = TEXT("\r\n");
#else
	static const TCHAR* Terminator = LINE_TERMINATOR;
#endif // PLATFORM_LINUX
	// Line Terminator won't change so we can keep everything as statics
	static const int32 TerminatorLength = FCString::Strlen(Terminator);
	static const int32 ConvertedTerminatorLength = FTCHARToUTF8_Convert::ConvertedLength(Terminator, TerminatorLength);

	// Calculate data length
	const int32 DataLength = FCString::Strlen(Data);
	const int32 ConvertedDataLength = FTCHARToUTF8_Convert::ConvertedLength(Data, DataLength);

	FString Prefix;
	if (!bSuppressEventTag)
	{
		Prefix = FormatLogLine(Verbosity, Category, nullptr, GPrintLogTimes, Time);
	}

	// Calculate prefix length
	const int32 PrefixLength = Prefix.Len();
	const int32 ConvertedPrefixLength = FTCHARToUTF8_Convert::ConvertedLength(*Prefix, Prefix.Len());

	// Allocate the conversion buffer. It's ok to always add some slack for the line terminator even if not required
	TArray<ANSICHAR, TInlineAllocator<2 * DEFAULT_STRING_CONVERSION_SIZE>> ConvertedText;
	ConvertedText.AddUninitialized(ConvertedPrefixLength + ConvertedDataLength + (bAutoEmitLineTerminator ? ConvertedTerminatorLength : 0));
	// Do the actual conversion to the pre-allocated buffer
	FTCHARToUTF8_Convert::Convert(ConvertedText.GetData(), ConvertedPrefixLength, *Prefix, PrefixLength);
	FTCHARToUTF8_Convert::Convert(ConvertedText.GetData() + ConvertedPrefixLength, ConvertedDataLength, Data, DataLength);
	if (bAutoEmitLineTerminator)
	{
		FTCHARToUTF8_Convert::Convert(ConvertedText.GetData() + ConvertedPrefixLength + ConvertedDataLength, ConvertedTerminatorLength, Terminator, TerminatorLength);
	}
	// Serialize to the destination archive
	Output.Serialize(ConvertedText.GetData(), ConvertedText.Num() * sizeof(ANSICHAR));
}
