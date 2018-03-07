// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Interfaces/OnlineStoreInterface.h"
#include "InAppPurchaseCallbackProxy.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInAppPurchaseResult, EInAppPurchaseState::Type, CompletionStatus, const FInAppPurchaseProductInfo&, InAppPurchaseInformation);

UCLASS(MinimalAPI)
class UInAppPurchaseCallbackProxy : public UObject
{
	GENERATED_UCLASS_BODY()

	// Called when there is a successful In-App Purchase transaction
	UPROPERTY(BlueprintAssignable)
	FInAppPurchaseResult OnSuccess;

	// Called when there is an unsuccessful In-App Purchase transaction
	UPROPERTY(BlueprintAssignable)
	FInAppPurchaseResult OnFailure;

	// Kicks off a transaction for the provided product identifier
	UFUNCTION(BlueprintCallable, meta = (DisplayName="Make an In-App Purchase"), Category="Online|InAppPurchase")
	static UInAppPurchaseCallbackProxy* CreateProxyObjectForInAppPurchase(class APlayerController* PlayerController, const FInAppPurchaseProductRequest& ProductRequest);

public:

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	//~ End UObject Interface

private:

	/** Called by the InAppPurchase system when the transaction has finished */
	void OnInAppPurchaseComplete_Delayed();
	void OnInAppPurchaseComplete(EInAppPurchaseState::Type CompletionState);

	/** Unregisters our delegate from the In-App Purchase system */
	void RemoveDelegate();

	/** Triggers the In-App Purchase Transaction for the specifed user; the Purchase Request object must already be set up */
	void Trigger(class APlayerController* PlayerController, const FInAppPurchaseProductRequest& ProductRequest);

private:

	/** Delegate called when a InAppPurchase has been successfully read */
	FOnInAppPurchaseCompleteDelegate InAppPurchaseCompleteDelegate;

	/** Handle to the registered InAppPurchaseCompleteDelegate */
	FDelegateHandle InAppPurchaseCompleteDelegateHandle;

	/** The InAppPurchase read request */
	FOnlineInAppPurchaseTransactionPtr PurchaseRequest;

	/** Did we fail immediately? */
	bool bFailedToEvenSubmit;

	/** Pointer to the world, needed to delay the results slightly */
	TWeakObjectPtr<UWorld> WorldPtr;

	/** Did the purchase succeed? */
	EInAppPurchaseState::Type SavedPurchaseState;
};
