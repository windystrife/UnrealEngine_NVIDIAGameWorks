// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectApplied_Self.h"

#include "AbilitySystemComponent.h"


UAbilityTask_WaitGameplayEffectApplied_Self::UAbilityTask_WaitGameplayEffectApplied_Self(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAbilityTask_WaitGameplayEffectApplied_Self* UAbilityTask_WaitGameplayEffectApplied_Self::WaitGameplayEffectAppliedToSelf(UGameplayAbility* OwningAbility, const FGameplayTargetDataFilterHandle InFilter, FGameplayTagRequirements InSourceTagRequirements, FGameplayTagRequirements InTargetTagRequirements, bool InTriggerOnce, AActor* OptionalExternalOwner, bool InListenForPeriodicEffect)
{
	UAbilityTask_WaitGameplayEffectApplied_Self* MyObj = NewAbilityTask<UAbilityTask_WaitGameplayEffectApplied_Self>(OwningAbility);
	MyObj->Filter = InFilter;
	MyObj->SourceTagRequirements = InSourceTagRequirements;
	MyObj->TargetTagRequirements = InTargetTagRequirements;
	MyObj->TriggerOnce = InTriggerOnce;
	MyObj->SetExternalActor(OptionalExternalOwner);
	MyObj->ListenForPeriodicEffects = InListenForPeriodicEffect;
	return MyObj;
}

UAbilityTask_WaitGameplayEffectApplied_Self* UAbilityTask_WaitGameplayEffectApplied_Self::WaitGameplayEffectAppliedToSelf_Query(UGameplayAbility* OwningAbility, const FGameplayTargetDataFilterHandle InFilter, FGameplayTagQuery InSourceTagQuery, FGameplayTagQuery InTargetTagQuery, bool InTriggerOnce, AActor* OptionalExternalOwner, bool InListenForPeriodicEffect)
{
	UAbilityTask_WaitGameplayEffectApplied_Self* MyObj = NewAbilityTask<UAbilityTask_WaitGameplayEffectApplied_Self>(OwningAbility);
	MyObj->Filter = InFilter;
	MyObj->SourceTagQuery = InSourceTagQuery;
	MyObj->TargetTagQuery = InTargetTagQuery;
	MyObj->TriggerOnce = InTriggerOnce;
	MyObj->SetExternalActor(OptionalExternalOwner);
	MyObj->ListenForPeriodicEffects = InListenForPeriodicEffect;
	return MyObj;
}

void UAbilityTask_WaitGameplayEffectApplied_Self::BroadcastDelegate(AActor* Avatar, FGameplayEffectSpecHandle SpecHandle, FActiveGameplayEffectHandle ActiveHandle)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnApplied.Broadcast(Avatar, SpecHandle, ActiveHandle);
	}
}

void UAbilityTask_WaitGameplayEffectApplied_Self::RegisterDelegate()
{
	OnApplyGameplayEffectCallbackDelegateHandle = GetASC()->OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &UAbilityTask_WaitGameplayEffectApplied::OnApplyGameplayEffectCallback);
	if (ListenForPeriodicEffects)
	{
		OnPeriodicGameplayEffectExecuteCallbackDelegateHandle = GetASC()->OnPeriodicGameplayEffectExecuteDelegateOnSelf.AddUObject(this, &UAbilityTask_WaitGameplayEffectApplied::OnApplyGameplayEffectCallback);
	}
}

void UAbilityTask_WaitGameplayEffectApplied_Self::RemoveDelegate()
{
	GetASC()->OnGameplayEffectAppliedDelegateToSelf.Remove(OnApplyGameplayEffectCallbackDelegateHandle);
	if (OnPeriodicGameplayEffectExecuteCallbackDelegateHandle.IsValid())
	{
		GetASC()->OnGameplayEffectAppliedDelegateToTarget.Remove(OnPeriodicGameplayEffectExecuteCallbackDelegateHandle);
	}
}
