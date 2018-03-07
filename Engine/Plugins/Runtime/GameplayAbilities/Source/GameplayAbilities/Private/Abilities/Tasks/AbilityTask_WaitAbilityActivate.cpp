// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Abilities/Tasks/AbilityTask_WaitAbilityActivate.h"

#include "AbilitySystemComponent.h"


UAbilityTask_WaitAbilityActivate::UAbilityTask_WaitAbilityActivate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IncludeTriggeredAbilities = false;
}

UAbilityTask_WaitAbilityActivate* UAbilityTask_WaitAbilityActivate::WaitForAbilityActivate(UGameplayAbility* OwningAbility, FGameplayTag InWithTag, FGameplayTag InWithoutTag, bool InIncludeTriggeredAbilities, bool InTriggerOnce)
{
	UAbilityTask_WaitAbilityActivate* MyObj = NewAbilityTask<UAbilityTask_WaitAbilityActivate>(OwningAbility);
	MyObj->WithTag = InWithTag;
	MyObj->WithoutTag = InWithoutTag;
	MyObj->IncludeTriggeredAbilities = InIncludeTriggeredAbilities;
	MyObj->TriggerOnce = InTriggerOnce;
	return MyObj;
}

UAbilityTask_WaitAbilityActivate* UAbilityTask_WaitAbilityActivate::WaitForAbilityActivateWithTagRequirements(UGameplayAbility* OwningAbility, FGameplayTagRequirements TagRequirements, bool InIncludeTriggeredAbilities, bool InTriggerOnce)
{
	UAbilityTask_WaitAbilityActivate* MyObj = NewAbilityTask<UAbilityTask_WaitAbilityActivate>(OwningAbility);
	MyObj->TagRequirements = TagRequirements;
	MyObj->IncludeTriggeredAbilities = InIncludeTriggeredAbilities;
	MyObj->TriggerOnce = InTriggerOnce;
	return MyObj;
}

UAbilityTask_WaitAbilityActivate* UAbilityTask_WaitAbilityActivate::WaitForAbilityActivate_Query(UGameplayAbility* OwningAbility, FGameplayTagQuery Query, bool InIncludeTriggeredAbilities, bool InTriggerOnce)
{
	UAbilityTask_WaitAbilityActivate* MyObj = NewAbilityTask<UAbilityTask_WaitAbilityActivate>(OwningAbility);
	MyObj->Query = Query;
	MyObj->IncludeTriggeredAbilities = InIncludeTriggeredAbilities;
	MyObj->TriggerOnce = InTriggerOnce;
	return MyObj;

}

void UAbilityTask_WaitAbilityActivate::Activate()
{
	if (AbilitySystemComponent)
	{
		OnAbilityActivateDelegateHandle = AbilitySystemComponent->AbilityActivatedCallbacks.AddUObject(this, &UAbilityTask_WaitAbilityActivate::OnAbilityActivate);
	}
}

void UAbilityTask_WaitAbilityActivate::OnAbilityActivate(UGameplayAbility* ActivatedAbility)
{
	if (!IncludeTriggeredAbilities && ActivatedAbility->IsTriggered())
	{
		return;
	}

	if (TagRequirements.IsEmpty())
	{
		if ((WithTag.IsValid() && !ActivatedAbility->AbilityTags.HasTag(WithTag)) ||
			(WithoutTag.IsValid() && ActivatedAbility->AbilityTags.HasTag(WithoutTag)))
		{
			// Failed tag check
			return;
		}
	}
	else
	{
		if (!TagRequirements.RequirementsMet(ActivatedAbility->AbilityTags))
		{
			// Failed tag check
			return;
		}
	}

	if (Query.IsEmpty() == false)
	{
		if (Query.Matches(ActivatedAbility->AbilityTags) == false)
		{
			// Failed query
			return;
		}
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnActivate.Broadcast(ActivatedAbility);
	}

	if (TriggerOnce)
	{
		EndTask();
	}
}

void UAbilityTask_WaitAbilityActivate::OnDestroy(bool AbilityEnded)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityActivatedCallbacks.Remove(OnAbilityActivateDelegateHandle);
	}

	Super::OnDestroy(AbilityEnded);
}
