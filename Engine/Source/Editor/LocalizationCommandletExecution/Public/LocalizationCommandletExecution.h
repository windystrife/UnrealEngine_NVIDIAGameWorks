// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"

class SWindow;

namespace LocalizationCommandletExecution
{
	struct LOCALIZATIONCOMMANDLETEXECUTION_API FTask
	{
		FTask() {}

		FTask(const FText& InName, const FString& InScriptPath, const bool InShouldUseProjectFile)
			: Name(InName)
			, ScriptPath(InScriptPath)
			, ShouldUseProjectFile(InShouldUseProjectFile)
		{}

		FText Name;
		FString ScriptPath;
		bool ShouldUseProjectFile;
	};

	bool Execute(const TSharedRef<SWindow>& ParentWindow, const FText& Title, const TArray<FTask>& Tasks);
};

class LOCALIZATIONCOMMANDLETEXECUTION_API FLocalizationCommandletProcess : public TSharedFromThis<FLocalizationCommandletProcess>
{
public:
	static TSharedPtr<FLocalizationCommandletProcess> Execute(const FString& ConfigFilePath, const bool UseProjectFile = true);

private:
	FLocalizationCommandletProcess(void* const InReadPipe, void* const InWritePipe, const FProcHandle InProcessHandle, const FString& InProcessArguments)
		: ReadPipe(InReadPipe)
		, WritePipe(InWritePipe)
		, ProcessHandle(InProcessHandle)
		, ProcessArguments(InProcessArguments)
	{
	}

public:
	~FLocalizationCommandletProcess();

	void* GetReadPipe() const
	{
		return ReadPipe;
	}

	FProcHandle& GetHandle()
	{
		return ProcessHandle;
	}

	const FString& GetProcessArguments() const
	{
		return ProcessArguments;
	}

private:
	void* const ReadPipe;
	void* const WritePipe;
	FProcHandle ProcessHandle;
	FString ProcessArguments;
};
