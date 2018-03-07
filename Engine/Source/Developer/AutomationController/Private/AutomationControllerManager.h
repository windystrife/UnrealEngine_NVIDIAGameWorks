// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Containers/Queue.h"
#include "Misc/AutomationTest.h"
#include "IAutomationControllerManager.h"
#include "IMessageContext.h"
#include "MessageEndpoint.h"
#include "Developer/AutomationController/Private/AutomationDeviceClusterManager.h"
#include "Developer/AutomationController/Private/AutomationReportManager.h"
#include "Async/Future.h"
#include "ImageComparer.h"
#include "Interfaces/IScreenShotManager.h"
#include "AutomationControllerManager.generated.h"

USTRUCT()
struct FAutomatedTestResult
{
	GENERATED_BODY()
public:

	TSharedPtr<IAutomationReport> Test;

	UPROPERTY()
	FString TestDisplayName;

	UPROPERTY()
	FString FullTestPath;

	UPROPERTY()
	EAutomationState State;

	UPROPERTY()
	TArray<FAutomationArtifact> Artifacts;

	FAutomatedTestResult()
	{
		State = EAutomationState::NotRun;
	}

	void SetEvents(const TArray<FAutomationEvent>& InEvents, int32 InWarnings, int32 InErrors)
	{
		Events = InEvents;
		Warnings = InWarnings;
		Errors = InErrors;
	}

	int32 GetWarningTotal() const { return Warnings; }
	int32 GetErrorTotal() const { return Errors; }

	const TArray<FAutomationEvent>& GetEvents() const { return Events; }

private:

	UPROPERTY()
	TArray<FAutomationEvent> Events;

	UPROPERTY()
	int32 Warnings;

	UPROPERTY()
	int32 Errors;
};

USTRUCT()
struct FAutomatedTestPassResults
{
	GENERATED_BODY()

public:
	FAutomatedTestPassResults()
		: Succeeded(0)
		, SucceededWithWarnings(0)
		, Failed(0)
		, NotRun(0)
		, TotalDuration(0)
		, ComparisonExported(false)
	{
	}

	UPROPERTY()
	int32 Succeeded;

	UPROPERTY()
	int32 SucceededWithWarnings;

	UPROPERTY()
	int32 Failed;

	UPROPERTY()
	int32 NotRun;

	UPROPERTY()
	float TotalDuration;

	UPROPERTY()
	bool ComparisonExported;

	UPROPERTY()
	FString ComparisonExportDirectory;

	UPROPERTY()
	TArray<FAutomatedTestResult> Tests;

	int32 GetTotalTests() const
	{
		return Succeeded + SucceededWithWarnings + Failed + NotRun;
	}

	void ClearAllEntries()
	{
		Succeeded = 0;
		SucceededWithWarnings = 0;
		Failed = 0;
		NotRun = 0;
		TotalDuration = 0;
		Tests.Empty();
	}
};


/**
 * Implements the AutomationController module.
 */
class FAutomationControllerManager : public IAutomationControllerManager
{
public:
	FAutomationControllerManager();

	// IAutomationController Interface
	virtual void RequestAvailableWorkers( const FGuid& InSessionId ) override;
	virtual void RequestTests() override;
	virtual void RunTests( const bool bIsLocalSession) override;
	virtual void StopTests() override;
	virtual void Init() override;
	virtual void RequestLoadAsset( const FString& InAssetName ) override;
	virtual void Tick() override;

	virtual void SetNumPasses(const int32 InNumPasses) override
	{
		NumTestPasses = InNumPasses;
	}

	virtual int32 GetNumPasses() override
	{
		return NumTestPasses;
	}

	virtual bool IsSendAnalytics() const override
	{
		return bSendAnalytics;
	}

	virtual void SetSendAnalytics(const bool bNewValue) override
	{
		bSendAnalytics = bNewValue;
	}

	virtual void SetFilter( TSharedPtr< AutomationFilterCollection > InFilter ) override
	{
		ReportManager.SetFilter( InFilter );
	}

	virtual TArray <TSharedPtr <IAutomationReport> >& GetReports() override
	{
		return ReportManager.GetFilteredReports();
	}

	virtual int32 GetNumDeviceClusters() const override
	{
		return DeviceClusterManager.GetNumClusters();
	}

	virtual int32 GetNumDevicesInCluster(const int32 ClusterIndex) const override
	{
		return DeviceClusterManager.GetNumDevicesInCluster(ClusterIndex);
	}

	virtual FString GetClusterGroupName(const int32 ClusterIndex) const override
	{
		return DeviceClusterManager.GetClusterGroupName(ClusterIndex);
	}

	virtual FString GetDeviceTypeName(const int32 ClusterIndex) const override
	{
		return DeviceClusterManager.GetClusterDeviceType(ClusterIndex);
	}

	virtual FString GetGameInstanceName(const int32 ClusterIndex, const int32 DeviceIndex) const override
	{
		return DeviceClusterManager.GetClusterDeviceName(ClusterIndex, DeviceIndex);
	}

	virtual void SetVisibleTestsEnabled(const bool bEnabled) override
	{
		ReportManager.SetVisibleTestsEnabled (bEnabled);
	}

	virtual int32 GetEnabledTestsNum() const override
	{
		return ReportManager.GetEnabledTestsNum();
	}

	virtual void GetEnabledTestNames(TArray<FString>& OutEnabledTestNames) const override
	{
		ReportManager.GetEnabledTestNames(OutEnabledTestNames);
	}

	virtual void SetEnabledTests(const TArray<FString>& EnabledTests) override
	{
		ReportManager.SetEnabledTests(EnabledTests);
	}

	virtual EAutomationControllerModuleState::Type GetTestState( ) const override
	{
		return AutomationTestState;
	}

	virtual void SetDeveloperDirectoryIncluded(const bool bInDeveloperDirectoryIncluded) override
	{
		bDeveloperDirectoryIncluded = bInDeveloperDirectoryIncluded;
	}

	virtual bool IsDeveloperDirectoryIncluded(void) const override
	{
		return bDeveloperDirectoryIncluded;
	}

	virtual void SetRequestedTestFlags(const uint32 InRequestedTestFlags) override
	{
		RequestedTestFlags = InRequestedTestFlags;
		RequestTests();
	}

	virtual const bool CheckTestResultsAvailable() const override
	{
		return 	bTestResultsAvailable;
	}

	virtual const bool ReportsHaveErrors() const override
	{
		return bHasErrors;
	}

	virtual const bool ReportsHaveWarnings() const override
	{
		return bHasWarning;
	}

	virtual const bool ReportsHaveLogs() const override
	{
		return bHasLogs;
	}

	virtual void ClearAutomationReports() override
	{
		ReportManager.Empty();
	}

	virtual const bool ExportReport(uint32 FileExportTypeMask) override;
	virtual bool IsTestRunnable( IAutomationReportPtr InReport ) const override;
	virtual void RemoveCallbacks() override;
	virtual void Shutdown() override;
	virtual void Startup() override;

	virtual FOnAutomationControllerManagerShutdown& OnShutdown( ) override
	{
		return ShutdownDelegate;
	}

	virtual FOnAutomationControllerManagerTestsAvailable& OnTestsAvailable( ) override
	{
		return TestsAvailableDelegate;
	}

	virtual FOnAutomationControllerTestsRefreshed& OnTestsRefreshed( ) override
	{
		return TestsRefreshedDelegate;
	}

	virtual FOnAutomationControllerTestsComplete& OnTestsComplete() override
	{
		return TestsCompleteDelegate;
	}

	virtual FOnAutomationControllerReset& OnControllerReset() override
	{
		return ControllerResetDelegate;
	}


	virtual bool IsDeviceGroupFlagSet( EAutomationDeviceGroupTypes::Type InDeviceGroup ) const override;
	virtual void ToggleDeviceGroupFlag( EAutomationDeviceGroupTypes::Type InDeviceGroup ) override;
	virtual void UpdateDeviceGroups() override;

	virtual FString GetReportOutputPath() const override;

	// Checkpoint logic
	virtual TArray<FString> GetCheckpointFileContents() override;

	virtual FArchive* GetCheckpointFileForWrite() override;

	virtual void CleanUpCheckpointFile() override;

	virtual void WriteLoadedCheckpointDataToFile() override;

	virtual void WriteLineToCheckpointFile(FString LineToWrite) override;

	virtual void ResetAutomationTestTimeout(const TCHAR* Reason) override;
	
protected:

	/**
	 * Adds a ping result from a running test.
	 *
	 * @param ResponderAddress The address of the message endpoint that responded to a ping.
	 */
	void AddPingResult( const FMessageAddress& ResponderAddress );

	/**
	* Spew all of our results of the test out to the log.
	*/
	void ReportTestResults();

	/**
	 * Create a json file that contains all of our test report data at /saved/automation/logs/AutomationReport-{CL}-{DateTime}.json
	 */
	bool GenerateJsonTestPassSummary(const FAutomatedTestPassResults& SerializedPassResults, FDateTime Timestamp);

	/**
	 * Generates a full html report of the testing, which may include links to images.  All of it will be bundled under a folder.
	 */
	bool GenerateHtmlTestPassSummary(const FAutomatedTestPassResults& SerializedPassResults, FDateTime Timestamp);

	/**
	* Gather all info, warning, and error lines generated over the course of a test.
	*
	* @param TestName The test that was just run.
	* @param TestResult All of the messages of note generated during the test case.
	*/
	void CollectTestResults(TSharedPtr<IAutomationReport> Report, const FAutomationTestResults& Results);

	/**
	 * Checks the child result.
	 *
	 * @param InReport The child result to check.
	 */
	void CheckChildResult( TSharedPtr< IAutomationReport > InReport );

	FString SlugString(const FString& DisplayString) const;

	FString CopyArtifact(const FString& DestFolder, const FString& SourceFile) const;

	/**
	 * Execute the next task thats available.
	 *
	 * @param ClusterIndex The Cluster index of the device type we intend to use.
	 * @param bAllTestsCompleted Whether all tests have been completed.
	 */
	void ExecuteNextTask( int32 ClusterIndex, OUT bool& bAllTestsCompleted );

	/** Process the comparison queue to see if there are comparisons we need to respond to the test with. */
	void ProcessComparisonQueue();

	/** Distributes any tests that are pending and deal with tests finishing. */
	void ProcessAvailableTasks();

	/** Processes the results after tests are complete. */
	void ProcessResults();

	/**
	 * Removes the test info.
	 *
	 * @param TestToRemove The test to remove.
	 */
	void RemoveTestRunning( const FMessageAddress& TestToRemove );

	/** Changes the controller state. */
	void SetControllerStatus( EAutomationControllerModuleState::Type AutomationTestState );

	/** Stores the tests that are valid for a particular device classification. */
	void SetTestNames(const FMessageAddress& AutomationWorkerAddress, TArray<FAutomationTestInfo>& TestInfo);

	/** Updates the tests to ensure they are all still running. */
	void UpdateTests();

private:

	/** Handles FAutomationWorkerFindWorkersResponse messages. */
	void HandleFindWorkersResponseMessage( const FAutomationWorkerFindWorkersResponse& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context );

	/** Handles FAutomationWorkerPong messages. */
	void HandlePongMessage( const FAutomationWorkerPong& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context );

	/** Handles FAutomationWorkerScreenImage messages. */
	void HandleReceivedScreenShot( const FAutomationWorkerScreenImage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context );

	/** Handles FAutomationWorkerTestDataRequest messages. */
	void HandleTestDataRequest(const FAutomationWorkerTestDataRequest& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Handles FAutomationWorkerPerformanceDataRequest messages. */
	void HandlePerformanceDataRequest(const FAutomationWorkerPerformanceDataRequest& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Handles FAutomationWorkerRequestNextNetworkCommand messages. */
	void HandleRequestNextNetworkCommandMessage( const FAutomationWorkerRequestNextNetworkCommand& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context );

	/** Handles FAutomationWorkerRequestTestsReplyComplete messages. */
	void HandleRequestTestsReplyCompleteMessage(const FAutomationWorkerRequestTestsReplyComplete& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Handles FAutomationWorkerRunTestsReply messages. */
	void HandleRunTestsReplyMessage( const FAutomationWorkerRunTestsReply& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context );

	/** Handles FAutomationWorkerWorkerOffline messages. */
	void HandleWorkerOfflineMessage( const FAutomationWorkerWorkerOffline& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context );

private:

	/** Session this controller is currently communicating with */
	FGuid ActiveSessionId;

	/** The automation test state */
	EAutomationControllerModuleState::Type AutomationTestState;

	/** Which grouping flags are enabled */
	uint32 DeviceGroupFlags;

	/** Whether to include developer content in the automation tests */
	bool bDeveloperDirectoryIncluded;

	/** Some tests have errors */
	bool bHasErrors;

	/** Some tests have warnings */
	bool bHasWarning;

	/** Some tests have logs */
	bool bHasLogs;

	/** Is this a local session */
	bool bIsLocalSession;

	/** Are tests results available */
	bool bTestResultsAvailable;

	/** Which sets of tests to consider */
	uint32 RequestedTestFlags;

	/** Timer to keep track of the last time tests were updated */
	double CheckTestTimer;

	/** Whether tick is still executing tests for different clusters */
	uint32 ClusterDistributionMask;

	/** Available worker GUIDs */
	FAutomationDeviceClusterManager DeviceClusterManager;

	/** The iteration number of executing the tests.  Ensures restarting the tests won't allow stale results to try and commit */
	uint32 ExecutionCount;

	/** Last time the update tests function was ticked */
	double LastTimeUpdateTicked;

	/** Holds the messaging endpoint. */
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;

	/** Counter for number of workers we have received responses from for Refreshing the Test List */
	uint32 RefreshTestResponses;

	/** Available stats/status for all tests. */
	FAutomationReportManager ReportManager;

	/** A data holder to keep track of how long tests have been running. */
	struct FTestRunningInfo
	{
		FTestRunningInfo( FMessageAddress InMessageAddress ):
			OwnerMessageAddress( InMessageAddress),
			LastPingTime( 0.f )
		{
		}
		/** The test runners message address */
		FMessageAddress OwnerMessageAddress;
		/** The time since we had a ping from the instance*/
		float LastPingTime;
	};

	/** A array of running tests. */
	TArray< FTestRunningInfo > TestRunningArray;

	/** The number of test passes to perform. */
	int32 NumTestPasses;

	/** The current test pass we are on. */
	int32 CurrentTestPass;

	/** If we should send result to analytics */
	bool bSendAnalytics;

	/** The list of results generated by our test pass. */
	FAutomatedTestPassResults OurPassResults;

	/** The screenshot manager. */
	IScreenShotManagerPtr ScreenshotManager;

	struct FComparisonEntry
	{
		FMessageAddress Sender;
		FString Name;
		TFuture<FImageComparisonResult> PendingComparison;
	};

	/** Pending image comparisons */
	TQueue<TSharedPtr<FComparisonEntry>> ComparisonQueue;

	/** The report folder override path that may have been provided over the commandline, -ReportOutputPath="" */
	FString ReportOutputPath;

	FString DeveloperReportUrl;

	// TODO Put into struct.
	// Checkpoint variables
	//Test pass checkpoint backup file.
	FArchive* CheckpointFile;

	FString CheckpointCommand;

	TArray<FString> TestsRun;

private:

	/** Holds a delegate that is invoked when the controller shuts down. */
	FOnAutomationControllerManagerShutdown ShutdownDelegate;

	/** Holds a delegate that is invoked when the controller has tests available. */
	FOnAutomationControllerManagerTestsAvailable TestsAvailableDelegate;

	/** Holds a delegate that is invoked when the controller's tests are being refreshed. */
	FOnAutomationControllerTestsRefreshed TestsRefreshedDelegate;

	/** Holds a delegate that is invoked when the controller's reset. */
	FOnAutomationControllerReset ControllerResetDelegate;	

	/** Holds a delegate that is invoked when the tests have completed. */
	FOnAutomationControllerTestsComplete TestsCompleteDelegate;
};
