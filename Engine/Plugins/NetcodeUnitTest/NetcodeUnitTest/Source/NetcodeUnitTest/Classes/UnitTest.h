// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateColor.h"
#include "Misc/App.h"
#include "Misc/OutputDeviceFile.h"
#include "NetcodeUnitTest.h"
#include "UnitTestBase.h"

#include "UnitTest.generated.h"

class FUnitTestEnvironment;
class SLogWindow;

// @todo #JohnBFeature: For bugtracking/changelist info, consider adding auto-launching of P4/TTP/Browser-JIRA links,
//				upon double-clicking these entries in the status windows


// Forward declarations
class FUnitTestEnvironment;
class SLogWindow;
struct FTCCommonVars;


/**
 * Enums
 */

/**
 * The verification status of the current unit test - normally its execution completes immediately after positive/negative verification
 */
UENUM()
enum class EUnitTestVerification : uint8
{
	/** Unit test is not yet verified */
	Unverified,
	/** Unit test is verified as not fixed */
	VerifiedNotFixed,
	/** Unit test is verified as fixed */
	VerifiedFixed,
	/** Unit test is no longer functioning, needs manual check/update (issue may be fixed, or unit test broken) */
	VerifiedNeedsUpdate,
	/** Unit test is verified as having executed unreliably */
	VerifiedUnreliable,
};


/**
 * Structs
 */

/**
 * Used for storing unit-test-specific logs, which are displayed in the status window
 * (upon completion of unit testing, a final summary is printed using this data, but in a more-ordered/easier-to-read fashion)
 */
struct FUnitStatusLog
{
	/** The log type for this status log */
	ELogType	LogType;

	/** The log line */
	FString		LogLine;


	FUnitStatusLog()
		: LogType(ELogType::None)
		, LogLine(TEXT(""))
	{
	}

	FUnitStatusLog(ELogType InLogType, const FString& InLogLine)
		: LogType(InLogType)
		, LogLine(InLogLine)
	{
	}
};


/**
 * Base class for all unit tests
 */
UCLASS(config=UnitTestStats)
class NETCODEUNITTEST_API UUnitTest : public UUnitTestBase
{
	GENERATED_UCLASS_BODY()

	friend class UUnitTestManager;
	friend class FUnitTestEnvironment;
	friend class UMinimalClient;
	friend struct NUTNet;


	/** Variables which should be specified by every subclass */
protected:
	/** The name/command for this unit test (N.B. Must be set in class constructor) */
	FString UnitTestName;

	/** The type of unit test this is (e.g. bug/exploit) (N.B. Must be set in class constructor) */
	FString UnitTestType;

	/** The date this unit test was added to the project (for ordering in help command) */
	FDateTime UnitTestDate;


	/** The bug tracking identifiers related to this unit test (e.g. TTP numbers) */
	TArray<FString> UnitTestBugTrackIDs;

	/** Source control changelists relevant to this unit test */
	TArray<FString> UnitTestCLs;


	/** Whether or not this unit test is a 'work in progress', and should not be included in automated tests */
	bool bWorkInProgress;

	/** Whether or not this unit test is unreliable, i.e. prone to giving incorrect/unexpected results, requiring multiple runs */
	bool bUnreliable;


	/**
	 * The unit test result we expect for each games codebase, i.e. whether we expect that the problem is fixed yet or not
	 * NOTE: Games which don't have an expected result specified here, are considered 'unsupported' and the unit test isn't run for them
	 */
	TMap<FString, EUnitTestVerification> ExpectedResult;


	/** The amount of time (in seconds), before the unit test should timeout and be marked as broken */
	uint32 UnitTestTimeout;


	/** Config variables */
public:
	/** Stores stats on the highest-ever reported memory usage, for this unit test - for estimating memory usage */
	UPROPERTY(config)
	uint64 PeakMemoryUsage;

	/** The amount of time it takes to reach 'PeakMemoryUsage' (or within 90% of its value) */
	UPROPERTY(config)
	float TimeToPeakMem;

	/** The amount of time it took to execute the unit test the last time it was run */
	UPROPERTY(config)
	float LastExecutionTime;



	/** Runtime variables */
protected:
	/** The unit test environment (not set until the current games unit test module is loaded - not set at all, if no such module) */
	static FUnitTestEnvironment* UnitEnv;

	/** The null unit test environment - for unit tests which support all games, due to requiring no game-specific features */
	static FUnitTestEnvironment* NullUnitEnv;

	/** The time of the last NetTick event */
	double LastNetTick;

	/** The current realtime memory usage of the unit test */
	uint64 CurrentMemoryUsage;


	/** The time at which execution of the unit test started */
	double StartTime;


	/** The time at which the unit test timeout will expire */
	double TimeoutExpire;

	/** The last time that the unit test timeout was reset */
	double LastTimeoutReset;

	/** Every timeout reset specifies a string to identify/describe the event that triggered it, for tracking */
	FString LastTimeoutResetEvent;

	/** Whether or not developer-mode has been enabled for this unit test (prevents it from ending execution) */
	bool bDeveloperMode;


	/** Whether it's the first time this unit test has run, i.e. whether prior memory stats exist (NOTE: Not set until first tick) */
	bool bFirstTimeStats;


	// @todo #JohnBRefactor: Merge the two below variables
	/** Whether or not the unit test has completed */
	bool bCompleted;

	/** Whether or not the success or failure of the current unit test has been verified */
	UPROPERTY()
	EUnitTestVerification VerificationState;

private:
	/** Whether or not the verification state as already logged (prevent spamming in developer mode) */
	bool bVerificationLogged;

protected:
	/** Whether or not the unit test has aborted execution */
	bool bAborted;


	/** The log window associated with this unit test */
	TSharedPtr<SLogWindow> LogWindow;

	/** Overrides the colour of UNIT_LOG log messages */
	FSlateColor LogColor;


	/** Collects unit test status logs, that have been printed to the summary window */
	TArray<TSharedPtr<FUnitStatusLog>> StatusLogSummary;


	/** The log file for outputting all log information for the current unit test */
	TUniquePtr<FOutputDeviceFile> UnitLog;

	/** The log directory for this unit test */
	FString UnitLogDir;


public:
	/**
	 * Returns the name/command, for the current unit test
	 */
	FORCEINLINE FString GetUnitTestName() const
	{
		return UnitTestName;
	}

	/**
	 * Returns the type of unit test (e.g. bug/exploit)
	 */
	FORCEINLINE FString GetUnitTestType() const
	{
		return UnitTestType;
	}

	/**
	 * Returns the date this unit test was first added to the code
	 */
	FORCEINLINE FDateTime GetUnitTestDate() const
	{
		return UnitTestDate;
	}

	/**
	 * Returns the expected result for the current game
	 */
	FORCEINLINE EUnitTestVerification GetExpectedResult()
	{
		EUnitTestVerification Result = EUnitTestVerification::Unverified;

		FString CurGame = FApp::GetProjectName();

		if (ExpectedResult.Contains(CurGame))
		{
			Result = ExpectedResult[CurGame];
		}
		else if (ExpectedResult.Contains(TEXT("NullUnitEnv")))
		{
			Result = ExpectedResult[TEXT("NullUnitEnv")];
		}

		return Result;
	}

	/**
	 * Returns the list of supported games, for this unit test
	 */
	FORCEINLINE TArray<FString> GetSupportedGames()
	{
		TArray<FString> SupportedGames;

		ExpectedResult.GenerateKeyArray(SupportedGames);

		return SupportedGames;
	}


	/**
	 * Returns whether or not this is the first time the unit test has been run/collecting-stats
	 */
	FORCEINLINE bool IsFirstTimeStats() const
	{
		return bFirstTimeStats || PeakMemoryUsage == 0;
	}


protected:
	/**
	 * Finishes initializing unit test settings, that rely upon the current unit test environment being loaded
	 */
	virtual void InitializeEnvironmentSettings()
	{
	}

	/**
	 * Validate that the unit test settings/flags specified for this unit test, are compatible with one another,
	 * and that the engine settings/environment, support running the unit test.
	 *
	 * @param bCDOCheck		If true, the class default object is being validated before running - don't check class runtime variables
	 * @return				Returns true, if all settings etc. check out
	 */
	virtual bool ValidateUnitTestSettings(bool bCDOCheck=false);

	/**
	 * Returns the type of log entries that this unit expects to output, for setting up log window filters
	 * (only needs to return values which affect what tabs are shown)
	 *
	 * @return		The log type mask, representing the type of log entries this unit test expects to output
	 */
	virtual ELogType GetExpectedLogTypes()
	{
		return ELogType::Local;
	}

	/**
	 * Resets the unit test timeout code - should be used liberally, within every unit test, when progress is made during execution
	 *
	 * @param ResetReason			Human-readable reason for resetting the timeout
	 * @param bResetConnTimeout		Whether or not to also reset timeouts on unit test connections
	 * @param MinDuration			The minimum timeout duration (used in subclasses, to override the timeout duration)
	 */
	virtual void ResetTimeout(FString ResetReason, bool bResetConnTimeout=false, uint32 MinDuration=0)
	{
		uint32 CurrentTimeout = FMath::Max(MinDuration, UnitTestTimeout);
		double NewTimeoutExpire = FPlatformTime::Seconds() + (double)CurrentTimeout;

		// Don't reset to a shorter timeout, than is already in place
		TimeoutExpire = FMath::Max(NewTimeoutExpire, TimeoutExpire);

		LastTimeoutReset = FPlatformTime::Seconds();
		LastTimeoutResetEvent = ResetReason;
	}

	virtual bool UTStartUnitTest() override final;

	/**
	 * Sets up the log directory and log output device instances.
	 */
	void InitializeLogs();

public:
	/**
	 * Executes the main unit test
	 *
	 * @return	Whether or not the unit test kicked off execution successfully
	 */
	virtual bool ExecuteUnitTest() PURE_VIRTUAL(UUnitTest::ExecuteUnitTest, return false;)

	/**
	 * Aborts execution of the unit test, part-way through
	 */
	virtual void AbortUnitTest();

	/**
	 * Called upon completion of the unit test (may not happen during same tick), for tearing down created worlds/connections/etc.
	 * NOTE: Should be called last, in overridden functions, as this triggers deletion of the unit test object
	 */
	virtual void EndUnitTest();

	/**
	 * Cleans up all items needing destruction, and removes the unit test from tracking, before deleting the unit test itself
	 */
	virtual void CleanupUnitTest();


	/**
	 * For implementation in subclasses, for helping to track local log entries related to this unit test.
	 * This covers logs from within the unit test, and (for UClientUnitTest) from processing net packets related to this unit test.
	 *
	 * NOTE: The parameters are the same as the unprocessed log 'serialize' parameters, to convert to a string, use:
	 * FOutputDeviceHelper::FormatLogLine(Verbosity, Category, Data, GPrintLogTimes)
	 *
	 * NOTE: Verbosity ELogVerbosity::SetColor is a special category, whose log messages can be ignored.
	 *
	 * @param InLogType	The type of local log message this is
	 * @param Data		The base log message being written
	 * @param Verbosity	The warning/filter level for the log message
	 * @param Category	The log message category (LogNet, LogNetTraffic etc.)
	 */
	virtual void NotifyLocalLog(ELogType InLogType, const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category);


	/**
	 * Notifies that there was a request to enable/disable developer mode
	 *
	 * @param bInDeveloperMode	Whether or not developer mode is being enabled/disabled
	 */
	void NotifyDeveloperModeRequest(bool bInDeveloperMode);

	/**
	 * Notifies that there was a request to execute a console command for the unit test, which can occur in a specific context,
	 * e.g. for a unit test server, for a local minimal-client (within the unit test), or for a separate unit test client process
	 *
	 * @param CommandContext	The context (local/server/client?) for the console command
	 * @param Command			The command to be executed
	 * @return					Whether or not the command was handled
	 */
	virtual bool NotifyConsoleCommandRequest(FString CommandContext, FString Command);

	/**
	 * Outputs the list of console command contexts, that this unit test supports (which can include custom contexts in subclasses)
	 *
	 * @param OutList				Outputs the list of supported console command contexts
	 * @param OutDefaultContext		Outputs the context which should be auto-selected/defaulted-to
	 */
	virtual void GetCommandContextList(TArray<TSharedPtr<FString>>& OutList, FString& OutDefaultContext);


	virtual void PostUnitTick(float DeltaTime) override;

	virtual bool IsTickable() const override;

	virtual void TickIsComplete(float DeltaTime) override;

	/**
	 * Triggered upon unit test completion, for outputting that the unit test has completed - plus other unit test state information
	 */
	virtual void LogComplete();


	/**
	 * Getters/Setters
	 */

	FORCEINLINE void SetLogColor(FSlateColor InLogColor=FSlateColor::UseForeground())
	{
		LogColor = InLogColor;
	}
};
