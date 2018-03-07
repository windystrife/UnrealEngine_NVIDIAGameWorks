// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Object.h"
#include "OnlineBlueprintCallProxyBase.h"
#include "OnlineIdentityInterface.h"
#include "OculusEntitlementCallbackProxy.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOculusEntitlementCheckResult);

/**
 * Exposes some of the Platform SDK for blueprint use.
 */
UCLASS(MinimalAPI)
class UOculusEntitlementCallbackProxy : public UOnlineBlueprintCallProxyBase
{
    GENERATED_UCLASS_BODY()

    // Called when there is a successful entitlement check
    UPROPERTY(BlueprintAssignable)
    FOculusEntitlementCheckResult OnSuccess;

    // Called when there is an unsuccessful entitlement check
    UPROPERTY(BlueprintAssignable)
    FOculusEntitlementCheckResult OnFailure;

    // Kick off entitlement check. Asynchronous-- see OnUserPrivilegeCompleteDelegate for results.
    UFUNCTION(BlueprintCallable, Category = "Oculus|Entitlement", meta = (BlueprintInternalUseOnly = "true"))
    static UOculusEntitlementCallbackProxy* VerifyEntitlement();

    /** UOnlineBlueprintCallProxyBase interface */
    virtual void Activate() override;

private:

    // Delegate for VerifyEntitlement.
    void OnUserPrivilegeCompleteDelegate(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, uint32 Result);

};
