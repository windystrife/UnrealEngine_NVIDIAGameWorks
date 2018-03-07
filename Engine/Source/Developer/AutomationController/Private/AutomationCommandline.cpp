// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Containers/Ticker.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Misc/FilterCollection.h"
#include "IAutomationControllerModule.h"
#include "FileManager.h"
#include "Paths.h"
#include "FileHelper.h"
#include "AssetRegistryModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogAutomationCommandLine, Log, All);

/** States for running the automation process */
enum class EAutomationTestState : uint8
{
	Initializing,		// 
	Idle,				// Automation process is not running
	FindWorkers,		// Find workers to run the tests
	RequestTests,		// Find the tests that can be run on the workers
	DoingRequestedWork,	// Do whatever was requested from the commandline
	Complete			// The process is finished
};

enum class EAutomationCommand : uint8
{
	ListAllTests,			//List all tests for the session
	RunCommandLineTests,	//Run only tests that are listed on the commandline
	RunCheckpointTests,		// Run only tests listed on the commandline with checkpoints in case of a crash.
	RunAll,					//Run all the tests that are supported
	RunFilter,              //
	Quit					//quit the app when tests are done
};


class FAutomationExecCmd : private FSelfRegisteringExec
{
public:
	static const float DefaultDelayTimer;
	static const float DefaultFindWorkersTimeout;

	FAutomationExecCmd()
	{
		DelayTimer = DefaultDelayTimer;
		FindWorkersTimeout = DefaultFindWorkersTimeout;
	}

	void Init()
	{
		SessionID = FApp::GetSessionId();

		// Set state to FindWorkers to kick off the testing process
		AutomationTestState = EAutomationTestState::Initializing;
		DelayTimer = DefaultDelayTimer;

		// Load the automation controller
		IAutomationControllerModule* AutomationControllerModule = &FModuleManager::LoadModuleChecked<IAutomationControllerModule>("AutomationController");
		AutomationController = AutomationControllerModule->GetAutomationController();

		AutomationController->Init();

		//TODO AUTOMATION Always use fullsize screenshots.
		const bool bFullSizeScreenshots = FParse::Param(FCommandLine::Get(), TEXT("FullSizeScreenshots"));
		const bool bSendAnalytics = FParse::Param(FCommandLine::Get(), TEXT("SendAutomationAnalytics"));

		// Register for the callback that tells us there are tests available
		AutomationController->OnTestsRefreshed().AddRaw(this, &FAutomationExecCmd::HandleRefreshTestCallback);

		TickHandler = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAutomationExecCmd::Tick));

		int32 NumTestLoops = 1;
		FParse::Value(FCommandLine::Get(), TEXT("TestLoops="), NumTestLoops);
		AutomationController->SetNumPasses(NumTestLoops);
		TestCount = 0;
		SetUpFilterMapping();
	}

	void SetUpFilterMapping()
	{
		FilterMaps.Empty();
		FilterMaps.Add("Engine", EAutomationTestFlags::EngineFilter);
		FilterMaps.Add("Smoke", EAutomationTestFlags::SmokeFilter);
		FilterMaps.Add("Stress", EAutomationTestFlags::StressFilter);
		FilterMaps.Add("Perf", EAutomationTestFlags::PerfFilter);
		FilterMaps.Add("Product", EAutomationTestFlags::ProductFilter);
	}
	
	void Shutdown()
	{
		IAutomationControllerModule* AutomationControllerModule = FModuleManager::GetModulePtr<IAutomationControllerModule>("AutomationController");
		if ( AutomationControllerModule )
		{
			AutomationController = AutomationControllerModule->GetAutomationController();
			AutomationController->OnTestsRefreshed().RemoveAll(this);
		}

		FTicker::GetCoreTicker().RemoveTicker(TickHandler);
	}

	bool IsTestingComplete()
	{
		// If the automation controller is no longer processing and we've reached the final stage of testing
		if ((AutomationController->GetTestState() != EAutomationControllerModuleState::Running) && (AutomationTestState == EAutomationTestState::Complete) && (AutomationCommandQueue.Num() == 0))
		{
			// If an actual test was ran we then will let the user know how many of them were ran.
			if (TestCount > 0)
			{
				UE_LOG(LogAutomationCommandLine, Display, TEXT("...Automation Test Queue Empty %d tests performed."), TestCount);
				TestCount = 0;
			}
			return true;
		}
		return false;
	}

	
	void GenerateTestNamesFromCommandLine(const TArray<FString>& AllTestNames, TArray<FString>& OutTestNames)
	{
		OutTestNames.Empty();
		
		//Split the test names up
		TArray<FString> Filters;
		StringCommand.ParseIntoArray(Filters, TEXT("+"), true);

		//trim cruft from all entries
		for (int32 FilterIndex = 0; FilterIndex < Filters.Num(); ++FilterIndex)
		{
			Filters[FilterIndex] = Filters[FilterIndex].TrimStart().Replace(TEXT(" "), TEXT(""));
		}

		for ( int32 TestIndex = 0; TestIndex < AllTestNames.Num(); ++TestIndex )
		{
			FString TestNamesNoWhiteSpaces = AllTestNames[TestIndex].Replace(TEXT(" "), TEXT(""));

			for ( int32 FilterIndex = 0; FilterIndex < Filters.Num(); ++FilterIndex )
			{
				if ( TestNamesNoWhiteSpaces.Contains(Filters[FilterIndex]))
				{
					OutTestNames.Add(AllTestNames[TestIndex]);
					TestCount++;
					break;
				}
			}
		}
		// If we have the TestsRun array set up and are using the same command as before, clear out already run tests. 
		if (TestsRun.Num() > 0)
		{
			if (TestsRun[0] == StringCommand)
			{
				for (int i = 1; i < TestsRun.Num(); i++)	
				{
					if (OutTestNames.Remove(TestsRun[i]))
					{
						UE_LOG(LogAutomationCommandLine, Display, TEXT("Skipping %s due to Checkpoint."), *TestsRun[i]);
					}
				}
			}
			else
			{
				AutomationController->CleanUpCheckpointFile();
			}
		}
	}

	void FindWorkers(float DeltaTime)
	{
		DelayTimer -= DeltaTime;

		if (DelayTimer <= 0)
		{
			// Request the workers
			AutomationController->RequestAvailableWorkers(SessionID);
			AutomationTestState = EAutomationTestState::RequestTests;
			FindWorkersTimeout = DefaultFindWorkersTimeout;
		}
	}

	void RequestTests(float DeltaTime)
	{
		FindWorkersTimeout -= DeltaTime;

		if (FindWorkersTimeout <= 0)
		{
			// Call the refresh callback manually
			HandleRefreshTestCallback();
		}
	}

	void HandleRefreshTestCallback()
	{
		TArray<FString> AllTestNames;
		
		if (AutomationController->GetNumDeviceClusters() == 0)
		{
			// Couldn't find any workers, go back into finding workers
			UE_LOG(LogAutomationCommandLine, Warning, TEXT("Can't find any workers! Searching again"));
			AutomationTestState = EAutomationTestState::FindWorkers;
			return;
		}

		// We have found some workers
		// Create a filter to add to the automation controller, otherwise we don't get any reports
		AutomationController->SetFilter(MakeShareable(new AutomationFilterCollection()));
		AutomationController->SetVisibleTestsEnabled(true);
		AutomationController->GetEnabledTestNames(AllTestNames);

		//assume we won't run any tests
		bool bRunTests = false;

		if (AutomationCommand == EAutomationCommand::ListAllTests)
		{
			UE_LOG(LogAutomationCommandLine, Display, TEXT("Found %d Automation Tests"), AllTestNames.Num());
			for ( const FString& TestName : AllTestNames )
			{
				UE_LOG(LogAutomationCommandLine, Display, TEXT("\t%s"), *TestName);
			}

			// Set state to complete
			AutomationTestState = EAutomationTestState::Complete;
		}
		else if (AutomationCommand == EAutomationCommand::RunCommandLineTests)
		{
			TArray<FString> FilteredTestNames;
			GenerateTestNamesFromCommandLine(AllTestNames, FilteredTestNames);
			
			UE_LOG(LogAutomationCommandLine, Display, TEXT("Found %d Automation Tests, based on '%s'."), FilteredTestNames.Num(), *StringCommand);

			for ( const FString& TestName : FilteredTestNames )
			{
				UE_LOG(LogAutomationCommandLine, Display, TEXT("\t%s"), *TestName);
			}

			if (FilteredTestNames.Num())
			{
				AutomationController->StopTests();
				AutomationController->SetEnabledTests(FilteredTestNames);
				bRunTests = true;
			}
			else
			{
				AutomationTestState = EAutomationTestState::Complete;
			}
		}
		else if (AutomationCommand == EAutomationCommand::RunCheckpointTests)
		{
			TArray <FString> FilteredTestNames;
			GenerateTestNamesFromCommandLine(AllTestNames, FilteredTestNames);
			if (FilteredTestNames.Num())
			{
				AutomationController->StopTests();
				AutomationController->SetEnabledTests(FilteredTestNames);
				if (TestsRun.Num())
				{
					AutomationController->WriteLoadedCheckpointDataToFile();
				}
				else
				{
					AutomationController->WriteLineToCheckpointFile(StringCommand);
				}
				bRunTests = true;
			}
			else
			{
				AutomationTestState = EAutomationTestState::Complete;
			}
		}
		else if (AutomationCommand == EAutomationCommand::RunFilter)
		{
			if (FilterMaps.Contains(StringCommand))
			{
				UE_LOG(LogAutomationCommandLine, Display, TEXT("Running %i Automation Tests"), AllTestNames.Num());
				AutomationController->SetEnabledTests(AllTestNames);
				bRunTests = true;
			}
			else
			{
				AutomationTestState = EAutomationTestState::Complete;
				UE_LOG(LogAutomationCommandLine, Display, TEXT("%s is not a valid flag to filter on! Valid options are: "), *StringCommand);
				TArray<FString> FlagNames;
				FilterMaps.GetKeys(FlagNames);
				for (int i = 0; i < FlagNames.Num(); i++)
				{
					UE_LOG(LogAutomationCommandLine, Display, TEXT("\t%s"), *FlagNames[i]);
				}
			}
		}
		else if (AutomationCommand == EAutomationCommand::RunAll)
		{
			bRunTests = true;
			TestCount = AllTestNames.Num();
		}

		if (bRunTests)
		{
			AutomationController->RunTests();

			// Set state to monitoring to check for test completion
			AutomationTestState = EAutomationTestState::DoingRequestedWork;
		}
	}

	void MonitorTests()
	{
		if (AutomationController->GetTestState() != EAutomationControllerModuleState::Running)
		{
			// We have finished the testing, and results are available
			AutomationTestState = EAutomationTestState::Complete;
		}
	}

	bool Tick(float DeltaTime)
	{
		// Update the automation controller to keep it running
		AutomationController->Tick();

		// Update the automation process
		switch (AutomationTestState)
		{
			case EAutomationTestState::Initializing:
			{
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				if ( AssetRegistryModule.Get().IsLoadingAssets() == false )
				{
					AutomationTestState = EAutomationTestState::Idle;
				}

				break;
			}
			case EAutomationTestState::FindWorkers:
			{
				//UE_LOG(LogAutomationCommandLine, Log, TEXT("Finding Workers..."));

				FindWorkers(DeltaTime);
				break;
			}
			case EAutomationTestState::RequestTests:
			{
				RequestTests(DeltaTime);
				break;
			}
			case EAutomationTestState::DoingRequestedWork:
			{
				MonitorTests();
				break;
			}
			case EAutomationTestState::Complete:
			case EAutomationTestState::Idle:
			default:
			{
				//pop next command
				if (AutomationCommandQueue.Num())
				{
					AutomationCommand = AutomationCommandQueue[0];
					AutomationCommandQueue.RemoveAt(0);
					if (AutomationCommand == EAutomationCommand::Quit)
					{
						if (AutomationCommandQueue.IsValidIndex(0))
						{
							// Add Quit back to the end of the array.
							AutomationCommandQueue.Add(EAutomationCommand::Quit);
							break;
						}
					}
					AutomationTestState = EAutomationTestState::FindWorkers;
				}

				// Only quit if Quit is the actual last element in the array.
				if (AutomationCommand == EAutomationCommand::Quit)
				{
					if (!GIsCriticalError)
					{
						GIsCriticalError = AutomationController->ReportsHaveErrors();
					}

					FPlatformMisc::RequestExit(true);

					// We have finished the testing, and results are available
					AutomationTestState = EAutomationTestState::Complete;
				}
				break;
			}
		}

		return !IsTestingComplete();
	}
	
	/** Console commands, see embeded usage statement **/
	virtual bool Exec(UWorld*, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		bool bHandled = false;
		// Track whether we have a flag we care about passing through.
		FString FlagToUse = "";

		// Hackiest hack to ever hack a hack to get this test running.
		if (FParse::Command(&Cmd, TEXT("RunPerfTests")))
		{
			Cmd = TEXT("Automation RunFilter Perf");
		}
		else if (FParse::Command(&Cmd, TEXT("RunProductTests")))
		{
			Cmd = TEXT("Automation RunFilter Product");
		}

		//figure out if we are handling this request
		if (FParse::Command(&Cmd, TEXT("Automation")))
		{
			StringCommand.Empty();

			TArray<FString> CommandList;
			StringCommand = Cmd;
			StringCommand.ParseIntoArray(CommandList, TEXT(";"), true);

			//assume we handle this
			Init();
			bHandled = true;

			for (int CommandIndex = 0; CommandIndex < CommandList.Num(); ++CommandIndex)
			{
				const TCHAR* TempCmd = *CommandList[CommandIndex];
				if (FParse::Command(&TempCmd, TEXT("StartRemoteSession")))
				{
					FString SessionString = TempCmd;
					if (!FGuid::Parse(SessionString, SessionID))
					{
						Ar.Logf(TEXT("%s is not a valid session guid!"), *SessionString);
						bHandled = false;
						break;
					}
				}
				else if (FParse::Command(&TempCmd, TEXT("List")))
				{
					AutomationCommandQueue.Add(EAutomationCommand::ListAllTests);
				}
				else if (FParse::Command(&TempCmd, TEXT("RunTests")) || FParse::Command(&TempCmd, TEXT("RunTest")))
				{
					if ( FParse::Command(&TempCmd, TEXT("Now")) )
					{
						DelayTimer = 0.0f;
					}

					//only one of these should be used
					StringCommand = TempCmd;
					Ar.Logf(TEXT("Automation: RunTests='%s' Queued."), *StringCommand);
					AutomationCommandQueue.Add(EAutomationCommand::RunCommandLineTests);
				}
				else if (FParse::Command(&TempCmd, TEXT("RunCheckpointedTests")))
				{
					StringCommand = TempCmd;
					Ar.Logf(TEXT("Running all tests with checkpoints matching substring: %s"), *StringCommand);
					AutomationCommandQueue.Add(EAutomationCommand::RunCheckpointTests);
					TestsRun = AutomationController->GetCheckpointFileContents();
					AutomationController->CleanUpCheckpointFile();
				}
				else if (FParse::Command(&TempCmd, TEXT("SetMinimumPriority")))
				{
					StringCommand = TempCmd;
					Ar.Logf(TEXT("Setting minimum priority of cases to run to: %s"), *StringCommand);
					if (StringCommand.Contains(TEXT("Low")))
					{
						AutomationController->SetRequestedTestFlags(EAutomationTestFlags::PriorityMask);
					}
					else if (StringCommand.Contains(TEXT("Medium")))
					{
						AutomationController->SetRequestedTestFlags(EAutomationTestFlags::MediumPriority);
					}
					else if (StringCommand.Contains(TEXT("High")))
					{
						AutomationController->SetRequestedTestFlags(EAutomationTestFlags::HighPriorityAndAbove);
					}
					else if (StringCommand.Contains(TEXT("Critical")))
					{
						AutomationController->SetRequestedTestFlags(EAutomationTestFlags::ClientContext);
					}
					else if (StringCommand.Contains(TEXT("None")))
					{
						AutomationController->SetRequestedTestFlags(0);
					}
					else
					{
						Ar.Logf(TEXT("%s is not a valid priority!\nValid priorities are Critical, High, Medium, Low, None"), *StringCommand);
					}
				}
				else if (FParse::Command(&TempCmd, TEXT("SetPriority")))
				{
					StringCommand = TempCmd;
					Ar.Logf(TEXT("Setting explicit priority of cases to run to: %s"), *StringCommand);
					if (StringCommand.Contains(TEXT("Low")))
					{
						AutomationController->SetRequestedTestFlags(EAutomationTestFlags::LowPriority);
					}
					else if (StringCommand.Contains(TEXT("Medium")))
					{
						AutomationController->SetRequestedTestFlags(EAutomationTestFlags::MediumPriority);
					}
					else if (StringCommand.Contains(TEXT("High")))
					{
						AutomationController->SetRequestedTestFlags(EAutomationTestFlags::HighPriority);
					}
					else if (StringCommand.Contains(TEXT("Critical")))
					{
						AutomationController->SetRequestedTestFlags(EAutomationTestFlags::CriticalPriority);
					}
					else if (StringCommand.Contains(TEXT("None")))
					{
						AutomationController->SetRequestedTestFlags(0);
					}

					else
					{
						Ar.Logf(TEXT("%s is not a valid priority!\nValid priorities are Critical, High, Medium, Low, None"), *StringCommand);
					}
				}
				else if (FParse::Command(&TempCmd, TEXT("RunFilter")))
				{
					FlagToUse = TempCmd;
					//only one of these should be used
					StringCommand = TempCmd;
					if (FilterMaps.Contains(FlagToUse))
					{
						AutomationController->SetRequestedTestFlags(FilterMaps[FlagToUse]);
						Ar.Logf(TEXT("Running all tests for filter: %s"), *FlagToUse);
					}
					AutomationCommandQueue.Add(EAutomationCommand::RunFilter);
				}
				else if (FParse::Command(&TempCmd, TEXT("RunAll")))
				{
					AutomationCommandQueue.Add(EAutomationCommand::RunAll);
					Ar.Logf(TEXT("Running all available automated tests for this program. NOTE: This may take a while."));
				}
				else if (FParse::Command(&TempCmd, TEXT("Quit")))
				{
					AutomationCommandQueue.Add(EAutomationCommand::Quit);
					Ar.Logf(TEXT("Automation: Quit Command Queued."));
				}
				else
				{
					Ar.Logf(TEXT("Incorrect automation command syntax! Supported commands are: "));
					Ar.Logf(TEXT("\tAutomation StartRemoteSession <sessionid>"));
					Ar.Logf(TEXT("\tAutomation List"));
					Ar.Logf(TEXT("\tAutomation RunTests <test string>"));
					Ar.Logf(TEXT("\tAutomation RunAll "));
					Ar.Logf(TEXT("\tAutomation RunFilter <filter name>"));
					Ar.Logf(TEXT("\tAutomation Quit"));
					bHandled = false;
				}
			}
		}
		
		// Shutdown our service
		return bHandled;
	}

private:
	/** The automation controller running the tests */
	IAutomationControllerManagerPtr AutomationController;

	/** The current state of the automation process */
	EAutomationTestState AutomationTestState;

	/** The priority flags we would like to run */
	EAutomationTestFlags::Type AutomationPriority;

	/** What work was requested */
	TArray<EAutomationCommand> AutomationCommandQueue;

	/** What work was requested */
	EAutomationCommand AutomationCommand;

	/** Delay used before finding workers on game instances. Just to ensure they have started up */
	float DelayTimer;

	/** Timer Handle for giving up on workers */
	float FindWorkersTimeout;

	/** Holds the session ID */
	FGuid SessionID;

	//so we can release control of the app and just get ticked like all other systems
	FDelegateHandle TickHandler;

	//Extra commandline params
	FString StringCommand;

	//This is the numbers of tests that are found in the command line.
	int32 TestCount;

	//Dictionary that maps flag names to flag values.
	TMap<FString, int32> FilterMaps;

	//Test pass checkpoint backup file.
	FArchive* CheckpointFile;

	FString CheckpointCommand;

	TArray<FString> TestsRun;
};

const float FAutomationExecCmd::DefaultDelayTimer = 5.0f;
const float FAutomationExecCmd::DefaultFindWorkersTimeout = 30.0f;
static FAutomationExecCmd AutomationExecCmd;

void EmptyLinkFunctionForStaticInitializationAutomationExecCmd()
{
	// This function exists to prevent the object file containing this test from
	// being excluded by the linker, because it has no publicly referenced symbols.
}

