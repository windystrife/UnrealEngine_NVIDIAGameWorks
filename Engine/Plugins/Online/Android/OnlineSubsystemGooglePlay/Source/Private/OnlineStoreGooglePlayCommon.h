// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineStoreInterface.h"
#include "OnlineStoreInterfaceV2.h"
#include "OnlinePurchaseInterface.h"

struct FGoogleTransactionData;

/** Possible responses returned from the Java GooglePlay billing interface */
enum class EGooglePlayBillingResponseCode : uint8
{
	Ok = 0,
	UserCancelled = 1,
	ServiceUnavailable = 2,
	BillingUnavailable = 3,
	ItemUnavailable = 4,
	DeveloperError = 5,
	Error = 6,
	ItemAlreadyOwned = 7,
	ItemNotOwned = 8,
};

inline const TCHAR* const ToString(EGooglePlayBillingResponseCode InResponseCode)
{
	switch (InResponseCode)
	{
		case EGooglePlayBillingResponseCode::Ok:
			return TEXT("Ok");
		case EGooglePlayBillingResponseCode::UserCancelled:
			return TEXT("UserCancelled");
		case EGooglePlayBillingResponseCode::ItemAlreadyOwned:
			return TEXT("ItemAlreadyOwned");
		case EGooglePlayBillingResponseCode::ItemNotOwned:
			return TEXT("ItemNotOwned");
		case EGooglePlayBillingResponseCode::ServiceUnavailable:
			return TEXT("ServiceUnavailable");
		case EGooglePlayBillingResponseCode::BillingUnavailable:
			return TEXT("BillingUnavailable");
		case EGooglePlayBillingResponseCode::ItemUnavailable:
			return TEXT("ItemUnavailable");
		case EGooglePlayBillingResponseCode::DeveloperError:
			return TEXT("DeveloperError");
		case EGooglePlayBillingResponseCode::Error:
			return TEXT("Error");
		default:
			return TEXT("UnknownError");
	}
}

inline EInAppPurchaseState::Type ConvertGPResponseCodeToIAPState(const EGooglePlayBillingResponseCode InResponseCode)
{
	switch (InResponseCode)
	{
		case EGooglePlayBillingResponseCode::Ok:
			return EInAppPurchaseState::Success;
		case EGooglePlayBillingResponseCode::UserCancelled:
			return EInAppPurchaseState::Cancelled;
		case EGooglePlayBillingResponseCode::ItemAlreadyOwned:
			return EInAppPurchaseState::AlreadyOwned;
		case EGooglePlayBillingResponseCode::ItemNotOwned:
			return EInAppPurchaseState::NotAllowed;
		case EGooglePlayBillingResponseCode::ServiceUnavailable:
		case EGooglePlayBillingResponseCode::BillingUnavailable:
		case EGooglePlayBillingResponseCode::ItemUnavailable:
		case EGooglePlayBillingResponseCode::DeveloperError:
		case EGooglePlayBillingResponseCode::Error:
		default:
			return EInAppPurchaseState::Failed;
	}
}

inline EPurchaseTransactionState ConvertGPResponseCodeToPurchaseTransactionState(const EGooglePlayBillingResponseCode InResponseCode)
{
	switch (InResponseCode)
	{
		case EGooglePlayBillingResponseCode::Ok:
			return EPurchaseTransactionState::Purchased;
		case EGooglePlayBillingResponseCode::UserCancelled:
			return EPurchaseTransactionState::Canceled;
		case EGooglePlayBillingResponseCode::ItemAlreadyOwned:
			// Non consumable purchased again?
			return EPurchaseTransactionState::Invalid;
		case EGooglePlayBillingResponseCode::ItemNotOwned:
			return EPurchaseTransactionState::Invalid;
		case EGooglePlayBillingResponseCode::ServiceUnavailable:
		case EGooglePlayBillingResponseCode::BillingUnavailable:
		case EGooglePlayBillingResponseCode::ItemUnavailable:
		case EGooglePlayBillingResponseCode::DeveloperError:
		case EGooglePlayBillingResponseCode::Error:
		default:
			return EPurchaseTransactionState::Failed;
	}
}

/**
 * The resulting state of an iap transaction
 */
enum class EInAppPurchaseResult : uint8
{
	Succeeded = 0,
	RestoredFromServer,
	Failed,
	Cancelled,
};

/**
 * Implementation of the Platform Purchase receipt. For this we provide an identifier and the encrypted data.
 */
class FGooglePlayPurchaseReceipt : public IPlatformPurchaseReceipt
{
public:
	// Product identifier
	FString Identifier;

	// The encrypted receipt data
	FString Data;
};

 /**
  * Delegate fired when an IAP query for available offers has completed
  *
  * @param Response response from GooglePlay backend
  * @param ProvidedProductInformation list of offers returned in response to a query on available offer ids
  */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGooglePlayAvailableIAPQueryComplete, EGooglePlayBillingResponseCode /*Response*/, const TArray<FInAppPurchaseProductInfo>& /*ProvidedProductInformation*/);
typedef FOnGooglePlayAvailableIAPQueryComplete::FDelegate FOnGooglePlayAvailableIAPQueryCompleteDelegate;

/**
 * Delegate fired when an IAP has completed
 *
 * @param InResponseCode response from the GooglePlay backend
 * @param InTransactionData transaction data for the completed purchase
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGooglePlayProcessPurchaseComplete, EGooglePlayBillingResponseCode /*InResponseCode*/, const FGoogleTransactionData& /*InTransactionData*/);
typedef FOnGooglePlayProcessPurchaseComplete::FDelegate FOnGooglePlayProcessPurchaseCompleteDelegate;

/**
 * Delegate fired internally an existing purchases query has completed
 *
 * @param InResponseCode response from the GooglePlay backend
 * @param InExistingPurchases known purchases for the user (non consumed or permanent)
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGooglePlayQueryExistingPurchasesComplete, EGooglePlayBillingResponseCode /*InResponseCode*/, const TArray<FGoogleTransactionData>& /*InExistingPurchases*/);
typedef FOnGooglePlayQueryExistingPurchasesComplete::FDelegate FOnGooglePlayQueryExistingPurchasesCompleteDelegate;

/**
 * Delegate fired internally when existing purchases have been restored (StoreV1 only)
 *
 * @param InResponseCode response from the GooglePlay backend
 * @param InRestoredPurchases restored for the user (non consumed or permanent)
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGooglePlayRestorePurchasesComplete, EGooglePlayBillingResponseCode /*InResponseCode*/, const TArray<FGoogleTransactionData>& /*InRestoredPurchases*/);
typedef FOnGooglePlayRestorePurchasesComplete::FDelegate FOnGooglePlayRestorePurchasesCompleteDelegate;
