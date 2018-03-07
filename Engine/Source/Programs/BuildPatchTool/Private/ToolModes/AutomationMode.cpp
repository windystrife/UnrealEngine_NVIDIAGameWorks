// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AutomationMode.h"

#if !UE_BUILD_SHIPPING

#include "Interfaces/IBuildPatchServicesModule.h"
#include "BuildPatchTool.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"

#include "IAutomationWorkerModule.h"
#include "IAutomationControllerModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"
#include "Containers/Ticker.h"

using namespace BuildPatchTool;

class FAutomationToolMode : public IToolMode
{
public:
	FAutomationToolMode(IBuildPatchServicesModule& InBpsInterface)
		: BpsInterface(InBpsInterface)
	{
	}

	virtual ~FAutomationToolMode()
	{
	}

	virtual EReturnCode Execute() override
	{
		// Parse commandline
		if (ProcessCommandline() == false)
		{
			return EReturnCode::ArgumentProcessingError;
		}

		// Print help if requested
		if (bHelp)
		{
			UE_LOG(LogBuildPatchTool, Log, TEXT("AUTOMATION TEST MODE"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("This tool mode runs automation tests."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("No arguments are required."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("Optional arguments:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -TestList=\"\"   Specifies in quotes, the list of tests to run. The list is + delimited."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("NB: If -TestList is not specified, then all BuildPatchServices tests are ran."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			return EReturnCode::OK;
		}

		// Main loop.
		double DeltaTime = 0.0;
		double LastTime = FPlatformTime::Seconds();

		// Setup desired frame times.
		float MainsFramerate = 500.0f;
		const float MainsFrameTime = 1.0f / MainsFramerate;

		// Required modules.
		static const FName AutomationWorkerModuleName = TEXT("AutomationWorker");
		static const FName AutomationController("AutomationController");
		IAutomationWorkerModule& AutomationWorkerModule = FModuleManager::LoadModuleChecked<IAutomationWorkerModule>(AutomationWorkerModuleName);
		IAutomationControllerModule& AutomationControllerModule = FModuleManager::LoadModuleChecked<IAutomationControllerModule>(AutomationController);
		AutomationControllerModule.Init();
		IAutomationControllerManagerRef AutomationControllerManager = AutomationControllerModule.GetAutomationController();
		AutomationControllerManager->OnTestsComplete().AddLambda([]()
		{
			GIsRequestingExit = true;
		});
		StaticExec(NULL, *TestList);

		while (!GIsRequestingExit)
		{
			// Increment global frame counter once for each app tick.
			GFrameCounter++;

			// Update sub-systems.
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			FTicker::GetCoreTicker().Tick(DeltaTime);
			AutomationWorkerModule.Tick();
			AutomationControllerModule.Tick();

			// Flush threaded logs.
			GLog->FlushThreadedLogs();

			// Throttle frame rate.
			FPlatformProcess::Sleep(FMath::Max<float>(0.0f, MainsFrameTime - (FPlatformTime::Seconds() - LastTime)));

			// Calculate deltas.
			const double AppTime = FPlatformTime::Seconds();
			DeltaTime = AppTime - LastTime;
			LastTime = AppTime;
		}

		// Check for failures and exit.
		bool bSuccess = !GIsCriticalError && RecursiveCheckReports(AutomationControllerManager->GetReports());
		return bSuccess ? EReturnCode::OK : EReturnCode::ToolFailure;
	}

private:
	bool RecursiveCheckReports(const TArray<TSharedPtr<IAutomationReport>>& Reports, FString ParentTestName = TEXT(""))
	{
		bool bSuccess = true;
		for (const TSharedPtr<IAutomationReport>& Report : Reports)
		{
			if (Report.IsValid())
			{
				FString ReportName = (ParentTestName + Report->GetDisplayName());
				if (Report->HasErrors())
				{
					UE_LOG(LogBuildPatchTool, Error, TEXT("%s: Failed"), *ReportName);
					bSuccess = false;
				}
				bSuccess = RecursiveCheckReports(Report->GetChildReports(), ReportName + TEXT(" ")) & bSuccess;
			}
		}
		return bSuccess;
	}

	bool ProcessCommandline()
	{
#define PARSE_SWITCH(Switch) ParseSwitch(TEXT(#Switch L"="), Switch, Switches)
		TArray<FString> Tokens, Switches;
		FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);

		bHelp = ParseOption(TEXT("help"), Switches);
		if (bHelp)
		{
			return true;
		}
		if (!PARSE_SWITCH(TestList) || TestList.Contains(TEXT(";")))
		{
			TestList = TEXT("BuildPatchServices");
		}
		TestList.InsertAt(0, TEXT("Automation RunTests "));

		return true;
#undef PARSE_SWITCH
	}

private:
	IBuildPatchServicesModule& BpsInterface;
	bool bHelp;
	FString TestList;
};

BuildPatchTool::IToolModeRef BuildPatchTool::FAutomationToolModeFactory::Create(IBuildPatchServicesModule& BpsInterface)
{
	return MakeShareable(new FAutomationToolMode(BpsInterface));
}

#endif // !UE_BUILD_SHIPPING
