// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlinePurchaseInterface.h"
#include "Serialization/JsonSerializerMacros.h"

enum class EGooglePlayBillingResponseCode : uint8;

/**
 * Holds in a common format the data that comes out of a Google purchase transaction
 */
struct FGoogleTransactionData
{
	FGoogleTransactionData(const FString& InOfferId, const FString& InProductToken, const FString& InReceiptData, const FString& InSignature);

	/** @return a string that prints useful debug information about this transaction */
	FString ToDebugString() const
	{
		return FString::Printf(TEXT("OfferId: %s TransactionId: %s ReceiptData: %s%s"),
			*OfferId,
			*TransactionIdentifier,
			*CombinedTransactionData.ToJson(),
			ErrorStr.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" Error: %s"), *ErrorStr));
	}

	/** @return offer id for this transaction */
	const FString& GetOfferId() const { return OfferId; }
	/** @return receipt data for this transaction */
	const FString GetCombinedReceiptData() const { return CombinedTransactionData.ToJson(); }
	/** @return receipt data for this transaction */
	const FString& GetReceiptData() const { return CombinedTransactionData.ReceiptData; }
	/** @return signature for this transaction */
	const FString& GetSignature() const { return CombinedTransactionData.Signature; }
	/** @return error string for this transaction, if applicable */
	const FString& GetErrorStr() const { return ErrorStr; }
	/** @return the transaction id */
	const FString& GetTransactionIdentifier() const	{ return TransactionIdentifier; }

private:

	/** Easy access to transmission of data required for backend validation */
	class FJsonReceiptData :
		public FJsonSerializable
	{
	public:

		FJsonReceiptData() {}
		FJsonReceiptData(const FString& InReceiptData, const FString& InSignature)
			: ReceiptData(InReceiptData)
			, Signature(InSignature)
		{ }

		/** Opaque store receipt data */
		FString ReceiptData;
		/** Signature associated with the transaction */
		FString Signature;

		// FJsonSerializable
		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE("receiptData", ReceiptData);
			JSON_SERIALIZE("signature", Signature);
		END_JSON_SERIALIZER
	};

	/** GooglePlay offer id */
	FString OfferId;
	/** Unique transaction id (purchaseToken) */
	FString TransactionIdentifier;
	/** Error on the transaction, if applicable */
	FString ErrorStr;
	/** Combined receipt with signature in JSON */
	FJsonReceiptData CombinedTransactionData;
};

/**
 * Info used to cache and track orders in progress.
 */
class FOnlinePurchasePendingTransactionGooglePlay
{
public:
	FOnlinePurchasePendingTransactionGooglePlay(
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
	}
	
	/**
	 * Generate a final receipt for all purchases made in this single transaction
	 */
	TSharedRef<FPurchaseReceipt> GenerateReceipt();
	
	/** Generate one off receipts for transactions initiated outside the current run of the application */
	static TSharedRef<FPurchaseReceipt> GenerateReceipt(const FGoogleTransactionData& Transaction);
	
	/** Add the single completed transaction to this transaction */
	bool AddCompletedOffer(EPurchaseTransactionState Result, const FGoogleTransactionData& Transaction);

public:
	
	/** Checkout info for the pending order */
	const FPurchaseCheckoutRequest CheckoutRequest;
	/** User for the pending order */
	const FUniqueNetIdString UserId;
	/** Delegate to call on completion */
	const FOnPurchaseCheckoutComplete CheckoutCompleteDelegate;
	/** Tracks the current state of the order */
	FPurchaseReceipt PendingPurchaseInfo;
};

/**
 * Implementation for online purchase via GooglePlay services
 */
class FOnlinePurchaseGooglePlay :
	public IOnlinePurchase,
	public TSharedFromThis<FOnlinePurchaseGooglePlay, ESPMode::ThreadSafe>
{

public:

	// IOnlinePurchase

	virtual bool IsAllowedToPurchase(const FUniqueNetId& UserId) override;
	virtual void Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& Delegate) override;
	virtual void FinalizePurchase(const FUniqueNetId& UserId, const FString& ReceiptId) override;
	virtual void RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate) override;
	virtual void QueryReceipts(const FUniqueNetId& UserId, bool bRestoreReceipts, const FOnQueryReceiptsComplete& Delegate) override;
	virtual void GetReceipts(const FUniqueNetId& UserId, TArray<FPurchaseReceipt>& OutReceipts) const override;
	
	// FOnlinePurchaseGooglePlay

	/**
	 * Constructor
	 *
	 * @param InSubsystem GooglePlay subsystem being used
	 */
	FOnlinePurchaseGooglePlay(class FOnlineSubsystemGooglePlay* InSubsystem);

	/**
	 * Constructor
	 */
	FOnlinePurchaseGooglePlay();

	/**
	 * Destructor
	 */
	virtual ~FOnlinePurchaseGooglePlay();
	
	/** Initialize the interface */
	void Init();

private:
	
	/** Mapping from user id to pending transaction */
	typedef TMap<FString, TSharedRef<FOnlinePurchasePendingTransactionGooglePlay> > FOnlinePurchasePendingTransactionMap;

	/** Mapping from user id to complete transactions */
	typedef TMap<FString, TArray< TSharedRef<FPurchaseReceipt> > > FOnlinePurchaseCompleteTransactionsMap;
	
	/** Array of transactions completion indirectly (previous run, etc) */
	typedef TArray< TSharedRef<FPurchaseReceipt> > FOnlineCompletedTransactions;
	
private:
	
	/** Are receipts being queried */
	bool bQueryingReceipts;
	/** Transient delegate to fire when query receipts has completed */
	FOnQueryReceiptsComplete QueryReceiptsComplete;

	/** Keeps track of pending user transactions */
	FOnlinePurchasePendingTransactionMap PendingTransactions;
	
	/** Cache of completed transactions */
	FOnlinePurchaseCompleteTransactionsMap CompletedTransactions;
	
	/** Cache of purchases completed outside the running instance */
	FOnlineCompletedTransactions OfflineTransactions;
	
	/** Reference to the parent subsystem */
	FOnlineSubsystemGooglePlay* Subsystem;

	/**
	 * Delegate fired when a purchase has completed
	 *
	 * @param InResponseCode response from the GooglePlay backend
	 * @param InTransactionData transaction data for the completed purchase
	 */
	void OnTransactionCompleteResponse(EGooglePlayBillingResponseCode InResponseCode, const FGoogleTransactionData& InTransactionData);
	FDelegateHandle ProcessPurchaseResultDelegateHandle;

	/**
	 * Delegate fired when a query for existing purchase has completed
	 *
	 * @param InResponseCode response from the GooglePlay backend
	 * @param InExistingPurchases known purchases for the user (non consumed or permanent)
	 */
	void OnQueryExistingPurchasesComplete(EGooglePlayBillingResponseCode InResponseCode, const TArray<FGoogleTransactionData>& InExistingPurchases);
	FDelegateHandle QueryExistingPurchasesCompleteDelegateHandle;
};

typedef TSharedPtr<FOnlinePurchaseGooglePlay, ESPMode::ThreadSafe> FOnlinePurchaseGooglePlayPtr;