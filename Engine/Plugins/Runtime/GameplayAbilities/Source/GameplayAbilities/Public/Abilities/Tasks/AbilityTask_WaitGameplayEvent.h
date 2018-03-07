// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitGameplayEvent.generated.h"

class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitGameplayEventDelegate, FGameplayEventData, Payload);

UCLASS()
class GAMEPLAYABILITIES_API UAbilityTask_WaitGameplayEvent : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitGameplayEventDelegate	EventReceived;

	/**
	 * 	Wait until the specified gameplay tag event is triggered. By default this will look at the owner of this ability. OptionalExternalTarget can be set to make this look at another actor's tags for changes. 
	 *  It will keep listening as long as OnlyTriggerOnce = false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitGameplayEvent* WaitGameplayEvent(UGameplayAbility* OwningAbility, FGameplayTag EventTag, AActor* OptionalExternalTarget=nullptr, bool OnlyTriggerOnce=false);

	void SetExternalTarget(AActor* Actor);

	UAbilitySystemComponent* GetTargetASC();

	virtual void Activate() override;

	virtual void GameplayEventCallback(const FGameplayEventData* Payload);

	void OnDestroy(bool AbilityEnding) override;

	FGameplayTag Tag;

	UPROPERTY()
	UAbilitySystemComponent* OptionalExternalTarget;

	bool UseExternalTarget;	
	bool OnlyTriggerOnce;

	FDelegateHandle MyHandle;
};
