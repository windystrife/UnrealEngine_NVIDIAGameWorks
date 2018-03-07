// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "QoSReporter.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "QoSReporterPrivate.h"

#include "Analytics.h"
#include "Interfaces/IAnalyticsProvider.h"
#if WITH_PERFCOUNTERS
	#include "PerfCountersModule.h"
#endif // WITH_PERFCOUNTERS
#if WITH_ENGINE
	#include "Net/PerfCountersHelpers.h"
#endif // WITH_ENGINE

#ifndef WITH_QOSREPORTER
	#error "WITH_QOSREPORTER should be defined in Build.cs file"
#endif

bool FQoSReporter::bIsInitialized;
TSharedPtr<IAnalyticsProvider> FQoSReporter::Analytics;

namespace
{
	FString		StoredDeploymentName;
};

/**
* External code should bind this delegate if QoS reporting is desired,
* preferably in private code that won't be redistributed.
*/
QOSREPORTER_API FAnalyticsProviderConfigurationDelegate& GetQoSOverrideConfigDelegate()
{
	static FAnalyticsProviderConfigurationDelegate Delegate;
	return Delegate;
}


/**
* Get analytics pointer
*/
QOSREPORTER_API IAnalyticsProvider& FQoSReporter::GetProvider()
{
	checkf(bIsInitialized && IsAvailable(), TEXT("FQoSReporter::GetProvider called outside of Initialize/Shutdown."));

	return *Analytics.Get();
}

QOSREPORTER_API void FQoSReporter::Initialize()
{
	checkf(!bIsInitialized, TEXT("FQoSReporter::Initialize called more than once."));

	// allow overriding from configs
	bool bEnabled = true;
	if (GConfig->GetBool(TEXT("QoSReporter"), TEXT("bEnabled"), bEnabled, GEngineIni) && !bEnabled)
	{
		UE_LOG(LogQoSReporter, Verbose, TEXT("QoSReporter disabled by config setting"));
		return;
	}

	// Setup some default engine analytics if there is nothing custom bound
	FAnalyticsProviderConfigurationDelegate DefaultEngineAnalyticsConfig;
	DefaultEngineAnalyticsConfig.BindLambda(
		[=](const FString& KeyName, bool bIsValueRequired) -> FString
	{
		static TMap<FString, FString> ConfigMap;
		if (ConfigMap.Num() == 0)
		{
			ConfigMap.Add(TEXT("ProviderModuleName"), TEXT("QoSReporter"));
			ConfigMap.Add(TEXT("APIKeyQoS"), FString::Printf(TEXT("%s.%s"), FApp::GetProjectName(), AnalyticsBuildTypeToString(GetAnalyticsBuildType())));
		}

		// Check for overrides
		if (GetQoSOverrideConfigDelegate().IsBound())
		{
			const FString OverrideValue = GetQoSOverrideConfigDelegate().Execute(KeyName, bIsValueRequired);
			if (!OverrideValue.IsEmpty())
			{
				return OverrideValue;
			}
		}

		FString* ConfigValue = ConfigMap.Find(KeyName);
		return ConfigValue != NULL ? *ConfigValue : TEXT("");
	});

	// Connect the engine analytics provider (if there is a configuration delegate installed)
	Analytics = FAnalytics::Get().CreateAnalyticsProvider(
		FName(*DefaultEngineAnalyticsConfig.Execute(TEXT("ProviderModuleName"), true)),
		DefaultEngineAnalyticsConfig);
	if (!Analytics.IsValid())
	{
		return;
	}

	// set this directly as SetBackendDeploymentName will early-out on same value
	Analytics->SetLocation(StoredDeploymentName);

	// check if Configs override the heartbeat interval
	float ConfigHeartbeatInterval = 0.0;
	if (GConfig->GetFloat(TEXT("QoSReporter"), TEXT("HeartbeatInterval"), ConfigHeartbeatInterval, GEngineIni))
	{
		HeartbeatInterval = ConfigHeartbeatInterval;
		UE_LOG(LogQoSReporter, Verbose, TEXT("HeartbeatInterval configured to %f from config."), HeartbeatInterval);
	}

	// randomize heartbeats to prevent servers from bursting at once (they hit rate limit on data router and get throttled with 429)
	LastHeartbeatTimestamp = FPlatformTime::Seconds() + HeartbeatInterval * FMath::FRand();

	bIsInitialized = true;
}

void FQoSReporter::Shutdown()
{
	checkf(!bIsInitialized || Analytics.IsValid(), TEXT("Analytics provider for QoS reporter module is left initialized - internal error."));

	Analytics.Reset();
	bIsInitialized = false;
}

FString FQoSReporter::GetQoSReporterInstanceId()
{
	if (Analytics.IsValid())
	{
		return Analytics->GetUserID();
	}

	return FString();
}

FString FQoSReporter::GetBackendDeploymentName()
{
	return StoredDeploymentName;
}

double FQoSReporter::ModuleInitializationTime = FPlatformTime::Seconds();
bool FQoSReporter::bStartupEventReported = false;
bool FQoSReporter::bCountHitches = false;

void FQoSReporter::ReportStartupCompleteEvent()
{
	if (bStartupEventReported || !Analytics.IsValid())
	{
		return;
	}

	double CurrentTime = FPlatformTime::Seconds();
	double Difference = CurrentTime - ModuleInitializationTime;

	TArray<FAnalyticsEventAttribute> ParamArray;

	ParamArray.Add(FAnalyticsEventAttribute(EQoSEvents::ToString(EQoSEventParam::StartupTime), Difference));
	Analytics->RecordEvent(EQoSEvents::ToString(EQoSEventParam::StartupTime), ParamArray);

	// log it so we can do basic analysis by log scraping
	UE_LOG(LogQoSReporter, Log, TEXT("Startup complete, took %f seconds."), Difference);

	bStartupEventReported = true;
}

void FQoSReporter::SetBackendDeploymentName(const FString & InDeploymentName)
{
	if (StoredDeploymentName == InDeploymentName)
	{
		return;
	}

	StoredDeploymentName = InDeploymentName;

	if (Analytics.IsValid())
	{
		// abuse somewhat outdated IAnalyticsProvider API for this
		Analytics->SetLocation(InDeploymentName);

		if (InDeploymentName.Len() > 0)
		{
			UE_LOG(LogQoSReporter, Log, TEXT("QoSReporter has been configured for '%s' deployment."), *InDeploymentName);
		}
		else
		{
			UE_LOG(LogQoSReporter, Log, TEXT("QoSReporter has been configured without a valid deployment name."));
		}
	}
	else
	{
		UE_LOG(LogQoSReporter, Log, TEXT("QoSReporter will be configured for '%s' deployment."), *StoredDeploymentName);
	}
}

void FQoSReporter::EnableCountingHitches(bool bEnable)
{
	bCountHitches = bEnable;
	PreviousTickTime = FPlatformTime::Seconds();	// reset the timer
	UE_LOG(LogQoSReporter, Log, TEXT("Counting hitches in QoSReporter has been %s."), bCountHitches ? TEXT("enabled") : TEXT("disabled"));
}

double FQoSReporter::HeartbeatInterval = 300;
double FQoSReporter::LastHeartbeatTimestamp = 0;
double FQoSReporter::PreviousTickTime = FPlatformTime::Seconds();
#if WITH_ENGINE
extern ENGINE_API float GAverageFPS;
#else
float GAverageFPS = 0.0f;	// fake
#endif // WITH_ENGINE

void FQoSReporter::Tick()
{
	if (!Analytics.IsValid())
	{
		return;
	}

	double CurrentTime = FPlatformTime::Seconds();

	if (HeartbeatInterval > 0 && CurrentTime - LastHeartbeatTimestamp > HeartbeatInterval)
	{
		SendHeartbeat();
		LastHeartbeatTimestamp = CurrentTime;
	}

	// detect too long pauses between ticks, unless configured to ignore them or running under debugger
#if !QOS_IGNORE_HITCHES
	if (bCountHitches && !FPlatformMisc::IsDebuggerPresent())
	{
		const double DeltaBetweenTicks = CurrentTime - PreviousTickTime;

		if (DeltaBetweenTicks > 0.1)
		{
#if USE_SERVER_PERF_COUNTERS
			PerfCountersIncrement(TEXT("HitchesAbove100msec"), 0, IPerfCounters::Flags::Transient);

			// count 250 ms
			if (DeltaBetweenTicks > 0.25)
			{
				PerfCountersIncrement(TEXT("HitchesAbove250msec"), 0, IPerfCounters::Flags::Transient);

				if (DeltaBetweenTicks > 0.5)
				{
					PerfCountersIncrement(TEXT("HitchesAbove500msec"), 0, IPerfCounters::Flags::Transient);

					if (DeltaBetweenTicks > 1)
					{
						PerfCountersIncrement(TEXT("HitchesAbove1000msec"), 0, IPerfCounters::Flags::Transient);
					}
				}
			}
#endif // USE_SERVER_PERF_COUNTERS

			UE_LOG(LogQoSReporter, Log, TEXT("QoS reporter could not tick for %f sec, average FPS is %f."),
				DeltaBetweenTicks, GAverageFPS);
		}
	}
#endif

	PreviousTickTime = CurrentTime;
}

void FQoSReporter::SendHeartbeat()
{
	checkf(Analytics.IsValid(), TEXT("SendHeartbeat() should not be called if Analytics provider was not configured"));

	TArray<FAnalyticsEventAttribute> ParamArray;

	if (IsRunningDedicatedServer())
	{
		AddServerHeartbeatAttributes(ParamArray);
		Analytics->RecordEvent(EQoSEvents::ToString(EQoSEventParam::ServerPerfCounters), ParamArray);
	}
	else
	{
		AddClientHeartbeatAttributes(ParamArray);
		Analytics->RecordEvent(EQoSEvents::ToString(EQoSEventParam::Heartbeat), ParamArray);
	}
}

/**
 * Sends heartbeat stats.
 */
void FQoSReporter::AddServerHeartbeatAttributes(TArray<FAnalyticsEventAttribute> & OutArray)
{
#if WITH_PERFCOUNTERS
	IPerfCounters* PerfCounters = IPerfCountersModule::Get().GetPerformanceCounters();
	if (PerfCounters)
	{
		const TMap<FString, IPerfCounters::FJsonVariant>& PerfCounterMap = PerfCounters->GetAllCounters();
		int NumAddedCounters = 0;

		// only add string and number values
		for (const auto& It : PerfCounterMap)
		{
			const IPerfCounters::FJsonVariant& JsonValue = It.Value;
			switch (JsonValue.Format)
			{
				case IPerfCounters::FJsonVariant::String:
					OutArray.Add(FAnalyticsEventAttribute(It.Key, JsonValue.StringValue));
					break;
				case IPerfCounters::FJsonVariant::Number:
					OutArray.Add(FAnalyticsEventAttribute(It.Key, FString::Printf(TEXT("%f"), JsonValue.NumberValue)));
					break;
				default:
					// don't write anything (supporting these requires more changes in PerfCounters API)
					UE_LOG(LogQoSReporter, Verbose, TEXT("PerfCounter '%s' of unsupported type %d skipped"), *It.Key, static_cast<int32>(JsonValue.Format));
					break;
			}
		}

		// reset perfcounters stats here
		UE_LOG(LogQoSReporter, Verbose, TEXT("Resetting PerfCounters - new stat period begins."));
		PerfCounters->ResetStatsForNextPeriod();
	}
	else if (UE_SERVER)
	{
		UE_LOG(LogQoSReporter, Warning, TEXT("PerfCounters module is not available, could not send proper server heartbeat."));
		OutArray.Add(FAnalyticsEventAttribute(TEXT("MisconfiguredPerfCounters"), 1));
	}
#else
	#if UE_SERVER
		#error "QoS module requires perfcounters for servers"
	#endif // UE_SERVER
#endif // WITH_PERFCOUNTERS
}

void FQoSReporter::AddClientHeartbeatAttributes(TArray<FAnalyticsEventAttribute> & OutArray)
{
	OutArray.Add(FAnalyticsEventAttribute(TEXT("AverageFPS"), GAverageFPS));
}
