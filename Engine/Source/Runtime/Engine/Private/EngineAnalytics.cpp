// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EngineAnalytics.h"
#include "Misc/Guid.h"
#include "Stats/Stats.h"
#include "Misc/ConfigCacheIni.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Misc/EngineBuildSettings.h"
#include "AnalyticsBuildType.h"
#include "AnalyticsEventAttribute.h"
#include "IAnalyticsProviderET.h"
#include "AnalyticsET.h"
#include "GeneralProjectSettings.h"
#include "EngineSessionManager.h"
#include "Misc/EngineVersion.h"
#include "RHI.h"

bool FEngineAnalytics::bIsInitialized;
bool FEngineAnalytics::bIsEditorRun;
bool FEngineAnalytics::bIsGameRun;
TSharedPtr<IAnalyticsProviderET> FEngineAnalytics::Analytics;
TSharedPtr<FEngineSessionManager> FEngineAnalytics::SessionManager;

/**
* Default config func.
*/
FAnalyticsET::Config DefaultEngineAnalyticsConfigFunc()
{
	return FAnalyticsET::Config();
}

/**
* Engine analytics config to initialize the analytics provider.
* External code should bind this delegate if engine analytics are desired,
* preferably in private code that won't be redistributed.
*/
TFunction<FAnalyticsET::Config()>& GetEngineAnalyticsConfigFunc()
{
	static TFunction<FAnalyticsET::Config()> Config = &DefaultEngineAnalyticsConfigFunc;
	return Config;
}

/**
 * Get analytics pointer
 */
IAnalyticsProvider& FEngineAnalytics::GetProvider()
{
	checkf(bIsInitialized && IsAvailable(), TEXT("FEngineAnalytics::GetProvider called outside of Initialize/Shutdown."));

	return *Analytics.Get();
}
 
void FEngineAnalytics::Initialize()
{
	checkf(!bIsInitialized, TEXT("FEngineAnalytics::Initialize called more than once."));

	check(GEngine);

#if WITH_EDITOR
	// this will only be true for builds that have editor support (currently PC, Mac, Linux)
	// The idea here is to only send editor events for actual editor runs, not for things like -game runs of the editor.
	bIsEditorRun = GIsEditor && !IsRunningCommandlet();
	bIsGameRun = false;
#else
	// We also want to identify a real run of a game, which is NOT necessarily the opposite of an editor run.
	// Ideally we'd be able to tell explicitly, but with content-only games, it becomes difficult.
	// So we ensure we are not an editor run, we don't have EDITOR stuff compiled in, we are not running a commandlet,
	// we are not a generic, utility program, and we require cooked data.
	bIsEditorRun = false;
	bIsGameRun = !IsRunningCommandlet() && !FPlatformProperties::IsProgram() && FPlatformProperties::RequiresCookedData();
#endif

#if UE_BUILD_DEBUG
	const bool bShouldInitAnalytics = false;
#else
	// Outside of the editor, the only engine analytics usage is the hardware survey
	const bool bShouldInitAnalytics = (bIsEditorRun && GEngine->AreEditorAnalyticsEnabled()) || (bIsGameRun && GEngine->AreGameAnalyticsEnabled());
#endif

	if (bShouldInitAnalytics)
	{
		// Get the default config.
		FAnalyticsET::Config Config = GetEngineAnalyticsConfigFunc()();
		// Set any fields that weren't set by default.
		if (Config.APIKeyET.IsEmpty())
		{
			// We always use the "Release" analytics account unless we're running in analytics test mode (usually with
			// a command-line parameter), or we're an internal Epic build
			const EAnalyticsBuildType AnalyticsBuildType = GetAnalyticsBuildType();
			const bool bUseReleaseAccount =
				(AnalyticsBuildType == EAnalyticsBuildType::Development || AnalyticsBuildType == EAnalyticsBuildType::Release) &&
				!FEngineBuildSettings::IsInternalBuild();	// Internal Epic build
			const TCHAR* BuildTypeStr = bUseReleaseAccount ? TEXT("Release") : TEXT("Dev");

			FString UE4TypeOverride;
			bool bHasOverride = GConfig->GetString(TEXT("Analytics"), TEXT("UE4TypeOverride"), UE4TypeOverride, GEngineIni);
			const TCHAR* UE4TypeStr = bHasOverride ? *UE4TypeOverride : FEngineBuildSettings::IsPerforceBuild() ? TEXT("Perforce") : TEXT("UnrealEngine");
			if (bIsEditorRun)
			{
				Config.APIKeyET = FString::Printf(TEXT("UEEditor.%s.%s"), UE4TypeStr, BuildTypeStr);
			}
			else
			{
				const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();
				Config.APIKeyET = FString::Printf(TEXT("UEGame.%s.%s|%s|%s"), UE4TypeStr, BuildTypeStr, *ProjectSettings.ProjectID.ToString(), *ProjectSettings.ProjectName);
			}
		}
		if (Config.APIServerET.IsEmpty())
		{
			Config.APIServerET = TEXT("https://datarouter.ol.epicgames.com/");
		}
		if (Config.AppEnvironment.IsEmpty())
		{
			Config.AppEnvironment = TEXT("datacollector-source");
		}
		if (Config.AppVersionET.IsEmpty())
		{
			Config.AppVersionET = FEngineVersion::Current().ToString();
		}


		// Connect the engine analytics provider (if there is a configuration delegate installed)
		Analytics = FAnalyticsET::Get().CreateAnalyticsProvider(Config);

		if (Analytics.IsValid())
		{
			// Use an anonymous user id in-game
			if (bIsGameRun && GEngine->AreGameAnalyticsAnonymous())
			{
				FString AnonymousId;
				if (!FPlatformMisc::GetStoredValue(TEXT("Epic Games"), TEXT("Unreal Engine/Privacy"), TEXT("AnonymousID"), AnonymousId) || AnonymousId.IsEmpty())
				{
					AnonymousId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensInBraces);
					FPlatformMisc::SetStoredValue(TEXT("Epic Games"), TEXT("Unreal Engine/Privacy"), TEXT("AnonymousID"), AnonymousId);
				}

				// Place the anonymous user id into the first field of the UserID set, as it most closely matches the LoginID semantics.
				Analytics->SetUserID(FString::Printf(TEXT("ANON-%s||"), *AnonymousId));
			}
			else
			{
				Analytics->SetUserID(FString::Printf(TEXT("%s|%s|%s"), *FPlatformMisc::GetLoginId(), *FPlatformMisc::GetEpicAccountId(), *FPlatformMisc::GetOperatingSystemId()));
			}

			TArray<FAnalyticsEventAttribute> StartSessionAttributes;
			GEngine->CreateStartupAnalyticsAttributes( StartSessionAttributes );
			// Add project info whether we are in editor or game.
			const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();
			FString OSMajor;
			FString OSMinor;
			FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
			StartSessionAttributes.Emplace(TEXT("ProjectName"), ProjectSettings.ProjectName);
			StartSessionAttributes.Emplace(TEXT("ProjectID"), ProjectSettings.ProjectID);
			StartSessionAttributes.Emplace(TEXT("ProjectDescription"), ProjectSettings.Description);
			StartSessionAttributes.Emplace(TEXT("ProjectVersion"), ProjectSettings.ProjectVersion);
			StartSessionAttributes.Emplace(TEXT("GPUVendorID"), GRHIVendorId);
			StartSessionAttributes.Emplace(TEXT("GPUDeviceID"), GRHIDeviceId);
			StartSessionAttributes.Emplace(TEXT("GRHIDeviceRevision"), GRHIDeviceRevision);
			StartSessionAttributes.Emplace(TEXT("GRHIAdapterInternalDriverVersion"), GRHIAdapterInternalDriverVersion);
			StartSessionAttributes.Emplace(TEXT("GRHIAdapterUserDriverVersion"), GRHIAdapterUserDriverVersion);
			StartSessionAttributes.Emplace(TEXT("TotalPhysicalRAM"), static_cast<uint64>(Stats.TotalPhysical));
			StartSessionAttributes.Emplace(TEXT("CPUPhysicalCores"), FPlatformMisc::NumberOfCores());
			StartSessionAttributes.Emplace(TEXT("CPULogicalCores"), FPlatformMisc::NumberOfCoresIncludingHyperthreads());
			StartSessionAttributes.Emplace(TEXT("DesktopGPUAdapter"), FPlatformMisc::GetPrimaryGPUBrand());
			StartSessionAttributes.Emplace(TEXT("RenderingGPUAdapter"), GRHIAdapterName);
			StartSessionAttributes.Emplace(TEXT("CPUVendor"), FPlatformMisc::GetCPUVendor());
			StartSessionAttributes.Emplace(TEXT("CPUBrand"), FPlatformMisc::GetCPUBrand());
			FPlatformMisc::GetOSVersions(/*out*/ OSMajor, /*out*/ OSMinor);
			StartSessionAttributes.Emplace(TEXT("OSMajor"), OSMajor);
			StartSessionAttributes.Emplace(TEXT("OSMinor"), OSMinor);
			StartSessionAttributes.Emplace(TEXT("OSVersion"), FPlatformMisc::GetOSVersion());
			StartSessionAttributes.Emplace(TEXT("Is64BitOS"), FPlatformMisc::Is64bitOperatingSystem());
			Analytics->StartSession(MoveTemp(StartSessionAttributes));

			bIsInitialized = true;
		}

		// Create the session manager singleton
		if (!SessionManager.IsValid())
		{
			if (bIsEditorRun || (bIsGameRun && GEngine->AreGameMTBFEventsEnabled()))
			{
				SessionManager = MakeShareable(new FEngineSessionManager(bIsEditorRun ? EEngineSessionManagerMode::Editor : EEngineSessionManagerMode::Game));
				SessionManager->Initialize();
			}
		}
	}
}


void FEngineAnalytics::Shutdown(bool bIsEngineShutdown)
{
	ensure(!Analytics.IsValid() || Analytics.IsUnique());
	Analytics.Reset();
	bIsInitialized = false;

	// Destroy the session manager singleton if it exists
	if (SessionManager.IsValid() && bIsEngineShutdown)
	{
		SessionManager->Shutdown();
		SessionManager.Reset();
	}
}

void FEngineAnalytics::Tick(float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineAnalytics_Tick);

	if (SessionManager.IsValid())
	{
		SessionManager->Tick(DeltaTime);
	}
}
