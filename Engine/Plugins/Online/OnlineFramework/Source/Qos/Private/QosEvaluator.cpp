// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "QosEvaluator.h"
#include "TimerManager.h"
#include "QosModule.h"
#include "QosBeaconClient.h"
#include "QosStats.h"
#include "OnlineSubsystemUtils.h"
#include "Icmp.h"

FOnlineSessionSearchQos::FOnlineSessionSearchQos()
{
	bIsLanQuery = false;
	MaxSearchResults = 100;

	FString GameModeStr(GAMEMODE_QOS);
	QuerySettings.Set(SETTING_GAMEMODE, GameModeStr, EOnlineComparisonOp::Equals);
	QuerySettings.Set(SETTING_QOS, 1, EOnlineComparisonOp::Equals);
}

TSharedPtr<FOnlineSessionSettings> FOnlineSessionSearchQos::GetDefaultSessionSettings() const
{
	return MakeShareable(new FOnlineSessionSettingsQos());
}

void FOnlineSessionSearchQos::SortSearchResults()
{
	TMap<FString, int32> RegionCounts;

	static const int32 MaxPerRegion = 5;

	UE_LOG(LogQos, Verbose, TEXT("Sorting QoS results"));
	for (int32 SearchResultIdx = 0; SearchResultIdx < SearchResults.Num();)
	{
		FString QosRegion;
		FOnlineSessionSearchResult& SearchResult = SearchResults[SearchResultIdx];

		if (!SearchResult.Session.SessionSettings.Get(SETTING_REGION, QosRegion) || QosRegion.IsEmpty())
		{
			UE_LOG(LogQos, Verbose, TEXT("Removed Qos search result, invalid region."));
			SearchResults.RemoveAtSwap(SearchResultIdx);
			continue;
		}

		int32& ResultCount = RegionCounts.FindOrAdd(QosRegion);
		ResultCount++;
		if (ResultCount > MaxPerRegion)
		{
			SearchResults.RemoveAtSwap(SearchResultIdx);
			continue;
		}

		SearchResultIdx++;
	}

	for (auto& It : RegionCounts)
	{
		UE_LOG(LogQos, Verbose, TEXT("Region: %s Count: %d"), *It.Key, It.Value);
	}
}

UQosEvaluator::UQosEvaluator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ParentWorld(nullptr)
	, QosSearchQuery(nullptr)
	, QosBeaconClient(nullptr)
	, bInProgress(false)
	, bCancelOperation(false)
	, AnalyticsProvider(nullptr)
	, QosStats(nullptr)
{
}

void UQosEvaluator::SetWorld(UWorld* InWorld)
{
	ParentWorld = InWorld;
}

void UQosEvaluator::SetAnalyticsProvider(TSharedPtr<IAnalyticsProvider> InAnalyticsProvider)
{
	AnalyticsProvider = InAnalyticsProvider;
}

void UQosEvaluator::Cancel()
{
	bCancelOperation = true;
}

void UQosEvaluator::FindDatacenters(const FQosParams& InParams, TArray<FQosDatacenterInfo>& InDatacenters, const FOnQosSearchComplete& InCompletionDelegate)
{
	if (!bInProgress)
	{
		bInProgress = true;
		ControllerId = InParams.ControllerId;

		double CurTimestamp = FPlatformTime::Seconds();

		StartAnalytics();

		Datacenters.Empty(InDatacenters.Num());
		for (const FQosDatacenterInfo& Datacenter : InDatacenters)
		{
			if (Datacenter.IsPingable())
			{
				new (Datacenters) FQosRegionInfo(Datacenter);
			}
			else
			{
				UE_LOG(LogQos, Verbose, TEXT("Skipping region [%s]"), *Datacenter.RegionId);
			}
		}

		if (!InParams.bUseOldQosServers)
		{
			// Ping list of known servers defined by config
			PingRegionServers(InParams, InCompletionDelegate);
		}
		else
		{
			// Discover servers automatically
			if (!FindQosServersByRegion(GetNextRegionId(), InCompletionDelegate))
			{
				// Failed to start
				TWeakObjectPtr<UQosEvaluator> WeakThisCap(this);
				GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([InCompletionDelegate, WeakThisCap]()
				{
					if (WeakThisCap.IsValid())
					{
						auto StrongThis = WeakThisCap.Get();
						StrongThis->FinalizeDatacenterResult(InCompletionDelegate, EQosCompletionResult::Failure, TArray<FQosRegionInfo>());
					}
				}));
			}
		}
	}
	else
	{
		UE_LOG(LogQos, Log, TEXT("Qos evaluation already in progress, ignoring"));
		// Just trigger delegate now (Finalize resets state vars)
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([InCompletionDelegate]() {
			InCompletionDelegate.ExecuteIfBound(EQosCompletionResult::Failure, TArray<FQosRegionInfo>());
		}));
	}
}

void UQosEvaluator::FinalizeDatacenterResult(const FOnQosSearchComplete& InCompletionDelegate, EQosCompletionResult CompletionResult, const TArray<FQosRegionInfo>& RegionInfo)
{
	UE_LOG(LogQos, Log, TEXT("Datacenter evaluation complete. Result: %s "), ToString(CompletionResult));

	EndAnalytics(CompletionResult);

	// Broadcast this data next frame
	TWeakObjectPtr<UQosEvaluator> WeakThisCap(this);
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([InCompletionDelegate, CompletionResult, RegionInfo, WeakThisCap]()
	{
		InCompletionDelegate.ExecuteIfBound(CompletionResult, RegionInfo);
		if (WeakThisCap.IsValid())
		{
			auto StrongThis = WeakThisCap.Get();
			StrongThis->ResetSearchVars();
		}
	}));
}

void UQosEvaluator::PingRegionServers(const FQosParams& InParams, const FOnQosSearchComplete& InCompletionDelegate)
{
	// Failsafe for bad configuration
	bool bDidNothing = true;

	const int32 NumTestsPerRegion = InParams.NumTestsPerRegion;

	TWeakObjectPtr<UQosEvaluator> WeakThisCap(this);
	for (FQosRegionInfo& Region : Datacenters)
	{
		if (Region.Region.bEnabled)
		{
			const FString& RegionId = Region.Region.RegionId;
			const int32 NumServers = Region.Region.Servers.Num();
			int32 ServerIdx = FMath::RandHelper(NumServers);
			// Default to invalid ping tests and set it to something else later
			Region.Result = EQosRegionResult::Invalid;
			if (NumServers > 0)
			{
				for (int32 PingIdx = 0; PingIdx < NumTestsPerRegion; PingIdx++)
				{
					const FQosPingServerInfo& Server = Region.Region.Servers[ServerIdx];
					const FString Address = FString::Printf(TEXT("%s:%d"), *Server.Address, Server.Port);

					auto CompletionDelegate = [WeakThisCap, RegionId, NumTestsPerRegion, InCompletionDelegate](FIcmpEchoResult InResult)
					{
						if (WeakThisCap.IsValid())
						{
							auto StrongThis = WeakThisCap.Get();
							StrongThis->OnPingResultComplete(RegionId, NumTestsPerRegion, InResult);
							if (StrongThis->AreAllRegionsComplete())
							{
								EQosCompletionResult TotalResult = EQosCompletionResult::Success;
								StrongThis->CalculatePingAverages();
								StrongThis->EndAnalytics(TotalResult);
								InCompletionDelegate.ExecuteIfBound(TotalResult, StrongThis->Datacenters);
								StrongThis->bInProgress = false;
							}
						}
					};

					UE_LOG(LogQos, Verbose, TEXT("Pinging [%s] %s"), *RegionId, *Address);
					FUDPPing::UDPEcho(Address, InParams.Timeout, CompletionDelegate);
					ServerIdx = (ServerIdx + 1) % NumServers;
					bDidNothing = false;
				}
			}
			else
			{
				UE_LOG(LogQos, Verbose, TEXT("Nothing to ping [%s]"), *RegionId);
			}
		}
		else
		{
			UE_LOG(LogQos, Verbose, TEXT("Region disabled [%s]"), *Region.Region.RegionId);
		}
	}

	if (bDidNothing)
	{
		InCompletionDelegate.ExecuteIfBound(EQosCompletionResult::Failure, Datacenters);
	}
}

bool UQosEvaluator::FindQosServersByRegion(int32 RegionIdx, FOnQosSearchComplete InCompletionDelegate)
{
	bool bSuccess = false;
	if (Datacenters.IsValidIndex(RegionIdx))
	{
		FQosRegionInfo& Datacenter = Datacenters[RegionIdx];
		Datacenter.Reset();

		IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
		if (OnlineSub)
		{
			IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
			if (SessionInt.IsValid())
			{
				if (!Datacenter.Region.RegionId.IsEmpty())
				{
					const TSharedRef<FOnlineSessionSearch> QosSearchParams = MakeShareable(new FOnlineSessionSearchQos());
					QosSearchParams->QuerySettings.Set(SETTING_REGION, Datacenter.Region.RegionId, EOnlineComparisonOp::Equals);

					QosSearchQuery = QosSearchParams;

					FOnFindSessionsCompleteDelegate OnFindDatacentersCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindQosServersByRegionComplete, RegionIdx, InCompletionDelegate);
					OnFindDatacentersCompleteDelegateHandle = SessionInt->AddOnFindSessionsCompleteDelegate_Handle(OnFindDatacentersCompleteDelegate);

					SessionInt->FindSessions(ControllerId, QosSearchParams);
				}
				else
				{
					OnFindQosServersByRegionComplete(false, RegionIdx, InCompletionDelegate);
				}
				bSuccess = true;
			}
		}
	}

	return bSuccess;
}

int32 UQosEvaluator::GetNextRegionId(int32 LastRegionId)
{
	int32 NextIdx = LastRegionId + 1;
	for (; Datacenters.IsValidIndex(NextIdx) && !Datacenters[NextIdx].Region.bEnabled; NextIdx++);
	return NextIdx;
}

void UQosEvaluator::OnFindQosServersByRegionComplete(bool bWasSuccessful, int32 RegionIdx, FOnQosSearchComplete InCompletionDelegate)
{
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	if (OnlineSub)
	{
		IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
		if (SessionInt.IsValid())
		{
			SessionInt->ClearOnFindSessionsCompleteDelegate_Handle(OnFindDatacentersCompleteDelegateHandle);
		}
	}

	if (!bCancelOperation)
	{
		FQosRegionInfo& Datacenter = Datacenters[RegionIdx];
		Datacenter.Result = bWasSuccessful ? EQosRegionResult::Success : EQosRegionResult::Invalid;
		
		// Copy the search results for later evaluation
		Datacenter.SearchResults = QosSearchQuery->SearchResults;
		QosSearchQuery = nullptr;

		int32 NextRegionIdx = GetNextRegionId(RegionIdx);
		if (Datacenters.IsValidIndex(NextRegionIdx))
		{
			TWeakObjectPtr<UQosEvaluator> WeakThisCap(this);
			GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([InCompletionDelegate, NextRegionIdx, WeakThisCap]()
			{
				if (WeakThisCap.IsValid())
				{
					auto StrongThis = WeakThisCap.Get();
					// Move to the next region results
					if (!StrongThis->FindQosServersByRegion(NextRegionIdx, InCompletionDelegate))
					{
						// Failed to start
						StrongThis->GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([InCompletionDelegate, WeakThisCap]()
						{
							if (WeakThisCap.IsValid())
							{
								auto StrongThis1 = WeakThisCap.Get();
								StrongThis1->FinalizeDatacenterResult(InCompletionDelegate, EQosCompletionResult::Failure, TArray<FQosRegionInfo>());
							}
						}));
					}
				}
			}));
		}
		else
		{
			// Evaluate search results for all regions next tick
			FOnQosPingEvalComplete CompletionDelegate = FOnQosPingEvalComplete::CreateUObject(this, &ThisClass::OnEvaluateForDatacenterComplete, InCompletionDelegate);
		
			TWeakObjectPtr<UQosEvaluator> WeakThisCap(this);
			GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([CompletionDelegate, WeakThisCap]()
			{
				if (WeakThisCap.IsValid())
				{
					auto StrongThis = WeakThisCap.Get();
					StrongThis->bInProgress = false;
					StrongThis->StartServerPing(CompletionDelegate);
				}
			}));
		}
	}
	else
	{
		QosSearchQuery = nullptr;

		// Mark all remaining datacenters as invalid
		for (int32 Idx = RegionIdx; Idx < Datacenters.Num(); Idx++)
		{
			Datacenters[Idx].Result = EQosRegionResult::Invalid;
		}

		FinalizeDatacenterResult(InCompletionDelegate, EQosCompletionResult::Canceled, Datacenters);
	}
}

void UQosEvaluator::OnEvaluateForDatacenterComplete(EQosCompletionResult Result, FOnQosSearchComplete InCompletionDelegate)
{
	if (Result == EQosCompletionResult::Success)
	{
		//@TODO: This is a temporary measure to reduce the effect of framerate on data center ping estimation
		// (due to the use of beacons that are ticked on the game thread for the estimation)
		// This value can be 0 to disable discounting or something like 1 or 2
		const float FractionOfFrameTimeToDiscount = 2.0f;

		extern ENGINE_API float GAverageMS;
		const int32 TimeToDiscount = (int32)(FractionOfFrameTimeToDiscount * GAverageMS);

		CalculatePingAverages(TimeToDiscount);
	}

	FinalizeDatacenterResult(InCompletionDelegate, Result, Datacenters);
}

void UQosEvaluator::CalculatePingAverages(int32 TimeToDiscount)
{
	for (FQosRegionInfo& Datacenter : Datacenters)
	{
		int32 TotalPingInMs = 0;
		int32 NumResults = 0;
		for (int32 PingIdx = 0; PingIdx < Datacenter.PingResults.Num(); PingIdx++)
		{
			if (Datacenter.PingResults[PingIdx] != UNREACHABLE_PING)
			{
				TotalPingInMs += Datacenter.PingResults[PingIdx];
				NumResults++;
			}
			else
			{
				UE_LOG(LogQos, Log, TEXT("Region[%s]: qos unreachable"), *Datacenter.Region.RegionId);
			}
		}

		int32 RawAvgPing = UNREACHABLE_PING;
		Datacenter.AvgPingMs = RawAvgPing;
		if (NumResults > 0)
		{
			RawAvgPing = TotalPingInMs / NumResults;
			Datacenter.AvgPingMs = FMath::Max<int32>(RawAvgPing - TimeToDiscount, 1);
		}

		UE_LOG(LogQos, Verbose, TEXT("Region[%s] Avg: %d Num: %d; Adjusted: %d"), *Datacenter.Region.RegionId, RawAvgPing, NumResults, Datacenter.AvgPingMs);

		if (QosStats.IsValid())
		{
			QosStats->RecordRegionInfo(Datacenter.Region.RegionId, Datacenter.AvgPingMs, NumResults);
		}
	}
}

void UQosEvaluator::StartServerPing(const FOnQosPingEvalComplete& InCompletionDelegate)
{
	if (!bInProgress)
	{
		OnQosPingEvalComplete = InCompletionDelegate;

		if (Datacenters.Num() > 0)
		{
			bInProgress = true;
			EvaluateRegionPing(GetNextRegionId());
		}
		else
		{
			// No regions to ping
			FinalizePingServers(EQosCompletionResult::Failure);
		}
	}
	else
	{
		// Already in progress
		InCompletionDelegate.ExecuteIfBound(EQosCompletionResult::Failure);
	}
}

void UQosEvaluator::EvaluateRegionPing(int32 RegionIdx)
{
	if (ensure(bInProgress))
	{
		if (Datacenters.IsValidIndex(RegionIdx))
		{
			CurrentSearchPass.RegionIdx = RegionIdx;
			CurrentSearchPass.CurrentSessionIdx = INDEX_NONE;
			ContinuePingRegion();
		}
		else
		{
			// Invalid region id
			FinalizePingServers(EQosCompletionResult::Failure);
		}
	}
}

void UQosEvaluator::ContinuePingRegion()
{
	if (!bCancelOperation)
	{
		bool bStartedPing = false;
		if (Datacenters.IsValidIndex(CurrentSearchPass.RegionIdx))
		{
			FQosRegionInfo& Datacenter = Datacenters[CurrentSearchPass.RegionIdx];
			CurrentSearchPass.CurrentSessionIdx++;
			if (Datacenter.Result != EQosRegionResult::Invalid)
			{
				if (Datacenter.SearchResults.IsValidIndex(CurrentSearchPass.CurrentSessionIdx))
				{
					bStartedPing = true;

					// There are more valid search results, keep attempting Qos requests
					UWorld* World = GetWorld();
					QosBeaconClient = World->SpawnActor<AQosBeaconClient>(AQosBeaconClient::StaticClass());

					QosBeaconClient->OnQosRequestComplete().BindUObject(this, &ThisClass::OnQosRequestComplete);
					QosBeaconClient->OnHostConnectionFailure().BindUObject(this, &ThisClass::OnQosConnectionFailure);
					QosBeaconClient->SendQosRequest(Datacenter.SearchResults[CurrentSearchPass.CurrentSessionIdx]);
				}
				else
				{
					// Out of search results
					Datacenter.LastCheckTimestamp = FDateTime::UtcNow();
				}
			}
		}
			
		if (!bStartedPing)
		{
			int32 NextRegionIdx = GetNextRegionId(CurrentSearchPass.RegionIdx);
			if (Datacenters.IsValidIndex(NextRegionIdx))
			{
				// Ran out of search results, advance regions
				EvaluateRegionPing(NextRegionIdx);
			}
			else
			{
				// Completely done
				FinalizePingServers(EQosCompletionResult::Success);
			}
		}
	}
	else
	{
		// Operation canceled
		FinalizePingServers(EQosCompletionResult::Canceled);
	}
}

void UQosEvaluator::OnQosRequestComplete(EQosResponseType QosResponse, int32 ResponseTime)
{
	if (QosBeaconClient.IsValid())
	{
		DestroyClientBeacons();
	}

	if (ensure(Datacenters.IsValidIndex(CurrentSearchPass.RegionIdx)))
	{
		FQosRegionInfo& Datacenter = Datacenters[CurrentSearchPass.RegionIdx];
		ensure(Datacenter.SearchResults.IsValidIndex(CurrentSearchPass.CurrentSessionIdx));
		FOnlineSessionSearchResult& SearchResult = Datacenter.SearchResults[CurrentSearchPass.CurrentSessionIdx];
		if (QosResponse == EQosResponseType::Success)
		{
			Datacenter.PingResults.Add(ResponseTime);
			SearchResult.PingInMs = ResponseTime;
		}
		else
		{
			Datacenter.PingResults.Add(UNREACHABLE_PING);
			SearchResult.PingInMs = UNREACHABLE_PING;
		}

		extern ENGINE_API float GAverageFPS;
		extern ENGINE_API float GAverageMS;
		UE_LOG(LogQos, Verbose, TEXT("Qos response received for region %s: %d ms FPS: %0.2f MS: %0.2f"), *Datacenter.Region.RegionId, ResponseTime, GAverageFPS, GAverageMS);

		if (QosStats.IsValid())
		{
			QosStats->RecordQosAttempt(SearchResult, QosResponse == EQosResponseType::Success);
		}
	}

	// Cancel operation will occur next tick if applicable
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::ContinuePingRegion));
}

void UQosEvaluator::OnQosConnectionFailure()
{
	OnQosRequestComplete(EQosResponseType::Failure, UNREACHABLE_PING);
}

void UQosEvaluator::FinalizePingServers(EQosCompletionResult Result)
{
	UE_LOG(LogQos, Log, TEXT("Ping evaluation complete. Result:%s"), ToString(Result));

	TWeakObjectPtr<UQosEvaluator> WeakThisCap(this);
	FTimerDelegate TimerDelegate = FTimerDelegate::CreateLambda([Result, WeakThisCap]() {
		if (WeakThisCap.IsValid())
		{
			auto StrongThis = WeakThisCap.Get();
			StrongThis->OnQosPingEvalComplete.ExecuteIfBound(Result);
			StrongThis->ResetSearchVars();
		}
	});
	GetWorldTimerManager().SetTimerForNextTick(TimerDelegate);
}

bool UQosEvaluator::AreAllRegionsComplete()
{
	for (FQosRegionInfo& Region : Datacenters)
	{
		if (Region.Region.bEnabled && Region.Result == EQosRegionResult::Invalid)
		{
			return false;
		}
	}

	return true;
}

void UQosEvaluator::OnPingResultComplete(const FString& RegionId, int32 NumTests, const FIcmpEchoResult& Result)
{
	for (FQosRegionInfo& Region : Datacenters)
	{
		if (Region.Region.RegionId == RegionId)
		{
			UE_LOG(LogQos, Verbose, TEXT("Ping Complete [%s] %s: %d"), *RegionId, *Result.ResolvedAddress, (int32)(Result.Time * 1000.0f));

			const bool bSuccess = (Result.Status == EIcmpResponseStatus::Success);
			int32 PingInMs = bSuccess ? (int32)(Result.Time * 1000.0f) : UNREACHABLE_PING;
			Region.PingResults.Add(PingInMs);
			Region.NumResponses = bSuccess ? (Region.NumResponses + 1) : Region.NumResponses;

			if (QosStats.IsValid())
			{
				QosStats->RecordQosAttempt(RegionId, Result.ResolvedAddress, PingInMs, bSuccess);
			}

			if (Region.PingResults.Num() == NumTests)
			{
				Region.LastCheckTimestamp = FDateTime::UtcNow();
				Region.Result = (Region.NumResponses == NumTests) ? EQosRegionResult::Success : EQosRegionResult::Incomplete;
			}
			break;
		}
	}
}

void UQosEvaluator::ResetSearchVars()
{
	bInProgress = false;
	bCancelOperation = false;
	CurrentSearchPass.RegionIdx = INDEX_NONE;
	CurrentSearchPass.CurrentSessionIdx = INDEX_NONE;
	OnQosPingEvalComplete.Unbind();
	QosSearchQuery = nullptr;
}

void UQosEvaluator::DestroyClientBeacons()
{
	if (QosBeaconClient.IsValid())
	{
		QosBeaconClient->OnQosRequestComplete().Unbind();
		QosBeaconClient->OnHostConnectionFailure().Unbind();
		QosBeaconClient->DestroyBeacon();
		QosBeaconClient.Reset();
	}
}

void UQosEvaluator::StartAnalytics()
{
	if (AnalyticsProvider.IsValid())
	{
		ensure(!QosStats.IsValid());
		QosStats = MakeShareable(new FQosDatacenterStats());
		QosStats->StartQosPass();
	}
}

void UQosEvaluator::EndAnalytics(EQosCompletionResult CompletionResult)
{
	if (QosStats.IsValid())
	{
		if (CompletionResult != EQosCompletionResult::Canceled)
		{
			EDatacenterResultType ResultType = EDatacenterResultType::Failure;
			if (CompletionResult != EQosCompletionResult::Failure)
			{
				ResultType = EDatacenterResultType::Normal;
			}

			QosStats->EndQosPass(ResultType);
			QosStats->Upload(AnalyticsProvider);
		}

		QosStats = nullptr;
	}
}

UWorld* UQosEvaluator::GetWorld() const
{
	UWorld* World = ParentWorld.Get();
	check(World);
	return World;
}

FTimerManager& UQosEvaluator::GetWorldTimerManager() const
{
	return GetWorld()->GetTimerManager();
}

