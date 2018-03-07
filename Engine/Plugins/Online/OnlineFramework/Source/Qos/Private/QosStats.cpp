// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "QosStats.h"
#include "Misc/Guid.h"
#include "QosModule.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"

#define QOS_STATS_VERSION 1
#define DEBUG_QOS_STATS 0

// Events
const FString FQosDatacenterStats::QosStats_DatacenterEvent = TEXT("QosStats_DatacenterEvent");

// Common attribution
const FString FQosDatacenterStats::QosStats_SessionId = TEXT("QosStats_SessionId");
const FString FQosDatacenterStats::QosStats_Version = TEXT("QosStats_Version");

// Header stats
const FString FQosDatacenterStats::QosStats_Timestamp = TEXT("QosStats_Timestamp");
const FString FQosDatacenterStats::QosStats_TotalTime = TEXT("QosStats_TotalTime");

// Qos stats
const FString FQosDatacenterStats::QosStats_DeterminationType = TEXT("QosStats_DeterminationType");
const FString FQosDatacenterStats::QosStats_NumRegions = TEXT("QosStats_NumRegions");
const FString FQosDatacenterStats::QosStats_RegionDetails = TEXT("QosStats_RegionDetails");
const FString FQosDatacenterStats::QosStats_NumResults = TEXT("QosStats_NumResults");
const FString FQosDatacenterStats::QosStats_NumSuccessCount = TEXT("QosStats_NumSuccessCount");
const FString FQosDatacenterStats::QosStats_SearchDetails = TEXT("QosStats_SearchDetails");

/**
 * Debug output for the contents of a recorded stats event
 *
 * @param StatsEvent event type recorded
 * @param Attributes attribution of the event
 */
inline void PrintEventAndAttributes(const FString& StatsEvent, const TArray<FAnalyticsEventAttribute>& Attributes)
{
#if DEBUG_QOS_STATS
	UE_LOG(LogQos, Display, TEXT("Event: %s"), *StatsEvent);
	for (int32 AttrIdx = 0; AttrIdx<Attributes.Num(); AttrIdx++)
	{
		const FAnalyticsEventAttribute& Attr = Attributes[AttrIdx];
		UE_LOG(LogQos, Display, TEXT("\t%s : %s"), *Attr.AttrName, *Attr.AttrValue);
	}
#endif
}

FQosDatacenterStats::FQosDatacenterStats() :
	StatsVersion(QOS_STATS_VERSION),
	bAnalyticsInProgress(false)
{
}

void FQosDatacenterStats::StartTimer(FQosStats_Timer& Timer)
{
	Timer.MSecs = FPlatformTime::Seconds();
	Timer.bInProgress = true;
}

void FQosDatacenterStats::StopTimer(FQosStats_Timer& Timer)
{
	if (Timer.bInProgress)
	{
		Timer.MSecs = (FPlatformTime::Seconds() - Timer.MSecs) * 1000;
		Timer.bInProgress = false;
	}
}

void FQosDatacenterStats::StartQosPass()
{
	if (!bAnalyticsInProgress)
	{
		FDateTime UTCTime = FDateTime::UtcNow();
		QosData.Timestamp = UTCTime.ToString();

		StartTimer(QosData.SearchTime);
		bAnalyticsInProgress = true;
	}
}

void FQosDatacenterStats::RecordRegionInfo(const FString& Region, int32 AvgPing, int32 NumResults)
{
	if (bAnalyticsInProgress)
	{
		FQosStats_RegionInfo& NewRegion = *new (QosData.Regions) FQosStats_RegionInfo();
		NewRegion.RegionId = Region;
		NewRegion.AvgPing = AvgPing;
		NewRegion.NumResults = NumResults;
	}
}

void FQosDatacenterStats::RecordQosAttempt(const FOnlineSessionSearchResult& SearchResult, bool bSuccess)
{
	if (bAnalyticsInProgress)
	{
		QosData.NumTotalSearches++;
		QosData.NumSuccessAttempts += bSuccess ? 1 : 0;

		FQosStats_QosSearchResult& NewSearchResult = *new (QosData.SearchResults) FQosStats_QosSearchResult();
		NewSearchResult.OwnerId = SearchResult.Session.OwningUserId.IsValid() ? SearchResult.Session.OwningUserId->ToString() : TEXT("Unknown");
		NewSearchResult.PingInMs = SearchResult.PingInMs;
		NewSearchResult.bIsValid = SearchResult.IsValid();

		FString TmpRegion;
		if (SearchResult.Session.SessionSettings.Get(SETTING_REGION, TmpRegion))
		{
			NewSearchResult.DatacenterId = TmpRegion;
		}
	}
}

void FQosDatacenterStats::RecordQosAttempt(const FString& Region, const FString& OwnerId, int32 PingInMs, bool bSuccess)
{
	if (bAnalyticsInProgress)
	{
		QosData.NumTotalSearches++;
		QosData.NumSuccessAttempts += bSuccess ? 1 : 0;

		FQosStats_QosSearchResult& NewSearchResult = *new (QosData.SearchResults) FQosStats_QosSearchResult();
		NewSearchResult.OwnerId = OwnerId;
		NewSearchResult.PingInMs = PingInMs;
		NewSearchResult.DatacenterId = Region;
		NewSearchResult.bIsValid = true;
	}
}

void FQosDatacenterStats::EndQosPass(EDatacenterResultType Result)
{
	if (bAnalyticsInProgress)
	{
		Finalize();
		QosData.DeterminationType = Result;
	}
}

void FQosDatacenterStats::Finalize()
{
	StopTimer(QosData.SearchTime);
	bAnalyticsInProgress = false;
}

void FQosDatacenterStats::Upload(TSharedPtr<IAnalyticsProvider>& AnalyticsProvider)
{
	if (bAnalyticsInProgress)
	{
		Finalize();
	}

	// GUID representing the entire datacenter determination attempt
	FGuid QosStatsGuid;
	FPlatformMisc::CreateGuid(QosStatsGuid);

	ParseQosResults(AnalyticsProvider, QosStatsGuid);
}

/**
 * @EventName QosStats_DatacenterEvent
 * @Trigger Attempt to determine a user datacenter from available QoS information
 * @Type static
 * @EventParam QosStats_SessionId string Guid of this attempt
 * @EventParam QosStats_Version integer Qos analytics version
 * @EventParam QosStats_Timestamp string Timestamp when this whole attempt started
 * @EventParam QosStats_TotalTime float Total time this complete attempt took, includes delay between all ping queries (ms)
 * @EventParam QosStats_DatacenterId string Data center selected
 * @EventParam QosStats_NumRegions integer Total number of regions considered or known at the time
 * @EventParam QosStats_NumResults integer Total number of results found for consideration
 * @EventParam QosStats_NumSuccessCount integer Total number of successful ping evaluations
 * @EventParam QosStats_RegionDetails string CSV details about the regions
 * @EventParam QosStats_SearchDetails string CSV details about the individual servers queried

 * @Comments Analytics data for a complete qos datacenter determination attempt
 * 
 * @Owner Josh.Markiewicz
 */
void FQosDatacenterStats::ParseQosResults(TSharedPtr<IAnalyticsProvider>& AnalyticsProvider, FGuid& SessionId)
{
	TArray<FAnalyticsEventAttribute> QoSAttributes;

	QoSAttributes.Add(FAnalyticsEventAttribute(QosStats_SessionId, SessionId.ToString()));
	QoSAttributes.Add(FAnalyticsEventAttribute(QosStats_Version, StatsVersion));
	QoSAttributes.Add(FAnalyticsEventAttribute(QosStats_Timestamp, QosData.Timestamp));
	QoSAttributes.Add(FAnalyticsEventAttribute(QosStats_TotalTime, QosData.SearchTime.MSecs));

	QoSAttributes.Add(FAnalyticsEventAttribute(QosStats_DeterminationType, ToString(QosData.DeterminationType)));
	QoSAttributes.Add(FAnalyticsEventAttribute(QosStats_NumRegions, QosData.Regions.Num()));
	QoSAttributes.Add(FAnalyticsEventAttribute(QosStats_NumResults, QosData.NumTotalSearches));
	QoSAttributes.Add(FAnalyticsEventAttribute(QosStats_NumSuccessCount, QosData.NumSuccessAttempts));

	FString RegionDetails;
	if (QosData.Regions.Num() > 0)
	{
		for (auto& Region : QosData.Regions)
		{
			RegionDetails += FString::Printf(TEXT("{\"RegionId\":\"%s\", \"AvgPing\":\"%d\", \"NumResults\":%d},"),
				!Region.RegionId.IsEmpty() ? *Region.RegionId : TEXT("Unknown"),
				Region.AvgPing,
				Region.NumResults
				);
		}
		RegionDetails = RegionDetails.LeftChop(1);
	}

	QoSAttributes.Add(FAnalyticsEventAttribute(QosStats_RegionDetails, RegionDetails));

	FString SearchDetails;
	if (QosData.SearchResults.Num() > 0)
	{
		for (auto& SearchResult : QosData.SearchResults)
		{
			SearchDetails += FString::Printf(TEXT("{\"OwnerId\":\"%s\", \"RegionId\":\"%s\", \"PingInMs\":%d, \"bIsValid\":%s},"),
				*SearchResult.OwnerId,
				*SearchResult.DatacenterId,
				SearchResult.PingInMs,
				SearchResult.bIsValid ? TEXT("true") : TEXT("false")
				);
		}
		SearchDetails = SearchDetails.LeftChop(1);
	}
	QoSAttributes.Add(FAnalyticsEventAttribute(QosStats_SearchDetails, SearchDetails));

	PrintEventAndAttributes(QosStats_DatacenterEvent, QoSAttributes);
	if (AnalyticsProvider.IsValid())
	{
		AnalyticsProvider->RecordEvent(QosStats_DatacenterEvent, QoSAttributes);
	}
}
