// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// SK includes
#import <StoreKit/SKRequest.h>
#import <StoreKit/SKError.h>
#import <StoreKit/SKProduct.h>
#import <StoreKit/SKProductsRequest.h>
#import <StoreKit/SKReceiptRefreshRequest.h>
#import <StoreKit/SKPaymentTransaction.h>
#import <StoreKit/SKPayment.h>
#import <StoreKit/SKPaymentQueue.h>

#include "OnlineSubsystemTypes.h"

/**
 * Holds in a common format the data that comes out of an SKPaymentTransaction
 */
struct FStoreKitTransactionData
{
	FStoreKitTransactionData(const SKPaymentTransaction* Transaction);
	
	/** @return a string that prints useful debug information about this transaction */
	FString ToDebugString() const
	{
		return FString::Printf(TEXT("OfferId: %s TransactionId: %s%s ReceiptData: %s%s"),
						*OfferId,
						*TransactionIdentifier,
						OriginalTransactionIdentifier.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" OriginalTransactionId: %s"), *OriginalTransactionIdentifier),
						*ReceiptData,
						ErrorStr.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" Error: %s"), *ErrorStr));
	}
	
	/** @return offer id for this transaction */
	const FString& GetOfferId() const { return OfferId; }
	/** @return receipt data for this transaction */
	const FString& GetReceiptData() const { return ReceiptData; }
	/** @return error string for this transaction, if applicable */
	const FString& GetErrorStr() const { return ErrorStr; }
	
	/** @return the correct transaction identifier relative to the original purchase */
	const FString& GetTransactionIdentifier() const
	{
		if (!OriginalTransactionIdentifier.IsEmpty())
		{
			return OriginalTransactionIdentifier;
		}
		else
		{
			return TransactionIdentifier;
		}
	}
	
private:
	
	/** iTunesConnect offer id */
	FString OfferId;
	/** Opaque store receipt data */
	FString ReceiptData;
	/** Error on the transaction, if applicable */
	FString ErrorStr;
	/** Unique transaction id */
	FString TransactionIdentifier;
	/** Original transaction id if restored purchase */
	FString OriginalTransactionIdentifier;
};

/**
 * Delegate fires when a single transaction has completed (may be part of many offers in a single user purchase)
 *
 * @param Result result of the transaction
 * @param TransactionData data associated with the completed transaction
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTransactionCompleteIOS, EPurchaseTransactionState /*Result*/, const FStoreKitTransactionData& /*TransactionData*/);
typedef FOnTransactionCompleteIOS::FDelegate FOnTransactionCompleteIOSDelegate;
/**
 * Delegate fires when a single restored transaction has completed (may be part of many user purchases that are restored)
 *
 * @param TransactionData data associated with the restored transaction
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTransactionRestoredIOS, const FStoreKitTransactionData& /*TransactionData*/);
typedef FOnTransactionRestoredIOS::FDelegate FOnTransactionRestoredIOSDelegate;

/**
 * Delegate fires when a transaction progress event occurs
 *
 * @param TransactionData data associated with the event
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTransactionProgress, const FStoreKitTransactionData& /*TransactionData*/);
typedef FOnTransactionProgress::FDelegate FOnTransactionProgressDelegate;

/**
 * Delegate fires when the entire restore transactions operation has completed
 *
 * @param Result aggregate result of the restore operation
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRestoreTransactionsCompleteIOS, EPurchaseTransactionState /*Result*/);
typedef FOnRestoreTransactionsCompleteIOS::FDelegate FOnRestoreTransactionsCompleteIOSDelegate;

/**
 * Delegate fires when a product request for offer(s) has completed
 *
 * @param Response the array of product information for the products that were queried
 * @param CompletionDelegate the delegate to fire externally when this operation completes
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnProductsRequestResponse, SKProductsResponse* /*Response*/, const FOnQueryOnlineStoreOffersComplete& /*CompletionDelegate*/);
typedef FOnProductsRequestResponse::FDelegate FOnProductsRequestResponseDelegate;

/**
 * Product request helper to store a delegate for this individual product request
 */
@interface FSKProductsRequestHelper : SKProductsRequest
{
};

/** Delegate to fire when this product request completes with the store kit */
@property FOnQueryOnlineStoreOffersComplete OfferDelegate;

@end

/** 
 * Helper class, which allows us to manage IAP product information requests, AND transactions
 * (legacy version, used by OnlineStoreInterface.h, mutually exclusive with FStoreKitHelperV2)
 */
@interface FStoreKitHelper : NSObject<SKProductsRequestDelegate, SKPaymentTransactionObserver, SKRequestDelegate>
{
};

/** Store kit request object, holds information about the products we are purchasing, or querying. */
@property (nonatomic, strong) SKRequest *Request;
/** collection of available products attaced through a store kit request */
@property (nonatomic, strong) NSArray *AvailableProducts;

/** Helper fn to start a store kit purchase request */
-(void)makePurchase:(NSMutableSet*)productIDs;

/** Helper fn to start a store kit purchase information query request */
-(void)requestProductData:(NSMutableSet*)productIDs;

/** Helper fn to direct a product request response back to our store interface */
-(void)productsRequest:(SKProductsRequest *)request didReceiveResponse : (SKProductsResponse *)response;

/** Helper fn to restore previously purchased products */
-(void)restorePurchases;
@end

/**
 * Helper class, which allows us to manage IAP product information requests, AND transactions
 * (version used by OnlineStoreIOS.h and OnlinePurchaseIOS.h)
 */
@interface FStoreKitHelperV2 : FStoreKitHelper
{
	/** delegate fired when a product request completes */
	FOnProductsRequestResponse _OnProductRequestResponse;
	/** delegate fired when a single purchase transaction has completed (may be a part of multiple requests at once) */
	FOnTransactionCompleteIOS _OnTransactionCompleteResponse;
	/** delegate fired when a single transaction is restored (may be a part of many restored purchases) */
	FOnTransactionRestoredIOS _OnTransactionRestored;
	/** delegate fired when all transactions have been restored */
	FOnRestoreTransactionsCompleteIOS _OnRestoreTransactionsComplete;
	/** delegate fired when a purchase in progress is detected */
	FOnTransactionProgress _OnTransactionPurchaseInProgress;
	/** delegate fired when a deferred purchase is detected */
	FOnTransactionProgress _OnTransactionDeferred;
};

/** list of known pending transactions that are in the queue but not marked as redeemed by the app */
@property (nonatomic, strong) NSMutableSet* PendingTransactions;

-(FDelegateHandle)AddOnProductRequestResponse: (const FOnProductsRequestResponseDelegate&) Delegate;
-(FDelegateHandle)AddOnTransactionComplete: (const FOnTransactionCompleteIOSDelegate&) Delegate;
-(FDelegateHandle)AddOnTransactionRestored: (const FOnTransactionRestoredIOSDelegate&) Delegate;
-(FDelegateHandle)AddOnRestoreTransactionsComplete: (const FOnRestoreTransactionsCompleteIOSDelegate&) Delegate;
-(FDelegateHandle)AddOnPurchaseInProgress: (const FOnTransactionProgressDelegate&) Delegate;
-(FDelegateHandle)AddOnTransactionDeferred: (const FOnTransactionProgressDelegate&) Delegate;

/**
 * Remove a transaction from the queue once it has been properly credited to the user
 *
 * @param receiptId id of the no longer pending transaction
 */
-(void)finalizeTransaction: (const FString&) receiptId;

/**
 * Make a purchase with the app store
 *
 * @param products array of SKProducts to purchase
 */
-(void)makePurchase:(NSArray*)products SimulateAskToBuy: (bool) bAskToBuy;

/**
 * Make a purchase with the app store
 *
 * @param products array of SKProducts to purchase
 * @param userId hashed and opaque representation of platform user id
 */
-(void)makePurchase:(NSArray*)products WithUserId: (const FString&) userId SimulateAskToBuy: (bool) bAskToBuy;

/**
 * Make a request for product information
 *
 * @param productIDs array of offer ids to get information for
 * @param delegate delegate to fire when operation completes
 */
-(void)requestProductData:(NSMutableSet*)productIDs WithDelegate : (const FOnQueryOnlineStoreOffersComplete&)delegate;

/**
 * Dumps the base64 encoded app receipt to log
 */
-(void)dumpAppReceipt;
@end
