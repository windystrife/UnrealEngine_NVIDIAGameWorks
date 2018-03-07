// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemIOSPrivatePCH.h"
#import "OnlineStoreKitHelper.h"

/**
 * Convert an Apple SKPaymentTransaction receipt into a string
 * 
 * @param transaction valid transaction to convert to hex encoded string
 *
 * @return hex encoded string with opaque data representing a completed transaction
 */
FString convertReceiptToString(const SKPaymentTransaction* transaction)
{
	FString ReceiptData;
	
#ifdef __IPHONE_7_0
	if ([IOSAppDelegate GetDelegate].OSVersion >= 7.0)
	{
		NSURL* nsReceiptUrl = [[NSBundle mainBundle] appStoreReceiptURL];
		NSData* nsReceiptData = [NSData dataWithContentsOfURL : nsReceiptUrl];
		if (nsReceiptData)
		{
			NSString* nsEncodedReceiptData = [nsReceiptData base64EncodedStringWithOptions : NSDataBase64EncodingEndLineWithLineFeed];
			ReceiptData = nsEncodedReceiptData;
		}
		else
		{
			UE_LOG(LogOnline, Log, TEXT("No receipt data found for transaction"));
		}
	}
	else
#endif
	{
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_7_0
		// If earlier than IOS 7, we will need to use the transactionReceipt
		NSString* nsEncodedReceiptData = [transaction.transactionReceipt base64EncodedStringWithOptions : NSDataBase64EncodingEndLineWithLineFeed];
		ReceiptData = nsEncodedReceiptData;
#endif
	}
	
	UE_LOG(LogOnline, VeryVerbose, TEXT("FStoreKitHelper::convertReceiptToString %s"), *ReceiptData);
	return ReceiptData;
}

/**
 * Retrieve the original transaction id from an Apple transaction object
 * Ignores code comment in SKPaymentTransaction.h that it can only be found in "restored" state
 * Successful attempts to repurchase already owned items (NOT restore purchase), will end in "purchased" state with an original transaction id
 *
 * @param Transaction the transaction to retrieve an original transaction id
 *
 * @return original transaction id for the transaction
 */
FString GetOriginalTransactionId(const SKPaymentTransaction* Transaction)
{
	SKPaymentTransaction* OriginalTransaction = nullptr;
	if (Transaction.originalTransaction)
	{
		if (Transaction.transactionState != SKPaymentTransactionStateRestored)
		{
			UE_LOG(LogOnline, Log, TEXT("Original transaction id in state %d"), static_cast<int32>(Transaction.transactionState));
		}
		
		int32 RecurseCount = 0;
		
		if (Transaction != Transaction.originalTransaction)
		{
			OriginalTransaction = Transaction.originalTransaction;
			while (OriginalTransaction.originalTransaction && (RecurseCount < 100))
			{
				++RecurseCount;
				OriginalTransaction = OriginalTransaction.originalTransaction;
			}
		}
		
		if (RecurseCount > 0)
		{
			UE_LOG(LogOnline, Log, TEXT("Original transaction id recurse count %d"), RecurseCount);
		}
	}
	
	return OriginalTransaction ? OriginalTransaction.transactionIdentifier : Transaction.transactionIdentifier;
}

////////////////////////////////////////////////////////////////////
/// FSKProductsRequestHelper implementation
/// Add a delegate to a product information request 

@implementation FSKProductsRequestHelper
@end

////////////////////////////////////////////////////////////////////
/// FStoreKitTransactionData implementation
FStoreKitTransactionData::FStoreKitTransactionData(const SKPaymentTransaction* Transaction)
	: ReceiptData(convertReceiptToString(Transaction))
	, ErrorStr([Transaction.error localizedDescription])
	, TransactionIdentifier(Transaction.transactionIdentifier)
{
	SKPayment* Payment = Transaction.payment;
	if (Payment)
	{
		OfferId = Payment.productIdentifier;
	}
	
	OriginalTransactionIdentifier = GetOriginalTransactionId(Transaction);
}

////////////////////////////////////////////////////////////////////
/// FStoreKitHelper implementation

@implementation FStoreKitHelper
@synthesize Request;
@synthesize AvailableProducts;

- (id)init
{
	self = [super init];
	return self;
}

-(void)dealloc
{
	[Request release];
	[AvailableProducts release];
	[super dealloc];
}

-(void)paymentQueue:(SKPaymentQueue *)queue updatedTransactions : (NSArray *)transactions
{
	// Parse the generic transaction update into appropriate execution paths
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelper::updatedTransactions"));
	for (SKPaymentTransaction *transaction in transactions)
	{
		switch ([transaction transactionState])
		{
			case SKPaymentTransactionStatePurchased:
				UE_LOG(LogOnline, Verbose, TEXT("FStoreKitHelper::completeTransaction"));
				[self completeTransaction : transaction];
				break;
			case SKPaymentTransactionStateFailed:
				UE_LOG(LogOnline, Verbose, TEXT("FStoreKitHelper::failedTransaction"));
				[self failedTransaction : transaction];
				break;
			case SKPaymentTransactionStateRestored:
				UE_LOG(LogOnline, Verbose, TEXT("FStoreKitHelper::restoreTransaction"));
				[self restoreTransaction : transaction];
				break;
			case SKPaymentTransactionStatePurchasing:
				UE_LOG(LogOnline, Verbose, TEXT("FStoreKitHelper::purchasingInProgress"));
				[self purchaseInProgress : transaction];
				continue;
			case SKPaymentTransactionStateDeferred:
				UE_LOG(LogOnline, Verbose, TEXT("FStoreKitHelper::purchaseDeferred"));
				[self purchaseDeferred : transaction];
				continue;
			default:
				UE_LOG(LogOnline, Verbose, TEXT("FStoreKitHelper::other: %d"), [transaction transactionState]);
				break;
		}
	}
}

-(void)paymentQueue:(SKPaymentQueue *)queue removedTransactions : (NSArray *)transactions
{
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelper::removedTransactions"));
}

-(void)paymentQueueRestoreCompletedTransactionsFinished:(SKPaymentQueue *)queue
{
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelper::paymentQueueRestoreCompletedTransactionsFinished"));

	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(IOS_SUBSYSTEM);

		FOnlineStoreInterfaceIOS* StoreInterface = (FOnlineStoreInterfaceIOS*)OnlineSub->GetStoreInterface().Get();
		if (StoreInterface->CachedPurchaseRestoreObject.IsValid())
		{
			StoreInterface->CachedPurchaseRestoreObject->ReadState = EOnlineAsyncTaskState::Done;
		}
		StoreInterface->ProcessRestorePurchases(EInAppPurchaseState::Restored);
		
		return true;
	}];
}

-(void)paymentQueue: (SKPaymentQueue *)queue restoreCompletedTransactionsFailedWithError : (NSError *)error
{
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelper::failedRestore - %s"), *FString([error localizedDescription]));
	
	EInAppPurchaseState::Type CompletionState = EInAppPurchaseState::Unknown;
	switch (error.code)
	{
		case SKErrorPaymentCancelled:
			CompletionState = EInAppPurchaseState::Cancelled;
			break;
		case SKErrorClientInvalid:
		case SKErrorStoreProductNotAvailable:
		case SKErrorPaymentInvalid:
			CompletionState = EInAppPurchaseState::Invalid;
			break;
		case SKErrorPaymentNotAllowed:
			CompletionState = EInAppPurchaseState::NotAllowed;
			break;
	}
	
	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(IOS_SUBSYSTEM);
		FOnlineStoreInterfaceIOS* StoreInterface = (FOnlineStoreInterfaceIOS*)OnlineSub->GetStoreInterface().Get();
		if (StoreInterface->CachedPurchaseRestoreObject.IsValid())
		{
			StoreInterface->CachedPurchaseRestoreObject->ReadState = EOnlineAsyncTaskState::Done;
		}
 
		StoreInterface->ProcessRestorePurchases(CompletionState);
	 
		return true;
	 }];
}

-(void)completeTransaction: (SKPaymentTransaction *)transaction
{
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelper::completeTransaction"));

	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(IOS_SUBSYSTEM);
		FOnlineStoreInterfaceIOS* StoreInterface = (FOnlineStoreInterfaceIOS*)OnlineSub->GetStoreInterface().Get();

		if (StoreInterface->CachedPurchaseStateObject.IsValid())
		{
			 const FString ReceiptData = convertReceiptToString(transaction);
			 
			 StoreInterface->CachedPurchaseStateObject->ProvidedProductInformation.ReceiptData = ReceiptData;
			 StoreInterface->CachedPurchaseStateObject->ProvidedProductInformation.TransactionIdentifier = transaction.transactionIdentifier;
			 StoreInterface->CachedPurchaseStateObject->ReadState = EOnlineAsyncTaskState::Done;
		}

		StoreInterface->TriggerOnInAppPurchaseCompleteDelegates(EInAppPurchaseState::Success);

		return true;
	}];

	// Remove the transaction from the payment queue.
	[[SKPaymentQueue defaultQueue] finishTransaction:transaction];
}

-(void)restoreTransaction: (SKPaymentTransaction *)transaction
{
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelper::restoreTransaction"));

	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(IOS_SUBSYSTEM);
		FOnlineStoreInterfaceIOS* StoreInterface = (FOnlineStoreInterfaceIOS*)OnlineSub->GetStoreInterface().Get();

		if (StoreInterface->CachedPurchaseRestoreObject.IsValid())
		{
			const FString ReceiptData = convertReceiptToString(transaction);
	 
			FInAppPurchaseRestoreInfo RestoreInfo;
			RestoreInfo.Identifier = FString(transaction.originalTransaction.payment.productIdentifier);
			RestoreInfo.ReceiptData = ReceiptData;
			StoreInterface->CachedPurchaseRestoreObject->ProvidedRestoreInformation.Add(RestoreInfo);
		}
		
		return true;
	}];

	[[SKPaymentQueue defaultQueue] finishTransaction:transaction];
}

-(void)failedTransaction: (SKPaymentTransaction *)transaction
{
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelper::failedTransaction - %s"), *FString([transaction.error localizedDescription]));

	EInAppPurchaseState::Type CompletionState = EInAppPurchaseState::Unknown;
	switch (transaction.error.code)
	{
		case SKErrorPaymentCancelled:
			CompletionState = EInAppPurchaseState::Cancelled;
			break;
		case SKErrorClientInvalid:
		case SKErrorStoreProductNotAvailable:
		case SKErrorPaymentInvalid:
			CompletionState = EInAppPurchaseState::Invalid;
			break;
		case SKErrorPaymentNotAllowed:
			CompletionState = EInAppPurchaseState::NotAllowed;
			break;
	}

	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(IOS_SUBSYSTEM);
		FOnlineStoreInterfaceIOS* StoreInterface = (FOnlineStoreInterfaceIOS*)OnlineSub->GetStoreInterface().Get();
		if (StoreInterface->CachedPurchaseStateObject.IsValid())
		{
			StoreInterface->CachedPurchaseStateObject->ReadState = EOnlineAsyncTaskState::Done;
		}

		StoreInterface->TriggerOnInAppPurchaseCompleteDelegates(CompletionState);
		return true;
	}];

	[[SKPaymentQueue defaultQueue] finishTransaction:transaction];
}

-(void)purchaseInProgress: (SKPaymentTransaction *)transaction
{
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelper::purchaseInProgress"));
}

-(void)purchaseDeferred: (SKPaymentTransaction *)transaction
{
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelper::purchaseDeferred"));
}

-(void)requestProductData: (NSMutableSet*)productIDs
{
	UE_LOG(LogOnline, Verbose, TEXT("FStoreKitHelper::requestProductData"));

	Request = [[SKProductsRequest alloc] initWithProductIdentifiers:productIDs];
	Request.delegate = self;

	[Request start];
}

-(void)makePurchase: (NSMutableSet*)productIDs
{
	UE_LOG(LogOnline, Verbose, TEXT("FStoreKitHelper::makePurchase"));

	Request = [[SKProductsRequest alloc] initWithProductIdentifiers:productIDs];
	Request.delegate = self;
	
	[Request start];
}

-(void)productsRequest: (SKProductsRequest *)request didReceiveResponse : (SKProductsResponse *)response
{
	UE_LOG(LogOnline, Verbose, TEXT("FStoreKitHelper::didReceiveResponse"));
	// Direct the response back to the store interface
	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(IOS_SUBSYSTEM);
		FOnlineStoreInterfaceIOS* StoreInterface = (FOnlineStoreInterfaceIOS*)OnlineSub->GetStoreInterface().Get();
		StoreInterface->ProcessProductsResponse(response);
		
		return true;
	}];

	[request autorelease];
}

-(void)requestDidFinish:(SKRequest*)request
{
#ifdef __IPHONE_7_0
	if ([Request isKindOfClass : [SKReceiptRefreshRequest class]])
	{
		[[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
	}
#endif
}

-(void)request:(SKRequest*)request didFailWithError : (NSError *)error
{
#ifdef __IPHONE_7_0
	if ([Request isKindOfClass : [SKReceiptRefreshRequest class]])
	{
		[self paymentQueue : [SKPaymentQueue defaultQueue]  restoreCompletedTransactionsFailedWithError : error];
		[Request release];
	}
#endif
}

-(void)restorePurchases
{
#ifdef __IPHONE_7_0
	if ([IOSAppDelegate GetDelegate].OSVersion >= 7.0)
	{
		Request = [[SKReceiptRefreshRequest alloc] init];
		Request.delegate = self;
		[Request start];
	}
	else
#endif
	{
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_7_0
		[[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
#endif
	}
}

@end

////////////////////////////////////////////////////////////////////
/// FStoreKitHelperV2 implementation

@implementation FStoreKitHelperV2

- (id)init
{
	self = [super init];
	self.PendingTransactions = [NSMutableSet setWithCapacity:5];
	return self;
}
-(void)dealloc
{
	[_PendingTransactions release];
	[super dealloc];
}

-(void)paymentQueueRestoreCompletedTransactionsFinished:(SKPaymentQueue *)queue
{
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelperV2::paymentQueueRestoreCompletedTransactionsFinished"));
	
	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		[self OnRestoreTransactionsComplete].Broadcast(EPurchaseTransactionState::Restored);
		return true;
	}];
}

-(void)paymentQueue: (SKPaymentQueue *)queue restoreCompletedTransactionsFailedWithError : (NSError *)error
{
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelperV2::failedRestore - %s"), *FString([error localizedDescription]));
	
	EPurchaseTransactionState CompletionState = EPurchaseTransactionState::Failed;
	switch (error.code)
	{
		case SKErrorPaymentCancelled:
			CompletionState = EPurchaseTransactionState::Canceled;
			break;
		case SKErrorClientInvalid:
		case SKErrorStoreProductNotAvailable:
		case SKErrorPaymentInvalid:
			CompletionState = EPurchaseTransactionState::Invalid;
			break;
		case SKErrorPaymentNotAllowed:
			CompletionState = EPurchaseTransactionState::NotAllowed;
			break;
	}
	
	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		self.OnRestoreTransactionsComplete.Broadcast(CompletionState);
		return true;
	}];
}

-(void)makePurchase:(NSArray*)products WithUserId: (const FString&) userId SimulateAskToBuy: (bool) bAskToBuy;
{
	UE_LOG(LogOnline, Verbose, TEXT("FStoreKitHelperV2::makePurchase by SKProduct with UserId"));
	
	for (SKProduct* Product in products)
	{
		SKMutablePayment* Payment = [SKMutablePayment paymentWithProduct:Product];
		Payment.quantity = 1;
		if ([IOSAppDelegate GetDelegate].OSVersion >= 8.3)
		{
			Payment.simulatesAskToBuyInSandbox = bAskToBuy;
		}
		if (!userId.IsEmpty())
		{
			// hash of username to detect irregular activity
			Payment.applicationUsername = [NSString stringWithFString: userId];
		}
		[[SKPaymentQueue defaultQueue] addPayment:Payment];
	}
}

-(void)makePurchase:(NSArray*)products SimulateAskToBuy: (bool) bAskToBuy
{
	UE_LOG(LogOnline, Verbose, TEXT("FStoreKitHelperV2::makePurchase by SKProduct"));
	FString EmptyString;
	[self makePurchase: products WithUserId: EmptyString SimulateAskToBuy: bAskToBuy];
}

-(void)requestProductData: (NSMutableSet*)productIDs WithDelegate : (const FOnQueryOnlineStoreOffersComplete&)delegate
{
	UE_LOG(LogOnline, Verbose, TEXT("FStoreKitHelperV2::requestProductData"));
	
	FSKProductsRequestHelper* TempRequest = [[FSKProductsRequestHelper alloc] initWithProductIdentifiers:productIDs];
	TempRequest.OfferDelegate = delegate;
	TempRequest.delegate = self;
	self.Request = TempRequest;
	
	[self.Request start];
}

-(void)productsRequest: (SKProductsRequest *)request didReceiveResponse : (SKProductsResponse *)response
{
	if (request == self.Request)
	{
		UE_LOG(LogOnline, Verbose, TEXT("FStoreKitHelperV2::didReceiveResponse"));
		
		if ([request isKindOfClass : [FSKProductsRequestHelper class]])
		{
			FSKProductsRequestHelper* Helper = (FSKProductsRequestHelper*)request;

			[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
			{
				// Notify listeners of the request completion
				self.OnProductRequestResponse.Broadcast(response, Helper.OfferDelegate);
				return true;
			}];
		}
		else
		{
			UE_LOG(LogOnline, Warning, TEXT("Wrong class associated with product request"));
		}
		
		[request autorelease];
	}
}

-(void)completeTransaction: (SKPaymentTransaction *)transaction
{
	FStoreKitTransactionData TransactionData(transaction);
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelperV2::completeTransaction - %s"), *TransactionData.ToDebugString());
	
	EPurchaseTransactionState Result = EPurchaseTransactionState::Failed;
	
	SKPayment* Payment = transaction.payment;
	if (Payment)
	{
		Result = EPurchaseTransactionState::Purchased;
	}
	
	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
	    // Notify listeners of the request completion
		self.OnTransactionCompleteResponse.Broadcast(Result, TransactionData);
		return true;
	}];

	// Transaction must be finalized before removed from the queue
	[self.PendingTransactions addObject:transaction];
}

-(void)failedTransaction: (SKPaymentTransaction *)transaction
{
	FStoreKitTransactionData TransactionData(transaction);
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelperV2::failedTransaction - %s"), *TransactionData.ToDebugString());

	EPurchaseTransactionState CompletionState = EPurchaseTransactionState::Failed;
	switch (transaction.error.code)
	{
		case SKErrorPaymentCancelled:
			CompletionState = EPurchaseTransactionState::Canceled;
			break;
		case SKErrorClientInvalid:
		case SKErrorStoreProductNotAvailable:
		case SKErrorPaymentInvalid:
			CompletionState = EPurchaseTransactionState::Invalid;
			break;
		case SKErrorPaymentNotAllowed:
			CompletionState = EPurchaseTransactionState::NotAllowed;
			break;
	}
	
	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
	    // Notify listeners of the request completion
		self.OnTransactionCompleteResponse.Broadcast(CompletionState, TransactionData);
		return true;
	}];
	
	// Remove the transaction from the payment queue.
	[[SKPaymentQueue defaultQueue] finishTransaction:transaction];
}

-(void)restoreTransaction: (SKPaymentTransaction *)transaction
{
	FStoreKitTransactionData TransactionData(transaction);
    UE_LOG(LogOnline, Log, TEXT("FStoreKitHelperV2::restoreTransaction - %s"), *TransactionData.ToDebugString());

	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		self.OnTransactionRestored.Broadcast(TransactionData);
		return true;
	}];
	
	// @todo Transaction must be finalized before removed from the queue?
	//[self.PendingTransactions addObject:transaction];
	// Remove the transaction from the payment queue.
	[[SKPaymentQueue defaultQueue] finishTransaction:transaction];
}

-(void)purchaseInProgress: (SKPaymentTransaction *)transaction
{
	FStoreKitTransactionData TransactionData(transaction);
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelperV2::purchaseInProgress - %s"), *TransactionData.ToDebugString());
	
	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		// Notify listeners a purchase is in progress
		self.OnTransactionPurchaseInProgress.Broadcast(TransactionData);
		return true;
	}];
}

-(void)purchaseDeferred: (SKPaymentTransaction *)transaction
{
	FStoreKitTransactionData TransactionData(transaction);
	UE_LOG(LogOnline, Log, TEXT("FStoreKitHelperV2::purchaseDeferred - %s"), *TransactionData.ToDebugString());
	
	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
	    // Notify listeners a purchase has been deferred
		self.OnTransactionDeferred.Broadcast(TransactionData);
		return true;
	}];
}

-(void)finalizeTransaction: (const FString&) receiptId
{
	UE_LOG(LogOnline, Verbose, TEXT("FStoreKitHelperV2::finalizeTransaction - %s"), *receiptId);
	for (SKPaymentTransaction* pendingTransaction in self.PendingTransactions)
	{
		const FString transId = pendingTransaction.transactionIdentifier;
		const FString originalTransId = GetOriginalTransactionId(pendingTransaction);
		UE_LOG(LogOnline, Verbose, TEXT("FStoreKitHelperV2::checking - id: %s origId: %s"), *transId,  *originalTransId);
		
		if ((!originalTransId.IsEmpty()) && (originalTransId == receiptId))
		{
			UE_LOG(LogOnline, Log, TEXT("FStoreKitHelperV2::finalizeTransaction - %s"), *receiptId);

			[self.PendingTransactions removeObject:pendingTransaction];
			// Remove the transaction from the payment queue.
			[[SKPaymentQueue defaultQueue] finishTransaction:pendingTransaction];
			break;
		}
	}
}

-(void)dumpAppReceipt
{
#ifdef __IPHONE_7_0
	FString receiptData = convertReceiptToString(nullptr);
	UE_LOG(LogOnline, Verbose, TEXT("FStoreKitHelper::dumpAppReceipt"));
	UE_LOG(LogOnline, Verbose, TEXT("%s"), *receiptData);
#endif
}

-(FOnProductsRequestResponse&)OnProductRequestResponse
{
	return _OnProductRequestResponse;
}

-(FDelegateHandle)AddOnProductRequestResponse: (const FOnProductsRequestResponseDelegate&) Delegate
{
	_OnProductRequestResponse.Add(Delegate);
	return Delegate.GetHandle();
}

-(FOnTransactionCompleteIOS&)OnTransactionCompleteResponse
{
	return _OnTransactionCompleteResponse;
}

-(FDelegateHandle)AddOnTransactionComplete: (const FOnTransactionCompleteIOSDelegate&) Delegate
{
	_OnTransactionCompleteResponse.Add(Delegate);
	return Delegate.GetHandle();
}

-(FOnTransactionRestoredIOS&)OnTransactionRestored
{
	return _OnTransactionRestored;
}

-(FDelegateHandle)AddOnTransactionRestored: (const FOnTransactionRestoredIOSDelegate&) Delegate
{
	_OnTransactionRestored.Add(Delegate);
	return Delegate.GetHandle();
}

-(FOnRestoreTransactionsCompleteIOS&)OnRestoreTransactionsComplete
{
	return _OnRestoreTransactionsComplete;
}

-(FDelegateHandle)AddOnRestoreTransactionsComplete: (const FOnRestoreTransactionsCompleteIOSDelegate&) Delegate
{
	_OnRestoreTransactionsComplete.Add(Delegate);
	return Delegate.GetHandle();
}

-(FOnTransactionProgress&)OnTransactionPurchaseInProgress
{
	return _OnTransactionPurchaseInProgress;
}

-(FDelegateHandle)AddOnPurchaseInProgress: (const FOnTransactionProgressDelegate&) Delegate
{
	_OnTransactionPurchaseInProgress.Add(Delegate);
	return Delegate.GetHandle();
}

-(FOnTransactionProgress&)OnTransactionDeferred
{
	return _OnTransactionDeferred;
}

-(FDelegateHandle)AddOnTransactionDeferred: (const FOnTransactionProgressDelegate&) Delegate
{
	_OnTransactionDeferred.Add(Delegate);
	return Delegate.GetHandle();
}

@end

