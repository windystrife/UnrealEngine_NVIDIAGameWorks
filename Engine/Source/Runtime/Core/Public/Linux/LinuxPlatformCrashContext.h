// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"

// commandline parameter to suppress DWARF parsing (greatly speeds up callstack generation)
#define CMDARG_SUPPRESS_DWARF_PARSING			"nodwarf"

struct CORE_API FLinuxCrashContext : public FGenericCrashContext
{
	/** Signal number */
	int32 Signal;
	
	/** Additional signal info */
	siginfo_t* Info;
	
	/** Thread context */
	ucontext_t*	Context;

	/** Whether backtrace was already captured */
	bool bCapturedBacktrace;

	/** Symbols received via backtrace_symbols(), if any (note that we will need to clean it up) */
	char ** BacktraceSymbols;

	/** Memory reserved for "exception" (signal) info */
	TCHAR SignalDescription[256];

	/** Memory reserved for minidump-style callstack info */
	char MinidumpCallstackInfo[16384];

	/** Fake siginfo used when handling ensure()s */
	static __thread siginfo_t	FakeSiginfoForEnsures;

	FLinuxCrashContext()
		:	Signal(0)
		,	Info(nullptr)
		,	Context(nullptr)
		,	bCapturedBacktrace(false)
		,	BacktraceSymbols(nullptr)
	{
		SignalDescription[ 0 ] = 0;
		MinidumpCallstackInfo[ 0 ] = 0;
	}

	FLinuxCrashContext(bool bInIsEnsure)
		: Signal(0)
		, Info(nullptr)
		, Context(nullptr)
		, bCapturedBacktrace(false)
		, BacktraceSymbols(nullptr)
	{
		SignalDescription[0] = 0;
		MinidumpCallstackInfo[0] = 0;
		bIsEnsure = bInIsEnsure;
	}

	~FLinuxCrashContext();

	/**
	 * Inits the crash context from data provided by a signal handler.
	 *
	 * @param InSignal number (SIGSEGV, etc)
	 * @param InInfo additional info (e.g. address we tried to read, etc)
	 * @param InContext thread context
	 */
	void InitFromSignal(int32 InSignal, siginfo_t* InInfo, void* InContext);

	/**
	 * Inits the crash context from ensure handler
	 *
	 * @param EnsureMessage Message about the ensure that failed
	 * @param CrashAddress address where "crash" happened
	 */
	void InitFromEnsureHandler(const TCHAR* EnsureMessage, const void* CrashAddress);

	/**
	 * Populates crash context stack trace and a few related fields
	 *
	 */
	void CaptureStackTrace();

	/**
	 * Generates a new crash report containing information needed for the crash reporter and launches it; may not return.
	 *
	 * @param bReportingNonCrash if true, we are not reporting a crash (but e.g. ensure()), so don't re-raise the signal.
	 *
	 * @return If bReportingNonCrash is false, the function will not return
	 */
	void GenerateCrashInfoAndLaunchReporter(bool bReportingNonCrash = false) const;

	/**
	 * Sets whether this crash represents a non-crash event like an ensure
	 */
	void SetIsEnsure(bool bInIsEnsure) { bIsEnsure = bInIsEnsure; }

protected:
	/**
	 * Dumps all the data from crash context to the "minidump" report.
	 *
	 * @param DiagnosticsPath Path to put the file to
	 */
	void GenerateReport(const FString & DiagnosticsPath) const;
};

typedef FLinuxCrashContext FPlatformCrashContext;
