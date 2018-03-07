// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/App.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Runtime/Launch/Resources/Version.h"
#include "HAL/LowLevelMemTracker.h"

DEFINE_LOG_CATEGORY_STATIC(LogApp, Log, All);

/* FApp static initialization
 *****************************************************************************/

FGuid FApp::InstanceId = FGuid::NewGuid();
FGuid FApp::SessionId = FGuid::NewGuid();
FString FApp::SessionName = FString();
FString FApp::SessionOwner = FString();
TArray<FString> FApp::SessionUsers = TArray<FString>();
bool FApp::Standalone = true;
bool FApp::bIsBenchmarking = false;
bool FApp::bUseFixedSeed = false;
bool FApp::bUseFixedTimeStep = false;
double FApp::FixedDeltaTime = 1 / 30.0;
double FApp::CurrentTime = 0.0;
double FApp::LastTime = 0.0;
double FApp::DeltaTime = 1 / 30.0;
double FApp::IdleTime = 0.0;
float FApp::VolumeMultiplier = 1.0f;
float FApp::UnfocusedVolumeMultiplier = 0.0f;
bool FApp::bUseVRFocus = false;
bool FApp::bHasVRFocus = false;


/* FApp static interface
 *****************************************************************************/

FString FApp::GetBranchName()
{
	return FString(TEXT(BRANCH_NAME));
}

const TCHAR* FApp::GetBuildVersion()
{
	return BUILD_VERSION;
}

int32 FApp::GetEngineIsPromotedBuild()
{
	return ENGINE_IS_PROMOTED_BUILD;
}


FString FApp::GetEpicProductIdentifier()
{
	return FString(TEXT(EPIC_PRODUCT_IDENTIFIER));
}

const TCHAR * FApp::GetDeploymentName()
{
	static TCHAR StaticDeploymentName[64] = {0};
	static bool bHaveDeployment = false;

	if (!bHaveDeployment)
	{
		// use -epicapp value from the commandline. Default deployment is not captured by this,
		// but this may not be a problem as that would be the case only during the development
		FParse::Value(FCommandLine::Get(), TEXT("EPICAPP="), StaticDeploymentName, ARRAY_COUNT(StaticDeploymentName) - 1);
		bHaveDeployment = true;
	}

	return StaticDeploymentName;
}

EBuildConfigurations::Type FApp::GetBuildConfiguration()
{
#if UE_BUILD_DEBUG
	return EBuildConfigurations::Debug;

#elif UE_BUILD_DEVELOPMENT
	// Detect DebugGame using an extern variable in monolithic configurations, or a command line argument in modular configurations.
	#if IS_MONOLITHIC
		extern const bool GIsDebugGame;
		return GIsDebugGame? EBuildConfigurations::DebugGame : EBuildConfigurations::Development;
	#else
		return IsRunningDebug() ? EBuildConfigurations::DebugGame : EBuildConfigurations::Development;
	#endif

#elif UE_BUILD_SHIPPING
	return EBuildConfigurations::Shipping;

#elif UE_BUILD_TEST
	return EBuildConfigurations::Test;

#else
	return EBuildConfigurations::Unknown;
#endif
}

bool FApp::IsRunningDebug()
{
	static FString RunConfig;
	static const bool bHasRunConfig = FParse::Value(FCommandLine::Get(), TEXT("RunConfig="), RunConfig);
	static const bool bRunningDebug = FParse::Param(FCommandLine::Get(), TEXT("debug"))
	                                  || (bHasRunConfig && RunConfig.StartsWith(TEXT("Debug")));
	return bRunningDebug;
}

FString FApp::GetBuildDate()
{
	return FString(ANSI_TO_TCHAR(__DATE__));
}


void FApp::InitializeSession()
{
	// parse session details on command line
	FString InstanceIdString;

	if (FParse::Value(FCommandLine::Get(), TEXT("-InstanceId="), InstanceIdString))
	{
		if (!FGuid::Parse(InstanceIdString, InstanceId))
		{
			UE_LOG(LogInit, Warning, TEXT("Invalid InstanceId on command line: %s"), *InstanceIdString);
		}
	}

	if (!InstanceId.IsValid())
	{
		InstanceId = FGuid::NewGuid();
	}

	FString SessionIdString;

	if (FParse::Value(FCommandLine::Get(), TEXT("-SessionId="), SessionIdString))
	{
		if (FGuid::Parse(SessionIdString, SessionId))
		{
			Standalone = false;
		}
		else
		{
			UE_LOG(LogInit, Warning, TEXT("Invalid SessionId on command line: %s"), *SessionIdString);
		}
	}

	FParse::Value(FCommandLine::Get(), TEXT("-SessionName="), SessionName);

	if (!FParse::Value(FCommandLine::Get(), TEXT("-SessionOwner="), SessionOwner))
	{
		SessionOwner = FPlatformProcess::UserName(false);
	}
}


bool FApp::IsInstalled()
{
	static int32 InstalledState = -1;

	if (InstalledState == -1)
	{
#if UE_BUILD_SHIPPING && PLATFORM_DESKTOP && !UE_SERVER
		bool bIsInstalled = true;
#else
		bool bIsInstalled = false;
#endif

#if PLATFORM_DESKTOP
		FString InstalledProjectBuildFile = FPaths::RootDir() / TEXT("Engine/Build/InstalledProjectBuild.txt");
		FPaths::NormalizeFilename(InstalledProjectBuildFile);
		bIsInstalled |= IFileManager::Get().FileExists(*InstalledProjectBuildFile);
#endif

		// Allow commandline options to disable/enable installed engine behavior
		if (bIsInstalled)
		{
			bIsInstalled = !FParse::Param(FCommandLine::Get(), TEXT("NotInstalled"));
		}
		else
		{
			bIsInstalled = FParse::Param(FCommandLine::Get(), TEXT("Installed"));
		}
		InstalledState = bIsInstalled ? 1 : 0;
	}
	return InstalledState == 1;
}


bool FApp::IsEngineInstalled()
{
	static int32 EngineInstalledState = -1;

	if (EngineInstalledState == -1)
	{
		bool bIsInstalledEngine = IsInstalled();

#if PLATFORM_DESKTOP
		FString InstalledBuildFile = FPaths::RootDir() / TEXT("Engine/Build/InstalledBuild.txt");
		FPaths::NormalizeFilename(InstalledBuildFile);
		bIsInstalledEngine |= IFileManager::Get().FileExists(*InstalledBuildFile);
#endif

		// Allow commandline options to disable/enable installed engine behavior
		if (bIsInstalledEngine)
		{
			bIsInstalledEngine = !FParse::Param(FCommandLine::Get(), TEXT("NotInstalledEngine"));
		}
		else
		{
			bIsInstalledEngine = FParse::Param(FCommandLine::Get(), TEXT("InstalledEngine"));
		}
		EngineInstalledState = bIsInstalledEngine ? 1 : 0;
	}

	return EngineInstalledState == 1;
}

#if PLATFORM_WINDOWS && defined(__clang__)
bool FApp::IsUnattended() // @todo clang: Workaround for missing symbol export
{
	static bool bIsUnattended = FParse::Param(FCommandLine::Get(), TEXT("UNATTENDED"));
	return bIsUnattended || GIsAutomationTesting;
}
#endif

#if HAVE_RUNTIME_THREADING_SWITCHES
bool FApp::ShouldUseThreadingForPerformance()
{
	static bool OnlyOneThread = 
		FParse::Param(FCommandLine::Get(), TEXT("ONETHREAD")) ||
		IsRunningDedicatedServer() ||
		!FPlatformProcess::SupportsMultithreading() ||
		FPlatformMisc::NumberOfCores() < 2;
	return !OnlyOneThread;
}
#endif // HAVE_RUNTIME_THREADING_SWITCHES


static bool GUnfocusedVolumeMultiplierInitialised = false;
float FApp::GetUnfocusedVolumeMultiplier()
{
	if (!GUnfocusedVolumeMultiplierInitialised)
	{
		GUnfocusedVolumeMultiplierInitialised = true;
		GConfig->GetFloat(TEXT("Audio"), TEXT("UnfocusedVolumeMultiplier"), UnfocusedVolumeMultiplier, GEngineIni);
	}
	return UnfocusedVolumeMultiplier;
}

void FApp::SetUnfocusedVolumeMultiplier(float InVolumeMultiplier)
{
	UnfocusedVolumeMultiplier = InVolumeMultiplier;
	GUnfocusedVolumeMultiplierInitialised = true;
}

void FApp::SetUseVRFocus(bool bInUseVRFocus)
{
	UE_CLOG(bUseVRFocus != bInUseVRFocus, LogApp, Verbose, TEXT("UseVRFocus has changed to %d"), int(bInUseVRFocus));
	bUseVRFocus = bInUseVRFocus;
}

void FApp::SetHasVRFocus(bool bInHasVRFocus)
{
	UE_CLOG(bHasVRFocus != bInHasVRFocus, LogApp, Verbose, TEXT("HasVRFocus has changed to %d"), int(bInHasVRFocus));
	bHasVRFocus = bInHasVRFocus;
}
