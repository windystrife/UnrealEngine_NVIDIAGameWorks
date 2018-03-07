// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineStoreInterface.h"
#include "OnlineStoreGooglePlayCommon.h"

class FOnlineSubsystemGooglePlay;
class FOnlineAsyncTaskGooglePlayQueryInAppPurchases;

/**
 *	FOnlineStoreGooglePlay - Implementation of the online store for GooglePlay
 */
class FOnlineStoreGooglePlay : 
	public IOnlineStore,
	public TSharedFromThis<FOnlineStoreGooglePlay, ESPMode::ThreadSafe>
{
public:
	/** C-tor */
	FOnlineStoreGooglePlay(FOnlineSubsystemGooglePlay* InSubsystem);
	/** Destructor */
	virtual ~FOnlineStoreGooglePlay();

	/** Initialize the interface */
	void Init();

	// Begin IOnlineStore 
	virtual bool QueryForAvailablePurchases(const TArray<FString>& ProductIDs, FOnlineProductInformationReadRef& InReadObject) override;
	virtual bool BeginPurchase(const FInAppPurchaseProductRequest& ProductRequest, FOnlineInAppPurchaseTransactionRef& InReadObject) override;
	virtual bool IsAllowedToMakePurchases() override;
	virtual bool RestorePurchases(const TArray<FInAppPurchaseProductRequest>& ConsumableProductFlags, FOnlineInAppPurchaseRestoreReadRef& InReadObject) override;
	// End IOnlineStore 

	/** Cached in-app purchase restore transaction object, used to provide details to the developer about what products should be restored */
	FOnlineInAppPurchaseRestoreReadPtr CachedPurchaseRestoreObject;

private:

	/** Pointer to owning subsystem */
	FOnlineSubsystemGooglePlay* Subsystem;

	/** The current query for iap async task */
	FOnlineAsyncTaskGooglePlayQueryInAppPurchases* CurrentQueryTask;

	/** Delegate fired when a query for purchases has completed, whether successful or unsuccessful */
	FOnQueryForAvailablePurchasesComplete OnQueryForAvailablePurchasesCompleteDelegate;

	/** Delegate fired when a purchase transaction has completed, whether successful or unsuccessful */
	FOnInAppPurchaseComplete OnPurchaseCompleteDelegate;

	/** Cached in-app purchase query object, used to provide the user with product information attained from the server */
	FOnlineProductInformationReadPtr ReadObject;

	/** Cached in-app purchase transaction object, used to provide details to the user, of the product that has just been purchased. */
	FOnlineInAppPurchaseTransactionPtr CachedPurchaseStateObject;

	/**
	 * Delegate fired where an IAP query for available offers has completed
	 *
	 * @param InResponseCode response from Google backend
	 * @param AvailablePurchases list of offers returned in response to a query on available offer ids
	 */
	void OnGooglePlayAvailableIAPQueryComplete(EGooglePlayBillingResponseCode InResponseCode, const TArray<FInAppPurchaseProductInfo>& AvailablePurchases);
	FDelegateHandle AvailableIAPQueryDelegateHandle;

	/**
	 * Delegate fired when a purchase has completed
	 *
	 * @param InResponseCode response from the GooglePlay backend
	 * @param InTransactionData transaction data for the completed purchase
	 */
	void OnProcessPurchaseResult(EGooglePlayBillingResponseCode InResponseCode, const FGoogleTransactionData& InTransactionData);
	FDelegateHandle ProcessPurchaseResultDelegateHandle;

	/**
	 * Delegate fired when purchases are restored
	 *
	 * @param InResponseCode response from the GooglePlay backend
	 * @param InRestoredPurchases transaction data for the restored purchases
	 */
	void OnRestorePurchasesComplete(EGooglePlayBillingResponseCode InResponseCode, const TArray<FGoogleTransactionData>& InRestoredPurchases);
	FDelegateHandle RestorePurchasesCompleteDelegateHandle;
};

typedef TSharedPtr<FOnlineStoreGooglePlay, ESPMode::ThreadSafe> FOnlineStoreGooglePlayPtr;

