// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LaunchEngineLoop.h"

#include "HAL/PlatformStackWalk.h"
#include "HAL/PlatformOutputDevices.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/QueuedThreadPool.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformAffinity.h"
#include "Misc/FileHelper.h"
#include "Internationalization/TextLocalizationManagerGlobals.h"
#include "Logging/LogSuppressionInterface.h"
#include "Async/TaskGraphInterfaces.h"
#include "Misc/TimeGuard.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/OutputDeviceConsole.h"
#include "HAL/PlatformFilemanager.h"
#include "Templates/ScopedPointer.h"
#include "HAL/FileManagerGeneric.h"
#include "HAL/ExceptionHandling.h"
#include "Stats/StatsMallocProfilerProxy.h"
#include "HAL/PlatformSplash.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/ThreadManager.h"
#include "ProfilingDebugging/ExternalProfiler.h"
#include "Containers/Ticker.h"

#include "Interfaces/IPluginManager.h"
#include "ProjectDescriptor.h"
#include "Interfaces/IProjectManager.h"
#include "Misc/UProjectInfo.h"
#include "Misc/EngineVersion.h"

#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "Runtime/Launch/Resources/Version.h"
#include "BuildVersion.h"
#include "ModuleManifest.h"
#include "UObject/DevObjectVersion.h"
#include "HAL/ThreadHeartBeat.h"

#include "Misc/NetworkVersion.h"
#include "UniquePtr.h"

#if WITH_COREUOBJECT
	#include "Internationalization/PackageLocalizationManager.h"
	#include "Misc/PackageName.h"
	#include "Misc/StartupPackages.h"
	#include "UObject/UObjectHash.h"
	#include "UObject/Package.h"
	#include "UObject/Linker.h"
	#include "UObject/LinkerLoad.h"
#endif

#if WITH_EDITOR
	#include "Blueprint/BlueprintSupport.h"
	#include "EditorStyleSet.h"
	#include "Misc/RemoteConfigIni.h"
	#include "EditorCommandLineUtils.h"
	#include "Input/Reply.h"
	#include "Styling/CoreStyle.h"
	#include "RenderingThread.h"
	#include "Editor/EditorEngine.h"
	#include "UnrealEdMisc.h"
	#include "UnrealEdGlobals.h"
	#include "Editor/UnrealEdEngine.h"
	#include "Settings/EditorExperimentalSettings.h"
	#include "Interfaces/IEditorStyleModule.h"
	#include "PIEPreviewDeviceProfileSelectorModule.h"

	#if PLATFORM_WINDOWS
		#include "AllowWindowsPlatformTypes.h"
			#include <objbase.h>
		#include "HideWindowsPlatformTypes.h"
	#endif
#endif //WITH_EDITOR

#if WITH_ENGINE
	#include "Engine/GameEngine.h"
	#include "UnrealClient.h"
	#include "Engine/LocalPlayer.h"
	#include "GameFramework/PlayerController.h"
	#include "GameFramework/GameUserSettings.h"
	#include "Features/IModularFeatures.h"
	#include "GameFramework/WorldSettings.h"
	#include "SystemSettings.h"
	#include "EngineStats.h"
	#include "EngineGlobals.h"
	#include "AudioThread.h"
#if WITH_ENGINE && !UE_BUILD_SHIPPING
	#include "IAutomationControllerModule.h"
#endif // WITH_ENGINE && !UE_BUILD_SHIPPING
	#include "Database.h"
	#include "DerivedDataCacheInterface.h"
	#include "ShaderCompiler.h"
	#include "DistanceFieldAtlas.h"
	#include "GlobalShader.h"
	#include "ShaderCodeLibrary.h"
	#include "Materials/MaterialInterface.h"
	#include "TextureResource.h"
	#include "Engine/Texture2D.h"
	#include "StringTable.h"
	#include "SceneUtils.h"
	#include "ParticleHelper.h"
	#include "PhysicsPublic.h"
	#include "PlatformFeatures.h"
	#include "DeviceProfiles/DeviceProfileManager.h"
	#include "Commandlets/Commandlet.h"
	#include "EngineService.h"
	#include "ContentStreaming.h"
	#include "HighResScreenshot.h"
	#include "Misc/HotReloadInterface.h"
	#include "ISessionServicesModule.h"
	#include "Net/OnlineEngineInterface.h"
	#include "Internationalization/EnginePackageLocalizationCache.h"
	#include "Rendering/SlateRenderer.h"
	#include "Layout/WidgetPath.h"
	#include "Framework/Application/SlateApplication.h"
	#include "IMessagingModule.h"
	#include "Engine/DemoNetDriver.h"
	#include "LongGPUTask.h"
	#include "RenderUtils.h"

#if !UE_SERVER
	#include "AppMediaTimeSource.h"
	#include "IHeadMountedDisplayModule.h"
	#include "IMediaModule.h"
	#include "HeadMountedDisplay.h"
	#include "MRMeshModule.h"
	#include "Interfaces/ISlateRHIRendererModule.h"
	#include "Interfaces/ISlateNullRendererModule.h"
	#include "EngineFontServices.h"
#endif

	#include "MoviePlayer.h"

	#include "ShaderCodeLibrary.h"
	#include "ShaderCache.h"

#if !UE_BUILD_SHIPPING
	#include "STaskGraph.h"
	#include "IProfilerServiceModule.h"
#endif

#if WITH_AUTOMATION_WORKER
	#include "IAutomationWorkerModule.h"
#endif
#endif  //WITH_ENGINE

class FSlateRenderer;
class SViewport;
class IPlatformFile;
class FExternalProfiler;
class FFeedbackContext;

#if WITH_EDITOR
	#include "FeedbackContextEditor.h"
	static FFeedbackContextEditor UnrealEdWarn;
	#include "AudioEditorModule.h"
#endif	// WITH_EDITOR

#if UE_EDITOR
	#include "DesktopPlatformModule.h"
#endif

#define LOCTEXT_NAMESPACE "LaunchEngineLoop"

#if PLATFORM_WINDOWS
	#include "AllowWindowsPlatformTypes.h"
	#include <ObjBase.h>
	#include "HideWindowsPlatformTypes.h"
#endif

#if WITH_ENGINE
	#include "EngineDefines.h"
	#if ENABLE_VISUAL_LOG
		#include "VisualLogger/VisualLogger.h"
	#endif
	#include "CsvProfiler.h"
#endif

#if defined(WITH_LAUNCHERCHECK) && WITH_LAUNCHERCHECK
	#include "ILauncherCheckModule.h"
#endif

#if WITH_COREUOBJECT
	#ifndef USE_LOCALIZED_PACKAGE_CACHE
		#define USE_LOCALIZED_PACKAGE_CACHE 1
	#endif
#else
	#define USE_LOCALIZED_PACKAGE_CACHE 0
#endif

int32 GUseDisregardForGCOnDedicatedServers = 1;
static FAutoConsoleVariableRef CVarUseDisregardForGCOnDedicatedServers(
	TEXT("gc.UseDisregardForGCOnDedicatedServers"),
	GUseDisregardForGCOnDedicatedServers,
	TEXT("If false, DisregardForGC will be disabled for dedicated servers."),
	ECVF_Default
);

static TAutoConsoleVariable<int32> CVarDoAsyncEndOfFrameTasksRandomize(
	TEXT("tick.DoAsyncEndOfFrameTasks.Randomize"),
	0,
	TEXT("Used to add random sleeps to tick.DoAsyncEndOfFrameTasks to shake loose bugs on either thread. Also does random render thread flushes from the game thread.")
	);

static FAutoConsoleTaskPriority CPrio_AsyncEndOfFrameGameTasks(
	TEXT("TaskGraph.TaskPriorities.AsyncEndOfFrameGameTasks"),
	TEXT("Task and thread priority for the experiemntal async end of frame tasks."),
	ENamedThreads::HighThreadPriority,
	ENamedThreads::NormalTaskPriority,
	ENamedThreads::HighTaskPriority
	);

/** Task that executes concurrently with Slate when tick.DoAsyncEndOfFrameTasks is true. */
class FExecuteConcurrentWithSlateTickTask
{
	TFunctionRef<void()> TickWithSlate;

public:

	FExecuteConcurrentWithSlateTickTask(TFunctionRef<void()> InTickWithSlate)
		: TickWithSlate(InTickWithSlate)
	{
	}

	static FORCEINLINE TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FExecuteConcurrentWithSlateTickTask, STATGROUP_TaskGraphTasks);
	}

	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_AsyncEndOfFrameGameTasks.Get();
	}

	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		TickWithSlate();
	}
};

// Pipe output to std output
// This enables UBT to collect the output for it's own use
class FOutputDeviceStdOutput : public FOutputDevice
{
public:

	FOutputDeviceStdOutput()
		: AllowedLogVerbosity(ELogVerbosity::Display)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("AllowStdOutLogVerbosity")))
		{
			AllowedLogVerbosity = ELogVerbosity::Log;
		}

		if (FParse::Param(FCommandLine::Get(), TEXT("FullStdOutLogOutput")))
		{
			AllowedLogVerbosity = ELogVerbosity::All;
		}
	}

	virtual ~FOutputDeviceStdOutput()
	{
	}

	virtual bool CanBeUsedOnAnyThread() const override
	{
		return true;
	}

	virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override
	{
		if (Verbosity <= AllowedLogVerbosity)
		{
#if PLATFORM_USE_LS_SPEC_FOR_WIDECHAR
			wprintf(TEXT("%ls\n"), *FOutputDeviceHelper::FormatLogLine(Verbosity, Category, V, GPrintLogTimes));
#else
			wprintf(TEXT("%s\n"), *FOutputDeviceHelper::FormatLogLine(Verbosity, Category, V, GPrintLogTimes));
#endif
			fflush(stdout);
		}
	}

private:
	ELogVerbosity::Type AllowedLogVerbosity;
};


// Exits the game/editor if any of the specified phrases appears in the log output
class FOutputDeviceTestExit : public FOutputDevice
{
	TArray<FString> ExitPhrases;
public:
	FOutputDeviceTestExit(const TArray<FString>& InExitPhrases)
		: ExitPhrases(InExitPhrases)
	{
	}
	virtual ~FOutputDeviceTestExit()
	{
	}
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		if (!GIsRequestingExit)
		{
			for (auto& Phrase : ExitPhrases)
			{
				if (FCString::Stristr(V, *Phrase) && !FCString::Stristr(V, TEXT("-testexit=")))
				{
#if WITH_ENGINE
					if (GEngine != nullptr)
					{
						if (GIsEditor)
						{
							GEngine->DeferredCommands.Add(TEXT("CLOSE_SLATE_MAINFRAME"));
						}
						else
						{
							GEngine->Exec(nullptr, TEXT("QUIT"));
						}
					}
#else
					FPlatformMisc::RequestExit(true);
#endif
					break;
				}
			}
		}
	}
};


static TUniquePtr<FOutputDeviceConsole>	GScopedLogConsole;
static TUniquePtr<FOutputDeviceStdOutput> GScopedStdOut;
static TUniquePtr<FOutputDeviceTestExit> GScopedTestExit;


#if WITH_ENGINE
static void RHIExitAndStopRHIThread()
{
#if HAS_GPU_STATS
	FRealtimeGPUProfiler::Get()->Release();
#endif
	RHIExit();

	// Stop the RHI Thread
	if (GRHIThread_InternalUseOnly)
	{
		DECLARE_CYCLE_STAT(TEXT("Wait For RHIThread Finish"), STAT_WaitForRHIThreadFinish, STATGROUP_TaskGraphTasks);
		FGraphEventRef QuitTask = TGraphTask<FReturnGraphTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(ENamedThreads::RHIThread);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(QuitTask, ENamedThreads::GameThread_Local);
	}
}
#endif

extern void DeferredPhysResourceCleanup();


/**
 * Initializes std out device and adds it to GLog
 **/
void InitializeStdOutDevice()
{
	// Check if something is trying to initialize std out device twice.
	check(!GScopedStdOut);

	GScopedStdOut = MakeUnique<FOutputDeviceStdOutput>();
	GLog->AddOutputDevice(GScopedStdOut.Get());
}


bool ParseGameProjectFromCommandLine(const TCHAR* InCmdLine, FString& OutProjectFilePath, FString& OutGameName)
{
	const TCHAR *CmdLine = InCmdLine;
	FString FirstCommandLineToken = FParse::Token(CmdLine, 0);

	// trim any whitespace at edges of string - this can happen if the token was quoted with leading or trailing whitespace
	// VC++ tends to do this in its "external tools" config
	FirstCommandLineToken.TrimStartInline();

	//
	OutProjectFilePath = TEXT("");
	OutGameName = TEXT("");

	if ( FirstCommandLineToken.Len() && !FirstCommandLineToken.StartsWith(TEXT("-")) )
	{
		// The first command line argument could be the project file if it exists or the game name if not launching with a project file
		const FString ProjectFilePath = FString(FirstCommandLineToken);
		if ( FPaths::GetExtension(ProjectFilePath) == FProjectDescriptor::GetExtension() )
		{
			OutProjectFilePath = FirstCommandLineToken;
			// Here we derive the game name from the project file
			OutGameName = FPaths::GetBaseFilename(OutProjectFilePath);
			return true;
		}
		else if (FPaths::IsRelative(FirstCommandLineToken) && FPlatformProperties::IsMonolithicBuild() == false)
		{
			// Full game name is assumed to be the first token
			OutGameName = MoveTemp(FirstCommandLineToken);
			// Derive the project path from the game name. All games must have a uproject file, even if they are in the root folder.
			OutProjectFilePath = FPaths::Combine(*FPaths::RootDir(), *OutGameName, *FString(OutGameName + TEXT(".") + FProjectDescriptor::GetExtension()));
			return true;
		}
	}

#if WITH_EDITOR
	if (FEditorCommandLineUtils::ParseGameProjectPath(InCmdLine, OutProjectFilePath, OutGameName))
	{
		return true;
	}
#endif
	return false;
}


bool LaunchSetGameName(const TCHAR *InCmdLine, FString& OutGameProjectFilePathUnnormalized)
{
	if (GIsGameAgnosticExe)
	{
		// Initialize GameName to an empty string. Populate it below.
		FApp::SetProjectName(TEXT(""));

		FString ProjFilePath;
		FString LocalGameName;
		if (ParseGameProjectFromCommandLine(InCmdLine, ProjFilePath, LocalGameName) == true)
		{
			// Only set the game name if this is NOT a program...
			if (FPlatformProperties::IsProgram() == false)
			{
				FApp::SetProjectName(*LocalGameName);
			}
			OutGameProjectFilePathUnnormalized = ProjFilePath;
			FPaths::SetProjectFilePath(ProjFilePath);
		}
#if UE_GAME
		else
		{
			// Try to use the executable name as the game name.
			LocalGameName = FPlatformProcess::ExecutableName();
			int32 FirstCharToRemove = INDEX_NONE;
			if (LocalGameName.FindChar(TCHAR('-'), FirstCharToRemove))
			{
				LocalGameName = LocalGameName.Left(FirstCharToRemove);
			}
			FApp::SetProjectName(*LocalGameName);

			// Check it's not UE4Game, otherwise assume a uproject file relative to the game project directory
			if (LocalGameName != TEXT("UE4Game"))
			{
				ProjFilePath = FPaths::Combine(TEXT(".."), TEXT(".."), TEXT(".."), *LocalGameName, *FString(LocalGameName + TEXT(".") + FProjectDescriptor::GetExtension()));
				OutGameProjectFilePathUnnormalized = ProjFilePath;
				FPaths::SetProjectFilePath(ProjFilePath);
			}
		}
#endif

		static bool bPrinted = false;
		if (!bPrinted)
		{
			bPrinted = true;
			if (FApp::HasProjectName())
			{
				UE_LOG(LogInit, Display, TEXT("Running engine for game: %s"), FApp::GetProjectName());
			}
			else
			{
				if (FPlatformProperties::RequiresCookedData())
				{
					UE_LOG(LogInit, Fatal, TEXT("Non-agnostic games on cooked platforms require a uproject file be specified."));
				}
				else
				{
					UE_LOG(LogInit, Display, TEXT("Running engine without a game"));
				}
			}
		}
	}
	else
	{
		FString ProjFilePath;
		FString LocalGameName;
		if (ParseGameProjectFromCommandLine(InCmdLine, ProjFilePath, LocalGameName) == true)
		{
			if (FPlatformProperties::RequiresCookedData())
			{
				// Non-agnostic exes that require cooked data cannot load projects, so make sure that the LocalGameName is the GameName
				if (LocalGameName != FApp::GetProjectName())
				{
					UE_LOG(LogInit, Fatal, TEXT("Non-agnostic games cannot load projects on cooked platforms - try running UE4Game."));
				}
			}
			// Only set the game name if this is NOT a program...
			if (FPlatformProperties::IsProgram() == false)
			{
				FApp::SetProjectName(*LocalGameName);
			}
			OutGameProjectFilePathUnnormalized = ProjFilePath;
			FPaths::SetProjectFilePath(ProjFilePath);
		}

		// In a non-game agnostic exe, the game name should already be assigned by now.
		if (!FApp::HasProjectName())
		{
			UE_LOG(LogInit, Fatal,TEXT("Could not set game name!"));
		}
	}

	return true;
}


void LaunchFixGameNameCase()
{
#if PLATFORM_DESKTOP && !IS_PROGRAM
	// This is to make sure this function is not misused and is only called when the game name is set
	check(FApp::HasProjectName());

	// correct the case of the game name, if possible (unless we're running a program and the game name is already set)
	if (FPaths::IsProjectFilePathSet())
	{
		const FString GameName(FPaths::GetBaseFilename(IFileManager::Get().GetFilenameOnDisk(*FPaths::GetProjectFilePath())));

		const bool bGameNameMatchesProjectCaseSensitive = (FCString::Strcmp(*GameName, FApp::GetProjectName()) == 0);
		if (!bGameNameMatchesProjectCaseSensitive && (FApp::IsProjectNameEmpty() || GIsGameAgnosticExe || (GameName.Len() > 0 && GIsGameAgnosticExe)))
		{
			if (GameName == FApp::GetProjectName()) // case insensitive compare
			{
				FApp::SetProjectName(*GameName);
			}
			else
			{
				const FText Message = FText::Format(
					NSLOCTEXT("Core", "MismatchedGameNames", "The name of the .uproject file ('{0}') must match the name of the project passed in the command line ('{1}')."),
					FText::FromString(*GameName),
					FText::FromString(FApp::GetProjectName()));
				if (!GIsBuildMachine)
				{
					UE_LOG(LogInit, Warning, TEXT("%s"), *Message.ToString());
					FMessageDialog::Open(EAppMsgType::Ok, Message);
				}
				FApp::SetProjectName(TEXT("")); // this disables part of the crash reporter to avoid writing log files to a bogus directory
				if (!GIsBuildMachine)
				{
					exit(1);
				}
				UE_LOG(LogInit, Fatal, TEXT("%s"), *Message.ToString());
			}
		}
	}
#endif	//PLATFORM_DESKTOP
}


static IPlatformFile* ConditionallyCreateFileWrapper(const TCHAR* Name, IPlatformFile* CurrentPlatformFile, const TCHAR* CommandLine, bool* OutFailedToInitialize = nullptr, bool* bOutShouldBeUsed = nullptr )
{
	if (OutFailedToInitialize)
	{
		*OutFailedToInitialize = false;
	}
	if ( bOutShouldBeUsed )
	{
		*bOutShouldBeUsed = false;
	}
	IPlatformFile* WrapperFile = FPlatformFileManager::Get().GetPlatformFile(Name);
	if (WrapperFile != nullptr && WrapperFile->ShouldBeUsed(CurrentPlatformFile, CommandLine))
	{
		if ( bOutShouldBeUsed )
		{
			*bOutShouldBeUsed = true;
		}
		if (WrapperFile->Initialize(CurrentPlatformFile, CommandLine) == false)
		{
			if (OutFailedToInitialize)
			{
				*OutFailedToInitialize = true;
			}
			// Don't delete the platform file. It will be automatically deleted by its module.
			WrapperFile = nullptr;
		}
	}
	else
	{
		// Make sure it won't be used.
		WrapperFile = nullptr;
	}
	return WrapperFile;
}


/**
 * Look for any file overrides on the command line (i.e. network connection file handler)
 */
bool LaunchCheckForFileOverride(const TCHAR* CmdLine, bool& OutFileOverrideFound)
{
	OutFileOverrideFound = false;

	// Get the physical platform file.
	IPlatformFile* CurrentPlatformFile = &FPlatformFileManager::Get().GetPlatformFile();

	// Try to create pak file wrapper
	{
		IPlatformFile* PlatformFile = ConditionallyCreateFileWrapper(TEXT("PakFile"), CurrentPlatformFile, CmdLine);
		if (PlatformFile)
		{
			CurrentPlatformFile = PlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}
		PlatformFile = ConditionallyCreateFileWrapper(TEXT("CachedReadFile"), CurrentPlatformFile, CmdLine);
		if (PlatformFile)
		{
			CurrentPlatformFile = PlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}
	}

	// Try to create sandbox wrapper
	{
		IPlatformFile* PlatformFile = ConditionallyCreateFileWrapper(TEXT("SandboxFile"), CurrentPlatformFile, CmdLine);
		if (PlatformFile)
		{
			CurrentPlatformFile = PlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}
	}

#if !UE_BUILD_SHIPPING // UFS clients are not available in shipping builds.
	// Streaming network wrapper (it has a priority over normal network wrapper)
	bool bNetworkFailedToInitialize = false;
	do
	{
		bool bShouldUseStreamingFile = false;
		IPlatformFile* NetworkPlatformFile = ConditionallyCreateFileWrapper(TEXT("StreamingFile"), CurrentPlatformFile, CmdLine, &bNetworkFailedToInitialize, &bShouldUseStreamingFile);
		if (NetworkPlatformFile)
		{
			CurrentPlatformFile = NetworkPlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}

		bool bShouldUseCookedIterativeFile = false;
		if ( !bShouldUseStreamingFile && !NetworkPlatformFile )
		{
			NetworkPlatformFile = ConditionallyCreateFileWrapper(TEXT("CookedIterativeFile"), CurrentPlatformFile, CmdLine, &bNetworkFailedToInitialize, &bShouldUseCookedIterativeFile);
			if (NetworkPlatformFile)
			{
				CurrentPlatformFile = NetworkPlatformFile;
				FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
			}
		}

		// if streaming network platform file was tried this loop don't try this one
		// Network file wrapper (only create if the streaming wrapper hasn't been created)
		if ( !bShouldUseStreamingFile && !bShouldUseCookedIterativeFile && !NetworkPlatformFile)
		{
			NetworkPlatformFile = ConditionallyCreateFileWrapper(TEXT("NetworkFile"), CurrentPlatformFile, CmdLine, &bNetworkFailedToInitialize);
			if (NetworkPlatformFile)
			{
				CurrentPlatformFile = NetworkPlatformFile;
				FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
			}
		}

		if (bNetworkFailedToInitialize)
		{
			FString HostIpString;
			FParse::Value(CmdLine, TEXT("-FileHostIP="), HostIpString);
#if PLATFORM_REQUIRES_FILESERVER
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed to connect to file server at %s. RETRYING in 5s.\n"), *HostIpString);
			FPlatformProcess::Sleep(5.0f);
			uint32 Result = 2;
#else	//PLATFORM_REQUIRES_FILESERVER
			// note that this can't be localized because it happens before we connect to a filserver - localizing would cause ICU to try to load.... from over the file server connection!
			FString Error = FString::Printf(TEXT("Failed to connect to any of the following file servers:\n\n    %s\n\nWould you like to try again? No will fallback to local disk files, Cancel will quit."), *HostIpString.Replace( TEXT("+"), TEXT("\n    ")));
			uint32 Result = FMessageDialog::Open( EAppMsgType::YesNoCancel, FText::FromString( Error ) );
#endif	//PLATFORM_REQUIRES_FILESERVER

			if (Result == EAppReturnType::No)
			{
				break;
			}
			else if (Result == EAppReturnType::Cancel)
			{
				// Cancel - return a failure, and quit
				return false;
			}
		}
	}
	while (bNetworkFailedToInitialize);
#endif

#if !UE_BUILD_SHIPPING
	// Try to create file profiling wrapper
	{
		IPlatformFile* PlatformFile = ConditionallyCreateFileWrapper(TEXT("ProfileFile"), CurrentPlatformFile, CmdLine);
		if (PlatformFile)
		{
			CurrentPlatformFile = PlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}
	}
	{
		IPlatformFile* PlatformFile = ConditionallyCreateFileWrapper(TEXT("SimpleProfileFile"), CurrentPlatformFile, CmdLine);
		if (PlatformFile)
		{
			CurrentPlatformFile = PlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}
	}
	// Try and create file timings stats wrapper
	{
		IPlatformFile* PlatformFile = ConditionallyCreateFileWrapper(TEXT("FileReadStats"), CurrentPlatformFile, CmdLine);
		if (PlatformFile)
		{
			CurrentPlatformFile = PlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}
	}
	// Try and create file open log wrapper (lists the order files are first opened)
	{
		IPlatformFile* PlatformFile = ConditionallyCreateFileWrapper(TEXT("FileOpenLog"), CurrentPlatformFile, CmdLine);
		if (PlatformFile)
		{
			CurrentPlatformFile = PlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}
	}
#endif	//#if !UE_BUILD_SHIPPING

	// Wrap the above in a file logging singleton if requested
	{
		IPlatformFile* PlatformFile = ConditionallyCreateFileWrapper(TEXT("LogFile"), CurrentPlatformFile, CmdLine);
		if (PlatformFile)
		{
			CurrentPlatformFile = PlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}
	}

	// If our platform file is different than it was when we started, then an override was used
	OutFileOverrideFound = (CurrentPlatformFile != &FPlatformFileManager::Get().GetPlatformFile());

	return true;
}


bool LaunchHasIncompleteGameName()
{
	if ( FApp::HasProjectName() && !FPaths::IsProjectFilePathSet() )
	{
		// Verify this is a legitimate game name
		// Launched with a game name. See if the <GameName> folder exists. If it doesn't, it could instead be <GameName>Game
		const FString NonSuffixedGameFolder = FPaths::RootDir() / FApp::GetProjectName();
		if (FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*NonSuffixedGameFolder) == false)
		{
			const FString SuffixedGameFolder = NonSuffixedGameFolder + TEXT("Game");
			if (FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*SuffixedGameFolder))
			{
				return true;
			}
		}
	}

	return false;
}


void LaunchUpdateMostRecentProjectFile()
{
	// If we are launching without a game name or project file, we should use the last used project file, if it exists
	const FString& AutoLoadProjectFileName = IProjectManager::Get().GetAutoLoadProjectFileName();
	FString RecentProjectFileContents;
	if ( FFileHelper::LoadFileToString(RecentProjectFileContents, *AutoLoadProjectFileName) )
	{
		if ( RecentProjectFileContents.Len() )
		{
			const FString AutoLoadInProgressFilename = AutoLoadProjectFileName + TEXT(".InProgress");
			if ( FPlatformFileManager::Get().GetPlatformFile().FileExists(*AutoLoadInProgressFilename) )
			{
				// We attempted to auto-load a project but the last run did not make it to UEditorEngine::InitEditor.
				// This indicates that there was a problem loading the project.
				// Do not auto-load the project, instead load normally until the next time the editor starts successfully.
				UE_LOG(LogInit, Display, TEXT("There was a problem auto-loading %s. Auto-load will be disabled until the editor successfully starts up with a project."), *RecentProjectFileContents);
			}
			else if ( FPlatformFileManager::Get().GetPlatformFile().FileExists(*RecentProjectFileContents) )
			{
				// The previously loaded project file was found. Change the game name here and update the project file path
				FApp::SetProjectName(*FPaths::GetBaseFilename(RecentProjectFileContents));
				FPaths::SetProjectFilePath(RecentProjectFileContents);
				UE_LOG(LogInit, Display, TEXT("Loading recent project file: %s"), *RecentProjectFileContents);

				// Write a file indicating that we are trying to auto-load a project.
				// This file prevents auto-loading of projects for as long as it exists. It is a detection system for failed auto-loads.
				// The file is deleted in UEditorEngine::InitEditor, thus if the load does not make it that far then the project will not be loaded again.
				FFileHelper::SaveStringToFile(TEXT(""), *AutoLoadInProgressFilename);
			}
		}
	}
}


/*-----------------------------------------------------------------------------
	FEngineLoop implementation.
-----------------------------------------------------------------------------*/

FEngineLoop::FEngineLoop()
#if WITH_ENGINE
	: EngineService(nullptr)
#endif
{ }


int32 FEngineLoop::PreInit(int32 ArgC, TCHAR* ArgV[], const TCHAR* AdditionalCommandline)
{
	FMemory::SetupTLSCachesOnCurrentThread();

	FString CmdLine;

	// loop over the parameters, skipping the first one (which is the executable name)
	for (int32 Arg = 1; Arg < ArgC; Arg++)
	{
		FString ThisArg = ArgV[Arg];
		if (ThisArg.Contains(TEXT(" ")) && !ThisArg.Contains(TEXT("\"")))
		{
			int32 EqualsAt = ThisArg.Find(TEXT("="));
			if (EqualsAt > 0 && ThisArg.Find(TEXT(" ")) > EqualsAt)
			{
				ThisArg = ThisArg.Left(EqualsAt + 1) + FString("\"") + ThisArg.RightChop(EqualsAt + 1) + FString("\"");

			}
			else
			{
				ThisArg = FString("\"") + ThisArg + FString("\"");
			}
		}

		CmdLine += ThisArg;
		// put a space between each argument (not needed after the end)
		if (Arg + 1 < ArgC)
		{
			CmdLine += TEXT(" ");
		}
	}

	// append the additional extra command line
	if (AdditionalCommandline)
	{
		CmdLine += TEXT(" ");
		CmdLine += AdditionalCommandline;
	}

	// send the command line without the exe name
	return GEngineLoop.PreInit(*CmdLine);
}


#if WITH_ENGINE
bool IsServerDelegateForOSS(FName WorldContextHandle)
{
	if (IsRunningDedicatedServer())
	{
		return true;
	}

	UWorld* World = nullptr;
#if WITH_EDITOR
	if (WorldContextHandle != NAME_None)
	{
		FWorldContext& WorldContext = GEngine->GetWorldContextFromHandleChecked(WorldContextHandle);
		check(WorldContext.WorldType == EWorldType::Game || WorldContext.WorldType == EWorldType::PIE);
		World = WorldContext.World();
	}
	else
#endif
	{
		ensure(WorldContextHandle == NAME_None);
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);

		if (GameEngine)
		{
			World = GameEngine->GetGameWorld();
		}
		else
		{
			UE_LOG(LogInit, Error, TEXT("Failed to determine if OSS is server in PIE, OSS requests will fail"));
			return false;
		}
	}

	ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
	return (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer);
}
#endif

DECLARE_CYCLE_STAT( TEXT( "FEngineLoop::PreInit.AfterStats" ), STAT_FEngineLoop_PreInit_AfterStats, STATGROUP_LoadTime );

int32 FEngineLoop::PreInit( const TCHAR* CmdLine )
{
	// disable/enable LLM based on commandline
	LLM(FLowLevelMemTracker::Get().ProcessCommandLine(CmdLine));
	LLM_SCOPE(ELLMTag::EnginePreInitMemory);

	FPlatformMisc::InitTaggedStorage(1024);

	if (FParse::Param(CmdLine, TEXT("UTF8Output")))
	{
		FPlatformMisc::SetUTF8Output();
	}

	// Switch into executable's directory.
	FPlatformProcess::SetCurrentWorkingDirectoryToBaseDir();

	// this is set later with shorter command lines, but we want to make sure it is set ASAP as some subsystems will do the tests themselves...
	// also realize that command lines can be pulled from the network at a slightly later time.
	if (!FCommandLine::Set(CmdLine))
	{
		// Fail, shipping builds will crash if setting command line fails
		return -1;
	}

	// Check for special instruction cpu support if needed
	if (FPlatformMisc::NeedsNonoptionalCPUFeaturesCheck())
	{
		bool bHasNonoptionalCPUFeatures = FPlatformMisc::HasNonoptionalCPUFeatures();
		// If it's not supported, we'll crash later so better to return an error
		if (!bHasNonoptionalCPUFeatures)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("RequiresNonoptionalCPUFeatures", "Error: This application requires a CPU that supports the specific instruction set(s)"));
			return -1;
		}
	}

#if WITH_ENGINE
	FCoreUObjectDelegates::PostGarbageCollectConditionalBeginDestroy.AddStatic(DeferredPhysResourceCleanup);
#endif

#if defined(WITH_LAUNCHERCHECK) && WITH_LAUNCHERCHECK
	if (ILauncherCheckModule::Get().WasRanFromLauncher() == false)
	{
		// Tell Launcher to run us instead
		ILauncherCheckModule::Get().RunLauncher(ELauncherAction::AppLaunch);
		// We wish to exit
		GIsRequestingExit = true;
		return 0;
	}
#endif

#if	STATS
	// Create the stats malloc profiler proxy.
	if( FStatsMallocProfilerProxy::HasMemoryProfilerToken() )
	{
		if (PLATFORM_USES_FIXED_GMalloc_CLASS)
		{
			UE_LOG(LogMemory, Fatal, TEXT("Cannot do malloc profiling with PLATFORM_USES_FIXED_GMalloc_CLASS."));
		}
		// Assumes no concurrency here.
		GMalloc = FStatsMallocProfilerProxy::Get();
	}
#endif // STATS

	// Name of project file before normalization (as specified in command line).
	// Used to fixup project name if necessary.
	FString GameProjectFilePathUnnormalized;

	// Set GameName, based on the command line
	if (LaunchSetGameName(CmdLine, GameProjectFilePathUnnormalized) == false)
	{
		// If it failed, do not continue
		return 1;
	}

	// Initialize log console here to avoid statics initialization issues when launched from the command line.
	GScopedLogConsole = TUniquePtr<FOutputDeviceConsole>(FPlatformApplicationMisc::CreateConsoleOutputDevice());

	// Always enable the backlog so we get all messages, we will disable and clear it in the game
	// as soon as we determine whether GIsEditor == false
	GLog->EnableBacklog(true);

	// Initialize std out device as early as possible if requested in the command line
	if (FParse::Param(FCommandLine::Get(), TEXT("stdout")))
	{
		InitializeStdOutDevice();
	}

#if !UE_BUILD_SHIPPING
	if (FPlatformProperties::SupportsQuit())
	{
		FString ExitPhrases;
		if (FParse::Value(FCommandLine::Get(), TEXT("testexit="), ExitPhrases))
		{
			TArray<FString> ExitPhrasesList;
			if (ExitPhrases.ParseIntoArray(ExitPhrasesList, TEXT("+"), true) > 0)
			{
				GScopedTestExit = MakeUnique<FOutputDeviceTestExit>(ExitPhrasesList);
				GLog->AddOutputDevice(GScopedTestExit.Get());
			}
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("emitdrawevents")))
	{
		GEmitDrawEvents = true;
	}
#endif // !UE_BUILD_SHIPPING

	// Switch into executable's directory (may be required by some of the platform file overrides)
	FPlatformProcess::SetCurrentWorkingDirectoryToBaseDir();

	// This fixes up the relative project path, needs to happen before we set platform file paths
	if (FPlatformProperties::IsProgram() == false)
	{
		if (FPaths::IsProjectFilePathSet())
		{
			FString ProjPath = FPaths::GetProjectFilePath();
			if (FPaths::FileExists(ProjPath) == false)
			{
				// display it multiple ways, it's very important error message...
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Project file not found: %s"), *ProjPath);
				UE_LOG(LogInit, Display, TEXT("Project file not found: %s"), *ProjPath);
				UE_LOG(LogInit, Display, TEXT("\tAttempting to find via project info helper."));
				// Use the uprojectdirs
				FString GameProjectFile = FUProjectDictionary::GetDefault().GetRelativeProjectPathForGame(FApp::GetProjectName(), FPlatformProcess::BaseDir());
				if (GameProjectFile.IsEmpty() == false)
				{
					UE_LOG(LogInit, Display, TEXT("\tFound project file %s."), *GameProjectFile);
					FPaths::SetProjectFilePath(GameProjectFile);

					// Fixup command line if project file wasn't found in specified directory to properly parse next arguments.
					FString OldCommandLine = FString(FCommandLine::Get());
					OldCommandLine.ReplaceInline(*GameProjectFilePathUnnormalized, *GameProjectFile, ESearchCase::CaseSensitive);
					FCommandLine::Set(*OldCommandLine);
					CmdLine = FCommandLine::Get();
				}
			}
		}
	}

	// allow the command line to override the platform file singleton
	bool bFileOverrideFound = false;
	if (LaunchCheckForFileOverride(CmdLine, bFileOverrideFound) == false)
	{
		// if it failed, we cannot continue
		return 1;
	}

	// Initialize file manager
	IFileManager::Get().ProcessCommandLineOptions();

	if( GIsGameAgnosticExe )
	{
		// If we launched without a project file, but with a game name that is incomplete, warn about the improper use of a Game suffix
		if ( LaunchHasIncompleteGameName() )
		{
			// We did not find a non-suffixed folder and we DID find the suffixed one.
			// The engine MUST be launched with <GameName>Game.
			const FText GameNameText = FText::FromString(FApp::GetProjectName());
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format( LOCTEXT("RequiresGamePrefix", "Error: UE4Editor does not append 'Game' to the passed in game name.\nYou must use the full name.\nYou specified '{0}', use '{0}Game'."), GameNameText ) );
			return 1;
		}
	}

	// remember thread id of the main thread
	GGameThreadId = FPlatformTLS::GetCurrentThreadId();
	GIsGameThreadIdInitialized = true;

	FPlatformProcess::SetThreadAffinityMask(FPlatformAffinity::GetMainGameMask());
	FPlatformProcess::SetupGameThread();

	// Figure out whether we're the editor, ucc or the game.
	const SIZE_T CommandLineSize = FCString::Strlen(CmdLine)+1;
	TCHAR* CommandLineCopy			= new TCHAR[ CommandLineSize ];
	FCString::Strcpy( CommandLineCopy, CommandLineSize, CmdLine );
	const TCHAR* ParsedCmdLine = CommandLineCopy;

	FString Token				= FParse::Token( ParsedCmdLine, 0);

#if WITH_ENGINE
	// Add the default engine shader dir
	FGenericPlatformProcess::AddShaderSourceDirectoryMapping(TEXT("/Engine"), FGenericPlatformProcess::ShaderDir());

	TArray<FString> Tokens;
	TArray<FString> Switches;
	UCommandlet::ParseCommandLine(CommandLineCopy, Tokens, Switches);

	bool bHasCommandletToken = false;

	for( int32 TokenIndex = 0; TokenIndex < Tokens.Num(); ++TokenIndex )
	{
		if( Tokens[TokenIndex].EndsWith(TEXT("Commandlet")) )
		{
			bHasCommandletToken = true;
			Token = Tokens[TokenIndex];
			break;
		}
	}

	for( int32 SwitchIndex = 0; SwitchIndex < Switches.Num() && !bHasCommandletToken; ++SwitchIndex )
	{
		if( Switches[SwitchIndex].StartsWith(TEXT("RUN=")) )
		{
			bHasCommandletToken = true;
			Token = Switches[SwitchIndex];
			break;
		}
	}

	if (bHasCommandletToken)
	{
		// will be reset later once the commandlet class loaded
		PRIVATE_GIsRunningCommandlet = true;
	}

#endif // WITH_ENGINE


	// trim any whitespace at edges of string - this can happen if the token was quoted with leading or trailing whitespace
	// VC++ tends to do this in its "external tools" config
	Token.TrimStartAndEndInline();

	// Path returned by FPaths::GetProjectFilePath() is normalized, so may have symlinks and ~ resolved and may differ from the original path to .uproject passed in the command line
	FString NormalizedToken = Token;
	FPaths::NormalizeFilename(NormalizedToken);

	const bool bFirstTokenIsGameName = (FApp::HasProjectName() && Token == FApp::GetProjectName());
	const bool bFirstTokenIsGameProjectFilePath = (FPaths::IsProjectFilePathSet() && NormalizedToken == FPaths::GetProjectFilePath());
	const bool bFirstTokenIsGameProjectFileShortName = (FPaths::IsProjectFilePathSet() && Token == FPaths::GetCleanFilename(FPaths::GetProjectFilePath()));

	if (bFirstTokenIsGameName || bFirstTokenIsGameProjectFilePath || bFirstTokenIsGameProjectFileShortName)
	{
		// first item on command line was the game name, remove it in all cases
		FString RemainingCommandline = ParsedCmdLine;
		FCString::Strcpy( CommandLineCopy, CommandLineSize, *RemainingCommandline );
		ParsedCmdLine = CommandLineCopy;

		// Set a new command-line that doesn't include the game name as the first argument
		FCommandLine::Set(ParsedCmdLine);

		Token = FParse::Token( ParsedCmdLine, 0);
		Token.TrimStartInline();

		// if the next token is a project file, then we skip it (which can happen on some platforms that combine
		// commandlines... this handles extra .uprojects, but if you run with MyGame MyGame, we can't tell if
		// the second MyGame is a map or not)
		while (FPaths::GetExtension(Token) == FProjectDescriptor::GetExtension())
		{
			Token = FParse::Token(ParsedCmdLine, 0);
			Token.TrimStartInline();
		}

		if (bFirstTokenIsGameProjectFilePath || bFirstTokenIsGameProjectFileShortName)
		{
			// Convert it to relative if possible...
			FString RelativeGameProjectFilePath = FFileManagerGeneric::DefaultConvertToRelativePath(*FPaths::GetProjectFilePath());
			if (RelativeGameProjectFilePath != FPaths::GetProjectFilePath())
			{
				FPaths::SetProjectFilePath(RelativeGameProjectFilePath);
			}
		}
	}

	// look early for the editor token
	bool bHasEditorToken = false;

#if UE_EDITOR
	// Check each token for '-game', '-server' or '-run='
	bool bIsNotEditor = false;

	// This isn't necessarily pretty, but many requests have been made to allow
	//   UE4Editor.exe <GAMENAME> -game <map>
	// or
	//   UE4Editor.exe <GAMENAME> -game 127.0.0.0
	// We don't want to remove the -game from the commandline just yet in case
	// we need it for something later. So, just move it to the end for now...
	const bool bFirstTokenIsGame = (Token == TEXT("-GAME"));
	const bool bFirstTokenIsServer = (Token == TEXT("-SERVER"));
	const bool bFirstTokenIsModeOverride = bFirstTokenIsGame || bFirstTokenIsServer || bHasCommandletToken;
	const TCHAR* CommandletCommandLine = nullptr;
	if (bFirstTokenIsModeOverride)
	{
		bIsNotEditor = true;
		if (bFirstTokenIsGame || bFirstTokenIsServer)
		{
			// Move the token to the end of the list...
			FString RemainingCommandline = ParsedCmdLine;
			RemainingCommandline.TrimStartInline();
			RemainingCommandline += FString::Printf(TEXT(" %s"), *Token);
			FCommandLine::Set(*RemainingCommandline);
		}
		if (bHasCommandletToken)
		{
#if STATS
			// Leave the stats enabled.
			if (!FStats::EnabledForCommandlet())
			{
				FThreadStats::MasterDisableForever();
			}
#endif
			if (Token.StartsWith(TEXT("run=")))
			{
				Token = Token.RightChop(4);
				if (!Token.EndsWith(TEXT("Commandlet")))
				{
					Token += TEXT("Commandlet");
				}
			}
			CommandletCommandLine = ParsedCmdLine;
		}
	}

	if (bHasCommandletToken)
	{
		// will be reset later once the commandlet class loaded
		PRIVATE_GIsRunningCommandlet = true;
	}

	if( !bIsNotEditor && GIsGameAgnosticExe )
	{
		// If we launched without a game name or project name, try to load the most recently loaded project file.
		// We can not do this if we are using a FilePlatform override since the game directory may already be established.
		const bool bIsBuildMachine = FParse::Param(FCommandLine::Get(), TEXT("BUILDMACHINE"));
		const bool bLoadMostRecentProjectFileIfItExists = !FApp::HasProjectName() && !bFileOverrideFound && !bIsBuildMachine && !FParse::Param( CmdLine, TEXT("norecentproject") );
		if (bLoadMostRecentProjectFileIfItExists )
		{
			LaunchUpdateMostRecentProjectFile();
		}
	}

	FString CheckToken = Token;
	bool bFoundValidToken = false;
	while (!bFoundValidToken && (CheckToken.Len() > 0))
	{
		if (!bIsNotEditor)
		{
			bool bHasNonEditorToken = (CheckToken == TEXT("-GAME")) || (CheckToken == TEXT("-SERVER")) || (CheckToken.StartsWith(TEXT("RUN="))) || CheckToken.EndsWith(TEXT("Commandlet"));
			if (bHasNonEditorToken)
			{
				bIsNotEditor = true;
				bFoundValidToken = true;
			}
		}

		CheckToken = FParse::Token(ParsedCmdLine, 0);
	}

	bHasEditorToken = !bIsNotEditor;
#elif WITH_ENGINE
	const TCHAR* CommandletCommandLine = nullptr;
	if (bHasCommandletToken)
	{
#if STATS
		// Leave the stats enabled.
		if (!FStats::EnabledForCommandlet())
		{
			FThreadStats::MasterDisableForever();
		}
#endif
		if (Token.StartsWith(TEXT("run=")))
		{
			Token = Token.RightChop(4);
			if (!Token.EndsWith(TEXT("Commandlet")))
			{
				Token += TEXT("Commandlet");
			}
		}
		CommandletCommandLine = ParsedCmdLine;
	}
#if WITH_EDITOR && WITH_EDITORONLY_DATA
	// If a non-editor target build w/ WITH_EDITOR and WITH_EDITORONLY_DATA, use the old token check...
	//@todo. Is this something we need to support?
	bHasEditorToken = Token == TEXT("EDITOR");
#else
	// Game, server and commandlets never set the editor token
	bHasEditorToken = false;
#endif
#endif	//UE_EDITOR

#if !UE_BUILD_SHIPPING
	// Benchmarking.
	FApp::SetBenchmarking(FParse::Param(FCommandLine::Get(),TEXT("BENCHMARK")));
#else
	FApp::SetBenchmarking(false);
#endif // !UE_BUILD_SHIPPING

	// "-Deterministic" is a shortcut for "-UseFixedTimeStep -FixedSeed"
	bool bDeterministic = FParse::Param(FCommandLine::Get(), TEXT("Deterministic"));

#if PLATFORM_HTML5
	bool bUseFixedTimeStep = false;
	GConfig->GetBool(TEXT("/Script/HTML5PlatformEditor.HTML5TargetSettings"), TEXT("UseFixedTimeStep"), bUseFixedTimeStep, GEngineIni);
	FApp::SetUseFixedTimeStep(bUseFixedTimeStep);
#else
	FApp::SetUseFixedTimeStep(bDeterministic || FParse::Param(FCommandLine::Get(), TEXT("UseFixedTimeStep")));
#endif

	FApp::bUseFixedSeed = bDeterministic || FApp::IsBenchmarking() || FParse::Param(FCommandLine::Get(),TEXT("FixedSeed"));

	// Initialize random number generator.
	{
		uint32 Seed1 = 0;
		uint32 Seed2 = 0;

		if(!FApp::bUseFixedSeed)
		{
			Seed1 = FPlatformTime::Cycles();
			Seed2 = FPlatformTime::Cycles();
		}

		FMath::RandInit(Seed1);
		FMath::SRandInit(Seed2);

		UE_LOG(LogInit, Display, TEXT("RandInit(%d) SRandInit(%d)."), Seed1, Seed2);
	}

	// Set up the module list and version information, if it's not compiled-in
#if !IS_MONOLITHIC || BUILT_FROM_CHANGELIST == 0
	static FBuildVersion Version;
	if(FBuildVersion::TryRead(FBuildVersion::GetFileNameForCurrentExecutable(), Version))
	{
		UE_LOG(LogInit, Log, TEXT("Read version file with build ID '%s'"), *Version.BuildId);
		if(Version.Changelist != 0)
		{
			int32 EffectiveCompatibleChangelist = (Version.CompatibleChangelist != 0)? Version.CompatibleChangelist : Version.Changelist;
			UE_LOG(LogInit, Log, TEXT("Overriding engine version to CL %d, compatible CL %d"), Version.Changelist, EffectiveCompatibleChangelist);
			FEngineVersion::OverrideCurrentVersionChangelist(Version.Changelist, EffectiveCompatibleChangelist);
		}

#if !IS_MONOLITHIC
		if(Version.BuildId.Len() > 0)
		{
			static FModuleEnumerator ModuleEnumerator(Version.BuildId);
			if(ModuleEnumerator.RegisterWithModuleManager())
			{
				UE_LOG(LogInit, Log, TEXT("Registered custom module enumerator with build ID '%s'"), *Version.BuildId);
			}
		}
#endif
	}
#endif

#if !IS_PROGRAM
	if ( !GIsGameAgnosticExe && FApp::HasProjectName() && !FPaths::IsProjectFilePathSet() )
	{
		// If we are using a non-agnostic exe where a name was specified but we did not specify a project path. Assemble one based on the game name.
		const FString ProjectFilePath = FPaths::Combine(*FPaths::ProjectDir(), *FString::Printf(TEXT("%s.%s"), FApp::GetProjectName(), *FProjectDescriptor::GetExtension()));
		FPaths::SetProjectFilePath(ProjectFilePath);
	}
#endif

	// Now verify the project file if we have one
	if (FPaths::IsProjectFilePathSet()
#if IS_PROGRAM
		// Programs don't need uproject files to exist, but some do specify them and if they exist we should load them
		&& FPaths::FileExists(FPaths::GetProjectFilePath())
#endif
		)
	{
		if (!IProjectManager::Get().LoadProjectFile(FPaths::GetProjectFilePath()))
		{
			// The project file was invalid or saved with a newer version of the engine. Exit.
			UE_LOG(LogInit, Warning, TEXT("Could not find a valid project file, the engine will exit now."));
			return 1;
		}

		if (IProjectManager::Get().GetCurrentProject() && IProjectManager::Get().GetCurrentProject()->bIsEnterpriseProject && FPaths::DirectoryExists(FPaths::EnterpriseDir()))
		{
			// Add the enterprise binaries directory if we're an enterprise project
			FModuleManager::Get().AddBinariesDirectory( *FPaths::Combine( FPaths::EnterpriseDir(), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory() ), false );
		}
	}

#if !IS_PROGRAM
	if( FApp::HasProjectName() )
	{
		// Tell the module manager what the game binaries folder is
		const FString ProjectBinariesDirectory = FPaths::Combine( FPlatformMisc::ProjectDir(), TEXT( "Binaries" ), FPlatformProcess::GetBinariesSubdirectory() );
		FPlatformProcess::AddDllDirectory(*ProjectBinariesDirectory);
		FModuleManager::Get().SetGameBinariesDirectory(*ProjectBinariesDirectory);

		LaunchFixGameNameCase();
	}
#endif

	// initialize task graph sub-system with potential multiple threads
	FTaskGraphInterface::Startup( FPlatformMisc::NumberOfCores() );
	FTaskGraphInterface::Get().AttachToThread( ENamedThreads::GameThread );

#if STATS
	FThreadStats::StartThread();
#endif

	FScopeCycleCounter CycleCount_AfterStats( GET_STATID( STAT_FEngineLoop_PreInit_AfterStats ) );

	// Load Core modules required for everything else to work (needs to be loaded before InitializeRenderingCVarsCaching)
	if (!LoadCoreModules())
	{
		UE_LOG(LogInit, Error, TEXT("Failed to load Core modules."));
		return 1;
	}

#if WITH_ENGINE
	extern ENGINE_API void InitializeRenderingCVarsCaching();
	InitializeRenderingCVarsCaching();
#endif

	bool bTokenDoesNotHaveDash = Token.Len() && FCString::Strnicmp(*Token, TEXT("-"), 1) != 0;

#if WITH_EDITOR
	// If we're running as an game but don't have a project, inform the user and exit.
	if (bHasEditorToken == false && bHasCommandletToken == false)
	{
		if ( !FPaths::IsProjectFilePathSet() )
		{
			//@todo this is too early to localize
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("Engine", "UE4RequiresProjectFiles", "UE4 games require a project file as the first parameter."));
			return 1;
		}
	}

	if (GIsUCCMakeStandaloneHeaderGenerator)
	{
		// Rebuilding script requires some hacks in the engine so we flag that.
		PRIVATE_GIsRunningCommandlet = true;
	}
#endif //WITH_EDITOR

	if (FPlatformProcess::SupportsMultithreading())
	{
		{
			GThreadPool = FQueuedThreadPool::Allocate();
			int32 NumThreadsInThreadPool = FPlatformMisc::NumberOfWorkerThreadsToSpawn();

			// we are only going to give dedicated servers one pool thread
			if (FPlatformProperties::IsServerOnly())
			{
				NumThreadsInThreadPool = 1;
			}
			verify(GThreadPool->Create(NumThreadsInThreadPool, 128 * 1024));
		}

#if WITH_EDITOR
		// when we are in the editor we like to do things like build lighting and such
		// this thread pool can be used for those purposes
		GLargeThreadPool = FQueuedThreadPool::Allocate();
		int32 NumThreadsInLargeThreadPool = FMath::Max(FPlatformMisc::NumberOfCoresIncludingHyperthreads() - 2, 2);

		verify(GLargeThreadPool->Create(NumThreadsInLargeThreadPool, 128 * 1024));
#endif
	}

	// Get a pointer to the log output device
	GLogConsole = GScopedLogConsole.Get();

	LoadPreInitModules();

#if WITH_ENGINE && CSV_PROFILER
	FCsvProfiler::Get()->Init();
#endif

	// Start the application
	if(!AppInit())
	{
		return 1;
	}

#if WITH_COREUOBJECT
	FPlatformFileManager::Get().InitializeNewAsyncIO();
#endif

	if (FPlatformProcess::SupportsMultithreading())
	{
		{
			GIOThreadPool = FQueuedThreadPool::Allocate();
			int32 NumThreadsInThreadPool = FPlatformMisc::NumberOfIOWorkerThreadsToSpawn();
			if (FPlatformProperties::IsServerOnly())
			{
				NumThreadsInThreadPool = 2;
			}
			verify(GIOThreadPool->Create(NumThreadsInThreadPool, 64 * 1024, TPri_AboveNormal));
		}
	}

#if WITH_ENGINE
	// Initialize system settings before anyone tries to use it...
	GSystemSettings.Initialize( bHasEditorToken );

	// Apply renderer settings from console variables stored in the INI.
	ApplyCVarSettingsFromIni(TEXT("/Script/Engine.RendererSettings"),*GEngineIni, ECVF_SetByProjectSetting);
	ApplyCVarSettingsFromIni(TEXT("/Script/Engine.RendererOverrideSettings"), *GEngineIni, ECVF_SetByProjectSetting);
	ApplyCVarSettingsFromIni(TEXT("/Script/Engine.StreamingSettings"), *GEngineIni, ECVF_SetByProjectSetting);
	ApplyCVarSettingsFromIni(TEXT("/Script/Engine.GarbageCollectionSettings"), *GEngineIni, ECVF_SetByProjectSetting);
	ApplyCVarSettingsFromIni(TEXT("/Script/Engine.NetworkSettings"), *GEngineIni, ECVF_SetByProjectSetting);

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		if (!IsRunningCommandlet())
		{
			// Note: It is critical that resolution settings are loaded before the movie starts playing so that the window size and fullscreen state is known
			UGameUserSettings::PreloadResolutionSettings();
		}
	}
#endif

	// As early as possible to avoid expensive re-init of subsystems,
	// after SystemSettings.ini file loading so we get the right state,
	// before ConsoleVariables.ini so the local developer can always override.
	// before InitializeCVarsForActiveDeviceProfile() so the platform can override user settings
	Scalability::LoadState((bHasEditorToken && !GEditorSettingsIni.IsEmpty()) ? GEditorSettingsIni : GGameUserSettingsIni);

	// Set all CVars which have been setup in the device profiles.
	UDeviceProfileManager::InitializeCVarsForActiveDeviceProfile();

	if (FApp::ShouldUseThreadingForPerformance() && FPlatformMisc::AllowRenderThread()
#if ENABLE_LOW_LEVEL_MEM_TRACKER
		// disable rendering thread when LLM is on so that memory is attributer better
		&& !FLowLevelMemTracker::Get().ShouldReduceThreads()
#endif
		)
	{
		GUseThreadedRendering = true;
	}
#endif

	FConfigCacheIni::LoadConsoleVariablesFromINI();

	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Platform Initialization"), STAT_PlatformInit, STATGROUP_LoadTime);

		// platform specific initialization now that the SystemSettings are loaded
		FPlatformMisc::PlatformInit();
		FPlatformApplicationMisc::Init();
		FPlatformMemory::Init();
	}

	// Let LogConsole know what ini file it should use to save its setting on exit.
	// We can't use GGameIni inside log console because it's destroyed in the global
	// scoped pointer and at that moment GGameIni may already be gone.
	if( GLogConsole != nullptr )
	{
		GLogConsole->SetIniFilename(*GGameIni);
	}


#if CHECK_PUREVIRTUALS
	FMessageDialog::Open( EAppMsgType::Ok, *NSLOCTEXT("Engine", "Error_PureVirtualsEnabled", "The game cannot run with CHECK_PUREVIRTUALS enabled.  Please disable CHECK_PUREVIRTUALS and rebuild the executable.").ToString() );
	FPlatformMisc::RequestExit(false);
#endif

#if WITH_ENGINE
	// allow for game explorer processing (including parental controls) and firewalls installation
	if (!FPlatformMisc::CommandLineCommands())
	{
		FPlatformMisc::RequestExit(false);
	}

	bool bIsRegularClient = false;

	if (!bHasEditorToken)
	{
		// See whether the first token on the command line is a commandlet.

		//@hack: We need to set these before calling StaticLoadClass so all required data gets loaded for the commandlets.
		GIsClient = true;
		GIsServer = true;
#if WITH_EDITOR
		GIsEditor = true;
#endif	//WITH_EDITOR
		PRIVATE_GIsRunningCommandlet = true;

		// Allow commandlet rendering and/or audio based on command line switch (too early to let the commandlet itself override this).
		PRIVATE_GAllowCommandletRendering = FParse::Param(FCommandLine::Get(), TEXT("AllowCommandletRendering"));
		PRIVATE_GAllowCommandletAudio = FParse::Param(FCommandLine::Get(), TEXT("AllowCommandletAudio"));

		// We need to disregard the empty token as we try finding Token + "Commandlet" which would result in finding the
		// UCommandlet class if Token is empty.
		bool bDefinitelyCommandlet = (bTokenDoesNotHaveDash && Token.EndsWith(TEXT("Commandlet")));
		if (!bTokenDoesNotHaveDash)
		{
			if (Token.StartsWith(TEXT("run=")))
			{
				Token = Token.RightChop(4);
				bDefinitelyCommandlet = true;
				if (!Token.EndsWith(TEXT("Commandlet")))
				{
					Token += TEXT("Commandlet");
				}
			}
		}
		else
		{
			if (!bDefinitelyCommandlet)
			{
				UClass* TempCommandletClass = FindObject<UClass>(ANY_PACKAGE, *(Token+TEXT("Commandlet")), false);

				if (TempCommandletClass)
				{
					check(TempCommandletClass->IsChildOf(UCommandlet::StaticClass())); // ok so you have a class that ends with commandlet that is not a commandlet

					Token += TEXT("Commandlet");
					bDefinitelyCommandlet = true;
				}
			}
		}

		if (!bDefinitelyCommandlet)
		{
			bIsRegularClient = true;
			GIsClient = true;
			GIsServer = false;
#if WITH_EDITORONLY_DATA
			GIsEditor = false;
#endif
			PRIVATE_GIsRunningCommandlet = false;
		}
	}

	bool bDisableDisregardForGC = bHasEditorToken;
	if (IsRunningDedicatedServer())
	{
		GIsClient = false;
		GIsServer = true;
		PRIVATE_GIsRunningCommandlet = false;
#if WITH_EDITOR
		GIsEditor = false;
#endif
		bDisableDisregardForGC |= FPlatformProperties::RequiresCookedData() && (GUseDisregardForGCOnDedicatedServers == 0);
	}

	// If std out device hasn't been initialized yet (there was no -stdout param in the command line) and
	// we meet all the criteria, initialize it now.
	if (!GScopedStdOut && !bHasEditorToken && !bIsRegularClient && !IsRunningDedicatedServer())
	{
		InitializeStdOutDevice();
	}

	// allow the platform to start up any features it may need
	IPlatformFeaturesModule::Get();

	// Init physics engine before loading anything, in case we want to do things like cook during post-load.
	InitGamePhys();

	// Delete temporary files in cache.
	FPlatformProcess::CleanFileCache();

#if !UE_BUILD_SHIPPING
	GIsDemoMode = FParse::Param( FCommandLine::Get(), TEXT( "DEMOMODE" ) );
#endif

	if (bHasEditorToken)
	{
#if WITH_EDITOR

		// We're the editor.
		GIsClient = true;
		GIsServer = true;
		GIsEditor = true;
		PRIVATE_GIsRunningCommandlet = false;

		GWarn = &UnrealEdWarn;

#else
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("Engine", "EditorNotSupported", "Editor not supported in this mode."));
		FPlatformMisc::RequestExit(false);
		return 1;
#endif //WITH_EDITOR
	}

#endif // WITH_ENGINE
	// If we're not in the editor stop collecting the backlog now that we know
	if (!GIsEditor)
	{
		GLog->EnableBacklog( false );
	}
#if WITH_ENGINE

	EndInitTextLocalization();

	if (GIsEditor)
	{
		// High DPI must be enabled before any windows are shown.
		// only doing this in editor for now
		FPlatformMisc::SetHighDPIMode();
	}

	UStringTable::InitializeEngineBridge();

	if (FApp::ShouldUseThreadingForPerformance() && FPlatformMisc::AllowAudioThread())
	{
		bool bUseThreadedAudio = false;
		if (!GIsEditor)
		{
			GConfig->GetBool(TEXT("Audio"), TEXT("UseAudioThread"), bUseThreadedAudio, GEngineIni);
		}
		FAudioThread::SetUseThreadedAudio(bUseThreadedAudio);
	}

	if (FPlatformProcess::SupportsMultithreading() && !IsRunningDedicatedServer() && (bIsRegularClient || bHasEditorToken))
	{
		FPlatformSplash::Show();
	}

	if (!IsRunningDedicatedServer() && (bHasEditorToken || bIsRegularClient))
	{
		// Init platform application
		FSlateApplication::Create();
	}
	else
	{
		// If we're not creating the slate application there is some basic initialization
		// that it does that still must be done
		EKeys::Initialize();
		FCoreStyle::ResetToDefault();
	}

	if (GIsEditor)
	{
		// The editor makes use of all cultures in its UI, so pre-load the resource data now to avoid a hitch later
		FInternationalization::Get().LoadAllCultureData();
	}

	FScopedSlowTask SlowTask(100, NSLOCTEXT("EngineLoop", "EngineLoop_Initializing", "Initializing..."));

	SlowTask.EnterProgressFrame(10);

#if USE_LOCALIZED_PACKAGE_CACHE
	FPackageLocalizationManager::Get().InitializeFromLazyCallback([](FPackageLocalizationManager& InPackageLocalizationManager)
	{
		InPackageLocalizationManager.InitializeFromCache(MakeShareable(new FEnginePackageLocalizationCache()));
	});
#endif	// USE_LOCALIZED_PACKAGE_CACHE

	// Initialize the RHI.
	RHIInit(bHasEditorToken);

	if (FPlatformProperties::RequiresCookedData())
	{
		// Will open material shader code storage if project was packaged with it
		FShaderCodeLibrary::InitForRuntime(GMaxRHIShaderPlatform);
	}

	FShaderCache::LoadBinaryCache();

	InitGamePhysPostRHI();

	if (!FPlatformProperties::RequiresCookedData())
	{
		check(!GShaderCompilingManager);
		GShaderCompilingManager = new FShaderCompilingManager();

		check(!GDistanceFieldAsyncQueue);
		GDistanceFieldAsyncQueue = new FDistanceFieldAsyncQueue();
	}

	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Initial UObject load"), STAT_InitialUObjectLoad, STATGROUP_LoadTime);

		// Initialize shader types before loading any shaders
		InitializeShaderTypes();

		SlowTask.EnterProgressFrame(30);

		// Load the global shaders.
		// if (!IsRunningCommandlet())
		// hack: don't load global shaders if we are cooking we will load the shaders for the correct platform later
		FString Commandline = FCommandLine::Get();
		if (!IsRunningDedicatedServer() &&
			Commandline.Contains(TEXT("cookcommandlet")) == false &&
			Commandline.Contains(TEXT("run=cook")) == false )
		// if (FParse::Param(FCommandLine::Get(), TEXT("Multiprocess")) == false)
		{
			CompileGlobalShaderMap(false);
			if (GIsRequestingExit)
			{
				// This means we can't continue without the global shader map.
				return 1;
			}
		}
		else if (FPlatformProperties::RequiresCookedData() == false)
		{
			GetDerivedDataCacheRef();
		}

		CreateMoviePlayer();
		// If platforms support early movie playback we have to start the rendering thread much earlier
#if PLATFORM_SUPPORTS_EARLY_MOVIE_PLAYBACK
		PostInitRHI();

		if(GUseThreadedRendering)
		{
			if(GRHISupportsRHIThread)
			{
				const bool DefaultUseRHIThread = true;
				GUseRHIThread_InternalUseOnly = DefaultUseRHIThread;
				if(FParse::Param(FCommandLine::Get(), TEXT("rhithread")))
				{
					GUseRHIThread_InternalUseOnly = true;
				}
				else if(FParse::Param(FCommandLine::Get(), TEXT("norhithread")))
				{
					GUseRHIThread_InternalUseOnly = false;
				}
			}

			StartRenderingThread();
		}
#endif

#if !UE_SERVER// && !UE_EDITOR
		if(!IsRunningDedicatedServer() && !IsRunningCommandlet())
		{
			TSharedRef<FSlateRenderer> SlateRenderer = GUsingNullRHI ?
				FModuleManager::Get().LoadModuleChecked<ISlateNullRendererModule>("SlateNullRenderer").CreateSlateNullRenderer() :
				FModuleManager::Get().GetModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer").CreateSlateRHIRenderer();

			// If Slate is being used, initialize the renderer after RHIInit
			FSlateApplication& CurrentSlateApp = FSlateApplication::Get();
			CurrentSlateApp.InitializeRenderer(SlateRenderer);

			// Create the engine font services now that the Slate renderer is ready
			FEngineFontServices::Create();

			// allow the movie player to load a sequence from the .inis (a PreLoadingScreen module could have already initialized a sequence, in which case
			// it wouldn't have anything in it's .ini file)
			GetMoviePlayer()->SetupLoadingScreenFromIni();

			if (GetMoviePlayer()->HasEarlyStartupMovie())
			{
				GetMoviePlayer()->Initialize(SlateRenderer.Get());

				// hide splash screen now
				FPlatformMisc::PlatformHandleSplashScreen(false);

				// only allowed to play any movies marked as early startup.  These movies or widgets can have no interaction whatsoever with uobjects or engine features
				GetMoviePlayer()->PlayEarlyStartupMovies();
			}
		}
		else if ( IsRunningCommandlet() )
		{
			// Create the engine font services now that the Slate renderer is ready
			FEngineFontServices::Create();
		}
#endif

		// In order to be able to use short script package names get all script
		// package names from ini files and register them with FPackageName system.
		FPackageName::RegisterShortPackageNamesForUObjectModules();

		SlowTask.EnterProgressFrame(5);


#if USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME
		// If we don't do this now and the async loading thread is active, then we will attempt to load this module from a thread
		if (GEventDrivenLoaderEnabled)
		{
			FModuleManager::Get().LoadModule("AssetRegistry");
		}
#endif

		// Make sure all UObject classes are registered and default properties have been initialized
		ProcessNewlyLoadedUObjects();
#if WITH_EDITOR
		if(FPIEPreviewDeviceProfileSelectorModule::IsRequestingPreviewDevice())
		{
			FPIEPreviewDeviceProfileSelectorModule* PIEPreviewDeviceProfileSelectorModule = FModuleManager::LoadModulePtr<FPIEPreviewDeviceProfileSelectorModule>("PIEPreviewDeviceProfileSelector");
			if (PIEPreviewDeviceProfileSelectorModule)
			{
				PIEPreviewDeviceProfileSelectorModule->ApplyPreviewDeviceState();
			}
		}
#endif
#if USE_LOCALIZED_PACKAGE_CACHE
		// CoreUObject is definitely available now, so make sure the package localization cache is available
		// This may have already been initialized from the CDO creation from ProcessNewlyLoadedUObjects
		FPackageLocalizationManager::Get().PerformLazyInitialization();
#endif	// USE_LOCALIZED_PACKAGE_CACHE

		// Default materials may have been loaded due to dependencies when loading
		// classes and class default objects. If not, do so now.
		UMaterialInterface::InitDefaultMaterials();
		UMaterialInterface::AssertDefaultMaterialsExist();
		UMaterialInterface::AssertDefaultMaterialsPostLoaded();
	}

	// Initialize the texture streaming system (needs to happen after RHIInit and ProcessNewlyLoadedUObjects).
	IStreamingManager::Get();

	SlowTask.EnterProgressFrame(5);

	// Tell the module manager is may now process newly-loaded UObjects when new C++ modules are loaded
	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	// Setup GC optimizations
	if (bDisableDisregardForGC)
	{
		GUObjectArray.DisableDisregardForGC();
	}

	SlowTask.EnterProgressFrame(10);

	if ( !LoadStartupCoreModules() )
	{
		// At least one startup module failed to load, return 1 to indicate an error
		return 1;
	}


	SlowTask.EnterProgressFrame(10);

	// Load up all modules that need to hook into the loading screen
	if (!IProjectManager::Get().LoadModulesForProject(ELoadingPhase::PreLoadingScreen) || !IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PreLoadingScreen))
	{
		return 1;
	}

#if !UE_SERVER
	if (!IsRunningDedicatedServer() && !IsRunningCommandlet() && !GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		if (FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer())
	{
			GetMoviePlayer()->Initialize(*Renderer);
		}
	}
#endif

		// do any post appInit processing, before the render thread is started.
	FPlatformApplicationMisc::PostInit();
	SlowTask.EnterProgressFrame(5);

#if !PLATFORM_SUPPORTS_EARLY_MOVIE_PLAYBACK
	PostInitRHI();

	if (GUseThreadedRendering)
	{
		if (GRHISupportsRHIThread)
		{
			const bool DefaultUseRHIThread = true;
			GUseRHIThread_InternalUseOnly = DefaultUseRHIThread;
			if (FParse::Param(FCommandLine::Get(),TEXT("rhithread")))
			{
				GUseRHIThread_InternalUseOnly = true;
			}
			else if (FParse::Param(FCommandLine::Get(),TEXT("norhithread")))
			{
				GUseRHIThread_InternalUseOnly = false;
			}
		}
		StartRenderingThread();
	}
#endif // !PLATFORM_SUPPORTS_EARLY_MOVIE_PLAYBACK


	// Playing a movie can only happen after the rendering thread is started.
#if !UE_SERVER// && !UE_EDITOR
	if (!IsRunningDedicatedServer() && !IsRunningCommandlet() && !GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		// Play any non-early startup loading movies.
		GetMoviePlayer()->PlayMovie();
	}
#endif

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		// show or hide splash screen based on movie
		FPlatformMisc::PlatformHandleSplashScreen(!GetMoviePlayer()->IsMovieCurrentlyPlaying());
	}
	else
#endif
	{
		// show splash screen
		FPlatformMisc::PlatformHandleSplashScreen(true);
	}

	{
		FCoreUObjectDelegates::PreGarbageCollectConditionalBeginDestroy.AddStatic(StartRenderCommandFenceBundler);
		FCoreUObjectDelegates::PostGarbageCollectConditionalBeginDestroy.AddStatic(StopRenderCommandFenceBundler);
	}

#if WITH_EDITOR
	// We need to mount the shared resources for templates (if there are any) before we try and load and game classes
	FUnrealEdMisc::Get().MountTemplateSharedPaths();
#endif

	if ( !LoadStartupModules() )
	{
		// At least one startup module failed to load, return 1 to indicate an error
		return 1;
	}

	// load up the seek-free startup packages
	if ( !FStartupPackages::LoadAll() )
	{
		// At least one startup package failed to load, return 1 to indicate an error
		return 1;
	}
#endif // WITH_ENGINE

#if WITH_COREUOBJECT
	if (GUObjectArray.IsOpenForDisregardForGC())
	{
		GUObjectArray.CloseDisregardForGC();
	}
	NotifyRegistrationComplete();
#endif // WITH_COREUOBJECT

#if WITH_ENGINE
	if (UOnlineEngineInterface::Get()->IsLoaded())
	{
		SetIsServerForOnlineSubsystemsDelegate(FQueryIsRunningServer::CreateStatic(&IsServerDelegateForOSS));
	}

	SlowTask.EnterProgressFrame(5);

	if (!bHasEditorToken)
	{
		UClass* CommandletClass = nullptr;

		if (!bIsRegularClient)
		{
			CommandletClass = FindObject<UClass>(ANY_PACKAGE,*Token,false);
			if (!CommandletClass)
			{
				if (GLogConsole && !GIsSilent)
				{
					GLogConsole->Show(true);
				}
				UE_LOG(LogInit, Error, TEXT("%s looked like a commandlet, but we could not find the class."), *Token);
				GIsRequestingExit = true;
				return 1;
			}

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
			extern bool GIsConsoleExecutable;
			if (GIsConsoleExecutable)
			{
				if (GLogConsole != nullptr && GLogConsole->IsAttached())
				{
					GLog->RemoveOutputDevice(GLogConsole);
				}
				// Setup Ctrl-C handler for console application
				FPlatformMisc::SetGracefulTerminationHandler();
			}
			else
#endif
			{
				// Bring up console unless we're a silent build.
				if( GLogConsole && !GIsSilent )
				{
					GLogConsole->Show( true );
				}
			}

			// print output immediately
			setvbuf(stdout, nullptr, _IONBF, 0);

			UE_LOG(LogInit, Log,  TEXT("Executing %s"), *CommandletClass->GetFullName() );

			// Allow commandlets to individually override those settings.
			UCommandlet* Default = CastChecked<UCommandlet>(CommandletClass->GetDefaultObject());

			if ( GIsRequestingExit )
			{
				// commandlet set GIsRequestingExit during construction
				return 1;
			}

			GIsClient = Default->IsClient;
			GIsServer = Default->IsServer;
#if WITH_EDITOR
			GIsEditor = Default->IsEditor;
#else
			if (Default->IsEditor)
			{
				UE_LOG(LogInit, Error, TEXT("Cannot run editor commandlet %s with game executable."), *CommandletClass->GetFullName());
				GIsRequestingExit = true;
				return 1;
			}
#endif
			PRIVATE_GIsRunningCommandlet = true;
			// Reset aux log if we don't want to log to the console window.
			if( !Default->LogToConsole )
			{
				GLog->RemoveOutputDevice( GLogConsole );
			}

			// allow the commandlet the opportunity to create a custom engine
			CommandletClass->GetDefaultObject<UCommandlet>()->CreateCustomEngine(CommandletCommandLine);
			if ( GEngine == nullptr )
			{
#if WITH_EDITOR
				if ( GIsEditor )
				{
					FString EditorEngineClassName;
					GConfig->GetString(TEXT("/Script/Engine.Engine"), TEXT("EditorEngine"), EditorEngineClassName, GEngineIni);
					UClass* EditorEngineClass = StaticLoadClass( UEditorEngine::StaticClass(), nullptr, *EditorEngineClassName);
					if (EditorEngineClass == nullptr)
					{
						UE_LOG(LogInit, Fatal, TEXT("Failed to load Editor Engine class '%s'."), *EditorEngineClassName);
					}

					GEngine = GEditor = NewObject<UEditorEngine>(GetTransientPackage(), EditorEngineClass);

					GEngine->ParseCommandline();

					UE_LOG(LogInit, Log, TEXT("Initializing Editor Engine..."));
					GEditor->InitEditor(this);
					UE_LOG(LogInit, Log, TEXT("Initializing Editor Engine Completed"));
				}
				else
#endif
				{
					FString GameEngineClassName;
					GConfig->GetString(TEXT("/Script/Engine.Engine"), TEXT("GameEngine"), GameEngineClassName, GEngineIni);

					UClass* EngineClass = StaticLoadClass( UEngine::StaticClass(), nullptr, *GameEngineClassName);

					if (EngineClass == nullptr)
					{
						UE_LOG(LogInit, Fatal, TEXT("Failed to load Engine class '%s'."), *GameEngineClassName);
					}

					// must do this here so that the engine object that we create on the next line receives the correct property values
					GEngine = NewObject<UEngine>(GetTransientPackage(), EngineClass);
					check(GEngine);

					GEngine->ParseCommandline();

					UE_LOG(LogInit, Log, TEXT("Initializing Game Engine..."));
					GEngine->Init(this);
					UE_LOG(LogInit, Log, TEXT("Initializing Game Engine Completed"));
				}
			}

			// Call init callbacks
			FCoreDelegates::OnPostEngineInit.Broadcast();

			// Load all the post-engine init modules
			ensure(IProjectManager::Get().LoadModulesForProject(ELoadingPhase::PostEngineInit));
			ensure(IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PostEngineInit));

			//run automation smoke tests now that the commandlet has had a chance to override the above flags and GEngine is available
			FAutomationTestFramework::Get().RunSmokeTests();

			UCommandlet* Commandlet = NewObject<UCommandlet>(GetTransientPackage(), CommandletClass);
			check(Commandlet);
			Commandlet->AddToRoot();

			// Execute the commandlet.
			double CommandletExecutionStartTime = FPlatformTime::Seconds();

			// Commandlets don't always handle -run= properly in the commandline so we'll provide them
			// with a custom version that doesn't have it.
			Commandlet->ParseParms( CommandletCommandLine );
#if	STATS
			// We have to close the scope, otherwise we will end with broken stats.
			CycleCount_AfterStats.StopAndResetStatId();
#endif // STATS
			FStats::TickCommandletStats();
			int32 ErrorLevel = Commandlet->Main( CommandletCommandLine );
			FStats::TickCommandletStats();

			GIsRequestingExit = true;

			// Log warning/ error summary.
			if( Commandlet->ShowErrorCount )
			{
				TArray<FString> AllErrors;
				TArray<FString> AllWarnings;
				GWarn->GetErrors(AllErrors);
				GWarn->GetWarnings(AllWarnings);

				if (AllErrors.Num() || AllWarnings.Num())
				{
					SET_WARN_COLOR(COLOR_WHITE);
					UE_LOG(LogInit, Display, TEXT(""));
					UE_LOG(LogInit, Display, TEXT("Warning/Error Summary (Unique only)"));
					UE_LOG(LogInit, Display, TEXT("-----------------------------------"));

					const int32 MaxMessagesToShow = (GIsBuildMachine || FParse::Param(FCommandLine::Get(), TEXT("DUMPALLWARNINGS"))) ?
						(AllErrors.Num() + AllWarnings.Num()) : 50;

					TSet<FString> ShownMessages;
					ShownMessages.Empty(MaxMessagesToShow);

					SET_WARN_COLOR(COLOR_RED);

					for (const FString& ErrorMessage : AllErrors)
					{
						bool bAlreadyShown = false;
						ShownMessages.Add(ErrorMessage, &bAlreadyShown);

						if (!bAlreadyShown)
						{
							if (ShownMessages.Num() > MaxMessagesToShow)
							{
								SET_WARN_COLOR(COLOR_WHITE);
								UE_CLOG(MaxMessagesToShow < AllErrors.Num(), LogInit, Display, TEXT("NOTE: Only first %d errors displayed."), MaxMessagesToShow);
								break;
							}

							UE_LOG(LogInit, Display, TEXT("%s"), *ErrorMessage);
						}
					}

					SET_WARN_COLOR(COLOR_YELLOW);
					ShownMessages.Empty(MaxMessagesToShow);

					for (const FString& WarningMessage : AllWarnings)
					{
						bool bAlreadyShown = false;
						ShownMessages.Add(WarningMessage, &bAlreadyShown);

						if (!bAlreadyShown)
						{
							if (ShownMessages.Num() > MaxMessagesToShow)
							{
								SET_WARN_COLOR(COLOR_WHITE);
								UE_CLOG(MaxMessagesToShow < AllWarnings.Num(), LogInit, Display, TEXT("NOTE: Only first %d warnings displayed."), MaxMessagesToShow);
								break;
							}

							UE_LOG(LogInit, Display, TEXT("%s"), *WarningMessage);
						}
					}
				}

				UE_LOG(LogInit, Display, TEXT(""));

				if( ErrorLevel != 0 )
				{
					SET_WARN_COLOR(COLOR_RED);
					UE_LOG(LogInit, Display, TEXT("Commandlet->Main return this error code: %d"), ErrorLevel );
					UE_LOG(LogInit, Display, TEXT("With %d error(s), %d warning(s)"), AllErrors.Num(), AllWarnings.Num() );
				}
				else if( ( AllErrors.Num() == 0 ) )
				{
					SET_WARN_COLOR(AllWarnings.Num() ? COLOR_YELLOW : COLOR_GREEN);
					UE_LOG(LogInit, Display, TEXT("Success - %d error(s), %d warning(s)"), AllErrors.Num(), AllWarnings.Num() );
				}
				else
				{
					SET_WARN_COLOR(COLOR_RED);
					UE_LOG(LogInit, Display, TEXT("Failure - %d error(s), %d warning(s)"), AllErrors.Num(), AllWarnings.Num() );
					ErrorLevel = 1;
				}
				CLEAR_WARN_COLOR();
			}
			else
			{
				UE_LOG(LogInit, Display, TEXT("Finished.") );
			}

			double CommandletExecutionTime = FPlatformTime::Seconds() - CommandletExecutionStartTime;
			UE_LOG(LogInit, Display, LINE_TERMINATOR TEXT( "Execution of commandlet took:  %.2f seconds"), CommandletExecutionTime );

			// We're ready to exit!
			return ErrorLevel;
		}
		else
		{
			// We're a regular client.
			check(bIsRegularClient);

			if (bTokenDoesNotHaveDash)
			{
				// here we give people a reasonable warning if they tried to use the short name of a commandlet
				UClass* TempCommandletClass = FindObject<UClass>(ANY_PACKAGE,*(Token+TEXT("Commandlet")),false);
				if (TempCommandletClass)
				{
					UE_LOG(LogInit, Fatal, TEXT("You probably meant to call a commandlet. Please use the full name %s."), *(Token+TEXT("Commandlet")));
				}
			}
		}
	}

	// exit if wanted.
	if( GIsRequestingExit )
	{
		if ( GEngine != nullptr )
		{
			GEngine->PreExit();
		}
		AppPreExit();
		// appExit is called outside guarded block.
		return 1;
	}

	FString MatineeName;

	if(FParse::Param(FCommandLine::Get(),TEXT("DUMPMOVIE")) || FParse::Value(FCommandLine::Get(), TEXT("-MATINEESSCAPTURE="), MatineeName))
	{
		// -1: remain on
		GIsDumpingMovie = -1;
	}

	// If dumping movie then we do NOT want on-screen messages
	GAreScreenMessagesEnabled = !GIsDumpingMovie && !GIsDemoMode;

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(),TEXT("NOSCREENMESSAGES")))
	{
		GAreScreenMessagesEnabled = false;
	}

	// Don't update INI files if benchmarking or -noini
	if( FApp::IsBenchmarking() || FParse::Param(FCommandLine::Get(),TEXT("NOINI")))
	{
		GConfig->Detach( GEngineIni );
		GConfig->Detach( GInputIni );
		GConfig->Detach( GGameIni );
		GConfig->Detach( GEditorIni );
	}
#endif // !UE_BUILD_SHIPPING

	delete [] CommandLineCopy;

	// initialize the pointer, as it is deleted before being assigned in the first frame
	PendingCleanupObjects = nullptr;

	// Initialize profile visualizers.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FModuleManager::Get().LoadModule(TEXT("TaskGraph"));
	if (FPlatformProcess::SupportsMultithreading())
	{
		FModuleManager::Get().LoadModule(TEXT("ProfilerService"));
		FModuleManager::Get().GetModuleChecked<IProfilerServiceModule>("ProfilerService").CreateProfilerServiceManager();
	}
#endif

	// Init HighRes screenshot system, unless running on server
	if (!IsRunningDedicatedServer())
	{
		GetHighResScreenshotConfig().Init();
	}

#else // WITH_ENGINE
	EndInitTextLocalization();
#if USE_LOCALIZED_PACKAGE_CACHE
	FPackageLocalizationManager::Get().InitializeFromDefaultCache();
#endif	// USE_LOCALIZED_PACKAGE_CACHE
	FPlatformApplicationMisc::PostInit();
#endif // WITH_ENGINE

	//run automation smoke tests now that everything is setup to run
	FAutomationTestFramework::Get().RunSmokeTests();

	// Note we still have 20% remaining on the slow task: this will be used by the Editor/Engine initialization next
	return 0;
}


bool FEngineLoop::LoadCoreModules()
{
	// Always attempt to load CoreUObject. It requires additional pre-init which is called from its module's StartupModule method.
#if WITH_COREUOBJECT
	return FModuleManager::Get().LoadModule(TEXT("CoreUObject")) != nullptr;
#else
	return true;
#endif
}


void FEngineLoop::LoadPreInitModules()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Loading PreInit Modules"), STAT_PreInitModules, STATGROUP_LoadTime);

	// GGetMapNameDelegate is initialized here
#if WITH_ENGINE
	FModuleManager::Get().LoadModule(TEXT("Engine"));

	FModuleManager::Get().LoadModule(TEXT("Renderer"));

	FModuleManager::Get().LoadModule(TEXT("AnimGraphRuntime"));

	FPlatformApplicationMisc::LoadPreInitModules();

#if !UE_SERVER
	if (!IsRunningDedicatedServer() )
	{
		if (!GUsingNullRHI)
		{
			// This needs to be loaded before InitializeShaderTypes is called
			FModuleManager::Get().LoadModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer");
		}
	}
#endif

	FModuleManager::Get().LoadModule(TEXT("Landscape"));

	// Initialize ShaderCore before loading or compiling any shaders,
	// But after Renderer and any other modules which implement shader types.
	FModuleManager::Get().LoadModule(TEXT("ShaderCore"));

#if WITH_EDITORONLY_DATA
	// Load the texture compressor module before any textures load. They may
	// compress asynchronously and that can lead to a race condition.
	FModuleManager::Get().LoadModule(TEXT("TextureCompressor"));
#endif

#endif // WITH_ENGINE

#if (WITH_EDITOR && !(UE_BUILD_SHIPPING || UE_BUILD_TEST))
	// Load audio editor module before engine class CDOs are loaded
	FModuleManager::Get().LoadModule(TEXT("AudioEditor"));
	FModuleManager::Get().LoadModule(TEXT("AnimationModifiers"));
#endif
}


#if WITH_ENGINE

bool FEngineLoop::LoadStartupCoreModules()
{
	FScopedSlowTask SlowTask(100);

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Loading Startup Modules"), STAT_StartupModules, STATGROUP_LoadTime);

	bool bSuccess = true;

	// Load all Runtime modules
	SlowTask.EnterProgressFrame(10);
	{
		FModuleManager::Get().LoadModule(TEXT("Core"));
		FModuleManager::Get().LoadModule(TEXT("Networking"));
	}

	SlowTask.EnterProgressFrame(10);
	FPlatformApplicationMisc::LoadStartupModules();

	// initialize messaging
	SlowTask.EnterProgressFrame(10);
	if (FPlatformProcess::SupportsMultithreading())
	{
		FModuleManager::LoadModuleChecked<IMessagingModule>("Messaging");
	}

	// Init Scene Reconstruction support
#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		FModuleManager::LoadModuleChecked<IMRMeshModule>("MRMesh");
	}
#endif

	SlowTask.EnterProgressFrame(10);
#if WITH_EDITOR
		FModuleManager::LoadModuleChecked<IEditorStyleModule>("EditorStyle");
#endif //WITH_EDITOR

	// Load UI modules
	SlowTask.EnterProgressFrame(10);
	if ( !IsRunningDedicatedServer() )
	{
		FModuleManager::Get().LoadModule("Slate");

#if !UE_BUILD_SHIPPING
		// Need to load up the SlateReflector module to initialize the WidgetSnapshotService
		FModuleManager::Get().LoadModule("SlateReflector");
#endif // !UE_BUILD_SHIPPING
	}

#if WITH_EDITOR
	// In dedicated server builds with the editor, we need to load UMG/UMGEditor for compiling blueprints.
	// UMG must be loaded for runtime and cooking.
	FModuleManager::Get().LoadModule("UMG");
#else
	if ( !IsRunningDedicatedServer() )
	{
		// UMG must be loaded for runtime and cooking.
		FModuleManager::Get().LoadModule("UMG");
	}
#endif //WITH_EDITOR

	// Load all Development modules
	SlowTask.EnterProgressFrame(20);
	if (!IsRunningDedicatedServer())
	{
#if WITH_UNREAL_DEVELOPER_TOOLS
		FModuleManager::Get().LoadModule("MessageLog");
		FModuleManager::Get().LoadModule("CollisionAnalyzer");
#endif	//WITH_UNREAL_DEVELOPER_TOOLS
	}

#if WITH_UNREAL_DEVELOPER_TOOLS
		FModuleManager::Get().LoadModule("FunctionalTesting");
#endif	//WITH_UNREAL_DEVELOPER_TOOLS

	SlowTask.EnterProgressFrame(30);
#if (WITH_EDITOR && !(UE_BUILD_SHIPPING || UE_BUILD_TEST))
	// HACK: load BT editor as early as possible for statically initialized assets (non cooked BT assets needs it)
	// cooking needs this module too
	FModuleManager::Get().LoadModule(TEXT("BehaviorTreeEditor"));

	// Ability tasks are based on GameplayTasks, so we need to make sure that module is loaded as well
	FModuleManager::Get().LoadModule(TEXT("GameplayTasksEditor"));

	IAudioEditorModule* AudioEditorModule = &FModuleManager::LoadModuleChecked<IAudioEditorModule>("AudioEditor");
	AudioEditorModule->RegisterAssetActions();

	// Load the StringTableEditor module to register its asset actions
	FModuleManager::Get().LoadModule("StringTableEditor");

	if( !IsRunningDedicatedServer() )
	{
		// VREditor needs to be loaded in non-server editor builds early, so engine content Blueprints can be loaded during DDC generation
		FModuleManager::Get().LoadModule(TEXT("VREditor"));
	}
	// -----------------------------------------------------

	// HACK: load EQS editor as early as possible for statically initialized assets (non cooked EQS assets needs it)
	// cooking needs this module too
	bool bEnvironmentQueryEditor = false;
	GConfig->GetBool(TEXT("EnvironmentQueryEd"), TEXT("EnableEnvironmentQueryEd"), bEnvironmentQueryEditor, GEngineIni);
	if (bEnvironmentQueryEditor
#if WITH_EDITOR
		|| GetDefault<UEditorExperimentalSettings>()->bEQSEditor
#endif // WITH_EDITOR
		)
	{
		FModuleManager::Get().LoadModule(TEXT("EnvironmentQueryEditor"));
	}

	// We need this for blueprint projects that have online functionality.
	//FModuleManager::Get().LoadModule(TEXT("OnlineBlueprintSupport"));

	if (IsRunningCommandlet())
	{
		FModuleManager::Get().LoadModule(TEXT("IntroTutorials"));
		FModuleManager::Get().LoadModule(TEXT("Blutility"));
	}

#endif //(WITH_EDITOR && !(UE_BUILD_SHIPPING || UE_BUILD_TEST))

#if WITH_ENGINE
	// Load runtime client modules (which are also needed at cook-time)
	if( !IsRunningDedicatedServer() )
	{
		FModuleManager::Get().LoadModule(TEXT("Overlay"));
	}

	FModuleManager::Get().LoadModule(TEXT("MediaAssets"));
#endif

	FModuleManager::Get().LoadModule(TEXT("ClothingSystemRuntime"));
#if WITH_EDITOR
	FModuleManager::Get().LoadModule(TEXT("ClothingSystemEditor"));
#endif

	FModuleManager::Get().LoadModule(TEXT("PacketHandler"));


	return bSuccess;
}


bool FEngineLoop::LoadStartupModules()
{
	FScopedSlowTask SlowTask(3);

	SlowTask.EnterProgressFrame(1);
	// Load any modules that want to be loaded before default modules are loaded up.
	if (!IProjectManager::Get().LoadModulesForProject(ELoadingPhase::PreDefault) || !IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PreDefault))
	{
		return false;
	}

	SlowTask.EnterProgressFrame(1);
	// Load modules that are configured to load in the default phase
	if (!IProjectManager::Get().LoadModulesForProject(ELoadingPhase::Default) || !IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::Default))
	{
		return false;
	}

	SlowTask.EnterProgressFrame(1);
	// Load any modules that want to be loaded after default modules are loaded up.
	if (!IProjectManager::Get().LoadModulesForProject(ELoadingPhase::PostDefault) || !IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PostDefault))
	{
		return false;
	}

	return true;
}


void FEngineLoop::InitTime()
{
	// Init variables used for benchmarking and ticking.
	FApp::SetCurrentTime(FPlatformTime::Seconds());
	MaxFrameCounter				= 0;
	MaxTickTime					= 0;
	TotalTickTime				= 0;
	LastFrameCycles				= FPlatformTime::Cycles();

	float FloatMaxTickTime		= 0;
#if !UE_BUILD_SHIPPING
	FParse::Value(FCommandLine::Get(),TEXT("SECONDS="),FloatMaxTickTime);
	MaxTickTime					= FloatMaxTickTime;

	// look of a version of seconds that only is applied if FApp::IsBenchmarking() is set. This makes it easier on
	// say, iOS, where we have a toggle setting to enable benchmarking, but don't want to have to make user
	// also disable the seconds setting as well. -seconds= will exit the app after time even if benchmarking
	// is not enabled
	// NOTE: This will override -seconds= if it's specified
	if (FApp::IsBenchmarking())
	{
		if (FParse::Value(FCommandLine::Get(),TEXT("BENCHMARKSECONDS="),FloatMaxTickTime) && FloatMaxTickTime)
		{
			MaxTickTime			= FloatMaxTickTime;
		}
	}

	// Use -FPS=X to override fixed tick rate if e.g. -BENCHMARK is used.
	float FixedFPS = 0;
	FParse::Value(FCommandLine::Get(),TEXT("FPS="),FixedFPS);
	if( FixedFPS > 0 )
	{
		FApp::SetFixedDeltaTime(1 / FixedFPS);
	}

#endif // !UE_BUILD_SHIPPING

	// convert FloatMaxTickTime into number of frames (using 1 / FApp::GetFixedDeltaTime() to convert fps to seconds )
	MaxFrameCounter = FMath::TruncToInt(MaxTickTime / FApp::GetFixedDeltaTime());
}


//called via FCoreDelegates::StarvedGameLoop
void GameLoopIsStarved()
{
	FlushPendingDeleteRHIResources_GameThread();
	FStats::AdvanceFrame( true, FStats::FOnAdvanceRenderingThreadStats::CreateStatic( &AdvanceRenderingThreadStatsGT ) );
}


int32 FEngineLoop::Init()
{
	LLM_SCOPE(ELLMTag::EngineInitMemory);

	CheckImageIntegrity();

	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FEngineLoop::Init" ), STAT_FEngineLoop_Init, STATGROUP_LoadTime );

	FScopedSlowTask SlowTask(100);
	SlowTask.EnterProgressFrame(10);

	// Figure out which UEngine variant to use.
	UClass* EngineClass = nullptr;
	if( !GIsEditor )
	{
		// We're the game.
		FString GameEngineClassName;
		GConfig->GetString(TEXT("/Script/Engine.Engine"), TEXT("GameEngine"), GameEngineClassName, GEngineIni);
		EngineClass = StaticLoadClass( UGameEngine::StaticClass(), nullptr, *GameEngineClassName);
		if (EngineClass == nullptr)
		{
			UE_LOG(LogInit, Fatal, TEXT("Failed to load UnrealEd Engine class '%s'."), *GameEngineClassName);
		}
		GEngine = NewObject<UEngine>(GetTransientPackage(), EngineClass);
	}
	else
	{
#if WITH_EDITOR
		// We're UnrealEd.
		FString UnrealEdEngineClassName;
		GConfig->GetString(TEXT("/Script/Engine.Engine"), TEXT("UnrealEdEngine"), UnrealEdEngineClassName, GEngineIni);
		EngineClass = StaticLoadClass(UUnrealEdEngine::StaticClass(), nullptr, *UnrealEdEngineClassName);
		if (EngineClass == nullptr)
		{
			UE_LOG(LogInit, Fatal, TEXT("Failed to load UnrealEd Engine class '%s'."), *UnrealEdEngineClassName);
		}
		GEngine = GEditor = GUnrealEd = NewObject<UUnrealEdEngine>(GetTransientPackage(), EngineClass);
#else
		check(0);
#endif
	}

	check( GEngine );

	GetMoviePlayer()->PassLoadingScreenWindowBackToGame();

	GEngine->ParseCommandline();

	InitTime();

	SlowTask.EnterProgressFrame(60);

	GEngine->Init(this);

	// Call init callbacks
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UEngine::OnPostEngineInit.Broadcast();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	FCoreDelegates::OnPostEngineInit.Broadcast();

	SlowTask.EnterProgressFrame(30);

	// initialize engine instance discovery
	if (FPlatformProcess::SupportsMultithreading())
	{
		if (!IsRunningCommandlet())
		{
			SessionService = FModuleManager::LoadModuleChecked<ISessionServicesModule>("SessionServices").GetSessionService();
			SessionService->Start();
		}

		EngineService = new FEngineService();
	}

	// Load all the post-engine init modules
	if (!IProjectManager::Get().LoadModulesForProject(ELoadingPhase::PostEngineInit) || !IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PostEngineInit))
	{
		GIsRequestingExit = true;
		return 1;
	}

	GEngine->Start();

	GetMoviePlayer()->WaitForMovieToFinish();

#if !UE_SERVER
	// initialize media framework
	IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

	if (MediaModule != nullptr)
	{
		MediaModule->SetTimeSource(MakeShareable(new FAppMediaTimeSource));
	}
#endif

	// initialize automation worker
#if WITH_AUTOMATION_WORKER
	FModuleManager::Get().LoadModule("AutomationWorker");
#endif

	// Automation tests can be invoked locally in non-editor builds configuration (e.g. performance profiling in Test configuration)
#if WITH_ENGINE && !UE_BUILD_SHIPPING
	FModuleManager::Get().LoadModule("AutomationController");
	FModuleManager::GetModuleChecked<IAutomationControllerModule>("AutomationController").Init();
#endif

#if WITH_EDITOR
	if (GIsEditor)
	{
		FModuleManager::Get().LoadModule(TEXT("ProfilerClient"));
	}

	FModuleManager::Get().LoadModule(TEXT("SequenceRecorder"));
	FModuleManager::Get().LoadModule(TEXT("SequenceRecorderSections"));
#endif

	GIsRunning = true;

	if (!GIsEditor)
	{
		// hide a couple frames worth of rendering
		FViewport::SetGameRenderingEnabled(true, 3);
	}

	// Begin the async platform hardware survey
	GEngine->StartHardwareSurvey();

	FCoreDelegates::StarvedGameLoop.BindStatic(&GameLoopIsStarved);

	// Ready to measure thread heartbeat
	FThreadHeartBeat::Get().Start();

	FCoreDelegates::OnFEngineLoopInitComplete.Broadcast();
	return 0;
}


void FEngineLoop::Exit()
{
	STAT_ADD_CUSTOMMESSAGE_NAME( STAT_NamedMarker, TEXT( "EngineLoop.Exit" ) );

	GIsRunning	= 0;
	GLogConsole	= nullptr;

	// shutdown visual logger and flush all data
#if ENABLE_VISUAL_LOG
	FVisualLogger::Get().Shutdown();
#endif


	// Make sure we're not in the middle of loading something.
	FlushAsyncLoading();

	// Block till all outstanding resource streaming requests are fulfilled.
	if (!IStreamingManager::HasShutdown())
	{
		UTexture2D::CancelPendingTextureStreaming();
		IStreamingManager::Get().BlockTillAllRequestsFinished();
	}

#if WITH_ENGINE
	// shut down messaging
	delete EngineService;
	EngineService = nullptr;

	if (SessionService.IsValid())
	{
		SessionService->Stop();
		SessionService.Reset();
	}

	if (GDistanceFieldAsyncQueue)
	{
		GDistanceFieldAsyncQueue->Shutdown();
		delete GDistanceFieldAsyncQueue;
	}
#endif // WITH_ENGINE

	if ( GEngine != nullptr )
	{
		GEngine->ShutdownAudioDeviceManager();
	}

	if ( GEngine != nullptr )
	{
		GEngine->PreExit();
	}

	// close all windows
	FSlateApplication::Shutdown();

#if !UE_SERVER
	if ( FEngineFontServices::IsInitialized() )
	{
		FEngineFontServices::Destroy();
	}
#endif

#if WITH_EDITOR
	// These module must be shut down first because other modules may try to access them during shutdown.
	// Accessing these modules at shutdown causes instability since the object system will have been shut down and these modules uses uobjects internally.
	FModuleManager::Get().UnloadModule("AssetTools", true);

#endif // WITH_EDITOR
	FModuleManager::Get().UnloadModule("AssetRegistry", true);

#if !PLATFORM_ANDROID 	// AppPreExit doesn't work on Android
	AppPreExit();

	TermGamePhys();
	ParticleVertexFactoryPool_FreePool();
#else
	// AppPreExit() stops malloc profiler, do it here instead
	MALLOC_PROFILER( GMalloc->Exec(nullptr, TEXT("MPROF STOP"), *GLog);	);
#endif // !ANDROID

	// Stop the rendering thread.
	StopRenderingThread();


	// Disable the shader cache
	FShaderCache::ShutdownShaderCache();

	// Close shader code map, if any
	FShaderCodeLibrary::Shutdown();

	// Tear down the RHI.
	RHIExitAndStopRHIThread();

#if !PLATFORM_ANDROID // UnloadModules doesn't work on Android
#if WITH_ENGINE
	// Save the hot reload state
	IHotReloadInterface* HotReload = IHotReloadInterface::GetPtr();
	if(HotReload != nullptr)
	{
		HotReload->SaveConfig();
	}
#endif

	// Unload all modules.  Note that this doesn't actually unload the module DLLs (that happens at
	// process exit by the OS), but it does call ShutdownModule() on all loaded modules in the reverse
	// order they were loaded in, so that systems can unregister and perform general clean up.
	FModuleManager::Get().UnloadModulesAtShutdown();
#endif // !ANDROID

	DestroyMoviePlayer();

	// Move earlier?
#if STATS
	FThreadStats::StopThread();
#endif

	FTaskGraphInterface::Shutdown();
	IStreamingManager::Shutdown();

	FPlatformMisc::ShutdownTaggedStorage();
}


void FEngineLoop::ProcessLocalPlayerSlateOperations() const
{
	FSlateApplication& SlateApp = FSlateApplication::Get();

	// For all the game worlds drill down to the player controller for each game viewport and process it's slate operation
	for ( const FWorldContext& Context : GEngine->GetWorldContexts() )
	{
		UWorld* CurWorld = Context.World();
		if ( CurWorld && CurWorld->IsGameWorld() )
		{
			UGameViewportClient* GameViewportClient = CurWorld->GetGameViewport();
			TSharedPtr< SViewport > ViewportWidget = GameViewportClient ? GameViewportClient->GetGameViewportWidget() : nullptr;

			if ( ViewportWidget.IsValid() )
			{
				FWidgetPath PathToWidget;
				SlateApp.GeneratePathToWidgetUnchecked(ViewportWidget.ToSharedRef(), PathToWidget);

				if (PathToWidget.IsValid())
				{
					for (FConstPlayerControllerIterator Iterator = CurWorld->GetPlayerControllerIterator(); Iterator; ++Iterator)
					{
						APlayerController* PlayerController = Iterator->Get();
						if (PlayerController)
						{
							ULocalPlayer* LocalPlayer = Cast< ULocalPlayer >(PlayerController->Player);
							if (LocalPlayer)
							{
								FReply& TheReply = LocalPlayer->GetSlateOperations();
								SlateApp.ProcessReply(PathToWidget, TheReply, nullptr, nullptr, LocalPlayer->GetControllerId());

								TheReply = FReply::Unhandled();
							}
						}
					}
				}
			}
		}
	}
}


bool FEngineLoop::ShouldUseIdleMode() const
{
	static const auto CVarIdleWhenNotForeground = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("t.IdleWhenNotForeground"));
	bool bIdleMode = false;

	// Yield cpu usage if desired
	if (FApp::IsGame()
		&& FPlatformProperties::SupportsWindowedMode()
		&& CVarIdleWhenNotForeground->GetValueOnGameThread()
		&& !FPlatformApplicationMisc::IsThisApplicationForeground())
	{
		bIdleMode = true;

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (!Context.World()->AreAlwaysLoadedLevelsLoaded())
			{
				bIdleMode = false;
				break;
			}
		}
	}

	return bIdleMode;
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST && MALLOC_GT_HOOKS

#include "Containers/StackTracker.h"
static TAutoConsoleVariable<int32> CVarLogGameThreadMallocChurn(
	TEXT("LogGameThreadMallocChurn.Enable"),
	0,
	TEXT("If > 0, then collect sample game thread malloc, realloc and free, periodically print a report of the worst offenders."));

static TAutoConsoleVariable<int32> CVarLogGameThreadMallocChurn_PrintFrequency(
	TEXT("LogGameThreadMallocChurn.PrintFrequency"),
	300,
	TEXT("Number of frames between churn reports."));

static TAutoConsoleVariable<int32> CVarLogGameThreadMallocChurn_Threshhold(
	TEXT("LogGameThreadMallocChurn.Threshhold"),
	10,
	TEXT("Minimum average number of allocs per frame to include in the report."));

static TAutoConsoleVariable<int32> CVarLogGameThreadMallocChurn_SampleFrequency(
	TEXT("LogGameThreadMallocChurn.SampleFrequency"),
	100,
	TEXT("Number of allocs to skip between samples. This is used to prevent churn sampling from slowing the game down too much."));

static TAutoConsoleVariable<int32> CVarLogGameThreadMallocChurn_StackIgnore(
	TEXT("LogGameThreadMallocChurn.StackIgnore"),
	2,
	TEXT("Number of items to discard from the top of a stack frame."));

static TAutoConsoleVariable<int32> CVarLogGameThreadMallocChurn_RemoveAliases(
	TEXT("LogGameThreadMallocChurn.RemoveAliases"),
	1,
	TEXT("If > 0 then remove aliases from the counting process. This essentialy merges addresses that have the same human readable string. It is slower."));

static TAutoConsoleVariable<int32> CVarLogGameThreadMallocChurn_StackLen(
	TEXT("LogGameThreadMallocChurn.StackLen"),
	3,
	TEXT("Maximum number of stack frame items to keep. This improves aggregation because calls that originate from multiple places but end up in the same place will be accounted together."));


extern CORE_API TFunction<void(int32)>* GGameThreadMallocHook;

struct FScopedSampleMallocChurn
{
	static FStackTracker GGameThreadMallocChurnTracker;
	static uint64 DumpFrame;

	bool bEnabled;
	int32 CountDown;
	TFunction<void(int32)> Hook;

	FScopedSampleMallocChurn()
		: bEnabled(CVarLogGameThreadMallocChurn.GetValueOnGameThread() > 0)
		, CountDown(CVarLogGameThreadMallocChurn_SampleFrequency.GetValueOnGameThread())
		, Hook(
		[this](int32 Index)
	{
		if (--CountDown <= 0)
		{
			CountDown = CVarLogGameThreadMallocChurn_SampleFrequency.GetValueOnGameThread();
			CollectSample();
		}
	}
	)
	{
		if (bEnabled)
		{
			check(IsInGameThread());
			check(!GGameThreadMallocHook);
			if (!DumpFrame)
			{
				DumpFrame = GFrameCounter + CVarLogGameThreadMallocChurn_PrintFrequency.GetValueOnGameThread();
				GGameThreadMallocChurnTracker.ResetTracking();
			}
			GGameThreadMallocChurnTracker.ToggleTracking(true, true);
			GGameThreadMallocHook = &Hook;
		}
		else
		{
			check(IsInGameThread());
			GGameThreadMallocChurnTracker.ToggleTracking(false, true);
			if (DumpFrame)
			{
				DumpFrame = 0;
				GGameThreadMallocChurnTracker.ResetTracking();
			}
		}
	}
	~FScopedSampleMallocChurn()
	{
		if (bEnabled)
		{
			check(IsInGameThread());
			check(GGameThreadMallocHook == &Hook);
			GGameThreadMallocHook = nullptr;
			GGameThreadMallocChurnTracker.ToggleTracking(false, true);
			check(DumpFrame);
			if (GFrameCounter > DumpFrame)
			{
				PrintResultsAndReset();
			}
		}
	}

	void CollectSample()
	{
		check(IsInGameThread());
		GGameThreadMallocChurnTracker.CaptureStackTrace(CVarLogGameThreadMallocChurn_StackIgnore.GetValueOnGameThread(), nullptr, CVarLogGameThreadMallocChurn_StackLen.GetValueOnGameThread(), CVarLogGameThreadMallocChurn_RemoveAliases.GetValueOnGameThread() > 0);
	}
	void PrintResultsAndReset()
	{
		DumpFrame = GFrameCounter + CVarLogGameThreadMallocChurn_PrintFrequency.GetValueOnGameThread();
		FOutputDeviceRedirector* Log = FOutputDeviceRedirector::Get();
		float SampleAndFrameCorrection = float(CVarLogGameThreadMallocChurn_SampleFrequency.GetValueOnGameThread()) / float(CVarLogGameThreadMallocChurn_PrintFrequency.GetValueOnGameThread());
		GGameThreadMallocChurnTracker.DumpStackTraces(CVarLogGameThreadMallocChurn_Threshhold.GetValueOnGameThread(), *Log, SampleAndFrameCorrection);
		GGameThreadMallocChurnTracker.ResetTracking();
	}
};
FStackTracker FScopedSampleMallocChurn::GGameThreadMallocChurnTracker;
uint64 FScopedSampleMallocChurn::DumpFrame = 0;

#endif

void FEngineLoop::Tick()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST && MALLOC_GT_HOOKS
	FScopedSampleMallocChurn ChurnTracker;
#endif
	// let the low level mem tracker pump once a frame to update states
	LLM(FLowLevelMemTracker::Get().UpdateStatsPerFrame());

	LLM_SCOPE(ELLMTag::EngineMisc);

	// Send a heartbeat for the diagnostics thread
	FThreadHeartBeat::Get().HeartBeat();

	// Make sure something is ticking the rendering tickables in -onethread mode to avoid leaks/bugs.
	if (!GUseThreadedRendering && !GIsRenderingThreadSuspended)
	{
		TickRenderingTickables();
	}

	// Ensure we aren't starting a frame while loading or playing a loading movie
	ensure(GetMoviePlayer()->IsLoadingFinished() && !GetMoviePlayer()->IsMovieCurrentlyPlaying());

	FExternalProfiler* ActiveProfiler = FActiveExternalProfilerBase::GetActiveProfiler();
	if (ActiveProfiler)
	{
		ActiveProfiler->FrameSync();
	}

	SCOPED_NAMED_EVENT(FEngineLoopTick, FColor::Red);

	// execute callbacks for cvar changes
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_Tick_CallAllConsoleVariableSinks);
		IConsoleManager::Get().CallAllConsoleVariableSinks();
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_FrameTime);

		#if WITH_PROFILEGPU
			// Issue the measurement of the execution time of a basic LongGPUTask unit on the very first frame
			// The results will be retrived on the first call of IssueScalableLongGPUTask
			if (GFrameCounter == 0 && IsFeatureLevelSupported(GMaxRHIShaderPlatform, ERHIFeatureLevel::SM4) && FApp::CanEverRender())
			{
				FlushRenderingCommands();

				ENQUEUE_UNIQUE_RENDER_COMMAND(
					MeasureLongGPUTaskExecutionTimeCmd,
					{
						MeasureLongGPUTaskExecutionTime(RHICmdList);
					});
			}
		#endif

		// beginning of RHI frame
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			BeginFrame,
		{
			GRHICommandList.LatchBypass();
			GFrameNumberRenderThread++;

			// If we are profiling, kick off a long GPU task to make the GPU always behind the CPU so that we
			// won't get GPU idle time measured in profiling results
			if (GTriggerGPUProfile && !GTriggerGPUHitchProfile)
			{
				IssueScalableLongGPUTask(RHICmdList);
			}

			RHICmdList.PushEvent(*FString::Printf(TEXT("Frame%d"),GFrameNumberRenderThread), FColor(0, 255, 0, 255));
			GPU_STATS_BEGINFRAME(RHICmdList);
			RHICmdList.BeginFrame();
		});

		FCoreDelegates::OnBeginFrame.Broadcast();

		// flush debug output which has been buffered by other threads
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_FlushThreadedLogs);
			GLog->FlushThreadedLogs();
		}

		// exit if frame limit is reached in benchmark mode, or if time limit is reached
		if ((FApp::IsBenchmarking() && MaxFrameCounter && (GFrameCounter > MaxFrameCounter)) ||
			(MaxTickTime && (TotalTickTime > MaxTickTime)))
		{
			FPlatformMisc::RequestExit(0);
		}

		// set FApp::CurrentTime, FApp::DeltaTime and potentially wait to enforce max tick rate
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_UpdateTimeAndHandleMaxTickRate);
			GEngine->UpdateTimeAndHandleMaxTickRate();
		}

		// tick performance monitoring
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_TickFPSChart);
			GEngine->TickPerformanceMonitoring( FApp::GetDeltaTime() );
		}

		// update memory allocator stats
		{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_Malloc_UpdateStats);
		GMalloc->UpdateStats();
		}
	}

	FStats::AdvanceFrame( false, FStats::FOnAdvanceRenderingThreadStats::CreateStatic( &AdvanceRenderingThreadStatsGT ) );

	{
		SCOPE_CYCLE_COUNTER( STAT_FrameTime );

		// Calculates average FPS/MS (outside STATS on purpose)
		CalculateFPSTimings();

		// Note the start of a new frame
		MALLOC_PROFILER(GMalloc->Exec(nullptr, *FString::Printf(TEXT("SNAPSHOTMEMORYFRAME")),*GLog));

		// handle some per-frame tasks on the rendering thread
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			ResetDeferredUpdates,
			{
				FDeferredUpdateResource::ResetNeedsUpdate();
				FlushPendingDeleteRHIResources_RenderThread();
			});

		{
			SCOPE_CYCLE_COUNTER(STAT_PumpMessages);
			FPlatformApplicationMisc::PumpMessages(true);
		}

		bool bIdleMode;
		{

			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_Idle);

			// Idle mode prevents ticking and rendering completely
			bIdleMode = ShouldUseIdleMode();
			if (bIdleMode)
			{
				// Yield CPU time
				FPlatformProcess::Sleep(.1f);
			}
		}

		// @todo vreditor urgent: Temporary hack to allow world-to-meters to be set before
		// input is polled for motion controller devices each frame.
		extern ENGINE_API float GNewWorldToMetersScale;
		if( GNewWorldToMetersScale != 0.0f  )
		{
#if WITH_ENGINE
			UWorld* WorldToScale = GWorld;

#if WITH_EDITOR
			if( GIsEditor && GEditor->PlayWorld != nullptr && GEditor->bIsSimulatingInEditor )
			{
				WorldToScale = GEditor->PlayWorld;
			}
#endif //WITH_EDITOR

			if( WorldToScale != nullptr )
		{
				if( GNewWorldToMetersScale != WorldToScale->GetWorldSettings()->WorldToMeters )
			{
					WorldToScale->GetWorldSettings()->WorldToMeters = GNewWorldToMetersScale;
			}
		}

			GNewWorldToMetersScale = 0.0f;
		}
#endif //WITH_ENGINE

		// tick active platform files
		FPlatformFileManager::Get().TickActivePlatformFile();

		// process accumulated Slate input
		if (FSlateApplication::IsInitialized() && !bIdleMode)
		{
			SCOPE_TIME_GUARD(TEXT("SlateInput"));
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_Tick_SlateInput);
			LLM_SCOPE(ELLMTag::UI);

			FSlateApplication& SlateApp = FSlateApplication::Get();
			SlateApp.PollGameDeviceState();
			// Gives widgets a chance to process any accumulated input
			SlateApp.FinishedInputThisFrame();
		}

#if !UE_SERVER
		// tick media framework
		IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

		if (MediaModule != nullptr)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_MediaTickPreEngine);
			MediaModule->TickPreEngine();
		}
#endif

		// main game engine tick (world, game objects, etc.)
		GEngine->Tick(FApp::GetDeltaTime(), bIdleMode);

#if !UE_SERVER
		// tick media framework
		if (MediaModule != nullptr)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_MediaTickPostEngine);
			MediaModule->TickPostEngine();
		}
#endif
		
		// If a movie that is blocking the game thread has been playing,
		// wait for it to finish before we continue to tick or tick again
		// We do this right after GEngine->Tick() because that is where user code would initiate a load / movie.
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_WaitForMovieToFinish);
			GetMoviePlayer()->WaitForMovieToFinish();
		}

		if (GShaderCompilingManager)
		{
			// Process any asynchronous shader compile results that are ready, limit execution time
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_Tick_GShaderCompilingManager);
			GShaderCompilingManager->ProcessAsyncResults(true, false);
		}

		if (GDistanceFieldAsyncQueue)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_Tick_GDistanceFieldAsyncQueue);
			GDistanceFieldAsyncQueue->ProcessAsyncTasks();
		}

#if !UE_SERVER
		// tick media framework
		if (MediaModule != nullptr)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_MediaTickPreSlate);
			MediaModule->TickPreSlate();
		}
#endif

#if WITH_ENGINE
		// process concurrent Slate tasks
		FGraphEventRef ConcurrentTask;
		const bool bDoConcurrentSlateTick = GEngine->ShouldDoAsyncEndOfFrameTasks();

		if (bDoConcurrentSlateTick)
		{
			const float DeltaSeconds = FApp::GetDeltaTime();
			const UGameViewportClient* const GameViewport = GEngine->GameViewport;
			const UWorld* const GameViewportWorld = GameViewport ? GameViewport->GetWorld() : nullptr;
			UDemoNetDriver* const CurrentDemoNetDriver = GameViewportWorld ? GameViewportWorld->DemoNetDriver : nullptr;

			if (CurrentDemoNetDriver && CurrentDemoNetDriver->ShouldTickFlushAsyncEndOfFrame())
			{
			ConcurrentTask = TGraphTask<FExecuteConcurrentWithSlateTickTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(
					[CurrentDemoNetDriver, DeltaSeconds]() {
					if (CVarDoAsyncEndOfFrameTasksRandomize.GetValueOnAnyThread(true) > 0)
					{
						FPlatformProcess::Sleep(FMath::RandRange(0.0f, .003f)); // this shakes up the threading to find race conditions
					}

						if (CurrentDemoNetDriver != nullptr)
						{
							CurrentDemoNetDriver->TickFlushAsyncEndOfFrame(DeltaSeconds);
					}
					});
				}
		}
#endif

		// tick Slate application
		if (FSlateApplication::IsInitialized() && !bIdleMode)
		{
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_ProcessPlayerControllersSlateOperations);
				check(!IsRunningDedicatedServer());

				// Process slate operations accumulated in the world ticks.
				ProcessLocalPlayerSlateOperations();
			}

			FSlateApplication::Get().Tick();
		}

#if WITH_ENGINE
		if (ConcurrentTask.GetReference())
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_ConcurrentWithSlateTickTasks_Wait);
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(ConcurrentTask);
			ConcurrentTask = nullptr;
		}
#endif

#if STATS
		// Clear any stat group notifications we have pending just in case they weren't claimed during FSlateApplication::Get().Tick
		extern CORE_API void ClearPendingStatGroups();
		ClearPendingStatGroups();
#endif

#if WITH_EDITOR && !UE_BUILD_SHIPPING
		// tick automation controller (Editor only)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_Tick_AutomationController);
			static FName AutomationController("AutomationController");
			if (FModuleManager::Get().IsModuleLoaded(AutomationController))
			{
				FModuleManager::GetModuleChecked<IAutomationControllerModule>(AutomationController).Tick();
			}
		}
#endif

#if WITH_ENGINE && WITH_AUTOMATION_WORKER
		// tick automation worker
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_Tick_AutomationWorker);
			static const FName AutomationWorkerModuleName = TEXT("AutomationWorker");
			if (FModuleManager::Get().IsModuleLoaded(AutomationWorkerModuleName))
			{
				FModuleManager::GetModuleChecked<IAutomationWorkerModule>(AutomationWorkerModuleName).Tick();
			}
		}
#endif

		// tick render hardware interface
		{			
			SCOPE_CYCLE_COUNTER(STAT_RHITickTime);
			RHITick( FApp::GetDeltaTime() ); // Update RHI.
		}

		// Increment global frame counter. Once for each engine tick.
		GFrameCounter++;

		// Disregard first few ticks for total tick time as it includes loading and such.
		if (GFrameCounter > 6)
		{
			TotalTickTime += FApp::GetDeltaTime();
		}

		// Find the objects which need to be cleaned up the next frame.
		FPendingCleanupObjects* PreviousPendingCleanupObjects = PendingCleanupObjects;
		PendingCleanupObjects = GetPendingCleanupObjects();

		{
			SCOPE_CYCLE_COUNTER(STAT_FrameSyncTime);
			// this could be perhaps moved down to get greater parallelism
			// Sync game and render thread. Either total sync or allowing one frame lag.
			static FFrameEndSync FrameEndSync;
			static auto CVarAllowOneFrameThreadLag = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.OneFrameThreadLag"));
			FrameEndSync.Sync( CVarAllowOneFrameThreadLag->GetValueOnGameThread() != 0 );
		}

		// tick core ticker, threads & deferred commands
		{
			SCOPE_CYCLE_COUNTER(STAT_DeferredTickTime);
			// Delete the objects which were enqueued for deferred cleanup before the previous frame.
			delete PreviousPendingCleanupObjects;

#if WITH_COREUOBJECT
			DeleteLoaders(); // destroy all linkers pending delete
#endif

			FTicker::GetCoreTicker().Tick(FApp::GetDeltaTime());
			FThreadManager::Get().Tick();
			GEngine->TickDeferredCommands();		
		}

#if !UE_SERVER
		// tick media framework
		if (MediaModule != nullptr)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_MediaTickPostRender);
			MediaModule->TickPostRender();
		}
#endif

		FCoreDelegates::OnEndFrame.Broadcast();

		// end of RHI frame
		ENQUEUE_UNIQUE_RENDER_COMMAND(EndFrame,
		{
			RHICmdList.EndFrame();
			GPU_STATS_ENDFRAME(RHICmdList);
			RHICmdList.PopEvent();
		});

		// Set CPU utilization stats.
		const FCPUTime CPUTime = FPlatformTime::GetCPUTime();
		SET_FLOAT_STAT( STAT_CPUTimePct, CPUTime.CPUTimePct );
		SET_FLOAT_STAT( STAT_CPUTimePctRelative, CPUTime.CPUTimePctRelative );

		// Set the UObject count stat
#if UE_GC_TRACK_OBJ_AVAILABLE
		SET_DWORD_STAT(STAT_Hash_NumObjects, GUObjectArray.GetObjectArrayNumMinusAvailable());
#endif
	}
}


void FEngineLoop::ClearPendingCleanupObjects()
{
	delete PendingCleanupObjects;
	PendingCleanupObjects = nullptr;
}

#endif // WITH_ENGINE


static TAutoConsoleVariable<int32> CVarLogTimestamp(
	TEXT("log.Timestamp"),
	1,
	TEXT("Defines if time is included in each line in the log file and in what form. Layout: [time][frame mod 1000]\n")
	TEXT("  0 = Do not display log timestamps\n")
	TEXT("  1 = Log time stamps in UTC and frame time (default) e.g. [2015.11.25-21.28.50:803][376]\n")
	TEXT("  2 = Log timestamps in seconds elapsed since GStartTime e.g. [0130.29][420]")
	TEXT("  3 = Log timestamps in local time and frame time e.g. [2017.08.04-17.59.50:803][420]"),
	ECVF_Default);


static TAutoConsoleVariable<int32> CVarLogCategory(
	TEXT("log.Category"),
	1,
	TEXT("Defines if the categoy is included in each line in the log file and in what form.\n")
	TEXT("  0 = Do not log category\n")
	TEXT("  2 = Log the category (default)"),
	ECVF_Default);


// Gets called any time cvars change (on the main thread)
static void CVarLogSinkFunction()
{
	{
		// for debugging
		ELogTimes::Type OldGPrintLogTimes = GPrintLogTimes;

		int32 LogTimestampValue = CVarLogTimestamp.GetValueOnGameThread();

		// Note GPrintLogTimes can be used on multiple threads but it should be no issue to change it on the fly
		switch(LogTimestampValue)
		{
			default:
			case 0: GPrintLogTimes = ELogTimes::None; break;
			case 1: GPrintLogTimes = ELogTimes::UTC; break;
			case 2: GPrintLogTimes = ELogTimes::SinceGStartTime; break;
			case 3: GPrintLogTimes = ELogTimes::Local; break;
		}
	}

	{
		int32 LogCategoryValue = CVarLogCategory.GetValueOnGameThread();

		// Note GPrintLogCategory can be used on multiple threads but it should be no issue to change it on the fly
		GPrintLogCategory = LogCategoryValue != 0;
	}
}


FAutoConsoleVariableSink CVarLogSink(FConsoleCommandDelegate::CreateStatic(&CVarLogSinkFunction));

static void CheckForPrintTimesOverride()
{
	// Determine whether to override the default setting for including timestamps in the log.
	FString LogTimes;
	if (GConfig->GetString( TEXT( "LogFiles" ), TEXT( "LogTimes" ), LogTimes, GEngineIni ))
	{
		if (LogTimes == TEXT( "None" ))
		{
			CVarLogTimestamp->Set((int)ELogTimes::None, ECVF_SetBySystemSettingsIni);
		}
		else if (LogTimes == TEXT( "UTC" ))
		{
			CVarLogTimestamp->Set((int)ELogTimes::UTC, ECVF_SetBySystemSettingsIni);
		}
		else if (LogTimes == TEXT( "SinceStart" ))
		{
			CVarLogTimestamp->Set((int)ELogTimes::SinceGStartTime, ECVF_SetBySystemSettingsIni);
		}
		else if (LogTimes == TEXT( "Local" ))
		{
			CVarLogTimestamp->Set((int)ELogTimes::Local, ECVF_SetBySystemSettingsIni);
		}
		// Assume this is a bool for backward compatibility
		else if (FCString::ToBool( *LogTimes ))
		{
			CVarLogTimestamp->Set((int)ELogTimes::UTC, ECVF_SetBySystemSettingsIni);
		}
	}

	if (FParse::Param( FCommandLine::Get(), TEXT( "LOGTIMES" ) ))
	{
		CVarLogTimestamp->Set((int)ELogTimes::UTC, ECVF_SetByCommandline);
	}
	else if (FParse::Param( FCommandLine::Get(), TEXT( "UTCLOGTIMES" ) ))
	{
		CVarLogTimestamp->Set((int)ELogTimes::UTC, ECVF_SetByCommandline);
	}
	else if (FParse::Param( FCommandLine::Get(), TEXT( "NOLOGTIMES" ) ))
	{
		CVarLogTimestamp->Set((int)ELogTimes::None, ECVF_SetByCommandline);
	}
	else if (FParse::Param( FCommandLine::Get(), TEXT( "LOGTIMESINCESTART" ) ))
	{
		CVarLogTimestamp->Set((int)ELogTimes::SinceGStartTime, ECVF_SetByCommandline);
	}
	else if (FParse::Param( FCommandLine::Get(), TEXT( "LOCALLOGTIMES" ) ))
	{
		CVarLogTimestamp->Set((int)ELogTimes::Local, ECVF_SetByCommandline);
	}
}


/* FEngineLoop static interface
 *****************************************************************************/

bool FEngineLoop::AppInit( )
{
	// Output devices.
	GError = FPlatformApplicationMisc::GetErrorOutputDevice();
	GWarn = FPlatformApplicationMisc::GetFeedbackContext();

	BeginInitTextLocalization();

	// Avoiding potential exploits by not exposing command line overrides in the shipping games.
#if !UE_BUILD_SHIPPING && WITH_EDITORONLY_DATA
	FString CmdLineFile;

	if (FParse::Value(FCommandLine::Get(), TEXT("-CmdLineFile="), CmdLineFile))
	{
		if (CmdLineFile.EndsWith(TEXT(".txt")))
		{
			FString FileCmds;

			if (FFileHelper::LoadFileToString(FileCmds, *CmdLineFile))
			{
				FileCmds = FString(TEXT(" ")) + FileCmds.TrimStartAndEnd();

				if (FileCmds.Len() > 1)
				{
					UE_LOG(LogInit, Log, TEXT("Appending commandline from file:%s"), *FileCmds);

					FCommandLine::Append(*FileCmds);
				}
			}
			else
			{
				UE_LOG(LogInit, Warning, TEXT("Failed to load commandline file '%s'."), *CmdLineFile);
			}
		}
		else
		{
			UE_LOG(LogInit, Warning, TEXT("Can only load commandline files ending with .txt, can't load: %s"), *CmdLineFile);
		}
	}


	// 8192 is the maximum length of the command line on Windows XP.
	TCHAR CmdLineEnv[8192];

	// Retrieve additional command line arguments from environment variable.
	FPlatformMisc::GetEnvironmentVariable(TEXT("UE-CmdLineArgs"), CmdLineEnv,ARRAY_COUNT(CmdLineEnv));

	// Manually nullptr terminate just in case. The nullptr string is returned above in the error case so
	// we don't have to worry about that.
	CmdLineEnv[ARRAY_COUNT(CmdLineEnv)-1] = 0;
	FString Env = FString(CmdLineEnv).TrimStart();

	if (Env.Len())
	{
		// Append the command line environment after inserting a space as we can't set it in the
		// environment. Note that any code accessing GCmdLine before appInit obviously won't
		// respect the command line environment additions.
		FCommandLine::Append(TEXT(" -EnvAfterHere "));
		FCommandLine::Append(CmdLineEnv);
	}
#endif

	// Error history.
	FCString::Strcpy(GErrorHist, TEXT("Fatal error!" LINE_TERMINATOR LINE_TERMINATOR));

	// Platform specific pre-init.
	FPlatformMisc::PlatformPreInit();
	FPlatformApplicationMisc::PreInit();

	// Keep track of start time.
	GSystemStartTime = FDateTime::Now().ToString();

	// Switch into executable's directory.
	FPlatformProcess::SetCurrentWorkingDirectoryToBaseDir();

	// Now finish initializing the file manager after the command line is set up
	IFileManager::Get().ProcessCommandLineOptions();

	FPageAllocator::LatchProtectedMode();

	if (FParse::Param(FCommandLine::Get(), TEXT("purgatorymallocproxy")))
	{
		FMemory::EnablePurgatoryTests();
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("poisonmallocproxy")))
	{
		FMemory::EnablePoisonTests();
	}

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("BUILDMACHINE")))
	{
		GIsBuildMachine = true;
	}

	// If "-WaitForDebugger" was specified, halt startup and wait for a debugger to attach before continuing
	if( FParse::Param( FCommandLine::Get(), TEXT( "WaitForDebugger" ) ) )
	{
		while( !FPlatformMisc::IsDebuggerPresent() )
		{
			FPlatformProcess::Sleep( 0.1f );
		}
	}
#endif // !UE_BUILD_SHIPPING

#if PLATFORM_WINDOWS

	// make sure that the log directory exists
	IFileManager::Get().MakeDirectory( *FPaths::ProjectLogDir() );

	// update the mini dump filename now that we have enough info to point it to the log folder even in installed builds
	FCString::Strcpy(MiniDumpFilenameW, *IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FString::Printf(TEXT("%sunreal-v%i-%s.dmp"), *FPaths::ProjectLogDir(), FEngineVersion::Current().GetChangelist(), *FDateTime::Now().ToString())));
#endif

	// Init logging to disk
	FPlatformOutputDevices::SetupOutputDevices();

	// init config system
	FConfigCacheIni::InitializeConfigSystem();

	// Now that configs have been initialized, setup stack walking options
	FPlatformStackWalk::Init();

#if WITH_EDITOR
	FBlueprintSupport::InitializeCompilationManager();
#endif

	CheckForPrintTimesOverride();

	// Check whether the project or any of its plugins are missing or are out of date
#if UE_EDITOR && !IS_MONOLITHIC
	if(!GIsBuildMachine && FPaths::IsProjectFilePathSet() && IPluginManager::Get().AreRequiredPluginsAvailable())
	{
		const FProjectDescriptor* CurrentProject = IProjectManager::Get().GetCurrentProject();
		if(CurrentProject != nullptr && CurrentProject->Modules.Num() > 0)
		{
			bool bNeedCompile = false;
			GConfig->GetBool(TEXT("/Script/UnrealEd.EditorLoadingSavingSettings"), TEXT("bForceCompilationAtStartup"), bNeedCompile, GEditorPerProjectIni);
			if(FParse::Param(FCommandLine::Get(), TEXT("SKIPCOMPILE")) || FParse::Param(FCommandLine::Get(), TEXT("MULTIPROCESS")))
			{
				bNeedCompile = false;
			}
			if(!bNeedCompile)
			{
				// Check if any of the project or plugin modules are out of date, and the user wants to compile them.
				TArray<FString> IncompatibleFiles;
				IProjectManager::Get().CheckModuleCompatibility(IncompatibleFiles);
				IPluginManager::Get().CheckModuleCompatibility(IncompatibleFiles);

				if (IncompatibleFiles.Num() > 0)
				{
					// Log the modules which need to be rebuilt
					FString ModulesList = TEXT("The following modules are missing or built with a different engine version:\n\n");
					for (int Idx = 0; Idx < IncompatibleFiles.Num(); Idx++)
					{
						UE_LOG(LogInit, Warning, TEXT("Incompatible or missing module: %s"), *IncompatibleFiles[Idx]);
						ModulesList += IncompatibleFiles[Idx] + TEXT("\n");
					}
					ModulesList += TEXT("\nWould you like to rebuild them now?");

					// If we're running with -stdout, assume that we're a non interactive process and about to fail
					if (FApp::IsUnattended() || FParse::Param(FCommandLine::Get(), TEXT("stdout")))
					{
						return false;
					}

					// Ask whether to compile before continuing
					if (FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *ModulesList, *FString::Printf(TEXT("Missing %s Modules"), FApp::GetProjectName())) == EAppReturnType::No)
					{
						return false;
					}

					bNeedCompile = true;
				}
			}

			if(bNeedCompile)
			{
				// Try to compile it
				FFeedbackContext *Context = (FFeedbackContext*)FDesktopPlatformModule::Get()->GetNativeFeedbackContext();
				Context->BeginSlowTask(FText::FromString(TEXT("Starting build...")), true, true);
				bool bCompileResult = FDesktopPlatformModule::Get()->CompileGameProject(FPaths::RootDir(), FPaths::GetProjectFilePath(), Context);
				Context->EndSlowTask();

				// Get a list of modules which are still incompatible
				TArray<FString> StillIncompatibleFiles;
				IProjectManager::Get().CheckModuleCompatibility(StillIncompatibleFiles);
				IPluginManager::Get().CheckModuleCompatibility(StillIncompatibleFiles);

				if(!bCompileResult || StillIncompatibleFiles.Num() > 0)
				{
					for (int Idx = 0; Idx < StillIncompatibleFiles.Num(); Idx++)
					{
						UE_LOG(LogInit, Warning, TEXT("Still incompatible or missing module: %s"), *StillIncompatibleFiles[Idx]);
					}
					if (!FApp::IsUnattended())
					{
						FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *FString::Printf(TEXT("%s could not be compiled. Try rebuilding from source manually."), FApp::GetProjectName()), TEXT("Error"));
					}
					return false;
				}
			}
		}
	}
#endif

	// Put the command line and config info into the suppression system (before plugins start loading)
	FLogSuppressionInterface::Get().ProcessConfigAndCommandLine();

	// NOTE: This is the earliest place to init the online subsystems (via plugins)
	// Code needs GConfigFile to be valid
	// Must be after FThreadStats::StartThread();
	// Must be before Render/RHI subsystem D3DCreate() for platform services that need D3D hooks like Steam

	// Load "pre-init" plugin modules
	if (!IProjectManager::Get().LoadModulesForProject(ELoadingPhase::PostConfigInit) || !IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PostConfigInit))
	{
		return false;
	}

	// Register the callback that allows the text localization manager to load data for plugins
	FTextLocalizationManager::Get().GatherAdditionalLocResPathsCallback.AddLambda([](TArray<FString>& OutLocResPaths)
	{
		IPluginManager::Get().GetLocalizationPathsForEnabledPlugins(OutLocResPaths);
	});

	PreInitHMDDevice();

	// after the above has run we now have the REQUIRED set of engine .INIs  (all of the other .INIs)
	// that are gotten from .h files' config() are not requires and are dynamically loaded when the .u files are loaded

#if !UE_BUILD_SHIPPING
	// Prompt the user for remote debugging?
	bool bPromptForRemoteDebug = false;
	GConfig->GetBool(TEXT("Engine.ErrorHandling"), TEXT("bPromptForRemoteDebugging"), bPromptForRemoteDebug, GEngineIni);
	bool bPromptForRemoteDebugOnEnsure = false;
	GConfig->GetBool(TEXT("Engine.ErrorHandling"), TEXT("bPromptForRemoteDebugOnEnsure"), bPromptForRemoteDebugOnEnsure, GEngineIni);

	if (FParse::Param(FCommandLine::Get(), TEXT("PROMPTREMOTEDEBUG")))
	{
		bPromptForRemoteDebug = true;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("PROMPTREMOTEDEBUGENSURE")))
	{
		bPromptForRemoteDebug = true;
		bPromptForRemoteDebugOnEnsure = true;
	}

	FPlatformMisc::SetShouldPromptForRemoteDebugging(bPromptForRemoteDebug);
	FPlatformMisc::SetShouldPromptForRemoteDebugOnEnsure(bPromptForRemoteDebugOnEnsure);

	// Feedback context.
	if (FParse::Param(FCommandLine::Get(), TEXT("WARNINGSASERRORS")))
	{
		GWarn->TreatWarningsAsErrors = true;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("SILENT")))
	{
		GIsSilent = true;
	}

#endif // !UE_BUILD_SHIPPING

	// Show log if wanted.
	if (GLogConsole && FParse::Param(FCommandLine::Get(), TEXT("LOG")))
	{
		GLogConsole->Show(true);
	}

	//// Command line.
	UE_LOG(LogInit, Log, TEXT("Build: %s"), FApp::GetBuildVersion());
	UE_LOG(LogInit, Log, TEXT("Engine Version: %s"), *FEngineVersion::Current().ToString());
	UE_LOG(LogInit, Log, TEXT("Compatible Engine Version: %s"), *FEngineVersion::CompatibleWith().ToString());
	UE_LOG(LogInit, Log, TEXT("Net CL: %u"), FNetworkVersion::GetNetworkCompatibleChangelist());
	FDevVersionRegistration::DumpVersionsToLog();

#if PLATFORM_64BITS
	UE_LOG(LogInit, Log, TEXT("Compiled (64-bit): %s %s"), ANSI_TO_TCHAR(__DATE__), ANSI_TO_TCHAR(__TIME__));
#else
	UE_LOG(LogInit, Log, TEXT("Compiled (32-bit): %s %s"), ANSI_TO_TCHAR(__DATE__), ANSI_TO_TCHAR(__TIME__));
#endif

	// Print compiler version info
#if defined(__clang__)
	UE_LOG(LogInit, Log, TEXT("Compiled with Clang: %s"), ANSI_TO_TCHAR( __clang_version__ ) );
#elif defined(__INTEL_COMPILER)
	UE_LOG(LogInit, Log, TEXT("Compiled with ICL: %d"), __INTEL_COMPILER);
#elif defined( _MSC_VER )
	#ifndef __INTELLISENSE__	// Intellisense compiler doesn't support _MSC_FULL_VER
	{
		const FString VisualCPPVersion( FString::Printf( TEXT( "%d" ), _MSC_FULL_VER ) );
		const FString VisualCPPRevisionNumber( FString::Printf( TEXT( "%02d" ), _MSC_BUILD ) );
		UE_LOG(LogInit, Log, TEXT("Compiled with Visual C++: %s.%s.%s.%s"),
			*VisualCPPVersion.Mid( 0, 2 ), // Major version
			*VisualCPPVersion.Mid( 2, 2 ), // Minor version
			*VisualCPPVersion.Mid( 4 ),	// Build version
			*VisualCPPRevisionNumber	// Revision number
			);
	}
	#endif
#else
	UE_LOG(LogInit, Log, TEXT("Compiled with unrecognized C++ compiler") );
#endif

	UE_LOG(LogInit, Log, TEXT("Build Configuration: %s"), EBuildConfigurations::ToString(FApp::GetBuildConfiguration()));
	UE_LOG(LogInit, Log, TEXT("Branch Name: %s"), *FApp::GetBranchName() );
	UE_LOG(LogInit, Log, TEXT("Command line: %s"), FCommandLine::GetForLogging() );
	UE_LOG(LogInit, Log, TEXT("Base directory: %s"), FPlatformProcess::BaseDir() );
	//UE_LOG(LogInit, Log, TEXT("Character set: %s"), sizeof(TCHAR)==1 ? TEXT("ANSI") : TEXT("Unicode") );
	UE_LOG(LogInit, Log, TEXT("Installed Engine Build: %d"), FApp::IsEngineInstalled() ? 1 : 0);

	// if a logging build, clear out old log files
#if !NO_LOGGING
	FMaintenance::DeleteOldLogs();
#endif

#if !UE_BUILD_SHIPPING
	FApp::InitializeSession();
#endif

	// Checks.
	check(sizeof(uint8) == 1);
	check(sizeof(int8) == 1);
	check(sizeof(uint16) == 2);
	check(sizeof(uint32) == 4);
	check(sizeof(uint64) == 8);
	check(sizeof(ANSICHAR) == 1);

#if PLATFORM_TCHAR_IS_4_BYTES
	check(sizeof(TCHAR) == 4);
#else
	check(sizeof(TCHAR) == 2);
#endif

	check(sizeof(int16) == 2);
	check(sizeof(int32) == 4);
	check(sizeof(int64) == 8);
	check(sizeof(bool) == 1);
	check(sizeof(float) == 4);
	check(sizeof(double) == 8);

	// Init list of common colors.
	GColorList.CreateColorMap();

	bool bForceSmokeTests = false;
	GConfig->GetBool(TEXT("AutomationTesting"), TEXT("bForceSmokeTests"), bForceSmokeTests, GEngineIni);
	bForceSmokeTests |= FParse::Param(FCommandLine::Get(), TEXT("bForceSmokeTests"));
	FAutomationTestFramework::Get().SetForceSmokeTests(bForceSmokeTests);

	// Init other systems.
	FCoreDelegates::OnInit.Broadcast();
	return true;
}


void FEngineLoop::AppPreExit( )
{
	UE_LOG(LogExit, Log, TEXT("Preparing to exit.") );

	FCoreDelegates::OnPreExit.Broadcast();

	MALLOC_PROFILER( GMalloc->Exec(nullptr, TEXT("MPROF STOP"), *GLog);	);

#if WITH_ENGINE
	if (FString(FCommandLine::Get()).Contains(TEXT("CreatePak")) && GetDerivedDataCache())
	{
		// if we are creating a Pak, we need to make sure everything is done and written before we exit
		UE_LOG(LogInit, Display, TEXT("Closing DDC Pak File."));
		GetDerivedDataCacheRef().WaitForQuiescence(true);
	}
#endif

#if WITH_EDITOR
	FRemoteConfig::Flush();
#endif

	FCoreDelegates::OnExit.Broadcast();

#if WITH_EDITOR
	if (GLargeThreadPool != nullptr)
	{
		GLargeThreadPool->Destroy();
	}
#endif // WITH_EDITOR

	// Clean up the thread pool
	if (GThreadPool != nullptr)
	{
		GThreadPool->Destroy();
	}

	if (GIOThreadPool != nullptr)
	{
		GIOThreadPool->Destroy();
	}

#if WITH_ENGINE
	if ( GShaderCompilingManager )
	{
		GShaderCompilingManager->Shutdown();

		delete GShaderCompilingManager;
		GShaderCompilingManager = nullptr;
	}
#endif
}


void FEngineLoop::AppExit( )
{
#if !WITH_ENGINE
	// when compiled WITH_ENGINE, this will happen in FEngineLoop::Exit()
	FTaskGraphInterface::Shutdown();
#endif // WITH_ENGINE

	UE_LOG(LogExit, Log, TEXT("Exiting."));

	FPlatformApplicationMisc::TearDown();
	FPlatformMisc::PlatformTearDown();

	if (GConfig)
	{
		GConfig->Exit();
		delete GConfig;
		GConfig = nullptr;
	}

	if( GLog )
	{
		GLog->TearDown();
	}

	FInternationalization::TearDown();
}

void FEngineLoop::PostInitRHI()
{
#if WITH_ENGINE
	TArray<uint32> PixelFormatByteWidth;
	PixelFormatByteWidth.AddUninitialized(PF_MAX);
	for (int i = 0; i < PF_MAX; i++)
	{
		PixelFormatByteWidth[i] = GPixelFormats[i].BlockBytes;
	}
	RHIPostInit(PixelFormatByteWidth);
#endif
}

void FEngineLoop::PreInitHMDDevice()
{
#if WITH_ENGINE && !UE_SERVER
	if (!FParse::Param(FCommandLine::Get(), TEXT("nohmd")) && !FParse::Param(FCommandLine::Get(), TEXT("emulatestereo")))
	{
		// Get a list of modules that implement this feature
		FName Type = IHeadMountedDisplayModule::GetModularFeatureName();
		IModularFeatures& ModularFeatures = IModularFeatures::Get();
		TArray<IHeadMountedDisplayModule*> HMDModules = ModularFeatures.GetModularFeatureImplementations<IHeadMountedDisplayModule>(Type);

		// Check whether the user passed in an explicit HMD module on the command line
		FString ExplicitHMDName;
		bool bUseExplicitHMDName = FParse::Value(FCommandLine::Get(), TEXT("hmd="), ExplicitHMDName);

		// Iterate over modules, checking ExplicitHMDName and calling PreInit
		for (auto HMDModuleIt = HMDModules.CreateIterator(); HMDModuleIt; ++HMDModuleIt)
		{
			IHeadMountedDisplayModule* HMDModule = *HMDModuleIt;


			bool bUnregisterHMDModule = false;
			if (bUseExplicitHMDName)
			{
				TArray<FString> HMDAliases;
				HMDModule->GetModuleAliases(HMDAliases);
				HMDAliases.Add(HMDModule->GetModuleKeyName());

				bUnregisterHMDModule = true;
				for (const FString& HMDModuleName : HMDAliases)
				{
					if (ExplicitHMDName.Equals(HMDModuleName, ESearchCase::IgnoreCase))
					{
						bUnregisterHMDModule = false;
						break;
					}
				}
			}
			else
			{
				bUnregisterHMDModule = !HMDModule->PreInit();
			}

			if (bUnregisterHMDModule)
			{
				// Unregister modules which don't match ExplicitHMDName, or which fail PreInit
				ModularFeatures.UnregisterModularFeature(Type, HMDModule);
			}
		}
		// Note we do not disable or warn here if no HMD modules matched ExplicitHMDName, as not all HMD plugins have been loaded yet.
	}
#endif // #if WITH_ENGINE && !UE_SERVER
}


#undef LOCTEXT_NAMESPACE
