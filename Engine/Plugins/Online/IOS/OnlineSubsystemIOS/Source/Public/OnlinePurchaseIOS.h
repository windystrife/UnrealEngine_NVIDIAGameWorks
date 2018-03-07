// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlinePurchaseInterface.h"

struct FStoreKitTransactionData;
@class FStoreKitHelperV2;

/**
 * Info used to cache and track orders in progress.
 */
class FOnlinePurchasePendingTransactionIOS
{
public:
	FOnlinePurchasePendingTransactionIOS(
		const FPurchaseCheckoutRequest& InCheckoutRequest,
		const FUniqueNetId& InUserId,
		const EPurchaseTransactionState InPendingTransactionState = EPurchaseTransactionState::NotStarted,
		const FOnPurchaseCheckoutComplete& InCheckoutCompleteDelegate = FOnPurchaseCheckoutComplete()
		)
		: CheckoutRequest(InCheckoutRequest)
		, UserId(InUserId)
		, CheckoutCompleteDelegate(InCheckoutCompleteDelegate)
	{
		PendingPurchaseInfo.TransactionState = InPendingTransactionState;
		
		// Setup purchase state for all pending offers
		OfferPurchaseStates.AddZeroed(CheckoutRequest.PurchaseOffers.Num());
	}
	
	/**
	 * Generate a final receipt for all purchases made in this single transaction
	 */
	TSharedRef<FPurchaseReceipt> GenerateReceipt();
	
	/** Generate one off receipts for transactions initiated outside the current run of the application */
	static TSharedRef<FPurchaseReceipt> GenerateReceipt(EPurchaseTransactionState Result, const FStoreKitTransactionData& Transaction);
	
	/** Mark this pending purchase as started */
	void StartProcessing();
	/** Add a single completed transaction (one of possibly many) to this transaction */
	bool AddCompletedOffer(EPurchaseTransactionState Result, const FStoreKitTransactionData& Transaction);
	/** @return true if all offers to purchase have completed transactions */
	bool AreAllOffersComplete() const;
	/** @return result of all purchases, if any fail it will return failed, if any cancel it will return canceled */
	EPurchaseTransactionState GetFinalTransactionState() const;
	
public:
	
	/** Checkout info for the pending order */
	const FPurchaseCheckoutRequest CheckoutRequest;
	/** Mirror array of purchase state for the various offers to purchase */
	TArray<EPurchaseTransactionState> OfferPurchaseStates;
	/** User for the pending order */
	const FUniqueNetIdString UserId;
	/** Delegate to call on completion */
	const FOnPurchaseCheckoutComplete CheckoutCompleteDelegate;
	/** Tracks the current state of the order */
	FPurchaseReceipt PendingPurchaseInfo;
};

/**
 * Implementation for online purchase via IOS services
 */
class FOnlinePurchaseIOS :
	public IOnlinePurchase,
	public TSharedFromThis<FOnlinePurchaseIOS, ESPMode::ThreadSafe>
{

public:

	// IOnlinePurchase

	virtual bool IsAllowedToPurchase(const FUniqueNetId& UserId) override;
	virtual void Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& Delegate) override;
	virtual void FinalizePurchase(const FUniqueNetId& UserId, const FString& ReceiptId) override;
	virtual void RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate) override;
	virtual void QueryReceipts(const FUniqueNetId& UserId, bool bRestoreReceipts, const FOnQueryReceiptsComplete& Delegate) override;
	virtual void GetReceipts(const FUniqueNetId& UserId, TArray<FPurchaseReceipt>& OutReceipts) const override;
	
	// FOnlinePurchaseIOS

	/**
	 * Constructor
	 *
	 * @param InSubsystem IOS subsystem being used
	 */
	FOnlinePurchaseIOS(class FOnlineSubsystemIOS* InSubsystem);

	/**
	 * Constructor
	 */
	FOnlinePurchaseIOS();

	/**
	 * Destructor
	 */
	virtual ~FOnlinePurchaseIOS();
	
	/** Initialize the FStoreKitHelper for interaction with the app store */
	void InitStoreKit(FStoreKitHelperV2* InStoreKit);
	
	
private:
	
	/** delegate fired when a product request completes */
	void OnProductPurchaseRequestResponse(SKProductsResponse* Response, const FOnQueryOnlineStoreOffersComplete& CompletionDelegate);
	/** delegate fired when a single purchase transaction has completed (may be a part of multiple requests at once) */
	void OnTransactionCompleteResponse(EPurchaseTransactionState Result, const FStoreKitTransactionData& TransactionData);
	/** delegate fired when a single transaction is restored (may be a part of many restored purchases) */
	void OnTransactionRestored(const FStoreKitTransactionData& TransactionData);
	/** delegate fired when all transactions have been restored */
	void OnRestoreTransactionsComplete(EPurchaseTransactionState Result);
	
	void OnTransactionInProgress(const FStoreKitTransactionData& TransactionData);
	void OnTransactionDeferred(const FStoreKitTransactionData& TransactionData);
	
private:
	
	/** Mapping from user id to pending transaction */
	typedef TMap<FString, TSharedRef<FOnlinePurchasePendingTransactionIOS> > FOnlinePurchasePendingTransactionMap;

	/** Mapping from user id to complete transactions */
	typedef TMap<FString, TArray< TSharedRef<FPurchaseReceipt> > > FOnlinePurchaseCompleteTransactionsMap;
	
	/** Array of transactions completion indirectly (previous run, etc) */
	typedef TArray< TSharedRef<FPurchaseReceipt> > FOnlineCompletedTransactions;
	
private:
	
	/** Store kit helper for interfacing with app store (owned by main OnlineSubsystem) */
	FStoreKitHelperV2* StoreHelper;
	
	/** Are transactions current being restored */
	bool bRestoringTransactions;
	/** Transient delegate to fire when query receipts has completed, when restoring transactions */
	FOnQueryReceiptsComplete QueryReceiptsComplete;
	
	/** Keeps track of pending user transactions */
	FOnlinePurchasePendingTransactionMap PendingTransactions;
	
	/** Cache of completed transactions */
	FOnlinePurchaseCompleteTransactionsMap CompletedTransactions;
	
	/** Cache of purchases completed outside the running instance */
	FOnlineCompletedTransactions OfflineTransactions;
	
	/** Reference to the parent subsystem */
	FOnlineSubsystemIOS* Subsystem;
};

typedef TSharedPtr<FOnlinePurchaseIOS, ESPMode::ThreadSafe> FOnlinePurchaseIOSPtr;