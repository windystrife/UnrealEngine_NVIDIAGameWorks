// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitCancel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitCancelDelegate);

UCLASS()
class GAMEPLAYABILITIES_API UAbilityTask_WaitCancel : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitCancelDelegate	OnCancel;
	
	UFUNCTION()
	void OnCancelCallback();

	UFUNCTION()
	void OnLocalCancelCallback();

	UFUNCTION(BlueprintCallable, meta=(HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true", DisplayName="Wait for Cancel Input"), Category="Ability|Tasks")
	static UAbilityTask_WaitCancel* WaitCancel(UGameplayAbility* OwningAbility);	

	virtual void Activate() override;

protected:

	virtual void OnDestroy(bool AbilityEnding) override;

	bool RegisteredCallbacks;
	
};
