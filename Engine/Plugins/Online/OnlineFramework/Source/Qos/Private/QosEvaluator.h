// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "OnlineSessionSettings.h"
#include "QosRegionManager.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "QosEvaluator.generated.h"

class AQosBeaconClient;
class FQosDatacenterStats;
class FTimerManager;
class IAnalyticsProvider;
struct FIcmpEchoResult;
enum class EQosResponseType : uint8;

#define GAMEMODE_QOS	TEXT("QOSSERVER")

/**
 * Search settings for QoS advertised sessions
 */
class QOS_API FOnlineSessionSearchQos : public FOnlineSessionSearch
{
public:
	FOnlineSessionSearchQos();
	virtual ~FOnlineSessionSearchQos() {}

	// FOnlineSessionSearch Interface begin
	virtual TSharedPtr<FOnlineSessionSettings> GetDefaultSessionSettings() const override;
	virtual void SortSearchResults() override;
	// FOnlineSessionSearch Interface end
};

/**
 * Internal state for a given QoS pass
 */
USTRUCT()
struct FQosSearchPass
{
	GENERATED_USTRUCT_BODY()

	/** Current Region index in the datacenter array */
	UPROPERTY()
	int32 RegionIdx;
	/** Current search result choice to test */
	UPROPERTY()
	int32 CurrentSessionIdx;
	/** Current array of QoS search results to evaluate */
	//TArray<FOnlineSessionSearchResult> SearchResults;

	FQosSearchPass()
		: RegionIdx(INDEX_NONE)
		, CurrentSessionIdx(INDEX_NONE)
	{}
};

/**
 * Input parameters to start a qos ping check
 */
struct FQosParams
{
	/** User that initiated the request */
	int32 ControllerId;
	/** Use the old qos beacon method for ping determination */
	bool bUseOldQosServers;
	/** Number of ping requests per region */
	int32 NumTestsPerRegion;
	/** Amount of time to wait for each request */
	float Timeout;
};

/*
 * Delegate triggered when an evaluation of ping for all servers in a search query have completed
 *
 * @param Result the ping operation result
 */
DECLARE_DELEGATE_OneParam(FOnQosPingEvalComplete, EQosCompletionResult /** Result */);

/** 
 * Delegate triggered when all QoS search results have been investigated 
 *
 * @param Result the QoS operation result
 * @param RegionInfo The per-region ping information
 */
DECLARE_DELEGATE_TwoParams(FOnQosSearchComplete, EQosCompletionResult /** Result */, const TArray<FQosRegionInfo>& /** RegionInfo */);

/**
 * Evaluates QoS metrics to determine the best datacenter under current conditions
 * Additionally capable of generically pinging an array of servers that have a QosBeaconHost active
 */
UCLASS(config = Engine)
class QOS_API UQosEvaluator : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * QoS services
	 */

	/**
	 * Find all the advertised datacenters and begin the process of evaluating ping results
	 * Will return the default datacenter in the event of failure or no advertised datacenters
	 *
	 * @param InParams parameters defining the request
	 * @param InDatacenters array of datacenters to query
	 * @param InCompletionDelegate delegate to fire when a datacenter choice has been made
	 */
	void FindDatacenters(const FQosParams& InParams, TArray<FQosDatacenterInfo>& InDatacenters, const FOnQosSearchComplete& InCompletionDelegate);

	/**
	 * Is a QoS operation active
	 *
	 * @return true if QoS is active, false otherwise
	 */
	bool IsActive() const { return bInProgress; }

	/**
	 * Cancel the current QoS operation at the earliest opportunity
	 */
	void Cancel();

	void SetWorld(UWorld* InWorld);

protected:

	/**
	 * Use the udp ping code to ping known servers
	 *
	 * @param InParams parameters defining the request
	 * @param InCompletionDelegate delegate to fire when all regions have completed their tests
	 */
	void PingRegionServers(const FQosParams& InParams, const FOnQosSearchComplete& InCompletionDelegate);

	/**
	 * Given a single region id, find available qos servers
	 * 
	 * @param RegionIdx region index in the array
	 * @param InCompletionDelegate delegate to fire when complete
	 */
	bool FindQosServersByRegion(int32 RegionIdx, FOnQosSearchComplete InCompletionDelegate);

	/**
	 * Start pinging all servers found across all regions, starting with the first datacenter
	 *
	 * @param InCompletionDelegate delegate to fire when complete
	 */
	void StartServerPing(const FOnQosPingEvalComplete& InCompletionDelegate);

	/**
	 * Evaluate ping reachability for a given list of servers.  Assumes there is a AQosBeaconHost on the machine
	 * to receive the request and send a response
	 *
	 * @param RegionIdx region to evaluate qos servers 
	 */
	void EvaluateRegionPing(int32 RegionIdx);

private:

	/** Current QoS search/eval state */
	UPROPERTY()
	FQosSearchPass CurrentSearchPass;

	/** Reference to external UWorld */
	TWeakObjectPtr<UWorld> ParentWorld;

	/** QoS search results */
	TSharedPtr<FOnlineSessionSearch> QosSearchQuery;

	/** Delegate for finding datacenters */
	FOnFindSessionsCompleteDelegate OnQosSessionsSearchCompleteDelegate;
	FOnQosPingEvalComplete OnQosPingEvalComplete;

	/** Beacon for sending QoS requests */
	TWeakObjectPtr<AQosBeaconClient> QosBeaconClient;

	/** User initiating the request */
	UPROPERTY()
	int32 ControllerId;
	/** A QoS operation is in progress */
	UPROPERTY()
	bool bInProgress;
	/** Should cancel occur at the next available opportunity */
	UPROPERTY()
	bool bCancelOperation;

	/** Array of datacenters currently being evaluated */
	UPROPERTY(Transient)
	TArray<FQosRegionInfo> Datacenters;

	/**
	 * Signal QoS operation has completed next frame
	 *
	 * @param InCompletionDelegate the delegate to trigger
	 * @param CompletionResult the QoS operation result
	 */
	void FinalizeDatacenterResult(const FOnQosSearchComplete& InCompletionDelegate, EQosCompletionResult CompletionResult, const TArray<FQosRegionInfo>& RegionInfo);

	/**
	 * Continue with the next datacenter endpoint and gather its ping result
	 */
	void ContinuePingRegion();

	/**
	 * Finalize the search results from pinging various datacenter endpoints to find the best datacenter
	 *
	 * @param Result the QoS operation result
	 */
	void FinalizePingServers(EQosCompletionResult Result);

	/**
	 * Delegate fired when all servers within a given search query have been evaluated
	 *
	 * @param Result the QoS operation result
	 * @param InCompletionDelegate the datacenter complete delegate to fire
	 */
	void OnEvaluateForDatacenterComplete(EQosCompletionResult Result, FOnQosSearchComplete InCompletionDelegate);

	/**
	 * Delegate fired when a datacenter search query has completed
	 *
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 * @param RegionId region search performed
	 * @param InCompletionDelegate the datacenter complete delegate to fire
	 */
	void OnFindQosServersByRegionComplete(bool bWasSuccessful, int32 RegionIdx, FOnQosSearchComplete InCompletionDelegate);

	/**
	 * Delegate from the QoS beacon fired with each beacon response
	 *
	 * @param QosResponse the response from the server, success or otherwise
	 * @param ResponseTime the time to complete the request, or MAX_QUERY_PING if unsuccessful
	 */
	void OnQosRequestComplete(EQosResponseType QosResponse, int32 ResponseTime);

	/**
	 * Delegate from the QoS beacon fired when there is a failure to connect to an endpoint
	 */
	void OnQosConnectionFailure();

	/**
	 * @return true if all ping requests have completed (new method)
	 */
	bool AreAllRegionsComplete();

	void OnPingResultComplete(const FString& RegionId, int32 NumTests, const FIcmpEchoResult& Result);

	/**
	 * Take all found ping results and process them before consumption at higher levels
	 *
	 * @param TimeToDiscount amount of time to subtract from calculation to compensate for external factors (frame rate, etc)
	 */
	void CalculatePingAverages(int32 TimeToDiscount = 0);

private:

	FDelegateHandle OnFindDatacentersCompleteDelegateHandle;

public:

	/**
	 * Analytics
	 */

	void SetAnalyticsProvider(TSharedPtr<IAnalyticsProvider> InAnalyticsProvider);

private:

	void StartAnalytics();
	void EndAnalytics(EQosCompletionResult CompletionResult);

	/** Reference to the provider to submit data to */
	TSharedPtr<IAnalyticsProvider> AnalyticsProvider;
	/** Stats related to these operations */
	TSharedPtr<FQosDatacenterStats> QosStats;

private:

	/**
	 * Helpers
	 */

	int32 GetNextRegionId(int32 LastRegionId = INDEX_NONE);

	/** Reset all variables related to the QoS evaluator */
	void ResetSearchVars();

	/** Quick access to the current world */
	UWorld* GetWorld() const;

	/** Quick access to the world timer manager */
	FTimerManager& GetWorldTimerManager() const;

	/** Client beacon cleanup */
	void DestroyClientBeacons();
};

inline const TCHAR* ToString(EQosRegionResult Result)
{
	switch (Result)
	{
		case EQosRegionResult::Invalid:
		{
			return TEXT("Invalid");
		}
		case EQosRegionResult::Success:
		{
			return TEXT("Success");
		}
		case EQosRegionResult::Incomplete:
		{
			return TEXT("Incomplete");
		}
	}

	return TEXT("");
}

inline const TCHAR* ToString(EQosCompletionResult Result)
{
	switch (Result)
	{
		case EQosCompletionResult::Invalid:
		{
			return TEXT("Invalid");
		}
		case EQosCompletionResult::Success:
		{
			return TEXT("Success");
		}
		case EQosCompletionResult::Failure:
		{
			return TEXT("Failure");
		}
		case EQosCompletionResult::Canceled:
		{
			return TEXT("Canceled");
		}
	}

	return TEXT("");
}
