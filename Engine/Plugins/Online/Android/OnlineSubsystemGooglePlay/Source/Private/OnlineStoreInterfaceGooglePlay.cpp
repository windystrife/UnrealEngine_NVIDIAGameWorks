// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineStoreInterfaceGooglePlay.h"
#include "OnlinePurchaseGooglePlay.h"
#include "OnlineAsyncTaskGooglePlayQueryInAppPurchases.h"
#include "TaskGraphInterfaces.h"
#include "Misc/ConfigCacheIni.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemGooglePlay.h"
#include <jni.h>


////////////////////////////////////////////////////////////////////
/// FOnlineStoreGooglePlay implementation

FOnlineStoreGooglePlay::FOnlineStoreGooglePlay(FOnlineSubsystemGooglePlay* InSubsystem)
	: Subsystem( InSubsystem )
	, CurrentQueryTask( nullptr )
{
	UE_LOG(LogOnline, Display, TEXT( "FOnlineStoreGooglePlay::FOnlineStoreGooglePlay" ));
}

FOnlineStoreGooglePlay::~FOnlineStoreGooglePlay()
{
	UE_LOG(LogOnline, Display, TEXT( "FOnlineStoreGooglePlay::~FOnlineStoreGooglePlay" ));

	if (Subsystem)
	{
		Subsystem->ClearOnGooglePlayAvailableIAPQueryCompleteDelegate_Handle(AvailableIAPQueryDelegateHandle);
		Subsystem->ClearOnGooglePlayProcessPurchaseCompleteDelegate_Handle(ProcessPurchaseResultDelegateHandle);
		Subsystem->ClearOnGooglePlayRestorePurchasesCompleteDelegate_Handle(RestorePurchasesCompleteDelegateHandle);
	}
}

void FOnlineStoreGooglePlay::Init()
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineStoreGooglePlay::Init"));
	FOnGooglePlayAvailableIAPQueryCompleteDelegate IAPQueryDelegate = FOnGooglePlayAvailableIAPQueryCompleteDelegate::CreateThreadSafeSP(this, &FOnlineStoreGooglePlay::OnGooglePlayAvailableIAPQueryComplete);
	AvailableIAPQueryDelegateHandle = Subsystem->AddOnGooglePlayAvailableIAPQueryCompleteDelegate_Handle(IAPQueryDelegate);

	FOnGooglePlayProcessPurchaseCompleteDelegate PurchaseCompleteDelegate = FOnGooglePlayProcessPurchaseCompleteDelegate::CreateThreadSafeSP(this, &FOnlineStoreGooglePlay::OnProcessPurchaseResult);
	ProcessPurchaseResultDelegateHandle = Subsystem->AddOnGooglePlayProcessPurchaseCompleteDelegate_Handle(PurchaseCompleteDelegate);

	FOnGooglePlayRestorePurchasesCompleteDelegate RestorePurchasesCompleteDelegate = FOnGooglePlayRestorePurchasesCompleteDelegate::CreateThreadSafeSP(this, &FOnlineStoreGooglePlay::OnRestorePurchasesComplete);
	RestorePurchasesCompleteDelegateHandle = Subsystem->AddOnGooglePlayRestorePurchasesCompleteDelegate_Handle(RestorePurchasesCompleteDelegate);

	FString GooglePlayLicenseKey;
	if (!GConfig->GetString(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("GooglePlayLicenseKey"), GooglePlayLicenseKey, GEngineIni) || GooglePlayLicenseKey.IsEmpty())
	{
		UE_LOG_ONLINE(Warning, TEXT("Missing GooglePlayLicenseKey key in /Script/AndroidRuntimeSettings.AndroidRuntimeSettings of DefaultEngine.ini"));
	}

	extern void AndroidThunkCpp_Iap_SetupIapService(const FString&);
	AndroidThunkCpp_Iap_SetupIapService(GooglePlayLicenseKey);
}

bool FOnlineStoreGooglePlay::IsAllowedToMakePurchases()
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineStoreGooglePlay::IsAllowedToMakePurchases"));

	extern bool AndroidThunkCpp_Iap_IsAllowedToMakePurchases();
	return AndroidThunkCpp_Iap_IsAllowedToMakePurchases();
}


bool FOnlineStoreGooglePlay::QueryForAvailablePurchases(const TArray<FString>& ProductIds, FOnlineProductInformationReadRef& InReadObject)
{
	UE_LOG(LogOnline, Display, TEXT( "FOnlineStoreGooglePlay::QueryForAvailablePurchases" ));
	
	ReadObject = InReadObject;
	ReadObject->ReadState = EOnlineAsyncTaskState::InProgress;

	CurrentQueryTask = new FOnlineAsyncTaskGooglePlayQueryInAppPurchases(
		Subsystem,
		ProductIds);
	Subsystem->QueueAsyncTask(CurrentQueryTask);

	return true;
}


JNI_METHOD void Java_com_epicgames_ue4_GooglePlayStoreHelper_nativeQueryComplete(JNIEnv* jenv, jobject thiz, jsize responseCode, jobjectArray productIDs, jobjectArray titles, jobjectArray descriptions, jobjectArray prices, jfloatArray pricesRaw, jobjectArray currencyCodes)
{
	TArray<FInAppPurchaseProductInfo> ProvidedProductInformation;
	EGooglePlayBillingResponseCode EGPResponse = (EGooglePlayBillingResponseCode)responseCode;
	bool bWasSuccessful = (EGPResponse == EGooglePlayBillingResponseCode::Ok);

	if (jenv && bWasSuccessful)
	{
		jsize NumProducts = jenv->GetArrayLength(productIDs);
		jsize NumTitles = jenv->GetArrayLength(titles);
		jsize NumDescriptions = jenv->GetArrayLength(descriptions);
		jsize NumPrices = jenv->GetArrayLength(prices);
		jsize NumPricesRaw = jenv->GetArrayLength(pricesRaw);
		jsize NumCurrencyCodes = jenv->GetArrayLength(currencyCodes);

		ensure((NumProducts == NumTitles) && (NumProducts == NumDescriptions) && (NumProducts == NumPrices) && (NumProducts == NumPricesRaw) && (NumProducts == NumCurrencyCodes));

		jfloat *PricesRaw = jenv->GetFloatArrayElements(pricesRaw, 0);

		for (jsize Idx = 0; Idx < NumProducts; Idx++)
		{
			// Build the product information strings.

			FInAppPurchaseProductInfo NewProductInfo;

			jstring NextId = (jstring)jenv->GetObjectArrayElement(productIDs, Idx);
			const char* charsId = jenv->GetStringUTFChars(NextId, 0);
			NewProductInfo.Identifier = FString(UTF8_TO_TCHAR(charsId));
			jenv->ReleaseStringUTFChars(NextId, charsId);
			jenv->DeleteLocalRef(NextId);

			jstring NextTitle = (jstring)jenv->GetObjectArrayElement(titles, Idx);
			const char* charsTitle = jenv->GetStringUTFChars(NextTitle, 0);
			NewProductInfo.DisplayName = FString(UTF8_TO_TCHAR(charsTitle));
			jenv->ReleaseStringUTFChars(NextTitle, charsTitle);
			jenv->DeleteLocalRef(NextTitle);

			jstring NextDesc = (jstring)jenv->GetObjectArrayElement(descriptions, Idx);
			const char* charsDesc = jenv->GetStringUTFChars(NextDesc, 0);
			NewProductInfo.DisplayDescription = FString(UTF8_TO_TCHAR(charsDesc));
			jenv->ReleaseStringUTFChars(NextDesc, charsDesc);
			jenv->DeleteLocalRef(NextDesc);

			jstring NextPrice = (jstring)jenv->GetObjectArrayElement(prices, Idx);
			const char* charsPrice = jenv->GetStringUTFChars(NextPrice, 0);
			NewProductInfo.DisplayPrice = FString(UTF8_TO_TCHAR(charsPrice));
			jenv->ReleaseStringUTFChars(NextPrice, charsPrice);
			jenv->DeleteLocalRef(NextPrice);

			NewProductInfo.RawPrice = PricesRaw[Idx];

			jstring NextCurrencyCode = (jstring)jenv->GetObjectArrayElement(currencyCodes, Idx);
			const char* charsCurrencyCodes = jenv->GetStringUTFChars(NextCurrencyCode, 0);
			NewProductInfo.CurrencyCode = FString(UTF8_TO_TCHAR(charsCurrencyCodes));
			jenv->ReleaseStringUTFChars(NextCurrencyCode, charsCurrencyCodes);
			jenv->DeleteLocalRef(NextCurrencyCode);

			ProvidedProductInformation.Add(NewProductInfo);

			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\nProduct Identifier: %s, Name: %s, Description: %s, Price: %s, Price Raw: %f, Currency Code: %s\n"),
				*NewProductInfo.Identifier,
				*NewProductInfo.DisplayName,
				*NewProductInfo.DisplayDescription,
				*NewProductInfo.DisplayPrice,
				NewProductInfo.RawPrice,
				*NewProductInfo.CurrencyCode);
		}
	}

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessQueryIapResult"), STAT_FSimpleDelegateGraphTask_ProcessQueryIapResult, STATGROUP_TaskGraphTasks);

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Adding task Success: %d Response: %s"), bWasSuccessful, ToString(EGPResponse));

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
	{
		if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get(GOOGLEPLAY_SUBSYSTEM))
		{
			FOnlineSubsystemGooglePlay* const OnlineSubGP = static_cast<FOnlineSubsystemGooglePlay* const>(OnlineSub);
			if (OnlineSubGP)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("TriggerOnGooglePlayAvailableIAPQueryCompleteDelegates %s Size: %d"), ToString(EGPResponse), ProvidedProductInformation.Num());
				OnlineSubGP->TriggerOnGooglePlayAvailableIAPQueryCompleteDelegates(EGPResponse, ProvidedProductInformation);
			}
		}
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("In-App Purchase query was completed  %s\n"), bWasSuccessful ? TEXT("successfully") : TEXT("unsuccessfully"));
	}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessQueryIapResult),
		nullptr,
		ENamedThreads::GameThread
		);
}


void FOnlineStoreGooglePlay::OnGooglePlayAvailableIAPQueryComplete(EGooglePlayBillingResponseCode InResponseCode, const TArray<FInAppPurchaseProductInfo>& AvailablePurchases)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineStoreGooglePlay::OnGooglePlayAvailableIAPQueryComplete"));

	if (ReadObject.IsValid())
	{
		ReadObject->ReadState = (InResponseCode == EGooglePlayBillingResponseCode::Ok) ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;
		ReadObject->ProvidedProductInformation.Insert(AvailablePurchases, 0);
	}

	CurrentQueryTask->ProcessQueryAvailablePurchasesResults(InResponseCode == EGooglePlayBillingResponseCode::Ok);

	// clear the pointer, it will be destroyed by the async task manager
	CurrentQueryTask = nullptr;
}

bool FOnlineStoreGooglePlay::BeginPurchase(const FInAppPurchaseProductRequest& ProductRequest, FOnlineInAppPurchaseTransactionRef& InPurchaseStateObject)
{
	UE_LOG(LogOnline, Display, TEXT( "FOnlineStoreGooglePlay::BeginPurchase" ));
	
	bool bCreatedNewTransaction = false;
	
	if (IsAllowedToMakePurchases())
	{
		CachedPurchaseStateObject = InPurchaseStateObject;
		CachedPurchaseStateObject->bIsConsumable = ProductRequest.bIsConsumable;

		extern bool AndroidThunkCpp_Iap_BeginPurchase(const FString&);
		bCreatedNewTransaction = AndroidThunkCpp_Iap_BeginPurchase(ProductRequest.ProductIdentifier);
		UE_LOG(LogOnline, Display, TEXT("Created Transaction? - %s"), 
			bCreatedNewTransaction ? TEXT("Created a transaction.") : TEXT("Failed to create a transaction."));

		if (!bCreatedNewTransaction)
		{
			UE_LOG(LogOnline, Display, TEXT("FOnlineStoreGooglePlay::BeginPurchase - Could not create a new transaction."));
			CachedPurchaseStateObject->ReadState = EOnlineAsyncTaskState::Failed;
			TriggerOnInAppPurchaseCompleteDelegates(EInAppPurchaseState::Invalid);
		}
		else
		{
			CachedPurchaseStateObject->ReadState = EOnlineAsyncTaskState::InProgress;
		}
	}
	else
	{
		UE_LOG(LogOnline, Display, TEXT("This device is not able to make purchases."));

		InPurchaseStateObject->ReadState = EOnlineAsyncTaskState::Failed;
		TriggerOnInAppPurchaseCompleteDelegates(EInAppPurchaseState::NotAllowed);
	}

	return bCreatedNewTransaction;
}


JNI_METHOD void Java_com_epicgames_ue4_GooglePlayStoreHelper_nativePurchaseComplete(JNIEnv* jenv, jobject thiz, jsize responseCode, jstring productId, jstring productToken, jstring receiptData, jstring signature)
{
	FString ProductId, ProductToken, ReceiptData, Signature;
	EGooglePlayBillingResponseCode EGPResponse = (EGooglePlayBillingResponseCode)responseCode;

	bool bWasSuccessful = (EGPResponse == EGooglePlayBillingResponseCode::Ok);
	if (bWasSuccessful)
	{
		const char* charsId = jenv->GetStringUTFChars(productId, 0);
		ProductId = FString(UTF8_TO_TCHAR(charsId));
		jenv->ReleaseStringUTFChars(productId, charsId);

		const char* charsToken = jenv->GetStringUTFChars(productToken, 0);
		ProductToken = FString(UTF8_TO_TCHAR(charsToken));
		jenv->ReleaseStringUTFChars(productToken, charsToken);

		const char* charsReceipt = jenv->GetStringUTFChars(receiptData, 0);
		ReceiptData = FString(UTF8_TO_TCHAR(charsReceipt));
		jenv->ReleaseStringUTFChars(receiptData, charsReceipt);

		const char* charsSignature = jenv->GetStringUTFChars(signature, 0);
		Signature = FString(UTF8_TO_TCHAR(charsSignature));
		jenv->ReleaseStringUTFChars(signature, charsSignature);
	}

	FGoogleTransactionData TransactionData(ProductId, ProductToken, ReceiptData, Signature);

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("1... Response: %s, Transaction %s"), ToString(EGPResponse), *TransactionData.ToDebugString());

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessIapResult"), STAT_FSimpleDelegateGraphTask_ProcessIapResult, STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("In-App Purchase was completed  %s\n"), bWasSuccessful ? TEXT("successfully") : TEXT("unsuccessfully"));
		if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get(GOOGLEPLAY_SUBSYSTEM))
		{
			FOnlineSubsystemGooglePlay* const OnlineSubGP = static_cast<FOnlineSubsystemGooglePlay* const>(OnlineSub);
			if (OnlineSubGP)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("2... Response %s Transaction %s"), ToString(EGPResponse), *TransactionData.ToDebugString());
				OnlineSubGP->TriggerOnGooglePlayProcessPurchaseCompleteDelegates(EGPResponse, TransactionData);
			}
		}
	}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessIapResult),
		nullptr,
		ENamedThreads::GameThread
		);
}


void FOnlineStoreGooglePlay::OnProcessPurchaseResult(EGooglePlayBillingResponseCode InResponseCode, const FGoogleTransactionData& InTransactionData)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineStoreGooglePlay::OnProcessPurchaseResult"));
	UE_LOG(LogOnline, Display, TEXT("3... Response: %s Transaction: %s"), ToString(InResponseCode), *InTransactionData.ToDebugString());

	bool bWasSuccessful = (InResponseCode == EGooglePlayBillingResponseCode::Ok);
	if (CachedPurchaseStateObject.IsValid())
	{
		if (CachedPurchaseStateObject->bIsConsumable && InTransactionData.GetErrorStr().IsEmpty())
		{
			// Consume right away to maintain behavior of legacy code (GooglePlayStoreHelper.java)
			// Technically wrong/dangerous because the game may not grant entitlements
			// on a crash or other interruption 
			extern void AndroidThunkCpp_Iap_ConsumePurchase(const FString&);
			AndroidThunkCpp_Iap_ConsumePurchase(InTransactionData.GetTransactionIdentifier());
		}

		FInAppPurchaseProductInfo& ProductInfo = CachedPurchaseStateObject->ProvidedProductInformation;
		ProductInfo.Identifier = InTransactionData.GetOfferId();
		ProductInfo.DisplayName = TEXT("n/a");
		ProductInfo.DisplayDescription = TEXT("n/a");
		ProductInfo.DisplayPrice = TEXT("n/a");
		ProductInfo.ReceiptData = InTransactionData.GetCombinedReceiptData();
		ProductInfo.TransactionIdentifier = InTransactionData.GetTransactionIdentifier();

		CachedPurchaseStateObject->ReadState = bWasSuccessful ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;
	}

	TriggerOnInAppPurchaseCompleteDelegates(ConvertGPResponseCodeToIAPState(InResponseCode));
}


bool FOnlineStoreGooglePlay::RestorePurchases(const TArray<FInAppPurchaseProductRequest>& ConsumableProductFlags, FOnlineInAppPurchaseRestoreReadRef& InReadObject)
{
	bool bSentAQueryRequest = false;
	CachedPurchaseRestoreObject = InReadObject;

	if (IsAllowedToMakePurchases())
	{
		TArray<FString> ProductIds;
		TArray<bool> IsConsumableFlags;

		for (int i = 0; i < ConsumableProductFlags.Num(); i++)
		{
			ProductIds.Add(ConsumableProductFlags[i].ProductIdentifier);
			IsConsumableFlags.Add(ConsumableProductFlags[i].bIsConsumable);
		}

		// Send JNI request
		extern bool AndroidThunkCpp_Iap_RestorePurchases(const TArray<FString>&, const TArray<bool>&);
		bSentAQueryRequest = AndroidThunkCpp_Iap_RestorePurchases(ProductIds, IsConsumableFlags);
	}
	else
	{
		UE_LOG(LogOnline, Display, TEXT("This device is not able to make purchases."));
		TriggerOnInAppPurchaseRestoreCompleteDelegates(EInAppPurchaseState::NotAllowed);
	}

	return bSentAQueryRequest;
}

JNI_METHOD void Java_com_epicgames_ue4_GooglePlayStoreHelper_nativeRestorePurchasesComplete(JNIEnv* jenv, jobject thiz, jsize responseCode, jobjectArray ProductIDs, jobjectArray ProductTokens, jobjectArray ReceiptsData, jobjectArray Signatures)
{
	TArray<FGoogleTransactionData> RestoredPurchaseInfo;

	EGooglePlayBillingResponseCode EGPResponse = (EGooglePlayBillingResponseCode)responseCode;
	bool bWasSuccessful = (EGPResponse == EGooglePlayBillingResponseCode::Ok);

	if (jenv && bWasSuccessful)
	{
		jsize NumProducts = jenv->GetArrayLength(ProductIDs);
		jsize NumProductTokens = jenv->GetArrayLength(ProductTokens);
		jsize NumReceipts = jenv->GetArrayLength(ReceiptsData);
		jsize NumSignatures = jenv->GetArrayLength(Signatures);

		ensure((NumProducts == NumProductTokens) && (NumProducts == NumReceipts) && (NumProducts == NumSignatures));

		for (jsize Idx = 0; Idx < NumProducts; Idx++)
		{
			// Build the restore product information strings.
			FInAppPurchaseRestoreInfo RestoreInfo;

			jstring NextId = (jstring)jenv->GetObjectArrayElement(ProductIDs, Idx);
			const char* charsId = jenv->GetStringUTFChars(NextId, 0);
			const FString OfferId = FString(UTF8_TO_TCHAR(charsId));
			jenv->ReleaseStringUTFChars(NextId, charsId);
			jenv->DeleteLocalRef(NextId);

			jstring NextToken = (jstring)jenv->GetObjectArrayElement(ProductTokens, Idx);
			const char* charsToken = jenv->GetStringUTFChars(NextToken, 0);
			const FString ProductToken = FString(UTF8_TO_TCHAR(charsToken));
			jenv->ReleaseStringUTFChars(NextToken, charsToken);
			jenv->DeleteLocalRef(NextToken);

			jstring NextReceipt = (jstring)jenv->GetObjectArrayElement(ReceiptsData, Idx);
			const char* charsReceipt = jenv->GetStringUTFChars(NextReceipt, 0);
			const FString ReceiptData = FString(UTF8_TO_TCHAR(charsReceipt));
			jenv->ReleaseStringUTFChars(NextReceipt, charsReceipt);
			jenv->DeleteLocalRef(NextReceipt);

			jstring NextSignature = (jstring)jenv->GetObjectArrayElement(Signatures, Idx);
			const char* charsSignature = jenv->GetStringUTFChars(NextSignature, 0);
			const FString SignatureData = FString(UTF8_TO_TCHAR(charsSignature));
			jenv->ReleaseStringUTFChars(NextSignature, charsSignature);
			jenv->DeleteLocalRef(NextSignature);

			FGoogleTransactionData RestoredPurchase(OfferId, ProductToken, ReceiptData, SignatureData);
			RestoredPurchaseInfo.Add(RestoredPurchase);

			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Restored Transaction: %s"), *RestoredPurchase.ToDebugString());
		}
	}

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.RestorePurchases"), STAT_FSimpleDelegateGraphTask_RestorePurchases, STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Restoring In-App Purchases was completed  %s\n"), bWasSuccessful ? TEXT("successfully") : TEXT("unsuccessfully"));

		if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get(GOOGLEPLAY_SUBSYSTEM))
		{
			FOnlineSubsystemGooglePlay* const OnlineSubGP = static_cast<FOnlineSubsystemGooglePlay* const>(OnlineSub);
			if (OnlineSubGP)
			{
				OnlineSubGP->TriggerOnGooglePlayRestorePurchasesCompleteDelegates(EGPResponse, RestoredPurchaseInfo);
			}
		}
	}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_RestorePurchases),
		nullptr,
		ENamedThreads::GameThread
		);
}

void FOnlineStoreGooglePlay::OnRestorePurchasesComplete(EGooglePlayBillingResponseCode InResponseCode, const TArray<FGoogleTransactionData>& InRestoredPurchases)
{
	UE_LOG(LogOnline, Verbose, TEXT("FOnlineStoreGooglePlay::OnRestorePurchasesComplete Response: %s Num: %d"), ToString(InResponseCode), InRestoredPurchases.Num());

	bool bWasSuccessful = (InResponseCode == EGooglePlayBillingResponseCode::Ok);
	if (CachedPurchaseRestoreObject.IsValid())
	{
		TArray<FInAppPurchaseRestoreInfo> RestoredPurchaseInfo;
		RestoredPurchaseInfo.Reserve(InRestoredPurchases.Num());
		for (const FGoogleTransactionData& RestoredPurchase : InRestoredPurchases)
		{
			FInAppPurchaseRestoreInfo RestoreInfo;
			RestoreInfo.Identifier = RestoredPurchase.GetOfferId();
			RestoreInfo.ReceiptData = RestoredPurchase.GetCombinedReceiptData();
			RestoreInfo.TransactionIdentifier = RestoredPurchase.GetTransactionIdentifier();
			RestoredPurchaseInfo.Add(RestoreInfo);
		}

		CachedPurchaseRestoreObject->ProvidedRestoreInformation = RestoredPurchaseInfo;
		CachedPurchaseRestoreObject->ReadState = bWasSuccessful ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;
	}

	EInAppPurchaseState::Type IAPState = bWasSuccessful ? EInAppPurchaseState::Restored : ConvertGPResponseCodeToIAPState(InResponseCode);

	TriggerOnInAppPurchaseRestoreCompleteDelegates(IAPState);
}

JNI_METHOD void Java_com_epicgames_ue4_GooglePlayStoreHelper_nativeQueryExistingPurchasesComplete(JNIEnv* jenv, jobject thiz, jsize responseCode, jobjectArray ProductIDs, jobjectArray ProductTokens, jobjectArray ReceiptsData, jobjectArray Signatures)
{
	TArray<FGoogleTransactionData> ExistingPurchaseInfo;

	EGooglePlayBillingResponseCode EGPResponse = (EGooglePlayBillingResponseCode)responseCode;

	bool bWasSuccessful = (EGPResponse == EGooglePlayBillingResponseCode::Ok);
	if (jenv && bWasSuccessful)
	{
		jsize NumProducts = jenv->GetArrayLength(ProductIDs);
		jsize NumProductTokens = jenv->GetArrayLength(ProductTokens);
		jsize NumReceipts = jenv->GetArrayLength(ReceiptsData);
		jsize NumSignatures = jenv->GetArrayLength(Signatures);

		ensure((NumProducts == NumProductTokens) && (NumProducts == NumReceipts) && (NumProducts == NumSignatures));

		for (jsize Idx = 0; Idx < NumProducts; Idx++)
		{
			// Build the product information strings.
			jstring NextId = (jstring)jenv->GetObjectArrayElement(ProductIDs, Idx);
			const char* charsId = jenv->GetStringUTFChars(NextId, 0);
			const FString OfferId = FString(UTF8_TO_TCHAR(charsId));
			jenv->ReleaseStringUTFChars(NextId, charsId);
			jenv->DeleteLocalRef(NextId);

			jstring NextToken = (jstring)jenv->GetObjectArrayElement(ProductTokens, Idx);
			const char* charsToken = jenv->GetStringUTFChars(NextToken, 0);
			const FString ProductToken = FString(UTF8_TO_TCHAR(charsToken));
			jenv->ReleaseStringUTFChars(NextToken, charsToken);
			jenv->DeleteLocalRef(NextToken);

			jstring NextReceipt = (jstring)jenv->GetObjectArrayElement(ReceiptsData, Idx);
			const char* charsReceipt = jenv->GetStringUTFChars(NextReceipt, 0);
			const FString ReceiptData = FString(UTF8_TO_TCHAR(charsReceipt));
			jenv->ReleaseStringUTFChars(NextReceipt, charsReceipt);
			jenv->DeleteLocalRef(NextReceipt);

			jstring NextSignature = (jstring)jenv->GetObjectArrayElement(Signatures, Idx);
			const char* charsSignature = jenv->GetStringUTFChars(NextSignature, 0);
			const FString SignatureData = FString(UTF8_TO_TCHAR(charsSignature));
			jenv->ReleaseStringUTFChars(NextSignature, charsSignature);
			jenv->DeleteLocalRef(NextSignature);

			FGoogleTransactionData ExistingPurchase(OfferId, ProductToken, ReceiptData, SignatureData);
			ExistingPurchaseInfo.Add(ExistingPurchase);

			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\Existing Product Identifier: %s"), *ExistingPurchase.ToDebugString());
		}
	}

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.QueryExistingPurchases"), STAT_FSimpleDelegateGraphTask_QueryExistingPurchases, STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Query existing purchases was completed %s\n"), bWasSuccessful ? TEXT("successfully") : TEXT("unsuccessfully"));
		if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get(GOOGLEPLAY_SUBSYSTEM))
		{
			FOnlineSubsystemGooglePlay* const OnlineSubGP = static_cast<FOnlineSubsystemGooglePlay* const>(OnlineSub);
			if (OnlineSubGP)
			{
				OnlineSubGP->TriggerOnGooglePlayQueryExistingPurchasesCompleteDelegates(EGPResponse, ExistingPurchaseInfo);
			}
		}
	}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_QueryExistingPurchases),
		nullptr,
		ENamedThreads::GameThread
		);
}
