// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Interfaces/OnlineStoreInterface.h"
#include "InAppPurchaseQueryCallbackProxy.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInAppPurchaseQueryResult, const TArray<FInAppPurchaseProductInfo>&, InAppPurchaseInformation);

UCLASS(MinimalAPI)
class UInAppPurchaseQueryCallbackProxy : public UObject
{
	GENERATED_UCLASS_BODY()

	// Called when there is a successful InAppPurchase query
	UPROPERTY(BlueprintAssignable)
	FInAppPurchaseQueryResult OnSuccess;

	// Called when there is an unsuccessful InAppPurchase query
	UPROPERTY(BlueprintAssignable)
	FInAppPurchaseQueryResult OnFailure;

	// Queries a InAppPurchase for an integer value
	UFUNCTION(BlueprintCallable, meta = (DisplayName="Read In App Purchase Information"), Category="Online|InAppPurchase")
	static UInAppPurchaseQueryCallbackProxy* CreateProxyObjectForInAppPurchaseQuery(class APlayerController* PlayerController, const TArray<FString>& ProductIdentifiers);

public:
	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	//~ End UObject Interface

private:
	/** Called by the InAppPurchase system when the read is finished */
	void OnInAppPurchaseRead_Delayed();
	void OnInAppPurchaseRead(bool bWasSuccessful);

	/** Unregisters our delegate from the InAppPurchase system */
	void RemoveDelegate();

	/** Triggers the query for a specifed user; the ReadObject must already be set up */
	void TriggerQuery(class APlayerController* PlayerController, const TArray<FString>& ProductIdentifiers);

private:
	/** Delegate called when a InAppPurchase has been successfully read */
	FOnQueryForAvailablePurchasesCompleteDelegate InAppPurchaseReadCompleteDelegate;

	/** InAppPurchaseReadComplete delegate handle */
	FDelegateHandle InAppPurchaseReadCompleteDelegateHandle;

	/** The InAppPurchase read request */
	FOnlineProductInformationReadPtr ReadObject;

	/** Did we fail immediately? */
	bool bFailedToEvenSubmit;

	// Pointer to the world, needed to delay the results slightly
	TWeakObjectPtr<UWorld> WorldPtr;

	// Did the read succeed?
	bool bSavedWasSuccessful;
	TArray<FInAppPurchaseProductInfo> SavedProductInformation;
};
