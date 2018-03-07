// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectApplied.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"

UAbilityTask_WaitGameplayEffectApplied::UAbilityTask_WaitGameplayEffectApplied(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Locked = false;
}

void UAbilityTask_WaitGameplayEffectApplied::Activate()
{
	if (GetASC())
	{
		RegisterDelegate();
	}
}

void UAbilityTask_WaitGameplayEffectApplied::OnApplyGameplayEffectCallback(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle)
{
	bool PassedComparison = false;

	AActor* AvatarActor = Target ? Target->AvatarActor : nullptr;

	if (!Filter.FilterPassesForActor(AvatarActor))
	{
		return;
	}
	if (!SourceTagRequirements.RequirementsMet(*SpecApplied.CapturedSourceTags.GetAggregatedTags()))
	{
		return;
	}
	if (!TargetTagRequirements.RequirementsMet(*SpecApplied.CapturedTargetTags.GetAggregatedTags()))
	{
		return;
	}

	if (SourceTagQuery.IsEmpty() == false)
	{
		if (!SourceTagQuery.Matches(*SpecApplied.CapturedSourceTags.GetAggregatedTags()))
		{
			return;
		}
	}

	if (TargetTagQuery.IsEmpty() == false)
	{
		if (!TargetTagQuery.Matches(*SpecApplied.CapturedTargetTags.GetAggregatedTags()))
		{
			return;
		}
	}

	if (Locked)
	{
		ABILITY_LOG(Error, TEXT("WaitGameplayEffectApplied recursion detected. Ability: %s. Applied Spec: %s. This could cause an infinite loop! Ignoring"), *GetNameSafe(Ability), *SpecApplied.ToSimpleString());
		return;
	}
	
	FGameplayEffectSpecHandle	SpecHandle(new FGameplayEffectSpec(SpecApplied));

	{
		TGuardValue<bool> GuardValue(Locked, true);	
		BroadcastDelegate(AvatarActor, SpecHandle, ActiveHandle);
	}

	if (TriggerOnce)
	{
		EndTask();
	}
}

void UAbilityTask_WaitGameplayEffectApplied::OnDestroy(bool AbilityEnded)
{
	if (GetASC())
	{
		RemoveDelegate();
	}

	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_WaitGameplayEffectApplied::SetExternalActor(AActor* InActor)
{
	if (InActor)
	{
		UseExternalOwner = true;
		ExternalOwner  = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InActor);
	}
}

UAbilitySystemComponent* UAbilityTask_WaitGameplayEffectApplied::GetASC()
{
	if (UseExternalOwner)
	{
		return ExternalOwner;
	}
	return AbilitySystemComponent;
}
