// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineStoreInterfaceV2.h"
#include "OnlineStoreGooglePlayCommon.h"

class FOnlineSubsystemGooglePlay;

/**
 * Async event tracking an outstanding "query available in app purchases" task for StoreV1
 */
class FOnlineAsyncTaskGooglePlayQueryInAppPurchases : public FOnlineAsyncTaskBasic<FOnlineSubsystemGooglePlay>
{

public:
	FOnlineAsyncTaskGooglePlayQueryInAppPurchases(
		FOnlineSubsystemGooglePlay* InSubsystem,
		const TArray<FString>& InProductIds);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("QueryInAppPurchases"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

	// FOnlineAsyncTask
	virtual void Tick() override;

	// FOnlineAsyncTaskGooglePlayQueryInAppPurchases
	void ProcessQueryAvailablePurchasesResults(bool bInSuccessful);

protected:

	// The product ids provided for this task
	const TArray<FString> ProductIds;

	/** Flag indicating that the request has been sent */
	bool bWasRequestSent;
};

/**
* Async event tracking an outstanding "query available in app purchases" task for StoreV2
*/
class FOnlineAsyncTaskGooglePlayQueryInAppPurchasesV2 : public FOnlineAsyncTaskBasic<FOnlineSubsystemGooglePlay>
{

public:
	FOnlineAsyncTaskGooglePlayQueryInAppPurchasesV2(
		FOnlineSubsystemGooglePlay* InSubsystem,
		const TArray<FUniqueOfferId>& InProductIds,
		const FOnQueryOnlineStoreOffersComplete& InCompletionDelegate);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("QueryInAppPurchasesV2"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

	// FOnlineAsyncTask
	virtual void Tick() override;

	/**
	 * Available in app purchase query is complete, finish the task before notifying external listeners
	 */
	void ProcessQueryAvailablePurchasesResults(EGooglePlayBillingResponseCode InResponse);

protected:

	/** The product ids provided for this task */
	const TArray<FUniqueOfferId> ProductIdsV2;

	/** Completion delegate passed in at the time of the purchase query */
	const FOnQueryOnlineStoreOffersComplete CompletionDelegate;

	/** Flag indicating that the request has been sent */
	bool bWasRequestSent;

	/** String storing any generated errors during the query */
	FString ErrorStr;
};
