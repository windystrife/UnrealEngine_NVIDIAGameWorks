// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayTagContainer.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTagBase.h"
#include "AbilityTask_WaitGameplayTag.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitGameplayTagDelegate);

UCLASS()
class GAMEPLAYABILITIES_API UAbilityTask_WaitGameplayTagAdded : public UAbilityTask_WaitGameplayTag
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitGameplayTagDelegate	Added;

	/**
	 * 	Wait until the specified gameplay tag is Added. By default this will look at the owner of this ability. OptionalExternalTarget can be set to make this look at another actor's tags for changes. 
	 *  If the tag is already present when this task is started, it will immediately broadcast the Added event. It will keep listening as long as OnlyTriggerOnce = false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitGameplayTagAdded* WaitGameplayTagAdd(UGameplayAbility* OwningAbility, FGameplayTag Tag, AActor* InOptionalExternalTarget=nullptr, bool OnlyTriggerOnce=false);

	virtual void Activate() override;

	virtual void GameplayTagCallback(const FGameplayTag Tag, int32 NewCount) override;
};

UCLASS()
class GAMEPLAYABILITIES_API UAbilityTask_WaitGameplayTagRemoved : public UAbilityTask_WaitGameplayTag
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitGameplayTagDelegate	Removed;

	/**
	 * 	Wait until the specified gameplay tag is Removed. By default this will look at the owner of this ability. OptionalExternalTarget can be set to make this look at another actor's tags for changes. 
	 *  If the tag is not present when this task is started, it will immediately broadcast the Removed event. It will keep listening as long as OnlyTriggerOnce = false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitGameplayTagRemoved* WaitGameplayTagRemove(UGameplayAbility* OwningAbility, FGameplayTag Tag, AActor* InOptionalExternalTarget=nullptr, bool OnlyTriggerOnce=false);

	virtual void Activate() override;

	virtual void GameplayTagCallback(const FGameplayTag Tag, int32 NewCount) override;
};
