// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayEffectTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitGameplayEffectStackChange.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FWaitGameplayEffectStackChangeDelegate, FActiveGameplayEffectHandle, Handle, int32, NewCount, int32, OldCount);

class AActor;
class UPrimitiveComponent;

/**
 *	Waits for the actor to activate another ability
 */
UCLASS()
class GAMEPLAYABILITIES_API UAbilityTask_WaitGameplayEffectStackChange : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitGameplayEffectStackChangeDelegate	OnChange;

	UPROPERTY(BlueprintAssignable)
	FWaitGameplayEffectStackChangeDelegate	InvalidHandle;

	virtual void Activate() override;

	UFUNCTION()
	void OnGameplayEffectStackChange(FActiveGameplayEffectHandle Handle, int32 NewCount, int32 OldCount);

	/** Wait until the specified gameplay effect is removed. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitGameplayEffectStackChange* WaitForGameplayEffectStackChange(UGameplayAbility* OwningAbility, FActiveGameplayEffectHandle Handle);

	FActiveGameplayEffectHandle Handle;

protected:

	virtual void OnDestroy(bool AbilityIsEnding) override;
	bool Registered;

	FDelegateHandle OnGameplayEffectStackChangeDelegateHandle;
};
