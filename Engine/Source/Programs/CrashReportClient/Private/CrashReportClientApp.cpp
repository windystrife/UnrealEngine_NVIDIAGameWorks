// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Misc/QueuedThreadPool.h"
#include "Internationalization/Internationalization.h"
#include "Math/Vector2D.h"
#include "Misc/ConfigCacheIni.h"
#include "GenericPlatform/GenericApplication.h"
#include "Misc/App.h"
#include "CrashReportClientConfig.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "CrashDescription.h"
#include "CrashReportAnalytics.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformApplicationMisc.h"

#if !CRASH_REPORT_UNATTENDED_ONLY
	#include "SCrashReportClient.h"
	#include "CrashReportClient.h"
	#include "CrashReportClientStyle.h"
	#include "ISlateReflectorModule.h"
	#include "Framework/Application/SlateApplication.h"
#endif // !CRASH_REPORT_UNATTENDED_ONLY

#include "CrashReportClientUnattended.h"
#include "Async/TaskGraphInterfaces.h"
#include "RequiredProgramMainCPPInclude.h"

#include "MainLoopTiming.h"

#include "PlatformErrorReport.h"
#include "XmlFile.h"

/** Default main window size */
const FVector2D InitialWindowDimensions(740, 560);

/** Average tick rate the app aims for */
const float IdealTickRate = 30.f;

/** Set this to true in the code to open the widget reflector to debug the UI */
const bool RunWidgetReflector = false;

IMPLEMENT_APPLICATION(CrashReportClient, "CrashReportClient");
DEFINE_LOG_CATEGORY(CrashReportClientLog);

/** Directory containing the report */
static TArray<FString> FoundReportDirectoryAbsolutePaths;

/** Name of the game passed via the command line. */
static FString GameNameFromCmd;

/** GUID of the crash passed via the command line. */
static FString CrashGUIDFromCmd;

/**
 * Look for the report to upload, either in the command line or in the platform's report queue
 */
void ParseCommandLine(const TCHAR* CommandLine)
{
	const TCHAR* CommandLineAfterExe = FCommandLine::RemoveExeName(CommandLine);

	FoundReportDirectoryAbsolutePaths.Empty();

	// Use the first argument if present and it's not a flag
	if (*CommandLineAfterExe)
	{
		TArray<FString> Switches;
		TArray<FString> Tokens;
		TMap<FString, FString> Params;
		{
			FString NextToken;
			while (FParse::Token(CommandLineAfterExe, NextToken, false))
			{
				if (**NextToken == TCHAR('-'))
				{
					new(Switches)FString(NextToken.Mid(1));
				}
				else
				{
					new(Tokens)FString(NextToken);
				}
			}

			for (int32 SwitchIdx = Switches.Num() - 1; SwitchIdx >= 0; --SwitchIdx)
			{
				FString& Switch = Switches[SwitchIdx];
				TArray<FString> SplitSwitch;
				if (2 == Switch.ParseIntoArray(SplitSwitch, TEXT("="), true))
				{
					Params.Add(SplitSwitch[0], SplitSwitch[1].TrimQuotes());
					Switches.RemoveAt(SwitchIdx);
				}
			}
		}

		if (Tokens.Num() > 0)
		{
			FoundReportDirectoryAbsolutePaths.Add(Tokens[0]);
		}

		GameNameFromCmd = Params.FindRef(TEXT("AppName"));

		CrashGUIDFromCmd = FString();
		if (Params.Contains(TEXT("CrashGUID")))
		{
			CrashGUIDFromCmd = Params.FindRef(TEXT("CrashGUID"));
		}
	}

	if (FoundReportDirectoryAbsolutePaths.Num() == 0)
	{
		FPlatformErrorReport::FindMostRecentErrorReports(FoundReportDirectoryAbsolutePaths, FTimespan::FromDays(30));  //FTimespan::FromMinutes(30));
	}
}

/**
 * Find the error report folder and check it matches the app name if provided
 */
FPlatformErrorReport LoadErrorReport()
{
	if (FoundReportDirectoryAbsolutePaths.Num() == 0)
	{
		UE_LOG(CrashReportClientLog, Warning, TEXT("No error report found"));
		return FPlatformErrorReport();
	}

	for (const FString& ReportDirectoryAbsolutePath : FoundReportDirectoryAbsolutePaths)
	{
		FPlatformErrorReport ErrorReport(ReportDirectoryAbsolutePath);

		FString Filename;
		// CrashContext.runtime-xml has the precedence over the WER
		if (ErrorReport.FindFirstReportFileWithExtension(Filename, *FGenericCrashContext::CrashContextExtension))
		{
			FPrimaryCrashProperties::Set(new FCrashContext(ReportDirectoryAbsolutePath / Filename));
		}
		else if (ErrorReport.FindFirstReportFileWithExtension(Filename, TEXT(".xml")))
		{
			FPrimaryCrashProperties::Set(new FCrashWERContext(ReportDirectoryAbsolutePath / Filename));
		}
		else
		{
			continue;
		}

#if CRASH_REPORT_UNATTENDED_ONLY
		return ErrorReport;
#else
		bool NameMatch = false;
		if (GameNameFromCmd.IsEmpty() || GameNameFromCmd == FPrimaryCrashProperties::Get()->GameName)
		{
			NameMatch = true;
		}

		bool GUIDMatch = false;
		if (CrashGUIDFromCmd.IsEmpty() || CrashGUIDFromCmd == FPrimaryCrashProperties::Get()->CrashGUID)
		{
			GUIDMatch = true;
		}

		if (NameMatch && GUIDMatch)
		{
			FString ConfigFilename;
			if (ErrorReport.FindFirstReportFileWithExtension(ConfigFilename, *FGenericCrashContext::CrashConfigExtension))
			{
				FConfigFile CrashConfigFile;
				CrashConfigFile.Read(ReportDirectoryAbsolutePath / ConfigFilename);
				FCrashReportClientConfig::Get().SetProjectConfigOverrides(CrashConfigFile);
			}

			return ErrorReport;
		}
#endif
	}

	// Don't display or upload anything if we can't find the report we expected
	return FPlatformErrorReport();
}

static void OnRequestExit()
{
	GIsRequestingExit = true;
}

#if !CRASH_REPORT_UNATTENDED_ONLY
bool RunWithUI(FPlatformErrorReport ErrorReport)
{
	// create the platform slate application (what FSlateApplication::Get() returns)
	TSharedRef<FSlateApplication> Slate = FSlateApplication::Create(MakeShareable(FPlatformApplicationMisc::CreateApplication()));

	// initialize renderer
	TSharedRef<FSlateRenderer> SlateRenderer = GetStandardStandaloneRenderer();

	// Grab renderer initialization retry settings from ini
	int32 SlateRendererInitRetryCount = 10;
	GConfig->GetInt(TEXT("CrashReportClient"), TEXT("UIInitRetryCount"), SlateRendererInitRetryCount, GEngineIni);
	double SlateRendererInitRetryInterval = 2.0;
	GConfig->GetDouble(TEXT("CrashReportClient"), TEXT("UIInitRetryInterval"), SlateRendererInitRetryInterval, GEngineIni);

	// Try to initialize the renderer. It's possible that we launched when the driver crashed so try a few times before giving up.
	bool bRendererInitialized = false;
	bool bRendererFailedToInitializeAtLeastOnce = false;
	do 
	{
		SlateRendererInitRetryCount--;
		bRendererInitialized = FSlateApplication::Get().InitializeRenderer(SlateRenderer, true);
		if (!bRendererInitialized && SlateRendererInitRetryCount > 0)
		{
			bRendererFailedToInitializeAtLeastOnce = true;
			FPlatformProcess::Sleep(SlateRendererInitRetryInterval);
		}
	} while (!bRendererInitialized && SlateRendererInitRetryCount > 0);

	if (!bRendererInitialized)
	{
		// Close down the Slate application
		FSlateApplication::Shutdown();
		return false;
	}
	else if (bRendererFailedToInitializeAtLeastOnce)
	{
		// Wait until the driver is fully restored
		FPlatformProcess::Sleep(2.0f);

		// Update the display metrics
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
		FSlateApplication::Get().GetPlatformApplication()->OnDisplayMetricsChanged().Broadcast(DisplayMetrics);
	}

	// Set up the main ticker
	FMainLoopTiming MainLoop(IdealTickRate, EMainLoopOptions::UsingSlate);

	// set the normal UE4 GIsRequestingExit when outer frame is closed
	FSlateApplication::Get().SetExitRequestedHandler(FSimpleDelegate::CreateStatic(&OnRequestExit));

	// Prepare the custom Slate styles
	FCrashReportClientStyle::Initialize();

	// Create the main implementation object
	TSharedRef<FCrashReportClient> CrashReportClient = MakeShareable(new FCrashReportClient(ErrorReport));

	// open up the app window	
	TSharedRef<SCrashReportClient> ClientControl = SNew(SCrashReportClient, CrashReportClient);

	const FSlateRect WorkArea = FSlateApplicationBase::Get().GetPreferredWorkArea();

	auto Window = FSlateApplication::Get().AddWindow(
		SNew(SWindow)
		.Title(NSLOCTEXT("CrashReportClient", "CrashReportClientAppName", "Unreal Engine 4 Crash Reporter"))
		.HasCloseButton(FCrashReportClientConfig::Get().IsAllowedToCloseWithoutSending())
		.ClientSize(InitialWindowDimensions * FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(WorkArea.Left, WorkArea.Top))
		[
			ClientControl
		]);

	Window->SetRequestDestroyWindowOverride(FRequestDestroyWindowOverride::CreateSP(CrashReportClient, &FCrashReportClient::RequestCloseWindow));

	// Setting focus seems to have to happen after the Window has been added
	FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);

	// Debugging code
	if (RunWidgetReflector)
	{
		FModuleManager::LoadModuleChecked<ISlateReflectorModule>("SlateReflector").DisplayWidgetReflector();
	}

	// loop until the app is ready to quit
	while (!GIsRequestingExit)
	{
		MainLoop.Tick();

		if (CrashReportClient->ShouldWindowBeHidden())
		{
			Window->HideWindow();
		}
	}

	// Clean up the custom styles
	FCrashReportClientStyle::Shutdown();

	// Close down the Slate application
	FSlateApplication::Shutdown();

	return true;
}
#endif // !CRASH_REPORT_UNATTENDED_ONLY

void RunUnattended(FPlatformErrorReport ErrorReport)
{
	// Set up the main ticker
	FMainLoopTiming MainLoop(IdealTickRate, EMainLoopOptions::CoreTickerOnly);

	// In the unattended mode we don't send any PII.
	FCrashReportClientUnattended CrashReportClient(ErrorReport);
	ErrorReport.SetUserComment(NSLOCTEXT("CrashReportClient", "UnattendedMode", "Sent in the unattended mode"));

	// loop until the app is ready to quit
	while (!GIsRequestingExit)
	{
		MainLoop.Tick();
	}
}


void RunCrashReportClient(const TCHAR* CommandLine)
{
	// Override the stack size for the thread pool.
	FQueuedThreadPool::OverrideStackSize = 256 * 1024;

	// Set up the main loop
	GEngineLoop.PreInit(CommandLine);

	// Make sure all UObject classes are registered and default properties have been initialized
	ProcessNewlyLoadedUObjects();

	// Tell the module manager is may now process newly-loaded UObjects when new C++ modules are loaded
	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	// Initialize config.
	FCrashReportClientConfig::Get();

	const bool bUnattended = 
#if CRASH_REPORT_UNATTENDED_ONLY
		true;
#else
		FApp::IsUnattended();
#endif // CRASH_REPORT_UNATTENDED_ONLY

	// Find the report to upload in the command line arguments
	ParseCommandLine(CommandLine);

	// Increase the HttpSendTimeout to 5 minutes
	GConfig->SetFloat(TEXT("HTTP"), TEXT("HttpSendTimeout"), 5*60.0f, GEngineIni);

	FPlatformErrorReport::Init();
	FPlatformErrorReport ErrorReport = LoadErrorReport();
	
	if (ErrorReport.HasFilesToUpload() && FPrimaryCrashProperties::Get() != nullptr)
	{
		ErrorReport.SetCrashReportClientVersion(FCrashReportClientConfig::Get().GetVersion());

		FCrashReportAnalytics::Initialize();

		if (bUnattended)
		{
			RunUnattended(ErrorReport);
		}
#if !CRASH_REPORT_UNATTENDED_ONLY
		else
		{
			if (!RunWithUI(ErrorReport))
			{
				// UI failed to initialize, probably due to driver crash. Send in unattended mode if allowed.
				bool bCanSendWhenUIFailedToInitialize = true;
				GConfig->GetBool(TEXT("CrashReportClient"), TEXT("CanSendWhenUIFailedToInitialize"), bCanSendWhenUIFailedToInitialize, GEngineIni);
				if (bCanSendWhenUIFailedToInitialize && !FCrashReportClientConfig::Get().IsAllowedToCloseWithoutSending())
				{
					RunUnattended(ErrorReport);
				}
			}
		}
#endif // !CRASH_REPORT_UNATTENDED_ONLY

		// Shutdown analytics.
		FCrashReportAnalytics::Shutdown();
	}
	else
	{
		// Needed to let systems that are shutting down that we are shutting down by request
		GIsRequestingExit = true;
	}

	FPrimaryCrashProperties::Shutdown();
	FPlatformErrorReport::ShutDown();

	FEngineLoop::AppPreExit();
	FModuleManager::Get().UnloadModulesAtShutdown();
	FTaskGraphInterface::Shutdown();

	FEngineLoop::AppExit();
}
