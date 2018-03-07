// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "IAutomationReport.h"
#include "AutomationWorkerMessages.h"
#include "IMessageContext.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"
#include "Modules/ModuleManager.h"
#include "AssetEditorMessages.h"
#include "ImageComparer.h"
#include "AutomationControllerManager.h"
#include "Interfaces/IScreenShotToolsModule.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "PlatformHttp.h"

#if WITH_EDITOR
#include "Logging/MessageLog.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(AutomationControllerLog, Log, All)

FAutomationControllerManager::FAutomationControllerManager()
{
	CheckpointFile = nullptr;

	if ( !FParse::Value(FCommandLine::Get(), TEXT("ReportOutputPath="), ReportOutputPath, false) )
	{
		if ( FParse::Value(FCommandLine::Get(), TEXT("DeveloperReportOutputPath="), ReportOutputPath, false) )
		{
			ReportOutputPath = ReportOutputPath / TEXT("dev") / FString(FPlatformProcess::UserName()).ToLower();
		}
	}

	if ( FParse::Value(FCommandLine::Get(), TEXT("DeveloperReportUrl="), DeveloperReportUrl, false) )
	{
		DeveloperReportUrl = DeveloperReportUrl / TEXT("dev") / FString(FPlatformProcess::UserName()).ToLower() / TEXT("index.html");
	}
}

void FAutomationControllerManager::RequestAvailableWorkers(const FGuid& SessionId)
{
	//invalidate previous tests
	++ExecutionCount;
	DeviceClusterManager.Reset();

	ControllerResetDelegate.Broadcast();

	// Don't allow reports to be exported
	bTestResultsAvailable = false;

	//store off active session ID to reject messages that come in from different sessions
	ActiveSessionId = SessionId;

	//EAutomationTestFlags::FilterMask

	//TODO AUTOMATION - include change list, game, etc, or remove when launcher is integrated
	int32 ChangelistNumber = 10000;
	FString ProcessName = TEXT("instance_name");

	MessageEndpoint->Publish(new FAutomationWorkerFindWorkers(ChangelistNumber, FApp::GetProjectName(), ProcessName, SessionId), EMessageScope::Network);

	// Reset the check test timers
	LastTimeUpdateTicked = FPlatformTime::Seconds();
	CheckTestTimer = 0.f;

	IScreenShotToolsModule& ScreenShotModule = FModuleManager::LoadModuleChecked<IScreenShotToolsModule>("ScreenShotComparisonTools");
	ScreenshotManager = ScreenShotModule.GetScreenShotManager();
}

void FAutomationControllerManager::RequestTests()
{
	//invalidate incoming results
	ExecutionCount++;
	//reset the number of responses we have received
	RefreshTestResponses = 0;

	ReportManager.Empty();

	for ( int32 ClusterIndex = 0; ClusterIndex < DeviceClusterManager.GetNumClusters(); ++ClusterIndex )
	{
		int32 DevicesInCluster = DeviceClusterManager.GetNumDevicesInCluster(ClusterIndex);
		if ( DevicesInCluster > 0 )
		{
			FMessageAddress MessageAddress = DeviceClusterManager.GetDeviceMessageAddress(ClusterIndex, 0);

			//issue tests on appropriate platforms
			MessageEndpoint->Send(new FAutomationWorkerRequestTests(bDeveloperDirectoryIncluded, RequestedTestFlags), MessageAddress);
		}
	}
}

void FAutomationControllerManager::RunTests(const bool bInIsLocalSession)
{
	ExecutionCount++;
	CurrentTestPass = 0;
	ReportManager.SetCurrentTestPass(CurrentTestPass);
	ClusterDistributionMask = 0;
	bTestResultsAvailable = false;
	TestRunningArray.Empty();
	bIsLocalSession = bInIsLocalSession;

	// Reset the check test timers
	LastTimeUpdateTicked = FPlatformTime::Seconds();
	CheckTestTimer = 0.f;

#if WITH_EDITOR
	FMessageLog AutomationTestingLog("AutomationTestingLog");
	FString NewPageName = FString::Printf(TEXT("-----Test Run %d----"), ExecutionCount);
	FText NewPageNameText = FText::FromString(*NewPageName);
	AutomationTestingLog.Open();
	AutomationTestingLog.NewPage(NewPageNameText);
	AutomationTestingLog.Info(NewPageNameText);
#endif
	//reset all tests
	ReportManager.ResetForExecution(NumTestPasses);

	for ( int32 ClusterIndex = 0; ClusterIndex < DeviceClusterManager.GetNumClusters(); ++ClusterIndex )
	{
		//enable each device cluster
		ClusterDistributionMask |= ( 1 << ClusterIndex );

		//for each device in this cluster
		for ( int32 DeviceIndex = 0; DeviceIndex < DeviceClusterManager.GetNumDevicesInCluster(ClusterIndex); ++DeviceIndex )
		{
			//mark the device as idle
			DeviceClusterManager.SetTest(ClusterIndex, DeviceIndex, NULL);

			// Send command to reset tests (delete local files, etc)
			FMessageAddress MessageAddress = DeviceClusterManager.GetDeviceMessageAddress(ClusterIndex, DeviceIndex);
			MessageEndpoint->Send(new FAutomationWorkerResetTests(), MessageAddress);
		}
	}

	// Inform the UI we are running tests
	if ( ClusterDistributionMask != 0 )
	{
		SetControllerStatus(EAutomationControllerModuleState::Running);
	}
}

void FAutomationControllerManager::StopTests()
{
	bTestResultsAvailable = false;
	ClusterDistributionMask = 0;

	ReportManager.StopRunningTests();

	// Inform the UI we have stopped running tests
	if ( DeviceClusterManager.HasActiveDevice() )
	{
		SetControllerStatus(EAutomationControllerModuleState::Ready);
	}
	else
	{
		SetControllerStatus(EAutomationControllerModuleState::Disabled);
	}

	TestRunningArray.Empty();
}

void FAutomationControllerManager::Init()
{
	extern void EmptyLinkFunctionForStaticInitializationAutomationExecCmd();
	EmptyLinkFunctionForStaticInitializationAutomationExecCmd();

	AutomationTestState = EAutomationControllerModuleState::Disabled;
	bTestResultsAvailable = false;
	bSendAnalytics = FParse::Param(FCommandLine::Get(), TEXT("SendAutomationAnalytics"));
}

void FAutomationControllerManager::RequestLoadAsset(const FString& InAssetName)
{
	MessageEndpoint->Publish(new FAssetEditorRequestOpenAsset(InAssetName), EMessageScope::Process);
}

void FAutomationControllerManager::Tick()
{
	ProcessAvailableTasks();
	ProcessComparisonQueue();
}

void FAutomationControllerManager::ProcessComparisonQueue()
{
	TSharedPtr<FComparisonEntry> Entry;
	if ( ComparisonQueue.Peek(Entry) )
	{
		if ( Entry->PendingComparison.IsReady() )
		{
			const bool Dequeued = ComparisonQueue.Dequeue(Entry);
			check(Dequeued);

			FImageComparisonResult Result = Entry->PendingComparison.Get();

			// Send the message back to the automation worker letting it know the results of the comparison test.
			{
				FAutomationWorkerImageComparisonResults* Message = new FAutomationWorkerImageComparisonResults(
					Result.IsNew(), Result.AreSimilar(), Result.MaxLocalDifference, Result.GlobalDifference, Result.ErrorMessage.ToString());

				MessageEndpoint->Send(Message, Entry->Sender);
			}

			// Find the game session instance info
			int32 ClusterIndex;
			int32 DeviceIndex;
			verify(DeviceClusterManager.FindDevice(Entry->Sender, ClusterIndex, DeviceIndex));

			// Get the current test.
			TSharedPtr<IAutomationReport> Report = DeviceClusterManager.GetTest(ClusterIndex, DeviceIndex);
			if (Report.IsValid())
			{
				// Record the artifacts for the test.
				FString ApprovedFolder = ScreenshotManager->GetLocalApprovedFolder();
				FString UnapprovedFolder = ScreenshotManager->GetLocalUnapprovedFolder();
				FString ComparisonFolder = ScreenshotManager->GetLocalComparisonFolder();

				TMap<FString, FString> LocalFiles;
				LocalFiles.Add(TEXT("approved"), ApprovedFolder / Result.ApprovedFile);
				LocalFiles.Add(TEXT("unapproved"), UnapprovedFolder / Result.IncomingFile);
				LocalFiles.Add(TEXT("difference"), ComparisonFolder / Result.ComparisonFile);

				Report->AddArtifact(ClusterIndex, CurrentTestPass, FAutomationArtifact(Entry->Name, EAutomationArtifactType::Comparison, LocalFiles));
			}
			else
			{
				UE_LOG(AutomationControllerLog, Error, TEXT("Cannot generate screenshot report for screenshot %s as report is missing"), *Result.IncomingFile);
			}
		}
	}
}

void FAutomationControllerManager::ProcessAvailableTasks()
{
	// Distribute tasks
	if ( ClusterDistributionMask != 0 )
	{
		// For each device cluster
		for ( int32 ClusterIndex = 0; ClusterIndex < DeviceClusterManager.GetNumClusters(); ++ClusterIndex )
		{
			bool bAllTestsComplete = true;

			// If any of the devices were valid
			if ( ( ClusterDistributionMask & ( 1 << ClusterIndex ) ) && DeviceClusterManager.GetNumDevicesInCluster(ClusterIndex) > 0 )
			{
				ExecuteNextTask(ClusterIndex, bAllTestsComplete);
			}

			//if we're all done running our tests
			if ( bAllTestsComplete )
			{
				//we don't need to test this cluster anymore
				ClusterDistributionMask &= ~( 1 << ClusterIndex );

				if ( ClusterDistributionMask == 0 )
				{
					ProcessResults();
					//Notify the graphical layout we are done processing results.
					TestsCompleteDelegate.Broadcast();
				}
			}
		}
	}

	if ( bIsLocalSession == false )
	{
		// Update the test status for timeouts if this is not a local session
		UpdateTests();
	}
}

void FAutomationControllerManager::ReportTestResults()
{
	GLog->Logf(TEXT("Test Pass Results:"));
	for ( int32 i = 0; i < OurPassResults.Tests.Num(); i++ )
	{
		GLog->Logf(TEXT("%s: %s"), *OurPassResults.Tests[i].TestDisplayName, ToString(OurPassResults.Tests[i].State));
	}
}

void FAutomationControllerManager::CollectTestResults(TSharedPtr<IAutomationReport> Report, const FAutomationTestResults& Results)
{
	// TODO This is slow, change to a map.
	for ( int32 i = 0; i < OurPassResults.Tests.Num(); i++ )
	{
		FAutomatedTestResult& ReportResult = OurPassResults.Tests[i];
		if ( ReportResult.FullTestPath == Report->GetFullTestPath() )
		{
			ReportResult.SetEvents(Results.GetEvents(), Results.GetWarningTotal(), Results.GetErrorTotal());
			ReportResult.State = Results.State;
			ReportResult.Artifacts = Results.Artifacts;

			switch ( Results.State )
			{
			case EAutomationState::Success:
				if ( Results.GetWarningTotal() > 0 )
				{
					OurPassResults.SucceededWithWarnings++;
				}
				else
				{
					OurPassResults.Succeeded++;
				}
				break;
			case EAutomationState::Fail:
				OurPassResults.Failed++;
				break;
			default:
				OurPassResults.NotRun++;
				break;
			}

			OurPassResults.TotalDuration += Results.Duration;

			return;
		}
	}
}

bool FAutomationControllerManager::GenerateJsonTestPassSummary(const FAutomatedTestPassResults& SerializedPassResults, FDateTime Timestamp)
{
	FString Json;
	if ( FJsonObjectConverter::UStructToJsonObjectString(SerializedPassResults, Json) )
	{
		FString ReportFileName = FString::Printf(TEXT("%s/index.json"), *ReportOutputPath);
		if ( FFileHelper::SaveStringToFile(Json, *ReportFileName, FFileHelper::EEncodingOptions::ForceUTF8) )
		{
			return true;
		}
	}
	
	UE_LOG(AutomationControllerLog, Warning, TEXT("Test Report Json is invalid - report not generated."));
	return false;
}

bool FAutomationControllerManager::GenerateHtmlTestPassSummary(const FAutomatedTestPassResults& SerializedPassResults, FDateTime Timestamp)
{
	FString ReportTemplate;
	const bool bLoadedResult = FFileHelper::LoadFileToString(ReportTemplate, *( FPaths::EngineContentDir() / TEXT("Automation/Report-Template.html") ));

	if ( bLoadedResult )
	{
		FString ReportFileName = FString::Printf(TEXT("%s/index.html"), *ReportOutputPath);
		if ( FFileHelper::SaveStringToFile(ReportTemplate, *ReportFileName, FFileHelper::EEncodingOptions::ForceUTF8) )
		{
			return true;
		}
	}
	
	UE_LOG(AutomationControllerLog, Warning, TEXT("Test Report Html is invalid - report not generated."));
	return false;
}

FString FAutomationControllerManager::SlugString(const FString& DisplayString) const
{
	FString GeneratedName = DisplayString;

	// Convert the display label, which may consist of just about any possible character, into a
	// suitable name for a UObject (remove whitespace, certain symbols, etc.)
	{
		for ( int32 BadCharacterIndex = 0; BadCharacterIndex < ARRAY_COUNT(INVALID_OBJECTNAME_CHARACTERS) - 1; ++BadCharacterIndex )
		{
			const TCHAR TestChar[2] = { INVALID_OBJECTNAME_CHARACTERS[BadCharacterIndex], 0 };
			const int32 NumReplacedChars = GeneratedName.ReplaceInline(TestChar, TEXT(""));
		}
	}

	return GeneratedName;
}

FString FAutomationControllerManager::CopyArtifact(const FString& DestFolder, const FString& SourceFile) const
{
	FString ArtifactFile = FString(TEXT("artifacts")) / FGuid::NewGuid().ToString(EGuidFormats::Digits) + FPaths::GetExtension(SourceFile, true);
	FString ArtifactDestination = DestFolder / ArtifactFile;
	IFileManager::Get().Copy(*ArtifactDestination, *SourceFile, true, true);

	return ArtifactFile;
}

FString FAutomationControllerManager::GetReportOutputPath() const
{
	return ReportOutputPath;
}

void FAutomationControllerManager::ExecuteNextTask( int32 ClusterIndex, OUT bool& bAllTestsCompleted )
{
	bool bTestThatRequiresMultiplePraticipantsHadEnoughParticipants = false;
	TArray< IAutomationReportPtr > TestsRunThisPass;

	// For each device in this cluster
	int32 NumDevicesInCluster = DeviceClusterManager.GetNumDevicesInCluster( ClusterIndex );
	for ( int32 DeviceIndex = 0; DeviceIndex < NumDevicesInCluster; ++DeviceIndex )
	{
		// If this device is idle
		if ( !DeviceClusterManager.GetTest(ClusterIndex, DeviceIndex).IsValid() && DeviceClusterManager.DeviceEnabled(ClusterIndex, DeviceIndex) )
		{
			// Get the next test that should be worked on
			TSharedPtr< IAutomationReport > NextTest = ReportManager.GetNextReportToExecute(bAllTestsCompleted, ClusterIndex, CurrentTestPass, NumDevicesInCluster);
			if ( NextTest.IsValid() )
			{
				// Get the status of the test
				EAutomationState TestState = NextTest->GetState(ClusterIndex, CurrentTestPass);
				if ( TestState == EAutomationState::NotRun )
				{
					// Reserve this device for the test
					DeviceClusterManager.SetTest(ClusterIndex, DeviceIndex, NextTest);
					TestsRunThisPass.Add(NextTest);

					// Register this as a test we'll need to report on.
					FAutomatedTestResult tempresult;
					tempresult.Test = NextTest;
					tempresult.TestDisplayName = NextTest->GetDisplayName();
					tempresult.FullTestPath = NextTest->GetFullTestPath();

					OurPassResults.Tests.Add(tempresult);

					// If we now have enough devices reserved for the test, run it!
					TArray<FMessageAddress> DeviceAddresses = DeviceClusterManager.GetDevicesReservedForTest(ClusterIndex, NextTest);
					if ( DeviceAddresses.Num() == NextTest->GetNumParticipantsRequired() )
					{
						// Send it to each device
						for ( int32 AddressIndex = 0; AddressIndex < DeviceAddresses.Num(); ++AddressIndex )
						{
							FAutomationTestResults TestResults;

							GLog->Logf(ELogVerbosity::Display, TEXT("Running Automation: '%s' (Class Name: '%s')"), *TestsRunThisPass[AddressIndex]->GetFullTestPath(), *TestsRunThisPass[AddressIndex]->GetCommand());
							TestResults.State = EAutomationState::InProcess;

							if (CheckpointFile)
							{
								WriteLineToCheckpointFile(NextTest->GetFullTestPath());
							}

							TestResults.GameInstance = DeviceClusterManager.GetClusterDeviceName(ClusterIndex, DeviceIndex);
							NextTest->SetResults(ClusterIndex, CurrentTestPass, TestResults);
							NextTest->ResetNetworkCommandResponses();

							// Mark the device as busy
							FMessageAddress DeviceAddress = DeviceAddresses[AddressIndex];

							// Send the test to the device for execution!
							MessageEndpoint->Send(new FAutomationWorkerRunTests(ExecutionCount, AddressIndex, NextTest->GetCommand(), NextTest->GetDisplayName(), bSendAnalytics), DeviceAddress);

							// Add a test so we can check later if the device is still active
							TestRunningArray.Add(FTestRunningInfo(DeviceAddress));
						}
					}
				}
			}
		}
		else
		{
			// At least one device is still working
			bAllTestsCompleted = false;
		}
	}

	// Ensure any tests we have attempted to run on this pass had enough participants to successfully run.
	for ( int32 TestIndex = 0; TestIndex < TestsRunThisPass.Num(); TestIndex++ )
	{
		IAutomationReportPtr CurrentTest = TestsRunThisPass[TestIndex];

		if ( CurrentTest->GetNumDevicesRunningTest() != CurrentTest->GetNumParticipantsRequired() )
		{
			if ( GetNumDevicesInCluster(ClusterIndex) < CurrentTest->GetNumParticipantsRequired() )
			{
				FAutomationTestResults TestResults;
				TestResults.State = EAutomationState::NotEnoughParticipants;
				TestResults.GameInstance = DeviceClusterManager.GetClusterDeviceName(ClusterIndex, 0);
				TestResults.AddEvent(FAutomationEvent(EAutomationEventType::Warning, FString::Printf(TEXT("Needed %d devices to participate, Only had %d available."), CurrentTest->GetNumParticipantsRequired(), DeviceClusterManager.GetNumDevicesInCluster(ClusterIndex))));

				CurrentTest->SetResults(ClusterIndex, CurrentTestPass, TestResults);
				DeviceClusterManager.ResetAllDevicesRunningTest(ClusterIndex, CurrentTest);
			}
		}
	}

	//Check to see if we finished a pass
	if ( bAllTestsCompleted && CurrentTestPass < NumTestPasses - 1 )
	{
		CurrentTestPass++;
		ReportManager.SetCurrentTestPass(CurrentTestPass);
		bAllTestsCompleted = false;
	}
}


void FAutomationControllerManager::Startup()
{
	MessageEndpoint = FMessageEndpoint::Builder("FAutomationControllerModule")
		.Handling<FAutomationWorkerFindWorkersResponse>(this, &FAutomationControllerManager::HandleFindWorkersResponseMessage)
		.Handling<FAutomationWorkerPong>(this, &FAutomationControllerManager::HandlePongMessage)
		.Handling<FAutomationWorkerRequestNextNetworkCommand>(this, &FAutomationControllerManager::HandleRequestNextNetworkCommandMessage)
		.Handling<FAutomationWorkerRequestTestsReplyComplete>(this, &FAutomationControllerManager::HandleRequestTestsReplyCompleteMessage)
		.Handling<FAutomationWorkerRunTestsReply>(this, &FAutomationControllerManager::HandleRunTestsReplyMessage)
		.Handling<FAutomationWorkerScreenImage>(this, &FAutomationControllerManager::HandleReceivedScreenShot)
		.Handling<FAutomationWorkerTestDataRequest>(this, &FAutomationControllerManager::HandleTestDataRequest)
		.Handling<FAutomationWorkerWorkerOffline>(this, &FAutomationControllerManager::HandleWorkerOfflineMessage);

	if ( MessageEndpoint.IsValid() )
	{
		MessageEndpoint->Subscribe<FAutomationWorkerWorkerOffline>();
	}

	ClusterDistributionMask = 0;
	ExecutionCount = 0;
	bDeveloperDirectoryIncluded = false;
	RequestedTestFlags = EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::PerfFilter;

	NumTestPasses = 1;

	//Default to machine name
	DeviceGroupFlags = 0;
	ToggleDeviceGroupFlag(EAutomationDeviceGroupTypes::MachineName);
}

void FAutomationControllerManager::Shutdown()
{
	MessageEndpoint.Reset();
	ShutdownDelegate.Broadcast();
	RemoveCallbacks();
}

void FAutomationControllerManager::RemoveCallbacks()
{
	ShutdownDelegate.Clear();
	TestsAvailableDelegate.Clear();
	TestsRefreshedDelegate.Clear();
	TestsCompleteDelegate.Clear();
}

void FAutomationControllerManager::SetTestNames(const FMessageAddress& AutomationWorkerAddress, TArray<FAutomationTestInfo>& TestInfo)
{
	int32 DeviceClusterIndex = INDEX_NONE;
	int32 DeviceIndex = INDEX_NONE;

	// Find the device that requested these tests
	if ( DeviceClusterManager.FindDevice(AutomationWorkerAddress, DeviceClusterIndex, DeviceIndex) )
	{
		// Sort tests by display name
		struct FCompareAutomationTestInfo
		{
			FORCEINLINE bool operator()(const FAutomationTestInfo& A, const FAutomationTestInfo& B) const
			{
				return A.GetDisplayName() < B.GetDisplayName();
			}
		};

		TestInfo.Sort(FCompareAutomationTestInfo());

		// Add each test to the collection
		for ( int32 TestIndex = 0; TestIndex < TestInfo.Num(); ++TestIndex )
		{
			// Ensure our test exists. If not, add it
			ReportManager.EnsureReportExists(TestInfo[TestIndex], DeviceClusterIndex, NumTestPasses);
		}
	}
	else
	{
		//todo automation - make sure to report error if the device was not discovered correctly
	}

	// Note the response
	RefreshTestResponses++;

	// If we have received all the responses we expect to
	if ( RefreshTestResponses == DeviceClusterManager.GetNumClusters() )
	{
		TestsRefreshedDelegate.Broadcast();
	}
}

void FAutomationControllerManager::ProcessResults()
{
	bHasErrors = false;
	bHasWarning = false;
	bHasLogs = false;

	TArray< TSharedPtr< IAutomationReport > >& TestReports = GetReports();

	if ( TestReports.Num() )
	{
		bTestResultsAvailable = true;

		for ( int32 Index = 0; Index < TestReports.Num(); Index++ )
		{
			CheckChildResult(TestReports[Index]);
		}
	}

	if ( !ReportOutputPath.IsEmpty() )
	{
		FDateTime Timestamp = FDateTime::Now();

		UE_LOG(AutomationControllerLog, Display, TEXT("Generating Automation Report @ %s."), *ReportOutputPath);

		if ( IFileManager::Get().DirectoryExists(*ReportOutputPath) )
		{
			UE_LOG(AutomationControllerLog, Display, TEXT("Existing report directory found, deleting %s."), *ReportOutputPath);

			// Clear the old report folder.  Why move it first?  Because RemoveDirectory
			// is actually an async call that is not immediately carried out by the Windows OS; Moving a directory on the other hand, is sync.
			// So we move, to a temporary location, then delete it.
			FString TempDirectory = FPaths::GetPath(ReportOutputPath) + TEXT("\\") + FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
			IFileManager::Get().Move(*TempDirectory, *ReportOutputPath);
			IFileManager::Get().DeleteDirectory(*TempDirectory, false, true);
		}
				
		FScreenshotExportResults ExportResults = ScreenshotManager->ExportComparisonResultsAsync(ReportOutputPath).Get();

		FAutomatedTestPassResults SerializedPassResults = OurPassResults;

		SerializedPassResults.ComparisonExported = ExportResults.Success;
		SerializedPassResults.ComparisonExportDirectory = ExportResults.ExportPath;

		{
			SerializedPassResults.Tests.StableSort([] (const FAutomatedTestResult& A, const FAutomatedTestResult& B) {
				if ( A.GetErrorTotal() > 0 )
				{
					if ( B.GetErrorTotal() > 0 )
						return ( A.FullTestPath < B.FullTestPath );
					else
						return true;
				}
				else if ( B.GetErrorTotal() > 0 )
				{
					return false;
				}

				if ( A.GetWarningTotal() > 0 )
				{
					if ( B.GetWarningTotal() > 0 )
						return ( A.FullTestPath < B.FullTestPath );
					else
						return true;
				}
				else if ( B.GetWarningTotal() > 0 )
				{
					return false;
				}

				return A.FullTestPath < B.FullTestPath;
			});

			for ( FAutomatedTestResult& Test : SerializedPassResults.Tests )
			{
				for ( FAutomationArtifact& Artifact : Test.Artifacts )
				{
					for ( const auto& Entry : Artifact.LocalFiles )
					{
						Artifact.Files.Add(Entry.Key, CopyArtifact(ReportOutputPath, Entry.Value));
					}
				}
			}
		}

		UE_LOG(AutomationControllerLog, Display, TEXT("Writing reports... %s."), *ReportOutputPath);

		// Generate Json
		GenerateJsonTestPassSummary(SerializedPassResults, Timestamp);

		// Generate Html
		GenerateHtmlTestPassSummary(SerializedPassResults, Timestamp);

		if ( !DeveloperReportUrl.IsEmpty() )
		{
			UE_LOG(AutomationControllerLog, Display, TEXT("Launching Report URL %s."), *DeveloperReportUrl);

			FPlatformProcess::LaunchURL(*DeveloperReportUrl, nullptr, nullptr);
		}

		UE_LOG(AutomationControllerLog, Display, TEXT("Done writing reports... %s."), *ReportOutputPath);
	}

	// Then clean our array for the next pass.
	OurPassResults.ClearAllEntries();
	CleanUpCheckpointFile();

	SetControllerStatus(EAutomationControllerModuleState::Ready);
}

void FAutomationControllerManager::CheckChildResult(TSharedPtr<IAutomationReport> InReport)
{
	TArray<TSharedPtr<IAutomationReport> >& ChildReports = InReport->GetChildReports();

	if ( ChildReports.Num() > 0 )
	{
		for ( int32 Index = 0; Index < ChildReports.Num(); Index++ )
		{
			CheckChildResult(ChildReports[Index]);
		}
	}
	else if ( ( bHasErrors && bHasWarning && bHasLogs ) == false && InReport->IsEnabled() )
	{
		for ( int32 ClusterIndex = 0; ClusterIndex < GetNumDeviceClusters(); ++ClusterIndex )
		{
			FAutomationTestResults TestResults = InReport->GetResults(ClusterIndex, CurrentTestPass);

			if ( TestResults.GetErrorTotal() > 0 )
			{
				bHasErrors = true;
			}
			if ( TestResults.GetWarningTotal() )
			{
				bHasWarning = true;
			}
			if ( TestResults.GetLogTotal() )
			{
				bHasLogs = true;
			}
		}
	}
}

void FAutomationControllerManager::SetControllerStatus(EAutomationControllerModuleState::Type InAutomationTestState)
{
	if ( InAutomationTestState != AutomationTestState )
	{
		// Inform the UI if the test state has changed
		AutomationTestState = InAutomationTestState;
		TestsAvailableDelegate.Broadcast(AutomationTestState);
	}
}

void FAutomationControllerManager::RemoveTestRunning(const FMessageAddress& TestAddressToRemove)
{
	for ( int32 Index = 0; Index < TestRunningArray.Num(); Index++ )
	{
		if ( TestRunningArray[Index].OwnerMessageAddress == TestAddressToRemove )
		{
			TestRunningArray.RemoveAt(Index);
			break;
		}
	}
}

void FAutomationControllerManager::AddPingResult(const FMessageAddress& ResponderAddress)
{
	for ( int32 Index = 0; Index < TestRunningArray.Num(); Index++ )
	{
		if ( TestRunningArray[Index].OwnerMessageAddress == ResponderAddress )
		{
			TestRunningArray[Index].LastPingTime = 0;
			break;
		}
	}
}

void FAutomationControllerManager::UpdateTests()
{
	static const float CheckTestInterval = 1.0f;
	static const float GameInstanceLostTimer = 200.0f;

	CheckTestTimer += FPlatformTime::Seconds() - LastTimeUpdateTicked;
	LastTimeUpdateTicked = FPlatformTime::Seconds();
	if ( CheckTestTimer > CheckTestInterval )
	{
		for ( int32 Index = 0; Index < TestRunningArray.Num(); Index++ )
		{
			TestRunningArray[Index].LastPingTime += CheckTestTimer;

			if ( TestRunningArray[Index].LastPingTime > GameInstanceLostTimer )
			{
				// Find the game session instance info
				int32 ClusterIndex;
				int32 DeviceIndex;
				verify(DeviceClusterManager.FindDevice(TestRunningArray[Index].OwnerMessageAddress, ClusterIndex, DeviceIndex));
				//verify this device thought it was busy
				TSharedPtr <IAutomationReport> Report = DeviceClusterManager.GetTest(ClusterIndex, DeviceIndex);
				check(Report.IsValid());
				// A dummy array used to report the result

				TArray<FString> EmptyStringArray;
				TArray<FString> ErrorStringArray;
				ErrorStringArray.Add(FString(TEXT("Failed")));
				bHasErrors = true;
				GLog->Logf(ELogVerbosity::Display, TEXT("Timeout hit. Nooooooo."));

				FAutomationTestResults TestResults;
				TestResults.State = EAutomationState::Fail;
				TestResults.GameInstance = DeviceClusterManager.GetClusterDeviceName(ClusterIndex, DeviceIndex);
				TestResults.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("Timeout waiting for device %s"), *TestResults.GameInstance)));

				// Set the results
				Report->SetResults(ClusterIndex, CurrentTestPass, TestResults);
				bTestResultsAvailable = true;

				const FAutomationTestResults& FinalResults = Report->GetResults(ClusterIndex, CurrentTestPass);

				// Gather all of the data relevant to this test for our json reporting.
				CollectTestResults(Report, FinalResults);

				// Disable the device in the cluster so it is not used again
				DeviceClusterManager.DisableDevice(ClusterIndex, DeviceIndex);

				// Remove the running test
				TestRunningArray.RemoveAt(Index--);

				// If there are no more devices, set the module state to disabled
				if ( DeviceClusterManager.HasActiveDevice() == false )
				{
					// Process results first to write out the report
					ProcessResults();

					GLog->Logf(ELogVerbosity::Display, TEXT("Module disabled"));
					SetControllerStatus(EAutomationControllerModuleState::Disabled);
					ClusterDistributionMask = 0;
				}
				else
				{
					GLog->Logf(ELogVerbosity::Display, TEXT("Module not disabled. Keep looking."));
					// Remove the cluster from the mask if there are no active devices left
					if ( DeviceClusterManager.GetNumActiveDevicesInCluster(ClusterIndex) == 0 )
					{
						ClusterDistributionMask &= ~( 1 << ClusterIndex );
					}
					if ( TestRunningArray.Num() == 0 )
					{
						SetControllerStatus(EAutomationControllerModuleState::Ready);
					}
				}
			}
			else
			{
				MessageEndpoint->Send(new FAutomationWorkerPing(), TestRunningArray[Index].OwnerMessageAddress);
			}
		}
		CheckTestTimer = 0.f;
	}
}

const bool FAutomationControllerManager::ExportReport(uint32 FileExportTypeMask)
{
	return ReportManager.ExportReport(FileExportTypeMask, GetNumDeviceClusters());
}

bool FAutomationControllerManager::IsTestRunnable(IAutomationReportPtr InReport) const
{
	bool bIsRunnable = false;

	for ( int32 ClusterIndex = 0; ClusterIndex < GetNumDeviceClusters(); ++ClusterIndex )
	{
		if ( InReport->IsSupported(ClusterIndex) )
		{
			if ( GetNumDevicesInCluster(ClusterIndex) >= InReport->GetNumParticipantsRequired() )
			{
				bIsRunnable = true;
				break;
			}
		}
	}

	return bIsRunnable;
}

/* FAutomationControllerModule callbacks
 *****************************************************************************/

void FAutomationControllerManager::HandleFindWorkersResponseMessage(const FAutomationWorkerFindWorkersResponse& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if ( Message.SessionId == ActiveSessionId )
	{
		DeviceClusterManager.AddDeviceFromMessage(Context->GetSender(), Message, DeviceGroupFlags);
	}

	RequestTests();

	SetControllerStatus(EAutomationControllerModuleState::Ready);
}

void FAutomationControllerManager::HandlePongMessage( const FAutomationWorkerPong& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
	AddPingResult(Context->GetSender());
}

void FAutomationControllerManager::HandleReceivedScreenShot(const FAutomationWorkerScreenImage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	FString ScreenshotIncomingFolder = FPaths::ProjectSavedDir() / TEXT("Automation/Incoming/");

	bool bTree = true;
	FString FileName = ScreenshotIncomingFolder / Message.ScreenShotName;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FileName), bTree);
	FFileHelper::SaveArrayToFile(Message.ScreenImage, *FileName);

	// TODO Automation There is identical code in, Engine\Source\Runtime\AutomationWorker\Private\AutomationWorkerModule.cpp,
	// need to move this code into common area.

	FString Json;
	if ( FJsonObjectConverter::UStructToJsonObjectString(Message.Metadata, Json) )
	{
		FString MetadataPath = FPaths::ChangeExtension(FileName, TEXT("json"));
		FFileHelper::SaveStringToFile(Json, *MetadataPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	TSharedRef<FComparisonEntry> Comparison = MakeShareable(new FComparisonEntry());
	Comparison->Sender = Context->GetSender();
	Comparison->Name = Message.Metadata.Name;
	Comparison->PendingComparison = ScreenshotManager->CompareScreensotAsync(Message.ScreenShotName);

	ComparisonQueue.Enqueue(Comparison);
}

void FAutomationControllerManager::HandleTestDataRequest(const FAutomationWorkerTestDataRequest& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	const FString TestDataRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Test"));
	const FString DataFile = Message.DataType / Message.DataPlatform / Message.DataTestName / Message.DataName + TEXT(".json");
	const FString DataFullPath = TestDataRoot / DataFile;

	// Generate the folder for the data if it doesn't exist.
	const bool bTree = true;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(DataFile), bTree);

	bool bIsNew = true;
	FString ResponseJsonData = Message.JsonData;

	if ( FPaths::FileExists(DataFullPath) )
	{
		if ( FFileHelper::LoadFileToString(ResponseJsonData, *DataFullPath) )
		{
			bIsNew = false;
		}
		else
		{
			// TODO Error
		}
	}

	if ( bIsNew )
	{
		FString IncomingTestData = FPaths::ProjectSavedDir() / TEXT("Automation/IncomingData/") / DataFile;
		if ( FFileHelper::SaveStringToFile(Message.JsonData, *IncomingTestData) )
		{
			//TODO Anything extra to do here?
		}
		else
		{
			//TODO What do we do if this fails?
		}
	}

	FAutomationWorkerTestDataResponse* ResponseMessage = new FAutomationWorkerTestDataResponse();
	ResponseMessage->bIsNew = bIsNew;
	ResponseMessage->JsonData = ResponseJsonData;

	MessageEndpoint->Send(ResponseMessage, Context->GetSender());
}

void FAutomationControllerManager::HandlePerformanceDataRequest(const FAutomationWorkerPerformanceDataRequest& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	//TODO Read/Performance data.

	FAutomationWorkerPerformanceDataResponse* ResponseMessage = new FAutomationWorkerPerformanceDataResponse();
	ResponseMessage->bSuccess = true;
	ResponseMessage->ErrorMessage = TEXT("");

	MessageEndpoint->Send(ResponseMessage, Context->GetSender());
}

void FAutomationControllerManager::HandleRequestNextNetworkCommandMessage(const FAutomationWorkerRequestNextNetworkCommand& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	// Harvest iteration of running the tests this result came from (stops stale results from being committed to subsequent runs)
	if ( Message.ExecutionCount == ExecutionCount )
	{
		// Find the device id for the address
		int32 ClusterIndex;
		int32 DeviceIndex;

		verify(DeviceClusterManager.FindDevice(Context->GetSender(), ClusterIndex, DeviceIndex));

		// Verify this device thought it was busy
		TSharedPtr<IAutomationReport> Report = DeviceClusterManager.GetTest(ClusterIndex, DeviceIndex);
		check(Report.IsValid());

		// Increment network command responses
		bool bAllResponsesReceived = Report->IncrementNetworkCommandResponses();

		// Test if we've accumulated all responses AND this was the result for the round of test running AND we're still running tests
		if ( bAllResponsesReceived && ( ClusterDistributionMask & ( 1 << ClusterIndex ) ) )
		{
			// Reset the counter
			Report->ResetNetworkCommandResponses();

			// For every device in this networked test
			TArray<FMessageAddress> DeviceAddresses = DeviceClusterManager.GetDevicesReservedForTest(ClusterIndex, Report);
			check(DeviceAddresses.Num() == Report->GetNumParticipantsRequired());

			// Send it to each device
			for ( int32 AddressIndex = 0; AddressIndex < DeviceAddresses.Num(); ++AddressIndex )
			{
				//send "next command message" to worker
				MessageEndpoint->Send(new FAutomationWorkerNextNetworkCommandReply(), DeviceAddresses[AddressIndex]);
			}
		}
	}
}

void FAutomationControllerManager::HandleRequestTestsReplyCompleteMessage(const FAutomationWorkerRequestTestsReplyComplete& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	TArray<FAutomationTestInfo> TestInfo;
	TestInfo.Reset(Message.Tests.Num());
	for (const FAutomationWorkerSingleTestReply& SingleTestReply : Message.Tests)
	{
		FAutomationTestInfo NewTest = SingleTestReply.GetTestInfo();
		TestInfo.Add(NewTest);
	}

	SetTestNames(Context->GetSender(), TestInfo);
}

void FAutomationControllerManager::HandleRunTestsReplyMessage(const FAutomationWorkerRunTestsReply& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	// If we should commit these results
	if ( Message.ExecutionCount == ExecutionCount )
	{
		FAutomationTestResults TestResults;

		TestResults.State = Message.Success ? EAutomationState::Success : EAutomationState::Fail;
		TestResults.Duration = Message.Duration;

		// Mark device as back on the market
		int32 ClusterIndex;
		int32 DeviceIndex;

		verify(DeviceClusterManager.FindDevice(Context->GetSender(), ClusterIndex, DeviceIndex));

		TestResults.GameInstance = DeviceClusterManager.GetClusterDeviceName(ClusterIndex, DeviceIndex);
		TestResults.SetEvents(Message.Events, Message.WarningTotal, Message.ErrorTotal);

		// Verify this device thought it was busy
		TSharedPtr<IAutomationReport> Report = DeviceClusterManager.GetTest(ClusterIndex, DeviceIndex);
		check(Report.IsValid());

		Report->SetResults(ClusterIndex, CurrentTestPass, TestResults);

		const FAutomationTestResults& FinalResults = Report->GetResults(ClusterIndex, CurrentTestPass);

		// Gather all of the data relevant to this test for our json reporting.
		CollectTestResults(Report, FinalResults);

#if WITH_EDITOR
		FMessageLog AutomationTestingLog("AutomationTestingLog");
		AutomationTestingLog.Open();
#endif

		for ( const FAutomationEvent& Event : TestResults.GetEvents() )
		{
			switch ( Event.Type )
			{
			case EAutomationEventType::Info:
				GLog->Logf(ELogVerbosity::Log, TEXT("%s"), *Event.ToString());
#if WITH_EDITOR
				AutomationTestingLog.Info(FText::FromString(Event.ToString()));
#endif
				break;
			case EAutomationEventType::Warning:
				GLog->Logf(ELogVerbosity::Warning, TEXT("%s"), *Event.ToString());
#if WITH_EDITOR
				AutomationTestingLog.Warning(FText::FromString(Event.ToString()));
#endif
				break;
			case EAutomationEventType::Error:
				GLog->Logf(ELogVerbosity::Error, TEXT("%s"), *Event.ToString());
#if WITH_EDITOR
				AutomationTestingLog.Error(FText::FromString(Event.ToString()));
#endif
				break;
			}
		}
		
		if ( TestResults.State == EAutomationState::Success )
		{
			FString SuccessString = FString::Printf(TEXT("...Automation Test Succeeded (%s)"), *Report->GetDisplayName());
			GLog->Logf(ELogVerbosity::Log, TEXT("%s"), *SuccessString);
#if WITH_EDITOR
			AutomationTestingLog.Info(FText::FromString(*SuccessString));
#endif
		}
		else
		{
			FString FailureString = FString::Printf(TEXT("...Automation Test Failed (%s)"), *Report->GetDisplayName());
			GLog->Logf(ELogVerbosity::Log, TEXT("%s"), *FailureString);
#if WITH_EDITOR
			AutomationTestingLog.Error(FText::FromString(*FailureString));
#endif
			//FAutomationTestFramework::Get().Lo
		}

		// const bool TestSucceeded = (TestResults.State == EAutomationState::Success);
		//FAutomationTestFramework::Get().LogEndTestMessage(Report->GetDisplayName(), TestSucceeded);

		// Device is now good to go
		DeviceClusterManager.SetTest(ClusterIndex, DeviceIndex, NULL);
	}

	// Remove the running test
	RemoveTestRunning(Context->GetSender());
}

void FAutomationControllerManager::HandleWorkerOfflineMessage( const FAutomationWorkerWorkerOffline& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
	FMessageAddress DeviceMessageAddress = Context->GetSender();
	DeviceClusterManager.Remove(DeviceMessageAddress);
}

bool FAutomationControllerManager::IsDeviceGroupFlagSet( EAutomationDeviceGroupTypes::Type InDeviceGroup ) const
{
	const uint32 FlagMask = 1 << InDeviceGroup;
	return (DeviceGroupFlags & FlagMask) > 0;
}

void FAutomationControllerManager::ToggleDeviceGroupFlag( EAutomationDeviceGroupTypes::Type InDeviceGroup )
{
	const uint32 FlagMask = 1 << InDeviceGroup;
	DeviceGroupFlags = DeviceGroupFlags ^ FlagMask;
}

void FAutomationControllerManager::UpdateDeviceGroups( )
{
	DeviceClusterManager.ReGroupDevices( DeviceGroupFlags );

	// Update the reports in case the number of clusters changed
	int32 NumOfClusters = DeviceClusterManager.GetNumClusters();
	ReportManager.ClustersUpdated(NumOfClusters);
}

TArray<FString> FAutomationControllerManager::GetCheckpointFileContents()
{
	TestsRun.Empty();
	FString CheckpointFileName = FString::Printf(TEXT("%sautomationcheckpoint.log"), *FPaths::AutomationDir());
	if (IFileManager::Get().FileExists(*CheckpointFileName))
	{
		FString FileData;
		FFileHelper::LoadFileToString(FileData, *CheckpointFileName);
		FileData.ParseIntoArrayLines(TestsRun);
		for (int i = 0; i < TestsRun.Num(); i++)
		{
			GLog->Log(TEXT("AutomationCheckpoint"), ELogVerbosity::Log, *TestsRun[i]);
		}
	}
	return TestsRun;
}

FArchive* FAutomationControllerManager::GetCheckpointFileForWrite()
{
	if (!CheckpointFile)
	{
		FString CheckpointFileName = FString::Printf(TEXT("%sautomationcheckpoint.log"), *FPaths::AutomationDir());
		CheckpointFile = IFileManager::Get().CreateFileWriter(*CheckpointFileName, 8);
	}
	return CheckpointFile;
}

void FAutomationControllerManager::CleanUpCheckpointFile()
{
	if (CheckpointFile)
	{
		CheckpointFile->Close();
		CheckpointFile = nullptr;
	}
	FString CheckpointFileName = FString::Printf(TEXT("%sautomationcheckpoint.log"), *FPaths::AutomationDir());
	if (IFileManager::Get().FileExists(*CheckpointFileName))
	{
		IFileManager::Get().Delete(*CheckpointFileName);
	}
}

void FAutomationControllerManager::WriteLoadedCheckpointDataToFile()
{
	GetCheckpointFileForWrite();
	if (CheckpointFile)
	{
		for (int i = 0; i < TestsRun.Num(); i++)
		{
			FString LineToWrite = FString::Printf(TEXT("%s\r\n"), *TestsRun[i]);
			CheckpointFile->Serialize(TCHAR_TO_ANSI(*LineToWrite), LineToWrite.Len());
			CheckpointFile->Flush();
		}
	}
}

void FAutomationControllerManager::WriteLineToCheckpointFile(FString StringToWrite)
{
	GetCheckpointFileForWrite();
	if (CheckpointFile)
	{
		FString LineToWrite = FString::Printf(TEXT("%s\r\n"), *StringToWrite);
		CheckpointFile->Serialize(TCHAR_TO_ANSI(*LineToWrite), LineToWrite.Len());
		CheckpointFile->Flush();
	}
}

void FAutomationControllerManager::ResetAutomationTestTimeout(const TCHAR* Reason)
{
	GLog->Logf(ELogVerbosity::Display, TEXT("Resetting automation test timeout: %s"), Reason);
	LastTimeUpdateTicked = FPlatformTime::Seconds();
}
