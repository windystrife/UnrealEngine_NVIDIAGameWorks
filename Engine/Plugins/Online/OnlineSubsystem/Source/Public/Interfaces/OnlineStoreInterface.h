// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineDelegateMacros.h"
#include "OnlineStoreInterface.generated.h"

/**
 * Possible result states of an in-app purchase transaction
 */
UENUM(BlueprintType)
namespace EInAppPurchaseState
{
	enum Type
	{
		Unknown = 0 UMETA(DisplayName = "Unknown"),
		Success UMETA(DisplayName = "Success"),
		Failed UMETA(DisplayName = "Failed"),
		Cancelled UMETA(DisplayName = "Cancelled"),
		Invalid UMETA(DisplayName = "Invalid"),
		NotAllowed UMETA(DisplayName = "NotAllowed"),
		Restored UMETA(DisplayName = "Restored"),
		AlreadyOwned UMETA(DisplayName = "AlreadyOwned"),
	};
}

/**
 * Platform derivitive for their own interpretation of product receipts.
 * Each Store implementation should provide their own receipt type extended from this.
 */
class IPlatformPurchaseReceipt
{
};


/**
 * Delegate fired when a session create request has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnQueryForAvailablePurchasesComplete, bool);
typedef FOnQueryForAvailablePurchasesComplete::FDelegate FOnQueryForAvailablePurchasesCompleteDelegate;

/**
 * Delegate fired when the online session has transitioned to the started state
 *
 * @param SessionName the name of the session the that has transitioned to started
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInAppPurchaseComplete, EInAppPurchaseState::Type);
typedef FOnInAppPurchaseComplete::FDelegate FOnInAppPurchaseCompleteDelegate;

/**
* Delegate fired when the online session has transitioned to the started state
*
* @param SessionName the name of the session the that has transitioned to started
* @param bWasSuccessful true if the async action completed without error, false if there was an error
*/
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInAppPurchaseRestoreComplete, EInAppPurchaseState::Type);
typedef FOnInAppPurchaseRestoreComplete::FDelegate FOnInAppPurchaseRestoreCompleteDelegate;


/**
 * Micro-transaction request information
 */
USTRUCT(BlueprintType)
struct FInAppPurchaseProductRequest
{
	GENERATED_USTRUCT_BODY()

	// The unique product identifier that matches the one from your targeted store.
	UPROPERTY(BlueprintReadWrite, Category = ProductInfo)
	FString ProductIdentifier;

	// Flag to determine whether this is a consumable purchase, or not.
	UPROPERTY(BlueprintReadWrite, Category = ProductInfo)
	bool bIsConsumable;
};


/**
 * Micro-transaction purchase information
 */
USTRUCT(BlueprintType)
struct FInAppPurchaseProductInfo
{
	GENERATED_USTRUCT_BODY()

	// The unique product identifier
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString Identifier;

	// the unique transaction identifier
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString TransactionIdentifier;

	// The localized display name
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString DisplayName;

	// The localized display description name
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString DisplayDescription;

	// The localized display price name
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString DisplayPrice;

	// Raw price without currency code and symbol
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	float RawPrice;

	// The localized currency code of the price
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString CurrencyCode;

	// The localized currency symbol of the price
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString CurrencySymbol;

	// The localized decimal separator used in the price
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString DecimalSeparator;

	// The localized grouping separator of the price
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString GroupingSeparator;

	// Opaque receipt data for the transaction
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString ReceiptData;
};


/**
 * Micro-transaction restored purchase information
 */
USTRUCT(BlueprintType)
struct FInAppPurchaseRestoreInfo
{
	GENERATED_USTRUCT_BODY()

	// The unique product identifier
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString Identifier;

	// The opaque receipt data for the platform
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString ReceiptData;

	// the unique transaction identifier
	UPROPERTY(BlueprintReadOnly, Category = ProductInfo)
	FString TransactionIdentifier;
};


/**
 *	Interface for reading data from an In App Purchase service
 */
class ONLINESUBSYSTEM_API FOnlineProductInformationRead
{
public:
	/** Indicates an error reading data occurred while processing */
	EOnlineAsyncTaskState::Type ReadState;

	FOnlineProductInformationRead() :
		ReadState(EOnlineAsyncTaskState::NotStarted)
	{
	}

	TArray<FInAppPurchaseProductInfo> ProvidedProductInformation;
};

typedef TSharedRef<FOnlineProductInformationRead, ESPMode::ThreadSafe> FOnlineProductInformationReadRef;
typedef TSharedPtr<FOnlineProductInformationRead, ESPMode::ThreadSafe> FOnlineProductInformationReadPtr;


/**
 *	Interface for reading data from an In App Purchase service
 */
class ONLINESUBSYSTEM_API FOnlineInAppPurchaseTransaction
{
public:
	/** Indicates an error reading data occurred while processing */
	EOnlineAsyncTaskState::Type ReadState;
	/** Is the purchase consumable */
	bool bIsConsumable;

	FOnlineInAppPurchaseTransaction() :
		ReadState(EOnlineAsyncTaskState::NotStarted),
		bIsConsumable(false)
	{
	}

	FInAppPurchaseProductInfo ProvidedProductInformation;
};

typedef TSharedRef<FOnlineInAppPurchaseTransaction, ESPMode::ThreadSafe> FOnlineInAppPurchaseTransactionRef;
typedef TSharedPtr<FOnlineInAppPurchaseTransaction, ESPMode::ThreadSafe> FOnlineInAppPurchaseTransactionPtr;


/**
 *	Interface for reading data from an In App Purchase service
 */
class ONLINESUBSYSTEM_API FOnlineInAppPurchaseRestoreRead
{
public:
	/** Indicates an error reading data occurred while processing */
	EOnlineAsyncTaskState::Type ReadState;

	FOnlineInAppPurchaseRestoreRead() :
		ReadState(EOnlineAsyncTaskState::NotStarted)
	{
	}

	TArray<FInAppPurchaseRestoreInfo> ProvidedRestoreInformation;
};

typedef TSharedRef<FOnlineInAppPurchaseRestoreRead, ESPMode::ThreadSafe> FOnlineInAppPurchaseRestoreReadRef;
typedef TSharedPtr<FOnlineInAppPurchaseRestoreRead, ESPMode::ThreadSafe> FOnlineInAppPurchaseRestoreReadPtr;

/**
 *	IOnlineStore - Interface class for microtransactions
 */
class IOnlineStore
{
public:

	virtual ~IOnlineStore() { }

	/**
	 * Search for what purchases are available
	 *
	 * @return - Whether a request was sent to check for purchases
	 */
	virtual bool QueryForAvailablePurchases(const TArray<FString>& ProductIDs, FOnlineProductInformationReadRef& InReadObject) = 0;

	/**
	 * Delegate which is executed when QueryForAvailablePurchases completes
	 */
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnQueryForAvailablePurchasesComplete, bool);

	/**
	 * Check whether microtransactions can be purchased
	 *
	 * @return - Whether the device can make purchases
	 */
	virtual bool IsAllowedToMakePurchases() = 0;

	/**
	 * Begin a purchase transaction for the product which relates to the given index
	 *
	 * @param Index The id of the object we would like to purchase.
	 *
	 * @return - whether a purchase request was sent
	 */
	virtual bool BeginPurchase(const FInAppPurchaseProductRequest& ProductRequest, FOnlineInAppPurchaseTransactionRef& InReadObject) = 0;

	/**
	 * Delegate which is executed when a Purchase completes
	 */
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnInAppPurchaseComplete, EInAppPurchaseState::Type);

	/**
	* Restore any purchases previously made
	*/
	virtual bool RestorePurchases(const TArray<FInAppPurchaseProductRequest>& ConsumableProductFlags, FOnlineInAppPurchaseRestoreReadRef& InReadObject) = 0;

	/**
	* Delegate which is executed when a Purchase completes
	*/
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnInAppPurchaseRestoreComplete, EInAppPurchaseState::Type);
};
