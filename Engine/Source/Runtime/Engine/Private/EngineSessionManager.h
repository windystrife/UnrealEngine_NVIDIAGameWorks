// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// TODO: implement Watchdog on Mac and Linux and replace Windows check with desktop check below
//#define PLATFORM_SUPPORTS_WATCHDOG		PLATFORM_DESKTOP
#define PLATFORM_SUPPORTS_WATCHDOG		PLATFORM_WINDOWS

struct FUserActivity;

enum class EEngineSessionManagerMode
{
	Editor,
	Game
};

/* Handles writing session records to platform's storage to track crashed and timed-out editor sessions */
class FEngineSessionManager
{
public:
	FEngineSessionManager(EEngineSessionManagerMode InMode)
		: Mode(InMode)
		, bInitializedRecords(false)
		, bShutdown(false)
		, HeartbeatTimeElapsed(0.0f)
	{
		CurrentSession.Mode = EEngineSessionManagerMode::Editor;
		CurrentSession.Timestamp = FDateTime::MinValue();		
		CurrentSession.bCrashed = false;
		CurrentSession.bGPUCrashed = false;
		CurrentSession.bIsDebugger = false;
		CurrentSession.bWasEverDebugger = false;
		CurrentSession.bIsDeactivated = false;
		CurrentSession.bIsInBackground = false;
		CurrentSession.bIsVanilla = false;
	}

	void Initialize();

	void Tick(float DeltaTime);

	void Shutdown();

private:
	struct FSessionRecord
	{
		FString SessionId;
		EEngineSessionManagerMode Mode;
		FString ProjectName;
		FString EngineVersion;
		FDateTime Timestamp;
		bool bCrashed;
		bool bGPUCrashed;
		bool bIsDebugger;
		bool bWasEverDebugger;
		bool bIsDeactivated;
		bool bIsInBackground;
		FString CurrentUserActivity;
		bool bIsVanilla;
	};

private:
	void InitializeRecords(bool bFirstAttempt);
	bool BeginReadWriteRecords();
	void EndReadWriteRecords();
	void DeleteStoredRecord(const FSessionRecord& Record);
	void SendAbnormalShutdownReport(const FSessionRecord& Record);
	void CreateAndWriteRecordForSession();
	void OnCrashing();
	void OnAppReactivate();
	void OnAppDeactivate();
	void OnAppBackground();
	void OnAppForeground();
	FString GetStoreSectionString(FString InSuffix);
	void OnUserActivity(const FUserActivity& UserActivity);
	void OnVanillaStateChanged(bool bIsVanilla);
	FString GetUserActivityString() const;

#if PLATFORM_SUPPORTS_WATCHDOG
	void StartWatchdog(const FString& RunType, const FString& ProjectName, const FString& PlatformName, const FString& SessionId, const FString& EngineVersion);
	FString GetWatchdogStoreSectionString(uint32 InPID);
#endif

private:
	EEngineSessionManagerMode Mode;
	bool bInitializedRecords;
	bool bShutdown;
	float HeartbeatTimeElapsed;
	FSessionRecord CurrentSession;
	FString CurrentSessionSectionName;
	TArray<FSessionRecord> SessionRecords;

#if PLATFORM_SUPPORTS_WATCHDOG
	FString WatchdogSectionName;
#endif
};
