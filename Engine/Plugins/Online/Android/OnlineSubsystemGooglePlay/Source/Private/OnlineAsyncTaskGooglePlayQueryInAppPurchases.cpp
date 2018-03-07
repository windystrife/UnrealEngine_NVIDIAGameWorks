// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncTaskGooglePlayQueryInAppPurchases.h"
#include "OnlineSubsystemGooglePlay.h"
#include "OnlineStoreGooglePlayCommon.h"

FOnlineAsyncTaskGooglePlayQueryInAppPurchases::FOnlineAsyncTaskGooglePlayQueryInAppPurchases(
	FOnlineSubsystemGooglePlay* InSubsystem,
	const TArray<FString>& InProductIds)
	: FOnlineAsyncTaskBasic(InSubsystem)
	, ProductIds(InProductIds)
	, bWasRequestSent(false)
{
}

void FOnlineAsyncTaskGooglePlayQueryInAppPurchases::ProcessQueryAvailablePurchasesResults(bool bInSuccessful)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineAsyncTaskGooglePlayQueryInAppPurchases::ProcessQueryAvailablePurchasesResults"));

	bWasSuccessful = bInSuccessful;
	bIsComplete = true;
}

void FOnlineAsyncTaskGooglePlayQueryInAppPurchases::Finalize()
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineAsyncTaskGooglePlayQueryInAppPurchases::Finalize"));
}

void FOnlineAsyncTaskGooglePlayQueryInAppPurchases::TriggerDelegates()
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineAsyncTaskGooglePlayQueryInAppPurchases::TriggerDelegates"));
	Subsystem->GetStoreInterface()->TriggerOnQueryForAvailablePurchasesCompleteDelegates(bWasSuccessful);
}

void FOnlineAsyncTaskGooglePlayQueryInAppPurchases::Tick()
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineAsyncTaskGooglePlayQueryInAppPurchases::Tick"));

	if (!bWasRequestSent)
	{
		extern bool AndroidThunkCpp_Iap_QueryInAppPurchases(const TArray<FString>&);
		AndroidThunkCpp_Iap_QueryInAppPurchases(ProductIds);

		bWasRequestSent = true;
	}
}

FOnlineAsyncTaskGooglePlayQueryInAppPurchasesV2::FOnlineAsyncTaskGooglePlayQueryInAppPurchasesV2(
	FOnlineSubsystemGooglePlay* InSubsystem,
	const TArray<FUniqueOfferId>& InProductIds,
	const FOnQueryOnlineStoreOffersComplete& InCompletionDelegate)
	: FOnlineAsyncTaskBasic(InSubsystem)
	, ProductIdsV2(InProductIds)
	, bWasRequestSent(false)
	, CompletionDelegate(InCompletionDelegate)
{
}

void FOnlineAsyncTaskGooglePlayQueryInAppPurchasesV2::ProcessQueryAvailablePurchasesResults(EGooglePlayBillingResponseCode InResponse)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineAsyncTaskGooglePlayQueryInAppPurchasesV2::ProcessQueryAvailablePurchasesResults %d"), (int32)InResponse);
	if (InResponse == EGooglePlayBillingResponseCode::Ok)
	{
		bWasSuccessful = true;
	}
	else
	{
		ErrorStr = ::ToString(InResponse);
	}

	bIsComplete = true;
}

void FOnlineAsyncTaskGooglePlayQueryInAppPurchasesV2::Finalize()
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineAsyncTaskGooglePlayQueryInAppPurchasesV2::Finalize"));
}

void FOnlineAsyncTaskGooglePlayQueryInAppPurchasesV2::TriggerDelegates()
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineAsyncTaskGooglePlayQueryInAppPurchasesV2::TriggerDelegates"));
	CompletionDelegate.ExecuteIfBound(bWasSuccessful, ProductIdsV2, ErrorStr);
}

void FOnlineAsyncTaskGooglePlayQueryInAppPurchasesV2::Tick()
{
	UE_LOG(LogOnline, VeryVerbose, TEXT("FOnlineAsyncTaskGooglePlayQueryInAppPurchasesV2::Tick"));

	if (!bWasRequestSent)
	{
		extern bool AndroidThunkCpp_Iap_QueryInAppPurchases(const TArray<FString>&);
		AndroidThunkCpp_Iap_QueryInAppPurchases(ProductIdsV2);

		bWasRequestSent = true;
	}
}

