// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealWatchdog.h"
#include "RequiredProgramMainCPPInclude.h"
#include "IAnalyticsProviderET.h"
#include "WatchdogAnalytics.h"
#include "ExceptionHandling.h"
#include "Misc/EngineBuildSettings.h"

IMPLEMENT_APPLICATION(UnrealWatchdog, "UnrealWatchdog");
DEFINE_LOG_CATEGORY(UnrealWatchdogLog);

#define LOCTEXT_NAMESPACE "UnrealWatchdog"

namespace WatchdogDefs
{
	static const FString StoreId(TEXT("Epic Games"));
	static const FString WatchdogRecordSectionPrefix(TEXT("Unreal Engine/Watchdog/"));
	static const FString WatchdogVersionString(TEXT("1_0"));
	static const FString CommandLineStoreKey(TEXT("CommandLine"));
	static const FString StartupTimeStoreKey(TEXT("StartupTimestamp"));
	static const FString TimestampStoreKey(TEXT("Timestamp"));
	static const FString StatusStoreKey(TEXT("LastExecutionState"));
	static const FString UserActivityStoreKey(TEXT("CurrentUserActivity"));
	static const FString WasDebuggerStoreKey(TEXT("WasEverDebugger"));
	static const FString RunningSessionToken(TEXT("Running"));
	static const FString ShutdownSessionToken(TEXT("Shutdown"));
	static const FString CrashSessionToken(TEXT("Crashed"));
	static const FString TrueValueString(TEXT("1"));

	static const FTimespan SendWatchdogHeartbeatPeriod(0, 5, 0);
	static const FTimespan CheckParentRunningPeriod(0, 0, 10);
	static const float TickSleepSeconds = 2.0f;
}

struct FWatchdogStoredValues
{
	FString CommandLine;
	FString StartTime;
	FString LastTimestamp;
	FString ExecutionStatus;
	FString UserActivity;
	FString WasDebugged;
};

void GetCommonEventAttributes(const FWatchdogCommandLine& CommandLine, TArray< FAnalyticsEventAttribute >& OutAttributes)
{
	OutAttributes.Add(FAnalyticsEventAttribute(TEXT("RunType"), CommandLine.RunType));
	OutAttributes.Add(FAnalyticsEventAttribute(TEXT("ProjectName"), CommandLine.ProjectName));
	OutAttributes.Add(FAnalyticsEventAttribute(TEXT("Platform"), CommandLine.PlatformName));
	OutAttributes.Add(FAnalyticsEventAttribute(TEXT("SessionId"), CommandLine.SessionId));
	OutAttributes.Add(FAnalyticsEventAttribute(TEXT("EngineVersion"), CommandLine.EngineVersion));
}

bool GetWatchdogStoredDebuggerValue(const FString& WatchdogSectionName)
{
	FString WasDebuggedString;
	FPlatformMisc::GetStoredValue(WatchdogDefs::StoreId, WatchdogSectionName, WatchdogDefs::WasDebuggerStoreKey, WasDebuggedString);
	return WasDebuggedString == WatchdogDefs::TrueValueString;
}

FWatchdogStoredValues GetWatchdogStoredValuesAndDelete(const FString& WatchdogSectionName)
{
	FWatchdogStoredValues StoredValues;

	FPlatformMisc::GetStoredValue(WatchdogDefs::StoreId, WatchdogSectionName, WatchdogDefs::CommandLineStoreKey, StoredValues.CommandLine);
	FPlatformMisc::GetStoredValue(WatchdogDefs::StoreId, WatchdogSectionName, WatchdogDefs::StartupTimeStoreKey, StoredValues.StartTime);
	FPlatformMisc::GetStoredValue(WatchdogDefs::StoreId, WatchdogSectionName, WatchdogDefs::TimestampStoreKey, StoredValues.LastTimestamp);
	FPlatformMisc::GetStoredValue(WatchdogDefs::StoreId, WatchdogSectionName, WatchdogDefs::StatusStoreKey, StoredValues.ExecutionStatus);
	FPlatformMisc::GetStoredValue(WatchdogDefs::StoreId, WatchdogSectionName, WatchdogDefs::UserActivityStoreKey, StoredValues.UserActivity);
	FPlatformMisc::GetStoredValue(WatchdogDefs::StoreId, WatchdogSectionName, WatchdogDefs::WasDebuggerStoreKey, StoredValues.WasDebugged);

	FPlatformMisc::DeleteStoredValue(WatchdogDefs::StoreId, WatchdogSectionName, WatchdogDefs::CommandLineStoreKey);
	FPlatformMisc::DeleteStoredValue(WatchdogDefs::StoreId, WatchdogSectionName, WatchdogDefs::StartupTimeStoreKey);
	FPlatformMisc::DeleteStoredValue(WatchdogDefs::StoreId, WatchdogSectionName, WatchdogDefs::TimestampStoreKey);
	FPlatformMisc::DeleteStoredValue(WatchdogDefs::StoreId, WatchdogSectionName, WatchdogDefs::StatusStoreKey);
	FPlatformMisc::DeleteStoredValue(WatchdogDefs::StoreId, WatchdogSectionName, WatchdogDefs::UserActivityStoreKey);
	FPlatformMisc::DeleteStoredValue(WatchdogDefs::StoreId, WatchdogSectionName, WatchdogDefs::WasDebuggerStoreKey);

	return StoredValues;
}

FString GetWatchdogStoredTimestamp(const FString& WatchdogSectionName)
{
	FString Timestamp;
	FPlatformMisc::GetStoredValue(WatchdogDefs::StoreId, WatchdogSectionName, WatchdogDefs::TimestampStoreKey, Timestamp);
	return Timestamp;
}

/**
 * @EventName UnrealWatchdog.Initialized
 *
 * @Trigger Event raised by UnrealWatchdog at startup. Records an instance of the watchdog running and whether the watched process was found successfully.
 *
 * @Type Static
 * @Owner Chris.Wood
 *
 * @EventParam RunType - Editor or Game
 * @EventParam ProjectName - Project for the session that abnormally terminated.
 * @EventParam Platform - Windows, Mac, Linux
 * @EventParam SessionId - Analytics SessionID of the session that abnormally terminated.
 * @EventParam EngineVersion - EngineVersion of the session that abnormally terminated.
 * @EventParam bValidPID - Was the Watchdog passed a valid PID on the command line? (True or False)
 * @EventParam bProcessFound - Was the watched process running when the Watchdog started up? (True or False)
 */
void SendStartupEvent(IAnalyticsProviderET& Analytics, const FWatchdogCommandLine& CommandLine, bool bInitiallyRunning)
{
	TArray< FAnalyticsEventAttribute > StartupAttributes;
	GetCommonEventAttributes(CommandLine, StartupAttributes);
	StartupAttributes.Add(FAnalyticsEventAttribute(TEXT("bValidPID"), CommandLine.bHasProcessId ? TEXT("True") : TEXT("False")));
	StartupAttributes.Add(FAnalyticsEventAttribute(TEXT("bProcessFound"), bInitiallyRunning ? TEXT("True") : TEXT("False")));

	UE_LOG(UnrealWatchdogLog, Log, TEXT("Sending event UnrealWatchdog.Initialized"));
	Analytics.RecordEvent(TEXT("UnrealWatchdog.Initialized"), MoveTemp(StartupAttributes));
}

/**
 * @EventName UnrealWatchdog.Heartbeat
 *
 * @Trigger Event raised by a running UnrealWatchdog process while waiting for a process to exit.
 *
 * @Type Static
 * @Owner Chris.Wood
 *
 * @EventParam RunType - Editor or Game
 * @EventParam ProjectName - Project for the session.
 * @EventParam Platform - Windows, Mac, Linux
 * @EventParam SessionId - Analytics SessionID of the session.
 * @EventParam EngineVersion - EngineVersion of the session.
 */
void SendHeartbeatEvent(IAnalyticsProviderET& Analytics, const FWatchdogCommandLine& CommandLine)
{
	static TArray< FAnalyticsEventAttribute > HeartbeatAttributes;
	static FDateTime LastEventTime;
	if (HeartbeatAttributes.Num() == 0)
	{
		// Init
		GetCommonEventAttributes(CommandLine, HeartbeatAttributes);
		HeartbeatAttributes.Add(FAnalyticsEventAttribute(TEXT("IntervalSec"), 0.0f));
		LastEventTime = FDateTime::UtcNow();
	}
	else
	{
		// Update the IntervalSec with the exact time since the last heartbeat
		FDateTime EventTime = FDateTime::UtcNow();
		FTimespan ActualInterval = EventTime - LastEventTime;
		HeartbeatAttributes.RemoveAt(HeartbeatAttributes.Num() - 1);
		HeartbeatAttributes.Add(FAnalyticsEventAttribute(TEXT("IntervalSec"), ActualInterval.GetTotalSeconds()));
		LastEventTime = EventTime;
	}

	UE_LOG(UnrealWatchdogLog, Verbose, TEXT("Sending event UnrealWatchdog.Heartbeat"));
	Analytics.RecordEvent(TEXT("UnrealWatchdog.Heartbeat"), CopyTemp(HeartbeatAttributes));
}

bool CheckParentHeartbeat(IAnalyticsProviderET& Analytics, const FWatchdogCommandLine& CommandLine, const FString& WatchdogSectionName)
{
	static FString HeartbeatString;
	FString NextHeartbeatString = GetWatchdogStoredTimestamp(WatchdogSectionName);
	if (HeartbeatString != NextHeartbeatString)
	{
		HeartbeatString = NextHeartbeatString;
		return true;
	}
	return false;
}

/**
 * @EventName UnrealWatchdog.HangDetected
 *
 * @Trigger Event raised when the watchdog detects an interruption in the heartbeat of the watched process.
 *
 * @Type Static
 * @Owner Chris.Wood
 *
 * @EventParam RunType - Editor or Game
 * @EventParam ProjectName - Project for the session.
 * @EventParam Platform - Windows, Mac, Linux
 * @EventParam SessionId - Analytics SessionID of the session.
 * @EventParam EngineVersion - EngineVersion of the session.
 * @EventParam HangUserResponse - Indicates the response, if any, from the user when asked about the hang by the watchdog (Unattended, Confirmed, False)
 * @EventParam AlreadyRecoveredUserResponse - If the user responded by confirming the hang (HangUserResponse=Confirmed), indicates the response when asked if the hang has recovered (Recovered, NotRecovered, N/A)
 */
void SendHangDetectedEvent(IAnalyticsProviderET& Analytics, const FWatchdogCommandLine& CommandLine)
{
	TArray< FAnalyticsEventAttribute > HangAttributes;
	GetCommonEventAttributes(CommandLine, HangAttributes);

	// Internal builds should popup dialogs for hangs
	FString HangResponse = TEXT("Unattended");
	FString RecoveredResponse = TEXT("Unattended");
	if (CommandLine.bAllowDialogs)
	{
		FText SessionLabel = FText::Format(LOCTEXT("HangSessionLabel", "{0} ({1})"), FText::FromString(CommandLine.ProjectName), FText::FromString(CommandLine.RunType));
		FText MessageTitleFormat(LOCTEXT("WatchdogPopupTitleHang", "{0} is unresponsive"));
		FText MessageTitle = FText::Format(MessageTitleFormat, SessionLabel);
		FText Message(LOCTEXT("WatchdogPopupQuestionHang", "We think the application may be hanging. Did the application freeze without closing or showing the Crash Reporter?"));

		EAppReturnType::Type HangAnswer = FMessageDialog::Open(EAppMsgType::YesNo, Message, &MessageTitle);

		if (HangAnswer == EAppReturnType::Yes)
		{
			UE_LOG(UnrealWatchdogLog, Log, TEXT("User confirmed hang"));
			HangResponse= TEXT("Confirmed");

			FText RecoveredMessage(LOCTEXT("WatchdogPopupRecoveredMessage", "Has the application recovered?"));

			EAppReturnType::Type RecoveredAnswer = FMessageDialog::Open(EAppMsgType::YesNo, RecoveredMessage, &MessageTitle);

			if (RecoveredAnswer == EAppReturnType::Yes)
			{
				UE_LOG(UnrealWatchdogLog, Log, TEXT("User confirmed recovery from hang"));
				RecoveredResponse = TEXT("Recovered");
			}
			else
			{
				UE_LOG(UnrealWatchdogLog, Log, TEXT("User confirmed hang not yet recovered"));
				RecoveredResponse = TEXT("NotRecovered");
			}

			FText CRCMessage(LOCTEXT("WatchdogPopupHangCRCMessage", "We will now open the Crash Reporter for you to tell us what happened."));
			FMessageDialog::Open(EAppMsgType::Ok, CRCMessage, &MessageTitle);

			FString EnsureText = FString::Printf(TEXT("Watchdog detected hang in %s."), *SessionLabel.ToString());
			ReportInteractiveEnsure(*EnsureText);
		}
		else  // HangAnswer == EAppReturnType::No
		{
			UE_LOG(UnrealWatchdogLog, Warning, TEXT("User didn't witness hang. False positive warning!"));
			HangResponse = TEXT("False");
			RecoveredResponse = TEXT("N/A");
		}
	}

	HangAttributes.Add(FAnalyticsEventAttribute(TEXT("HangUserResponse"), HangResponse));
	HangAttributes.Add(FAnalyticsEventAttribute(TEXT("AlreadyRecoveredUserResponse"), RecoveredResponse));
	UE_LOG(UnrealWatchdogLog, Log, TEXT("Sending event UnrealWatchdog.HangDetected"));
	Analytics.RecordEvent(TEXT("UnrealWatchdog.HangDetected"), MoveTemp(HangAttributes));
}

/**
 * @EventName UnrealWatchdog.HangRecovered
 *
 * @Trigger Event raised when the watchdog detects that the heartbeat of the watched process has resumed updating after a hang was detected.
 *
 * @Type Static
 * @Owner Chris.Wood
 *
 * @EventParam RunType - Editor or Game
 * @EventParam ProjectName - Project for the session.
 * @EventParam Platform - Windows, Mac, Linux
 * @EventParam SessionId - Analytics SessionID of the session.
 * @EventParam EngineVersion - EngineVersion of the session.
 */
void SendHangRecoveredEvent(IAnalyticsProviderET& Analytics, const FWatchdogCommandLine& CommandLine)
{
	TArray< FAnalyticsEventAttribute > HangAttributes;
	GetCommonEventAttributes(CommandLine, HangAttributes);

	UE_LOG(UnrealWatchdogLog, Log, TEXT("Sending event UnrealWatchdog.HangRecovered"));
	Analytics.RecordEvent(TEXT("UnrealWatchdog.HangRecovered"), MoveTemp(HangAttributes));
}

/**
 * @EventName UnrealWatchdog.Shutdown
 *
 * @Trigger Event raised by UnrealWatchdog at shutdown. The watched process has exited (or wasn't found).
 *
 * @Type Static
 * @Owner Chris.Wood
 *
 * @EventParam RunType - Editor or Game
 * @EventParam ProjectName - Project for the session that abnormally terminated.
 * @EventParam Platform - Windows, Mac, Linux
 * @EventParam SessionId - Analytics SessionID of the session that abnormally terminated.
 * @EventParam EngineVersion - EngineVersion of the session that abnormally terminated.
 * @EventParam bReturnCodeObtained - Did the Watchdog get the return/exit code from the process? (True or False)
 * @EventParam OSReturnCode - The exit code of the watched process.
 * @EventParam CommandLine - Full command line that ran the watched process.
 * @EventParam StartTime - The time the watched process initialized the watchdog.
 * @EventParam LastTimestamp - The last time the watched process updated its stored status (not necessarily the shutdown time).
 * @EventParam LastExecutionStatus - The last updated status of the watched process when it was running. (Running, Shutdown, Crashed or Hang)
 * @EventParam LastUserActivity - The last updated user activity, if any, of the watched process.
 * @EventParam AbnormalShutdownUserResponse - Indicates the response, if any, from the user when asked about the shutdown by the watchdog (Unattended, Confirmed, False)
 */
void SendShutdownEvent(IAnalyticsProviderET& Analytics, const FWatchdogCommandLine& CommandLine, bool bReturnCodeObtained, uint32 ReturnCode, FAnalyticsEventAttribute& UserResponse, const FWatchdogStoredValues& StoredValues, const FDateTime& StartupTime)
{
	TArray< FAnalyticsEventAttribute > ShutdownAttributes;
	GetCommonEventAttributes(CommandLine, ShutdownAttributes);
	ShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("bReturnCodeObtained"), bReturnCodeObtained ? TEXT("True") : TEXT("False")));
	ShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("OSReturnCode"), ReturnCode));
	ShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("CommandLine"), StoredValues.CommandLine));
	ShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("StartTime"), StoredValues.StartTime));
	ShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("LastTimestamp"), StoredValues.LastTimestamp));
	ShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("LastExecutionStatus"), StoredValues.ExecutionStatus));
	ShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("LastUserActivity"), StoredValues.UserActivity));
	ShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("WasDebugged"), StoredValues.WasDebugged));
	ShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("TotalRunTimeSeconds"), (int)((FDateTime::UtcNow() - StartupTime).GetTotalSeconds())));
	ShutdownAttributes.Add(UserResponse);

	UE_LOG(UnrealWatchdogLog, Log, TEXT("Sending event UnrealWatchdog.Shutdown"));
	Analytics.RecordEvent(TEXT("UnrealWatchdog.Shutdown"), MoveTemp(ShutdownAttributes));
}

void TickHeartbeat(IAnalyticsProviderET& Analytics, const FWatchdogCommandLine& CommandLine, FDateTime& InOutNextHeartbeatSend)
{
	// Send heartbeat due?
	while (FDateTime::UtcNow() >= InOutNextHeartbeatSend)		// "while" allows watchdog to send heartbeats missed during a modal dialog popup 
	{
		InOutNextHeartbeatSend += WatchdogDefs::SendWatchdogHeartbeatPeriod;
		SendHeartbeatEvent(Analytics, CommandLine);
	}
}

bool TickProcessCheck(const FWatchdogCommandLine& CommandLine, FProcHandle& InParentProcess, FDateTime& InOutNextProcessCheck, bool& OutGotProcReturnCode, int32& OutReturnCode)
{
	// Parent application still running?
	if (FDateTime::UtcNow() >= InOutNextProcessCheck)
	{	
		if (!FPlatformProcess::IsApplicationRunning(CommandLine.ParentProcessId))
		{
			UE_LOG(UnrealWatchdogLog, Log, TEXT("Watchdog detected terminated process PID %u"), CommandLine.ParentProcessId);
			OutGotProcReturnCode = FPlatformProcess::GetProcReturnCode(InParentProcess, &OutReturnCode);
			return false;
		}

		InOutNextProcessCheck = FDateTime::UtcNow() + WatchdogDefs::CheckParentRunningPeriod;
	}
	return true;
}

void TickHangCheck(IAnalyticsProviderET& Analytics, const FWatchdogCommandLine& CommandLine, FDateTime& InOutNextHeartbeatCheck, const FString& WatchdogSectionName, bool& bOutHang)
{
	if (FDateTime::UtcNow() >= InOutNextHeartbeatCheck)
	{
		bool bHangDetected = !CheckParentHeartbeat(Analytics, CommandLine, WatchdogSectionName);

		if (bHangDetected && !bOutHang)
		{
			// New hang
			SendHangDetectedEvent(Analytics, CommandLine);
			bOutHang = true;
		}
		else if (!bHangDetected && bOutHang)
		{
			// Previous hang recovered
			SendHangRecoveredEvent(Analytics, CommandLine);
			bOutHang = false;
		}

		static const FTimespan CheckParentHeartbeatPeriod(0, 0, CommandLine.HangThresholdSeconds);
		InOutNextHeartbeatCheck = FDateTime::UtcNow() + CheckParentHeartbeatPeriod;
	}
}

bool WaitForProcess(IAnalyticsProviderET& Analytics, const FWatchdogCommandLine& CommandLine, int32& OutReturnCode, bool& bOutHang, const FString& WatchdogSectionName)
{
	FDateTime NextHeartbeatSend = FDateTime::UtcNow();
	FDateTime NextProcessCheck = FDateTime::UtcNow();
	FDateTime NextHeartbeatCheck = FDateTime::UtcNow();
	bool bSuccess = false;
	bOutHang = false;
	OutReturnCode = -1;

	FProcHandle ParentProcess = GetProcessHandle(CommandLine);
	if (ParentProcess.IsValid())
	{
		while (!GIsRequestingExit)
		{
			TickHeartbeat(Analytics, CommandLine, NextHeartbeatSend);

			if (!TickProcessCheck(CommandLine, ParentProcess, NextProcessCheck, bSuccess, OutReturnCode))
			{
				// process not running
				break;
			}

			if (CommandLine.bAllowDetectHangs)
			{
				// Check parent heartbeat for hang?
				TickHangCheck(Analytics, CommandLine, NextHeartbeatCheck, WatchdogSectionName, bOutHang);
			}

			FPlatformProcess::Sleep(WatchdogDefs::TickSleepSeconds);
		}

		FPlatformProcess::CloseProc(ParentProcess);
	}
	else
	{
		UE_LOG(UnrealWatchdogLog, Error, TEXT("Watchdog failed to get handle to process PID %u"), CommandLine.ParentProcessId);
	}

	return bSuccess;
}

/**
* @EventName UnrealWatchdog.AbnormalShutdown
*
* @Trigger Event raised by UnrealWatchdog as part of MTBF tracking. Records an instance of a watchdog protected process shutting down abnormally.
*
* @Type Static
* @Owner Chris.Wood
*
* @EventParam RunType - Editor or Game
* @EventParam ProjectName - Project for the session that abnormally terminated.
* @EventParam Platform - Windows, Mac, Linux
* @EventParam SessionId - Analytics SessionID of the session that abnormally terminated.
* @EventParam EngineVersion - EngineVersion of the session that abnormally terminated.
* @EventParam UserResponse - What did the user click if and when the watchdog showed a message box. One of Confirmed, False or Unattended.
*/
int RunUnrealWatchdog(const TCHAR* CommandLine)
{
	// start up the main loop
	GEngineLoop.PreInit(CommandLine);
	FDateTime StartupTime = FDateTime::UtcNow();

	check(GConfig && GConfig->IsReadyForUse());

	FWatchdogCommandLine WatchdogCommandLine(CommandLine);
	bool bInitiallyRunning = WatchdogCommandLine.bHasProcessId && FPlatformProcess::IsApplicationRunning(WatchdogCommandLine.ParentProcessId);

	// Send watchdog startup event
	FWatchdogAnalytics::Initialize();
	IAnalyticsProviderET& Analytics = FWatchdogAnalytics::GetProvider();
	SendStartupEvent(Analytics, WatchdogCommandLine, bInitiallyRunning);

	if (!WatchdogCommandLine.bHasProcessId)
	{
		UE_LOG(UnrealWatchdogLog, Error, TEXT("Watchdog wasn't given a valid PID"));
	}
	else if (!bInitiallyRunning)
	{
		UE_LOG(UnrealWatchdogLog, Warning, TEXT("Watchdog didn't find running process PID %u..."), WatchdogCommandLine.ParentProcessId);
	}
	else
	{
		UE_LOG(UnrealWatchdogLog, Log, TEXT("Watchdog beginning to watch PID %u..."), WatchdogCommandLine.ParentProcessId);
	}

	FString WatchdogSectionName = FString::Printf(TEXT("%s%s/%u"), *WatchdogDefs::WatchdogRecordSectionPrefix, *WatchdogDefs::WatchdogVersionString, WatchdogCommandLine.ParentProcessId);

	// Wait for the process to exit
	int32 ReturnCode = WatchdogCommandLine.SuccessReturnCode;
	bool bHang = false;
	bool bReturnCodeObtained = WaitForProcess(Analytics, WatchdogCommandLine, ReturnCode, bHang, WatchdogSectionName);

	// Read any stored values from process
	FWatchdogStoredValues StoredValues = GetWatchdogStoredValuesAndDelete(WatchdogSectionName);
	if (bHang)
	{
		StoredValues.ExecutionStatus = TEXT("Hang");
	}

	// Optional section for dialogs and CRC in internal builds
	FString ShutdownResponse = TEXT("Unattended");
	if (WatchdogCommandLine.bAllowDialogs && !bHang && StoredValues.WasDebugged != WatchdogDefs::TrueValueString)
	{
		EAppReturnType::Type UserAnswer = EAppReturnType::Cancel;
		FText SessionLabel = FText::Format(LOCTEXT("SessionLabel", "{0} ({1})"), FText::FromString(WatchdogCommandLine.ProjectName), FText::FromString(WatchdogCommandLine.RunType));
		FString EnsureText;

		if (StoredValues.ExecutionStatus == WatchdogDefs::CrashSessionToken)
		{
			// Crashed status
			FText MessageTitleFormat(LOCTEXT("WatchdogPopupTitleCrashed", "{0} crashed"));
			FText MessageTitle = FText::Format(MessageTitleFormat, SessionLabel);
			FText Message(LOCTEXT("WatchdogPopupQuestionCrashed", "We think a crash occurred, was handled correctly, and the Crash Reporter appeared. Please tell us if you saw the Crash Reporter?"));

			EnsureText = FString::Printf(TEXT("Watchdog detected crash in %s."), *SessionLabel.ToString());
			UserAnswer = FMessageDialog::Open(EAppMsgType::YesNo, Message, &MessageTitle);

			if (UserAnswer == EAppReturnType::Yes)
			{
				UE_LOG(UnrealWatchdogLog, Log, TEXT("User confirmed crash and crash report client"));
				ShutdownResponse = TEXT("Confirmed");
			}
			else if (UserAnswer == EAppReturnType::No)
			{
				UE_LOG(UnrealWatchdogLog, Warning, TEXT("User didn't witness crash and crash report client. False positive warning!"));
				ShutdownResponse = TEXT("False");

				FText CRCMessage(LOCTEXT("WatchdogPopupCRCMessage", "We will now open the Crash Reporter for you to tell us what happened."));
				FMessageDialog::Open(EAppMsgType::Ok, CRCMessage, &MessageTitle);

				ReportInteractiveEnsure(*EnsureText);
			}
		}
		else
		{
			// Non-crash status
			const bool bBadReturnCode = bReturnCodeObtained && ReturnCode != WatchdogCommandLine.SuccessReturnCode;

			if (StoredValues.ExecutionStatus == WatchdogDefs::RunningSessionToken)
			{
				// Abnormal shutdown status
				FText MessageTitle;
				FText Message;
				if (bBadReturnCode)
				{
					FText MessageTitleFormat(LOCTEXT("WatchdogPopupTitleAbnormalShutdown", "{0} terminated unexpectedly"));
					MessageTitle = FText::Format(MessageTitleFormat, SessionLabel);
					Message = LOCTEXT("WatchdogPopupQuestionAbnormalShutdown", "We think a crash was not handled correctly. Did the application crash without showing the Crash Reporter?");

					EnsureText = FString::Printf(TEXT("Watchdog detected abnormal shutdown and returned error code %d in %s."), ReturnCode, *SessionLabel.ToString());
				}
				else
				{
					FText MessageTitleFormat(LOCTEXT("WatchdogPopupTitlePossibleAbnormalShutdown", "{0} did not close correctly"));
					MessageTitle = FText::Format(MessageTitleFormat, SessionLabel);
					Message = LOCTEXT("WatchdogPopupQuestionPossibleAbnormalShutdown", "We think we exited normally but didn't cleanup correctly. Did the application crash without showing the Crash Reporter?");

					EnsureText = FString::Printf(TEXT("Watchdog detected abnormal shutdown with successful return code %d in %s."), ReturnCode, *SessionLabel.ToString());
				}

				UserAnswer = FMessageDialog::Open(EAppMsgType::YesNo, Message, &MessageTitle);
			}
			else if (StoredValues.ExecutionStatus == WatchdogDefs::ShutdownSessionToken)
			{
				if (bBadReturnCode)
				{
					// Normal shutdown status with bad return code
					FText MessageTitleFormat(LOCTEXT("WatchdogPopupTitleNormalShutdown", "{0} returned an error code"));
					FText MessageTitle = FText::Format(MessageTitleFormat, SessionLabel);
					FText Message(LOCTEXT("WatchdogPopupQuestionNormalShutdown", "We think we shutdown correctly but returned an error code. Did the application crash without showing the Crash Reporter?"));

					EnsureText = FString::Printf(TEXT("Watchdog detected normal shutdown but returned error code %d in %s."), ReturnCode, *SessionLabel.ToString());
					UserAnswer = FMessageDialog::Open(EAppMsgType::YesNo, Message, &MessageTitle);
				}
			}
			else if (bBadReturnCode)
			{
				// Missing or unknown status with error code
				FText MessageTitleFormat(LOCTEXT("WatchdogPopupTitleUnknownError", "{0} returned an error code"));
				FText MessageTitle = FText::Format(MessageTitleFormat, SessionLabel);
				FText Message(LOCTEXT("WatchdogPopupQuestionUnknownError", "Did the application crash without showing the Crash Reporter?"));

				EnsureText = FString::Printf(TEXT("Watchdog detected unknown shutdown status and returned error code %d in %s."), ReturnCode, *SessionLabel.ToString());
				UserAnswer = FMessageDialog::Open(EAppMsgType::YesNo, Message, &MessageTitle);
			}
			else
			{
				// Missing or unknown status (no error code)
				FText MessageTitleFormat(LOCTEXT("WatchdogPopupTitleUnknownNoError", "{0} failed to signal the watchdog"));
				FText MessageTitle = FText::Format(MessageTitleFormat, SessionLabel);
				FText Message(LOCTEXT("WatchdogPopupQuestionUnknownNoError", "Process didn't communicate with this watchdog correctly. Did the application crash without showing the Crash Reporter?"));

				EnsureText = FString::Printf(TEXT("Watchdog detected unknown shutdown status with successful return code %d in %s."), ReturnCode, *SessionLabel.ToString());
				UserAnswer = FMessageDialog::Open(EAppMsgType::YesNo, Message, &MessageTitle);
			}

			if (UserAnswer == EAppReturnType::Yes)
			{
				UE_LOG(UnrealWatchdogLog, Log, TEXT("User confirmed abnormal shutdown"));
				ShutdownResponse = TEXT("Confirmed");

				FText MessageTitleFormat(LOCTEXT("WatchdogPopupTitleAbnormalShutdown", "{0} terminated unexpectedly"));
				FText MessageTitle = FText::Format(MessageTitleFormat, SessionLabel);
				FText Message(LOCTEXT("WatchdogPopupCRCMessage", "We will now open the Crash Reporter for you to tell us what happened."));

				FMessageDialog::Open(EAppMsgType::Ok, Message, &MessageTitle);

				ReportInteractiveEnsure(*EnsureText);
			}
			else if (UserAnswer == EAppReturnType::No)
			{
				UE_LOG(UnrealWatchdogLog, Warning, TEXT("User didn't witness abnormal shutdown. False positive warning!"));
				ShutdownResponse = TEXT("False");
			}
		}
	}

	// Send watchdog shutdown event
	UE_LOG(UnrealWatchdogLog, Log, TEXT("Watchdog watched process exited. bReturnCodeObtained=%s, ReturnCode=%u, RecordedShutdownType=%s"),
		bReturnCodeObtained ? TEXT("1") : TEXT("0"), ReturnCode, *StoredValues.ExecutionStatus);
	FAnalyticsEventAttribute UserResponse(TEXT("AbnormalShutdownUserResponse"), ShutdownResponse);
	SendShutdownEvent(Analytics, WatchdogCommandLine, bReturnCodeObtained, ReturnCode, UserResponse, StoredValues, StartupTime);

	// Shutdown tool and engine
	FWatchdogAnalytics::Shutdown();
	UE_LOG(UnrealWatchdogLog, Log, TEXT("Watchdog exiting"));
	FEngineLoop::AppPreExit();
	FModuleManager::Get().UnloadModulesAtShutdown();

	FEngineLoop::AppExit();
	
	return 0;
}

#undef LOCTEXT_NAMESPACE
