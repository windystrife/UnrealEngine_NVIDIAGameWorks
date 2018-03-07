// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlinePurchaseGooglePlay.h"
#include "OnlineSubsystemGooglePlay.h"
#include "OnlineStoreGooglePlay.h"
#include "OnlineError.h"
#include "Base64.h"

#define LOCTEXT_NAMESPACE "OnlineSubsystemGooglePlay"
#define GOOGLEPLAYUSER TEXT("GooglePlayUser")

FGoogleTransactionData::FGoogleTransactionData(const FString& InOfferId, const FString& InProductToken, const FString& InReceiptData, const FString& InSignature)
	: OfferId(InOfferId)
	, TransactionIdentifier(InProductToken)
	, CombinedTransactionData(InReceiptData, InSignature)
{
	if (TransactionIdentifier.IsEmpty())
	{
		ErrorStr = TEXT("Receipt does not contain purchase token");
	}
	else if (CombinedTransactionData.ReceiptData.IsEmpty())
	{
		ErrorStr = TEXT("Receipt does not contain receipt data");
	}
	else if (CombinedTransactionData.Signature.IsEmpty())
	{
		ErrorStr = TEXT("Receipt does not contain signature data");
	}
}

FOnlinePurchaseGooglePlay::FOnlinePurchaseGooglePlay(FOnlineSubsystemGooglePlay* InSubsystem)
	: bQueryingReceipts(false)
	, Subsystem(InSubsystem)
{
	UE_LOG(LogOnline, Verbose, TEXT( "FOnlinePurchaseGooglePlay::FOnlinePurchaseGooglePlay" ));
}

FOnlinePurchaseGooglePlay::FOnlinePurchaseGooglePlay()
{
	UE_LOG(LogOnline, Verbose, TEXT( "FOnlinePurchaseGooglePlay::FOnlinePurchaseGooglePlay" ));
}

FOnlinePurchaseGooglePlay::~FOnlinePurchaseGooglePlay()
{
	if (Subsystem)
	{
		Subsystem->ClearOnGooglePlayProcessPurchaseCompleteDelegate_Handle(ProcessPurchaseResultDelegateHandle);
		Subsystem->ClearOnGooglePlayQueryExistingPurchasesCompleteDelegate_Handle(QueryExistingPurchasesCompleteDelegateHandle);
	}
}

void FOnlinePurchaseGooglePlay::Init()
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlinePurchaseGooglePlay::Init"));

	FOnGooglePlayProcessPurchaseCompleteDelegate PurchaseCompleteDelegate = FOnGooglePlayProcessPurchaseCompleteDelegate::CreateThreadSafeSP(this, &FOnlinePurchaseGooglePlay::OnTransactionCompleteResponse);
	ProcessPurchaseResultDelegateHandle = Subsystem->AddOnGooglePlayProcessPurchaseCompleteDelegate_Handle(PurchaseCompleteDelegate);

	FOnGooglePlayQueryExistingPurchasesCompleteDelegate QueryExistingPurchasesCompleteDelegate = FOnGooglePlayQueryExistingPurchasesCompleteDelegate::CreateThreadSafeSP(this, &FOnlinePurchaseGooglePlay::OnQueryExistingPurchasesComplete);
	QueryExistingPurchasesCompleteDelegateHandle = Subsystem->AddOnGooglePlayQueryExistingPurchasesCompleteDelegate_Handle(QueryExistingPurchasesCompleteDelegate);
}

bool FOnlinePurchaseGooglePlay::IsAllowedToPurchase(const FUniqueNetId& UserId)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlinePurchaseGooglePlay::IsAllowedToPurchase"));
	extern bool AndroidThunkCpp_Iap_IsAllowedToMakePurchases();
	return AndroidThunkCpp_Iap_IsAllowedToMakePurchases();
}

void FOnlinePurchaseGooglePlay::Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& Delegate)
{
	bool bStarted = false;
	FText ErrorMessage;

	TSharedRef<FOnlinePurchasePendingTransactionGooglePlay> RequestedTransaction = MakeShareable(new FOnlinePurchasePendingTransactionGooglePlay(CheckoutRequest, UserId, EPurchaseTransactionState::NotStarted, Delegate));

	if (IsAllowedToPurchase(UserId))
	{
		const FString UserIdStr = GOOGLEPLAYUSER;
		const TSharedRef<FOnlinePurchasePendingTransactionGooglePlay>* UserPendingTransaction = PendingTransactions.Find(UserIdStr);
		if (UserPendingTransaction == nullptr)
		{
			FOnlineStoreGooglePlayV2Ptr StoreInterface = StaticCastSharedPtr<FOnlineStoreGooglePlayV2>(Subsystem->GetStoreV2Interface());
			if (StoreInterface.IsValid())
			{
				TSharedRef<FOnlinePurchasePendingTransactionGooglePlay> PendingTransaction = PendingTransactions.Add(UserIdStr, RequestedTransaction);

				int32 NumOffers = CheckoutRequest.PurchaseOffers.Num();
				if (NumOffers > 0)
				{
					const FPurchaseCheckoutRequest::FPurchaseOfferEntry& Offer = CheckoutRequest.PurchaseOffers[0];

					extern bool AndroidThunkCpp_Iap_BeginPurchase(const FString&);
					bStarted = AndroidThunkCpp_Iap_BeginPurchase(Offer.OfferId);
					UE_LOG(LogOnline, Display, TEXT("Created Transaction? - %s"),
						bStarted ? TEXT("Created a transaction.") : TEXT("Failed to create a transaction."));

					RequestedTransaction->PendingPurchaseInfo.TransactionState = bStarted ? EPurchaseTransactionState::Processing : EPurchaseTransactionState::Failed;
					if (NumOffers > 1)
					{
						UE_LOG(LogOnline, Warning, TEXT("GooglePlay supports purchasing one offer at a time, %d were requested and ignored"), NumOffers - 1);
					}
				}
				else
				{
					ErrorMessage = NSLOCTEXT("GooglePlayPurchase", "ErrorNoOffersSpecified", "Failed to checkout, no offers given.");
					RequestedTransaction->PendingPurchaseInfo.TransactionState = EPurchaseTransactionState::Failed;
				}
			}
		}
		else
		{
			ErrorMessage = NSLOCTEXT("GooglePlayPurchase", "ErrorTransactionInProgress", "Failed to checkout, user has in progress transaction.");
			RequestedTransaction->PendingPurchaseInfo.TransactionState = EPurchaseTransactionState::Failed;
		}
	}
	else
	{
		ErrorMessage = NSLOCTEXT("GooglePlayPurchase", "ErrorPurchaseNotAllowed", "Failed to checkout, user not allowed to purchase.");
		RequestedTransaction->PendingPurchaseInfo.TransactionState = EPurchaseTransactionState::Failed;
	}

	if (!bStarted)
	{
		TSharedRef<FPurchaseReceipt> FailReceipt = RequestedTransaction->GenerateReceipt();

		Subsystem->ExecuteNextTick([ErrorMessage, FailReceipt, Delegate]()
		{
			FOnlineError Error(ErrorMessage);
			Delegate.ExecuteIfBound(Error, FailReceipt);
		});
	}
}

void FOnlinePurchaseGooglePlay::FinalizePurchase(const FUniqueNetId& UserId, const FString& ReceiptId)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlinePurchaseGooglePlay::FinalizePurchase %s %s"), *UserId.ToString(), *ReceiptId);
	extern void AndroidThunkCpp_Iap_ConsumePurchase(const FString&);
	AndroidThunkCpp_Iap_ConsumePurchase(ReceiptId);
}

void FOnlinePurchaseGooglePlay::RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate)
{
	// Not supported
	FOnlineError Result;
	Delegate.ExecuteIfBound(Result, MakeShareable(new FPurchaseReceipt()));
}

void FOnlinePurchaseGooglePlay::QueryReceipts(const FUniqueNetId& UserId, bool bRestoreReceipts, const FOnQueryReceiptsComplete& Delegate)
{
	bool bSuccess = false;
	bool bTriggerDelegate = true;
	if (!bQueryingReceipts)
	{
		/**
		 * bRestoreReceipts is irrelevant because GooglePlay requires the client to consume purchase in order to make a new purchase
		 * There is no concept of restore here, any purchase query will reveal any non consumed purchases. They will remain in the list until the the game
		 * consumes it via FinalizePurchase().
		 */
		bQueryingReceipts = true;
		QueryReceiptsComplete = Delegate;

		extern bool AndroidThunkCpp_Iap_QueryExistingPurchases();
		if (AndroidThunkCpp_Iap_QueryExistingPurchases())
		{
			bTriggerDelegate = false;
			bSuccess = true;
		}
		else
		{
			UE_LOG(LogOnline, Verbose, TEXT("FOnlinePurchaseGooglePlay::QueryReceipts failed to start query"));
		}
	}
	else
	{
		UE_LOG(LogOnline, Verbose, TEXT("FOnlinePurchaseGooglePlay::QueryReceipts already in progress."));
	}

	if (bTriggerDelegate)
	{
		Subsystem->ExecuteNextTick([this, Delegate, bSuccess]() {
			FOnlineError Result(bSuccess);
			Delegate.ExecuteIfBound(Result);
			bQueryingReceipts = false;
			QueryReceiptsComplete.Unbind();
		});
	}
}

void FOnlinePurchaseGooglePlay::GetReceipts(const FUniqueNetId& UserId, TArray<FPurchaseReceipt>& OutReceipts) const
{
	OutReceipts.Empty();

	// Add the cached list of user purchases
	FString UserIdStr = GOOGLEPLAYUSER;
	const TArray<TSharedRef<FPurchaseReceipt>>* UserCompletedTransactionsPtr = CompletedTransactions.Find(UserIdStr);
	if (UserCompletedTransactionsPtr != nullptr)
	{
		const TArray<TSharedRef<FPurchaseReceipt>>& UserCompletedTransactions = *UserCompletedTransactionsPtr;
		for (int32 Idx = 0; Idx < UserCompletedTransactions.Num(); Idx++)
		{
			const TSharedRef<FPurchaseReceipt>& Transaction = UserCompletedTransactions[Idx];
			OutReceipts.Add(*Transaction);
		}
	}
	
	// Add purchases completed while "offline"
	for (int32 Idx = 0; Idx < OfflineTransactions.Num(); Idx++)
	{
		OutReceipts.Add(*OfflineTransactions[Idx]);
	}
}

void FOnlinePurchaseGooglePlay::OnTransactionCompleteResponse(EGooglePlayBillingResponseCode InResponseCode, const FGoogleTransactionData& InTransactionData)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlinePurchaseGooglePlay::OnTransactionCompleteResponse %s"), ToString(InResponseCode));
	UE_LOG(LogOnline, Verbose, TEXT("3... Transaction: %s"), *InTransactionData.ToDebugString());
	EPurchaseTransactionState Result = ConvertGPResponseCodeToPurchaseTransactionState(InResponseCode);

	FString UserIdStr = GOOGLEPLAYUSER;
	const TSharedRef<FOnlinePurchasePendingTransactionGooglePlay>* UserPendingTransactionPtr = PendingTransactions.Find(UserIdStr);
	if(UserPendingTransactionPtr != nullptr)
	{
		const TSharedRef<FOnlinePurchasePendingTransactionGooglePlay> UserPendingTransaction = *UserPendingTransactionPtr;
		const FString& ErrorStr = InTransactionData.GetErrorStr();

		if (Result == EPurchaseTransactionState::Purchased || Result == EPurchaseTransactionState::Restored)
		{
			if (!UserPendingTransaction->AddCompletedOffer(Result, InTransactionData))
			{
				UE_LOG(LogOnline, Verbose, TEXT("Offer not found in pending transaction"));
				Result = EPurchaseTransactionState::Failed;
			}
		}

		UserPendingTransaction->PendingPurchaseInfo.TransactionState = Result;
		UserPendingTransaction->PendingPurchaseInfo.TransactionId = InTransactionData.GetTransactionIdentifier();

		FOnlineError FinalResult;
		switch (Result)
		{
			case EPurchaseTransactionState::Failed:
				FinalResult.SetFromErrorCode(TEXT("com.epicgames.purchase.failure"));
				FinalResult.ErrorMessage = !ErrorStr.IsEmpty() ? FText::FromString(ErrorStr) : LOCTEXT("GooglePlayTransactionFailed", "Transaction Failed");
				break;
			case EPurchaseTransactionState::Canceled:
				FinalResult.SetFromErrorCode(TEXT("com.epicgames.catalog_helper.user_cancelled"));
				FinalResult.ErrorMessage = !ErrorStr.IsEmpty() ? FText::FromString(ErrorStr) : LOCTEXT("GooglePlayTransactionCancel", "Transaction Canceled");
				break;
			case EPurchaseTransactionState::Purchased:
				FinalResult.bSucceeded = true;
				break;
			case EPurchaseTransactionState::Invalid:
				FinalResult.SetFromErrorCode(TEXT("com.epicgames.purchase.invalid"));
				FinalResult.ErrorMessage = !ErrorStr.IsEmpty() ? FText::FromString(ErrorStr) : LOCTEXT("GooglePlayInvalidState", "Invalid purchase result");
				UserPendingTransaction->PendingPurchaseInfo.TransactionState = EPurchaseTransactionState::Invalid;
				break;
			default:
				UE_LOG(LogOnline, Warning, TEXT("Unexpected state after purchase %d"), Result);
				FinalResult.SetFromErrorCode(TEXT("com.epicgames.purchase.unexpected_state"));
				FinalResult.ErrorMessage = !ErrorStr.IsEmpty() ? FText::FromString(ErrorStr) : LOCTEXT("GooglePlayUnexpectedState", "Unexpected purchase result");
				UserPendingTransaction->PendingPurchaseInfo.TransactionState = EPurchaseTransactionState::Failed;
				break;
		}

		TSharedRef<FPurchaseReceipt> FinalReceipt = UserPendingTransaction->GenerateReceipt();

		TArray< TSharedRef<FPurchaseReceipt> >& UserCompletedTransactions = CompletedTransactions.FindOrAdd(UserIdStr);

		PendingTransactions.Remove(UserIdStr);
		UserCompletedTransactions.Add(FinalReceipt);

		UserPendingTransaction->CheckoutCompleteDelegate.ExecuteIfBound(FinalResult, FinalReceipt);
	}
	else
	{
		UE_LOG(LogOnline, Log, TEXT("No pending transaction found associated with this purchase completion event"));
	}
}

void FOnlinePurchaseGooglePlay::OnQueryExistingPurchasesComplete(EGooglePlayBillingResponseCode InResponseCode, const TArray<FGoogleTransactionData>& InExistingPurchases)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlinePurchaseGooglePlay::OnQueryExistingPurchasesComplete Response: %s Num: %d"), ToString(InResponseCode), InExistingPurchases.Num());

	if (bQueryingReceipts)
	{
		bool bSuccess = (InResponseCode == EGooglePlayBillingResponseCode::Ok);
		if (bSuccess)
		{
			EPurchaseTransactionState Result = ConvertGPResponseCodeToPurchaseTransactionState(InResponseCode);
			for (const FGoogleTransactionData& Purchase : InExistingPurchases)
			{
				UE_LOG(LogOnline, Verbose, TEXT("Adding existing receipt %s"), *Purchase.ToDebugString());
				TSharedRef<FPurchaseReceipt> OfflineReceipt = FOnlinePurchasePendingTransactionGooglePlay::GenerateReceipt(Purchase);
				OfflineTransactions.Add(OfflineReceipt);
			}
		}
		else
		{
			UE_LOG(LogOnline, Verbose, TEXT("OnQueryExistingPurchasesComplete failed"));
		}

		Subsystem->ExecuteNextTick([this, bSuccess]()
		{
			FOnlineError Result(bSuccess);
			QueryReceiptsComplete.ExecuteIfBound(Result);
			bQueryingReceipts = false;
			QueryReceiptsComplete.Unbind();
		});
	}
	else
	{
		UE_LOG(LogOnline, Warning, TEXT("FOnlinePurchaseGooglePlay::OnQueryExistingPurchasesComplete unexpected call"));
	}
}

TSharedRef<FPurchaseReceipt> FOnlinePurchasePendingTransactionGooglePlay::GenerateReceipt()
{
	TSharedRef<FPurchaseReceipt> Receipt = MakeShareable(new FPurchaseReceipt());
	
	Receipt->TransactionState = PendingPurchaseInfo.TransactionState;
	Receipt->TransactionId = PendingPurchaseInfo.TransactionId;
	
	if(PendingPurchaseInfo.TransactionState == EPurchaseTransactionState::Purchased ||
	   PendingPurchaseInfo.TransactionState == EPurchaseTransactionState::Restored)
	{
		Receipt->ReceiptOffers = PendingPurchaseInfo.ReceiptOffers;
	}
	else
	{
		// Add the requested offers to the receipt in the event of an incomplete purchase.
		for(const auto& RequestedOffer : CheckoutRequest.PurchaseOffers)
		{
			Receipt->AddReceiptOffer(RequestedOffer.OfferNamespace, RequestedOffer.OfferId, RequestedOffer.Quantity);
		}
	}
	
	return Receipt;
}

TSharedRef<FPurchaseReceipt> FOnlinePurchasePendingTransactionGooglePlay::GenerateReceipt(const FGoogleTransactionData& Transaction)
{
	TSharedRef<FPurchaseReceipt> Receipt = MakeShareable(new FPurchaseReceipt());
	
	Receipt->TransactionState = Transaction.GetErrorStr().IsEmpty() ? EPurchaseTransactionState::Purchased : EPurchaseTransactionState::Failed;
	Receipt->TransactionId = Transaction.GetTransactionIdentifier();
	
	if (Receipt->TransactionState == EPurchaseTransactionState::Purchased ||
		Receipt->TransactionState == EPurchaseTransactionState::Restored)
	{
		FPurchaseReceipt::FReceiptOfferEntry ReceiptEntry(TEXT(""), Transaction.GetOfferId(), 1);

		int32 Idx = ReceiptEntry.LineItems.AddZeroed();

		FPurchaseReceipt::FLineItemInfo& LineItem = ReceiptEntry.LineItems[Idx];

		LineItem.ItemName = Transaction.GetOfferId();
		LineItem.UniqueId = Transaction.GetTransactionIdentifier();
		LineItem.ValidationInfo = Transaction.GetCombinedReceiptData();

		Receipt->AddReceiptOffer(ReceiptEntry);
	}
	
	return Receipt;
}

bool FOnlinePurchasePendingTransactionGooglePlay::AddCompletedOffer(EPurchaseTransactionState Result, const FGoogleTransactionData& Transaction)
{
	for (int32 OfferIdx = 0; OfferIdx < CheckoutRequest.PurchaseOffers.Num(); ++OfferIdx)
	{
		const FPurchaseCheckoutRequest::FPurchaseOfferEntry& Offer = CheckoutRequest.PurchaseOffers[OfferIdx];
		if (Transaction.GetOfferId() == Offer.OfferId)
		{
			FPurchaseReceipt::FReceiptOfferEntry Receipt(TEXT(""), Transaction.GetOfferId(), 1);

			int32 Idx = Receipt.LineItems.AddZeroed();
			
			FPurchaseReceipt::FLineItemInfo& LineItem = Receipt.LineItems[Idx];
			
			LineItem.ItemName = Transaction.GetOfferId();
			LineItem.UniqueId = Transaction.GetTransactionIdentifier();
			LineItem.ValidationInfo = Transaction.GetCombinedReceiptData();
			
			PendingPurchaseInfo.AddReceiptOffer(Receipt);
			return true;
		}
	}
	
	return false;
}

#undef LOCTEXT_NAMESPACE