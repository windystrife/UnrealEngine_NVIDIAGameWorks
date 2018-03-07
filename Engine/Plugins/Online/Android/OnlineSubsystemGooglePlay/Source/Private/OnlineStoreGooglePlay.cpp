// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineStoreGooglePlay.h"
#include "OnlineSubsystemGooglePlay.h"
#include "OnlineAsyncTaskGooglePlayQueryInAppPurchases.h"

#include "Internationalization.h"
#include "Internationalization/Culture.h"
#include "FastDecimalFormat.h"
#include "Misc/ConfigCacheIni.h"

FOnlineStoreGooglePlayV2::FOnlineStoreGooglePlayV2(FOnlineSubsystemGooglePlay* InSubsystem)
	: bIsQueryInFlight(false)
	, Subsystem(InSubsystem)
{
	UE_LOG(LogOnline, Verbose, TEXT( "FOnlineStoreGooglePlayV2::FOnlineStoreGooglePlayV2" ));
}

FOnlineStoreGooglePlayV2::FOnlineStoreGooglePlayV2()
	: bIsQueryInFlight(false)
	, Subsystem(nullptr)
{
	UE_LOG(LogOnline, Verbose, TEXT( "FOnlineStoreGooglePlayV2::FOnlineStoreGooglePlayV2 empty" ));
}

FOnlineStoreGooglePlayV2::~FOnlineStoreGooglePlayV2()
{
	if (Subsystem)
	{
		Subsystem->ClearOnGooglePlayAvailableIAPQueryCompleteDelegate_Handle(AvailableIAPQueryDelegateHandle);
	}
}

void FOnlineStoreGooglePlayV2::Init()
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineStoreGooglePlayV2::Init"));

	FOnGooglePlayAvailableIAPQueryCompleteDelegate Delegate = FOnGooglePlayAvailableIAPQueryCompleteDelegate::CreateThreadSafeSP(this, &FOnlineStoreGooglePlayV2::OnGooglePlayAvailableIAPQueryComplete);
	AvailableIAPQueryDelegateHandle = Subsystem->AddOnGooglePlayAvailableIAPQueryCompleteDelegate_Handle(Delegate);

	FString GooglePlayLicenseKey;
	if (!GConfig->GetString(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("GooglePlayLicenseKey"), GooglePlayLicenseKey, GEngineIni) || GooglePlayLicenseKey.IsEmpty())
	{
		UE_LOG_ONLINE(Warning, TEXT("Missing GooglePlayLicenseKey key in /Script/AndroidRuntimeSettings.AndroidRuntimeSettings of DefaultEngine.ini"));
	}

	extern void AndroidThunkCpp_Iap_SetupIapService(const FString&);
	AndroidThunkCpp_Iap_SetupIapService(GooglePlayLicenseKey);
}

TSharedRef<FOnlineStoreOffer> ConvertProductToStoreOffer(const FInAppPurchaseProductInfo& Product)
{
	TSharedRef<FOnlineStoreOffer> NewProductInfo = MakeShareable(new FOnlineStoreOffer());

	NewProductInfo->OfferId = Product.Identifier;

	FString Title = Product.DisplayName;
	int32 OpenParenIdx = -1;
	int32 CloseParenIdx = -1;
	if (Title.FindLastChar(TEXT(')'), CloseParenIdx) && Title.FindLastChar(TEXT('('), OpenParenIdx) && (OpenParenIdx < CloseParenIdx))
	{
		Title = Title.Left(OpenParenIdx).TrimEnd();
	}

	NewProductInfo->Title = FText::FromString(Title);
	NewProductInfo->Description = FText::FromString(Product.DisplayDescription); // Google has only one description, map it to (short) description to match iOS
	//NewProductInfo->LongDescription = FText::FromString(Product.DisplayDescription); // leave this empty so we know it's not set (client can apply more info from MCP)
	NewProductInfo->PriceText = FText::FromString(Product.DisplayPrice);
	NewProductInfo->CurrencyCode = Product.CurrencyCode;

	// Convert the backend stated price into its base units
	FInternationalization& I18N = FInternationalization::Get();
	const FCulture& Culture = *I18N.GetCurrentCulture();

	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetCurrencyFormattingRules(NewProductInfo->CurrencyCode);
	const FNumberFormattingOptions& FormattingOptions = FormattingRules.CultureDefaultFormattingOptions;
	double Val = static_cast<double>(Product.RawPrice) * static_cast<double>(FMath::Pow(10.0f, FormattingOptions.MaximumFractionalDigits));

	NewProductInfo->NumericPrice = FMath::TruncToInt(Val + 0.5);

	// Google doesn't support these fields, set to min and max defaults
	NewProductInfo->ReleaseDate = FDateTime::MinValue();
	NewProductInfo->ExpirationDate = FDateTime::MaxValue();

	return NewProductInfo;
}

void FOnlineStoreGooglePlayV2::OnGooglePlayAvailableIAPQueryComplete(EGooglePlayBillingResponseCode InResponseCode, const TArray<FInAppPurchaseProductInfo>& InProvidedProductInformation)
{ 
	TArray<FUniqueOfferId> OfferIds;
	for (const FInAppPurchaseProductInfo& Product : InProvidedProductInformation)
	{
		TSharedRef<FOnlineStoreOffer> NewProductOffer = ConvertProductToStoreOffer(Product);

		AddOffer(NewProductOffer);
		OfferIds.Add(NewProductOffer->OfferId);

		UE_LOG(LogOnline, Log, TEXT("Product Identifier: %s, Name: %s, Desc: %s, Long Desc: %s, Price: %s IntPrice: %d"),
			*NewProductOffer->OfferId,
			*NewProductOffer->Title.ToString(),
			*NewProductOffer->Description.ToString(),
			*NewProductOffer->LongDescription.ToString(),
			*NewProductOffer->PriceText.ToString(),
			NewProductOffer->NumericPrice);
	}

	if (CurrentQueryTask)
	{
		CurrentQueryTask->ProcessQueryAvailablePurchasesResults(InResponseCode);

		// clear the pointer, it will be destroyed by the async task manager
		CurrentQueryTask = nullptr;
	}
	else
	{
		UE_LOG(LogOnline, Log, TEXT("OnGooglePlayAvailableIAPQueryComplete: No IAP query task in flight"));
	}

	bIsQueryInFlight = false;
}

void FOnlineStoreGooglePlayV2::QueryCategories(const FUniqueNetId& UserId, const FOnQueryOnlineStoreCategoriesComplete& Delegate)
{
	Delegate.ExecuteIfBound(false, TEXT("No CatalogService"));
}

void FOnlineStoreGooglePlayV2::GetCategories(TArray<FOnlineStoreCategory>& OutCategories) const
{
	OutCategories.Empty();
}

void FOnlineStoreGooglePlayV2::QueryOffersByFilter(const FUniqueNetId& UserId, const FOnlineStoreFilter& Filter, const FOnQueryOnlineStoreOffersComplete& Delegate)
{
	Delegate.ExecuteIfBound(false, TArray<FUniqueOfferId>(), TEXT("No CatalogService"));
}

void FOnlineStoreGooglePlayV2::QueryOffersById(const FUniqueNetId& UserId, const TArray<FUniqueOfferId>& OfferIds, const FOnQueryOnlineStoreOffersComplete& Delegate)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineStoreGooglePlayV2::QueryOffersById"));

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
	CurrentQueryTask = new FOnlineAsyncTaskGooglePlayQueryInAppPurchasesV2(
		Subsystem,
		OfferIds,
		Delegate);
	Subsystem->QueueAsyncTask(CurrentQueryTask);

		bIsQueryInFlight = true;
	}
}

void FOnlineStoreGooglePlayV2::AddOffer(const TSharedRef<FOnlineStoreOffer>& NewOffer)
{
	TSharedRef<FOnlineStoreOffer>* Existing = CachedOffers.Find(NewOffer->OfferId);
	if (Existing != nullptr)
	{
		// Replace existing offer
		*Existing = NewOffer;
	}
	else
	{
		CachedOffers.Add(NewOffer->OfferId, NewOffer);
	}
}

void FOnlineStoreGooglePlayV2::GetOffers(TArray<FOnlineStoreOfferRef>& OutOffers) const
{
	for (const auto& CachedEntry : CachedOffers)
	{
		const TSharedRef<FOnlineStoreOffer>& CachedOffer = CachedEntry.Value;
		OutOffers.Add(CachedOffer);
	}
}

TSharedPtr<FOnlineStoreOffer> FOnlineStoreGooglePlayV2::GetOffer(const FUniqueOfferId& OfferId) const
{
	TSharedPtr<FOnlineStoreOffer> Result;

	const TSharedRef<FOnlineStoreOffer>* Existing = CachedOffers.Find(OfferId);
	if (Existing != nullptr)
	{
		Result = (*Existing);
	}

	return Result;
}
