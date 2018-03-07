// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayEffectTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitGameplayEffectRemoved.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitGameplayEffectRemovedDelegate, const FGameplayEffectRemovalInfo &, GameplayEffectRemovalInfo);

class AActor;
class UPrimitiveComponent;

/**
 *	Waits for the actor to activate another ability
 */
UCLASS()
class GAMEPLAYABILITIES_API UAbilityTask_WaitGameplayEffectRemoved : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitGameplayEffectRemovedDelegate	OnRemoved;

	UPROPERTY(BlueprintAssignable)
	FWaitGameplayEffectRemovedDelegate	InvalidHandle;

	virtual void Activate() override;

	UFUNCTION()
	void OnGameplayEffectRemoved(const FGameplayEffectRemovalInfo& InGameplayEffectRemovalInfo);

	/** Wait until the specified gameplay effect is removed. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitGameplayEffectRemoved* WaitForGameplayEffectRemoved(UGameplayAbility* OwningAbility, FActiveGameplayEffectHandle Handle);

	FActiveGameplayEffectHandle Handle;

protected:

	virtual void OnDestroy(bool AbilityIsEnding) override;
	bool Registered;

	FDelegateHandle OnGameplayEffectRemovedDelegateHandle;
	FDelegateHandle OnGameplayEffectRemovedDelegateHandle_Deprecated;
};
