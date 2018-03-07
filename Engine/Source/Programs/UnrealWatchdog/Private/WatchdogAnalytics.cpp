// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WatchdogAnalytics.h"
#include "UnrealWatchdog.h"
#include "EngineBuildSettings.h"
#include "AnalyticsET.h"
#include "IAnalyticsProviderET.h"

bool FWatchdogAnalytics::bIsInitialized;
TSharedPtr<IAnalyticsProviderET> FWatchdogAnalytics::Analytics;

/**
* Default config func that essentially tells the crash reporter to disable analytics.
*/
FAnalyticsET::Config DefaultAnalyticsConfigFunc()
{
	return FAnalyticsET::Config();
}

/**
* Engine analytics config to initialize the watchdog analytics provider.
* External code should bind this delegate if watchdog analytics in private code that won't be redistributed.
*/
TFunction<FAnalyticsET::Config()>& GetWatchdogAnalyticsConfigFunc()
{
	static TFunction<FAnalyticsET::Config()> Config = &DefaultAnalyticsConfigFunc;
	return Config;
}

/**
* On-demand construction of the singleton.
*/
IAnalyticsProviderET& FWatchdogAnalytics::GetProvider()
{
	checkf(bIsInitialized && Analytics.IsValid(), TEXT("FWatchdogAnalytics::GetProvider called outside of Initialize/Shutdown."));
	return *Analytics.Get();
}

void FWatchdogAnalytics::Initialize()
{
	checkf(!bIsInitialized, TEXT("FWatchdogAnalytics::Initialize called more than once."));

	FAnalyticsET::Config Config = GetWatchdogAnalyticsConfigFunc()();
	if (!Config.APIServerET.IsEmpty())
	{
		// Connect the engine analytics provider (if there is a configuration delegate installed)
		Analytics = FAnalyticsET::Get().CreateAnalyticsProvider(Config);
		if (Analytics.IsValid())
		{
			Analytics->SetUserID(FString::Printf(TEXT("%s|%s|%s"), *FPlatformMisc::GetLoginId(), *FPlatformMisc::GetEpicAccountId(), *FPlatformMisc::GetOperatingSystemId()));
			Analytics->StartSession();
		}
	}
	bIsInitialized = true;
}


void FWatchdogAnalytics::Shutdown()
{
	checkf(bIsInitialized, TEXT("FWatchdogAnalytics::Shutdown called outside of Initialize."));
	Analytics.Reset();
	bIsInitialized = false;
}
