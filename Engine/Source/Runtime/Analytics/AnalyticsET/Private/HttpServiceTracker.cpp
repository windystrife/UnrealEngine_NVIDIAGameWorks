// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HttpServiceTracker.h"
#include "HAL/PlatformTime.h"
#include "AnalyticsEventAttribute.h"
#include "IAnalyticsProviderET.h"
#include "AnalyticsET.h"
#include "Interfaces/IHttpResponse.h"
#include "Analytics.h"

bool FHttpServiceTracker::Tick(float DeltaTime)
{
	// flush events at the specified interval.
	if (FPlatformTime::Seconds() > NextFlushTime)
	{
		if (AnalyticsProvider.IsValid())
		{
			TArray<FAnalyticsEventAttribute> Attrs;
			Attrs.Reserve(10);
			// one event per endpoint.
			for (const auto& MetricsMapPair : EndpointMetricsMap)
			{
				Attrs.Reset();
				Attrs.Emplace(TEXT("DomainName"), MetricsMapPair.Value.LastAnalyticsName);
				Attrs.Emplace(TEXT("FailCount"), MetricsMapPair.Value.FailCount);
				Attrs.Emplace(TEXT("SuccessCount"), MetricsMapPair.Value.SuccessCount);
				// We may have had no successful requests, so these values would be undefined.
				if (MetricsMapPair.Value.SuccessCount > 0)
				{
					Attrs.Emplace(TEXT("DownloadBytesSuccessTotal"), MetricsMapPair.Value.DownloadBytesSuccessTotal);
					Attrs.Emplace(TEXT("ElapsedTimeSuccessTotal"), MetricsMapPair.Value.ElapsedTimeSuccessTotal);
					Attrs.Emplace(TEXT("ElapsedTimeSuccessMin"), MetricsMapPair.Value.ElapsedTimeSuccessMin);
					Attrs.Emplace(TEXT("ElapsedTimeSuccessMax"), MetricsMapPair.Value.ElapsedTimeSuccessMax);
				}
				if (MetricsMapPair.Value.FailCount > 0)
				{
					Attrs.Emplace(TEXT("DownloadBytesFailTotal"), MetricsMapPair.Value.DownloadBytesFailTotal);
					Attrs.Emplace(TEXT("ElapsedTimeFailTotal"), MetricsMapPair.Value.ElapsedTimeFailTotal);
					Attrs.Emplace(TEXT("ElapsedTimeFailMin"), MetricsMapPair.Value.ElapsedTimeFailMin);
					Attrs.Emplace(TEXT("ElapsedTimeFailMax"), MetricsMapPair.Value.ElapsedTimeFailMax);
				}
				// one attribute per response code.
				for (const auto& ResponseCodeMapPair : MetricsMapPair.Value.ResponseCodes)
				{
					Attrs.Emplace(FString(TEXT("Code-")) + Lex::ToString(ResponseCodeMapPair.Key), ResponseCodeMapPair.Value);
				}
				AnalyticsProvider->RecordEvent(MetricsMapPair.Key.ToString(), Attrs);
			}
			// force an immediate flush always. We already summarized.
			AnalyticsProvider->FlushEvents();
		}
		EndpointMetricsMap.Reset();
		NextFlushTime += FlushIntervalSec;
	}
	return true;
}

void FHttpServiceTracker::TrackRequest(const FHttpRequestPtr& Request, FName EndpointName)
{
	auto& Metrics = EndpointMetricsMap.FindOrAdd(EndpointName);
	Metrics.TrackRequest(Request);
}

FHttpServiceTracker::FHttpServiceTracker(const FHttpServiceTrackerConfig& Config) 
	: FlushIntervalSec((float)Config.AggregationInterval.GetTotalSeconds())
	, NextFlushTime(FPlatformTime::Seconds() + FlushIntervalSec)
{
	AnalyticsProvider = FAnalyticsET::Get().CreateAnalyticsProvider(FAnalyticsET::Config(Config.APIKey, Config.APIServer, Config.ApiVersion, false, TEXT("unknown"), TEXT("qosmetrics")));
	// Use the standard UserID
	AnalyticsProvider->SetUserID(FString::Printf(TEXT("%s|%s|%s"), *FPlatformMisc::GetLoginId(), *FPlatformMisc::GetEpicAccountId(), *FPlatformMisc::GetOperatingSystemId()));
	// Note we also don't start/stop the session. The AnalyticsET provider allows this, and this enables our collector
	// to receive ONLY monitoring events.
}

bool FHttpServiceTracker::EndpointMetrics::IsSuccessfulResponse(int32 ResponseCode)
{
	return ResponseCode >= 200 && ResponseCode < 400;
}

namespace
{
	/**
	 * @brief Returns name of the endpoint for analytics.
	 *
	 * @param FullURL actual URL used
	 * @return Name for analytics (currently this is a domain name).
	 */
	FString GetAnalyticsName(const FString & FullURL)
	{
		// use the first part of address
		int DomainNameBegin = FullURL.Find(TEXT("://"), ESearchCase::CaseSensitive);

		if (DomainNameBegin == INDEX_NONE)
		{
			UE_LOG(LogAnalytics, Warning, TEXT("Could not find protocol in URL '%s', analytics name will likely be incorrect"), *FullURL);
			return FullURL;
		}

		DomainNameBegin += 3;	// length of "://"

		int DomainNameEnd = FullURL.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, DomainNameBegin);

		if (DomainNameEnd == INDEX_NONE || DomainNameEnd <= DomainNameBegin)
		{
			UE_LOG(LogAnalytics, Warning, TEXT("Could not determine domain name in URL '%s', analytics name will likely be incorrect"), *FullURL);
			return FullURL;
		}

		return FullURL.Mid(DomainNameBegin, DomainNameEnd - DomainNameBegin);
	}
}


void FHttpServiceTracker::EndpointMetrics::TrackRequest(const FHttpRequestPtr& HttpRequest)
{
	if(HttpRequest.IsValid())
	{
		// keep a histogram of response codes
		const FHttpResponsePtr HttpResponse = HttpRequest->GetResponse();

		const int32 ResponseCode = HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : 0;
		// track all responses in a histogram
		ResponseCodes.FindOrAdd(ResponseCode)++;
		const float ElapsedTime = HttpRequest->GetElapsedTime();
		const int64 DownloadBytes = HttpResponse.IsValid() ? HttpResponse->GetContent().Num() : 0;
		// track successes/fails separately
		if (IsSuccessfulResponse(ResponseCode))
		{
			++SuccessCount;
			// sum elapsed time for average calc
			ElapsedTimeSuccessTotal += ElapsedTime;
			ElapsedTimeSuccessMax = FMath::Max(ElapsedTimeSuccessMax, ElapsedTime);
			ElapsedTimeSuccessMin = FMath::Min(ElapsedTimeSuccessMin, ElapsedTime);
			// sum download rate for average calc
			DownloadBytesSuccessTotal += DownloadBytes;
		}
		else
		{
			++FailCount;
			// sum elapsed time for average calc
			ElapsedTimeFailTotal += ElapsedTime;
			ElapsedTimeFailMax = FMath::Max(ElapsedTimeFailMax, ElapsedTime);
			ElapsedTimeFailMin = FMath::Min(ElapsedTimeFailMin, ElapsedTime);
			// sum download rate for average calc
			DownloadBytesFailTotal += DownloadBytes;
		}

		FString AnalyticsName = GetAnalyticsName(HttpRequest->GetURL());
		if (LastAnalyticsName.Len() > 0 && AnalyticsName != LastAnalyticsName)
		{
			UE_LOG(LogAnalytics, Warning, TEXT("Endpoint analytics name has changed from '%s' to '%s', aggregated stats will be incorrect"),
				*LastAnalyticsName, *AnalyticsName);
		}

		LastAnalyticsName = AnalyticsName;
	}
}

FHttpServiceTracker::EndpointMetrics::EndpointMetrics() : DownloadBytesSuccessTotal(0L)
, ElapsedTimeSuccessTotal(0.0f)
, ElapsedTimeSuccessMin(FLT_MAX)
, ElapsedTimeSuccessMax(-FLT_MAX)
, DownloadBytesFailTotal(0L)
, ElapsedTimeFailTotal(0.0f)
, ElapsedTimeFailMin(FLT_MAX)
, ElapsedTimeFailMax(-FLT_MAX)
, SuccessCount(0)
, FailCount(0)
{

}
