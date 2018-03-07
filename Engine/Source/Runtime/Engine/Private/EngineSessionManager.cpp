// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EngineSessionManager.h"
#include "Misc/Guid.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EngineVersion.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformProcess.h"
#include "EngineAnalytics.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "GeneralProjectSettings.h"
#include "UserActivityTracking.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "EngineBuildSettings.h"
#include "SlateApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "HAL/PlatformProcess.h"


#define LOCTEXT_NAMESPACE "SessionManager"

DEFINE_LOG_CATEGORY(LogEngineSessionManager);

namespace SessionManagerDefs
{
	static const FTimespan SessionRecordExpiration = FTimespan(25920000000000); // workaround for HTML5, was FTimespan::FromDays(30.0);
	static const FTimespan SessionRecordTimeout = FTimespan::FromMinutes(3.0);
	static const FTimespan GlobalLockWaitTimeout = FTimespan::FromSeconds(0.5);
	static const int HeartbeatPeriodSeconds(60);
	static const FString DefaultUserActivity(TEXT("Unknown"));
	static const FString StoreId(TEXT("Epic Games"));
	static const FString RunningSessionToken(TEXT("Running"));
	static const FString ShutdownSessionToken(TEXT("Shutdown"));
	static const FString CrashSessionToken(TEXT("Crashed"));
	static const FString DebuggerSessionToken(TEXT("Debugger"));
	static const FString AbnormalSessionToken(TEXT("AbnormalShutdown"));
	static const FString PS4SessionToken(TEXT("AbnormalShutdownPS4"));
	static const FString SessionRecordListSection(TEXT("List"));
	static const FString EditorSessionRecordSectionPrefix(TEXT("Unreal Engine/Editor Sessions/"));
	static const FString GameSessionRecordSectionPrefix(TEXT("Unreal Engine/Game Sessions/"));
	static const FString WatchdogRecordSectionPrefix(TEXT("Unreal Engine/Watchdog/"));
	static const FString SessionsVersionString(TEXT("1_3"));
	static const FString WatchdogVersionString(TEXT("1_0"));
	static const FString ModeStoreKey(TEXT("Mode"));
	static const FString ProjectNameStoreKey(TEXT("ProjectName"));
	static const FString CommandLineStoreKey(TEXT("CommandLine"));
	static const FString CrashStoreKey(TEXT("IsCrash"));
	static const FString GPUCrashStoreKey(TEXT("IsGPUCrash"));
	static const FString DeactivatedStoreKey(TEXT("IsDeactivated"));
	static const FString BackgroundStoreKey(TEXT("IsInBackground"));
	static const FString EngineVersionStoreKey(TEXT("EngineVersion"));
	static const FString TimestampStoreKey(TEXT("Timestamp"));
	static const FString StartupTimeStoreKey(TEXT("StartupTimestamp"));
	static const FString SessionIdStoreKey(TEXT("SessionId"));
	static const FString StatusStoreKey(TEXT("LastExecutionState"));
	static const FString DebuggerStoreKey(TEXT("IsDebugger"));
	static const FString WasDebuggerStoreKey(TEXT("WasEverDebugger"));
	static const FString UserActivityStoreKey(TEXT("CurrentUserActivity"));
	static const FString VanillaStoreKey(TEXT("IsVanilla"));
	static const FString GlobalLockName(TEXT("UE4_SessionManager_Lock"));
	static const FString FalseValueString(TEXT("0"));
	static const FString TrueValueString(TEXT("1"));
	static const FString EditorValueString(TEXT("Editor"));
	static const FString GameValueString(TEXT("Game"));
	static const FString UnknownProjectValueString(TEXT("UnknownProject"));
}

namespace
{
	FString TimestampToString(FDateTime InTimestamp)
	{
		return Lex::ToString(InTimestamp.ToUnixTimestamp());
	}

	FDateTime StringToTimestamp(FString InString)
	{
		int64 TimestampUnix;
		if (Lex::TryParseString(TimestampUnix, *InString))
		{
			return FDateTime::FromUnixTimestamp(TimestampUnix);
		}
		return FDateTime::MinValue();
	}
}

/* FEngineSessionManager */

void FEngineSessionManager::Initialize()
{
	// Register for crash and app state callbacks
	FCoreDelegates::OnHandleSystemError.AddRaw(this, &FEngineSessionManager::OnCrashing);
	FCoreDelegates::ApplicationHasReactivatedDelegate.AddRaw(this, &FEngineSessionManager::OnAppReactivate);
	FCoreDelegates::ApplicationWillDeactivateDelegate.AddRaw(this, &FEngineSessionManager::OnAppDeactivate);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FEngineSessionManager::OnAppBackground);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FEngineSessionManager::OnAppForeground);
	FUserActivityTracking::OnActivityChanged.AddRaw(this, &FEngineSessionManager::OnUserActivity);
	FCoreDelegates::IsVanillaProductChanged.AddRaw(this, &FEngineSessionManager::OnVanillaStateChanged);
	FSlateApplication::Get().GetOnModalLoopTickEvent().AddRaw(this, &FEngineSessionManager::Tick);
	
	const bool bFirstInitAttempt = true;
	InitializeRecords(bFirstInitAttempt);
}

void FEngineSessionManager::InitializeRecords(bool bFirstAttempt)
{
	if (!FEngineAnalytics::IsAvailable())
	{
		return;
	}

	TArray<FSessionRecord> SessionRecordsToReport;

	{
		// Scoped lock
		FSystemWideCriticalSection StoredValuesLock(SessionManagerDefs::GlobalLockName, bFirstAttempt ? SessionManagerDefs::GlobalLockWaitTimeout : FTimespan::Zero());

		// Get list of sessions in storage
		if (StoredValuesLock.IsValid() && BeginReadWriteRecords())
		{
			UE_LOG(LogEngineSessionManager, Verbose, TEXT("Initializing EngineSessionManager for abnormal shutdown tracking"));

			TArray<FSessionRecord> SessionRecordsToDelete;

			// Attempt check each stored session
			for (FSessionRecord& Record : SessionRecords)
			{
				FTimespan RecordAge = FDateTime::UtcNow() - Record.Timestamp;

				if (Record.bCrashed)
				{
					// Crashed sessions
					SessionRecordsToReport.Add(Record);
					SessionRecordsToDelete.Add(Record);
				}
				else if (RecordAge > SessionManagerDefs::SessionRecordExpiration)
				{
					// Delete expired session records
					SessionRecordsToDelete.Add(Record);
				}
				else if (RecordAge > SessionManagerDefs::SessionRecordTimeout)
				{
					// Timed out sessions
					SessionRecordsToReport.Add(Record);
					SessionRecordsToDelete.Add(Record);
				}
			}

			for (FSessionRecord& DeletingRecord : SessionRecordsToDelete)
			{
				DeleteStoredRecord(DeletingRecord);
			}

			// Create a session record for this session
			CreateAndWriteRecordForSession();

			// Update and release list of sessions in storage
			EndReadWriteRecords();

			bInitializedRecords = true;

			UE_LOG(LogEngineSessionManager, Log, TEXT("EngineSessionManager initialized"));
		}
	}

	for (FSessionRecord& ReportingSession : SessionRecordsToReport)
	{
		// Send error report for session that timed out or crashed
		SendAbnormalShutdownReport(ReportingSession);
	}
}

void FEngineSessionManager::Tick(float DeltaTime)
{
	HeartbeatTimeElapsed += DeltaTime;

	if (HeartbeatTimeElapsed > (float)SessionManagerDefs::HeartbeatPeriodSeconds && !bShutdown)
	{
		HeartbeatTimeElapsed = 0.0f;

		if (!bInitializedRecords)
		{
			// Try late initialization
			const bool bFirstInitAttempt = false;
			InitializeRecords(bFirstInitAttempt);
		}

		// Update timestamp in the session record for this session 
		if (bInitializedRecords)
		{	
			bool bIsDebuggerPresent = FPlatformMisc::IsDebuggerPresent();
			if (CurrentSession.bIsDebugger != bIsDebuggerPresent)
			{
				CurrentSession.bIsDebugger = bIsDebuggerPresent;
			
				FString IsDebuggerString = CurrentSession.bIsDebugger ? SessionManagerDefs::TrueValueString : SessionManagerDefs::FalseValueString;
				FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::DebuggerStoreKey, IsDebuggerString);

				if (!CurrentSession.bWasEverDebugger && CurrentSession.bIsDebugger)
				{
					CurrentSession.bWasEverDebugger = true;

					FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::WasDebuggerStoreKey, SessionManagerDefs::TrueValueString);

#if PLATFORM_SUPPORTS_WATCHDOG
					if (!WatchdogSectionName.IsEmpty())
					{
						FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, WatchdogSectionName, SessionManagerDefs::WasDebuggerStoreKey, SessionManagerDefs::TrueValueString);
					}
#endif
				}
			}

			CurrentSession.Timestamp = FDateTime::UtcNow();
			FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::TimestampStoreKey, TimestampToString(CurrentSession.Timestamp));

#if PLATFORM_SUPPORTS_WATCHDOG
			if (!WatchdogSectionName.IsEmpty())
			{
				FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, WatchdogSectionName, SessionManagerDefs::TimestampStoreKey, TimestampToString(CurrentSession.Timestamp));
			}
#endif
		}
	}
}

void FEngineSessionManager::Shutdown()
{
	FCoreDelegates::OnHandleSystemError.RemoveAll(this);
	FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.RemoveAll(this);
	FCoreDelegates::IsVanillaProductChanged.RemoveAll(this);
	FSlateApplication::Get().GetOnModalLoopTickEvent().RemoveAll(this);

	// Clear the session record for this session
	if (bInitializedRecords)
	{
		if (!CurrentSession.bCrashed)
		{
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::ModeStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::ProjectNameStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::CrashStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::GPUCrashStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::EngineVersionStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::TimestampStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::DebuggerStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::WasDebuggerStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::DeactivatedStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::BackgroundStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::UserActivityStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::VanillaStoreKey);

#if PLATFORM_SUPPORTS_WATCHDOG
			if (!WatchdogSectionName.IsEmpty())
			{
				FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, WatchdogSectionName, SessionManagerDefs::StatusStoreKey, SessionManagerDefs::ShutdownSessionToken);
				FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, WatchdogSectionName, SessionManagerDefs::TimestampStoreKey, TimestampToString(FDateTime::UtcNow()));
				WatchdogSectionName = FString();
			}
#endif
		}

		bInitializedRecords = false;
		bShutdown = true;
	}
}

bool FEngineSessionManager::BeginReadWriteRecords()
{
	SessionRecords.Empty();

	// Lock and read the list of sessions in storage
	FString ListSectionName = GetStoreSectionString(SessionManagerDefs::SessionRecordListSection);

	// Write list to SessionRecords member
	FString SessionListString;
	FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, ListSectionName, TEXT("SessionList"), SessionListString);

	// Parse SessionListString for session ids
	TArray<FString> SessionIds;
	SessionListString.ParseIntoArray(SessionIds, TEXT(","));

	// Retrieve all the sessions in the list from storage
	for (FString& SessionId : SessionIds)
	{
		FString SectionName = GetStoreSectionString(SessionId);

		FString IsCrashString;
		FString EngineVersionString;
		FString TimestampString;
		FString IsDebuggerString;

		// Read manditory values
		if (FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::CrashStoreKey, IsCrashString) &&
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::EngineVersionStoreKey, EngineVersionString) &&
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::TimestampStoreKey, TimestampString) &&
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::DebuggerStoreKey, IsDebuggerString))
		{
			// Read optional values
			FString WasDebuggerString = IsDebuggerString;
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::WasDebuggerStoreKey, WasDebuggerString);
			FString ModeString = SessionManagerDefs::EditorValueString;
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::ModeStoreKey, ModeString);
			FString ProjectName = SessionManagerDefs::UnknownProjectValueString;
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::ProjectNameStoreKey, ProjectName);
			FString IsDeactivatedString = SessionManagerDefs::FalseValueString;
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::DeactivatedStoreKey, IsDeactivatedString);
			FString IsInBackgroundString = SessionManagerDefs::FalseValueString;
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::BackgroundStoreKey, IsInBackgroundString);
			FString UserActivityString = SessionManagerDefs::DefaultUserActivity;
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::UserActivityStoreKey, UserActivityString);
			FString IsVanillaString = SessionManagerDefs::FalseValueString;
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::VanillaStoreKey, IsVanillaString);
			FString IsGPUCrashString = SessionManagerDefs::FalseValueString;
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::GPUCrashStoreKey, IsGPUCrashString);

			// Create new record from the read values
			FSessionRecord NewRecord;
			NewRecord.SessionId = SessionId;
			NewRecord.Mode = ModeString == SessionManagerDefs::EditorValueString ? EEngineSessionManagerMode::Editor : EEngineSessionManagerMode::Game;
			NewRecord.ProjectName = ProjectName;
			NewRecord.EngineVersion = EngineVersionString;
			NewRecord.Timestamp = StringToTimestamp(TimestampString);
			NewRecord.bCrashed = IsCrashString == SessionManagerDefs::TrueValueString;
			NewRecord.bGPUCrashed = IsGPUCrashString == SessionManagerDefs::TrueValueString;
			NewRecord.bIsDebugger = IsDebuggerString == SessionManagerDefs::TrueValueString;
			NewRecord.bWasEverDebugger = WasDebuggerString == SessionManagerDefs::TrueValueString;
			NewRecord.bIsDeactivated = IsDeactivatedString == SessionManagerDefs::TrueValueString;
			NewRecord.bIsInBackground = IsInBackgroundString == SessionManagerDefs::TrueValueString;
			NewRecord.CurrentUserActivity = UserActivityString;
			NewRecord.bIsVanilla = IsVanillaString == SessionManagerDefs::TrueValueString;

			SessionRecords.Add(NewRecord);
		}
		else
		{
			// Clean up orphaned values, if there are any
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::ModeStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::ProjectNameStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::CrashStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::GPUCrashStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::EngineVersionStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::TimestampStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::DebuggerStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::WasDebuggerStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::DeactivatedStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::BackgroundStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::UserActivityStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::VanillaStoreKey);
		}
	}

	return true;
}

void FEngineSessionManager::EndReadWriteRecords()
{
	// Update the list of sessions in storage to match SessionRecords
	FString SessionListString;
	if (SessionRecords.Num() > 0)
	{
		for (FSessionRecord& Session : SessionRecords)
		{
			SessionListString.Append(Session.SessionId);
			SessionListString.Append(TEXT(","));
		}
		SessionListString.RemoveAt(SessionListString.Len() - 1);
	}

	FString ListSectionName = GetStoreSectionString(SessionManagerDefs::SessionRecordListSection);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, ListSectionName, TEXT("SessionList"), SessionListString);

	// Clear SessionRecords member
	SessionRecords.Empty();
}

void FEngineSessionManager::DeleteStoredRecord(const FSessionRecord& Record)
{
	// Delete the session record in storage
	FString SessionId = Record.SessionId;
	FString SectionName = GetStoreSectionString(SessionId);

	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::ModeStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::ProjectNameStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::CrashStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::GPUCrashStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::EngineVersionStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::TimestampStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::DebuggerStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::WasDebuggerStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::DeactivatedStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::BackgroundStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::UserActivityStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::VanillaStoreKey);

	// Remove the session record from SessionRecords list
	SessionRecords.RemoveAll([&SessionId](const FSessionRecord& X){ return X.SessionId == SessionId; });
}

/**
 * @EventName Engine.AbnormalShutdown
 *
 * @Trigger Fired only by the engine during startup, once for each "abnormal shutdown" detected that has not already been sent.
 *
 * @Type Static
 * @Owner Chris.Wood
 *
 * @EventParam RunType - Editor or Game
 * @EventParam ProjectName - Project for the session that abnormally terminated. 
 * @EventParam Platform - Windows, Mac, Linux, PS4, XBoxOne or Unknown
 * @EventParam SessionId - Analytics SessionID of the session that abnormally terminated.
 * @EventParam EngineVersion - EngineVersion of the session that abnormally terminated.
 * @EventParam ShutdownType - one of Crashed, Debugger, or AbormalShutdown
 *               * Crashed - we definitely detected a crash (whether or not a debugger was attached)
 *               * Debugger - the session crashed or shutdown abnormally, but we had a debugger attached at startup, so abnormal termination is much more likely because the user was debugging.
 *               * AbnormalShutdown - this happens when we didn't detect a normal shutdown, but none of the above cases is the cause. A session record simply timed-out without being closed.
 * @EventParam Timestamp - the UTC time of the last known time the abnormally terminated session was running, within 5 minutes.
 * @EventParam CurrentUserActivity - If one was set when the session abnormally terminated, this is the activity taken from the FUserActivityTracking API.
 * @EventParam IsVanilla - Value from the engine's IsVanillaProduct() method. Basically if this is a Epic-distributed Editor with zero third party plugins or game code modules.
 * @EventParam GPUCrash - A GPU Hang or Crash was detected before the final assert, fatal log, or other exit.
 
 *
 * @TODO: Debugger should be a completely separate flag, since it's orthogonal to whether we detect a crash or shutdown.
 *
 * @Comments The engine will only try to check for abnormal terminations if it determines it is a "real" editor or game run (not a commandlet or PIE, or editor -game run), and the user has not disabled sending usage data to Epic via the settings.
 * 
 * The SessionId parameter should be used to find the actual session associated with this crash.
 * 
 * If multiple versions of the editor or launched, this code will properly track each one and its shutdown status. So during startup, an editor instance may need to fire off several events.
 *
 * When attributing abnormal terminations to engine versions, be sure to use the EngineVersion associated with this event, and not the AppVersion. AppVersion is for the session that is currently sending the event, not for the session that crashed. That is why EngineVersion is sent separately.
 *
 * The editor updates Timestamp every 5 minutes, so we should know the time of the crash within 5 minutes. It should technically correlate with the last heartbeat we receive in the data for that session.
 *
 * The main difference between an AbnormalShutdown and a Crash is that we KNOW a crash occurred, so we can send the event right away. If the engine did not shut down correctly, we don't KNOW that, so simply wait up to 30m (the engine updates the timestamp every 5 mins) to be sure that it's probably not running anymore.
 *
 * We have seen data in the wild that indicated editor freezing for up to 8 days but we're assuming that was likely stopped in a debugger. That's also why we added the ShutdownType of Debugger to the event. However, this code does not check IMMEDIATELY on crash if the debugger is present (that might be dangerous in a crash handler perhaps), we only check if a debugger is attached at startup. Then if an A.S. is detected, we just say "Debugger" because it's likely they just stopped the debugger and killed the process.
 */
void FEngineSessionManager::SendAbnormalShutdownReport(const FSessionRecord& Record)
{
	FString PlatformName(FPlatformProperties::PlatformName());

#if PLATFORM_WINDOWS | PLATFORM_MAC | PLATFORM_LINUX
	// do nothing
#elif PLATFORM_PS4
	if (Record.bIsDeactivated && !Record.bCrashed)
	{
		// Shutting down in deactivated state on PS4 is normal - don't report it
		return;
	}
#elif PLATFORM_XBOXONE
	if (Record.bIsInBackground && !Record.bCrashed)
	{
		// Shutting down in background state on XB1 is normal - don't report it
		return;
	}
#else
	return; // TODO: CWood: disabled on other platforms
#endif

	FGuid SessionId;
	FString SessionIdString = Record.SessionId;
	if (FGuid::Parse(SessionIdString, SessionId))
	{
		// convert session guid to one with braces for sending to analytics
		SessionIdString = SessionId.ToString(EGuidFormats::DigitsWithHyphensInBraces);
	}

#if !PLATFORM_PS4
	FString ShutdownTypeString = Record.bCrashed ? SessionManagerDefs::CrashSessionToken :
		(Record.bWasEverDebugger ? SessionManagerDefs::DebuggerSessionToken : SessionManagerDefs::AbnormalSessionToken);
#else
	// PS4 cannot set the crash flag so report abnormal shutdowns with a specific token meaning "crash or abnormal shutdown".
	FString ShutdownTypeString = Record.bWasEverDebugger ? SessionManagerDefs::DebuggerSessionToken : SessionManagerDefs::PS4SessionToken;
#endif

	FString RunTypeString = Record.Mode == EEngineSessionManagerMode::Editor ? SessionManagerDefs::EditorValueString : SessionManagerDefs::GameValueString;
	FString IsVanillaString = Record.bIsVanilla ? SessionManagerDefs::TrueValueString : SessionManagerDefs::FalseValueString;
	FString WasDebuggedString = Record.bWasEverDebugger ? SessionManagerDefs::TrueValueString : SessionManagerDefs::FalseValueString;
	FString GPUCrashedString = Record.bGPUCrashed ? SessionManagerDefs::TrueValueString : SessionManagerDefs::FalseValueString;

	TArray< FAnalyticsEventAttribute > AbnormalShutdownAttributes;
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("RunType"), RunTypeString));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("ProjectName"), Record.ProjectName));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("Platform"), PlatformName));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("SessionId"), SessionIdString));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("EngineVersion"), Record.EngineVersion));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("ShutdownType"), ShutdownTypeString));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("Timestamp"), Record.Timestamp.ToIso8601()));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("CurrentUserActivity"), Record.CurrentUserActivity));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("IsVanilla"), IsVanillaString));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("WasDebugged"), WasDebuggedString));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("GPUCrash"), GPUCrashedString));

	FEngineAnalytics::GetProvider().RecordEvent(TEXT("Engine.AbnormalShutdown"), AbnormalShutdownAttributes);

	UE_LOG(LogEngineSessionManager, Log, TEXT("EngineSessionManager sent abnormal shutdown report. Type=%s, SessionId=%s"), *ShutdownTypeString, *SessionIdString);
}

void FEngineSessionManager::CreateAndWriteRecordForSession()
{
	FGuid SessionId;
	if (FGuid::Parse(FEngineAnalytics::GetProvider().GetSessionID(), SessionId))
	{
		// convert session guid to one without braces or other chars that might not be suitable for storage
		CurrentSession.SessionId = SessionId.ToString(EGuidFormats::DigitsWithHyphens);
	}
	else
	{
		CurrentSession.SessionId = FEngineAnalytics::GetProvider().GetSessionID();
	}

	const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();

	CurrentSession.Mode = Mode;
	CurrentSession.ProjectName = ProjectSettings.ProjectName;
	CurrentSession.EngineVersion = FEngineVersion::Current().ToString(EVersionComponent::Changelist);
	CurrentSession.Timestamp = FDateTime::UtcNow();
	CurrentSession.bIsDebugger = CurrentSession.bWasEverDebugger = FPlatformMisc::IsDebuggerPresent();
	CurrentSession.CurrentUserActivity = GetUserActivityString();
	CurrentSession.bIsVanilla = GEngine && GEngine->IsVanillaProduct();
	CurrentSessionSectionName = GetStoreSectionString(CurrentSession.SessionId);

	FString ModeString = CurrentSession.Mode == EEngineSessionManagerMode::Editor ? SessionManagerDefs::EditorValueString : SessionManagerDefs::GameValueString;
	FString IsDebuggerString = CurrentSession.bIsDebugger ? SessionManagerDefs::TrueValueString : SessionManagerDefs::FalseValueString;
	FString WasDebuggerString = CurrentSession.bWasEverDebugger ? SessionManagerDefs::TrueValueString : SessionManagerDefs::FalseValueString;
	FString IsDeactivatedString = CurrentSession.bIsDeactivated ? SessionManagerDefs::TrueValueString : SessionManagerDefs::FalseValueString;
	FString IsInBackgroundString = CurrentSession.bIsInBackground ? SessionManagerDefs::TrueValueString : SessionManagerDefs::FalseValueString;
	FString IsVanillaString = CurrentSession.bIsVanilla ? SessionManagerDefs::TrueValueString : SessionManagerDefs::FalseValueString;

	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::ModeStoreKey, ModeString);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::ProjectNameStoreKey, CurrentSession.ProjectName);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::CrashStoreKey, SessionManagerDefs::FalseValueString);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::EngineVersionStoreKey, CurrentSession.EngineVersion);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::TimestampStoreKey, TimestampToString(CurrentSession.Timestamp));
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::DebuggerStoreKey, IsDebuggerString);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::WasDebuggerStoreKey, WasDebuggerString);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::DeactivatedStoreKey, IsDeactivatedString);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::BackgroundStoreKey, IsInBackgroundString);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::UserActivityStoreKey, CurrentSession.CurrentUserActivity);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::VanillaStoreKey, IsVanillaString);

	SessionRecords.Add(CurrentSession);

#if PLATFORM_SUPPORTS_WATCHDOG
	bool bUseWatchdog = false;
	GConfig->GetBool(TEXT("EngineSessionManager"), TEXT("UseWatchdogMTBF"), bUseWatchdog, GEngineIni);
	if ((!CurrentSession.bWasEverDebugger && bUseWatchdog && !FParse::Param(FCommandLine::Get(), TEXT("NoWatchdog"))) || FParse::Param(FCommandLine::Get(), TEXT("ForceWatchdog")))
	{
		StartWatchdog(ModeString, CurrentSession.ProjectName, FPlatformProperties::PlatformName(), CurrentSession.SessionId, CurrentSession.EngineVersion);
	}
#endif
}

extern CORE_API bool GIsGPUCrashed;
void FEngineSessionManager::OnCrashing()
{
	if (!CurrentSession.bCrashed && bInitializedRecords)
	{
		CurrentSession.bCrashed = true;
		CurrentSession.bGPUCrashed = GIsGPUCrashed;
		FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::CrashStoreKey, SessionManagerDefs::TrueValueString);
		FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::GPUCrashStoreKey, CurrentSession.bGPUCrashed ? SessionManagerDefs::TrueValueString : SessionManagerDefs::FalseValueString);

#if PLATFORM_SUPPORTS_WATCHDOG
		if (!WatchdogSectionName.IsEmpty())
		{
			FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, WatchdogSectionName, SessionManagerDefs::StatusStoreKey, SessionManagerDefs::CrashSessionToken);
			FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, WatchdogSectionName, SessionManagerDefs::TimestampStoreKey, TimestampToString(FDateTime::UtcNow()));
		}
#endif
	}
}

void FEngineSessionManager::OnAppReactivate()
{
	if (CurrentSession.bIsDeactivated)
	{
		CurrentSession.bIsDeactivated = false;
		FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::DeactivatedStoreKey, SessionManagerDefs::FalseValueString);
	}
}

void FEngineSessionManager::OnAppDeactivate()
{
	if (!CurrentSession.bIsDeactivated)
	{
		CurrentSession.bIsDeactivated = true;
		FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::DeactivatedStoreKey, SessionManagerDefs::TrueValueString);
	}
}

void FEngineSessionManager::OnAppBackground()
{
	if (!CurrentSession.bIsInBackground)
	{
		CurrentSession.bIsInBackground = true;
		FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::BackgroundStoreKey, SessionManagerDefs::TrueValueString);
	}
}

void FEngineSessionManager::OnAppForeground()
{
	if (CurrentSession.bIsInBackground)
	{
		CurrentSession.bIsInBackground = false;
		FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::BackgroundStoreKey, SessionManagerDefs::FalseValueString);
	}
}

FString FEngineSessionManager::GetStoreSectionString(FString InSuffix)
{
	check(Mode == EEngineSessionManagerMode::Editor || Mode == EEngineSessionManagerMode::Game)

	if (Mode == EEngineSessionManagerMode::Editor)
	{
		return FString::Printf(TEXT("%s%s/%s"), *SessionManagerDefs::EditorSessionRecordSectionPrefix, *SessionManagerDefs::SessionsVersionString, *InSuffix);
	}
	else
	{
		const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();
		return FString::Printf(TEXT("%s%s/%s/%s"), *SessionManagerDefs::GameSessionRecordSectionPrefix, *SessionManagerDefs::SessionsVersionString, *ProjectSettings.ProjectName, *InSuffix);
	}
}

void FEngineSessionManager::OnVanillaStateChanged(bool bIsVanilla)
{
	if (CurrentSession.bIsVanilla != bIsVanilla && bInitializedRecords)
	{
		CurrentSession.bIsVanilla = bIsVanilla;
		FString IsVanillaString = CurrentSession.bIsVanilla ? SessionManagerDefs::TrueValueString : SessionManagerDefs::FalseValueString;
		FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::VanillaStoreKey, IsVanillaString);
	}
}

void FEngineSessionManager::OnUserActivity(const FUserActivity& UserActivity)
{
	if (!CurrentSession.bCrashed && bInitializedRecords)
	{
		CurrentSession.CurrentUserActivity = GetUserActivityString();
		FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::UserActivityStoreKey, CurrentSession.CurrentUserActivity);

#if PLATFORM_SUPPORTS_WATCHDOG
		if (!WatchdogSectionName.IsEmpty())
		{
			FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, WatchdogSectionName, SessionManagerDefs::UserActivityStoreKey, CurrentSession.CurrentUserActivity);
			FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, WatchdogSectionName, SessionManagerDefs::TimestampStoreKey, TimestampToString(FDateTime::UtcNow()));
		}
#endif
	}
}

FString FEngineSessionManager::GetUserActivityString() const
{
	const FUserActivity& UserActivity = FUserActivityTracking::GetUserActivity();
	
	if (UserActivity.ActionName.IsEmpty())
	{
		return SessionManagerDefs::DefaultUserActivity;
	}

	return UserActivity.ActionName;
}

#if PLATFORM_SUPPORTS_WATCHDOG

/**
 * @EventName Engine.StartWatchdog
 *
 * @Trigger Event raised by EngineSessionManager as part of MTBF tracking. Records an attempt to start the UnrealWatchdog process.
 *
 * @Type Static
 * @Owner Chris.Wood
 *
 * @EventParam RunType - Editor or Game
 * @EventParam ProjectName - Project for the session.
 * @EventParam Platform - Windows, Mac, Linux
 * @EventParam SessionId - Analytics SessionID of the session.
 * @EventParam EngineVersion - EngineVersion of the session.
 * @EventParam IsInternalBuild - internal Epic build environment or not? Calls FEngineBuildSettings::IsInternalBuild(). Value is Yes or No.
 * @EventParam Outcome - Whether the watchdog was started successfully. One of Succeeded, CreateProcFailed or MissingBinaryFailed.
 *
 * @Comments Currently only runs Watchdog when MTBF is enabled, we aren't debugging, we're a DESKTOP platform and watchdog is specifically enabled via config or command line arg.
 */
void FEngineSessionManager::StartWatchdog(const FString& RunType, const FString& ProjectName, const FString& PlatformName, const FString& SessionId, const FString& EngineVersion)
{
	uint32 ProcessId =  FPlatformProcess::GetCurrentProcessId();
	const int SuccessfulRtnCode = 0;	// hardcode this for now, zero might not always be correct

	FString WatchdogClientArguments =
		FString::Printf(TEXT(
			"-PID=%u -RunType=%s -ProjectName=\"%s\" -Platform=%s -SessionId=%s -EngineVersion=%s -SuccessfulRtnCode=%d"),
			ProcessId, *RunType, *ProjectName, *PlatformName, *SessionId, *EngineVersion, SuccessfulRtnCode);

	bool bAllowWatchdogDetectHangs = false;
	GConfig->GetBool(TEXT("EngineSessionManager"), TEXT("AllowWatchdogDetectHangs"), bAllowWatchdogDetectHangs, GEngineIni);

	if (bAllowWatchdogDetectHangs)
	{
		int HangSeconds = 120;
		GConfig->GetInt(TEXT("EngineSessionManager"), TEXT("WatchdogMinimumHangSeconds"), HangSeconds, GEngineIni);

		WatchdogClientArguments.Append(FString::Printf(TEXT(" -DetectHangs -HangSeconds=%d"), HangSeconds));
	}

	if (FEngineBuildSettings::IsInternalBuild())
	{
		// Suppress the watchdog dialogs if this engine session should never show interactive UI
		if (!FApp::IsUnattended() && !IsRunningDedicatedServer() && FApp::CanEverRender())
		{
			// Only show watchdog dialogs if it's set in config
			bool bAllowWatchdogDialogs = false;
			GConfig->GetBool(TEXT("EngineSessionManager"), TEXT("AllowWatchdogDialogs"), bAllowWatchdogDialogs, GEngineIni);

			if (bAllowWatchdogDialogs)
			{
				WatchdogClientArguments.Append(TEXT(" -AllowDialogs"));
			}
		}
	}

	FString WatchdogPath = FPaths::ConvertRelativePathToFull(FPlatformProcess::GenerateApplicationPath(TEXT("UnrealWatchdog"), EBuildConfigurations::Development));

	TArray< FAnalyticsEventAttribute > WatchdogStartedAttributes;
	WatchdogStartedAttributes.Add(FAnalyticsEventAttribute(TEXT("RunType"), RunType));
	WatchdogStartedAttributes.Add(FAnalyticsEventAttribute(TEXT("ProjectName"), ProjectName));
	WatchdogStartedAttributes.Add(FAnalyticsEventAttribute(TEXT("Platform"), PlatformName));
	WatchdogStartedAttributes.Add(FAnalyticsEventAttribute(TEXT("SessionId"), SessionId));
	WatchdogStartedAttributes.Add(FAnalyticsEventAttribute(TEXT("IsInternalBuild"), FEngineBuildSettings::IsInternalBuild() ? TEXT("Yes") : TEXT("No")));

	if (FPaths::FileExists(WatchdogPath))
	{
		FProcHandle WatchdogProcessHandle = FPlatformProcess::CreateProc(*WatchdogPath, *WatchdogClientArguments, true, true, false, NULL, 0, NULL, NULL);

		if (WatchdogProcessHandle.IsValid())
		{
			FString WatchdogStartTimeString = TimestampToString(FDateTime::UtcNow());
			FString WasDebuggerString = CurrentSession.bWasEverDebugger ? SessionManagerDefs::TrueValueString : SessionManagerDefs::FalseValueString;

			WatchdogStartedAttributes.Add(FAnalyticsEventAttribute(TEXT("Outcome"), TEXT("Succeeded")));
			UE_LOG(LogEngineSessionManager, Log, TEXT("Started UnrealWatchdog for process id %u"), ProcessId);

			WatchdogSectionName = GetWatchdogStoreSectionString(ProcessId);

			FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, WatchdogSectionName, SessionManagerDefs::CommandLineStoreKey, FCommandLine::GetOriginalForLogging());
			FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, WatchdogSectionName, SessionManagerDefs::StartupTimeStoreKey, WatchdogStartTimeString);
			FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, WatchdogSectionName, SessionManagerDefs::TimestampStoreKey, WatchdogStartTimeString);
			FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, WatchdogSectionName, SessionManagerDefs::StatusStoreKey, SessionManagerDefs::RunningSessionToken);
			FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, WatchdogSectionName, SessionManagerDefs::UserActivityStoreKey, CurrentSession.CurrentUserActivity);
			FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, WatchdogSectionName, SessionManagerDefs::WasDebuggerStoreKey, WasDebuggerString);
		}
		else
		{
			WatchdogStartedAttributes.Add(FAnalyticsEventAttribute(TEXT("Outcome"), TEXT("CreateProcFailed")));
			UE_LOG(LogEngineSessionManager, Warning, TEXT("Unable to start UnrealWatchdog.exe. CreateProc failed."));
		}
	}
	else
	{
		WatchdogStartedAttributes.Add(FAnalyticsEventAttribute(TEXT("Outcome"), TEXT("MissingBinaryFailed")));
		UE_LOG(LogEngineSessionManager, Warning, TEXT("Unable to start UnrealWatchdog.exe. File not found."));
	}

	FEngineAnalytics::GetProvider().RecordEvent(TEXT("Engine.StartWatchdog"), WatchdogStartedAttributes);
}

FString FEngineSessionManager::GetWatchdogStoreSectionString(uint32 InPID)
{
	return FString::Printf(TEXT("%s%s/%u"), *SessionManagerDefs::WatchdogRecordSectionPrefix, *SessionManagerDefs::WatchdogVersionString, InPID);
}

#endif // PLATFORM_SUPPORTS_WATCHDOG

#undef LOCTEXT_NAMESPACE
