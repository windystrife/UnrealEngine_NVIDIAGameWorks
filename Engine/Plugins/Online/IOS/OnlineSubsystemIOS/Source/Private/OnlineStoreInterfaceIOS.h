// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineStoreInterface.h"

@class FStoreKitHelper;
@class SKProductsResponse;

/**
 * The resulting state of an iap transaction
 */
namespace EInAppPurchaseResult
{
    enum Type
    {
        Succeeded = 0,
        RestoredFromServer,
        Failed,
        Cancelled,
    };
}

/**
 *	FOnlineStoreInterfaceIOS - Implementation of the online store for IOS
 */
class FOnlineStoreInterfaceIOS : public IOnlineStore
{
public:
	/** C-tor */
	FOnlineStoreInterfaceIOS();
	/** Destructor */
	virtual ~FOnlineStoreInterfaceIOS();

	//~ Begin IOnlineStore Interface
	virtual bool QueryForAvailablePurchases(const TArray<FString>& ProductIDs, FOnlineProductInformationReadRef& InReadObject) override;
	virtual bool BeginPurchase(const FInAppPurchaseProductRequest& ProductRequest, FOnlineInAppPurchaseTransactionRef& InReadObject) override;
	virtual bool IsAllowedToMakePurchases() override;
	virtual bool RestorePurchases(const TArray<FInAppPurchaseProductRequest>& ConsumableProductFlags, FOnlineInAppPurchaseRestoreReadRef& InReadObject) override;
	//~ End IOnlineStore Interface

	/**
	 * Process a response from the StoreKit
	 */
	void ProcessProductsResponse( SKProductsResponse* Response );

	/**
	* Process a response from the StoreKit
	*/
	void ProcessRestorePurchases( EInAppPurchaseState::Type InCompletionState );

	/** Cached in-app purchase query object, used to provide the user with product information attained from the server */
	FOnlineProductInformationReadPtr CachedReadObject;
    
	/** Cached in-app purchase transaction object, used to provide details to the user, of the product that has just been purchased. */
	FOnlineInAppPurchaseTransactionPtr CachedPurchaseStateObject;

	/** Cached in-app purchase restore transaction object, used to provide details to the developer about what products should be restored */
	FOnlineInAppPurchaseRestoreReadPtr CachedPurchaseRestoreObject;


	/** Flags which determine the state of our transaction buffer, only one action at a time */
	bool bIsPurchasing, bIsProductRequestInFlight, bIsRestoringPurchases;

private:
	/** Access to the IOS Store kit interface */
	FStoreKitHelper* StoreHelper;

	/** Delegate fired when a query for purchases has completed, whether successful or unsuccessful */
	FOnQueryForAvailablePurchasesComplete OnQueryForAvailablePurchasesCompleteDelegate;

	/** Delegate fired when a purchase transaction has completed, whether successful or unsuccessful */
	FOnInAppPurchaseComplete OnPurchaseCompleteDelegate;

	/** Delegate fired when the purchase restoration has completed, whether successful or unsuccessful */
	FOnInAppPurchaseRestoreComplete OnPurchaseRestoreCompleteDelegate;
};

typedef TSharedPtr<FOnlineStoreInterfaceIOS, ESPMode::ThreadSafe> FOnlineStoreInterfaceIOSPtr;

