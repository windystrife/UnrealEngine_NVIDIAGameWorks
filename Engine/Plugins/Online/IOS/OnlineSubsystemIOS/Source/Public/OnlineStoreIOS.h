// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineStoreInterfaceV2.h"

enum class EPurchaseTransactionState : uint8;
struct FStoreKitTransactionData;
@class FStoreKitHelperV2;
@class SKProductsResponse;
@class SKProduct;

/**
 * Implementation for online store via IOS services
 */
class FOnlineStoreIOS :
	public IOnlineStoreV2,
	public TSharedFromThis<FOnlineStoreIOS, ESPMode::ThreadSafe>
{
public:

	// IOnlineStoreV2

	virtual void QueryCategories(const FUniqueNetId& UserId, const FOnQueryOnlineStoreCategoriesComplete& Delegate) override;
	virtual void GetCategories(TArray<FOnlineStoreCategory>& OutCategories) const override;
	virtual void QueryOffersByFilter(const FUniqueNetId& UserId, const FOnlineStoreFilter& Filter, const FOnQueryOnlineStoreOffersComplete& Delegate) override;
	virtual void QueryOffersById(const FUniqueNetId& UserId, const TArray<FUniqueOfferId>& OfferIds, const FOnQueryOnlineStoreOffersComplete& Delegate) override;
	virtual void GetOffers(TArray<FOnlineStoreOfferRef>& OutOffers) const override;
	virtual TSharedPtr<FOnlineStoreOffer> GetOffer(const FUniqueOfferId& OfferId) const override;

	// FOnlineStoreIOS

	/**
	 * Constructor
	 *
	 * @param InSubsystem Online subsystem being used
	 */
	FOnlineStoreIOS(class FOnlineSubsystemIOS* InSubsystem);

	/**
	 * Constructor
	 */
	FOnlineStoreIOS();

	/**
	 * Destructor
	 */
	virtual ~FOnlineStoreIOS();
	
	/** Initialize the FStoreKitHelper for interaction with the app store */
	void InitStoreKit(FStoreKitHelperV2* InStoreKit);
	
	/**
	 * Get the production information for a given offer id
	 * Must have previously been retrieved vi QueryOffers*
	 *
	 * @return the SKProduct previously queried for product information
	 */
	SKProduct* GetSKProductByOfferId(const FUniqueOfferId& OfferId);

private:
	
	/**
	 * Representation of a single product offer
	 */
	struct FOnlineStoreOfferIOS
	{
		FOnlineStoreOfferIOS()
		: Product(nil)
		, Offer(nullptr)
		{
		}
		
		FOnlineStoreOfferIOS(SKProduct* InProduct, const FOnlineStoreOffer& InOffer)
		{
			Product = InProduct;
			[Product retain];
			
			Offer = MakeShareable(new FOnlineStoreOffer(InOffer));
		}
		
		FOnlineStoreOfferIOS(const FOnlineStoreOfferIOS& Other)
		{
			Product = Other.Product;
			[Product retain];
			Offer = Other.Offer;
		}
		
		FOnlineStoreOfferIOS& operator=(const FOnlineStoreOfferIOS& Other)
		{
			// Free
			[Product release];

			// Copy
			Product = Other.Product;
			[Product retain];
			Offer = Other.Offer;
			return *this;
		}
		
		~FOnlineStoreOfferIOS()
		{
			[Product release];
		}
		
		/** @return true if the store offer is valid/proper */
		bool IsValid() const
		{
			return Product != nil && Offer.IsValid();
		}
		
		/** Reference to the app store product information */
		SKProduct* Product;
		/** Product information about this offer */
		TSharedPtr<FOnlineStoreOffer> Offer;
	};

	void AddOffer(const FOnlineStoreOfferIOS& NewOffer);

private:

	/** delegate fired when a product request completes */
	void OnProductPurchaseRequestResponse(SKProductsResponse* Response, const FOnQueryOnlineStoreOffersComplete& CompletionDelegate);
	/** delegate fired when a single purchase transaction has completed (may be a part of multiple requests at once) */
	void OnTransactionCompleteResponse(EPurchaseTransactionState Result, const FStoreKitTransactionData& TransactionData);
	/** delegate fired when a single transaction is restored (may be a part of many restored purchases) */
	void OnTransactionRestored(const FStoreKitTransactionData& TransactionData);
	/** delegate fired when all transactions have been restored */
	void OnRestoreTransactionsComplete(EPurchaseTransactionState Result);
	
private:
	
	/** Mapping from offer id to product information */
	typedef TMap<FUniqueOfferId, FOnlineStoreOfferIOS> FOnlineOfferDescriptionMap;

	/** Mapping of all queried offers to their product information */
	FOnlineOfferDescriptionMap CachedOffers;

	/** Store kit helper for interfacing with app store */
	FStoreKitHelperV2* StoreHelper;

	/** Is a query already in flight */
	bool bIsQueryInFlight;
	
	/** Reference to the parent subsystem */
	FOnlineSubsystemIOS* Subsystem;
};

typedef TSharedPtr<FOnlineStoreIOS, ESPMode::ThreadSafe> FOnlineStoreIOSPtr;