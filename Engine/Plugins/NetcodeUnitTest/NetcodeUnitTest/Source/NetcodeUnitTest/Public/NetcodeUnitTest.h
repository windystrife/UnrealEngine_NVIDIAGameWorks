// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"

/**
 * Compatibility defines
 */

// @todo #JohnBRefactor: Perhaps move compatibility defines to its own header

// @todo #JohnB: I need accurate mainline CL numbers for UT integrations (they aren't included in the merge changelist descriptions)


/** List of mainline UE4 changelists, representing UE4 version changes (as judged by changes to Version.h) */
#define CL_4_10	2626674
#define CL_4_9	2526821
#define CL_4_8	2388573
#define CL_4_7	2347015
#define CL_4_6	2308471

/** List of UnrealTournament mainline merges (number represents merged mainline CL) NOTE: Estimated. No integrated CL number in merge */
#define CL_UT_4_8 CL_4_9	// Integrated with final 4.8 release, which is approximately same time as main branch started 4.9
#define CL_UT_4_7 CL_4_7
#define CL_UT_4_6 CL_4_6

/** List of Fortnite mainline merges (number represents merged mainline CL) */
#define CL_FORT_4_8_APRIL 2509925
#define CL_FORT_4_8 2415178
#define CL_FORT_4_7 2349525


/**
 * List of mainline UE4 engine changelists, that required a NetcodeUnitTest compatibility adjustment (newest first)
 * Every time you update NetcodeUnitTest code, to toggle based on a changelist, add a define for it with the CL number here
 */
#define CL_PREVERSIONCHANGE	2960134	// Not an accurate CL, just refers to the first version of UE4 code I can be sure changes work on
#define CL_STATELESSCONNECT	2866629	// @todo #JohnB: Refers to Dev-Networking, update so it refers to /UE4//Main merge CL
#define CL_FENGINEVERSION	2655102
#define CL_INITCONNPARAM	2567692
#define CL_CONSTUNIQUEID	2540329
#define CL_CONSTNETCONN		2501704
#define CL_INPUTCHORD		2481648
#define CL_CLOSEPROC		2476050
#define CL_STRINGPARSEARRAY	2466824
#define CL_BEACONHOST		2456855
#define CL_GETSELECTIONMODE	2425976
#define CL_DEPRECATENEW		2425600
#define CL_DEPRECATEDEL		2400883
#define CL_FNETWORKVERSION	2384479



/**
 * The changelist number for //depot/UE4/... that NetcodeUnitTest should adjust compatibility for. Set to the CL your workspace uses.
 *
 * If using NetcodeUnitTest with a different branch (e.g. UnrealTournament/Fortnite), target the last-merged CL from //depot/UE4/...
 *
 * If in doubt, set to the top CL from list above ('List of mainline UE4 engine changelists'), and work your way down until it compiles.
 */
#define TARGET_UE4_CL 2960134	// IMPORTANT: If you're hovering over this because compiling failed, you need to adjust this value.

class Error;
class UNetConnection;
class UUnitTest;
class UUnitTestBase;
class UUnitTestManager;

/**
 * Forward declarations
 */
class UNetConnection;
class UUnitTestManager;
class UUnitTestBase;


// @todo #JohnBLowPri: Status window logs (such as unit test success/failure), don't appear to add the same colouring to main game log window;
//				add this sometime (probably based on how FUnitTestProcess does it)

/**
 * Enums
 */

// @todo #JohnBRefactor: Trim values from the below enum - not all are necessary

/**
 * Used to help identify what type of log is being processed
 *
 * NOTE: This enum is not compatible with UENUM (that requires uint8)
 */
enum class ELogType : uint32
{
	None				= 0x00000000,					// Not set

	/** What part of the engine the log message originates from locally (may not be set) */
	OriginUnitTest		= 0x00000001,					// Log originating from unit test code
	OriginEngine		= 0x00000002,					// Log originating from an engine event in the unit test (e.g. unit test Tick)
	OriginNet			= 0x00000004,					// Log originating from netcode for current unit test (specifically net receive)
	OriginConsole		= 0x00000008,					// Log originating from a console command (typically from the unit test window)
	OriginVoid			= 0x00000010,					// Log which should have no origin, and which should be ignored by log capturing

	OriginMask			= OriginUnitTest | OriginEngine | OriginNet | OriginConsole | OriginVoid,


	/** What class of unit test log this is */
	Local				= 0x00000080,					// Log from locally executed code (displayed in 'Local' tab)
	Server				= 0x00000100,					// Log from a server instance (displayed in 'Server' tab)
	Client				= 0x00000200,					// Log from a client instance (displayed in 'Client' tab)


	/** What class of unit test status log this is (UNIT_LOG vs UNIT_STATUS_LOG/STATUS_LOG) */
	GlobalStatus		= 0x00000400,					// Status placed within the overall status window
	UnitStatus			= 0x00000800,					// Status placed within the unit test window

	/** Status log modifiers (used with the UNIT_LOG and UNIT_STATUS_LOG/STATUS_LOG macro's) */
	StatusImportant		= 0x00001000,					// An important status event (displayed in the 'Summary' tab)
	StatusSuccess		= 0x00002000 | StatusImportant,	// Success event status
	StatusWarning		= 0x00004000 | StatusImportant,	// Warning event status
	StatusFailure		= 0x00008000 | StatusImportant,	// Failure event status
	StatusError			= 0x00010000 | StatusFailure,	// Error/Failure event status, that triggers an overall unit test failure
	StatusDebug			= 0x00020000,					// Debug status (displayed in the 'Debug' tab)
	StatusAdvanced		= 0x00040000,					// Status event containing advanced/technical information
	StatusVerbose		= 0x00080000,					// Status event containing verbose information
	StatusAutomation	= 0x00100000,					// Status event which should be printed out to the automation tool


	/** Log output style modifiers */
	StyleBold			= 0x00200000,					// Output text in bold
	StyleItalic			= 0x00400000,					// Output text in italic
	StyleUnderline		= 0x00800000,					// Output pseudo-underline text (add newline and --- chars; UE4 can't underline)
	StyleMonospace		= 0x01000000,					// Output monospaced text (e.g. for list tab formatting); can't use bold/italic

	All					= 0xFFFFFFFF,


	/** Log lines that should request focus when logged (i.e. if not displayed in the current tab, switch to a tab that does display) */
	FocusMask			= OriginConsole,
};

ENUM_CLASS_FLAGS(ELogType);


/**
 * Globals/externs
 */

/** Holds a reference to the object in charge of managing unit tests */
extern UUnitTestManager* GUnitTestManager;


// IMPORTANT: If you add more engine-log-capture globals, you must add them to the 'UNIT_EVENT_CLEAR' and related macros

/** Used by to aid in hooking log messages triggered by unit tests */
extern NETCODEUNITTEST_API UUnitTestBase* GActiveLogUnitTest;
extern UUnitTestBase* GActiveLogEngineEvent;
extern UWorld* GActiveLogWorld;				// @todo #JohnBReview: Does this make other log hooks redundant?

/** Keeps track of the UUnitTestNetConnection, currently processing received data (to aid with selective log hooking) */
extern UNetConnection* GActiveReceiveUnitConnection;

/** Whether not an actor channel, is in the process of initializing the remote actor */
extern bool GIsInitializingActorChan;

/** The current ELogType flag modifiers, associated with a UNIT_LOG or UNIT_STATUS_LOG/STATUS_LOG call */
extern NETCODEUNITTEST_API ELogType GActiveLogTypeFlags;


/**
 * Declarations
 */

NETCODEUNITTEST_API DECLARE_LOG_CATEGORY_EXTERN(LogUnitTest, Log, All);

// Hack to allow log entries to print without the category (specify log type of 'none')
DECLARE_LOG_CATEGORY_EXTERN(NetCodeTestNone, Log, All);


/**
 * Defines
 */

/**
 * The IPC pipe used for resuming a suspended server
 * NOTE: You must append the process ID of the server to this string
 */
#define NUT_SUSPEND_PIPE TEXT("\\\\.\\Pipe\\NetcodeUnitTest_SuspendResume")


/**
 * Macro defines
 */

/**
 * Helper function, for macro defines - allows the 'UnitLogTypeFlags' macro parameters below, to be optional
 */
inline ELogType OptionalFlags(ELogType InFlags=ELogType::None)
{
	return InFlags;
}

// Actual asserts (not optimized out, like almost all other engine assert macros)
// @todo #JohnBLowPri: Try to get this to show up a message box, or some other notification, rather than just exiting immediately
#define UNIT_ASSERT(Condition) \
	if (!(Condition)) \
	{ \
		UE_LOG(LogUnitTest, Error, TEXT(PREPROCESSOR_TO_STRING(__FILE__)) TEXT(": Line: ") TEXT(PREPROCESSOR_TO_STRING(__LINE__)) \
				TEXT(":")); \
		UE_LOG(LogUnitTest, Error, TEXT("Condition '(") TEXT(PREPROCESSOR_TO_STRING(Condition)) TEXT(")' failed")); \
		check(false); /* Try to get a meaningful stack trace. */ \
		FPlatformMisc::RequestExit(true); \
		CA_ASSUME(false); \
	}


// Used with the below macros, and sometimes to aid with large unit tests logs

#define UNIT_LOG_BEGIN(UnitTestObj, UnitLogTypeFlags) \
	{ \
		UUnitTestBase* OldUnitLogVal = GActiveLogUnitTest; \
		GActiveLogUnitTest = (UnitTestObj != nullptr ? UnitTestObj : GActiveLogUnitTest); \
		GActiveLogTypeFlags = ELogType::UnitStatus | OptionalFlags(UnitLogTypeFlags);

#define UNIT_LOG_END() \
		GActiveLogTypeFlags = ELogType::None; \
		GActiveLogUnitTest = OldUnitLogVal; \
	}

#define UNIT_LOG_VOID_START() \
	GActiveLogTypeFlags = ELogType::OriginVoid;

#define UNIT_LOG_VOID_END() \
	GActiveLogTypeFlags = ELogType::None;


/**
 * Special log macro for unit tests, to aid in hooking logs from these unit tests
 * NOTE: These logs are scoped
 *
 * @param UnitTestObj		Reference to the unit test this log message is originating from
 * @param UnitLogTypeFlags	(Optional) The additional ELogFlags flags, to be applied to this log
 * @param Format			The format of the log message
 */
#define UNIT_LOG_OBJ(UnitTestObj, UnitLogTypeFlags, Format, ...) \
	UNIT_LOG_BEGIN(UnitTestObj, UnitLogTypeFlags); \
		if ((OptionalFlags(UnitLogTypeFlags) & ELogType::StatusError) == ELogType::StatusError) \
		{ \
			UE_LOG(LogUnitTest, Error, Format, ##__VA_ARGS__); \
		} \
		else if ((OptionalFlags(UnitLogTypeFlags) & ELogType::StatusWarning) == ELogType::StatusWarning) \
		{ \
			UE_LOG(LogUnitTest, Warning, Format, ##__VA_ARGS__); \
		} \
		else \
		{ \
			UE_LOG(LogUnitTest, Log, Format, ##__VA_ARGS__); \
		} \
	UNIT_LOG_END(); \
	UNIT_LOG_VOID_START(); \
	if ((OptionalFlags(UnitLogTypeFlags) & ELogType::StatusError) == ELogType::StatusError) \
	{ \
		UE_LOG(LogUnitTest, Error, TEXT("%s: %s"), (UnitTestObj != nullptr ? *(UnitTestObj->GetUnitTestName()) : TEXT("nullptr")), \
				*FString::Printf(Format, ##__VA_ARGS__)); \
	} \
	else if ((OptionalFlags(UnitLogTypeFlags) & ELogType::StatusWarning) == ELogType::StatusWarning) \
	{ \
		UE_LOG(LogUnitTest, Warning, TEXT("%s: %s"), (UnitTestObj != nullptr ? *(UnitTestObj->GetUnitTestName()) : TEXT("nullptr")), \
				*FString::Printf(Format, ##__VA_ARGS__)); \
	} \
	UNIT_LOG_VOID_END();


// More-concise version of the above macro

#define UNIT_LOG(UnitLogTypeFlags, Format, ...) \
	UNIT_LOG_OBJ(this, UnitLogTypeFlags, Format, ##__VA_ARGS__)


// @todo #JohnBDoc: Document these macros

#define UNIT_EVENT_BEGIN(UnitTestObj) \
	UUnitTestBase* OldEventLogVal = GActiveLogEngineEvent; \
	GActiveLogEngineEvent = UnitTestObj;
	
#define UNIT_EVENT_END \
	GActiveLogEngineEvent = OldEventLogVal;

/**
 * Stores and then clears all engine-log-capture events (in order to prevent capture of a new log entry)
 * NOTE: Do not clear GActiveLogUnitTest here, as some macros that use this, rely upon that
 */
#define UNIT_EVENT_CLEAR \
	UUnitTestBase* OldEventLogVal = GActiveLogEngineEvent; \
	UWorld* OldEventLogWorld = GActiveLogWorld; \
	GActiveLogEngineEvent = nullptr; \
	GActiveLogWorld = nullptr;

/**
 * Restores all stored/cleared engine-log-capture events
 */
#define UNIT_EVENT_RESTORE \
	UNIT_EVENT_END \
	GActiveLogWorld = OldEventLogWorld;


/**
 * Special log macro, for messages that should be printed to the unit test status window
 */
#define STATUS_LOG_BASE(UnitLogTypeFlags, Format, ...) \
	{ \
		GActiveLogTypeFlags = ELogType::GlobalStatus | OptionalFlags(UnitLogTypeFlags); \
		UUnitTestManager* Manager = UUnitTestManager::Get(); \
		Manager->SetStatusLog(true); \
		Manager->Logf(Format, ##__VA_ARGS__); \
		Manager->SetStatusLog(false); \
		\
		if ((GActiveLogTypeFlags & ELogType::StatusAutomation) == ELogType::StatusAutomation && GIsAutomationTesting) \
		{ \
			GWarn->Logf(Format, ##__VA_ARGS__); \
		} \
		\
		GActiveLogTypeFlags = ELogType::None; \
	}

#define STATUS_LOG_OBJ(InUnitTest, UnitLogTypeFlags, Format, ...) \
	{ \
		GActiveLogTypeFlags = ELogType::GlobalStatus | OptionalFlags(UnitLogTypeFlags); \
		UUnitTest* SourceUnitTest = InUnitTest; \
		UNIT_EVENT_CLEAR; \
		if ((GActiveLogTypeFlags & ELogType::StatusError) == ELogType::StatusError) \
		{ \
			if (SourceUnitTest != nullptr) \
			{ \
				UE_LOG(LogUnitTest, Error, TEXT("%s: %s"), *SourceUnitTest->GetUnitTestName(), *FString::Printf(Format, ##__VA_ARGS__)); \
			} \
			else \
			{ \
				UE_LOG(LogUnitTest, Error, Format, ##__VA_ARGS__) \
			} \
		} \
		else if ((GActiveLogTypeFlags & ELogType::StatusWarning) == ELogType::StatusWarning) \
		{ \
			if (SourceUnitTest != nullptr) \
			{ \
				UE_LOG(LogUnitTest, Warning, TEXT("%s: %s"), *SourceUnitTest->GetUnitTestName(), \
						*FString::Printf(Format, ##__VA_ARGS__)); \
			} \
			else \
			{ \
				UE_LOG(LogUnitTest, Warning, Format, ##__VA_ARGS__) \
			} \
		} \
		else \
		{ \
			UE_LOG(LogUnitTest, Log, Format, ##__VA_ARGS__) \
		} \
		UNIT_EVENT_RESTORE; \
		STATUS_LOG_BASE(UnitLogTypeFlags, Format, ##__VA_ARGS__) \
		GActiveLogTypeFlags = ELogType::None; \
	}

#define STATUS_LOG(UnitLogTypeFlags, Format, ...) STATUS_LOG_OBJ(nullptr, UnitLogTypeFlags, Format, ##__VA_ARGS__)

/**
 * Version of the above, for unit test status window entries, which are from specific unit tests
 */
#define UNIT_STATUS_LOG_OBJ(UnitTestObj, UnitLogTypeFlags, Format, ...) \
	UNIT_LOG_BEGIN(UnitTestObj, UnitLogTypeFlags); \
	STATUS_LOG_OBJ(UnitTestObj, UnitLogTypeFlags, Format, ##__VA_ARGS__) \
	UNIT_LOG_END();

#define UNIT_STATUS_LOG(UnitLogTypeFlags, Format, ...) \
	UNIT_STATUS_LOG_OBJ(this, UnitLogTypeFlags, Format, ##__VA_ARGS__)


/**
 * Changes/resets the colour of messages printed to the unit test status window
 */
#define STATUS_SET_COLOR(InColor) \
	{ \
		UUnitTestManager* Manager = UUnitTestManager::Get(); \
		Manager->SetStatusColor(InColor); \
	}

#define STATUS_RESET_COLOR() \
	{ \
		UUnitTestManager* Manager = UUnitTestManager::Get(); \
		Manager->SetStatusColor(); /* No value specified = reset */ \
	}

