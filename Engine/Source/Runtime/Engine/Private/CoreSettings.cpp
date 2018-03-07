// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/CoreSettings.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"
#include "Misc/App.h"

DEFINE_LOG_CATEGORY_STATIC(LogCoreSettings, Log, All);

int32 GUseBackgroundLevelStreaming = 1;
float GAsyncLoadingTimeLimit = 5.0f;
int32 GAsyncLoadingUseFullTimeLimit = 1;
float GPriorityAsyncLoadingExtraTime = 20.0f;
float GLevelStreamingActorsUpdateTimeLimit = 5.0f;
float GLevelStreamingUnregisterComponentsTimeLimit = 1.0f;
int32 GLevelStreamingComponentsRegistrationGranularity = 10;
int32 GLevelStreamingComponentsUnregistrationGranularity = 5;

static FAutoConsoleVariableRef CVarUseBackgroundLevelStreaming(
	TEXT("s.UseBackgroundLevelStreaming"),
	GUseBackgroundLevelStreaming,
	TEXT("Whether to allow background level streaming."),
	ECVF_Default
	);

static FAutoConsoleVariableRef CVarAsyncLoadingTimeLimit(
	TEXT("s.AsyncLoadingTimeLimit"),
	GAsyncLoadingTimeLimit,
	TEXT("Maximum amount of time to spend doing asynchronous loading (ms per frame)."),
	ECVF_Default
	);

static FAutoConsoleVariableRef CVarAsyncLoadingUseFullTimeLimit(
	TEXT("s.AsyncLoadingUseFullTimeLimit"),
	GAsyncLoadingUseFullTimeLimit,
	TEXT("Whether to use the entire time limit even if blocked on I/O."),
	ECVF_Default
	);

static FAutoConsoleVariableRef CVarPriorityAsyncLoadingExtraTime(
	TEXT("s.PriorityAsyncLoadingExtraTime"),
	GPriorityAsyncLoadingExtraTime,
	TEXT("Additional time to spend asynchronous loading during a high priority load."),
	ECVF_Default
	);

static FAutoConsoleVariableRef CVarLevelStreamingActorsUpdateTimeLimit(
	TEXT("s.LevelStreamingActorsUpdateTimeLimit"),
	GLevelStreamingActorsUpdateTimeLimit,
	TEXT("Maximum allowed time to spend for actor registration steps during level streaming (ms per frame)."),
	ECVF_Default
	);

static FAutoConsoleVariableRef CVarLevelStreamingUnregisterComponentsTimeLimit(
	TEXT("s.UnregisterComponentsTimeLimit"),
	GLevelStreamingUnregisterComponentsTimeLimit,
	TEXT("Maximum allowed time to spend for actor unregistration steps during level streaming (ms per frame). If this is zero then we don't timeslice"),
	ECVF_Default
);

static FAutoConsoleVariableRef CVarLevelStreamingComponentsRegistrationGranularity(
	TEXT("s.LevelStreamingComponentsRegistrationGranularity"),
	GLevelStreamingComponentsRegistrationGranularity,
	TEXT("Batching granularity used to register actor components during level streaming."),
	ECVF_Default
	);

static FAutoConsoleVariableRef CVarLevelStreamingComponentsUnregistrationGranularity(
	TEXT("s.LevelStreamingComponentsUnregistrationGranularity"),
	GLevelStreamingComponentsUnregistrationGranularity,
	TEXT("Batching granularity used to unregister actor components during level unstreaming."),
	ECVF_Default
	);

UStreamingSettings::UStreamingSettings()
	: Super()
{
	SectionName = TEXT("Streaming");

	AsyncLoadingThreadEnabled = false;
	WarnIfTimeLimitExceeded = false;
	TimeLimitExceededMultiplier = 1.5f;
	TimeLimitExceededMinTime = 0.005f;
	MinBulkDataSizeForAsyncLoading = 131072;
	UseBackgroundLevelStreaming = true;
	AsyncLoadingTimeLimit = 5.0f;
	AsyncLoadingUseFullTimeLimit = true;
	PriorityAsyncLoadingExtraTime = 20.0f;
	LevelStreamingActorsUpdateTimeLimit = 5.0f;
	LevelStreamingComponentsRegistrationGranularity = 10;
	LevelStreamingUnregisterComponentsTimeLimit = 1.0f;
	LevelStreamingComponentsUnregistrationGranularity = 5;
	EventDrivenLoaderEnabled = false;
}

void UStreamingSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	if (IsTemplate())
	{
		ImportConsoleVariableValues();
	}
#endif // #if WITH_EDITOR
}

#if WITH_EDITOR
void UStreamingSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static FName NAME_EventDrivenLoaderEnabled(TEXT("EventDrivenLoaderEnabled"));

	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (PropertyChangedEvent.Property)
	{
		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);
	}
}
#endif // #if WITH_EDITOR

UGarbageCollectionSettings::UGarbageCollectionSettings()
: Super()
{
	SectionName = TEXT("Garbage Collection");

	TimeBetweenPurgingPendingKillObjects = 60.0f;
	FlushStreamingOnGC = false;
	AllowParallelGC = true;
	NumRetriesBeforeForcingGC = 0;
	MaxObjectsNotConsideredByGC = 0;
	SizeOfPermanentObjectPool = 0;
	MaxObjectsInEditor = 12 * 1024 * 1024;
	MaxObjectsInGame = 2 * 1024 * 1024;
	CreateGCClusters = true;
	MergeGCClusters = false;
	ActorClusteringEnabled = true;
	BlueprintClusteringEnabled = false;
	UseDisregardForGCOnDedicatedServers = false;
}

void UGarbageCollectionSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	if (IsTemplate())
	{
		ImportConsoleVariableValues();
	}
#endif // #if WITH_EDITOR
}

#if WITH_EDITOR
void UGarbageCollectionSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);
	}
}
#endif // #if WITH_EDITOR
