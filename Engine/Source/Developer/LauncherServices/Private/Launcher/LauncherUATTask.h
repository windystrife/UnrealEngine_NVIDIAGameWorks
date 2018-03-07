// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ILauncherWorker.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Launcher/LauncherTask.h"
#include "Misc/App.h"

/**
 * class for UAT launcher tasks.
 */
class FLauncherUATTask
	: public FLauncherTask
{
public:

	FLauncherUATTask( const FString& InCommandLine, const FString& InName, const FString& InDesc, void* InReadPipe, void* InWritePipe, const FString& InEditorExe, FProcHandle& InProcessHandle, ILauncherWorker* InWorker, const FString& InCommandEnd)
		: FLauncherTask(InName, InDesc, InReadPipe, InWritePipe)
		, CommandLine(InCommandLine)
		, EditorExe(InEditorExe)
		, ProcessHandle(InProcessHandle)
		, CommandText(InCommandEnd)
	{
		EndTextFound = false;
		InWorker->OnOutputReceived().AddRaw(this, &FLauncherUATTask::HandleOutputReceived);
	}

protected:

	/**
	 * Performs the actual task.
	 *
	 * @param ChainState - Holds the state of the task chain.
	 *
	 * @return true if the task completed successfully, false otherwise.
	 */
	virtual bool PerformTask( FLauncherTaskChainState& ChainState ) override
	{
		// spawn a UAT process to cook the data
		// UAT executable
		FString ExecutablePath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() + FString(TEXT("Build")) / TEXT("BatchFiles"));
#if PLATFORM_MAC
		FString Executable = TEXT("RunUAT.command");
#elif PLATFORM_LINUX
		FString Executable = TEXT("RunUAT.sh");
#else
		FString Executable = TEXT("RunUAT.bat");
#endif

		// base UAT command arguments
		static const FString ConfigStrings[] = { TEXT("Unknown"), TEXT("Debug"), TEXT("DebugGame"), TEXT("Development"), TEXT("Shipping"), TEXT("Test") };
		FString UATCommandLine;
		FString ProjectPath = *ChainState.Profile->GetProjectPath();
		ProjectPath = FPaths::ConvertRelativePathToFull(ProjectPath);
		UATCommandLine = FString::Printf(TEXT("-ScriptsForProject=\"%s\" BuildCookRun -project=\"%s\" -noP4 -clientconfig=%s -serverconfig=%s"),
			*ProjectPath,
			*ProjectPath,
			*ConfigStrings[ChainState.Profile->GetBuildConfiguration()],
			*ConfigStrings[ChainState.Profile->GetBuildConfiguration()]);

		// we expect to pass -nocompile to UAT here as we generally expect UAT to be fully compiled. Besides, installed builds don't even have the source to compile UAT scripts.
		// Only allow UAT to compile scripts dynamically if we pass -development or we have the IsBuildingUAT property set, the latter of which should not allow itself to be set in installed situations.
		bool bAllowCompile = true;
		UATCommandLine += (bAllowCompile && (FParse::Param( FCommandLine::Get(), TEXT("development") ) || ChainState.Profile->IsBuildingUAT())) ? TEXT("") : TEXT(" -nocompile");
		// we never want to build the editor when launching from the editor or running with an installed engine (which can't rebuild itself)
		UATCommandLine += GIsEditor || FApp::IsEngineInstalled() ? TEXT(" -nocompileeditor") : TEXT("");
		UATCommandLine += FApp::IsEngineInstalled() ? TEXT(" -installed") : TEXT("");

		// specify the path to the editor exe if necessary
		if(EditorExe.Len() > 0)
		{
			UATCommandLine += FString::Printf(TEXT(" -ue4exe=\"%s\""), *EditorExe);

			if (FApp::IsRunningDebug())
			{
				UATCommandLine += TEXT(" -UseDebugParamForEditorExe");
			}
		}

		// specialized command arguments for this particular task
		UATCommandLine += CommandLine;

		// launch UAT and monitor its progress
		ProcessHandle = FPlatformProcess::CreateProc(*(ExecutablePath / Executable), *UATCommandLine, false, true, true, NULL, 0, *ExecutablePath, WritePipe);

		while (FPlatformProcess::IsProcRunning(ProcessHandle) && !EndTextFound)
		{

			if (IsCancelling())
			{
				FPlatformProcess::TerminateProc(ProcessHandle, true);
				return false;
			}

			FPlatformProcess::Sleep(0.25);
		}

		if (!EndTextFound && !FPlatformProcess::GetProcReturnCode(ProcessHandle, &Result))
		{
			return false;
		}

		return (Result == 0);
	}

	void HandleOutputReceived(const FString& InMessage)
	{
		if (InMessage.Contains(TEXT("Error:"), ESearchCase::IgnoreCase))
		{
			++ErrorCounter;
		}
		else if (InMessage.Contains(TEXT("Warning:"), ESearchCase::IgnoreCase))
		{
			++WarningCounter;
		}

		EndTextFound |= InMessage.Contains(CommandText);
	}

private:

	// Command line
	FString CommandLine;

	// The editor executable that UAT should use
	FString EditorExe;

	// process handle
	FProcHandle& ProcessHandle;

	// The editor executable that UAT should use
	FString CommandText;

	bool EndTextFound;
};

