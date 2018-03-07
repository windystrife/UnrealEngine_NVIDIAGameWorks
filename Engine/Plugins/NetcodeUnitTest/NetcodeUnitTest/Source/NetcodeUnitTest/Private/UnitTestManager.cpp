// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnitTestManager.h"

#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/App.h"
#include "Misc/FeedbackContext.h"
#include "UObject/Package.h"
#include "Engine/NetConnection.h"
#include "UI/LogWindowManager.h"
#include "Widgets/SWindow.h"

#include "NetcodeUnitTest.h"
#include "UnitTestEnvironment.h"
#include "UnitTest.h"
#include "ProcessUnitTest.h"
#include "ClientUnitTest.h"
#include "MinimalClient.h"

#include "UI/SLogWidget.h"
#include "UI/SLogWindow.h"
#include "UI/SLogDialog.h"

#include "NUTUtil.h"
#include "NUTUtilDebug.h"
#include "Net/NUTUtilNet.h"
#include "NUTUtilProfiler.h"
#include "NUTUtilReflectionParser.h"

// @todo #JohnBFeature: Add an overall-timer, and then start debugging the memory management in more detail


UUnitTestManager* GUnitTestManager = NULL;

UUnitTestBase* GActiveLogUnitTest = NULL;
UUnitTestBase* GActiveLogEngineEvent = NULL;
UWorld* GActiveLogWorld = NULL;


// @todo JohnB: If you need the main unit test manager, to add more logs to the final summary, replace below with a generalized solution

/** Stores a list of log messages for 'unsupported' unit tests, for printout in the final summary */
static TMap<FString, FString> UnsupportedUnitTests;



// @todo #JohnBHighPri: Add detection for unit tests that either 1: Never end (e.g. if they become outdated and break, or 2: keep on hitting
//				memory limits and get auto-closed and re-run recursively, forever

// @todo #JohnBFeatureUI: For unit tests that are running in the background (no log window for controlling them),
//				add a drop-down listbox button to the status window, for re-opening background unit test log windows
//				(can even add an option, to have them all running in background by default, so you have to do this to open
//				log window at all)


/**
 * UUnitTestManager
 */

UUnitTestManager::UUnitTestManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bCapUnitTestCount(false)
	, MaxUnitTestCount(0)
	, bCapUnitTestMemory(false)
	, MaxMemoryPercent(0)
	, AutoCloseMemoryPercent(0)
	, MaxAutoCloseCount(0)
	, PendingUnitTests()
	, ActiveUnitTests()
	, FinishedUnitTests()
	, bAbortedFirstRunUnitTest(false)
	, bAllowRequeuingUnitTests(true)
	, LogWindowManager(NULL)
	, bStatusLog(false)
	, StatusColor(FSlateColor::UseForeground())
	, DialogWindows()
	, StatusWindow()
	, AbortAllDialog()
	, StatusLog()
	, BaseUnitLogDir()
	, LastMemoryLimitHit(0.0)
	, MemoryTickCountdown(0)
	, MemoryUsageUponCountdown(0)
{
}

UUnitTestManager* UUnitTestManager::Get()
{
	if (GUnitTestManager == NULL)
	{
		GUnitTestManager = NewObject<UUnitTestManager>();

		if (GUnitTestManager != NULL)
		{
			GUnitTestManager->Initialize();
		}
	}

	return GUnitTestManager;
}

void UUnitTestManager::Initialize()
{
	// Detect if the configuration file doesn't exist, and initialize it if that's the case
	if (MaxUnitTestCount == 0)
	{
		bCapUnitTestCount = false;
		MaxUnitTestCount = 4;
		bCapUnitTestMemory = true;

		// Being a little conservative here, as the code estimating memory usage, can undershoot a bit
		MaxMemoryPercent = 75;

		// Since the above can undershoot, the limit at which unit tests are automatically terminated, is a bit higher
		AutoCloseMemoryPercent = 90;

		MaxAutoCloseCount = 4;

		UE_LOG(LogUnitTest, Log, TEXT("Creating initial unit test config file"));

		SaveConfig();
	}

	// Add this object to the root set, to disable garbage collection until desired (it is not referenced by any UProperties)
	AddToRoot();

	// Add a log hook
	if (!GLog->IsRedirectingTo(this))
	{
		GLog->AddOutputDevice(this);
	}

	if (LogWindowManager == NULL)
	{
		LogWindowManager = new FLogWindowManager();
		LogWindowManager->Initialize(800, 400);
	}
}

UUnitTestManager::~UUnitTestManager()
{
	if (LogWindowManager != NULL)
	{
		delete LogWindowManager;
		LogWindowManager = NULL;
	}

	if (GLog != nullptr)
	{
		GLog->RemoveOutputDevice(this);
	}
}

void UUnitTestManager::InitializeLogs()
{
	static bool bInitializedLogs = false;

	if (!bInitializedLogs)
	{
		bInitializedLogs = true;

		// Look for and delete old unit test logs, past a certain date and/or number, based on the main log file cleanup settings
		class FUnitLogPurger : public IPlatformFile::FDirectoryVisitor
		{
			int32 PurgeLogsDays;
			int32 MaxLogFilesOnDisk;
			IFileManager& FM;

			TMap<FString, FDateTime> DirList;

		public:
			FUnitLogPurger()
				: PurgeLogsDays(INDEX_NONE)
				, MaxLogFilesOnDisk(INDEX_NONE)
				, FM(IFileManager::Get())
			{
				GConfig->GetInt(TEXT("LogFiles"), TEXT("PurgeLogsDays"), PurgeLogsDays, GEngineIni);
				GConfig->GetInt(TEXT("LogFiles"), TEXT("MaxLogFilesOnDisk"), MaxLogFilesOnDisk, GEngineIni);
			}

			void ScanAndPurge()
			{
				if (PurgeLogsDays != INDEX_NONE || MaxLogFilesOnDisk != INDEX_NONE)
				{
					FM.IterateDirectory(*FPaths::ProjectLogDir(), *this);

					DirList.ValueSort(TLess<FDateTime>());

					// First purge directories older than a certain date
					if (PurgeLogsDays != INDEX_NONE)
					{
						for (TMap<FString, FDateTime>::TIterator It(DirList); It; ++It)
						{
							if ((FDateTime::Now() -  It.Value()).GetDays() > PurgeLogsDays)
							{
								UE_LOG(LogUnitTest, Log, TEXT("Deleting old unit test log directory: %s"), *It.Key());

								FM.DeleteDirectory(*It.Key(), true, true);

								It.RemoveCurrent();
							}
						}
					}

					// Now see how many directories are remaining, and if over the log file limit, purge the oldest ones first
					if (MaxLogFilesOnDisk != INDEX_NONE && DirList.Num() > MaxLogFilesOnDisk)
					{
						int32 RemoveCount = DirList.Num() - MaxLogFilesOnDisk;

						for (TMap<FString, FDateTime>::TIterator It(DirList); It && RemoveCount > 0; ++It, RemoveCount--)
						{
							UE_LOG(LogUnitTest, Log, TEXT("Deleting old unit test log directory: %s"), *It.Key());

							FM.DeleteDirectory(*It.Key(), true, true);
						}
					}
				}
			}

			virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
			{
				if (bIsDirectory)
				{
					FString DirName = FilenameOrDirectory;
					int32 UnitDirIdx = DirName.Find(TEXT("/UnitTests"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
					FString UnitDirName = (UnitDirIdx != INDEX_NONE ? DirName.Mid(UnitDirIdx+1) : TEXT(""));
					bool bValidUnitDir = !UnitDirName.Contains(TEXT("/")) &&
											(UnitDirName.EndsWith(TEXT("UnitTests")) || UnitDirName.Contains(TEXT("UnitTests_")));

					if (bValidUnitDir)
					{
						FFileStatData DirStats = FM.GetStatData(*DirName);

						if (DirStats.bIsValid)
						{
							DirList.Add(DirName, DirStats.CreationTime);
						}
					}
				}

				return true;
			}
		};

		FUnitLogPurger Purger;

		Purger.ScanAndPurge();


		// Determine if the log folder already exists, and if so, advance the session count until there is an empty directory
		BaseUnitLogDir = FPaths::ProjectLogDir() + TEXT("UnitTests");

		for (int32 DirCount=0; FPaths::DirectoryExists(BaseUnitLogDir + FString::Printf(TEXT("_%i"), UnitTestSessionCount)); DirCount++)
		{
			UNIT_ASSERT(DirCount < 16384);
			UnitTestSessionCount++;
		}

		if (UnitTestSessionCount > 0 || FPaths::DirectoryExists(BaseUnitLogDir))
		{
			BaseUnitLogDir += FString::Printf(TEXT("_%i"), UnitTestSessionCount);
		}

		BaseUnitLogDir += TEXT("/");


		// Create the directory and logfile
		IFileManager::Get().MakeDirectory(*BaseUnitLogDir);

		StatusLog = MakeUnique<FOutputDeviceFile>(*(BaseUnitLogDir + TEXT("UnitTestStatus.log")));


		UnitTestSessionCount++;
		SaveConfig();
	}
}

bool UUnitTestManager::QueueUnitTest(UClass* UnitTestClass, bool bRequeued/*=false*/)
{
	bool bSuccess = false;

	InitializeLogs();


	// Before anything else, open up the unit test status window (but do not pop up again if closed, for re-queued unit tests)
	if (!bRequeued && !FApp::IsUnattended())
	{
		OpenStatusWindow();
	}


	bool bValidUnitTestClass = UnitTestClass->IsChildOf(UUnitTest::StaticClass()) && UnitTestClass != UUnitTest::StaticClass() &&
								UnitTestClass != UClientUnitTest::StaticClass() && UnitTestClass != UProcessUnitTest::StaticClass();

	UUnitTest* UnitTestDefault = (bValidUnitTestClass ? Cast<UUnitTest>(UnitTestClass->GetDefaultObject()) : NULL);
	bool bSupportsAllGames = (bValidUnitTestClass ? UnitTestDefault->GetSupportedGames().Contains("NullUnitEnv") : false); //-V595

	bValidUnitTestClass = UnitTestDefault != NULL;


	if (bValidUnitTestClass && (UUnitTest::UnitEnv != NULL || bSupportsAllGames))
	{
		FString UnitTestName = UnitTestDefault->GetUnitTestName();
		bool bCurrentGameSupported = bSupportsAllGames || UnitTestDefault->GetSupportedGames().Contains(FApp::GetProjectName());

		if (bCurrentGameSupported)
		{
			// Check that the unit test is not already active or queued
			bool bActiveOrQueued = PendingUnitTests.Contains(UnitTestClass) ||
				ActiveUnitTests.ContainsByPredicate(
					[&](UUnitTest* Element)
					{
						return Element->GetClass() == UnitTestClass;
					});

			if (!bActiveOrQueued)
			{
				// Ensure the CDO has its environment settings setup
				UnitTestDefault->InitializeEnvironmentSettings();

				// Now validate the unit test settings, using the CDO, prior to queueing
				if (UnitTestDefault->ValidateUnitTestSettings(true))
				{
					PendingUnitTests.Add(UnitTestClass);
					bSuccess = true;

					STATUS_LOG(ELogType::StatusImportant, TEXT("Successfully queued unit test '%s' for execution."), *UnitTestName);
				}
				else
				{
					STATUS_LOG(ELogType::StatusError, TEXT("Failed to validate unit test '%s' for execution."), *UnitTestName);
				}
			}
			else
			{
				STATUS_LOG(, TEXT("Unit test '%s' is already queued or active"), *UnitTestName);
			}
		}
		else
		{
			TArray<FString> SupportedGamesList = UnitTestDefault->GetSupportedGames();
			FString SupportedGames = TEXT("");

			for (auto CurGame : SupportedGamesList)
			{
				SupportedGames += (SupportedGames.Len() == 0 ? CurGame : FString(TEXT(", ")) + CurGame);
			}


			FString LogMsg = FString::Printf(TEXT("Unit test '%s' doesn't support the current game ('%s'). Supported games: %s"),
												*UnitTestName, FApp::GetProjectName(), *SupportedGames);

			UnsupportedUnitTests.Add(UnitTestName, LogMsg);


			STATUS_SET_COLOR(FLinearColor(1.f, 1.f, 0.f));

			STATUS_LOG(ELogType::StatusWarning | ELogType::StyleBold, TEXT("%s"), *LogMsg);

			STATUS_RESET_COLOR();
		}
	}
	else if (!bValidUnitTestClass)
	{
		STATUS_LOG(ELogType::StatusError | ELogType::StyleBold, TEXT("Class '%s' is not a valid unit test class"),
						(UnitTestClass != NULL ? *UnitTestClass->GetName() : TEXT("")));
	}
	else if (UUnitTest::UnitEnv == NULL)
	{
		ELogType StatusType = ELogType::StyleBold;

		// @todo #JohnBAutomation: This should be enabled by default instead, so automation testing does give an error when a game doesn't
		//				have a unit test environment. You should probably setup a whitelist of 'known-unsupported-games' somewhere,
		//				to keep track of what games need support added, without breaking automation testing (but without breaking the
		//				flow of setting-up/running automation tests)
		if (!GIsAutomationTesting)
		{
			StatusType |= ELogType::StatusError;
		}

		STATUS_LOG(StatusType,
				TEXT("No unit test environment found (need to load unit test environment module for this game '%s', or create it)."),
				FApp::GetProjectName());
	}

	return bSuccess;
}

void UUnitTestManager::PollUnitTestQueue()
{
	// @todo #JohnB: Maybe consider staggering the start of unit tests, as perhaps this may give late-starters
	//				a boost from OS precaching of file data, and may also help stagger the 'peak memory' time,
	//				which often gets multiple unit tests closed early due to happening at the same time?

	// If the memory limit was recently hit, wait a number of seconds before launching any more unit tests
	if (PendingUnitTests.Num() > 0 && (FPlatformTime::Seconds() - LastMemoryLimitHit) > 4.0)
	{
		// Keep kicking off unit tests in order, until the list is empty, or until the unit test cap is reached
		for (int32 i=0; i<PendingUnitTests.Num(); i++)
		{
			bool bAlreadyRemoved = false;

			// Lambda for remove-safe and multi-remove-safe handling within this loop
			auto RemoveCurrent = [&]()
				{
					if (!bAlreadyRemoved)
					{
						bAlreadyRemoved = true;
						PendingUnitTests.RemoveAt(i);
						i--;
					}
				};


			UClass* CurUnitTestClass = PendingUnitTests[i];
			bool bWithinUnitTestLimits = ActiveUnitTests.Num() == 0 || WithinUnitTestLimits(CurUnitTestClass);

			// This unit test isn't within limits, continue to the next one and see if it fits
			if (!bWithinUnitTestLimits)
			{
				continue;
			}


			UUnitTest* CurUnitTestDefault = Cast<UUnitTest>(CurUnitTestClass->GetDefaultObject());

			if (CurUnitTestDefault != nullptr)
			{
				auto CurUnitTest = NewObject<UUnitTest>(GetTransientPackage(), CurUnitTestClass);

				if (CurUnitTest != nullptr)
				{
					if (UUnitTest::UnitEnv != nullptr)
					{
						UUnitTest::UnitEnv->UnitTest = CurUnitTest;

						CurUnitTest->InitializeEnvironmentSettings();

						UUnitTest::UnitEnv->UnitTest = nullptr;
					}

					// Remove from PendingUnitTests, and add to ActiveUnitTests
					RemoveCurrent();
					ActiveUnitTests.Add(CurUnitTest);

					// Create the log window (if starting the unit test fails, this is unset during cleanup)
					if (!FApp::IsUnattended())
					{
						OpenUnitTestLogWindow(CurUnitTest);
					}


					if (CurUnitTest->StartUnitTest())
					{
						STATUS_LOG(ELogType::StatusImportant, TEXT("Started unit test '%s'"), *CurUnitTest->GetUnitTestName());
					}
					else
					{
						STATUS_LOG(ELogType::StatusError | ELogType::StyleBold, TEXT("Failed to kickoff unit test '%s'"),
										*CurUnitTestDefault->GetUnitTestName());
					}
				}
				else
				{
					STATUS_LOG(ELogType::StatusError | ELogType::StyleBold, TEXT("Failed to construct unit test: %s"),
									*CurUnitTestDefault->GetUnitTestName());
				}
			}
			else
			{
				STATUS_LOG(ELogType::StatusError | ELogType::StyleBold, TEXT("Failed to find default object for unit test class '%s'"),
								*CurUnitTestClass->GetName());
			}

			RemoveCurrent();
		}
	}
}

bool UUnitTestManager::WithinUnitTestLimits(UClass* PendingUnitTest/*=NULL*/)
{
	bool bReturnVal = false;

	// @todo #JohnBReview: Could do with non-spammy logging of when limits are reached

	// Check max unit test count
	bReturnVal = !bCapUnitTestCount || ActiveUnitTests.Num() < MaxUnitTestCount;

	int32 CommandlineCap = 0;

	if (bReturnVal && FParse::Value(FCommandLine::Get(), TEXT("UnitTestCap="), CommandlineCap) && CommandlineCap > 0)
	{
		bReturnVal = ActiveUnitTests.Num() < CommandlineCap;
	}

	// Limit the number of first-run unit tests (which don't have any stats gathered), to MaxUnitTestCount, even if !bCapUnitTestCount.
	// If any first-run unit tests have had to be aborted, this might signify a problem, so make the cap very strict (two at a time)
	uint8 FirstRunCap = (bAbortedFirstRunUnitTest ? 2 : MaxUnitTestCount);

	if (bReturnVal && !bCapUnitTestCount && ActiveUnitTests.Num() >= FirstRunCap)
	{
		// @todo #JohnB: Add prominent logging for hitting this cap - preferably 'status' log window

		uint32 FirstRunCount = 0;

		for (auto CurUnitTest : ActiveUnitTests)
		{
			if (CurUnitTest->IsFirstTimeStats())
			{
				FirstRunCount++;
			}
		}

		bReturnVal = FirstRunCount < FirstRunCap;
	}


	// Check that physical memory usage is currently within limits (does not factor in any unit tests)
	SIZE_T TotalPhysicalMem = 0;
	SIZE_T UsedPhysicalMem = 0;
	SIZE_T MaxPhysicalMem = 0;

	if (bReturnVal)
	{
		TotalPhysicalMem = FPlatformMemory::GetConstants().TotalPhysical;
		UsedPhysicalMem = TotalPhysicalMem - FPlatformMemory::GetStats().AvailablePhysical;
		MaxPhysicalMem = ((TotalPhysicalMem / (SIZE_T)100) * (SIZE_T)MaxMemoryPercent);

		bReturnVal = MaxPhysicalMem > UsedPhysicalMem;
	}


	// Iterate through running plus pending unit tests, calculate the time at which each unit test will reach peak memory usage,
	// and estimate the total memory consumption of all unit tests combined, at the time of each peak.
	// The highest value, gives an estimate of the peak system memory consumption that will be reached, which we check is within limits.
	//
	// TLDR: Estimate worst-case peak memory usage for all unit tests together (active+pending), and check it's within limits.
	if (bReturnVal)
	{
		UUnitTest* PendingUnitTestDefObj = (PendingUnitTest != NULL ? Cast<UUnitTest>(PendingUnitTest->GetDefaultObject()) : NULL);
		double CurrentTime = FPlatformTime::Seconds();

		// Lambda for estimating how much memory an individual unit test will be using, at a specific time
		auto UnitMemUsageForTime = [&](UUnitTest* InUnitTest, double TargetTime)
			{
				SIZE_T ReturnVal = 0;

				// The calculation is based on previously collected stats for the unit test - peak mem usage and time it took to reach 
				float UnitTimeToPeakMem = InUnitTest->TimeToPeakMem;
				double UnitStartTime = InUnitTest->StartTime;
				double PeakMemTime = UnitStartTime + (double)UnitTimeToPeakMem;

				if (UnitTimeToPeakMem > 0.5f && PeakMemTime >= CurrentTime)
				{
					// Only return a value, if we expect the unit test to still be running at TargetTime
					if (PeakMemTime > TargetTime)
					{
						// Simple/dumb memory usage estimate, calculating linearly from 0 to PeakMem, based on unit test execution time
						float RunningTime = (float)(CurrentTime - UnitStartTime);
						SIZE_T PercentComplete = (SIZE_T)((RunningTime * 100.f) / UnitTimeToPeakMem);

						ReturnVal = (InUnitTest->PeakMemoryUsage / (SIZE_T)100) * PercentComplete;
					}
				}
				// If the unit test is running past TimeToPeakMem (or if that time is unknown), return worst case peak mem
				else
				{
					ReturnVal = InUnitTest->PeakMemoryUsage;
				}

				return ReturnVal;
			};

		// Lambda for estimating how much memory ALL unit tests will be using, when a specific unit test is at its peak memory usage
		auto TotalUnitMemUsageForUnitPeak = [&](UUnitTest* InUnitTest)
			{
				SIZE_T ReturnVal = 0;
				double PeakMemTime = InUnitTest->StartTime + (double)InUnitTest->TimeToPeakMem;

				for (auto CurUnitTest : ActiveUnitTests)
				{
					if (CurUnitTest == InUnitTest)
					{
						ReturnVal += InUnitTest->PeakMemoryUsage;
					}
					else
					{
						ReturnVal += UnitMemUsageForTime(CurUnitTest, PeakMemTime);
					}
				}

				// Duplicate of above
				if (PendingUnitTestDefObj != NULL)
				{
					if (PendingUnitTestDefObj == InUnitTest)
					{
						ReturnVal += PendingUnitTestDefObj->PeakMemoryUsage;
					}
					else
					{
						ReturnVal += UnitMemUsageForTime(PendingUnitTestDefObj, PeakMemTime);
					}
				}

				return ReturnVal;
			};


		// Iterate unit tests, estimating the total memory usage for all unit tests, at the time of each unit test reaching peak mem,
		// and determine the worst case value for this - this worst case value should be the peak memory usage for all unit tests
		SIZE_T WorstCaseTotalUnitMemUsage = 0;
		SIZE_T CurrentTotalUnitMemUsage = 0;

		for (auto CurUnitTest : ActiveUnitTests)
		{
			CurrentTotalUnitMemUsage += CurUnitTest->CurrentMemoryUsage;

			SIZE_T EstTotalUnitMemUsage = TotalUnitMemUsageForUnitPeak(CurUnitTest);

			if (EstTotalUnitMemUsage > WorstCaseTotalUnitMemUsage)
			{
				WorstCaseTotalUnitMemUsage = EstTotalUnitMemUsage;
			}
		}

		// Duplicate of above
		if (PendingUnitTestDefObj != NULL)
		{
			SIZE_T EstTotalUnitMemUsage = TotalUnitMemUsageForUnitPeak(PendingUnitTestDefObj);

			if (EstTotalUnitMemUsage > WorstCaseTotalUnitMemUsage)
			{
				WorstCaseTotalUnitMemUsage = EstTotalUnitMemUsage;
			}
		}

		// Now that we've got the worst case, estimate peak memory usage for the whole system, and see that it falls within limits
		SIZE_T EstimatedPeakPhysicalMem = (UsedPhysicalMem - CurrentTotalUnitMemUsage) + WorstCaseTotalUnitMemUsage;

		bReturnVal = MaxPhysicalMem > EstimatedPeakPhysicalMem;
	}

	return bReturnVal;
}

void UUnitTestManager::NotifyUnitTestComplete(UUnitTest* InUnitTest, bool bAborted)
{
	if (bAborted)
	{
		STATUS_LOG(ELogType::StatusWarning, TEXT("Aborted unit test '%s'"), *InUnitTest->GetUnitTestName());

		if (InUnitTest->IsFirstTimeStats())
		{
			bAbortedFirstRunUnitTest = true;
		}
	}
	else
	{
		PrintUnitTestResult(InUnitTest);
	}

	FinishedUnitTests.Add(InUnitTest);

	// Every time a unit test completes, poll the unit test queue, for any pending unit tests waiting for a space
	PollUnitTestQueue();
}

void UUnitTestManager::NotifyUnitTestCleanup(UUnitTest* InUnitTest)
{
	ActiveUnitTests.Remove(InUnitTest);

	UProcessUnitTest* CurProcUnitTest = Cast<UProcessUnitTest>(InUnitTest);

	if (CurProcUnitTest != NULL)
	{
		CurProcUnitTest->OnSuspendStateChange.Unbind();
	}


	TSharedPtr<SLogWindow>& LogWindow = InUnitTest->LogWindow;

	if (LogWindow.IsValid())
	{
		TSharedPtr<SLogWidget>& LogWidget = LogWindow->LogWidget;

		if (LogWidget.IsValid())
		{
			LogWidget->OnSuspendClicked.Unbind();
			LogWidget->OnDeveloperClicked.Unbind();
			LogWidget->OnConsoleCommand.Unbind();

			if (LogWidget->bAutoClose)
			{
				LogWindow->RequestDestroyWindow();
			}
		}

		LogWindow.Reset();
	}

	// Remove any open dialogs for this window
	for (auto It = DialogWindows.CreateIterator(); It; ++It)
	{
		if (It.Value() == InUnitTest)
		{
			// Don't let the dialog return the 'window closed' event as user input
			It.Key()->SetOnWindowClosed(FOnWindowClosed());

			It.Key()->RequestDestroyWindow();
			It.RemoveCurrent();

			break;
		}
	}
}

void UUnitTestManager::NotifyLogWindowClosed(const TSharedRef<SWindow>& ClosedWindow)
{
	if (ClosedWindow == StatusWindow)
	{
		if (!AbortAllDialog.IsValid() && IsRunningUnitTests())
		{
			FText CloseAllMsg = FText::FromString(FString::Printf(TEXT("Abort all active unit tests?")));
			FText ClostAllTitle = FText::FromString(FString::Printf(TEXT("Abort unit tests?")));

			TSharedRef<SWindow> CurDialogWindow = OpenLogDialog_NonModal(EAppMsgType::YesNo, CloseAllMsg, ClostAllTitle,
				FOnLogDialogResult::CreateUObject(this, &UUnitTestManager::NotifyCloseAllDialogResult));

			AbortAllDialog = CurDialogWindow;
		}

		StatusWindow.Reset();
	}
	else
	{
		// Match the log window to a unit test
		UUnitTest** CurUnitTestRef = ActiveUnitTests.FindByPredicate(
			[&](const UUnitTest* Element)
			{
				return Element->LogWindow == ClosedWindow;
			});

		UUnitTest* CurUnitTest = (CurUnitTestRef != NULL ? *CurUnitTestRef : NULL);

		if (CurUnitTest != NULL)
		{
			UProcessUnitTest* CurProcUnitTest = Cast<UProcessUnitTest>(CurUnitTest);

			if (CurProcUnitTest != NULL)
			{
				CurProcUnitTest->OnSuspendStateChange.Unbind();
			}


			if (!CurUnitTest->bCompleted && !CurUnitTest->bAborted)
			{
				// Show a message box, asking the player if they'd like to also abort the unit test
				FString UnitTestName = CurUnitTest->GetUnitTestName();

				FText CloseMsg = FText::FromString(FString::Printf(TEXT("Abort unit test '%s'? (currently running in background)"),
																	*UnitTestName));

				FText CloseTitle = FText::FromString(FString::Printf(TEXT("Abort '%s'?"), *UnitTestName));

				TSharedRef<SWindow> CurDialogWindow = OpenLogDialog_NonModal(EAppMsgType::YesNoCancel, CloseMsg, CloseTitle,
					FOnLogDialogResult::CreateUObject(this, &UUnitTestManager::NotifyCloseDialogResult));

				DialogWindows.Add(CurDialogWindow, CurUnitTest);
			}

			CurUnitTest->LogWindow.Reset();
		}
	}
}

void UUnitTestManager::NotifyCloseDialogResult(const TSharedRef<SWindow>& DialogWindow, EAppReturnType::Type Result, bool bNoResult)
{
	UUnitTest* CurUnitTest = DialogWindows.FindAndRemoveChecked(DialogWindow);

	if (CurUnitTest != NULL && ActiveUnitTests.Contains(CurUnitTest))
	{
		if (!bNoResult && Result == EAppReturnType::Yes)
		{
			CurUnitTest->AbortUnitTest();
		}
		// If the answer was 'cancel', or if the dialog was closed without answering, re-open the unit test log window
		else if (bNoResult || Result == EAppReturnType::Cancel)
		{
			if (ActiveUnitTests.Contains(CurUnitTest))
			{
				OpenUnitTestLogWindow(CurUnitTest);
			}
		}
	}
}

void UUnitTestManager::NotifyCloseAllDialogResult(const TSharedRef<SWindow>& DialogWindow, EAppReturnType::Type Result, bool bNoResult)
{
	if (!bNoResult && Result == EAppReturnType::Yes)
	{
		// First delete the pending list, to prevent any unit tests from being added
		PendingUnitTests.Empty();

		// Now abort all active unit tests
		TArray<UUnitTest*> ActiveUnitTestsCopy(ActiveUnitTests);

		for (auto CurUnitTest : ActiveUnitTestsCopy)
		{
			CurUnitTest->AbortUnitTest();
		}
	}
	else
	{
		// Re-open the status window if 'no' was clicked; don't allow it to be closed, or the client loses the ability to 'abort-all'
		if (IsRunningUnitTests())
		{
			OpenStatusWindow();
		}
	}

	AbortAllDialog.Reset();
}


void UUnitTestManager::DumpStatus(bool bForce/*=false*/)
{
	static bool bLastDumpWasBlank = false;

	bool bCurDumpIsBlank = ActiveUnitTests.Num() == 0 && PendingUnitTests.Num() == 0;

	// When no unit tests are active, don't keep dumping stats
	if (bForce || !bLastDumpWasBlank || !bCurDumpIsBlank)
	{
		bLastDumpWasBlank = bCurDumpIsBlank;

		// Give the status update logs a unique colour, so that dumping so much text into the status window,
		// doesn't disrupt the flow of text (otherwise, all the other events/updates in the status window, become hard to read)
		STATUS_SET_COLOR(FLinearColor(0.f, 1.f, 1.f));

		// @todo #JohnB: Prettify the status command, by seeing if you can evenly space unit test info into columns;
		//				use the console monospacing to help do this (if it looks ok)

		// @todo #JohnBFeatureUI: Better yet, replace the unit test status, text, with an actual list of running unit tests

		SIZE_T TotalMemoryUsage = 0;

		STATUS_LOG(, TEXT(""));
		STATUS_LOG(ELogType::StyleUnderline, TEXT("Unit test status:"))
		STATUS_LOG(, TEXT("- Number of active unit tests: %i"), ActiveUnitTests.Num());

		for (auto CurUnitTest : ActiveUnitTests)
		{
			TotalMemoryUsage += CurUnitTest->CurrentMemoryUsage;

			STATUS_LOG(, TEXT("     - (%s) %s (Memory usage: %uMB)"), *CurUnitTest->GetUnitTestType(), *CurUnitTest->GetUnitTestName(),
					(CurUnitTest->CurrentMemoryUsage / 1048576));
		}

		STATUS_LOG(, TEXT("- Total unit test memory usage: %uMB"), (TotalMemoryUsage / 1048576));

		STATUS_LOG(, TEXT("- Number of pending unit tests: %i"), PendingUnitTests.Num());

		for (auto CurClass : PendingUnitTests)
		{
			UUnitTest* CurUnitTest =  Cast<UUnitTest>(CurClass->GetDefaultObject());

			if (CurUnitTest != NULL)
			{
				STATUS_LOG(, TEXT("     - (%s) %s"), *CurUnitTest->GetUnitTestType(), *CurUnitTest->GetUnitTestName());
			}
		}

		STATUS_LOG(, TEXT(""));


		STATUS_RESET_COLOR();
	}
}

void UUnitTestManager::PrintUnitTestResult(UUnitTest* InUnitTest, bool bFinalSummary/*=false*/, bool bUnfinished/*=false*/)
{
	static const UEnum* VerificationStateEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EUnitTestVerification"));

	EUnitTestVerification UnitTestResult = InUnitTest->VerificationState;

	// Only include the automation flag, if this is the final summary
	ELogType StatusAutomationFlag = (bFinalSummary ? ELogType::StatusAutomation : ELogType::None);

	if (!bFinalSummary)
	{
		STATUS_LOG_OBJ(InUnitTest, ELogType::StatusImportant,	TEXT("Unit test '%s' completed:"), *InUnitTest->GetUnitTestName());
	}

	STATUS_LOG_OBJ(InUnitTest, ELogType::StatusImportant,	TEXT("  - Result: %s"),
					(bUnfinished ? TEXT("Aborted/Unfinished") : *VerificationStateEnum->GetNameStringByValue((int64)UnitTestResult)));
	STATUS_LOG_OBJ(InUnitTest, ELogType::StatusVerbose,	TEXT("  - Execution Time: %f"), InUnitTest->LastExecutionTime);


	auto PrintShortList =
		[&](TArray<FString>& ListSource, FString ListDesc)
		{
			FString ListStr;
			bool bMultiLineList = false;

			for (int32 i=0; i<ListSource.Num(); i++)
			{
				// If any list entry look like a lengthy description, have a line for each entry, instead of a one-line summary
				if (ListSource[i].Len() > 32)
				{
					bMultiLineList = true;
					break;
				}

				ListStr += ListSource[i];

				if (i+1 < ListSource.Num())
				{
					ListStr += TEXT(", ");
				}
			}

			if (bMultiLineList)
			{
				STATUS_LOG_OBJ(InUnitTest, ELogType::StatusVerbose, TEXT("  - %s:"), *ListDesc);

				for (auto CurEntry : ListSource)
				{
					STATUS_LOG_OBJ(InUnitTest, ELogType::StatusVerbose, TEXT("    - %s"), *CurEntry);
				}
			}
			else
			{
				STATUS_LOG_OBJ(InUnitTest, ELogType::StatusVerbose, TEXT("  - %s: %s"), *ListDesc, *ListStr);
			}
		};

	// Print bug-tracking information
	if (InUnitTest->UnitTestBugTrackIDs.Num() > 0)
	{
		PrintShortList(InUnitTest->UnitTestBugTrackIDs, TEXT("Bug tracking"));
	}

	// Print changelist information
	if (InUnitTest->UnitTestCLs.Num() > 0)
	{
		PrintShortList(InUnitTest->UnitTestCLs, TEXT("Changelists"));
	}


	EUnitTestVerification ExpectedResult = InUnitTest->GetExpectedResult();

	if (bUnfinished)
	{
		STATUS_LOG_OBJ(InUnitTest, ELogType::StatusWarning | StatusAutomationFlag | ELogType::StyleBold,
						TEXT("  - WARNING: Unit test was aborted and could not be successfully run."))
	}
	else if (ExpectedResult == EUnitTestVerification::Unverified)
	{
		STATUS_LOG_OBJ(InUnitTest, ELogType::StatusError | StatusAutomationFlag | ELogType::StyleBold,
						TEXT("  - Unit test does not have 'ExpectedResult' set"));
	}
	else
	{
		if (UnitTestResult == EUnitTestVerification::VerifiedFixed && ExpectedResult == UnitTestResult)
		{
			STATUS_SET_COLOR(FLinearColor(0.f, 1.f, 0.f));

			STATUS_LOG_OBJ(InUnitTest, ELogType::StatusSuccess | StatusAutomationFlag,
							TEXT("  - Unit test issue has been fixed."));

			STATUS_RESET_COLOR();
		}
		else
		{
			bool bExpectedResult = ExpectedResult == UnitTestResult;

			if (!bExpectedResult)
			{
				if (UnitTestResult == EUnitTestVerification::VerifiedNeedsUpdate)
				{
					STATUS_LOG_OBJ(InUnitTest, ELogType::StatusWarning | StatusAutomationFlag | ELogType::StyleBold,
								TEXT("  - WARNING: Unit test returned 'needs update' as its result."))
				}
				else
				{
					STATUS_LOG_OBJ(InUnitTest, ELogType::StatusWarning | StatusAutomationFlag | ELogType::StyleBold,
									TEXT("  - Unit test did not return expected result - unit test needs an update."));
				}

				if (InUnitTest->bUnreliable || UnitTestResult == EUnitTestVerification::VerifiedUnreliable)
				{
					STATUS_LOG_OBJ(InUnitTest, ELogType::StatusWarning | StatusAutomationFlag,
								TEXT("  - NOTE: Unit test marked 'unreliable' - may need multiple runs to get expected result."));
				}
			}
			// For when the unit test is expected to be unreliable
			else if (bExpectedResult && UnitTestResult == EUnitTestVerification::VerifiedUnreliable)
			{
				STATUS_LOG_OBJ(InUnitTest, ELogType::StatusWarning | StatusAutomationFlag,
							TEXT("  - NOTE: Unit test expected to be unreliable - multiple runs may not change result/outcome."));
			}


			if (UnitTestResult != EUnitTestVerification::VerifiedFixed)
			{
				if (ExpectedResult == EUnitTestVerification::VerifiedFixed)
				{
					STATUS_LOG_OBJ(InUnitTest, ELogType::StatusError | StatusAutomationFlag | ELogType::StyleBold,
									TEXT("  - Unit test issue is no longer fixed."));
				}
				else
				{
					STATUS_LOG_OBJ(InUnitTest, ELogType::StatusError | StatusAutomationFlag,
									TEXT("  - Unit test issue has NOT been fixed."));
				}
			}
		}
	}
}

void UUnitTestManager::PrintFinalSummary()
{
	// @todo #JohnBFeatureUI: Add some extra stats eventually, such as overall time taken etc.

	STATUS_LOG(ELogType::StatusImportant, TEXT(""));
	STATUS_LOG(ELogType::StatusImportant, TEXT(""));
	STATUS_LOG(ELogType::StatusImportant | ELogType::StatusAutomation | ELogType::StyleBold,
				TEXT("----------------------------------------------------------------FINAL UNIT TEST SUMMARY")
				TEXT("----------------------------------------------------------------"));
	STATUS_LOG(ELogType::StatusImportant, TEXT(""));
	STATUS_LOG(ELogType::StatusImportant, TEXT(""));


	// First print the unsupported unit tests
	for (auto It = UnsupportedUnitTests.CreateConstIterator(); It; ++It)
	{
		STATUS_LOG(ELogType::StatusWarning | ELogType::StatusAutomation | ELogType::StyleBold, TEXT("%s: %s"), *It.Key(), *It.Value());
	}

	if (UnsupportedUnitTests.Num() > 0)
	{
		STATUS_LOG(ELogType::StatusImportant, TEXT(""));
	}

	UnsupportedUnitTests.Empty();


	// Then print the aborted unit tests, and unit tests that have aborted so many times that they can't complete
	TArray<FString> AbortList;
	TArray<UUnitTest*> UnfinishedUnitTests;

	for (auto CurUnitTest : FinishedUnitTests)
	{
		if (CurUnitTest->bAborted)
		{
			AbortList.Add(CurUnitTest->GetUnitTestName());
		}
	}

	for (int32 AbortIdx=0; AbortIdx<AbortList.Num(); AbortIdx++)
	{
		FString CurAbort = AbortList[AbortIdx];
		uint8 NumberOfAborts = 1;

		// Count and remove duplicate aborts
		for (int32 DupeIdx=AbortList.Num()-1; DupeIdx>AbortIdx; DupeIdx--)
		{
			if (CurAbort == AbortList[DupeIdx])
			{
				NumberOfAborts++;
				AbortList.RemoveAt(DupeIdx);
			}

			// NOTE: DupeIdx is invalid past here (as it can not be decremented after RemoveAt above)
		}


		// If the unit test did not have a successful execution, note this
		UUnitTest* UnfinishedTest = NULL;

		bool bUnitTestCompleted = false;

		for (auto CurUnitTest : FinishedUnitTests)
		{
			if (CurUnitTest->GetUnitTestName() == CurAbort)
			{
				UnfinishedTest = CurUnitTest;

				if (!CurUnitTest->bAborted)
				{
					bUnitTestCompleted = true;
					break;
				}
			}
		}

		if (!bUnitTestCompleted)
		{
			UnfinishedUnitTests.Add(UnfinishedTest);
		}


		FString AbortMsg;

		if (NumberOfAborts == 1)
		{
			AbortMsg = FString::Printf(TEXT("%s: Aborted."), *CurAbort);
		}
		else
		{
			AbortMsg = FString::Printf(TEXT("%s: Aborted ('%i' times)."), *CurAbort, NumberOfAborts);
		}

		if (bUnitTestCompleted)
		{
			STATUS_LOG(ELogType::StatusWarning | ELogType::StyleBold, TEXT("%s"), *AbortMsg);
		}
		else
		{
			AbortMsg += TEXT(" Failed to successfully retry unit test after aborting.");

			STATUS_LOG(ELogType::StatusWarning | ELogType::StyleBold | ELogType::StyleItalic, TEXT("%s"), *AbortMsg);
		}
	}


	if (AbortList.Num() > 0 || UnfinishedUnitTests.Num() > 0)
	{
		STATUS_LOG(ELogType::StatusImportant, TEXT(""));
		STATUS_LOG(ELogType::StatusImportant, TEXT(""));
	}

	// Now print the completed and unfinished unit tests, which have more detailed information
	auto StatusPrintResult =
		[&](UUnitTest* CurUnitTest, bool bUnfinished)
		{
			if (!CurUnitTest->bAborted || bUnfinished)
			{
				STATUS_SET_COLOR(FLinearColor(0.25f, 0.25f, 0.25f));

				STATUS_LOG(ELogType::StatusImportant | ELogType::StatusAutomation | ELogType::StyleBold | ELogType::StyleUnderline,
							TEXT("%s:"), *CurUnitTest->GetUnitTestName());

				STATUS_RESET_COLOR();


				// Print out the main result header
				PrintUnitTestResult(CurUnitTest, true, bUnfinished);


				// Now print out the full event history
				bool bHistoryContainsImportant = CurUnitTest->StatusLogSummary.ContainsByPredicate(
					[](const TSharedPtr<FUnitStatusLog>& CurEntry)
					{
						return ((CurEntry->LogType & ELogType::StatusImportant) == ELogType::StatusImportant);
					});


				if (bHistoryContainsImportant)
				{
					STATUS_LOG(ELogType::StatusImportant, TEXT("  - Log summary:"));
				}
				else
				{
					STATUS_LOG(ELogType::StatusVerbose, TEXT("  - Log summary:"));
				}

				for (auto CurStatusLog : CurUnitTest->StatusLogSummary)
				{
					STATUS_LOG(CurStatusLog->LogType, TEXT("      %s"), *CurStatusLog->LogLine);
				}


				STATUS_LOG(ELogType::StatusImportant, TEXT(""));
			}
		};

	for (auto CurUnitTest : FinishedUnitTests)
	{
		StatusPrintResult(CurUnitTest, false);
	}

	for (auto CurUnitTest : UnfinishedUnitTests)
	{
		StatusPrintResult(CurUnitTest, true);
	}
}


void UUnitTestManager::OpenUnitTestLogWindow(UUnitTest* InUnitTest)
{
	if (LogWindowManager != NULL)
	{
		InUnitTest->LogWindow = LogWindowManager->CreateLogWindow(InUnitTest->GetUnitTestName(), InUnitTest->GetExpectedLogTypes());
		TSharedPtr<SLogWidget> CurLogWidget = (InUnitTest->LogWindow.IsValid() ? InUnitTest->LogWindow->LogWidget : NULL);

		if (CurLogWidget.IsValid())
		{
			// Setup the widget console command context list, and then bind the console command delegate
			InUnitTest->GetCommandContextList(CurLogWidget->ConsoleContextList, CurLogWidget->DefaultConsoleContext);

			CurLogWidget->OnConsoleCommand.BindUObject(InUnitTest, &UUnitTest::NotifyConsoleCommandRequest);
			CurLogWidget->OnDeveloperClicked.BindUObject(InUnitTest, &UUnitTest::NotifyDeveloperModeRequest);


			UProcessUnitTest* CurProcUnitTest = Cast<UProcessUnitTest>(InUnitTest);

			if (CurProcUnitTest != NULL)
			{
				CurLogWidget->OnSuspendClicked.BindUObject(CurProcUnitTest, &UProcessUnitTest::NotifySuspendRequest);
				CurProcUnitTest->OnSuspendStateChange.BindSP(CurLogWidget.Get(), &SLogWidget::OnSuspendStateChanged);
			}
		}
	}
}

void UUnitTestManager::OpenStatusWindow()
{
	if (!StatusWindow.IsValid() && LogWindowManager != NULL)
	{
		StatusWindow = LogWindowManager->CreateLogWindow(TEXT("Unit Test Status"), ELogType::None, true);

		TSharedPtr<SLogWidget> CurLogWidget = (StatusWindow.IsValid() ? StatusWindow->LogWidget : NULL);

		if (CurLogWidget.IsValid())
		{
			// Bind the status window console command event
			CurLogWidget->OnConsoleCommand.BindLambda(
				[&](FString CommandContext, FString Command)
				{
					bool bSuccess = false;

					// @todo #JohnBReview: Revisit this - doesn't STATUS_LOG on its own (>not< STATUS_LOG_BASE),
					//				already output to log? Don't think the extra code here is needed.

					// Need an output device redirector, to send console command log output to both GLog and unit test status log,
					// and need the 'dynamic' device, to implement a custom output device, which does the unit test status log output
					FOutputDeviceRedirector LogSplitter;
					FDynamicOutputDevice StatusLogOutput;

					StatusLogOutput.OnSerialize.AddStatic(
						[](const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
						{
							FString LogLine = FOutputDeviceHelper::FormatLogLine(Verbosity, Category, V);

							STATUS_LOG_BASE(ELogType::OriginConsole, TEXT("%s"), *LogLine);
						});


					LogSplitter.AddOutputDevice(GLog);
					LogSplitter.AddOutputDevice(&StatusLogOutput);

					bSuccess = GEngine->Exec(NULL, *Command, LogSplitter);

					return bSuccess;
				});
		}
	}
}


void UUnitTestManager::Tick(float DeltaTime)
{
	// Tick unit tests here, instead of them using FTickableGameObject, as the order of ticking is important (for memory stats)
	{
		TArray<UUnitTest*> ActiveUnitTestsCopy(ActiveUnitTests);

		double CurTime = FPlatformTime::Seconds();
		const double NetTickInterval = 1.0 / 60.0;

		for (auto CurUnitTest : ActiveUnitTestsCopy)
		{
			if (!CurUnitTest->IsPendingKill())
			{
				if (CurUnitTest->IsTickable())
				{
					UNIT_EVENT_BEGIN(CurUnitTest);

					CurUnitTest->UnitTick(DeltaTime);

					UClientUnitTest* CurClientUnitTest = Cast<UClientUnitTest>(CurUnitTest);

					// @todo #JohnB: Move NetTick to UClientUnitTest?
					if ((CurTime - CurUnitTest->LastNetTick) > NetTickInterval && CurClientUnitTest != nullptr &&
						CurClientUnitTest->MinClient != nullptr)
					{
						CurUnitTest->NetTick();
						CurUnitTest->LastNetTick = CurTime;
					}

					CurUnitTest->PostUnitTick(DeltaTime);

					UNIT_EVENT_END;
				}

				CurUnitTest->TickIsComplete(DeltaTime);
			}
		}

		ActiveUnitTestsCopy.Empty();
	}


	// Poll the unit test queue
	PollUnitTestQueue();


	// If there are no pending or active unit tests, but there are finished unit tests waiting for a summary printout, then do that now
	if (FinishedUnitTests.Num() > 0 && ActiveUnitTests.Num() == 0 && PendingUnitTests.Num() == 0)
	{
		PrintFinalSummary();


		// Now mark all of these unit tests, for garbage collection
		for (auto CurUnitTest : FinishedUnitTests)
		{
			CurUnitTest->MarkPendingKill();
		}

		FinishedUnitTests.Empty();
	}


	// If we've exceeded system memory limits, kill enough recently launched unit tests, to get back within limits
	SIZE_T TotalPhysicalMem = 0;
	SIZE_T UsedPhysicalMem = 0;
	SIZE_T MaxPhysicalMem = 0;

	TotalPhysicalMem = FPlatformMemory::GetConstants().TotalPhysical;
	UsedPhysicalMem = TotalPhysicalMem - FPlatformMemory::GetStats().AvailablePhysical;
	MaxPhysicalMem = ((TotalPhysicalMem / (SIZE_T)100) * (SIZE_T)AutoCloseMemoryPercent);


	// If we've recently force-closed a unit test, wait a number of ticks for memory stats to update
	//	(unless memory consumption increases yet again, in which case end the countdown immediately)
	if (MemoryTickCountdown > 0)
	{
		--MemoryTickCountdown;

		if (UsedPhysicalMem > MemoryUsageUponCountdown)
		{
			MemoryTickCountdown = 0;
		}
	}

	if (ActiveUnitTests.Num() > 0 && MemoryTickCountdown <= 0 && UsedPhysicalMem > MaxPhysicalMem)
	{
		SIZE_T MemOvershoot = UsedPhysicalMem - MaxPhysicalMem;

		STATUS_LOG(ELogType::StatusImportant | ELogType::StyleBold | ELogType::StyleItalic,
						TEXT("Unit test system memory limit exceeded (Used: %lluMB, Limit: %lluMB), closing some unit tests"),
							(UsedPhysicalMem / 1048576), (MaxPhysicalMem / 1048576));


		// Wait a number of ticks, before re-enabling auto-close of unit tests
		MemoryTickCountdown = 10;
		MemoryUsageUponCountdown = UsedPhysicalMem;

		LastMemoryLimitHit = FPlatformTime::Seconds();


		for (int32 i=ActiveUnitTests.Num()-1; i>=0; i--)
		{
			UUnitTest* CurUnitTest = ActiveUnitTests[i];
			SIZE_T CurMemUsage = CurUnitTest->CurrentMemoryUsage;


			// Kill the unit test and return it to the pending queue
			UClass* UnitTestClass = CurUnitTest->GetClass();

			CurUnitTest->AbortUnitTest();
			CurUnitTest = NULL;


			if (bAllowRequeuingUnitTests)
			{
				bool bAllowRequeue = true;

				// If the number of auto-abort re-queue's is limited, make sure we're not exceeding the limit
				if (MaxAutoCloseCount > 0)
				{
					uint8 CloseCount = 0;

					for (auto CurFinished : FinishedUnitTests)
					{
						if (CurFinished->bAborted && CurFinished->GetClass() == UnitTestClass)
						{
							CloseCount++;
						}
					}

					if (CloseCount >= MaxAutoCloseCount)
					{
						STATUS_LOG(ELogType::StatusWarning | ELogType::StyleBold,
									TEXT("Unit Test '%s' was aborted more than the maximum of '%i' times, and can't be re-queued."),
									*GetDefault<UUnitTest>(UnitTestClass)->GetUnitTestName(), MaxAutoCloseCount);

						bAllowRequeue = false;
					}
				}

				if (bAllowRequeue)
				{
					QueueUnitTest(UnitTestClass, true);
				}
			}


			// Keep closing unit tests, until we get back within memory limits
			if (CurMemUsage < MemOvershoot)
			{
				MemOvershoot -= CurMemUsage;
			}
			else
			{
				break;
			}
		}
	}


	// Dump unit test status every now and then
	static double LastStatusDump = 0.0;
	static bool bResetTimer = true;

	if (ActiveUnitTests.Num() > 0 || PendingUnitTests.Num() > 0)
	{
		double CurSeconds = FPlatformTime::Seconds();

		if (bResetTimer)
		{
			bResetTimer = false;
			LastStatusDump = CurSeconds;
		}
		else if (CurSeconds - LastStatusDump > 10.0)
		{
			LastStatusDump = CurSeconds;

			DumpStatus();
		}
	}
	// If no unit tests are active, reset the status dump counter next time unit tests are running/queued
	else
	{
		bResetTimer = true;
	}
}


bool UUnitTestManager::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bReturnVal = true;

	// @todo #JohnBBug: Detecting the originating unit test, by World, no longer works due to delayed fake client launches;
	//				either find another way of determining the originating unit test, or give all unit tests a unique World early on,
	//				before the fake client launch

	FString UnitTestName = FParse::Token(Cmd, false);
	bool bValidUnitTestName = false;

	// @todo #JohnBReview: Refactor some of this function, so that the unit test defaults scanning/updating,
	//				is done in a separate specific function
	//				IMPORTANT: It should be called multiple times, when needed, not just once,
	//				as there will likely be unit tests in multiple loaded packages in the future

	// First off, gather a full list of unit test classes
	TArray<UUnitTest*> UnitTestClassDefaults;

	NUTUtil::GetUnitTestClassDefList(UnitTestClassDefaults);

	// All unit tests should be given a proper date, so give big errors when this is not set
	// @todo #JohnBReview: Move to the unit test validation function
	for (int i=0; i<UnitTestClassDefaults.Num(); i++)
	{
		if (UnitTestClassDefaults[i]->GetUnitTestDate() == FDateTime::MinValue())
		{
			Ar.Logf(TEXT("ERROR: Unit Test '%s' (%s) does not have a date set!!!! A date must be added to every unit test!"),
					*UnitTestClassDefaults[i]->GetUnitTestName(), *UnitTestClassDefaults[i]->GetClass()->GetName());
		}
	}


	if (UnitTestName == TEXT("status"))
	{
		DumpStatus(true);

		bValidUnitTestName = true;
	}
	else if (UnitTestName == TEXT("detector"))
	{
		FString DetectorClass = FParse::Token(Cmd, false);

		if (DetectorClass == TEXT("FFrameProfiler"))
		{
#if STATS
			FString TargetEvent = FParse::Token(Cmd, false);
			uint8 FramePercentThreshold = FCString::Atoi(*FParse::Token(Cmd, false));

			if (!TargetEvent.IsEmpty() && FramePercentThreshold > 0)
			{
				FFrameProfiler* NewProfiler = new FFrameProfiler(FName(*TargetEvent), FramePercentThreshold);
				NewProfiler->Start();

				// @todo #JohnBLowPri: Perhaps add tracking at some stage, and remove self-destruct code from above class
			}
			else
			{
				UE_LOG(LogUnitTest, Log, TEXT("TargetEvent (%s) must be set and FramePercentThreshold (%u) must be non-zero"),
						*TargetEvent, FramePercentThreshold);
			}
#else
			UE_LOG(LogUnitTest, Log, TEXT("Can't use FFrameProfiler, stats not enable during compile."));
#endif
		}
		else
		{
			UE_LOG(LogUnitTest, Log, TEXT("Unknown detector class '%s'"), *DetectorClass);
		}

		bValidUnitTestName = true;
	}
	// If this command was triggered within a specific unit test (as identified by InWorld), abort it
	else if (UnitTestName == TEXT("abort"))
	{
		if (InWorld != NULL)
		{
			UUnitTest** AbortUnitTestRef = ActiveUnitTests.FindByPredicate(
				[&InWorld](const UUnitTest* InElement)
				{
					const UClientUnitTest* CurUnitTest = Cast<UClientUnitTest>(InElement);
					UMinimalClient* CurMinClient = (CurUnitTest != nullptr ? CurUnitTest->MinClient : nullptr);

					return CurMinClient != nullptr && CurMinClient->GetUnitWorld() == InWorld;
				});

			UUnitTest* AbortUnitTest = (AbortUnitTestRef != NULL ? *AbortUnitTestRef : NULL);

			if (AbortUnitTest != NULL)
			{
				AbortUnitTest->AbortUnitTest();
			}
		}
		else
		{
			UE_LOG(LogUnitTest, Log, TEXT("Unit test abort command, must be called within a specific unit test window."));
		}
	}
	// Debug unit test commands
	else if (UnitTestName == TEXT("debug"))
	{
		UUnitTest** TargetUnitTestRef = (InWorld != nullptr ? ActiveUnitTests.FindByPredicate(
			[&InWorld](const UUnitTest* InElement)
			{
				auto CurUnitTest = Cast<UClientUnitTest>(InElement);
				UMinimalClient* CurMinClient = (CurUnitTest != nullptr ? CurUnitTest->MinClient : nullptr);

				return CurMinClient != nullptr && CurMinClient->GetUnitWorld() == InWorld;
			})
			: nullptr);

		UClientUnitTest* TargetUnitTest = (TargetUnitTestRef != nullptr ? Cast<UClientUnitTest>(*TargetUnitTestRef) : nullptr);

		// Alternatively, if a unit test has not launched started connecting to a server, its world may not be setup,
		// so can detect by checking the active log unit test too
		if (TargetUnitTest == nullptr && GActiveLogUnitTest != nullptr)
		{
			TargetUnitTest = Cast<UClientUnitTest>(GActiveLogUnitTest);
		}

		if (TargetUnitTest != nullptr)
		{
			if (FParse::Command(&Cmd, TEXT("Requirements")))
			{
				EUnitTestFlags RequirementsFlags = (TargetUnitTest->UnitTestFlags & EUnitTestFlags::RequirementsMask);
				EUnitTestFlags MetRequirements = TargetUnitTest->GetMetRequirements();

				// Iterate over the requirements mask flag bits
				FString RequiredBits = TEXT("");
				FString MetBits = TEXT("");
				FString FailBits = TEXT("");
				TArray<FString> FlagResults;
				EUnitTestFlags FirstFlag =
					(EUnitTestFlags)(1UL << (31 - FMath::CountLeadingZeros((uint32)EUnitTestFlags::RequirementsMask)));

				for (EUnitTestFlags CurFlag=FirstFlag; !!(CurFlag & EUnitTestFlags::RequirementsMask);
						CurFlag = (EUnitTestFlags)((uint32)CurFlag >> 1))
				{
					bool bCurFlagReq = !!(CurFlag & RequirementsFlags);
					bool bCurFlagSet = !!(CurFlag & MetRequirements);

					RequiredBits += (bCurFlagReq ? TEXT("1") : TEXT("0"));
					MetBits += (bCurFlagSet ? TEXT("1") : TEXT("0"));
					FailBits += ((bCurFlagReq && !bCurFlagSet) ? TEXT("1") : TEXT("0"));

					FlagResults.Add(FString::Printf(TEXT(" - %s: Required: %i, Set: %i, Failed: %i"), *GetUnitTestFlagName(CurFlag),
									(uint32)bCurFlagReq, (uint32)bCurFlagSet, (uint32)(bCurFlagReq && !bCurFlagSet)));
				}

				Ar.Logf(TEXT("Requirements flags for unit test '%s': Required: %s, Set: %s, Failed: %s"),
						*TargetUnitTest->GetUnitTestName(), *RequiredBits, *MetBits, *FailBits);

				for (auto CurResult : FlagResults)
				{
					Ar.Logf(*CurResult);
				}
			}
			else if (FParse::Command(&Cmd, TEXT("ForceReady")))
			{
				if (TargetUnitTest != nullptr && !!(TargetUnitTest->UnitTestFlags & EUnitTestFlags::LaunchServer) &&
					TargetUnitTest->ServerHandle.IsValid() && TargetUnitTest->UnitPC == nullptr)
				{
					Ar.Logf(TEXT("Forcing unit test '%s' as ready to connect client."), *TargetUnitTest->GetUnitTestName());

					TargetUnitTest->ConnectMinimalClient();
				}
			}
			else if (FParse::Command(&Cmd, TEXT("Disconnect")))
			{
				UNetConnection* UnitConn = (TargetUnitTest != nullptr && TargetUnitTest->MinClient != nullptr ?
											TargetUnitTest->MinClient->GetConn() : nullptr);

				if (UnitConn != nullptr)
				{
					Ar.Logf(TEXT("Forcing unit test '%s' to disconnect."), *TargetUnitTest->GetUnitTestName());

					UnitConn->Close();
				}
			}
		}
		else
		{
			UE_LOG(LogUnitTest, Log, TEXT("Unit test 'debug' command, must be called from within a specific unit test window."));
		}

		bValidUnitTestName = true;
	}
	else if (UnitTestName == TEXT("all"))
	{
		// When executing all unit tests, allow them to be requeued if auto-aborted
		bAllowRequeuingUnitTests = true;

		for (int i=0; i<UnitTestClassDefaults.Num(); i++)
		{
			if (!UnitTestClassDefaults[i]->bWorkInProgress)
			{
				if (!QueueUnitTest(UnitTestClassDefaults[i]->GetClass()))
				{
					Ar.Logf(TEXT("Failed to add unit test '%s' to queue"), *UnitTestClassDefaults[i]->GetUnitTestName());
				}
			}
		}

		// After queuing the unit tests, poll the queue to see we're ready to execute more
		PollUnitTestQueue();

		bValidUnitTestName = true;
	}
	else if (UnitTestName.Len() > 0)
	{
		UClass* CurUnitTestClass = NULL;

		for (int i=0; i<UnitTestClassDefaults.Num(); i++)
		{
			UUnitTest* CurDefault = UnitTestClassDefaults[i];

			if (CurDefault->GetUnitTestName() == UnitTestName)
			{
				CurUnitTestClass = UnitTestClassDefaults[i]->GetClass();
				break;
			}
		}

		bValidUnitTestName = CurUnitTestClass != NULL;

		if (bValidUnitTestName && QueueUnitTest(CurUnitTestClass))
		{
			// Don't allow requeuing of single unit tests, if they are auto-aborted
			bAllowRequeuingUnitTests = false;

			// Now poll the unit test queue, to see we're ready to execute more
			PollUnitTestQueue();
		}
		else
		{
			Ar.Logf(TEXT("Failed to add unit test '%s' to queue"), *UnitTestName);
		}
	}

	// List all unit tests
	if (!bValidUnitTestName)
	{
		Ar.Logf(TEXT("Could not find unit test '%s', listing all unit tests:"), *UnitTestName);
		Ar.Logf(TEXT("- 'status': Lists status of all currently running unit tests"));
		Ar.Logf(TEXT("- 'all': Executes all unit tests at once"));

		// First sort the unit test class defaults
		NUTUtil::SortUnitTestClassDefList(UnitTestClassDefaults);

		// Now list them, now that they are ordered in terms of type and date
		FString LastType;

		for (int i=0; i<UnitTestClassDefaults.Num(); i++)
		{
			FString CurType = UnitTestClassDefaults[i]->GetUnitTestType();

			if (LastType != CurType)
			{
				Ar.Logf(TEXT("- '%s' unit tests:"), *CurType);

				LastType = CurType;
			}

			Ar.Logf(TEXT("     - %s"), *UnitTestClassDefaults[i]->GetUnitTestName());
		}
	}

	return bReturnVal;
}


void UUnitTestManager::Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	if (bStatusLog)
	{
		if (StatusLog.IsValid())
		{
			StatusLog.Get()->Serialize(Data, Verbosity, NAME_None);
		}

		if (StatusWindow.IsValid())
		{
			TSharedPtr<SLogWidget>& LogWidget = StatusWindow->LogWidget;

			if (LogWidget.IsValid())
			{
				ELogType CurLogType = ELogType::Local | GActiveLogTypeFlags;
				bool bSetTypeColor = false;


				// Colour-in some log types, that are typically passed in from unit tests (unless the colour was overridden)
				if (!StatusColor.IsColorSpecified())
				{
					bSetTypeColor = true;

					if ((CurLogType & ELogType::StatusError) == ELogType::StatusError)
					{
						SetStatusColor(FLinearColor(1.f, 0.f, 0.f));
					}
					else if ((CurLogType & ELogType::StatusWarning) == ELogType::StatusWarning)
					{
						SetStatusColor(FLinearColor(1.f, 1.f, 0.f));
					}
					else if ((CurLogType & ELogType::StatusAdvanced) == ELogType::StatusAdvanced ||
								(CurLogType & ELogType::StatusVerbose) == ELogType::StatusVerbose)
					{
						SetStatusColor(FLinearColor(0.25f, 0.25f, 0.25f));
					}
					else
					{
						bSetTypeColor = false;
					}
				}


				FString* LogLine = nullptr;
				UUnitTest* CurLogUnitTest = Cast<UUnitTest>(GActiveLogUnitTest);

				if (CurLogUnitTest != nullptr)
				{
					// Store the log within the unit test
					CurLogUnitTest->StatusLogSummary.Add(MakeShareable(new FUnitStatusLog(CurLogType, FString(Data))));

					LogLine = new FString(FString::Printf(TEXT("%s: %s"), *CurLogUnitTest->GetUnitTestName(), Data));
				}
				else
				{
					LogLine = new FString(Data);
				}

				TSharedRef<FString> LogLineRef = MakeShareable(LogLine);
				bool bRequestFocus = !!(CurLogType & ELogType::FocusMask);

				LogWidget->AddLine(CurLogType, LogLineRef, StatusColor, bRequestFocus);


				if (bSetTypeColor)
				{
					SetStatusColor();
				}
			}
		}
	}
	// Unit test logs (including hooked log events) - also double check that this is not a log for the global status window
	else if (!(GActiveLogTypeFlags & (ELogType::GlobalStatus | ELogType::OriginVoid)))
	{
		// Prevent re-entrant code
		static bool bLogSingularCheck = false;

		if (!bLogSingularCheck)
		{
			bLogSingularCheck = true;


			ELogType CurLogType = ELogType::Local | GActiveLogTypeFlags;
			UUnitTest* SourceUnitTest = nullptr;

			// If this log was triggered, while a unit test net connection was processing a packet, find and notify the unit test
			if (GActiveReceiveUnitConnection != nullptr)
			{
				CurLogType |= ELogType::OriginNet;

				for (auto CurUnitTest : ActiveUnitTests)
				{
					UClientUnitTest* CurClientUnitTest = Cast<UClientUnitTest>(CurUnitTest);
					UNetConnection* UnitConn = (CurClientUnitTest != nullptr && CurClientUnitTest->MinClient != nullptr ?
												CurClientUnitTest->MinClient->GetConn() : nullptr);

					if (UnitConn != nullptr && UnitConn == GActiveReceiveUnitConnection)
					{
						SourceUnitTest = CurUnitTest;
						break;
					}
				}
			}
			// If it was triggered from within a unit test log, also notify
			else if (GActiveLogUnitTest != nullptr)
			{
				CurLogType |= ELogType::OriginUnitTest;
				SourceUnitTest = Cast<UUnitTest>(GActiveLogUnitTest);
			}
			// If it was triggered within an engine event, within a unit test, again notify
			else if (GActiveLogEngineEvent != nullptr)
			{
				CurLogType |= ELogType::OriginEngine;
				SourceUnitTest = Cast<UUnitTest>(GActiveLogEngineEvent);
			}
			// If it was triggered during UWorld::Tick, for the world assigned to a unit test, again find and notify
			else if (GActiveLogWorld != nullptr)
			{
				CurLogType |= ELogType::OriginEngine;

				for (auto CurUnitTest : ActiveUnitTests)
				{
					UClientUnitTest* CurClientUnitTest = Cast<UClientUnitTest>(CurUnitTest);
					UMinimalClient* CurMinClient = (CurClientUnitTest != nullptr ? CurClientUnitTest->MinClient : nullptr);

					if (CurMinClient != nullptr && CurMinClient->GetUnitWorld() == GActiveLogWorld)
					{
						SourceUnitTest = CurUnitTest;
						break;
					}
				}
			}

			if (SourceUnitTest != nullptr && (CurLogType & ELogType::OriginMask) != ELogType::None)
			{
				SourceUnitTest->NotifyLocalLog(CurLogType, Data, Verbosity, Category);
			}


			bLogSingularCheck = false;
		}
	}
}


/**
 * Exec hook for the unit test manager (also handles creation of unit test manager)
 */

static bool UnitTestExec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bReturnVal = false;

	if (FParse::Command(&Cmd, TEXT("UnitTest")))
	{
		// Create the unit test manager, if it hasn't already been created
		if (GUnitTestManager == NULL)
		{
			UUnitTestManager::Get();
		}

		if (GUnitTestManager != NULL)
		{
			bReturnVal = GUnitTestManager->Exec(InWorld, Cmd, Ar);
		}
		else
		{
			Ar.Logf(TEXT("Failed to execute unit test command '%s', GUnitTestManager == NULL"), Cmd);
		}

		bReturnVal = true; //-V519
	}
	/**
	 * For the connection-per-unit-test code, which also creates a whole new world/netdriver etc. per unit test,
	 * you need to very carefully remove references to the world when done, outside ticking of GEngine->WorldList.
	 * This needs to be done using GEngine->TickDeferredCommands, which is what should trigger this console command.
	 */
	else if (FParse::Command(&Cmd, TEXT("CleanupUnitTestWorlds")))
	{
		NUTNet::CleanupUnitTestWorlds();

		bReturnVal = true;
	}
	/**
	 * Special 'StackTrace' command, for adding complex arbitrary stack tracing, as a debugging method.
	 *
	 * Usage: (NOTE: Replace 'TraceName' as desired, to help identify traces in logs)
	 *
	 * Once-off stack trace/dump:
	 *	GEngine->Exec(NULL, TEXT("StackTrace TraceName"));
	 *
	 *
	 * Multiple tracked stack traces: (grouped by TraceName)
	 *	- Add a stack trace to tracking: ('-Log' also logs that a stack trace was added, and '-Dump' immediately dumps it to log,
	 *										'-StartDisabled' only begins tracking once the 'enable' command below is called)
	 *		GEngine->Exec(NULL, TEXT("StackTrace TraceName Add"));
	 *		GEngine->Exec(NULL, TEXT("StackTrace TraceName Add -Log"));
	 *		GEngine->Exec(NULL, TEXT("StackTrace TraceName Add -Log -Dump"));
	 *		GEngine->Exec(NULL, TEXT("StackTrace TraceName Add -StartDisabled"));
	 *
	 *	- Dump collected/tracked stack traces: (also removes from tracking by default, unless -Keep is added)
	 *		GEngine->Exec(NULL, TEXT("StackTrace TraceName Dump"));
	 *		GEngine->Exec(NULL, TEXT("StackTrace TraceName Dump -Keep"));
	 *
	 *	- Temporarily disable tracking: (NOTE: Dump must be used with -Keep to use this)
	 *		GEngine->Exec(NULL, TEXT("StackTrace TraceName Disable"));
	 *
	 *	- Enable/re-enable tracking: (usually not necessary, unless tracking was previously disabled)
	 *		GEngine->Exec(NULL, TEXT("StackTrace TraceName Enable"));
	 *
	 *
	 * Additional commands:
	 * - Dump all active stack traces (optionally skip resetting built up stack traces, and optionally stop all active traces)
	 *		"StackTrace DumpAll"
	 *		"StackTrace DumpAll -NoReset"
	 *		"StackTrace DumpAll -Stop"
	 */
	else if (FParse::Command(&Cmd, TEXT("StackTrace")))
	{
		FString TraceName;

		if (FParse::Command(&Cmd, TEXT("DumpAll")))
		{
			bool bKeepTraceHistory = FParse::Param(Cmd, TEXT("KEEPHISTORY"));
			bool bStopTracking = FParse::Param(Cmd, TEXT("STOP"));

			GTraceManager->DumpAll(bKeepTraceHistory, !bStopTracking);
		}
		else if (FParse::Token(Cmd, TraceName, true))
		{
			if (FParse::Command(&Cmd, TEXT("Enable")))
			{
				GTraceManager->Enable(TraceName);
			}
			else if (FParse::Command(&Cmd, TEXT("Disable")))
			{
				GTraceManager->Disable(TraceName);
			}
			else if (FParse::Command(&Cmd, TEXT("Add")))
			{
				bool bLogAdd = FParse::Param(Cmd, TEXT("LOG"));
				bool bDump = FParse::Param(Cmd, TEXT("DUMP"));
				bool bStartDisabled = FParse::Param(Cmd, TEXT("STARTDISABLED"));

				GTraceManager->AddTrace(TraceName, bLogAdd, bDump, bStartDisabled);
			}
			else if (FParse::Command(&Cmd, TEXT("Dump")))
			{
				GTraceManager->Dump(TraceName);
			}
			// If no subcommands above are specified, assume this is a once-off stack trace dump
			// @todo #JohnB: This will also capture mistyped commands, so find a way to detect that
			else
			{
				GTraceManager->TraceAndDump(TraceName);
			}
		}
		else
		{
			Ar.Logf(TEXT("Need to specify TraceName, i.e. 'StackTrace TraceName'"));
		}

		bReturnVal = true;
	}
	/**
	 * Special 'LogTrace' command, which ties into the stack tracking code as used by the 'StackTrace' command.
	 * Every time a matching log entry is encountered, a stack trace is dumped.
	 *
	 * NOTE: Does not track the category or verbosity of log entries
	 *
	 * Usage: (NOTE: Replace 'LogLine' with the log text to be tracked)
	 *
	 *	- Add an exact log line to tracking (case sensitive, and must match length too):
	 *		GEngine->Exec(NULL, TEXT("LogTrace Add LogLine"));
	 *
	 *	- Add a partial log line to tracking (case insensitive, and can match substrings):
	 *		GEngine->Exec(NULL, TEXT("LogTrace AddPartial LogLine"));
	 *
	 *	- Dump accumulated log entries, for a specified log line, and clears it from tracing:
	 *		GEngine->Exec(NULL, TEXT("LogTrace Dump LogLine"));
	 *
	 *	- Clear the specified log line from tracing:
	 *		GEngine->Exec(NULL, TEXT("LogTrace Clear LogLine"));
	 *
	 *	- Clear all log lines from tracing:
	 *		GEngine->Exec(NULL, TEXT("LogTrace ClearAll"));
	 *		GEngine->Exec(NULL, TEXT("LogTrace ClearAll -Dump"));
	 */
	else if (FParse::Command(&Cmd, TEXT("LogTrace")))
	{
		FString LogLine = TEXT("NotSet");

		if (FParse::Command(&Cmd, TEXT("Add")) && (LogLine = Cmd).Len() > 0)
		{
			GLogTraceManager->AddLogTrace(LogLine, false);
		}
		else if (FParse::Command(&Cmd, TEXT("AddPartial")) && (LogLine = Cmd).Len() > 0)
		{
			GLogTraceManager->AddLogTrace(LogLine, true);
		}
		else if (FParse::Command(&Cmd, TEXT("Dump")) && (LogLine = Cmd).Len() > 0)
		{
			GLogTraceManager->ClearLogTrace(LogLine, true);
		}
		else if (FParse::Command(&Cmd, TEXT("Clear")) && (LogLine = Cmd).Len() > 0)
		{
			GLogTraceManager->ClearLogTrace(LogLine, false);
		}
		else if (FParse::Command(&Cmd, TEXT("ClearAll")))
		{
			bool bDump = FParse::Param(Cmd, TEXT("DUMP"));

			GLogTraceManager->ClearAll(bDump);
		}
		// If LogLine is now zero-length instead of 'NotSet', that means a valid command was encountered, but no LogLine specified
		else if (LogLine.Len() == 0)
		{
			Ar.Logf(TEXT("Need to specify a log line for tracing."));
		}

		bReturnVal = true;
	}
	/**
	 * Special 'LogHex' command, for taking an arbitrary memory location, plus its length, and spitting out a hex byte-dump.
	 * Access implemented through a console command, so that this can be used without a dependency, throughout the engine.
	 *
	 * Usage: (copy-paste into code, at location desired, Pointer is the pointer, Length is the length of data Pointer references)
	 *	GEngine->Exec(NULL, *FString::Printf(TEXT("LogHex -Data=%llu -DataLen=%u"), (uint64)Pointer, Length));
	 */
	else if (FParse::Command(&Cmd, TEXT("LogHex")))
	{
		uint64 PointerVal = 0;
		uint32 DataLen = 0;

		if (FParse::Value(Cmd, TEXT("Data="), PointerVal) && FParse::Value(Cmd, TEXT("DataLen="), DataLen))
		{
			uint8* Data = (uint8*)PointerVal;

			// NOTE: This case covers TArray's which are empty, and can be allocated (Data != nullptr) or unallocated (Data == nullptr)
			if (Data != nullptr || DataLen == 0)
			{
				NUTDebug::LogHexDump(Data, DataLen);
			}
			else //if (Data == nullptr)
			{
				Ar.Logf(TEXT("Invalid Data parameter."));
			}
		}
		else
		{
			Ar.Logf(TEXT("Need to specify '-Data=DataAddress' and '-DataLen=Len'."));
		}

		bReturnVal = true;
	}
	/**
	 * As above, except for bit-based-logging
	 *
	 * Usage: (copy-paste into code, at location desired, Pointer is the pointer, Length is the length of data Pointer references)
	 *	GEngine->Exec(NULL, *FString::Printf(TEXT("LogBits -Data=%llu -DataLen=%u"), (uint64)Pointer, Length));
	 */
	else if (FParse::Command(&Cmd, TEXT("LogBits")))
	{
		uint64 PointerVal = 0;
		uint32 DataLen = 0;

		if (FParse::Value(Cmd, TEXT("Data="), PointerVal) && FParse::Value(Cmd, TEXT("DataLen="), DataLen))
		{
			uint8* Data = (uint8*)PointerVal;

			// NOTE: This case covers TArray's which are empty, and can be allocated (Data != nullptr) or unallocated (Data == nullptr)
			if (Data != nullptr || DataLen == 0)
			{
				NUTDebug::LogBitDump(Data, DataLen);
			}
			else //if (Data == nullptr)
			{
				Ar.Logf(TEXT("Invalid Data parameter."));
			}
		}
		else
		{
			Ar.Logf(TEXT("Need to specify '-Data=DatAddress' and '-DataLen=Len'."));
		}
	}
	/**
	 * Watches for the specified assert log, and then blocks it to prevent the game from crashing.
	 * Does a partial match for the assert, rather than an exact match.
	 */
	else if (FParse::Command(&Cmd, TEXT("AssertDisable")))
	{
		FString Assert = Cmd;

		if (Assert.Len() > 0)
		{
			FAssertHookDevice::AddAssertHook(Assert);

			Ar.Logf(TEXT("Blocking asserts matching '%s'."), *Assert);
		}
		else
		{
			Ar.Logf(TEXT("Need to specify the log string that should be matched, for detecting the assert."));
		}

		bReturnVal = true;
	}
	/**
	 * Implements a command for utilizing the UE4 reflection system, through the NetcodeUnitTest FVMReflection helper class.
	 * This is like a supercharged version of the 'get/set' commands, able to access anything in the UE4 VM, using C++-like syntax.
	 *
	 * This can save lots of time spent debugging using other means (e.g. log messages and associated recompiling/launching),
	 * by allowing much better reach through the UE4 VM - almost like writing/executing code from the console.
	 *
	 *
	 * Basic Usage:
	 *	To get a reference to an object, use the 'Find(Name, Class)' function, where 'Name' is the full or partial name of an object,
	 *	and (optionally) 'Class' is the full class name the object derives from.
	 *
	 *	For example, this will print the first PlayerController found:
	 *		- Command:
	 *			Reflect Find(,PlayerController)
	 *
	 *		- Output:
	 *			OrionPlayerController_Main /Game/Maps/FrontEndScene.FrontEndScene:PersistentLevel.OrionPlayerController_Main_0
	 *
	 *	Once you find an object, then you can step-through and/or print members from the object, using C++-like syntax:
	 *		- Command:
	 *			Reflect Find(,PlayerController).Player.ViewportClient.GameInstance.LocalPlayers
	 *
	 *		- Output:
	 *			(OrionLocalPlayer'/Engine/Transient.OrionEngine_0:OrionLocalPlayer_0')
	 *
	 *
	 * Future features: (@todo #JohnB: NOTE: These will be implemented slowly, over a very long time, as they become useful to me)
	 *	- Assignment operator '=':
	 *		- This will allow variables to be assigned new values, rather than just printed
	 *
	 *	- Function calls:
	 *		- Function's can not yet be called, only the inbuilt 'Find' function works at the moment
	 *
	 *	- Array operator '[0]':
	 *		- Dynamic/static array elements can not be specified at the moment
	 *
	 *	- Misc. type support:
	 *		- There are a few UProperty's I haven't used with reflection yet (e.g. TMap), which will be supported when it becomes useful
	 *
	 *	- Console autocomplete:
	 *		- Eventually, I may utilize the console autocomplete, to automatically evaluate the reflect command as it is typed,
	 *			and list accessible members for the current typed statement etc., like in Visual Studio.
	 *
	 *	- C++ reflection:
	 *		- In theory, it's possible to extend reflection to C++ variables/functions using RTTI (Run-Time Type Information).
	 *			If implemented, this will be experimental and very limited.
	 */
	else if (FParse::Command(&Cmd, TEXT("Reflect")))
	{
		FVMReflectionParser Parser;

		TValueOrError<FString, FExpressionError> Result = Parser.EvaluateString(Cmd);

		if (Result.IsValid())
		{
			Ar.Logf(TEXT("%s"), *Result.GetValue());
		}
		else
		{
			Ar.Logf(TEXT("Reflect: Error parsing: %s"), *Result.GetError().Text.ToString());
		}

		bReturnVal = true;
	}

	return bReturnVal;
}

// Register the above static exec function
FStaticSelfRegisteringExec UnitTestExecRegistration(UnitTestExec);

