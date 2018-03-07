// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "OnlineSessionSettings.h"

#include "QosRegionManager.generated.h"

class IAnalyticsProvider;
class UQosEvaluator;

#define UNREACHABLE_PING 9999

/** Enum for single region QoS return codes */
UENUM()
enum class EQosRegionResult : uint8
{
	/** Incomplete, invalid result */
	Invalid,
	/** QoS operation was successful */
	Success,
	/** QoS operation with one or more ping failures */
	Incomplete
};

/** Enum for possible QoS return codes */
UENUM()
enum class EQosCompletionResult : uint8
{
	/** Incomplete, invalid result */
	Invalid,
	/** QoS operation was successful */
	Success,
	/** QoS operation ended in failure */
	Failure,
	/** QoS operation was canceled */
	Canceled
};

/**
 * Individual ping server details
 */
USTRUCT()
struct FQosPingServerInfo
{
	GENERATED_USTRUCT_BODY()

	/** Address of server */
	UPROPERTY()
	FString Address;
	/** Port of server */
	UPROPERTY()
	int32 Port;
};

/**
 * Metadata about datacenters that can be queried
 */
USTRUCT()
struct FQosDatacenterInfo
{
	GENERATED_USTRUCT_BODY()

	/** Localized name of the datacenter */
	UPROPERTY()
	FText DisplayName;
	/** RegionId for this datacenter */
	UPROPERTY()
	FString RegionId;
	/** Is this region tested */
	UPROPERTY()
	bool bEnabled;
	/** Is this region visible in the UI */
	UPROPERTY()
	bool bVisible;
	/** Is this region "beta" */
	UPROPERTY()
	bool bBeta;
	/** Addresses of ping servers */
	UPROPERTY()
	TArray<FQosPingServerInfo> Servers;

	FQosDatacenterInfo()
		: RegionId()
		, bEnabled(true)
		, bVisible(true)
		, bBeta(false)
	{
	}

	bool IsPingable() const
	{
		return !RegionId.IsEmpty() && bEnabled;
	}

	bool IsUsable() const
	{
		return IsPingable() && bVisible;
	}
};

/** Runtime information about a given region */
USTRUCT()
struct QOS_API FQosRegionInfo
{
	GENERATED_USTRUCT_BODY()

	/** Information about the region */
	UPROPERTY(Transient)
	FQosDatacenterInfo Region;

	/** Success of the qos evaluation */
	UPROPERTY(Transient)
	EQosRegionResult Result;
	/** Avg ping times across all search results */
	UPROPERTY(Transient)
	int32 AvgPingMs;

	/** Transient list of search results for a given region */
	TArray<FOnlineSessionSearchResult> SearchResults;
	/** Transient list of ping times for the above search results */
	UPROPERTY(Transient)
	TArray<int32> PingResults;
	/** Number of good results */
	int32 NumResponses;
	/** Last time this datacenter was checked */
	UPROPERTY(Transient)
	FDateTime LastCheckTimestamp;

	FQosRegionInfo()
		: Result(EQosRegionResult::Invalid)
		, AvgPingMs(UNREACHABLE_PING)
		, NumResponses(0)
		, LastCheckTimestamp(0)
	{
	}

	FQosRegionInfo(const FQosDatacenterInfo& InMeta)
		: Region(InMeta)
		, Result(EQosRegionResult::Invalid)
		, AvgPingMs(UNREACHABLE_PING)
		, NumResponses(0)
		, LastCheckTimestamp(0)
	{
	}

	/** @return if this region data is usable externally */
	bool IsUsable() const
	{
		return Region.IsUsable();
	}

	/** reset the data to its default state */
	void Reset()
	{
		// Only the transient values get reset
		Result = EQosRegionResult::Invalid;
		AvgPingMs = UNREACHABLE_PING;
		SearchResults.Empty();
		PingResults.Empty();
		LastCheckTimestamp = FDateTime(0);
	}
};

/**
 * Generic settings a server runs when hosting a simple QoS response service
 */
class QOS_API FOnlineSessionSettingsQos : public FOnlineSessionSettings
{
public:

	FOnlineSessionSettingsQos(bool bInIsDedicated = true);
	virtual ~FOnlineSessionSettingsQos() {}
};

/**
 * Main Qos interface for actions related to server quality of service
 */
UCLASS(config = Engine)
class QOS_API UQosRegionManager : public UObject
{

	GENERATED_UCLASS_BODY()

public:

	/**
	 * Start running the async QoS evaluation 
	 */
	void BeginQosEvaluation(UWorld* World, const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider, const FSimpleDelegate& OnComplete);

	/**
	 * Get the region ID for this instance, checking ini and commandline overrides.
	 * 
	 * Dedicated servers will have this value specified on the commandline
	 * 
	 * Clients pull this value from the settings (or command line) and do a ping test to determine if the setting is viable.
	 *
	 * @return the default region identifier
	 */
	FString GetRegionId() const;

	/**
	 * Get the list of regions that the client can choose from (returned from search and must meet min ping requirements)
	 *
	 * If this list is empty, the client cannot play.
	 */
	const TArray<FQosRegionInfo>& GetRegionOptions() const;

	/**
	 * Try to set the selected region ID (must be present in GetRegionOptions)
	 *
	 * @param bForce if true then use selected region even if QoS eval has not completed successfully
	 */
	bool SetSelectedRegion(const FString& RegionId, bool bForce=false);

	/**
	 * Force the selected region creating a fake RegionOption if necessary
	 */
	void ForceSelectRegion(const FString& RegionId);

	/**
	 * Get the datacenter id for this instance, checking ini and commandline overrides
	 * This is only relevant for dedicated servers (so they can advertise). Client does 
	 * not search on this (but may choose to prioritize results later)
	 *
	 * @return the default datacenter identifier
	 */
	static FString GetDatacenterId();

    /** @return true if a reasonable enough number of results were returned from all known regions, false otherwise */
	bool AllRegionsFound() const;

	/**
	 * Debug output for current region / datacenter information
	 */
	void DumpRegionStats();

public:

	/** Begin UObject interface */
	virtual void PostReloadConfig(class UProperty* PropertyThatWasLoaded) override;
	/** End UObject interface */

private:

	void OnQosEvaluationComplete(EQosCompletionResult Result, const TArray<FQosRegionInfo>& RegionInfo);

	/**
	 * Use the existing set value, or if it is currently invalid, set the next best region available
	 */
	void TrySetDefaultRegion();

	/**
	 * @return max allowable ping to any region and still be allowed to play
	 */
	int32 GetMaxPingMs() const;

	/** Use old server method */
	UPROPERTY(Config)
	bool bUseOldQosServers;

	/** Number of times to ping a given region using random sampling of available servers */
	UPROPERTY(Config)
	int32 NumTestsPerRegion;

	/** Timeout value for each ping request */
	UPROPERTY(Config)
	float PingTimeout;

	/** Expected datacenters metadata */
	UPROPERTY(Config)
	TArray<FQosDatacenterInfo> Datacenters;

	UPROPERTY()
	FDateTime LastCheckTimestamp;

	/** Reference to the evaluator for making datacenter determinations (null when not active) */
	UPROPERTY()
	UQosEvaluator* Evaluator;
	/** Result of the last datacenter test */
	UPROPERTY()
	EQosCompletionResult QosEvalResult;
	/** Array of all known datacenters and their status */
	UPROPERTY()
	TArray<FQosRegionInfo> RegionOptions;

	/** Value forced to be the region (development) */
	UPROPERTY()
	FString ForceRegionId;
	/** Value set by the game to be the current region */
	UPROPERTY()
	FString SelectedRegionId;

	TArray<FSimpleDelegate> OnQosEvalCompleteDelegate;
};

