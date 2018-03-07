// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProcessUnitTest.h"
#include "Containers/ArrayBuilder.h"
#include "Misc/FeedbackContext.h"

#include "UnitTestManager.h"
#include "UnitTestEnvironment.h"
#include "NUTUtil.h"

#include "UI/SLogWindow.h"
#include "UI/SLogWidget.h"

#include "Internationalization/Regex.h"


/**
 * UProcessUnitTest
 */
UProcessUnitTest::UProcessUnitTest(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ActiveProcesses()
	, LastBlockingProcessCheck(0)
	, OnSuspendStateChange()
{
}

void UProcessUnitTest::NotifyProcessSuspendState(TWeakPtr<FUnitTestProcess> InProcess, ESuspendState InSuspendState)
{
	if (InProcess.IsValid())
	{
		InProcess.Pin()->SuspendState = InSuspendState;
	}
}


void UProcessUnitTest::CleanupUnitTest()
{
	for (int32 i=ActiveProcesses.Num()-1; i>=0; i--)
	{
		if (ActiveProcesses[i].IsValid())
		{
			ShutdownUnitTestProcess(ActiveProcesses[i]);
		}
	}

	Super::CleanupUnitTest();
}

TWeakPtr<FUnitTestProcess> UProcessUnitTest::StartUnitTestProcess(FString Path, FString InCommandline, bool bMinimized/*=true*/)
{
	TSharedPtr<FUnitTestProcess> ReturnVal = MakeShareable(new FUnitTestProcess());

	verify(FPlatformProcess::CreatePipe(ReturnVal->ReadPipe, ReturnVal->WritePipe));

	UNIT_LOG(ELogType::StatusImportant, TEXT("Starting process with parameters: %s"), *InCommandline);

	ReturnVal->ProcessHandle = FPlatformProcess::CreateProc(*Path, *InCommandline, true, bMinimized, false, &ReturnVal->ProcessID,
															0, NULL, ReturnVal->WritePipe);

	if (ReturnVal->ProcessHandle.IsValid())
	{
		ReturnVal->ProcessTag = FString::Printf(TEXT("Process_%i"), ReturnVal->ProcessID);
		ReturnVal->LogPrefix = FString::Printf(TEXT("[PROC%i]"), ReturnVal->ProcessID);

		ActiveProcesses.Add(ReturnVal);
	}
	else
	{
		UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to start process"));
	}

	return ReturnVal;
}

TWeakPtr<FUnitTestProcess> UProcessUnitTest::StartUE4UnitTestProcess(FString InCommandline, bool bMinimized/*=true*/)
{
	TWeakPtr<FUnitTestProcess> ReturnVal = NULL;
	FString GamePath = FPlatformProcess::GenerateApplicationPath(FApp::GetName(), FApp::GetBuildConfiguration());

	ReturnVal = StartUnitTestProcess(GamePath, InCommandline, bMinimized);

	if (ReturnVal.IsValid())
	{
		auto CurHandle = ReturnVal.Pin();

		if (CurHandle->ProcessHandle.IsValid())
		{
			CurHandle->ProcessTag = FString::Printf(TEXT("UE4_%i"), CurHandle->ProcessID);
			CurHandle->LogPrefix = FString::Printf(TEXT("[UE4%i]"), CurHandle->ProcessID);
		}
	}

	return ReturnVal;
}

void UProcessUnitTest::ShutdownUnitTestProcess(TSharedPtr<FUnitTestProcess> InHandle)
{
	if (InHandle->ProcessHandle.IsValid())
	{
		FString LogMsg = FString::Printf(TEXT("Shutting down process '%s'."), *InHandle->ProcessTag);

		UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);


		// @todo #JohnBHighPri: Restore 'true' here, once the issue where killing child processes sometimes kills all processes,
		//						is fixed.
		FPlatformProcess::TerminateProc(InHandle->ProcessHandle);//, true);

#if TARGET_UE4_CL < CL_CLOSEPROC
		InHandle->ProcessHandle.Close();
#else
		FPlatformProcess::CloseProc(InHandle->ProcessHandle);
#endif
	}

	FPlatformProcess::ClosePipe(InHandle->ReadPipe, InHandle->WritePipe);
	InHandle->ReadPipe = NULL;
	InHandle->WritePipe = NULL;

	ActiveProcesses.Remove(InHandle);

	// Print out any detected error logs
	if (InHandle->ErrorLogStage != EErrorLogStage::ELS_NoError && InHandle->ErrorText.Num() > 0)
	{
		PrintUnitTestProcessErrors(InHandle);
	}
}

void UProcessUnitTest::PrintUnitTestProcessErrors(TSharedPtr<FUnitTestProcess> InHandle)
{
	FString LogMsg = FString::Printf(TEXT("Detected a crash in process '%s':"), *InHandle->ProcessTag);

	UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
	UNIT_STATUS_LOG(ELogType::StatusImportant, TEXT("%s (click 'Advanced Summary' for more detail)"), *LogMsg);


	uint32 CallstackCount = 0;

	for (auto CurErrorLine : InHandle->ErrorText)
	{
		ELogType CurLogType = ELogType::None;

		if (CurErrorLine.Stage == EErrorLogStage::ELS_ErrorStart)
		{
			CurLogType = ELogType::StatusAdvanced;
		}
		else if (CurErrorLine.Stage == EErrorLogStage::ELS_ErrorDesc)
		{
			CurLogType = ELogType::StatusImportant | ELogType::StatusAdvanced | ELogType::StyleBold;
		}
		else if (CurErrorLine.Stage == EErrorLogStage::ELS_ErrorCallstack)
		{
			CurLogType = ELogType::StatusAdvanced;

			// Include the first five callstack lines in the main summary printout
			if (CallstackCount < 5)
			{
				CurLogType |= ELogType::StatusImportant;
			}

			CallstackCount++;
		}
		else if (CurErrorLine.Stage == EErrorLogStage::ELS_ErrorExit)
		{
			continue;
		}
		else
		{
			// Make sure there's a check for all error log stages
			UNIT_ASSERT(false);
		}

		// For the current unit-test log window, all entries are marked 'StatusImportant'
		UNIT_LOG(CurLogType | ELogType::StatusImportant, TEXT("%s"), *CurErrorLine.Line);

		UNIT_STATUS_LOG(CurLogType, TEXT("%s"), *CurErrorLine.Line);
	}
}

void UProcessUnitTest::UpdateProcessStats()
{
	SIZE_T TotalProcessMemoryUsage = 0;

	for (auto CurHandle : ActiveProcesses)
	{
		// Check unit test memory stats, and update if necessary (NOTE: Processes not guaranteed to still be running)
		if (CurHandle.IsValid() && CurHandle->ProcessID != 0)
		{
			SIZE_T ProcessMemUsage = 0;

			if (FPlatformProcess::GetApplicationMemoryUsage(CurHandle->ProcessID, &ProcessMemUsage) && ProcessMemUsage != 0)
			{
				TotalProcessMemoryUsage += ProcessMemUsage;
			}
		}
	}

	if (TotalProcessMemoryUsage > 0)
	{
		CurrentMemoryUsage = TotalProcessMemoryUsage;

		float RunningTime = (float)(FPlatformTime::Seconds() - StartTime);

		// Update saved memory usage stats, to help with tracking estimated memory usage, on future unit test launches
		if (CurrentMemoryUsage > PeakMemoryUsage)
		{
			if (PeakMemoryUsage == 0)
			{
				bFirstTimeStats = true;
			}

			PeakMemoryUsage = CurrentMemoryUsage;

			// Reset TimeToPeakMem
			TimeToPeakMem = RunningTime;

			SaveConfig();
		}
		// Check if we have hit a new record, for the time it takes to get within 90% of PeakMemoryUsage
		else if (RunningTime < TimeToPeakMem && (CurrentMemoryUsage * 100) >= (PeakMemoryUsage * 90))
		{
			TimeToPeakMem = RunningTime;
			SaveConfig();
		}
	}
}

void UProcessUnitTest::CheckOutputForError(TSharedPtr<FUnitTestProcess> InProcess, const TArray<FString>& InLines)
{
	// Perform error parsing, for UE4 processes
	if (InProcess->ProcessTag.StartsWith(TEXT("UE4_")))
	{
		// Array of log messages that can indicate the start of an error
		static const TArray<FString> ErrorStartLogs = TArrayBuilder<FString>()
			.Add(FString(TEXT("Windows GetLastError: ")))
			.Add(FString(TEXT("=== Critical error: ===")));


		for (FString CurLine : InLines)
		{
			// Using 'ContainsByPredicate' as an iterator
			const auto CheckForErrorLog =
				[&CurLine](const FString& ErrorLine)
				{
					return CurLine.Contains(ErrorLine);
				};

			// Search for the beginning of an error log message
			if (InProcess->ErrorLogStage == EErrorLogStage::ELS_NoError)
			{
				if (ErrorStartLogs.ContainsByPredicate(CheckForErrorLog))
				{
					InProcess->ErrorLogStage = EErrorLogStage::ELS_ErrorStart;

					// Reset the timeout for both the unit test and unit test connection here, as callstack logs are prone to failure
					//	(do it for at least two minutes as well, as it can take a long time to get a crashlog)
					ResetTimeout(TEXT("Detected crash."), true, 120);
				}
			}


			if (InProcess->ErrorLogStage != EErrorLogStage::ELS_NoError)
			{
				// Regex pattern for matching callstack logs - matches:
				//	" (0x000007fefe22cacd) + 0 bytes"
				//	" {0x000007fefe22cacd} + 0 bytes"
				const FRegexPattern CallstackPattern(TEXT("\\s[\\(|\\{]0x[0-9,a-f]+[\\)|\\}] \\+ [0-9]+ bytes"));

				// Matches:
				//	"ntldll.dll"
				const FRegexPattern AltCallstackPattern(TEXT("^[a-z,A-Z,0-9,\\-,_]+\\.[exe|dll]"));


				// Check for the beginning of description logs
				if (InProcess->ErrorLogStage == EErrorLogStage::ELS_ErrorStart &&
					!ErrorStartLogs.ContainsByPredicate(CheckForErrorLog))
				{
					InProcess->ErrorLogStage = EErrorLogStage::ELS_ErrorDesc;
				}

				// Check-for/verify callstack logs
				if (InProcess->ErrorLogStage == EErrorLogStage::ELS_ErrorDesc ||
					InProcess->ErrorLogStage == EErrorLogStage::ELS_ErrorCallstack)
				{
					FRegexMatcher CallstackMatcher(CallstackPattern, CurLine);
					FRegexMatcher AltCallstackMatcher(AltCallstackPattern, CurLine);

					if (CallstackMatcher.FindNext() || AltCallstackMatcher.FindNext())
					{
						InProcess->ErrorLogStage = EErrorLogStage::ELS_ErrorCallstack;
					}
					else if (InProcess->ErrorLogStage == EErrorLogStage::ELS_ErrorCallstack)
					{
						InProcess->ErrorLogStage = EErrorLogStage::ELS_ErrorExit;
					}
				}

				// The rest of the lines, after callstack parsing, should be ELS_ErrorExit logs - no need to verify these


				InProcess->ErrorText.Add(FErrorLog(InProcess->ErrorLogStage, CurLine));
			}
		}
	}
}

void UProcessUnitTest::UnitTick(float DeltaTime)
{
	Super::UnitTick(DeltaTime);

	// Detect processes which indicate a blocking task, and reset timeouts while this is occurring
	if ((FPlatformTime::Seconds() - LastBlockingProcessCheck) > 5.0 && IsBlockingProcessPresent(true))
	{
		ResetTimeout(TEXT("Blocking Child Process"), true, 60);

		LastBlockingProcessCheck = FPlatformTime::Seconds();
	}


	PollProcessOutput();

	// Update/save memory stats
	UpdateProcessStats();
}

void UProcessUnitTest::PostUnitTick(float DeltaTime)
{
	Super::PostUnitTick(DeltaTime);

	TArray<TSharedPtr<FUnitTestProcess>> HandlesPendingShutdown;

	for (auto CurHandle : ActiveProcesses)
	{
		if (CurHandle.IsValid() && !FPlatformProcess::IsProcRunning(CurHandle->ProcessHandle))
		{
			FString LogMsg = FString::Printf(TEXT("Process '%s' has finished."), *CurHandle->ProcessTag);

			UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
			UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

			HandlesPendingShutdown.Add(CurHandle);
		}
	}

	for (auto CurProcHandle : HandlesPendingShutdown)
	{
		NotifyProcessFinished(CurProcHandle);
	}


	// Finally, clean up all handles pending shutdown (if they aren't already cleaned up)
	if (HandlesPendingShutdown.Num() > 0)
	{
		for (auto CurHandle : HandlesPendingShutdown)
		{
			if (CurHandle.IsValid())
			{
				ShutdownUnitTestProcess(CurHandle);
			}
		}

		HandlesPendingShutdown.Empty();
	}
}

bool UProcessUnitTest::IsTickable() const
{
	bool bReturnVal = Super::IsTickable();

	bReturnVal = bReturnVal || ActiveProcesses.Num() > 0;

	return bReturnVal;
}


void UProcessUnitTest::PollProcessOutput()
{
	for (auto CurHandle : ActiveProcesses)
	{
		if (CurHandle.IsValid())
		{
			// Process any log/stdout data
			FString LogDump = TEXT("");
			bool bProcessedPipeRead = false;

			// Need to iteratively grab pipe data, as it has a buffer (can miss data near process-end, for large logs, e.g. stack dumps)
			while (true)
			{
				FString CurStdOut = FPlatformProcess::ReadPipe(CurHandle->ReadPipe);

				if (CurStdOut.Len() > 0)
				{
					LogDump += CurStdOut;
					bProcessedPipeRead = true;
				}
				// Sometimes large reads (typically > 4096 on the write side) clog the pipe buffer,
				// and even looped reads won't receive it all, so when a large enough amount of data gets read, sleep momentarily
				// (not ideal, but it works - spent a long time trying to find a better solution)
				else if (bProcessedPipeRead && LogDump.Len() > 2048)
				{
					bProcessedPipeRead = false;

					// Limit blocking pipe sleeps, to 5 seconds per every minute, to limit UI freezes
					static double LastPipeCounterReset = 0.0;
					static uint32 PipeCounter = 0;
					const float PipeDelay = 0.1f;
					const float MaxPipeDelay = 5.0f;

					double CurTime = FPlatformTime::Seconds();

					if ((CurTime - LastPipeCounterReset) - 60.0 >= 0)
					{
						LastPipeCounterReset = CurTime;
						PipeCounter = 0;
					}

					if (MaxPipeDelay - (PipeCounter * PipeDelay) > 0.0f)
					{
						FPlatformProcess::Sleep(PipeDelay);
						PipeCounter++;
					}
					else
					{
						// NOTE: This MUST be set to 0.0, because in extreme circumstances (such as DDoS outputting lots of log data),
						//			this can actually block the current thread, due to the sleep being too long
						FPlatformProcess::Sleep(0.0f);
					}
				}
				else
				{
					break;
				}
			}

			if (LogDump.Len() > 0)
			{
				// Every log line should start with an endline, so if one is missing, print that into the log as an error
				bool bPartialRead = !LogDump.StartsWith(FString(LINE_TERMINATOR));
				FString PartialLog = FString::Printf(TEXT("--MISSING ENDLINE - PARTIAL PIPE READ (First 32 chars: %s)--"),
														*LogDump.Left(32));

				// @todo #JohnB: I don't remember why I implemented this with StartsWith, but it worked for ~3-4 years,
				//					and now it is throwing up 'partial pipe read' errors for Fortnite,
				//					and I can't figure out why it worked at all, and why it's not working now.
#if 1
				bPartialRead = !LogDump.EndsWith(FString(LINE_TERMINATOR));
#endif

				// Now split up the log into multiple lines
				TArray<FString> LogLines;
				
				// @todo #JohnB: Perhaps add support for both platforms line terminator, at some stage
#if TARGET_UE4_CL < CL_STRINGPARSEARRAY
				LogDump.ParseIntoArray(&LogLines, LINE_TERMINATOR, true);
#else
				LogDump.ParseIntoArray(LogLines, LINE_TERMINATOR, true);
#endif


				// For process logs, replace the timestamp/category with a tag (e.g. [SERVER]), and set a unique colour so it stands out
				ELogTimes::Type bPreviousPrintLogTimes = GPrintLogTimes;
				GPrintLogTimes = ELogTimes::None;

				SET_WARN_COLOR(CurHandle->MainLogColor);

				// Clear the engine-event log hook, to prevent duplication of the below log entry
				UNIT_EVENT_CLEAR;

				if (bPartialRead)
				{
					UE_LOG(NetCodeTestNone, Log, TEXT("%s"), *PartialLog);
				}

				for (int LineIdx=0; LineIdx<LogLines.Num(); LineIdx++)
				{
					UE_LOG(NetCodeTestNone, Log, TEXT("%s%s"), *CurHandle->LogPrefix, *(LogLines[LineIdx]));
				}

				// Output to the unit log file
				if (UnitLog.IsValid())
				{
					if (bPartialRead)
					{
						UnitLog->Serialize(*PartialLog, ELogVerbosity::Log, NAME_None);
					}

					for (int LineIdx=0; LineIdx<LogLines.Num(); LineIdx++)
					{
						NUTUtil::SpecialLog(UnitLog.Get(), *CurHandle->LogPrefix, *(LogLines[LineIdx]), ELogVerbosity::Log, NAME_None);
					}
				}

				// Restore the engine-event log hook
				UNIT_EVENT_RESTORE;

				CLEAR_WARN_COLOR();

				GPrintLogTimes = bPreviousPrintLogTimes;


				// Also output to the unit test log window
				if (LogWindow.IsValid())
				{
					TSharedPtr<SLogWidget>& LogWidget = LogWindow->LogWidget;

					if (LogWidget.IsValid())
					{
						if (bPartialRead)
						{
							LogWidget->AddLine(CurHandle->BaseLogType, MakeShareable(new FString(PartialLog)),
												CurHandle->SlateLogColor);
						}

						for (int LineIdx=0; LineIdx<LogLines.Num(); LineIdx++)
						{
							FString LogLine = CurHandle->LogPrefix + LogLines[LineIdx];

							LogWidget->AddLine(CurHandle->BaseLogType, MakeShareable(new FString(LogLine)), CurHandle->SlateLogColor);
						}
					}
				}


				// Now trigger notification of log lines (only if the unit test is not yet verified though)
				if (VerificationState == EUnitTestVerification::Unverified)
				{
					NotifyProcessLog(CurHandle, LogLines);
				}


				// If the verification state has not changed, pass on the log lines to the error checking code
				CheckOutputForError(CurHandle, LogLines);
			}
		}
	}
}

bool UProcessUnitTest::IsBlockingProcessPresent(bool bLogIfFound/*=false*/)
{
	bool bReturnVal = false;
	const TArray<FString>* BlockingProcesses = NULL;

	UnitEnv->GetProgressBlockingProcesses(BlockingProcesses);


	// Map all process id's and parent ids, to generate the entire child process tree
	TMap<uint32, uint32> ProcMap;

	{
		FPlatformProcess::FProcEnumerator ProcIt;

		while (ProcIt.MoveNext())
		{
			auto CurProc = ProcIt.GetCurrent();
			ProcMap.Add(CurProc.GetPID(), CurProc.GetParentPID());
		}
	}


	TArray<uint32> ChildProcs;
	ChildProcs.Add(FPlatformProcess::GetCurrentProcessId());

	for (int32 i=0; i<ChildProcs.Num(); i++)
	{
		uint32 SearchParentId = ChildProcs[i];

		for (TMap<uint32, uint32>::TConstIterator It(ProcMap); It; ++It)
		{
			if (It.Value() == SearchParentId)
			{
				ChildProcs.AddUnique(It.Key());
			}
		}
	}


	// Now check if any of those child processes are a blocking process
	TArray<FString> BlockingProcNames;

	{
		FPlatformProcess::FProcEnumerator ProcIt;

		while (ProcIt.MoveNext())
		{
			auto CurProc = ProcIt.GetCurrent();

			if (ChildProcs.Contains(CurProc.GetPID()))
			{
				FString CurProcName = CurProc.GetName();
				int32 Delim = CurProcName.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);

				if (Delim != INDEX_NONE)
				{
					CurProcName = CurProcName.Left(Delim);
				}

				if (BlockingProcesses->Contains(CurProcName))
				{
					bReturnVal = true;
					BlockingProcNames.AddUnique(CurProcName);
				}
			}
		}
	}

	if (bLogIfFound && bReturnVal)
	{
		FString ProcString;

		for (auto CurProc : BlockingProcNames)
		{
			if (ProcString.Len() != 0)
			{
				ProcString += TEXT(", ");
			}

			ProcString += CurProc;
		}

		UNIT_LOG(ELogType::StatusImportant, TEXT("Detected blocking child process '%s'."), *ProcString);


		// Longstanding source of issues, with running unit tests - haven't found a good fully-automated solution yet
		if (BlockingProcNames.Contains(TEXT("ShaderCompileWorker")))
		{
			UNIT_LOG(ELogType::StatusImportant,
					TEXT("Recommend generating shaders separately before running unit tests, to avoid bugs and unit test failure."));

			UNIT_LOG(ELogType::StatusImportant,
					TEXT("One way to do this (takes a long time), is through: UE4Editor-Cmd YourGame -run=DerivedDataCache -fill"));
		}
	}

	return bReturnVal;
}

void UProcessUnitTest::FinishDestroy()
{
	Super::FinishDestroy();

	// Force close any processes still running
	for (int32 i=ActiveProcesses.Num()-1; i>=0; i--)
	{
		if (ActiveProcesses[i].IsValid())
		{
			ShutdownUnitTestProcess(ActiveProcesses[i]);
		}
	}
}

void UProcessUnitTest::ShutdownAfterError()
{
	Super::ShutdownAfterError();

	// Force close any processes still running
	for (int32 i=ActiveProcesses.Num()-1; i>=0; i--)
	{
		if (ActiveProcesses[i].IsValid())
		{
			ShutdownUnitTestProcess(ActiveProcesses[i]);
		}
	}
}

