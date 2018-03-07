// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitConfirmCancel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitConfirmCancelDelegate);

// Fixme: this name is conflicting with AbilityTask_WaitConfirm
// UAbilityTask_WaitConfirmCancel = Wait for Targeting confirm/cancel
// UAbilityTask_WaitConfirm = Wait for server to confirm ability activation

UCLASS()
class GAMEPLAYABILITIES_API UAbilityTask_WaitConfirmCancel : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitConfirmCancelDelegate	OnConfirm;

	UPROPERTY(BlueprintAssignable)
	FWaitConfirmCancelDelegate	OnCancel;
	
	UFUNCTION()
	void OnConfirmCallback();

	UFUNCTION()
	void OnCancelCallback();

	UFUNCTION()
	void OnLocalConfirmCallback();

	UFUNCTION()
	void OnLocalCancelCallback();

	UFUNCTION(BlueprintCallable, meta=(HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true", DisplayName="Wait for Confirm Input"), Category="Ability|Tasks")
	static UAbilityTask_WaitConfirmCancel* WaitConfirmCancel(UGameplayAbility* OwningAbility);	

	virtual void Activate() override;

protected:

	virtual void OnDestroy(bool AbilityEnding) override;

	bool RegisteredCallbacks;
	
};
