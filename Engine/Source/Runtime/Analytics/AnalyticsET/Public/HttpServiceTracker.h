// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Interfaces/IHttpRequest.h"

class IAnalyticsProviderET;

/**
 * Struct used for configuring an FHttpServiceTracker instance.
 * The service tracker creates an AnalyticsProviderET under the hood,
 * so it needs to configure that along with details about the tracker itself.
 */
class FHttpServiceTrackerConfig
{
public:
	/** Matches APIKey for the ET analytics provider. */
	FString APIKey;
	/** Matches APIServer for the ET analytics provider. */
	FString APIServer;
	/** Matches APIVersion for the ET analytics provider. */
	FString ApiVersion;
	/** interval to aggregate HTTP request metrics before dumping them to the configured analytics provider. */
	FTimespan AggregationInterval;
};

/**
 * Enables monitoring of HTTP services so end-user experience can be tracked.
 * This allows us to get end-user insight into perceived availability of HTTP services beyond
 * the tracking that may be done internal to the datacenter.
 * 
 * Periodically flushes a summary of all requests to an external data collector.
 */
class ANALYTICSET_API FHttpServiceTracker : FTickerObjectBase
{
public:
	/**
	 * Ctor to initialize the tracker. Will create an AnalyticsProviderET under the hood to do the tracking.
	 * 
	 * @param Config the configuration parameters for the instance.
	 */
	FHttpServiceTracker(const FHttpServiceTrackerConfig& Config);

	/**
	 * Tracks an HTTP request.
	 * 
	 * @param Request the request being tracked.
	 * @param EndpointName the endpoint name to track (used as a key to distinguish requests for a particular subsystem or API.
	 */
	void TrackRequest(const FHttpRequestPtr& Request, FName EndpointName);

private:
	/**
	 * Internal class used to aggregate metrics for a particular Endpoint.
	 * Tracks success error codes separately from failures so the analytics can report on them separately.
	 * Also contains a sideband histogram of response code counts to know how commonly they are being hit.
	 */
	class ANALYTICSET_API EndpointMetrics
	{
	public:
		EndpointMetrics();

		/** Tracks a HTTP request. */
		void TrackRequest(const FHttpRequestPtr& HttpRequest);

		/** Determine if an HTTP response code is "valid". We only track successful responses from our server, as we don't want to mix latency with server errors. */
		bool IsSuccessfulResponse(int32 ResponseCode);

		int64 DownloadBytesSuccessTotal;
		float ElapsedTimeSuccessTotal;
		float ElapsedTimeSuccessMin;
		float ElapsedTimeSuccessMax;
		int64 DownloadBytesFailTotal;
		float ElapsedTimeFailTotal;
		float ElapsedTimeFailMin;
		float ElapsedTimeFailMax;
		int32 SuccessCount;
		int32 FailCount;
		TMap<int32, int32> ResponseCodes;
		FString LastAnalyticsName;
	};

	/** Mapping of a service endpoint to summary metrics about it. */
	TMap<FName, EndpointMetrics> EndpointMetricsMap;
	/** The analytics provider we will user to send the summary metrics. */
	TSharedPtr<IAnalyticsProviderET> AnalyticsProvider;
	/** The interval used to summarize enpoint metrics. */
	const float FlushIntervalSec;
	/** Tracks the next system time when we will flush. */
	float NextFlushTime;

	/** Ticker function used to flush endpoint summary metrics at a specified interval. */
	virtual bool Tick(float DeltaTime) override;
};

