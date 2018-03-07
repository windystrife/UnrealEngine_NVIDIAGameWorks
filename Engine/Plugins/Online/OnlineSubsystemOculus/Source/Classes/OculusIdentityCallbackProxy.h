// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Object.h"
#include "OnlineBlueprintCallProxyBase.h"
#include "OnlineIdentityInterface.h"
#include "OculusIdentityCallbackProxy.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOculusIdentitySuccessResult, FString, OculusId, FString, OculusName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOculusIdentityFailureResult);

/**
 * Exposes the oculus id of the Platform SDK for blueprint use.
 */
UCLASS(MinimalAPI)
class UOculusIdentityCallbackProxy : public UOnlineBlueprintCallProxyBase
{
GENERATED_UCLASS_BODY()

	// Called when it successfully gets back the oculus id
	UPROPERTY(BlueprintAssignable)
	FOculusIdentitySuccessResult OnSuccess;

	// Called when it fails to get the oculus id
	UPROPERTY(BlueprintAssignable)
	FOculusIdentityFailureResult OnFailure;

	// Kick off GetOculusIdentity. Asynchronous-- see OnLoginCompleteDelegate for results.
	UFUNCTION(BlueprintCallable, Category = "Oculus|Identity", meta = (BlueprintInternalUseOnly = "true"))
	static UOculusIdentityCallbackProxy* GetOculusIdentity(int32 LocalUserNum);

	/** UOnlineBlueprintCallProxyBase interface */
	virtual void Activate() override;

private:

	int32 LocalUserNum;

	FDelegateHandle DelegateHandle;

	// Delegate for GetOculusID.
	void OnLoginCompleteDelegate(int32 Unused, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& ErrorStr);

};
