// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

// Includes

#include "CoreMinimal.h"
#include "NetcodeUnitTest.h"
#include "NUTUtil.h"
#include "Containers/StackTracker.h"

class FLogStackTraceManager;
class FStackTraceManager;
class UClientUnitTest;

// Globals

/** Provides a globally accessible trace manager, for easy access to stack trace debugging */
extern FStackTraceManager* GTraceManager;

/** Log hook for managing tying of log entry detection to the trace manager */
extern FLogStackTraceManager* GLogTraceManager;


// Enable access to FStackTracker.bIsEnabled
IMPLEMENT_GET_PRIVATE_VAR(FStackTracker, bIsEnabled, bool);

// The depth of stack traces, which the stack tracker should ignore by default
#define TRACE_IGNORE_DEPTH 7


/**
 * Provides a globally-accessible wrapper for the Exec function, which all modules can use (including those that don't inherit Engine),
 * for executing console commands - useful for debugging e.g. pre-Engine netcode (such as the PacketHandler code).
 */

extern "C" DLLEXPORT bool GGlobalExec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar);

// Use this following code snippet, to execute the above function.
#if 0
	FModuleStatus MS;
	void* DH = FModuleManager::Get().QueryModule(TEXT("NetcodeUnitTest"), MS) ? FPlatformProcess::GetDllHandle(*MS.FilePath) : nullptr;
	void* ExecPtr = DH != nullptr ? FPlatformProcess::GetDllExport(DH, TEXT("GGlobalExec")) : nullptr;

	if (ExecPtr != nullptr)
	{
		((bool (*)(void* InWorld, const TCHAR* Cmd, FOutputDevice& Ar))ExecPtr)
			(nullptr, TEXT("Place console command here."), *GLog);
	}
#endif


/**
 * A class for enabling verbose log message categories, within a particular code scope (disabled when going out of scope).
 * NOTE: If you are logging any kind of net-related log messages, specify a unit test (even if you aren't doing remote logging)
 *
 * Also supports remote (server) logging, for net functions executed within the current code scope
 * (causes net packets to be flushed both upon entering the current scope, and when exiting it - required for correct log timing).
 *
 * NOTE: If you are trying to catch remote log messages deep within the internal game netcode, 
 *			then this class may not be appropriate, as remote logging passes through the netcode (YMMV)
 */
class NETCODEUNITTEST_API FScopedLog
{
protected:
	FScopedLog()
	{
	}

public:
	/**
	 * Constructor used for setting up the type of logging that is done.
	 *
	 * @param InLogCategories	The list of log categories to be enabled
	 * @param InUnitTest		When tracking netcode-related logs, or doing remote logging, specify the client unit test here
	 * @param bInRemoteLogging	Whether or not to enable logging on the remote server
	 */
	FScopedLog(const TArray<FString>& InLogCategories, UClientUnitTest* InUnitTest=NULL, bool bInRemoteLogging=false)
	{
		InternalConstruct(InLogCategories, InUnitTest, bInRemoteLogging);
	}

	// As above, but for a single log category
	FScopedLog(const FString InLogCategory, UClientUnitTest* InUnitTest=NULL, bool bInRemoteLogging=false)
	{
		TArray<FString> TempLogCategories;
		TempLogCategories.Add(InLogCategory);

		InternalConstruct(TempLogCategories, InUnitTest, bInRemoteLogging);
	}

protected:
	void InternalConstruct(const TArray<FString>& InLogCategories, UClientUnitTest* InUnitTest, bool bInRemoteLogging);

public:
	~FScopedLog();


protected:
	/** The list of unsuppressed log messages */
	TArray<FString> LogCategories;

	/** Stores a reference to the unit test doing the logging, if specified */
	UClientUnitTest* UnitTest;

	/** Whether or not this is also controlling remote logging as well */
	bool bRemoteLogging;
};

/**
 * Version of the above, for scoped logging of all netcode-related logs
 */
class NETCODEUNITTEST_API FScopedLogNet : public FScopedLog
{
public:
	FScopedLogNet(UClientUnitTest* InUnitTest, bool bInRemoteLogging=false)
	{
		TArray<FString> TempLogCategories;

		// @todo #JohnBDebug: See if there are any other good net categories to add here
		TempLogCategories.Add(TEXT("LogNet"));
		TempLogCategories.Add(TEXT("LogRep"));
		TempLogCategories.Add(TEXT("LogNetTraffic"));
		TempLogCategories.Add(TEXT("LogRepTraffic"));
		TempLogCategories.Add(TEXT("LogNetSerialization"));
		TempLogCategories.Add(TEXT("LogNetPackageMap"));
		TempLogCategories.Add(TEXT("LogNetPlayerMovement"));
		TempLogCategories.Add(TEXT("LogNetDormancy"));
		TempLogCategories.Add(TEXT("LogProperty"));

		InternalConstruct(TempLogCategories, InUnitTest, bInRemoteLogging);
	}
};


// @todo #JohnB: When you continue implementing this, as a part of the ProcessEvent stack trace feature below,
//					merge this class with the very similar 'FProcessEventHook' class in NUTUtilNet.h,
//					then make the stack trace hook use that.
#if 0
#if !UE_BUILD_SHIPPING
/**
 * Base class for transparently hooking ProcessEvent
 */
class NETCODEUNITTEST_API FProcessEventHookBase
{
public:
	FProcessEventHookBase();

	virtual ~FProcessEventHookBase();

	// NOTE: Technically I 'could' allow the virtual override to return bool as well,
	//			but there is too great a risk of breaking something accidentally, so will not add until needed
	bool InternalProcessEventHook(AActor* Actor, UFunction* Function, void* Parameters);

	/**
	 * Implement the ProcessEvent hook, by overriding this in a subclass
	 *
	 * @param Actor			The actor the function is being called on
	 * @param Function		The function being called
	 * @param Parameters	The raw function parameters
	 */
	virtual void ProcessEventHook(AActor* Actor, UFunction* Function, void* Parameters)
		PURE_VIRTUAL(FProcessEventHookBase::ProcessEventHook(),);

protected:
	/** If a 'Actor::ProcessEventDelegate' value was already set, this caches it so it can be transparently hooked and restored later */
	FOnProcessEvent		OrigEventHook;
};
#endif
#endif



// @todo #JohnB: Reimplement this, by refactoring the above commented class, when this debug feature is next needed
#if 0
/**
 * A class for hooking and logging all ProcessEvent calls, within a particular code scope, similar to the above code
 */
class NETCODEUNITTEST_API FScopedProcessEventLog : FProcessEventHookBase
{
public:
	FScopedProcessEventLog()
		: FProcessEventHookBase()
	{
	}

	virtual void ProcessEventHook(AActor* Actor, UFunction* Function, void* Parameters) override;
};
#endif


/**
 * A class for dumping a stack trace, upon encountering a specific piece of code
 */
class FNUTStackTrace
{
public:
	/**
	 * Constructs the debug stack trace
	 *
	 * @param InTraceName	A human readable trace name for logging/debugging/tracking
	 */
	FNUTStackTrace(FString InTraceName);

	/**
	 * Destructor
	 */
	~FNUTStackTrace();


	/**
	 * Enable stack tracking
	 */
	void Enable();

	/**
	 * Disable stack tracking (past traces are still kept in tracking, but no new ones are added until re-enabled)
	 */
	void Disable();

	/**
	 * Adds a new trace to the stack tracker (optionally dumping to log at the same time)
	 *
	 * @param bLogAdd	Whether or not to also log this new trace being added
	 */
	void AddTrace(bool bLogAdd=false);

	/**
	 * Dumps accumulated stack traces
	 *
	 * @param bKeepTraceHistory		Whether or not to keep the trace history after dumping it (defaults to false)
	 */
	void Dump(bool bKeepTraceHistory=false);


	/**
	 * Whether or not the stack tracker, is currently tracking
	 */
	FORCEINLINE bool IsTrackingEnabled()
	{
		return GET_PRIVATE(FStackTracker, &Tracker, bIsEnabled);
	}

protected:
	/** The human-readable name to provide for this stack trace */
	FString TraceName;

	/** The stack tracker associated with this debug trace */
	FStackTracker Tracker;
};


// @todo #JohnBDocMain: Add documentation for all of the stack trace stuff, to the primary NetcodeUnitTest documentation

/**
 * Manager for handling multiple debug stack traces on-the-fly, and allowing abstraction of stack traces,
 * so you don't have to manually handle FNUTStackTrace objects (which can be complicated/bug-prone).
 *
 * This is a more intuitive way of handling tracing, you just use a call to 'GTraceManager->AddTrace' wherever needed,
 * and add calls to 'Enable'/'Disable' whenever you want to accept/ignore AddTrace calls - then 'Dump' to see the results.
 * No messing with managing instances of above objects.
 *
 * This also hooks into console commands as well, allowing it to be used throughout the engine, instead of depending on this module.
 * See the documentation for the 'StackTrace' command in UnitTestManager.cpp.
 */
class FStackTraceManager
{
public:
	/**
	 * Base constructor
	 */
	FStackTraceManager();

	/**
	 * Destructor
	 */
	~FStackTraceManager();


	/**
	 * Passes on an 'Enable' call, to the specified stack trace
	 *
	 * @param TraceName		The name of the trace
	 */
	void Enable(FString TraceName);

	/**
	 * Passes on a 'Disable' call, to the specified stack trace (if it exists)
	 *
	 * @param TraceName		The name of the trace
	 */
	void Disable(FString TraceName);

	/**
	 * Adds a new stack trace, to the specified trace, optionally logging/dumping in the process
	 *
	 * @param TraceName			The name of the trace
	 * @param bDump				Whether or not to dump the trace as well (does not remove trace from tracking, unlike 'Dump' function)
	 * @param bStartDisabled	Whether or not this trace should start as disabled, until 'Enable' is called for it
	 */
	void AddTrace(FString TraceName, bool bLogAdd=false, bool bDump=false, bool bStartDisabled=false);

	/**
	 * Dumps accumulated stack traces, and removes from tracking (unless otherwise specified)
	 *
	 * @param TraceName				The name of the trace
	 * @param bKeepTraceHistory		Whether or not to keep the trace history after dumping it (defaults to false)
	 * @param bKeepTracking			Whether or not to keep on tracking this particular trace name (defaults to true)
	 */
	void Dump(FString TraceName, bool bKeepTraceHistory=false, bool bKeepTracking=true);

	/**
	 * Clears the specified trace from tracking
	 *
	 * @param TraceName		The name of the trace
	 */
	void Clear(FString TraceName);

	/**
	 * Dumps accumulated stack traces, for all tracked traces
	 *
	 * @param bKeepTraceHistory		Whether or not to keep the trace history after dumping (defaults to false)
	 * @param bKeepTracking			Whether or not to keep on tracking dumped traces (defaults to true)
	 */
	void DumpAll(bool bKeepTraceHistory=false, bool bKeepTracking=true);


	/**
	 * Performs a once-off stack trace, with no tracking (but if there is already a trace active with this name,
	 * respect its 'enabled' status)
	 *
	 * @param TraceName		The name of the trace
	 */
	void TraceAndDump(FString TraceName);


	/**
	 * Whether or not a trace of this name is present
	 *
	 * @param TraceName		The name of the trace
	 */
	FORCEINLINE bool ContainsTrace(FString TraceName)
	{
		return Traces.Contains(TraceName);
	}

protected:
	/**
	 * Gets the trace of the specified name, or NULL if it doesn't exist
	 *
	 * @param TraceName		The name of the trace
	 * @return				The trace or NULL if it doesn't exist
	 */
	FORCEINLINE FNUTStackTrace* GetTrace(FString TraceName)
	{
		FNUTStackTrace** MapPtr = Traces.Find(TraceName);

		return (MapPtr != NULL ? *MapPtr : NULL);
	}

	/**
	 * Gets or creates a trace, of the specified name
	 *
	 * @param TraceName		The name of the trace
	 * @param bOutNewTrace	Optionally, specifies whether this trace is newly created
	 */
	FORCEINLINE FNUTStackTrace* GetOrCreateTrace(FString TraceName, bool* bOutNewTrace=NULL)
	{
		FNUTStackTrace* ReturnVal = GetTrace(TraceName);

		if (bOutNewTrace != NULL)
		{
			*bOutNewTrace = false;
		}

		if (ReturnVal == NULL)
		{
			ReturnVal = Traces.Add(TraceName, new FNUTStackTrace(TraceName));

			if (bOutNewTrace)
			{
				*bOutNewTrace = true;
			}
		}

		return ReturnVal;
	}

protected:
	/** A map of active debug stack traces */
	TMap<FString, FNUTStackTrace*> Traces;
};


// @todo #JohnBDebug: The log trace stuff doesn't support the 'startenabled' and enable/disable etc. features;
//				some stuff (like enable/disable) should be easy to add though, so do that eventually


// @todo #JohnB: NOTE: Don't move the log stack trace manager to .cpp yet; not until you decide how to implement,
//				the similar ProcessEvent hook that will tie in to the trace manager, as you may try to generalize the below for that

/**
 * A log hook, which watches the log for specified log entries, and ties them into the stack trace manager
 *
 * Most easily used through the LogTrace console command, as documented in UnitTestManager.cpp
 */
class FLogStackTraceManager : public FOutputDevice
{
public:
	/**
	 * Log trace entry
	 */
	struct FLogTraceEntry
	{
		/**	The log line being to watch for */
		FString LogLine;

		/** Whether or not to do a dump every time the log entry is encountered */
		bool bDump;

		FLogTraceEntry()
			: LogLine()
			, bDump(false)
		{
		}
	};

public:
	/**
	 * Base constructor
	 */
	FLogStackTraceManager()
	{
	}

	/**
	 * Destructor
	 */
	virtual ~FLogStackTraceManager() override
	{
		if (GLog != nullptr)
		{
			GLog->RemoveOutputDevice(this);
		}
	}

	/**
	 * Adds a log line for log trace tracking
	 * NOTE: The LogLine does NOT match the category or verbosity (e.g. LogNet or Warning/Verbose) of logs
	 * NOTE: Partial matches, will output to log when encountered, so matched logs can be identified
	 *
	 * @param LogLine	The line to be tracked
	 * @param bPartial	Whether or not to do partial matches for this line (case insensitive, and matches substrings)
	 * @param bDump		Whether or not to dump traces as they are encountered
	 */
	void AddLogTrace(FString LogLine, bool bPartial=true, bool bDump=true)
	{
		UE_LOG(LogUnitTest, Log, TEXT("Adding %slog trace for line: %s"), (bPartial ? TEXT("partial ") : TEXT("")), *LogLine);

		// Add the log hook, if not active already
		if (!GLog->IsRedirectingTo(this))
		{
			GLog->AddOutputDevice(this);
		}

		FLogTraceEntry CurEntry;

		CurEntry.LogLine = LogLine;
		CurEntry.bDump = bDump;


		auto MatchLogLine =
			[&](const FLogTraceEntry& InEntry)
			{
				return InEntry.LogLine == LogLine;
			};

		if (bPartial)
		{
			if (!PartialMatches.ContainsByPredicate(MatchLogLine))
			{
				PartialMatches.Add(CurEntry);
			}
		}
		else
		{
			if (!ExactMatches.ContainsByPredicate(MatchLogLine))
			{
				ExactMatches.Add(CurEntry);
			}
		}
	}

	/**
	 * Removes a log line from trace tracking
	 *
	 * @param LogLine	The log line that was being tracked
	 * @param bDump		Whether or not to dump from the trace manager as well (defaults to true); cleared from trace manager either way
	 */
	void ClearLogTrace(FString LogLine, bool bDump=true)
	{
		UE_LOG(LogUnitTest, Log, TEXT("Clearing log trace for line: %s"), *LogLine);

		auto MatchLogLine =
			[&](const FLogTraceEntry& CurEntry)
			{
				return CurEntry.LogLine == LogLine;
			};

		int32 ExactIdx = ExactMatches.IndexOfByPredicate(MatchLogLine);
		int32 PartialIdx = PartialMatches.IndexOfByPredicate(MatchLogLine);

		if (ExactIdx != INDEX_NONE)
		{
			ExactMatches.RemoveAt(ExactIdx);
		}

		if (PartialIdx != INDEX_NONE)
		{
			PartialMatches.RemoveAt(PartialIdx);
		}


		if (PartialIdx != INDEX_NONE || ExactIdx != INDEX_NONE)
		{
			if (bDump)
			{
				GTraceManager->Dump(LogLine, false, false);
			}
			else
			{
				GTraceManager->Clear(LogLine);
			}
		}

		if (PartialMatches.Num() == 0 && ExactMatches.Num() == 0)
		{
			GLog->RemoveOutputDevice(this);
		}
	}

	/**
	 * Clears all log tracing
	 *
	 * @param bDump		Whether or not to dump all cleared log trace entries
	 */
	void ClearAll(bool bDump=false)
	{
		UE_LOG(LogUnitTest, Log, TEXT("Clearing all log traces."));

		if (bDump)
		{
			for (auto CurEntry : ExactMatches)
			{
				if (GTraceManager->ContainsTrace(CurEntry.LogLine))
				{
					GTraceManager->Dump(CurEntry.LogLine, false, false);
				}
				else
				{
					UE_LOG(LogUnitTest, Log, TEXT("No stack traces for log trace: %s"), *CurEntry.LogLine);
				}
			}

			for (auto CurEntry : PartialMatches)
			{
				if (GTraceManager->ContainsTrace(CurEntry.LogLine))
				{
					GTraceManager->Dump(CurEntry.LogLine, false, false);
				}
				else
				{
					UE_LOG(LogUnitTest, Log, TEXT("No stack traces for (partial) log trace: %s"), *CurEntry.LogLine);
				}
			}
		}

		ExactMatches.Empty();
		PartialMatches.Empty();

		GLog->RemoveOutputDevice(this);
	}


	// We're hiding UObject::Serialize() by declaring this.  That's OK, but Clang will warn about it.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif
	/**
	 * Detects log entries
	 *
	 * @param Data		The text to write
	 * @param Verbosity	The log level verbosity for the message
	 * @param Category	The log category for the message
	 */
	virtual void Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		static bool bWithinLogTrace = false;

		if (!bWithinLogTrace)
		{
			for (auto CurEntry : ExactMatches)
			{
				if (CurEntry.LogLine == Data)
				{
					bWithinLogTrace = true;
					GTraceManager->AddTrace(CurEntry.LogLine, false, CurEntry.bDump);
					bWithinLogTrace = false;
				}
			}

			if (PartialMatches.Num() > 0)
			{
				for (auto CurEntry : PartialMatches)
				{
					if (FCString::Stristr(Data, *CurEntry.LogLine) != NULL)
					{
						// NOTE: Do not use Data for the TraceName, as this makes things much harder to track
						bWithinLogTrace = true;
						GTraceManager->AddTrace(CurEntry.LogLine, false, CurEntry.bDump);
						bWithinLogTrace = false;
					}
				}
			}
		}
	}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif


public:
	/** List of exact log entries to watch for */
	TArray<FLogTraceEntry> ExactMatches;

	/** List of partial log entries to watch for */
	TArray<FLogTraceEntry> PartialMatches;
};


// @todo #JohnBFeatureDebug: Continue implementing
#if 0
#if !UE_BUILD_SHIPPING
/**
 * A ProcessEvent hook, which watches for the specified function calls, and ties them into the stack trace manager
 */
class FEventStackTraceManager : FProcessEventHookBase
{
public:
	/**
	 * Base constructor
	 */
	FEventStackTraceManager()
		: FProcessEventHookBase()
	{
	}

	virtual void ProcessEventHook(AActor* Actor, UFunction* Function, void* Parameters) override
	{
		// @todo JohnB: Continue
	}
};
#endif
#endif


/**
 * General debug functions
 */
struct NETCODEUNITTEST_API NUTDebug
{
	// @todo #JohnBMerge: Might be useful to find a place for the hexdump functions within the base engine code itself;
	//				they are very useful

	// NOTE: If outputting hex dump info to the unit test log windows, make use of bMonospace, to retain the hex formatting

	/**
	 * Quick conversion of a string to a hex-dumpable byte array
	 *
	 * @param InString		The string to convert
	 * @param OutBytes		The byte array to output to
	 */
	static inline void StringToBytes(const FString& InString, TArray<uint8>& OutBytes)
	{
		TArray<TCHAR> InStrCharArray = InString.GetCharArray();

		for (int i=0; i<InString.Len(); i++)
		{
			uint8 CharBytes[sizeof(TCHAR)];
			FMemory::Memcpy(&CharBytes[0], &InStrCharArray[i], sizeof(TCHAR));

			for (int CharIdx=0; CharIdx<sizeof(TCHAR); CharIdx++)
			{
				OutBytes.Add(CharBytes[CharIdx]);
			}
		}
	}

	/**
	 * Takes an array of bytes, and generates a hex dump string out of them, optionally including
	 * an ASCII dump and dumping byte offsets also (intended for debugging in the log window)
	 *
	 * @param InBytes		The array of bytes to be dumped
	 * @param bDumpASCII	Whether or not to dump ASCII along with the hex
	 * @param bDumpOffset	Whether or not to dump offset margins for hex rows/columns
	 * @return				Returns the hex dump, including line terminators
	 */
	static FString HexDump(const TArray<uint8>& InBytes, bool bDumpASCII=true, bool bDumpOffset=true);

	/**
	 * Version of the above hex-dump function, which takes a byte pointer and length as input
	 */
	static inline FString HexDump(const uint8* InBytes, const uint32 InBytesLen, bool bDumpASCII=true, bool bDumpOffset=true)
	{
		TArray<uint8> InBytesArray;

		InBytesArray.AddUninitialized(InBytesLen);
		FMemory::Memcpy(InBytesArray.GetData(), InBytes, InBytesLen);

		return HexDump(InBytesArray, bDumpASCII, bDumpOffset);
	}

	/**
	 * Version of the above hex-dump function, which takes a string as input
	 */
	static inline FString HexDump(const FString& InString, bool bDumpASCII=true, bool bDumpOffset=false)
	{
		TArray<uint8> InStrBytes;

		StringToBytes(InString, InStrBytes);

		return HexDump(InStrBytes, bDumpASCII, bDumpOffset);
	}

	/**
	 * Version of the above hex-dump function, which dumps in a format more friendly/readable
	 * in log text files
	 */
	static inline void LogHexDump(const TArray<uint8>& InBytes, bool bDumpASCII=true, bool bDumpOffset=false, FOutputDevice* OutLog=GLog)
	{
		FString HexDumpStr = HexDump(InBytes, bDumpASCII, bDumpOffset);
		TArray<FString> LogLines;

#if TARGET_UE4_CL < CL_STRINGPARSEARRAY
		HexDumpStr.ParseIntoArray(&LogLines, LINE_TERMINATOR, false);
#else
		HexDumpStr.ParseIntoArray(LogLines, LINE_TERMINATOR, false);
#endif

		for (int32 i=0; i<LogLines.Num(); i++)
		{
			// NOTE: It's important to pass it in as a parameter, otherwise there is a crash if the line contains '%s'
			OutLog->Logf(TEXT(" %s"), *LogLines[i]);
		}
	}

	/**
	 * Version of the above hex-dump logging function, which takes a byte pointer and length as input
	 */
	static inline void LogHexDump(const uint8* InBytes, const uint32 InBytesLen, bool bDumpASCII=true, bool bDumpOffset=false,
							FOutputDevice* OutLog=GLog)
	{
		TArray<uint8> InBytesArray;

		InBytesArray.AddUninitialized(InBytesLen);
		FMemory::Memcpy(InBytesArray.GetData(), InBytes, InBytesLen);

		LogHexDump(InBytesArray, bDumpASCII, bDumpOffset, OutLog);
	}

	/**
	 * Version of the above, which takes a string as input
	 */
	static inline void LogHexDump(const FString& InString, bool bDumpASCII=true, bool bDumpOffset=false, FOutputDevice* OutLog=GLog)
	{
		TArray<uint8> InStrBytes;

		StringToBytes(InString, InStrBytes);
		LogHexDump(InStrBytes, bDumpASCII, bDumpOffset, OutLog);
	}


	/**
	 * Takes an array of bytes, and generates a bit-based/binary dump string out of them, optionally including
	 * byte offsets also (intended for debugging in the log window)
	 *
	 * @param InBytes		The array of bytes to be dumped
	 * @param bDumpOffset	Whether or not to dump offset margins for bit rows/columns
	 * @param bLSBFirst		Whether or not the Least Significant Bit should be printed first, instead of last
	 * @return				Returns the hex dump, including line terminators
	 */
	static FString BitDump(const TArray<uint8>& InBytes, bool bDumpOffset=true, bool bLSBFirst=false);


	/**
	 * Version of the above bit-dump function, which takes a byte pointer and length as input
	 */
	static inline FString BitDump(const uint8* InBytes, const uint32 InBytesLen, bool bDumpOffset=true, bool bLSBFirst=false)
	{
		TArray<uint8> InBytesArray;

		InBytesArray.AddUninitialized(InBytesLen);
		FMemory::Memcpy(InBytesArray.GetData(), InBytes, InBytesLen);

		return BitDump(InBytesArray, bDumpOffset, bLSBFirst);
	}

	/**
	 * Version of the above bit-dump function, which takes a string as input
	 */
	static inline FString BitDump(const FString& InString, bool bDumpOffset=false, bool bLSBFirst=false)
	{
		TArray<uint8> InStrBytes;

		StringToBytes(InString, InStrBytes);

		return BitDump(InStrBytes, bDumpOffset, bLSBFirst);
	}

	/**
	 * Version of the above bit-dump function, which dumps in a format more friendly/readable
	 * in log text files
	 */
	static inline void LogBitDump(const TArray<uint8>& InBytes, bool bDumpOffset=false, bool bLSBFirst=false, FOutputDevice* OutLog=GLog)
	{
		FString BitDumpStr = BitDump(InBytes, bDumpOffset, bLSBFirst);
		TArray<FString> LogLines;

#if TARGET_UE4_CL < CL_STRINGPARSEARRAY
		BitDumpStr.ParseIntoArray(&LogLines, LINE_TERMINATOR, false);
#else
		BitDumpStr.ParseIntoArray(LogLines, LINE_TERMINATOR, false);
#endif

		for (int32 i=0; i<LogLines.Num(); i++)
		{
			// NOTE: It's important to pass it in as a parameter, otherwise there is a crash if the line contains '%s'
			OutLog->Logf(TEXT(" %s"), *LogLines[i]);
		}
	}

	/**
	 * Version of the above bit-dump logging function, which takes a byte pointer and length as input
	 */
	static inline void LogBitDump(const uint8* InBytes, const uint32 InBytesLen, bool bDumpOffset=false, bool bLSBFirst=false,
							FOutputDevice* OutLog=GLog)
	{
		TArray<uint8> InBytesArray;

		InBytesArray.AddUninitialized(InBytesLen);
		FMemory::Memcpy(InBytesArray.GetData(), InBytes, InBytesLen);

		LogBitDump(InBytesArray, bDumpOffset, bLSBFirst, OutLog);
	}

	/**
	 * Version of the above, which takes a string as input
	 */
	static inline void LogBitDump(const FString& InString, bool bDumpOffset=false, bool bLSBFirst=false, FOutputDevice* OutLog=GLog)
	{
		TArray<uint8> InStrBytes;

		StringToBytes(InString, InStrBytes);
		LogBitDump(InStrBytes, bDumpOffset, bLSBFirst, OutLog);
	}
};

