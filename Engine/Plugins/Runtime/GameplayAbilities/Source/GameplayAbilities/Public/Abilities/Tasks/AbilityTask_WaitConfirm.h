// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitConfirm.generated.h"

UCLASS()
class GAMEPLAYABILITIES_API UAbilityTask_WaitConfirm : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate	OnConfirm;

	UFUNCTION()
	void OnConfirmCallback(UGameplayAbility* InAbility);

	virtual void Activate() override;

	/** Wait until the server confirms the use of this ability. This is used to gate predictive portions of the ability */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitConfirm* WaitConfirm(UGameplayAbility* OwningAbility);

protected:

	virtual void OnDestroy(bool AbilityEnded) override;

	bool RegisteredCallback;

	FDelegateHandle OnConfirmCallbackDelegateHandle;
};
