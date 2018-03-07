// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	ApplePlatformCrashContext.h: Apple platform crash context declaration
==============================================================================================*/

#pragma once

#include "GenericPlatform/GenericPlatformCrashContext.h"

/**
 * Declaration for common crash context implementation for Apple platforms.
 */
struct CORE_API FApplePlatformCrashContext : public FGenericCrashContext
{
	FApplePlatformCrashContext();
	~FApplePlatformCrashContext();
	
	/** Signal number */
	int32 Signal;
	
	/** Additional signal info */
	siginfo_t* Info;
	
	/** Thread context */
	ucontext_t*	Context;
	
	/** Number of stack frames to ignore as they are crash handling/stack walking */
	uint32 IgnoreDepth;
	
	/** Memory reserved for "exception" (signal) info */
	char SignalDescription[128];
	
	/** Memory reserved for minidump-style callstack info */
	char MinidumpCallstackInfo[65536];
	
	/**
	 * Inits the crash context from data provided by a signal handler.
	 * @param InSignal number (SIGSEGV, etc)
	 * @param InInfo additional info (e.g. address we tried to read, etc)
	 * @param InContext thread context
	 */
	void InitFromSignal(int32 InSignal, siginfo_t* InInfo, void* InContext);
	
	/** Initialise context for reporting crash. */
	int32 ReportCrash(void) const;
	
	/** Generates a string representation for the execption/signal Info. */
	static void CreateExceptionInfoString(int32 Signal, siginfo_t* Info);
	
	/** Write a line of UTF-8 to a file */
	static void WriteLine(int ReportFile, const ANSICHAR* Line = NULL);
	
	/** Serializes UTF string to UTF-16 */
	static void WriteUTF16String(int ReportFile, const TCHAR* UTFString4BytesChar, uint32 NumChars);
	
	/** Serializes UTF string to UTF-16 */
	static void WriteUTF16String(int ReportFile, const TCHAR* UTFString4BytesChar);
	
	/** Writes UTF-16 line to a file */
	static void WriteLine(int ReportFile, const TCHAR* Line);
	
	/** Async-safe ItoA */
	static ANSICHAR* ItoANSI(uint64 Val, uint64 Base, uint32 Len = 0);
	
	/** Async-safe ItoT */
	static TCHAR* ItoTCHAR(uint64 Val, uint64 Base, uint32 Len = 0);
};
