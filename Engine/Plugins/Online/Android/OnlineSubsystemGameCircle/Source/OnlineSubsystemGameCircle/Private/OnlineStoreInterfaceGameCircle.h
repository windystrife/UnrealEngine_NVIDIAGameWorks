// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineStoreInterface.h"

class FOnlineSubsystemGameCircle;

/**
 * The resulting state of an iap transaction
 */
enum class EAmazonResponseStatus : uint8
{
	Successful,
	Failed,
	NotSupported,
	AlreadyPurchased,
	InvalidSKU,
	Unknown
};

/**
 * Implementation of the Platform Purchase receipt. For this we provide an identifier and the encrypted data.
 */
class FGameCirclePurchaseReceipt : public IPlatformPurchaseReceipt
{
public:
	// Product identifier
	FString Identifier;

	// The encrypted receipt data
	FString Data;
};


/**
 *	FOnlineStoreGameCircle - Implementation of the online store for GameCircle
 */
class FOnlineStoreGameCircle : public IOnlineStore
{
public:
	/** C-tor */
	FOnlineStoreGameCircle(FOnlineSubsystemGameCircle* InSubsystem);
	/** Destructor */
	virtual ~FOnlineStoreGameCircle();

	// Begin IOnlineStore 
	virtual bool QueryForAvailablePurchases(const TArray<FString>& ProductIDs, FOnlineProductInformationReadRef& InReadObject) override;
	virtual bool BeginPurchase(const FInAppPurchaseProductRequest& ProductRequest, FOnlineInAppPurchaseTransactionRef& InReadObject) override;
	virtual bool IsAllowedToMakePurchases() override;
	virtual bool RestorePurchases(const TArray<FInAppPurchaseProductRequest>& ConsumableProductFlags, FOnlineInAppPurchaseRestoreReadRef& InReadObject) override;
	// End IOnlineStore 

	void ProcessQueryAvailablePurchasesResults(EInAppPurchaseState::Type InResult, const TArray<FInAppPurchaseProductInfo>& AvailablePurchases);
	void ProcessPurchaseResult(EInAppPurchaseState::Type InResult, const FString& InProductId, const FString& InReceiptData, const FString& Signature);

	/** Cached in-app purchase restore transaction object, used to provide details to the developer about what products should be restored */
	FOnlineInAppPurchaseRestoreReadPtr CachedPurchaseRestoreObject;

private:

	/** Pointer to owning subsystem */
	FOnlineSubsystemGameCircle* Subsystem;

	/** Delegate fired when a query for purchases has completed, whether successful or unsuccessful */
	FOnQueryForAvailablePurchasesComplete OnQueryForAvailablePurchasesCompleteDelegate;

	/** Delegate fired when a purchase transaction has completed, whether successful or unsuccessful */
	FOnInAppPurchaseComplete OnPurchaseCompleteDelegate;

	/** Cached in-app purchase query object, used to provide the user with product information attained from the server */
	FOnlineProductInformationReadPtr ReadObject;

	/** Cached in-app purchase transaction object, used to provide details to the user, of the product that has just been purchased. */
	FOnlineInAppPurchaseTransactionPtr CachedPurchaseStateObject;
};

typedef TSharedPtr<FOnlineStoreGameCircle, ESPMode::ThreadSafe> FOnlineStoreGameCirclePtr;

