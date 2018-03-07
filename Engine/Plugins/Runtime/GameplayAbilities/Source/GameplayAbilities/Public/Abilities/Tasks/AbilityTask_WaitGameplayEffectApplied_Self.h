// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayEffectTypes.h"
#include "Abilities/GameplayAbilityTargetDataFilter.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectApplied.h"
#include "AbilityTask_WaitGameplayEffectApplied_Self.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGameplayEffectAppliedSelfDelegate, AActor*, Source, FGameplayEffectSpecHandle, SpecHandle,  FActiveGameplayEffectHandle, ActiveHandle );

UCLASS()
class GAMEPLAYABILITIES_API UAbilityTask_WaitGameplayEffectApplied_Self : public UAbilityTask_WaitGameplayEffectApplied
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FGameplayEffectAppliedSelfDelegate OnApplied;

	/**	 
	 * Wait until the owner *receives* a GameplayEffect from a given source (the source may be the owner too!). If TriggerOnce is true, this task will only return one time. Otherwise it will return everytime a GE is applied that meets the requirements over the life of the ability
	 * Optional External Owner can be used to run this task on someone else (not the owner of the ability). By default you can leave this empty.
	 */	 	 
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitGameplayEffectApplied_Self* WaitGameplayEffectAppliedToSelf(UGameplayAbility* OwningAbility, const FGameplayTargetDataFilterHandle SourceFilter, FGameplayTagRequirements SourceTagRequirements, FGameplayTagRequirements TargetTagRequirements, bool TriggerOnce=false, AActor* OptionalExternalOwner=nullptr, bool ListenForPeriodicEffect=false );

	/**	 
	 * Wait until the owner *receives* a GameplayEffect from a given source (the source may be the owner too!). If TriggerOnce is true, this task will only return one time. Otherwise it will return everytime a GE is applied that meets the requirements over the life of the ability
	 * Optional External Owner can be used to run this task on someone else (not the owner of the ability). By default you can leave this empty.
	 *
	 * This version uses FGameplayTagQuery (more power) instead of FGameplayTagRequirements (faster)
	 */	
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitGameplayEffectApplied_Self* WaitGameplayEffectAppliedToSelf_Query(UGameplayAbility* OwningAbility, const FGameplayTargetDataFilterHandle SourceFilter, FGameplayTagQuery SourceTagQuery, FGameplayTagQuery TargetTagQuery, bool TriggerOnce=false, AActor* OptionalExternalOwner=nullptr, bool ListenForPeriodicEffect=false );
	
protected:

	virtual void BroadcastDelegate(AActor* Avatar, FGameplayEffectSpecHandle SpecHandle, FActiveGameplayEffectHandle ActiveHandle) override;
	virtual void RegisterDelegate() override;
	virtual void RemoveDelegate() override;

private:
	FDelegateHandle OnApplyGameplayEffectCallbackDelegateHandle;
	FDelegateHandle OnPeriodicGameplayEffectExecuteCallbackDelegateHandle;
};
