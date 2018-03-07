// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PerfCountersModule.h"
#include "HAL/PlatformProcess.h"
#include "PerfCounters.h"

class FPerfCountersModule : public IPerfCountersModule
{
private:

	/** All created perf counter instances from this module */
	FPerfCounters* PerfCountersSingleton;

public:

	FPerfCountersModule() : 
		PerfCountersSingleton(NULL)
	{}

	void ShutdownModule()
	{
		if (PerfCountersSingleton)
		{
			delete PerfCountersSingleton;
			PerfCountersSingleton = nullptr;
		}
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

	virtual bool SupportsAutomaticShutdown() override
	{
		return false;
	}

	IPerfCounters* GetPerformanceCounters() const
	{
		return PerfCountersSingleton;
	}

	IPerfCounters* CreatePerformanceCounters(const FString& UniqueInstanceId) override
	{
		if (PerfCountersSingleton)
		{
			UE_LOG(LogPerfCounters, Display, TEXT("CreatePerformanceCounters: instance already exists, new instance not created."));
			return PerfCountersSingleton;
		}

		FString InstanceUID = UniqueInstanceId;
		if (InstanceUID.IsEmpty())
		{
			InstanceUID = FString::Printf(TEXT("perfcounters-of-pid-%d"), FPlatformProcess::GetCurrentProcessId());
		}

		FPerfCounters* PerfCounters = new FPerfCounters(InstanceUID);
		if (!PerfCounters->Initialize())
		{
			UE_LOG(LogPerfCounters, Warning, TEXT("CreatePerformanceCounters: could not create perfcounters"));
			delete PerfCounters;
			return nullptr;
		}

		PerfCountersSingleton = PerfCounters;
		return PerfCounters;
	}
};

IMPLEMENT_MODULE(FPerfCountersModule, PerfCounters)
DEFINE_LOG_CATEGORY(LogPerfCounters);

const FName IPerfCounters::Histograms::FrameTime(TEXT("FrameTime"));
const FName IPerfCounters::Histograms::FrameTimePeriodic(TEXT("FrameTimePeriodic"));
const FName IPerfCounters::Histograms::FrameTimeWithoutSleep(TEXT("FrameTimeWithoutSleep"));
const FName IPerfCounters::Histograms::ServerReplicateActorsTime(TEXT("ServerReplicateActorsTime"));
const FName IPerfCounters::Histograms::SleepTime(TEXT("SleepTime"));
const FName IPerfCounters::Histograms::ZeroLoadFrameTime(TEXT("ZeroLoadFrameTime"));
