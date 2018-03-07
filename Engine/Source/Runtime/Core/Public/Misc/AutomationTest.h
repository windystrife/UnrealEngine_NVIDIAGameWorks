// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/Array.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "Templates/Function.h"
#include "Containers/Map.h"
#include "Math/Color.h"
#include "Templates/SharedPointer.h"
#include "Delegates/Delegate.h"
#include "Misc/Optional.h"
#include "HAL/PlatformTime.h"
#include "HAL/ThreadSafeBool.h" 
#include "HAL/PlatformStackWalk.h"
#include "GenericPlatformStackWalk.h" 
#include "Containers/Queue.h"
#include "Misc/FeedbackContext.h"
#include "Future.h"
#include "Async.h"
#include "Misc/Guid.h"
#include "Math/Vector.h"
#include "Math/Color.h"
#include "PlatformProcess.h"
#include "Misc/AutomationEvent.h"
#include "Internationalization/Regex.h"

/** Flags for specifying automation test requirements/behavior */
namespace EAutomationTestFlags
{
	enum Type
	{
		//~ Application context required for the test
		// Test is suitable for running within the editor
		EditorContext = 0x00000001,
		// Test is suitable for running within the client
		ClientContext = 0x00000002,
		// Test is suitable for running within the server
		ServerContext = 0x00000004,
		// Test is suitable for running within a commandlet
		CommandletContext = 0x00000008,
		ApplicationContextMask = EditorContext | ClientContext | ServerContext | CommandletContext,

		//~ Features required for the test - not specifying means it is valid for any feature combination
		// Test requires a non-null RHI to run correctly
		NonNullRHI = 0x00000100,
		// Test requires a user instigated session
		RequiresUser = 0x00000200,
		FeatureMask = NonNullRHI | RequiresUser,

		//~ One-off flag to allow for fast disabling of tests without commenting code out
		// Temp disabled and never returns for a filter
		Disabled = 0x00010000,

		//~ Priority of the test
		// The highest priority possible. Showstopper/blocker.
		CriticalPriority			= 0x00100000,
		// High priority. Major feature functionality etc. 
		HighPriority				= 0x00200000,
		// Mask for High on SetMinimumPriority
		HighPriorityAndAbove		= CriticalPriority | HighPriority,
		// Medium Priority. Minor feature functionality, major generic content issues.
		MediumPriority				= 0x00400000,
		// Mask for Medium on SetMinimumPriority
		MediumPriorityAndAbove		= CriticalPriority | HighPriority | MediumPriority,
		// Low Priority. Minor content bugs. String errors. Etc.
		LowPriority					= 0x00800000,
		PriorityMask = CriticalPriority | HighPriority | MediumPriority | LowPriority,

		//~ Speed of the test
		//Super Fast Filter
		SmokeFilter					= 0x01000000,
		//Engine Level Test
		EngineFilter				= 0x02000000,
		//Product Level Test
		ProductFilter				= 0x04000000,
		//Performance Test
		PerfFilter					= 0x08000000,
		//Stress Test
		StressFilter				= 0x10000000,
		//Negative Test. For tests whose correct expected outcome is failure.
		NegativeFilter				= 0x20000000,
		FilterMask = SmokeFilter | EngineFilter | ProductFilter | PerfFilter | StressFilter | NegativeFilter
	};
};

/** Flags for indicating the matching type to use for an expected error */
namespace EAutomationExpectedErrorFlags
{
	enum MatchType
	{
		// When matching expected errors, do so exactly.
		Exact,
		// When matching expected errors, just see if the error string is contained in the string to be evaluated.
		Contains,
	};

	inline const TCHAR* ToString(EAutomationExpectedErrorFlags::MatchType ThisType)
	{
		switch (ThisType)
		{
		case Contains:
			return TEXT("Contains");
		case Exact:
			return TEXT("Exact");
		}
		return TEXT("Unknown");
	}
}

/** Simple class to store the results of the execution of a automation test */
class CORE_API FAutomationTestExecutionInfo
{
public:
	/** Constructor */
	FAutomationTestExecutionInfo() 
		: bSuccessful( false )
		, Duration(0.0f)
		, Errors(0)
		, Warnings(0)
	{}

	/** Destructor */
	~FAutomationTestExecutionInfo()
	{
		Clear();
	}

	/** Helper method to clear out the results from a previous execution */
	void Clear();

	int32 RemoveAllEvents(EAutomationEventType EventType);

	int32 RemoveAllEvents(TFunctionRef<bool(FAutomationEvent&)> FilterPredicate);

	/** Any errors that occurred during execution */
	const TArray<FAutomationEvent>& GetEvents() const { return Events; }

	void AddEvent(const FAutomationEvent& Event);

	void AddWarning(const FString& WarningMessage);
	void AddError(const FString& ErrorMessage);

	int32 GetWarningTotal() const { return Warnings; }
	int32 GetErrorTotal() const { return Errors; }

	const FString& GetContext() const
	{
		static FString EmptyContext;
		return ContextStack.Num() ? ContextStack.Top() : EmptyContext;
	}

	void PushContext(const FString& Context)
	{
		ContextStack.Push(Context);
	}

	void PopContext()
	{
		if ( ContextStack.Num() > 0 )
		{
			ContextStack.Pop();
		}
	}

public:

	/** Whether the automation test completed successfully or not */
	bool bSuccessful;
	
	/** Any analytics items that occurred during execution */
	TArray<FString> AnalyticsItems;

	/** Time to complete the task */
	float Duration;

private:
	/** Any errors that occurred during execution */
	TArray<FAutomationEvent> Events;

	int32 Errors;
	int32 Warnings;

	TArray<FString> ContextStack;
};

/** Simple class to store the automation test info */
class CORE_API FAutomationTestInfo
{
public:

	// Default constructor
	FAutomationTestInfo( )
		: TestFlags( 0 )
		, NumParticipantsRequired( 0 )
		, NumDevicesCurrentlyRunningTest( 0 )
	{}

	/**
	 * Constructor
	 *
	 * @param	InDisplayName - Name used in the UI
	 * @param	InTestName - The test command string
	 * @param	InTestFlag - Test flags
	 * @param	InParameterName - optional parameter. e.g. asset name
	 */
	FAutomationTestInfo(const FString& InDisplayName, const FString& InFullTestPath, const FString& InTestName, const uint32 InTestFlags, const int32 InNumParticipantsRequired, const FString& InParameterName = FString(), const FString& InSourceFile = FString(), int32 InSourceFileLine = 0, const FString& InAssetPath = FString(), const FString& InOpenCommand = FString())
		: DisplayName( InDisplayName )
		, FullTestPath( InFullTestPath )
		, TestName( InTestName )
		, TestParameter( InParameterName )
		, SourceFile( InSourceFile )
		, SourceFileLine( InSourceFileLine )
		, AssetPath( InAssetPath )
		, OpenCommand( InOpenCommand )
		, TestFlags( InTestFlags )
		, NumParticipantsRequired( InNumParticipantsRequired )
		, NumDevicesCurrentlyRunningTest( 0 )
	{}

public:

	/**
	 * Add a test flag if a parent node.
	 *
	 * @Param InTestFlags - the child test flag to add.
	 */
	void AddTestFlags( const uint8 InTestFlags)
	{
		TestFlags |= InTestFlags;
	}

	/**
	 * Get the display name of this test.
	 *
	 * @return the display name.
	 */
	const FString& GetDisplayName() const
	{
		return DisplayName;
	}

	/**
	 * Gets the full path for this test if you wanted to run it.
	 *
	 * @return the display name.
	 */
	const FString& GetFullTestPath() const
	{
		return FullTestPath;
	}

	/**
	 * Get the test name of this test.
	 *
	 * @return The test name.
	 */
	FString GetTestName() const
	{
		return TestName;
	}

	/**
	 * Get the type of parameter. This will be the asset name for linked assets.
	 *
	 * @return the parameter.
	 */
	const FString GetTestParameter() const
	{
		return TestParameter;
	}

	/**
	 * Get the source file this test originated in.
	 *
	 * @return the source file.
	 */
	const FString GetSourceFile() const
	{
		return SourceFile;
	}

	/**
	 * Get the line number in the source file this test originated on.
	 *
	 * @return the source line number.
	 */
	const int32 GetSourceFileLine() const
	{
		return SourceFileLine;
	}

	/**
	 * Gets the asset potentially associated with the test.
	 *
	 * @return the source line number.
	 */
	const FString GetAssetPath() const
	{
		return AssetPath;
	}

	/**
	 * Gets the open command potentially associated with the test.
	 *
	 * @return the source line number.
	 */
	const FString GetOpenCommand() const
	{
		return OpenCommand;
	}

	/**
	 * Get the type of test.
	 *
	 * @return the test type.
	 */
	const uint32 GetTestFlags() const
	{
		return TestFlags;
	}
	
	/**
	 * Zero the number of devices running this test
	 */
	void ResetNumDevicesRunningTest()
	{
		NumDevicesCurrentlyRunningTest = 0;
	}
	
	/**
	 * Be notified of a new device running the test so we should update our flag counting these
	 */
	void InformOfNewDeviceRunningTest()
	{
		NumDevicesCurrentlyRunningTest++;
	}
	
	/**
	 * Get the number of devices running this test
	 *
	 * @return The number of devices which have been given this test to run
	 */
	const int GetNumDevicesRunningTest() const
	{
		return NumDevicesCurrentlyRunningTest;
	}

	/**
	 * Get the number of participant this test needs in order to be run
	 *
	 * @return The number of participants needed
	 */
	const int32 GetNumParticipantsRequired() const
	{
		return NumParticipantsRequired;
	}


	/**
	 * Set the display name of the child node.
	 *
	 * @Param InDisplayName - the new child test name.
	 */
	void SetDisplayName( const FString& InDisplayName )
	{
		DisplayName = InDisplayName;
	}

	/**
	 * Set the number of participant this test needs in order to be run
	 *
	 * @Param NumRequired - The new number of participants needed
	 */
	void SetNumParticipantsRequired( int32 NumRequired )
	{
		NumParticipantsRequired = NumRequired;
	}

private:
	/** Display name used in the UI */
	FString DisplayName; 

	FString FullTestPath;

	/** Test name used to run the test */
	FString TestName;

	/** Parameter - e.g. an asset name or map name */
	FString TestParameter;

	/** The source file this test originated in. */
	FString SourceFile;

	/** The line number in the source file this test originated on. */
	int32 SourceFileLine;

	/** The asset path associated with the test. */
	FString AssetPath;

	/** A custom open command for the test. */
	FString OpenCommand;

	/** The test flags. */
	uint32 TestFlags;

	/** The number of participants this test requires */
	uint32 NumParticipantsRequired;

	/** The number of devices which have been given this test to run */
	uint32 NumDevicesCurrentlyRunningTest;
};


/**
 * Simple abstract base class for creating time deferred of a single test that need to be run sequentially (Loadmap & Wait, Open Editor & Wait, then execute...)
 */
class IAutomationLatentCommand : public TSharedFromThis<IAutomationLatentCommand>
{
public:
	/* virtual destructor */
	virtual ~IAutomationLatentCommand() {};

	/**
	 * Updates the current command and will only return TRUE when it has fulfilled its role (Load map has completed and wait time has expired)
	 */
	virtual bool Update() = 0;

private:
	/**
	 * Private update that allows for use of "StartTime"
	 */
	bool InternalUpdate()
	{
		if (StartTime == 0.0)
		{
			StartTime = FPlatformTime::Seconds();
		}
		return Update();
	}

protected:
	/** Default constructor*/
	IAutomationLatentCommand()
		: StartTime(0.0f)
	{
	}

	/** For timers, track the first time this ticks */
	double StartTime;

	friend class FAutomationTestFramework;
};

/**
 * A simple latent command that runs the provided function on another thread
 */
class FThreadedAutomationLatentCommand : public IAutomationLatentCommand
{
public:

	virtual ~FThreadedAutomationLatentCommand() {};

	virtual bool Update() override
	{
		if (!Future.IsValid())
		{
			Future = Async(EAsyncExecution::Thread, Function);
		}

		return Future.IsReady();
	}

	FThreadedAutomationLatentCommand(TFunction<void()> InFunction)
		: Function(InFunction)
	{ }

protected:

	const TFunction<void()> Function;

	TFuture<void> Future;

	friend class FAutomationTestFramework;
};


/**
 * Simple abstract base class for networked, multi-participant tests
 */
class IAutomationNetworkCommand : public TSharedFromThis<IAutomationNetworkCommand>
{
public:
	/* virtual destructor */
	virtual ~IAutomationNetworkCommand() {};

	/** 
	 * Identifier to distinguish which worker on the network should execute this command 
	 * 
	 * The index of the worker that should execute this command
	 */
	virtual uint32 GetRoleIndex() const = 0;
	
	/** Runs the network command */
	virtual void Run() = 0;
};

struct FAutomationExpectedError
{
	// Original regular expression pattern string matching expected error message.
	// NOTE: using the Exact comparison type wraps the pattern string with ^ and $ tokens,
	// but the base pattern string is preserved to allow checks for duplicate entries.
	FString ErrorPatternString;
	// Regular expression pattern for ErrorPatternString
	FRegexPattern ErrorPattern;
	// Type of comparison to perform on error message using ErrorPattern.
	EAutomationExpectedErrorFlags::MatchType CompareType;
	/** 
	 * Number of occurrences expected for error. If set greater than 0, it will cause the test to fail if the
	 * exact number of occurrences expected is not matched. If set to 0, it will suppress all matching messages. 
	 */
	int32 ExpectedNumberOfOccurrences;
	int32 ActualNumberOfOccurrences;

	/**
	* Constructor
	*/
	
	FAutomationExpectedError(FString& InErrorPattern, EAutomationExpectedErrorFlags::MatchType InCompareType, int32 InExpectedNumberOfOccurrences = 1)
		: ErrorPatternString(InErrorPattern)
		, ErrorPattern((InCompareType == EAutomationExpectedErrorFlags::Exact) ? FString::Printf(TEXT("^%s$"), *InErrorPattern) : InErrorPattern)
		, CompareType(InCompareType)
		, ExpectedNumberOfOccurrences(InExpectedNumberOfOccurrences)
		, ActualNumberOfOccurrences(0)
	{}

	FAutomationExpectedError(FString& InErrorPattern, int32 InExpectedNumberOfOccurrences)
		: ErrorPatternString(InErrorPattern)
		, ErrorPattern(InErrorPattern)
		, CompareType(EAutomationExpectedErrorFlags::Contains)
		, ExpectedNumberOfOccurrences(InExpectedNumberOfOccurrences)
	{}
};

struct FAutomationScreenshotData
{
	FString Name;
	FString Context;

	FGuid Id;
	FString Commit;

	int32 Width;
	int32 Height;

	// RHI Details
	FString Platform;
	FString Rhi;
	FString FeatureLevel;
	bool bIsStereo;

	// Hardware Details
	FString Vendor;
	FString AdapterName;
	FString AdapterInternalDriverVersion;
	FString AdapterUserDriverVersion;
	FString UniqueDeviceId;

	// Quality Levels
	float ResolutionQuality;
	int32 ViewDistanceQuality;
	int32 AntiAliasingQuality;
	int32 ShadowQuality;
	int32 PostProcessQuality;
	int32 TextureQuality;
	int32 EffectsQuality;
	int32 FoliageQuality;

	// Comparison Requests
	bool bHasComparisonRules;
	uint8 ToleranceRed;
	uint8 ToleranceGreen;
	uint8 ToleranceBlue;
	uint8 ToleranceAlpha;
	uint8 ToleranceMinBrightness;
	uint8 ToleranceMaxBrightness;
	float MaximumLocalError;
	float MaximumGlobalError;
	bool bIgnoreAntiAliasing;
	bool bIgnoreColors;

	FString Path;

	FAutomationScreenshotData()
		: Id()
		, Commit()
		, Width(0)
		, Height(0)
		, bIsStereo(false)
		, ResolutionQuality(1.0f)
		, ViewDistanceQuality(0)
		, AntiAliasingQuality(0)
		, ShadowQuality(0)
		, PostProcessQuality(0)
		, TextureQuality(0)
		, EffectsQuality(0)
		, FoliageQuality(0)
		, bHasComparisonRules(false)
		, ToleranceRed(0)
		, ToleranceGreen(0)
		, ToleranceBlue(0)
		, ToleranceAlpha(0)
		, ToleranceMinBrightness(0)
		, ToleranceMaxBrightness(255)
		, MaximumLocalError(0.0f)
		, MaximumGlobalError(0.0f)
		, bIgnoreAntiAliasing(false)
		, bIgnoreColors(false)
	{
	}
};



/**
 * Delegate type for when a test screenshot has been captured
 *
 * The first parameter is the array of the raw color data.
 * The second parameter is the image metadata.
 */
DECLARE_DELEGATE_TwoParams(FOnTestScreenshotCaptured, const TArray<FColor>&, const FAutomationScreenshotData&);

DECLARE_MULTICAST_DELEGATE_FiveParams(FOnTestScreenshotComparisonComplete, bool /*bWasNew*/, bool /*bWasSimilar*/, double /*MaxLocalDifference*/, double /*GlobalDifference*/, FString /*ErrorMessage*/);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTestDataRetrieved, bool /*bWasNew*/, const FString& /*JsonData*/);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPerformanceDataRetrieved, bool /*bSuccess*/, const FString& /*ErrorMessage*/);

/** Class representing the main framework for running automation tests */
class CORE_API FAutomationTestFramework
{
public:
	/** Called right before unit testing is about to begin */
	FSimpleMulticastDelegate PreTestingEvent;
	
	/** Called after all unit tests have completed */
	FSimpleMulticastDelegate PostTestingEvent;

	/** Called when a screenshot comparison completes. */
	FOnTestScreenshotComparisonComplete OnScreenshotCompared;

	/** Called when the test data is retrieved. */
	FOnTestDataRetrieved OnTestDataRetrieved;

	/** Called when the performance data is retrieved. */
	FOnPerformanceDataRetrieved OnPerformanceDataRetrieved;

	/** The final call related to screenshots, after they've been taken, and after they've been compared (or not if automation isn't running). */
	FSimpleMulticastDelegate OnScreenshotTakenAndCompared;

	/**
	 * Return the singleton instance of the framework.
	 *
	 * @return The singleton instance of the framework.
	 */
	static FAutomationTestFramework& Get();
	static FAutomationTestFramework& GetInstance() { return Get(); }

	/**
	 * Gets a scratch space location outside of the project and saved directories.  When an automation test needs
	 * to do something like generate project files, or create new projects it should use this directory, rather
	 * than pollute other areas of the machine.
	 */
	FString GetUserAutomationDirectory() const;

	/**
	 * Register a automation test into the framework. The automation test may or may not be necessarily valid
	 * for the particular application configuration, but that will be determined when tests are attempted
	 * to be run.
	 *
	 * @param	InTestNameToRegister	Name of the test being registered
	 * @param	InTestToRegister		Actual test to register
	 *
	 * @return	true if the test was successfully registered; false if a test was already registered under the same
	 *			name as before
	 */
	bool RegisterAutomationTest( const FString& InTestNameToRegister, class FAutomationTestBase* InTestToRegister );

	/**
	 * Unregister a automation test with the provided name from the framework.
	 *
	 * @return true if the test was successfully unregistered; false if a test with that name was not found in the framework.
	 */
	bool UnregisterAutomationTest( const FString& InTestNameToUnregister );

	/**
	 * Enqueues a latent command for execution on a subsequent frame
	 *
	 * @param NewCommand - The new command to enqueue for deferred execution
	 */
	void EnqueueLatentCommand(TSharedPtr<IAutomationLatentCommand> NewCommand);

	/**
	 * Enqueues a network command for execution in accordance with this workers role
	 *
	 * @param NewCommand - The new command to enqueue for network execution
	 */
	void EnqueueNetworkCommand(TSharedPtr<IAutomationNetworkCommand> NewCommand);

	/**
	 * Checks if a provided test is contained within the framework.
	 *
	 * @param InTestName	Name of the test to check
	 *
	 * @return	true if the provided test is within the framework; false otherwise
	 */
	bool ContainsTest( const FString& InTestName ) const;
		
	/**
	 * Attempt to run all fast smoke tests that are valid for the current application configuration.
	 *
	 * @return	true if all smoke tests run were successful, false if any failed
	 */
	bool RunSmokeTests();

	/**
	 * Reset status of worker (delete local files, etc)
	 */
	void ResetTests();

	/**
	 * Attempt to start the specified test.
	 *
	 * @param	InTestToRun			Name of the test that should be run
	 * @param	InRoleIndex			Identifier for which worker in this group that should execute a command
	 */
	void StartTestByName( const FString& InTestToRun, const int32 InRoleIndex );

	/**
	 * Stop the current test and return the results of execution
	 *
	 * @return	true if the test ran successfully, false if it did not (or the test could not be found/was invalid)
	 */
	bool StopTest( FAutomationTestExecutionInfo& OutExecutionInfo );

	/**
	 * Execute all latent functions that complete during update
	 *
	 * @return - true if the latent command queue is now empty and the test is complete
	 */
	bool ExecuteLatentCommands();

	/**
	 * Execute the next network command if you match the role, otherwise just dequeue
	 *
	 * @return - true if any network commands were in the queue to give subsequent latent commands a chance to execute next frame
	 */
	bool ExecuteNetworkCommands();

	/**
	 * Load any modules that are not loaded by default and have test classes in them
	 */
	void LoadTestModules();

	/**
	 * Populates the provided array with the names of all tests in the framework that are valid to run for the current
	 * application settings.
	 *
	 * @param	TestInfo	Array to populate with the test information
	 */
	void GetValidTestNames( TArray<FAutomationTestInfo>& TestInfo ) const;

	/**
	 * Whether the testing framework should allow content to be tested or not.  Intended to block developer directories.
	 * @param Path - Full path to the content in question
	 * @return - Whether this content should have tests performed on it
	 */
	bool ShouldTestContent(const FString& Path) const;

	/**
	 * Sets whether we want to include content in developer directories in automation testing
	 */
	void SetDeveloperDirectoryIncluded(const bool bInDeveloperDirectoryIncluded);

	/**
	* Sets which set of tests to pull from.
	*/
	void SetRequestedTestFilter(const uint32 InRequestedTestFlags);
	

	/**
	 * Accessor for delegate called when a png screenshot is captured 
	 */
	FOnTestScreenshotCaptured& OnScreenshotCaptured();

	/**
	 * Sets forcing smoke tests.
	 */
	void SetForceSmokeTests(const bool bInForceSmokeTests)
	{
		bForceSmokeTests = bInForceSmokeTests;
	}

	bool GetCaptureStack() const
	{
		return bCaptureStack;
	}

	void SetCaptureStack(bool bCapture)
	{
		bCaptureStack = bCapture;
	}

	/**
	 * Adds a analytics string to the current test to be parsed later.  Must be called only when an automation test is in progress
	 *
	 * @param	AnalyticsItem	Log item to add to the current test
	 */
	void AddAnalyticsItemToCurrentTest( const FString& AnalyticsItem );

	/**
	 * Returns the actively executing test or null if there isn't one
	 */
	FAutomationTestBase* GetCurrentTest() const
	{
		return CurrentTest;
	}

	bool GetTreatWarningsAsErrors() const;
	void SetTreatWarningsAsErrors(TOptional<bool> bTreatWarningsAsErrors);

	void NotifyScreenshotComparisonComplete(bool bWasNew, bool bWasSimilar, double MaxLocalDifference, double GlobalDifference, FString ErrorMessage);
	void NotifyTestDataRetrieved(bool bWasNew, const FString& JsonData);
	void NotifyPerformanceDataRetrieved(bool bSuccess, const FString& ErrorMessage);

	void NotifyScreenshotTakenAndCompared();

private:

	/** Special feedback context used exclusively while automation testing */
	 class FAutomationTestFeedbackContext : public FFeedbackContext
	{
	public:

		/** Constructor */
		FAutomationTestFeedbackContext() 
			: CurTest( NULL ) {}

		/** Destructor */
		~FAutomationTestFeedbackContext()
		{
			CurTest = NULL;
		}

		/**
		 * FOutputDevice interface
		 *
		 * @param	V		String to serialize within the context
		 * @param	Event	Event associated with the string
		 */
		virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override;

		/**
		 * Set the automation test associated with the feedback context. The automation test is where all warnings, errors, etc.
		 * will be routed to.
		 *
		 * @param	InAutomationTest	Automation test to associate with the feedback context.
		 */
		void SetCurrentAutomationTest( class FAutomationTestBase* InAutomationTest )
		{
			CurTest = InAutomationTest;
		}

	private:

		/** Associated automation test; all warnings, errors, etc. are routed to the automation test to track */
		class FAutomationTestBase* CurTest;
	};

	 friend class FAutomationTestFeedbackContext;
	/** Helper method called to prepare settings for automation testing to follow */
	void PrepForAutomationTests();

	/** Helper method called after automation testing is complete to restore settings to how they should be */
	void ConcludeAutomationTests();

	/**
	 * Helper method to dump the contents of the provided test name to execution info map to the provided feedback context
	 *
	 * @param	InContext		Context to dump the execution info to
	 * @param	InInfoToDump	Execution info that should be dumped to the provided feedback context
	 */
	void DumpAutomationTestExecutionInfo( const TMap<FString, FAutomationTestExecutionInfo>& InInfoToDump );

	/**
	 * Internal helper method designed to simply start the provided test name.
	 *
	 * @param	InTestToRun			Name of the test that should be run
	 * @param	OutExecutionInfo	Results of executing the test
	 */
	void InternalStartTest( const FString& InTestToRun );

	/**
	 * Internal helper method designed to stop current executing test and return the results of execution.
	 *
	 * @return	true if the test was successfully run; false if it was not, could not be found, or is invalid for
	 *			the current application settings
	 */
	bool InternalStopTest(FAutomationTestExecutionInfo& OutExecutionInfo);


	/** Constructor */
	FAutomationTestFramework();

	/** Destructor */
	~FAutomationTestFramework();

	// Copy constructor and assignment operator intentionally left unimplemented
	FAutomationTestFramework( const FAutomationTestFramework& );
	FAutomationTestFramework& operator=( const FAutomationTestFramework& );

	/** Specialized feedback context used for automation testing */
	FAutomationTestFeedbackContext AutomationTestFeedbackContext;

	/** Mapping of automation test names to their respective object instances */
	TMap<FString, class FAutomationTestBase*> AutomationTestClassNameToInstanceMap;

	/** Queue of deferred commands */
	TQueue< TSharedPtr<IAutomationLatentCommand> > LatentCommands;

	/** Queue of deferred commands */
	TQueue< TSharedPtr<IAutomationNetworkCommand> > NetworkCommands;

	/** Whether we are currently executing smoke tests for startup/commandlet to minimize log spam */
	uint32 RequestedTestFilter;

	/** Time when the test began executing */
	double StartTime;

	/** True if the execution of the test (but possibly not the latent actions) were successful */
	bool bTestSuccessful;

	/** Pointer to the current test being run */
	FAutomationTestBase* CurrentTest;

	/** Copy of the parameters for the active test */
	FString Parameters;

	/** Whether we want to run automation tests on content within the Developer Directories */
	bool bDeveloperDirectoryIncluded;

	/** Participation role as given by the automation controller */
	uint32 NetworkRoleIndex;

	/** Delegate called at the end of the frame when a screenshot is captured and a .png is requested */
	FOnTestScreenshotCaptured TestScreenshotCapturedDelegate;

	/** Forces running smoke tests */
	bool bForceSmokeTests;

	bool bCaptureStack;
};


/** Simple abstract base class for all automation tests */
class CORE_API FAutomationTestBase
{
public:
	/**
	 * Constructor
	 *
	 * @param	InName	Name of the test
	 */
	FAutomationTestBase( const FString& InName, const bool bInComplexTask )
		: bComplexTask( bInComplexTask )
		, bSuppressLogs( false )
		, TestName( InName )
	{
		// Register the newly created automation test into the automation testing framework
		FAutomationTestFramework::Get().RegisterAutomationTest( InName, this );
	}

	/** Destructor */
	virtual ~FAutomationTestBase() 
	{ 
		// Unregister the automation test from the automation testing framework
		FAutomationTestFramework::Get().UnregisterAutomationTest( TestName );
	}

	/**
	 * Pure virtual method; returns the flags associated with the given automation test
	 *
	 * @return	Automation test flags associated with the test
	 */
	virtual uint32 GetTestFlags() const = 0;

	/**
	 * Pure virtual method; returns the number of participants for this test
	 *
	 * @return	Number of required participants
	 */
	virtual uint32 GetRequiredDeviceNum() const = 0;

	/** Clear any execution info/results from a prior running of this test */
	void ClearExecutionInfo();

	/**
	 * Adds an error message to this test
	 *
	 * @param	InError	Error message to add to this test
	 */
	virtual void AddError( const FString& InError, int32 StackOffset = 0 );

	/**
	 * Adds an error message to this test
	 *
	 * @param	InError	Error message to add to this test
	 * @param	InFilename	The filename the error originated in
	 * @param	InLineNumber	The line number in the file this error originated in
	 */
	virtual void AddError(const FString& InError, const FString& InFilename, int32 InLineNumber);

	/**
	 * Adds an warning message to this test
	 *
	 * @param	InWarning	Warning message to add to this test
	 * @param	InFilename	The filename the error originated in
	 * @param	InLineNumber	The line number in the file this error originated in
	 */
	virtual void AddWarning(const FString& InWarning, const FString& InFilename, int32 InLineNumber);

	/**
	 * Adds a warning to this test
	 *
	 * @param	InWarning	Warning message to add to this test
	 */
	virtual void AddWarning( const FString& InWarning, int32 StackOffset = 0);

	DEPRECATED(4.16, "Use AddInfo")
	FORCEINLINE void AddLogItem(const FString& InLogItem)
	{
		AddInfo(InLogItem, 0);
	}

	/**
	 * Adds a log item to this test
	 *
	 * @param	InLogItem	Log item to add to this test
	 */
	virtual void AddInfo( const FString& InLogItem, int32 StackOffset = 0);

	/**
	 * Adds a analytics string to parse later
	 *
	 * @param	InLogItem	Log item to add to this test
	 */
	virtual void AddAnalyticsItem(const FString& InAnalyticsItem);

	/**
	 * Returns whether this test has any errors associated with it or not
	 *
	 * @return true if this test has at least one error associated with it; false if not
	 */
	bool HasAnyErrors() const;

	/**
	* Returns whether this test has encountered all expected errors defined for it
	*
	* @return true if this test has encountered all expected errors; false if not
	*/
	bool HasMetExpectedErrors();

	/**
	 * Forcibly sets whether the test has succeeded or not
	 *
	 * @param	bSuccessful	true to mark the test successful, false to mark the test as failed
	 */
	void SetSuccessState( bool bSuccessful );

	/**
	 * Populate the provided execution info object with the execution info contained within the test. Not particularly efficient,
	 * but providing direct access to the test's private execution info could result in errors.
	 *
	 * @param	OutInfo	Execution info to be populated with the same data contained within this test's execution info
	 */
	void GetExecutionInfo( FAutomationTestExecutionInfo& OutInfo ) const;

	/** 
	 * Helper function that will generate a list of sub-tests via GetTests
	 */
	void GenerateTestNames( TArray<FAutomationTestInfo>& TestInfo ) const;

	/**
	* Adds a regex pattern to an internal list that this test will expect to encounter in error or warning logs during its execution. If an expected pattern
	* is not encountered, it will cause this test to fail.
	*
	* @param ExpectedPatternString - The expected message string. Supports basic regex patterns.
	* @param CompareType - How to match this string with an encountered error, should it match exactly or simply just contain the string.
	* @param Occurrences - How many times to expect this error string to be seen. If > 0, the error must be seen the exact number of times
	* specified or the test will fail. If == 0, the error must be seen one or more times (with no upper limit) or the test will fail.
	*/
	void AddExpectedError(FString ExpectedPatternString, EAutomationExpectedErrorFlags::MatchType CompareType = EAutomationExpectedErrorFlags::Contains, int32 Occurrences = 1);

	/**
	* Populate the provided expected errors object with the expected errors contained within the test. Not particularly efficient,
	* but providing direct access to the test's private execution errors list could result in errors.
	*
	* @param OutInfo - Array of Expected Errors to be populated with the same data contained within this test's expected errors list
	*/
	void GetExpectedErrors(TArray<FAutomationExpectedError>& OutInfo) const;

	/**
	 * Is this a complex tast - if so it will be a stress test.
	 *
	 * @return true if this is a complex task.
	 */
	const bool IsComplexTask() const
	{
		return bComplexTask;
	}

	const bool IsRanOnSeparateThread() const
	{
		return bRunOnSeparateThread;
	}

	/**
	 * Used to suppress / unsuppress logs.
	 *
	 * @param bNewValue - True if you want to suppress logs.  False to unsuppress.
	 */
	void SetSuppressLogs(bool bNewValue)
	{
		bSuppressLogs = bNewValue;
	}

	/**
	 * Enqueues a new latent command.
	 */
	FORCEINLINE void AddCommand(IAutomationLatentCommand* NewCommand)
	{
		TSharedRef<IAutomationLatentCommand> CommandPtr = MakeShareable(NewCommand);
		FAutomationTestFramework::Get().EnqueueLatentCommand(CommandPtr);
	}

	/**
	 * Enqueues a new latent network command.
	 */
	FORCEINLINE void AddCommand(IAutomationNetworkCommand* NewCommand)
	{
		TSharedRef<IAutomationNetworkCommand> CommandPtr = MakeShareable(NewCommand);
		FAutomationTestFramework::Get().EnqueueNetworkCommand(CommandPtr);
	}

	/** Gets the filename where this test was defined. */
	virtual FString GetTestSourceFileName() const { return TEXT(""); }

	/** Gets the line number where this test was defined. */
	virtual int32 GetTestSourceFileLine() const { return 0; }

	/** Gets the filename where this test was defined. */
	virtual FString GetTestSourceFileName(const FString& InTestName) const { return GetTestSourceFileName(); }

	/** Gets the line number where this test was defined. */
	virtual int32 GetTestSourceFileLine(const FString& InTestName) const { return GetTestSourceFileLine(); }

	/** Allows navigation to the asset associated with the test if there is one. */
	virtual FString GetTestAssetPath(const FString& Parameter) const { return TEXT(""); }

	/** Return an exec command to open the test associated with this parameter. */
	virtual FString GetTestOpenCommand(const FString& Parameter) const { return TEXT(""); }

	void PushContext(const FString& Context)
	{
		ExecutionInfo.PushContext(Context);
	}

	void PopContext()
	{
		ExecutionInfo.PopContext();
	}

public:

	void TestEqual(const FString& What, const int32 Actual, const int32 Expected);
	void TestEqual(const FString& What, const float Actual, const float Expected, float Tolerance = KINDA_SMALL_NUMBER);
	void TestEqual(const FString& What, const FVector Actual, const FVector Expected, float Tolerance = KINDA_SMALL_NUMBER);
	void TestEqual(const FString& What, const FColor Actual, const FColor Expected);

	/**
	 * Logs an error if the two values are not equal.
	 *
	 * @param Description - Description text for the test.
	 * @param A - The first value.
	 * @param B - The second value.
	 *
	 * @see TestNotEqual
	 */
	template<typename ValueType> 
	void TestEqual(const FString& Description, const ValueType& A, const ValueType& B)
	{
		if (A != B)
		{
			AddError(FString::Printf(TEXT("%s: The two values are not equal."), *Description), 1);
		}
	}

	void TestEqual(const FString& Description, const FString& A, const TCHAR* B)
	{
		if (A != B)
		{
			AddError(FString::Printf(TEXT("%s: The two values are not equal.\n  Actual: '%s'\nExpected: '%s'"), *Description, *A, B), 1);
		}
	}

	void TestEqual(const FString& Description, const TCHAR* A, const TCHAR* B)
	{
		if (A != B)
		{
			AddError(FString::Printf(TEXT("%s: The two values are not equal.\n  Actual: '%s'\nExpected: '%s'"), *Description, A, B), 1);
		}
	}

	void TestEqual(const FString& Description, const TCHAR* A, const FString& B)
	{
		if (A != B)
		{
			AddError(FString::Printf(TEXT("%s: The two values are not equal.\n  Actual: '%s'\nExpected: '%s'"), *Description, A, *B), 1);
		}
	}

	/**
	 * Logs an error if the specified Boolean value is not false.
	 *
	 * @param Description - Description text for the test.
	 * @param Value - The value to test.
	 *
	 * @see TestFalse
	 */
	void TestFalse(const FString& What, bool Value);

	/**
	 * Logs an error if the given shared pointer is valid.
	 *
	 * @param Description - Description text for the test.
	 * @param SharedPointer - The shared pointer to test.
	 *
	 * @see TestValid
	 */
	template<typename ValueType> void TestInvalid(const FString& Description, const TSharedPtr<ValueType>& SharedPointer)
	{
		if (SharedPointer.IsValid())
		{
			AddError(FString::Printf(TEXT("%s: The shared pointer is valid."), *Description), 1);
		}
	}

	/**
	 * Logs an error if the two values are equal.
	 *
	 * @param Description - Description text for the test.
	 * @param A - The first value.
	 * @param B - The second value.
	 *
	 * @see TestEqual
	 */
	template<typename ValueType> void TestNotEqual(const FString& Description, const ValueType& A, const ValueType& B)
	{
		if (A == B)
		{
			AddError(FString::Printf(TEXT("%s: The two values are not equal."), *Description), 1);
		}
	}

	/**
	 * Logs an error if the specified pointer is NULL.
	 *
	 * @param Description - Description text for the test.
	 * @param Pointer - The pointer to test.
	 *
	 * @see TestNull
	 */
	template<typename ValueType> void TestNotNull(const FString& What, ValueType* Pointer)
	{
		if (Pointer == nullptr)
		{
			AddError(FString::Printf(TEXT("Expected '%s' to be not null."), *What), 1);
		}
		else
		{
			AddInfo(FString::Printf(TEXT("Expected '%s' to be not null."), *What), 1);
		}
	}

	/**
	 * Logs an error if the two values are the same object in memory.
	 *
	 * @param Description - Description text for the test.
	 * @param A - The first value.
	 * @param B - The second value.
	 *
	 * @see TestSame
	 */
	template<typename ValueType> void TestNotSame(const FString& Description, const ValueType& A, const ValueType& B)
	{
		if (&A == &B)
		{
			AddError(FString::Printf(TEXT("%s: The two values are the same."), *Description), 1);
		}
	}

	/**
	 * Logs an error if the specified pointer is not NULL.
	 *
	 * @param Description - Description text for the test.
	 * @param Pointer - The pointer to test.
	 *
	 * @see TestNotNull
	 */
	void TestNull(const FString& What, const void* Pointer);

	/**
	 * Logs an error if the two values are not the same object in memory.
	 *
	 * @param Description - Description text for the test.
	 * @param A - The first value.
	 * @param B - The second value.
	 *
	 * @see TestNotSame
	 */
	template<typename ValueType> void TestSame(const FString& Description, const ValueType& A, const ValueType& B)
	{
		if (&A != &B)
		{
			AddError(FString::Printf(TEXT("%s: The two values are not the same."), *Description), 1);
		}
	}

	/**
	 * Logs an error if the specified Boolean value is not true.
	 *
	 * @param Description - Description text for the test.
	 * @param Value - The value to test.
	 *
	 * @see TestFalse
	 */
	void TestTrue(const FString& What, bool Value);

	/** Macro version of above, uses the passed in expression as the description as well */
	#define TestTrueExpr(Expression) TestTrue(TEXT(#Expression), Expression)

	/**
	 * Logs an error if the given shared pointer is not valid.
	 *
	 * @param Description - Description text for the test.
	 * @param SharedPointer - The shared pointer to test.
	 *
	 * @see TestInvalid
	 */
	template<typename ValueType> void TestValid(const FString& Description, const TSharedPtr<ValueType>& SharedPointer)
	{
		if (!SharedPointer.IsValid())
		{
			AddError(FString::Printf(TEXT("%s: The shared pointer is not valid."), *Description), 1);
		}
	}

protected:
	/**
	 * Asks the test to enumerate variants that will all go through the "RunTest" function with different parameters (for load all maps, this should enumerate all maps to load)\
	 *
	 * @param OutBeautifiedNames - Name of the test that can be displayed by the UI (for load all maps, it would be the map name without any directory prefix)
	 * @param OutTestCommands - The parameters to be specified to each call to RunTests (for load all maps, it would be the map name to load)
	 */
	virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const = 0;

	/**
	 * Virtual call to execute the automation test.  
	 *
	 * @param Parameters - Parameter list for the test (but it will be empty for simple tests)
	 * @return TRUE if the test was run successfully; FALSE otherwise
	 */
	virtual bool RunTest(const FString& Parameters)=0;

	/**
	 * Returns the beautified test name
	 */
	virtual FString GetBeautifiedTestName() const = 0;

protected:

	//Flag to indicate if this is a complex task
	bool bComplexTask;

	//Flag to indicate if this test should be ran on it's own thread
	bool bRunOnSeparateThread;

	/** Flag to suppress logs */
	bool bSuppressLogs;

	/** Name of the test */
	FString TestName;

	/** Info related to the last execution of this test */
	FAutomationTestExecutionInfo ExecutionInfo;

	//allow framework to call protected function
	friend class FAutomationTestFramework;

private:
	/**
	* Returns whether this test has defined any expected errors matching the given message.
	* If a match is found, the expected error definition increments it actual occurrence count.
	*
	* @return true if this message matches any of the expected errors
	*/
	bool IsExpectedError(const FString& Error);

	/* Errors to be expected while processing this test.*/
	TArray< FAutomationExpectedError> ExpectedErrors;

};

class CORE_API FBDDAutomationTestBase : public FAutomationTestBase
{ 
public:
	FBDDAutomationTestBase(const FString& InName, const bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask) 
		, bIsDiscoveryMode(false)
		, bBaseRunTestRan(false)
	{}

	virtual bool RunTest(const FString& Parameters) override
	{
		bBaseRunTestRan = true;
		TestIdToExecute = Parameters;

		return true;
	}

	virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const override
	{
		const_cast<FBDDAutomationTestBase*>(this)->BeautifiedNames.Empty();
		const_cast<FBDDAutomationTestBase*>(this)->TestCommands.Empty();

		bIsDiscoveryMode = true;
		const_cast<FBDDAutomationTestBase*>(this)->RunTest(FString());
		bIsDiscoveryMode = false;

		OutBeautifiedNames.Append(BeautifiedNames);
		OutTestCommands.Append(TestCommands);
		bBaseRunTestRan = false;
	}

	bool IsDiscoveryMode() const
	{
		return bIsDiscoveryMode;
	}

	void xDescribe(const FString& InDescription, TFunction<void()> DoWork)
	{
		check(bBaseRunTestRan);
		//disabled this suite
	}

	void Describe(const FString& InDescription, TFunction<void()> DoWork)
	{
		check(bBaseRunTestRan);

		PushDescription(InDescription);

		int32 OriginalBeforeEachCount = BeforeEachStack.Num();
		int32 OriginalAfterEachCount = AfterEachStack.Num();

		DoWork();

		check(OriginalBeforeEachCount <= BeforeEachStack.Num());
		if (OriginalBeforeEachCount != BeforeEachStack.Num())
		{
			BeforeEachStack.Pop();
		}

		check(OriginalAfterEachCount <= AfterEachStack.Num());
		if (OriginalAfterEachCount != AfterEachStack.Num())
		{
			AfterEachStack.Pop();
		}

		PopDescription(InDescription);
	}
	
	void xIt(const FString& InDescription, TFunction<void()> DoWork)
	{
		check(bBaseRunTestRan);
		//disabled this spec
	}

	void It(const FString& InDescription, TFunction<void()> DoWork)
	{
		check(bBaseRunTestRan);

		PushDescription(InDescription);

		if (bIsDiscoveryMode)
		{
			BeautifiedNames.Add(GetDescription());

			bIsDiscoveryMode = false;
			TestCommands.Add(GetDescription());
			bIsDiscoveryMode = true;
		}
		else if (TestIdToExecute.IsEmpty() || GetDescription() == TestIdToExecute)
		{
			for (int32 Index = 0; Index < BeforeEachStack.Num(); Index++)
			{
				BeforeEachStack[Index]();
			}

			DoWork();

			for (int32 Index = AfterEachStack.Num() - 1; Index >= 0; Index--)
			{
				AfterEachStack[Index]();
			}
		}

		PopDescription(InDescription);
	}

	void BeforeEach(TFunction<void()> DoWork)
	{
		BeforeEachStack.Push(DoWork);
	}

	void AfterEach(TFunction<void()> DoWork)
	{
		AfterEachStack.Push(DoWork);
	}

private:

	void PushDescription(const FString& InDescription)
	{
		Description.Add(InDescription);
	}

	void PopDescription(const FString& InDescription)
	{
		Description.RemoveAt(Description.Num() - 1);
	}

	FString GetDescription() const
	{
		FString CompleteDescription;
		for (int32 Index = 0; Index < Description.Num(); ++Index)
		{
			if (Description[Index].IsEmpty())
			{
				continue;
			}

			if (CompleteDescription.IsEmpty())
			{
				CompleteDescription = Description[Index];
			}
			else if (FChar::IsWhitespace(CompleteDescription[CompleteDescription.Len() - 1]) || FChar::IsWhitespace(Description[Index][0]))
			{
				if (bIsDiscoveryMode)
				{
					CompleteDescription = CompleteDescription + TEXT(".") + Description[Index];
				}
				else
				{
					CompleteDescription = CompleteDescription + Description[Index];
				}
			}
			else
			{
				if (bIsDiscoveryMode)
				{
					CompleteDescription = FString::Printf(TEXT("%s.%s"), *CompleteDescription, *Description[Index]);
				}
				else
				{
					CompleteDescription = FString::Printf(TEXT("%s %s"), *CompleteDescription, *Description[Index]);
				}
			}
		}

		return CompleteDescription;
	}

private:

	FString TestIdToExecute;
	TArray<FString> Description;
	TArray<TFunction<void()>> BeforeEachStack;
	TArray<TFunction<void()>> AfterEachStack;

	TArray<FString> BeautifiedNames;
	TArray<FString> TestCommands;
	mutable bool bIsDiscoveryMode;
	mutable bool bBaseRunTestRan;
};

DECLARE_DELEGATE(FDoneDelegate);

class CORE_API FAutomationSpecBase : public FAutomationTestBase
{
private:

	class FSingleExecuteLatentCommand : public IAutomationLatentCommand
	{
	public:
		FSingleExecuteLatentCommand(TFunction<void()> InPredicate)
			: Predicate(MoveTemp(InPredicate))
		{ }

		virtual ~FSingleExecuteLatentCommand()
		{ }

		virtual bool Update() override
		{
			Predicate();
			return true;
		}

	private:

		const TFunction<void()> Predicate;
	};

	class FUntilDoneLatentCommand : public IAutomationLatentCommand
	{
	public:

		FUntilDoneLatentCommand(TFunction<void(const FDoneDelegate&)> InPredicate)
			: Predicate(MoveTemp(InPredicate))
			, bIsRunning(false)
			, bDone(false)
		{ }

		virtual ~FUntilDoneLatentCommand()
		{ }

		virtual bool Update() override
		{
			if (!bIsRunning)
			{
				Predicate(FDoneDelegate::CreateSP(this, &FUntilDoneLatentCommand::Done));
				bIsRunning = true;
			}

			if (bDone)
			{
				// Reset the done for the next potential run of this command
				bDone = false;
				bIsRunning = false;
				return true;
			}

			return false;
		}

	private:

		void Done()
		{
			bDone = true;
		}

	private:

		const TFunction<void(const FDoneDelegate&)> Predicate;

		bool bIsRunning;
		FThreadSafeBool bDone;
	};

	class FAsyncUntilDoneLatentCommand : public IAutomationLatentCommand
	{
	public:

		FAsyncUntilDoneLatentCommand(EAsyncExecution InExecution, TFunction<void(const FDoneDelegate&)> InPredicate)
			: Execution(InExecution)
			, Predicate(MoveTemp(InPredicate))
			, bDone(false)
		{ }

		virtual ~FAsyncUntilDoneLatentCommand()
		{ }

		virtual bool Update() override
		{
			if (!Future.IsValid())
			{
				Future = Async<void>(Execution, [this]() {
					Predicate(FDoneDelegate::CreateRaw(this, &FAsyncUntilDoneLatentCommand::Done));
				});
			}

			if (bDone)
			{
				// Reset the done for the next potential run of this command
				bDone = false;
				Future = TFuture<void>();
				return true;
			}

			return false;
		}

	private:

		void Done()
		{
			bDone = true;
		}

	private:

		const EAsyncExecution Execution;
		const TFunction<void(const FDoneDelegate&)> Predicate;

		TFuture<void> Future;
		FThreadSafeBool bDone;
	};

	class FAsyncLatentCommand : public IAutomationLatentCommand
	{
	public:

		FAsyncLatentCommand(EAsyncExecution InExecution, TFunction<void()> InPredicate)
			: Execution(InExecution)
			, Predicate(MoveTemp(InPredicate))
			, bDone(false)
		{ }

		virtual ~FAsyncLatentCommand()
		{ }

		virtual bool Update() override
		{
			if (!Future.IsValid())
			{
				Future = Async<void>(Execution, [this]() {
					Predicate();
					bDone = true;
				});
			}

			if (bDone)
			{
				// Reset the done for the next potential run of this command
				bDone = false;
				Future = TFuture<void>();
				return true;
			}

			return false;
		}

	private:

		void Done()
		{
			bDone = true;
		}

	private:

		const EAsyncExecution Execution;
		const TFunction<void()> Predicate;

		TFuture<void> Future;
		FThreadSafeBool bDone;
	};

	struct FSpecIt
	{
	public:

		FString Description;
		FString Id;
		FString Filename;
		int32 LineNumber;
		TSharedRef<IAutomationLatentCommand> Command;

		FSpecIt(FString InDescription, FString InId, FString InFilename, int32 InLineNumber, TSharedRef<IAutomationLatentCommand> InCommand)
			: Description(MoveTemp(InDescription))
			, Id(MoveTemp(InId))
			, Filename(InFilename)
			, LineNumber(MoveTemp(InLineNumber))
			, Command(MoveTemp(InCommand))
		{ }
	};

	struct FSpecDefinitionScope
	{
	public:

		FString Description;

		TArray<TSharedRef<IAutomationLatentCommand>> BeforeEach;
		TArray<TSharedRef<FSpecIt>> It;
		TArray<TSharedRef<IAutomationLatentCommand>> AfterEach;

		TArray<TSharedRef<FSpecDefinitionScope>> Children;
	};

	struct FSpec
	{
	public:

		FString Id;
		FString Description;
		FString Filename;
		int32 LineNumber;
		TArray<TSharedRef<IAutomationLatentCommand>> Commands;
	};

public:

	FAutomationSpecBase(const FString& InName, const bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask)
		, RootDefinitionScope(MakeShareable(new FSpecDefinitionScope()))
	{
		DefinitionScopeStack.Push(RootDefinitionScope.ToSharedRef());
	}

	virtual bool RunTest(const FString& InParameters) override
	{
		EnsureDefinitions();

		if (!InParameters.IsEmpty())
		{
			const TSharedRef<FSpec>* SpecToRun = IdToSpecMap.Find(InParameters);
			if (SpecToRun != nullptr)
			{
				for (int32 Index = 0; Index < (*SpecToRun)->Commands.Num(); ++Index)
				{
					FAutomationTestFramework::GetInstance().EnqueueLatentCommand((*SpecToRun)->Commands[Index]);
				}
			}
		}
		else
		{
			TArray<TSharedRef<FSpec>> Specs;
			IdToSpecMap.GenerateValueArray(Specs);

			for (int32 SpecIndex = 0; SpecIndex < Specs.Num(); SpecIndex++)
			{
				for (int32 CommandIndex = 0; CommandIndex < Specs[SpecIndex]->Commands.Num(); ++CommandIndex)
				{
					FAutomationTestFramework::GetInstance().EnqueueLatentCommand(Specs[SpecIndex]->Commands[CommandIndex]);
				}
			}
		}

		return true;
	}

	virtual bool IsStressTest() const 
	{
		return false; 
	}

	virtual uint32 GetRequiredDeviceNum() const override 
	{ 
		return 1; 
	}

	virtual FString GetTestSourceFileName(const FString& InTestName) const override
	{
		FString TestId = InTestName;
		if (TestId.StartsWith(TestName + TEXT(" ")))
		{
			TestId = InTestName.RightChop(TestName.Len() + 1);
		}

		const TSharedRef<FSpec>* Spec = IdToSpecMap.Find(TestId);
		if (Spec != nullptr)
		{
			return (*Spec)->Filename;
		}

		return FAutomationTestBase::GetTestSourceFileName();
	}

	virtual int32 GetTestSourceFileLine(const FString& InTestName) const override
	{ 
		FString TestId = InTestName;
		if (TestId.StartsWith(TestName + TEXT(" ")))
		{
			TestId = InTestName.RightChop(TestName.Len() + 1);
		}

		const TSharedRef<FSpec>* Spec = IdToSpecMap.Find(TestId);
		if (Spec != nullptr)
		{
			return (*Spec)->LineNumber;
		}

		return FAutomationTestBase::GetTestSourceFileLine();
	}

	virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const override
	{
		EnsureDefinitions();

		TArray<TSharedRef<FSpec>> Specs;
		IdToSpecMap.GenerateValueArray(Specs);

		for (int32 Index = 0; Index < Specs.Num(); Index++)
		{
			OutTestCommands.Push(Specs[Index]->Id);
			OutBeautifiedNames.Push(Specs[Index]->Description);
		}
	}

	void xDescribe(const FString& InDescription, TFunction<void()> DoWork)
	{
		//disabled
	}

	void Describe(const FString& InDescription, TFunction<void()> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> ParentScope = DefinitionScopeStack.Last();
		const TSharedRef<FSpecDefinitionScope> NewScope = MakeShareable(new FSpecDefinitionScope());
		NewScope->Description = InDescription;
		ParentScope->Children.Push(NewScope);

		DefinitionScopeStack.Push(NewScope);
		PushDescription(InDescription);
		DoWork();
		PopDescription(InDescription);
		DefinitionScopeStack.Pop();

		if (NewScope->It.Num() == 0 && NewScope->Children.Num() == 0)
		{
			ParentScope->Children.Remove(NewScope);
		}
	}

	void xIt(const FString& InDescription, TFunction<void()> DoWork)
	{
		//disabled
	}

	void xIt(const FString& InDescription, EAsyncExecution Execution, TFunction<void()> DoWork)
	{
		//disabled
	}

	void xLatentIt(const FString& InDescription, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		//disabled
	}

	void xLatentIt(const FString& InDescription, EAsyncExecution Execution, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		//disabled
	}

	void It(const FString& InDescription, TFunction<void()> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		const TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(1, 1);

		PushDescription(InDescription);
		CurrentScope->It.Push(MakeShareable(new FSpecIt(GetDescription(), GetId(), Stack[0].Filename, Stack[0].LineNumber, MakeShareable(new FSingleExecuteLatentCommand(DoWork)))));
		PopDescription(InDescription);
	}

	void It(const FString& InDescription, EAsyncExecution Execution, TFunction<void()> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		const TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(1, 1);

		PushDescription(InDescription);
		CurrentScope->It.Push(MakeShareable(new FSpecIt(GetDescription(), GetId(), Stack[0].Filename, Stack[0].LineNumber, MakeShareable(new FAsyncLatentCommand(Execution, DoWork)))));
		PopDescription(InDescription);
	}

	void LatentIt(const FString& InDescription, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		const TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(1, 1);

		PushDescription(InDescription);
		CurrentScope->It.Push(MakeShareable(new FSpecIt(GetDescription(), GetId(), Stack[0].Filename, Stack[0].LineNumber, MakeShareable(new FUntilDoneLatentCommand(DoWork)))));
		PopDescription(InDescription);
	}

	void LatentIt(const FString& InDescription, EAsyncExecution Execution, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		const TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(1, 1);

		PushDescription(InDescription);
		CurrentScope->It.Push(MakeShareable(new FSpecIt(GetDescription(), GetId(), Stack[0].Filename, Stack[0].LineNumber, MakeShareable(new FAsyncUntilDoneLatentCommand(Execution, DoWork)))));
		PopDescription(InDescription);
	}

	void xBeforeEach(TFunction<void()> DoWork)
	{
		// disabled
	}

	void xBeforeEach(EAsyncExecution Execution, TFunction<void()> DoWork)
	{
		// disabled
	}

	void xLatentBeforeEach(TFunction<void(const FDoneDelegate&)> DoWork)
	{
		// disabled
	}

	void xLatentBeforeEach(EAsyncExecution Execution, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		// disabled
	}

	void BeforeEach(TFunction<void()> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->BeforeEach.Push(MakeShareable(new FSingleExecuteLatentCommand(DoWork)));
	}

	void BeforeEach(EAsyncExecution Execution, TFunction<void()> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->BeforeEach.Push(MakeShareable(new FAsyncLatentCommand(Execution, DoWork)));
	}

	void LatentBeforeEach(TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->BeforeEach.Push(MakeShareable(new FUntilDoneLatentCommand(DoWork)));
	}
	     
	void LatentBeforeEach(EAsyncExecution Execution, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->BeforeEach.Push(MakeShareable(new FAsyncUntilDoneLatentCommand(Execution, DoWork)));
	}

	void xAfterEach(TFunction<void()> DoWork)
	{
		// disabled
	}

	void xAfterEach(EAsyncExecution Execution, TFunction<void()> DoWork)
	{
		// disabled
	}

	void xLatentAfterEach(TFunction<void(const FDoneDelegate&)> DoWork)
	{
		// disabled
	}

	void xLatentAfterEach(EAsyncExecution Execution, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		// disabled
	}

	void AfterEach(TFunction<void()> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->AfterEach.Push(MakeShareable(new FSingleExecuteLatentCommand(DoWork)));
	}

	void AfterEach(EAsyncExecution Execution, TFunction<void()> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->AfterEach.Push(MakeShareable(new FAsyncLatentCommand(Execution, DoWork)));
	}

	void LatentAfterEach(TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->AfterEach.Push(MakeShareable(new FUntilDoneLatentCommand(DoWork)));
	}

	void LatentAfterEach(EAsyncExecution Execution, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->AfterEach.Push(MakeShareable(new FAsyncUntilDoneLatentCommand(Execution, DoWork)));
	}

protected:

	void EnsureDefinitions() const
	{
		if (!bHasBeenDefined)
		{
			const_cast<FAutomationSpecBase*>(this)->Define();
			const_cast<FAutomationSpecBase*>(this)->PostDefine();
		}
	}

	virtual void Define() = 0;

	void PostDefine()
	{
		TArray<TSharedRef<FSpecDefinitionScope>> Stack;
		Stack.Push(RootDefinitionScope.ToSharedRef());

		TArray<TSharedRef<IAutomationLatentCommand>> BeforeEach;
		TArray<TSharedRef<IAutomationLatentCommand>> AfterEach;

		while (Stack.Num() > 0)
		{
			const TSharedRef<FSpecDefinitionScope> Scope = Stack.Last();

			BeforeEach.Append(Scope->BeforeEach);
			AfterEach.Append(Scope->AfterEach); // iterate in reverse

			for (int32 ItIndex = 0; ItIndex < Scope->It.Num(); ItIndex++)
			{
				TSharedRef<FSpecIt> It = Scope->It[ItIndex];

				TSharedRef<FSpec> Spec = MakeShareable(new FSpec());
				Spec->Id = It->Id;
				Spec->Description = It->Description;
				Spec->Filename = It->Filename;
				Spec->LineNumber = It->LineNumber;
				Spec->Commands.Append(BeforeEach);
				Spec->Commands.Add(It->Command);

				for (int32 AfterEachIndex = AfterEach.Num() - 1; AfterEachIndex >= 0; AfterEachIndex--)
				{
					Spec->Commands.Add(AfterEach[AfterEachIndex]);
				}

				check(!IdToSpecMap.Contains(Spec->Id));
				IdToSpecMap.Add(Spec->Id, Spec);
			}
			Scope->It.Empty();

			if (Scope->Children.Num() > 0)
			{
				Stack.Append(Scope->Children);
				Scope->Children.Empty();
			}
			else
			{
				while (Stack.Num() > 0 && Stack.Last()->Children.Num() == 0 && Stack.Last()->It.Num() == 0)
				{
					const TSharedRef<FSpecDefinitionScope> PoppedScope = Stack.Pop();

					if (PoppedScope->BeforeEach.Num() > 0)
					{
						BeforeEach.RemoveAt(BeforeEach.Num() - PoppedScope->BeforeEach.Num(), PoppedScope->BeforeEach.Num());
					}

					if (PoppedScope->AfterEach.Num() > 0)
					{
						AfterEach.RemoveAt(AfterEach.Num() - PoppedScope->AfterEach.Num(), PoppedScope->AfterEach.Num());
					}
				}
			}
		}

		RootDefinitionScope.Reset();
		DefinitionScopeStack.Reset();
		bHasBeenDefined = true;
	}

	void Redefine()
	{
		Description.Empty();
		IdToSpecMap.Empty();
		RootDefinitionScope.Reset();
		DefinitionScopeStack.Empty();
		bHasBeenDefined = false;
	}

private:

	void PushDescription(const FString& InDescription)
	{
		Description.Add(InDescription);
	}

	void PopDescription(const FString& InDescription)
	{
		Description.RemoveAt(Description.Num() - 1);
	}

	FString GetDescription() const
	{
		FString CompleteDescription;
		for (int32 Index = 0; Index < Description.Num(); ++Index)
		{
			if (Description[Index].IsEmpty())
			{
				continue;
			}

			if (CompleteDescription.IsEmpty())
			{
				CompleteDescription = Description[Index];
			}
			else if (FChar::IsWhitespace(CompleteDescription[CompleteDescription.Len() - 1]) || FChar::IsWhitespace(Description[Index][0]))
			{
				CompleteDescription = CompleteDescription + TEXT(".") + Description[Index];
			}
			else
			{
				CompleteDescription = FString::Printf(TEXT("%s.%s"), *CompleteDescription, *Description[Index]);
			}
		}

		return CompleteDescription;
	}

	FString GetId() const
	{
		if (Description.Last().EndsWith(TEXT("]")))
		{
			FString ItDescription = Description.Last();
			ItDescription.RemoveAt(ItDescription.Len() - 1);

			int32 StartingBraceIndex = INDEX_NONE;
			if (ItDescription.FindLastChar(TEXT('['), StartingBraceIndex) && StartingBraceIndex != ItDescription.Len() - 1)
			{
				FString CommandId = ItDescription.RightChop(StartingBraceIndex + 1);
				return CommandId;
			}
		}

		FString CompleteId;
		for (int32 Index = 0; Index < Description.Num(); ++Index)
		{
			if (Description[Index].IsEmpty())
			{
				continue;
			}

			if (CompleteId.IsEmpty())
			{
				CompleteId = Description[Index];
			}
			else if (FChar::IsWhitespace(CompleteId[CompleteId.Len() - 1]) || FChar::IsWhitespace(Description[Index][0]))
			{
				CompleteId = CompleteId + Description[Index];
			}
			else
			{
				CompleteId = FString::Printf(TEXT("%s %s"), *CompleteId, *Description[Index]);
			}
		}

		return CompleteId;
	}

private:

	TArray<FString> Description;
	TMap<FString, TSharedRef<FSpec>> IdToSpecMap;
	TSharedPtr<FSpecDefinitionScope> RootDefinitionScope;
	TArray<TSharedRef<FSpecDefinitionScope>> DefinitionScopeStack;
	bool bHasBeenDefined;
};


//////////////////////////////////////////////////////////////////////////
// Latent command definition macros

#define DEFINE_LATENT_AUTOMATION_COMMAND(CommandName)	\
class CommandName : public IAutomationLatentCommand \
	{ \
	public: \
	virtual ~CommandName() \
		{} \
		virtual bool Update() override; \
}

#define DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(CommandName,ParamType,ParamName)	\
class CommandName : public IAutomationLatentCommand \
	{ \
	public: \
	CommandName(ParamType InputParam) \
	: ParamName(InputParam) \
		{} \
		virtual ~CommandName() \
		{} \
		virtual bool Update() override; \
	private: \
	ParamType ParamName; \
}

#define DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND(EXPORT_API, CommandName)	\
class EXPORT_API CommandName : public IAutomationLatentCommand \
	{ \
	public: \
	virtual ~CommandName() \
		{} \
		virtual bool Update() override; \
}

#define DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(EXPORT_API, CommandName,ParamType,ParamName)	\
class EXPORT_API CommandName : public IAutomationLatentCommand \
	{ \
	public: \
	CommandName(ParamType InputParam) \
	: ParamName(InputParam) \
		{} \
		virtual ~CommandName() \
		{} \
		virtual bool Update() override; \
	private: \
	ParamType ParamName; \
}

#define DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND(CommandName)	\
	DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND(ENGINE_API, CommandName)

#define DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(CommandName,ParamType,ParamName)	\
	DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(ENGINE_API, CommandName, ParamType, ParamName)

//macro to simply the syntax for enqueueing a latent command
#define ADD_LATENT_AUTOMATION_COMMAND(ClassDeclaration) FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShareable(new ClassDeclaration));


//declare the class
#define START_NETWORK_AUTOMATION_COMMAND(ClassDeclaration)	\
class F##ClassDeclaration : public IAutomationNetworkCommand \
{ \
private:\
	int32 RoleIndex; \
public: \
	F##ClassDeclaration(int32 InRoleIndex) : RoleIndex(InRoleIndex) {} \
	virtual ~F##ClassDeclaration() {} \
	virtual uint32 GetRoleIndex() const override { return RoleIndex; } \
	virtual void Run() override 

//close the class and add to the framework
#define END_NETWORK_AUTOMATION_COMMAND(ClassDeclaration,InRoleIndex) }; \
	FAutomationTestFramework::Get().EnqueueNetworkCommand(MakeShareable(new F##ClassDeclaration(InRoleIndex))); \

/**
 * Macros to simplify the creation of new automation tests. To create a new test one simply must put
 * IMPLEMENT_SIMPLE_AUTOMATION_TEST( NewAutomationClassName, AutomationClassFlags )
 * IMPLEMENT_COMPLEX_AUTOMATION_TEST( NewAutomationClassName, AutomationClassFlags )
 * in their cpp file, and then proceed to write an implementation for:
 * bool NewAutomationTestClassName::RunTest() {}
 * While the macro could also have allowed the code to be specified, leaving it out of the macro allows
 * the code to be debugged more easily.
 *
 * Builds supporting automation tests will automatically create and register an instance of the automation test within
 * the automation test framework as a result of the macro.
 */

#define IMPLEMENT_SIMPLE_AUTOMATION_TEST_PRIVATE( TClass, TBaseClass, PrettyName, TFlags, FileName, LineNumber ) \
	class TClass : public TBaseClass \
	{ \
	public: \
		TClass( const FString& InName ) \
		:TBaseClass( InName, false ) {\
			static_assert((TFlags)&EAutomationTestFlags::ApplicationContextMask, "AutomationTest has no application flag.  It shouldn't run.  See AutomationTest.h."); \
			static_assert(	(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::SmokeFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::EngineFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::ProductFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::PerfFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::StressFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::NegativeFilter), \
							"All AutomationTests must have exactly 1 filter type specified.  See AutomationTest.h."); \
		} \
		virtual uint32 GetTestFlags() const override { return TFlags; } \
		virtual bool IsStressTest() const { return false; } \
		virtual uint32 GetRequiredDeviceNum() const override { return 1; } \
		virtual FString GetTestSourceFileName() const override { return FileName; } \
		virtual int32 GetTestSourceFileLine() const override { return LineNumber; } \
	protected: \
		virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const override \
		{ \
			OutBeautifiedNames.Add(PrettyName); \
			OutTestCommands.Add(FString()); \
		} \
		virtual bool RunTest(const FString& Parameters) override; \
		virtual FString GetBeautifiedTestName() const override { return PrettyName; } \
	};

#define IMPLEMENT_COMPLEX_AUTOMATION_TEST_PRIVATE( TClass, TBaseClass, PrettyName, TFlags, FileName, LineNumber ) \
	class TClass : public TBaseClass \
	{ \
	public: \
		TClass( const FString& InName ) \
		:TBaseClass( InName, true ) { \
			static_assert((TFlags)&EAutomationTestFlags::ApplicationContextMask, "AutomationTest has no application flag.  It shouldn't run.  See AutomationTest.h."); \
			static_assert(	(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::SmokeFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::EngineFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::ProductFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::PerfFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::StressFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::NegativeFilter), \
							"All AutomationTests must have exactly 1 filter type specified.  See AutomationTest.h."); \
		} \
		virtual uint32 GetTestFlags() const override { return ((TFlags) & ~(EAutomationTestFlags::SmokeFilter)); } \
		virtual bool IsStressTest() const { return true; } \
		virtual uint32 GetRequiredDeviceNum() const override { return 1; } \
		virtual FString GetTestSourceFileName() const override { return FileName; } \
		virtual int32 GetTestSourceFileLine() const override { return LineNumber; } \
	protected: \
		virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const override; \
		virtual bool RunTest(const FString& Parameters) override; \
		virtual FString GetBeautifiedTestName() const override { return PrettyName; } \
	};

#define IMPLEMENT_NETWORKED_AUTOMATION_TEST_PRIVATE(TClass, TBaseClass, PrettyName, TFlags, NumParticipants, FileName, LineNumber) \
	class TClass : public TBaseClass \
	{ \
	public: \
		TClass( const FString& InName ) \
		:TBaseClass( InName, false ) { \
			static_assert((TFlags)&EAutomationTestFlags::ApplicationContextMask, "AutomationTest has no application flag.  It shouldn't run.  See AutomationTest.h."); \
			static_assert(	(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::SmokeFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::EngineFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::ProductFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::PerfFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::StressFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::NegativeFilter), \
							"All AutomationTests must have exactly 1 filter type specified.  See AutomationTest.h."); \
		} \
		virtual uint32 GetTestFlags() const override { return ((TFlags) & ~(EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::SmokeFilter)); } \
		virtual uint32 GetRequiredDeviceNum() const override { return NumParticipants; } \
		virtual FString GetTestSourceFileName() const override { return FileName; } \
		virtual int32 GetTestSourceFileLine() const override { return LineNumber; } \
	protected: \
		virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const override \
		{ \
			OutBeautifiedNames.Add(PrettyName); \
			OutTestCommands.Add(FString()); \
		} \
		virtual bool RunTest(const FString& Parameters) override; \
		virtual FString GetBeautifiedTestName() const override { return PrettyName; } \
	};

#define IMPLEMENT_BDD_AUTOMATION_TEST_PRIVATE( TClass, PrettyName, TFlags, FileName, LineNumber ) \
	class TClass : public FBDDAutomationTestBase \
	{ \
	public: \
		TClass( const FString& InName ) \
		:FBDDAutomationTestBase( InName, false ) {\
			static_assert((TFlags)&EAutomationTestFlags::ApplicationContextMask, "AutomationTest has no application flag.  It shouldn't run.  See AutomationTest.h."); \
			static_assert(	(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::SmokeFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::EngineFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::ProductFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::PerfFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::StressFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::NegativeFilter), \
							"All AutomationTests must have exactly 1 filter type specified.  See AutomationTest.h."); \
		} \
		virtual uint32 GetTestFlags() const override { return TFlags; } \
		virtual bool IsStressTest() const { return false; } \
		virtual uint32 GetRequiredDeviceNum() const override { return 1; } \
		virtual FString GetTestSourceFileName() const override { return FileName; } \
		virtual int32 GetTestSourceFileLine() const override { return LineNumber; } \
	protected: \
		virtual bool RunTest(const FString& Parameters) override; \
		virtual FString GetBeautifiedTestName() const override { return PrettyName; } \
	private: \
		void Define(); \
	};

#define DEFINE_SPEC_PRIVATE( TClass, PrettyName, TFlags, FileName, LineNumber ) \
	class TClass : public FAutomationSpecBase \
	{ \
	public: \
		TClass( const FString& InName ) \
		: FAutomationSpecBase( InName, false ) {\
			static_assert((TFlags)&EAutomationTestFlags::ApplicationContextMask, "AutomationTest has no application flag.  It shouldn't run.  See AutomationTest.h."); \
			static_assert(	(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::SmokeFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::EngineFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::ProductFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::PerfFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::StressFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::NegativeFilter), \
							"All AutomationTests must have exactly 1 filter type specified.  See AutomationTest.h."); \
		} \
		virtual uint32 GetTestFlags() const override { return TFlags; } \
        using FAutomationSpecBase::GetTestSourceFileName; \
		virtual FString GetTestSourceFileName() const override { return FileName; } \
        using FAutomationSpecBase::GetTestSourceFileLine; \
		virtual int32 GetTestSourceFileLine() const override { return LineNumber; } \
		virtual FString GetTestSourceFileName(const FString&) const override { return GetTestSourceFileName(); } \
		virtual int32 GetTestSourceFileLine(const FString&) const override { return GetTestSourceFileLine(); } \
	protected: \
		virtual FString GetBeautifiedTestName() const override { return PrettyName; } \
		virtual void Define() override; \
	};

#define BEGIN_DEFINE_SPEC_PRIVATE( TClass, PrettyName, TFlags, FileName, LineNumber ) \
	class TClass : public FAutomationSpecBase \
	{ \
	public: \
		TClass( const FString& InName ) \
		: FAutomationSpecBase( InName, false ) {\
			static_assert((TFlags)&EAutomationTestFlags::ApplicationContextMask, "AutomationTest has no application flag.  It shouldn't run.  See AutomationTest.h."); \
			static_assert(	(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::SmokeFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::EngineFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::ProductFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::PerfFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::StressFilter) || \
							(((TFlags)&EAutomationTestFlags::FilterMask) == EAutomationTestFlags::NegativeFilter), \
							"All AutomationTests must have exactly 1 filter type specified.  See AutomationTest.h."); \
		} \
		virtual uint32 GetTestFlags() const override { return TFlags; } \
		using FAutomationSpecBase::GetTestSourceFileName; \
		virtual FString GetTestSourceFileName() const override { return FileName; } \
		using FAutomationSpecBase::GetTestSourceFileLine; \
		virtual int32 GetTestSourceFileLine() const override { return LineNumber; } \
	protected: \
		virtual FString GetBeautifiedTestName() const override { return PrettyName; } \
		virtual void Define() override;

#if WITH_AUTOMATION_WORKER
	#define IMPLEMENT_SIMPLE_AUTOMATION_TEST( TClass, PrettyName, TFlags ) \
		IMPLEMENT_SIMPLE_AUTOMATION_TEST_PRIVATE(TClass, FAutomationTestBase, PrettyName, TFlags, __FILE__, __LINE__) \
		namespace\
		{\
			TClass TClass##AutomationTestInstance( TEXT(#TClass) );\
		}
	#define IMPLEMENT_COMPLEX_AUTOMATION_TEST( TClass, PrettyName, TFlags ) \
		IMPLEMENT_COMPLEX_AUTOMATION_TEST_PRIVATE(TClass, FAutomationTestBase, PrettyName, TFlags, __FILE__, __LINE__) \
		namespace\
		{\
			TClass TClass##AutomationTestInstance( TEXT(#TClass) );\
		}
	#define IMPLEMENT_COMPLEX_AUTOMATION_CLASS( TClass, PrettyName, TFlags ) \
		IMPLEMENT_COMPLEX_AUTOMATION_TEST_PRIVATE(TClass, FAutomationTestBase, PrettyName, TFlags, __FILE__, __LINE__)
	#define IMPLEMENT_NETWORKED_AUTOMATION_TEST(TClass, PrettyName, TFlags, NumParticipants) \
		IMPLEMENT_NETWORKED_AUTOMATION_TEST_PRIVATE(TClass, FAutomationTestBase, PrettyName, TFlags, NumParticipants, __FILE__, __LINE__) \
		namespace\
		{\
			TClass TClass##AutomationTestInstance( TEXT(#TClass) );\
		}

	#define IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST( TClass, TBaseClass, PrettyName, TFlags ) \
		IMPLEMENT_SIMPLE_AUTOMATION_TEST_PRIVATE(TClass, TBaseClass, PrettyName, TFlags, __FILE__, __LINE__) \
		namespace\
		{\
			TClass TClass##AutomationTestInstance( TEXT(#TClass) );\
		}

	#define IMPLEMENT_CUSTOM_COMPLEX_AUTOMATION_TEST( TClass, TBaseClass, PrettyName, TFlags ) \
		IMPLEMENT_COMPLEX_AUTOMATION_TEST_PRIVATE(TClass, TBaseClass, PrettyName, TFlags, __FILE__, __LINE__) \
		namespace\
		{\
			TClass TClass##AutomationTestInstance( TEXT(#TClass) );\
		}

	#define IMPLEMENT_BDD_AUTOMATION_TEST( TClass, PrettyName, TFlags ) \
		IMPLEMENT_BDD_AUTOMATION_TEST_PRIVATE(TClass, PrettyName, TFlags, __FILE__, __LINE__) \
		namespace\
		{\
			TClass TClass##AutomationTestInstance( TEXT(#TClass) );\
		}

	#define DEFINE_SPEC( TClass, PrettyName, TFlags ) \
		DEFINE_SPEC_PRIVATE(TClass, PrettyName, TFlags, __FILE__, __LINE__) \
		namespace\
		{\
			TClass TClass##AutomationSpecInstance( TEXT(#TClass) );\
		}

	#define BEGIN_DEFINE_SPEC( TClass, PrettyName, TFlags ) \
		BEGIN_DEFINE_SPEC_PRIVATE(TClass, PrettyName, TFlags, __FILE__, __LINE__) 

	#define END_DEFINE_SPEC( TClass ) \
		};\
		namespace\
		{\
			TClass TClass##AutomationSpecInstance( TEXT(#TClass) );\
		}

	//#define BEGIN_CUSTOM_COMPLEX_AUTOMATION_TEST( TClass, TBaseClass, PrettyName, TFlags ) \
	//	BEGIN_COMPLEX_AUTOMATION_TEST_PRIVATE(TClass, TBaseClass, PrettyName, TFlags, __FILE__, __LINE__)
	//
	//#define END_CUSTOM_COMPLEX_AUTOMATION_TEST( TClass ) \
	//	BEGIN_COMPLEX_AUTOMATION_TEST_PRIVATE(TClass, TBaseClass, PrettyName, TFlags, __FILE__, __LINE__)
	//	namespace\
	//	{\
	//		TClass TClass##AutomationTestInstance( TEXT(#TClass) );\
	//	}

#else
	#define IMPLEMENT_SIMPLE_AUTOMATION_TEST( TClass, PrettyName, TFlags ) \
		IMPLEMENT_SIMPLE_AUTOMATION_TEST_PRIVATE(TClass, FAutomationTestBase, PrettyName, TFlags, __FILE__, __LINE__)
	#define IMPLEMENT_COMPLEX_AUTOMATION_TEST( TClass, PrettyName, TFlags ) \
		IMPLEMENT_COMPLEX_AUTOMATION_TEST_PRIVATE(TClass, FAutomationTestBase, PrettyName, TFlags, __FILE__, __LINE__)
	#define IMPLEMENT_NETWORKED_AUTOMATION_TEST(TClass, PrettyName, TFlags, NumParticipants) \
		IMPLEMENT_NETWORKED_AUTOMATION_TEST_PRIVATE(TClass, FAutomationTestBase, PrettyName, TFlags, NumParticipants, __FILE__, __LINE__)

	#define IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST( TClass, TBaseClass, PrettyName, TFlags ) \
		IMPLEMENT_SIMPLE_AUTOMATION_TEST_PRIVATE(TClass, TBaseClass, PrettyName, TFlags, __FILE__, __LINE__)
	#define IMPLEMENT_CUSTOM_COMPLEX_AUTOMATION_TEST( TClass, TBaseClass, PrettyName, TFlags ) \
		IMPLEMENT_COMPLEX_AUTOMATION_TEST_PRIVATE(TClass, TBaseClass, PrettyName, TFlags, __FILE__, __LINE__)
	#define IMPLEMENT_BDD_AUTOMATION_TEST(TClass, PrettyName, TFlags) \
		IMPLEMENT_BDD_AUTOMATION_TEST_PRIVATE(TClass, PrettyName, TFlags, __FILE__, __LINE__)
	#define DEFINE_SPEC(TClass, PrettyName, TFlags) \
		DEFINE_SPEC_PRIVATE(TClass, PrettyName, TFlags, __FILE__, __LINE__)
	#define BEGIN_DEFINE_SPEC(TClass, PrettyName, TFlags) \
		BEGIN_DEFINE_SPEC_PRIVATE(TClass, PrettyName, TFlags, __FILE__, __LINE__)
	#define END_DEFINE_SPEC(TClass) \
		}; \

	//#define BEGIN_CUSTOM_COMPLEX_AUTOMATION_TEST( TClass, TBaseClass, PrettyName, TFlags ) \
	//	BEGIN_CUSTOM_COMPLEX_AUTOMATION_TEST_PRIVATE(TClass, TBaseClass, PrettyName, TFlags, __FILE__, __LINE__)

	//#define END_CUSTOM_COMPLEX_AUTOMATION_TEST( TClass )
	//	END_COMPLEX_AUTOMATION_TEST_PRIVATE(TClass, TBaseClass, PrettyName, TFlags, __FILE__, __LINE__)
#endif // #if WITH_AUTOMATION_WORKER


//////////////////////////////////////////////////////////////////////////
// Basic Latent Commands

/**
 * Run some code latently with a predicate lambda.  If the predicate returns true, the latent action will be called 
 * again next frame.  If it returns false, the command will stop running.
 */
class FFunctionLatentCommand : public IAutomationLatentCommand
{
public:
	FFunctionLatentCommand(TFunction<bool()> InLatentPredicate)
		: LatentPredicate(MoveTemp(InLatentPredicate))
	{
	}

	virtual ~FFunctionLatentCommand()
	{
	}

	virtual bool Update() override
	{
		return LatentPredicate();
	}

private:
	TFunction<bool()> LatentPredicate;
};


class FDelayedFunctionLatentCommand : public IAutomationLatentCommand
{
public:
	FDelayedFunctionLatentCommand(TFunction<void()> InCallback, float InDelay = 0.1f)
		: Callback(MoveTemp(InCallback))
		, Delay(InDelay)
	{}

	virtual bool Update() override
	{
		float NewTime = FPlatformTime::Seconds();
		if ( NewTime - StartTime >= Delay )
		{
			Callback();
			return true;
		}
		return false;
	}

private:
	TFunction<void()> Callback;
	float Delay;
};


class FUntilCommand : public IAutomationLatentCommand
{
public:
	FUntilCommand(TFunction<bool()> InCallback, TFunction<bool()> InTimeoutCallback, float InTimeout = 5.0f)
		: Callback(MoveTemp(InCallback))
		, TimeoutCallback(MoveTemp(InTimeoutCallback))
		, Timeout(InTimeout)
	{}

	virtual bool Update() override
	{
		if ( !Callback() )
		{
			float NewTime = FPlatformTime::Seconds();
			if ( NewTime - StartTime >= Timeout )
			{
				TimeoutCallback();
				return false;
			}

			return true;
		}

		return false;
	}

private:
	TFunction<bool()> Callback;
	TFunction<bool()> TimeoutCallback;
	float Timeout;
};
