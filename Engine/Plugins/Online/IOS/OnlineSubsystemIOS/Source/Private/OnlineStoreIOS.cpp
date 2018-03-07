// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemIOSPrivatePCH.h"
#include "OnlineStoreIOS.h"
#include "Internationalization.h"
#include "Internationalization/Culture.h"
#include "FastDecimalFormat.h"
#import "OnlineStoreKitHelper.h"

FOnlineStoreIOS::FOnlineStoreIOS(FOnlineSubsystemIOS* InSubsystem)
	: StoreHelper(nullptr)
	, bIsQueryInFlight(false)
	, Subsystem(InSubsystem)
{
	UE_LOG(LogOnline, Verbose, TEXT( "FOnlineStoreIOS::FOnlineStoreIOS" ));
}

FOnlineStoreIOS::FOnlineStoreIOS()
	: bIsQueryInFlight(false)
{
	UE_LOG(LogOnline, Verbose, TEXT( "FOnlineStoreIOS::FOnlineStoreIOS" ));
}

void FOnlineStoreIOS::InitStoreKit(FStoreKitHelperV2* InStoreKit)
{
	StoreHelper = InStoreKit;
	
	FOnProductsRequestResponseDelegate OnProductsRequestResponseDelegate;
	OnProductsRequestResponseDelegate.BindRaw(this, &FOnlineStoreIOS::OnProductPurchaseRequestResponse);
	[StoreHelper AddOnProductRequestResponse: OnProductsRequestResponseDelegate];

	// FOnTransactionCompleteIOSDelegate OnTransactionCompleteResponseDelegate = FOnTransactionCompleteIOSDelegate::CreateRaw(this, &FOnlineStoreIOS::OnTransactionCompleteResponse);
	// [StoreHelper AddOnTransactionComplete: OnTransactionCompleteResponseDelegate];
	
	// FOnTransactionRestoredIOSDelegate OnTransactionRestoredDelegate = FOnTransactionRestoredIOSDelegate::CreateRaw(this, &FOnlineStoreIOS::OnTransactionRestored);
	// [StoreHelper AddOnTransactionRestored: OnTransactionRestoredDelegate];
	
	// FOnRestoreTransactionsCompleteIOSDelegate OnRestoreTransactionsCompleteDelegate = FOnRestoreTransactionsCompleteIOSDelegate::CreateRaw(this, &FOnlineStoreIOS::OnRestoreTransactionsComplete);
	// [StoreHelper AddOnRestoreTransactionsComplete: OnRestoreTransactionsCompleteDelegate];
}

FOnlineStoreIOS::~FOnlineStoreIOS()
{
	[StoreHelper release];
}

void FOnlineStoreIOS::QueryCategories(const FUniqueNetId& UserId, const FOnQueryOnlineStoreCategoriesComplete& Delegate)
{
	Delegate.ExecuteIfBound(false, TEXT("No CatalogService"));
}

void FOnlineStoreIOS::GetCategories(TArray<FOnlineStoreCategory>& OutCategories) const
{
	OutCategories.Empty();
}

void FOnlineStoreIOS::QueryOffersByFilter(const FUniqueNetId& UserId, const FOnlineStoreFilter& Filter, const FOnQueryOnlineStoreOffersComplete& Delegate)
{
	Delegate.ExecuteIfBound(false, TArray<FUniqueOfferId>(), TEXT("No CatalogService"));
}

void FOnlineStoreIOS::QueryOffersById(const FUniqueNetId& UserId, const TArray<FUniqueOfferId>& OfferIds, const FOnQueryOnlineStoreOffersComplete& Delegate)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineStoreIOS::QueryOffersById"));
	
	if (bIsQueryInFlight)
	{
		Delegate.ExecuteIfBound(false, OfferIds, TEXT("Request already in flight"));
	}
	else if (OfferIds.Num() == 0)
	{
		Delegate.ExecuteIfBound(false, OfferIds, TEXT("No offers to query for"));
	}
	else
	{
		// autoreleased NSSet to hold IDs
		NSMutableSet* ProductSet = [NSMutableSet setWithCapacity:OfferIds.Num()];
		for (int32 OfferIdx = 0; OfferIdx < OfferIds.Num(); OfferIdx++)
		{
			NSString* ID = [NSString stringWithFString:OfferIds[OfferIdx]];
			// convert to NSString for the set objects
			[ProductSet addObject:ID];
		}
		
		FOnQueryOnlineStoreOffersComplete DelegateCopy = Delegate;
		dispatch_async(dispatch_get_main_queue(), ^
        {
			[StoreHelper requestProductData:ProductSet WithDelegate:DelegateCopy];
		});
		
		bIsQueryInFlight = true;
	}
}

/**
 * Convert an Apple SKProduct reference into a engine FOnlineStoreOffer
 * (Apple has only Title/Description converted to Title/(short)Description)
 *
 * @param Product information about an Apple offer from iTunesConnect
 *
 * @return FOnlineStoreOffer with proper parameters filled in
 */
FOnlineStoreOffer ConvertProductToStoreOffer(SKProduct* Product)
{
	FOnlineStoreOffer NewProductInfo;
	
	NewProductInfo.OfferId = [Product productIdentifier];
	
	NewProductInfo.Title = FText::FromString([Product localizedTitle]);
	NewProductInfo.Description = FText::FromString([Product localizedDescription]);
	//NewProductInfo.LongDescription = FText::FromString([Product localizedDescription]);
	NewProductInfo.CurrencyCode = [Product.priceLocale objectForKey : NSLocaleCurrencyCode];
	
	// Convert the backend stated price into its base units
	FInternationalization& I18N = FInternationalization::Get();
	const FCulture& Culture = *I18N.GetCurrentCulture();
	
	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetCurrencyFormattingRules(NewProductInfo.CurrencyCode);
	const FNumberFormattingOptions& FormattingOptions = FormattingRules.CultureDefaultFormattingOptions;
	double Val = static_cast<double>([Product.price doubleValue]) * static_cast<double>(FMath::Pow(10.0f, FormattingOptions.MaximumFractionalDigits));
	NewProductInfo.NumericPrice = FMath::TruncToInt(Val + 0.5);
	
	// iOS doesn't support these fields, set to min and max defaults
	NewProductInfo.ReleaseDate = FDateTime::MinValue();
	NewProductInfo.ExpirationDate = FDateTime::MaxValue();

	NewProductInfo.PriceText = FText::AsCurrencyBase(NewProductInfo.NumericPrice, NewProductInfo.CurrencyCode);
	
	return NewProductInfo;
}

void FOnlineStoreIOS::AddOffer(const FOnlineStoreOfferIOS& NewOffer)
{
	if (NewOffer.IsValid())
	{
		FOnlineStoreOfferIOS* Existing = CachedOffers.Find(NewOffer.Offer->OfferId);
		if (Existing != nullptr)
		{
			// Replace existing offer
			*Existing = NewOffer;
		}
		else
		{
			CachedOffers.Add(NewOffer.Offer->OfferId, NewOffer);
		}
	}
}

void FOnlineStoreIOS::OnProductPurchaseRequestResponse(SKProductsResponse* Response, const FOnQueryOnlineStoreOffersComplete& CompletionDelegate)
{
	if(bIsQueryInFlight)
	{
		bool bWasSuccessful = [Response.products count] > 0;
		int32 NumInvalidProducts = [Response.invalidProductIdentifiers count];
		if ([Response.products count] == 0 && NumInvalidProducts == 0)
		{
			UE_LOG(LogOnline, Warning, TEXT("Wrong number of products [%d] in the response when trying to make a single purchase"), [Response.products count]);
		}
		
		TArray<FUniqueOfferId> OfferIds;
		for (SKProduct* Product in Response.products)
		{
			FOnlineStoreOfferIOS NewProductOffer(Product, ConvertProductToStoreOffer(Product));
			
			AddOffer(NewProductOffer);
			OfferIds.Add(NewProductOffer.Offer->OfferId);
			
			UE_LOG(LogOnline, Log, TEXT("Product Identifier: %s, Name: %s, Desc: %s, Long Desc: %s, Price: %s IntPrice: %d"),
				   *NewProductOffer.Offer->OfferId,
				   *NewProductOffer.Offer->Title.ToString(),
				   *NewProductOffer.Offer->Description.ToString(),
				   *NewProductOffer.Offer->LongDescription.ToString(),
				   *NewProductOffer.Offer->PriceText.ToString(),
				   NewProductOffer.Offer->NumericPrice);
		}
		
		for (NSString *invalidProduct in Response.invalidProductIdentifiers)
		{
			UE_LOG(LogOnline, Warning, TEXT("Problem in iTunes connect configuration for product: %s"), *FString(invalidProduct));
		}
		
		CompletionDelegate.ExecuteIfBound(bWasSuccessful, OfferIds, TEXT(""));
		bIsQueryInFlight = false;
	}
}

void FOnlineStoreIOS::OnTransactionCompleteResponse(EPurchaseTransactionState Result, const FStoreKitTransactionData& TransactionData)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineStoreIOS::OnTransactionCompleteResponse"));
}

void FOnlineStoreIOS::OnTransactionRestored(const FStoreKitTransactionData& TransactionData)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineStoreIOS::OnTransactionRestored"));
}

void FOnlineStoreIOS::OnRestoreTransactionsComplete(EPurchaseTransactionState Result)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineStoreIOS::OnRestoreTransactionsComplete"));	
}

void FOnlineStoreIOS::GetOffers(TArray<FOnlineStoreOfferRef>& OutOffers) const
{
	for (const auto& CachedEntry : CachedOffers)
	{
		const FOnlineStoreOfferIOS& CachedOffer = CachedEntry.Value;
		if (CachedOffer.IsValid())
		{
			OutOffers.Add(CachedOffer.Offer.ToSharedRef());
		}
	}
}

TSharedPtr<FOnlineStoreOffer> FOnlineStoreIOS::GetOffer(const FUniqueOfferId& OfferId) const
{
	TSharedPtr<FOnlineStoreOffer> Result;

	const FOnlineStoreOfferIOS* Existing = CachedOffers.Find(OfferId);
	if (Existing != nullptr && Existing->IsValid())
	{
		Result = Existing->Offer;
	}
	
	return Result;
}

SKProduct* FOnlineStoreIOS::GetSKProductByOfferId(const FUniqueOfferId& OfferId)
{
	const FOnlineStoreOfferIOS* Existing = CachedOffers.Find(OfferId);
	if (Existing != nullptr && Existing->IsValid())
	{
		return Existing->Product;
	}
	
	return nil;
}