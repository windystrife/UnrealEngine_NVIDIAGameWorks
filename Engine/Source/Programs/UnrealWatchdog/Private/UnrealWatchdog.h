// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"

DECLARE_LOG_CATEGORY_EXTERN(UnrealWatchdogLog, Log, All);

struct FWatchdogCommandLine
{
	FString RunType;
	FString ProjectName;
	FString PlatformName;
	FString SessionId;
	FString EngineVersion;
	uint32 ParentProcessId;
	int32 SuccessReturnCode;
	bool bAllowDetectHangs;
	int32 HangThresholdSeconds;
	bool bAllowDialogs;
	bool bHasProcessId;

	FWatchdogCommandLine(const TCHAR* InCommandLine)
	{
		// Read command line args
		FParse::Value(InCommandLine, TEXT("RunType="), RunType);
		FParse::Value(InCommandLine, TEXT("ProjectName="), ProjectName);
		FParse::Value(InCommandLine, TEXT("Platform="), PlatformName);
		FParse::Value(InCommandLine, TEXT("SessionId="), SessionId);
		FParse::Value(InCommandLine, TEXT("EngineVersion="), EngineVersion);
		bHasProcessId = FParse::Value(InCommandLine, TEXT("PID="), ParentProcessId);
		SuccessReturnCode = 0;
		FParse::Value(InCommandLine, TEXT("SuccessfulRtnCode="), SuccessReturnCode);
		bAllowDetectHangs = FParse::Param(InCommandLine, TEXT("DetectHangs"));
		HangThresholdSeconds = 120;
		FParse::Value(InCommandLine, TEXT("HangSeconds="), HangThresholdSeconds);
		bAllowDialogs = FParse::Param(InCommandLine, TEXT("AllowDialogs"));
	}
};

bool GetWatchdogStoredDebuggerValue(const FString& WatchdogSectionName);
int RunUnrealWatchdog(const TCHAR* CommandLine);
FProcHandle GetProcessHandle(const FWatchdogCommandLine& CommandLine);
