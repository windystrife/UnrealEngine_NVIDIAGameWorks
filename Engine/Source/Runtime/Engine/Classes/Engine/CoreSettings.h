// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/DeveloperSettings.h"

#include "CoreSettings.generated.h"

struct FPropertyChangedEvent;

/**
 * Streaming settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Streaming"))
class ENGINE_API UStreamingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UStreamingSettings();

protected:

	UPROPERTY(config, EditAnywhere, Category = PackageStreaming, meta = (
		ConsoleVariable = "s.AsyncLoadingThreadEnabled", DisplayName = "Async Loading Thread Enabled",
		ToolTip = "Enables separate thread for package streaming. Requires restart to take effect."))
	uint32 AsyncLoadingThreadEnabled : 1;

	UPROPERTY(config, EditAnywhere, Category = PackageStreaming, AdvancedDisplay, meta = (
		ConsoleVariable = "s.WarnIfTimeLimitExceeded", DisplayName = "Warn If Time Limit Has Been Exceeded",
		ToolTip = "Enables log warning if time limit for time-sliced package streaming has been exceeded."))
	uint32 WarnIfTimeLimitExceeded : 1;

	UPROPERTY(config, EditAnywhere, Category = PackageStreaming, AdvancedDisplay, meta = (
		ConsoleVariable = "s.TimeLimitExceededMultiplier", DisplayName = "Time Limit Exceeded Warning Multiplier",
		ToolTip = "Multiplier for time limit exceeded warning time threshold."))
	float TimeLimitExceededMultiplier;

	UPROPERTY(config, EditAnywhere, Category = PackageStreaming, AdvancedDisplay, meta = (
		ConsoleVariable = "s.TimeLimitExceededMinTime", DisplayName = "Minimum Time Limit For Time Limit Exceeded Warning",
		ToolTip = "Minimum time the time limit exceeded warning will be triggered by."))
	float TimeLimitExceededMinTime;

	UPROPERTY(config, EditAnywhere, Category = PackageStreaming, AdvancedDisplay, meta = (
		ConsoleVariable = "s.MinBulkDataSizeForAsyncLoading", DisplayName = "Minimum Bulk Data Size For Async Loading",
		ToolTip = "Minimum time the time limit exceeded warning will be triggered by."))
	int32 MinBulkDataSizeForAsyncLoading;

	UPROPERTY(EditAnywhere, config, Category = LevelStreaming, meta = (
		ConsoleVariable = "s.UseBackgroundLevelStreaming", DisplayName = "Use Background Level Streaming",
		ToolTip = "Whether to allow background level streaming."))
	uint32 UseBackgroundLevelStreaming:1;

	/** Whether to use the entire time limit even if blocked on I/O */
	UPROPERTY(EditAnywhere, config, Category = LevelStreaming, AdvancedDisplay, meta = (
		ConsoleVariable = "s.AsyncLoadingUseFullTimeLimit", DisplayName = "Async Loading Use Full Time Limit",
		ToolTip = "Whether to use the entire time limit even if blocked on I/O."))
	uint32 AsyncLoadingUseFullTimeLimit:1;

	UPROPERTY(EditAnywhere, config, Category = LevelStreaming, AdvancedDisplay, meta = (
		ConsoleVariable = "s.AsyncLoadingTimeLimit", DisplayName = "Async Loading Time Limit",
		ToolTip = "Maximum amount of time to spend doing asynchronous loading (ms per frame)."))
	float AsyncLoadingTimeLimit;

	UPROPERTY(EditAnywhere, config, Category = LevelStreaming, AdvancedDisplay, meta = (
		ConsoleVariable = "s.PriorityAsyncLoadingExtraTime", DisplayName = "Priority Async Loading Extra Time",
		ToolTip = "Additional time to spend asynchronous loading during a high priority load."))
	float PriorityAsyncLoadingExtraTime;

	/** Maximum allowed time to spend for actor registration steps during level streaming (ms per frame)*/
	UPROPERTY(EditAnywhere, config, Category = LevelStreaming, AdvancedDisplay, meta = (
		ConsoleVariable = "s.LevelStreamingActorsUpdateTimeLimit", DisplayName = "Actor Initialization Update Time Limit",
		ToolTip = "Maximum allowed time to spend for actor registration steps during level streaming (ms per frame)."))
	float LevelStreamingActorsUpdateTimeLimit;

	/** Batching granularity used to register actor components during level streaming */
	UPROPERTY(EditAnywhere, config, Category = LevelStreaming, AdvancedDisplay, meta = (
		ConsoleVariable = "s.LevelStreamingComponentsRegistrationGranularity", DisplayName = "Components Registration Granularity",
		ToolTip = "Batching granularity used to register actor components during level streaming."))
	int32 LevelStreamingComponentsRegistrationGranularity;

	/** Maximum allowed time to spend while unregistering components during level streaming (ms per frame) */
	UPROPERTY(EditAnywhere, config, Category = LevelStreaming, AdvancedDisplay, meta = (
		ConsoleVariable = "s.UnregisterComponentsTimeLimit", DisplayName = "Component Unregister Update Time Limit",
		ToolTip = "Maximum allowed time to spend while unregistering components during level streaming (ms per frame)."))
		float LevelStreamingUnregisterComponentsTimeLimit;

	/** Batching granularity used to unregister actor components during level streaming */
	UPROPERTY(EditAnywhere, config, Category = LevelStreaming, AdvancedDisplay, meta = (
		ConsoleVariable = "s.LevelStreamingComponentsUnregistrationGranularity", DisplayName = "Components Unregistration Granularity",
		ToolTip = "Batching granularity used to unregister actor components during level streaming."))
	int32 LevelStreamingComponentsUnregistrationGranularity;

	UPROPERTY(config, EditAnywhere, Category = PackageStreaming, meta = (
		ConsoleVariable = "s.EventDrivenLoaderEnabled", DisplayName = "Event Driven Loader Enabled",
		ToolTip = "Enables the event driven loader in cooked builds."))
	uint32 EventDrivenLoaderEnabled : 1;

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface
};

/** Whether to allow background level streaming. */
extern ENGINE_API int32 GUseBackgroundLevelStreaming;
/** Maximum amount of time to spend doing asynchronous loading (ms per frame). */
extern ENGINE_API float GAsyncLoadingTimeLimit;
/** Whether to use the entire time limit even if blocked on I/O. */
extern ENGINE_API int32 GAsyncLoadingUseFullTimeLimit;
/** Additional time to spend asynchronous loading during a high priority load. */
extern ENGINE_API float GPriorityAsyncLoadingExtraTime;
/** Maximum allowed time to spend for actor registration steps during level streaming (ms per frame). */
extern ENGINE_API float GLevelStreamingActorsUpdateTimeLimit;
/** Batching granularity used to register actor components during level streaming. */
extern ENGINE_API int32 GLevelStreamingComponentsRegistrationGranularity;
/** Batching granularity used to unregister actor components during level streaming.  */
extern ENGINE_API int32 GLevelStreamingComponentsUnregistrationGranularity;
/** Maximum allowed time to spend for actor unregistration steps during level streaming (ms per frame). If this is 0.0 then we don't timeslice.*/
extern ENGINE_API float GLevelStreamingUnregisterComponentsTimeLimit;

/**
* Implements the settings for garbage collection.
*/
UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "Garbage Collection"))
class ENGINE_API UGarbageCollectionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UGarbageCollectionSettings();

protected:

	UPROPERTY(EditAnywhere, config, Category = General, meta = (
		ConsoleVariable = "gc.TimeBetweenPurgingPendingKillObjects", DisplayName = "Time Between Purging Pending Kill Objects",
		ToolTip = "Time in seconds (game time) we should wait between purging object references to objects that are pending kill."))
	float TimeBetweenPurgingPendingKillObjects;

	UPROPERTY(EditAnywhere, config, Category = General, meta = (
		ConsoleVariable = "gc.FlushStreamingOnGC", DisplayName = "Flush Streaming On GC",
		ToolTip = "If enabled, streaming will be flushed each time garbage collection is triggered."))
	uint32 FlushStreamingOnGC : 1;

	UPROPERTY(EditAnywhere, config, Category = Optimization, meta = (
		ConsoleVariable = "gc.AllowParallelGC", DisplayName = "Allow Parallel GC",
		ToolTip = "If enabled, garbage collection will use multiple threads."))
	uint32 AllowParallelGC : 1;

	UPROPERTY(EditAnywhere, config, Category = Optimization, meta = (
		ConsoleVariable = "gc.CreateGCClusters", DisplayName = "Create Garbage Collector UObject Clusters",
		ToolTip = "If true, the engine will attempt to create clusters of objects for better garbage collection performance."))
	uint32 CreateGCClusters : 1;

	UPROPERTY(EditAnywhere, config, Category = Optimization, meta = (
		ConsoleVariable = "gc.MergeGCClusters", DisplayName = "Merge GC Clusters",
		ToolTip = "If true, when creating clusters, the clusters referenced from another cluster will get merged into one big cluster."))
	uint32 MergeGCClusters : 1;

	UPROPERTY(EditAnywhere, config, Category = Optimization, meta = (
		ConsoleVariable = "gc.ActorClusteringEnabled", DisplayName = "Actor Clustering Enabled",
		ToolTip = "Whether to allow levels to create actor clusters for GC."))
	uint32 ActorClusteringEnabled : 1;

	UPROPERTY(EditAnywhere, config, Category = Optimization, meta = (
		ConsoleVariable = "gc.BlueprintClusteringEnabled", DisplayName = "Blueprint Clustering Enabled",
		ToolTip = "Whether to allow Blueprint classes to create GC clusters."))
	uint32 BlueprintClusteringEnabled : 1;

	UPROPERTY(EditAnywhere, config, Category = Optimization, meta = (
		ConsoleVariable = "gc.UseDisregardForGCOnDedicatedServers", DisplayName = "Use DisregardForGC On Dedicated Servers",
		ToolTip = "If false, DisregardForGC will be disabled for dedicated servers."))
	uint32 UseDisregardForGCOnDedicatedServers : 1;

	UPROPERTY(EditAnywhere, config, Category = General, meta = (
		ConsoleVariable = "gc.NumRetriesBeforeForcingGC", DisplayName = "Number Of Retries Before Forcing GC",
		ToolTip = "Maximum number of times GC can be skipped if worker threads are currently modifying UObject state. 0 = never force GC"))
	int32 NumRetriesBeforeForcingGC;

	UPROPERTY(EditAnywhere, config, Category = Optimization, meta = (
		ConsoleVariable = "gc.MaxObjectsNotConsideredByGC", DisplayName = "Maximum Object Count Not Considered By GC",
		ToolTip = "Maximum Object Count Not Considered By GC. Works only in cooked builds."))
	int32 MaxObjectsNotConsideredByGC;

	UPROPERTY(EditAnywhere, config, Category = Optimization, meta = (
		ConsoleVariable = "gc.SizeOfPermanentObjectPool", DisplayName = "Size Of Permanent Object Pool",
		ToolTip = "Size Of Permanent Object Pool (bytes). Works only in cooked builds."))
	int32 SizeOfPermanentObjectPool;

	UPROPERTY(EditAnywhere, config, Category = Optimization, meta = (
		ConsoleVariable = "gc.MaxObjectsInGame", DisplayName = "Maximum number of UObjects that can exist in cooked game",
		ToolTip = "Maximum number of UObjects that can exist in cooked game. Keep this as small as possible."))
	int32 MaxObjectsInGame;

	UPROPERTY(EditAnywhere, config, Category = Optimization, meta = (
		ConsoleVariable = "gc.MaxObjectsInEditor", DisplayName = "Maximum number of UObjects that can exist in the editor",
		ToolTip = "Maximum number of UObjects that can exist in the editor game. Make sure this can hold enough objects for the editor and commadlets within reasonable limit."))
	int32 MaxObjectsInEditor;

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface
};
